#include "pch.h"
#include "core/config.h"

#include "default_config.h"

#include <cameraunlock/config/ini_reader.h>
#include <cameraunlock/config/value_guards.h>
#include <cameraunlock/protocol/port_utils.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace TWHT {

namespace {

namespace guards = cameraunlock::config;

// Where the guards' diagnostics go while the config is being read.
//
// cameraunlock::config reports through a printf-style sink, and there is
// nowhere to send it yet: whether to open the log file at all is itself a
// config key, so the file log does not exist until Mod::Initialize has this
// struct back. Each line is parked in the vector Mod::Initialize replays.
//
// Set for the duration of one LoadFromIni call, which runs once on the init
// thread before any other thread is reading a Config.
std::vector<std::string>* g_parseWarnings = nullptr;

void CollectWarning(const char* fmt, ...) {
    if (g_parseWarnings == nullptr) return;
    char msg[512] = {};
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    g_parseWarnings->emplace_back(std::string("WARN: ") + msg);
}

// A float key, parsed whole and clamped into [lo, hi]. The whole-token parse is
// what catches "LocalSmoothing=0,15" - a European decimal comma, and the reader
// pins the C locale, so strtod stops at the comma and yields 0.0, which is
// inside every valid range and used to pass silently. Non-finite values are
// caught in the same place: NaN fails every comparison, so a clamp does not
// reject one, and it would reach the camera quaternion.
float ReadFloatKey(const cameraunlock::IniReader& r, const char* section, const char* key,
                   float fallback, float lo, float hi) {
    return guards::ReadFloatChecked(r, section, key, fallback, lo, hi, &CollectWarning);
}

// A bool key, with the same reporting every other key here gets. IniReader
// matches the WHOLE value against a fixed list and answers the default for
// anything else, and GetPrivateProfileStringA does not strip inline comments -
// so `CollisionEnabled=true ; confirmed` reads as false, and used to say
// nothing at all about it.
bool ReadBoolKey(const cameraunlock::IniReader& r, const char* section, const char* key,
                 bool fallback) {
    const std::string raw = guards::ReadRawValue(r, section, key);
    if (raw.empty()) return fallback;

    const bool value = r.ReadBool(section, key, fallback);
    // ReadBool answers the fallback both for "the user wrote the fallback" and
    // for "the user wrote nonsense", so the two are told apart by asking again
    // with the opposite default: a value the reader understands is unmoved by
    // the default it was given.
    if (r.ReadBool(section, key, !fallback) == value) return value;

    // The UNSTRIPPED text, because ReadBool saw that and `raw` has already had
    // any trailing comment cut off it - quoting `raw` would tell a user that
    // `true` is not true.
    CollectWarning("[%s] %s=%s is not true or false - using %s.",
                   section, key, r.ReadString(section, key, "").c_str(),
                   fallback ? "true" : "false");
    return fallback;
}

// The camera's near clip distance, in metres. Registered by the engine as
// `z_near` and read off the tuning block at 0.0598; the standoff has to clear it
// or the wall it holds the eye off is culled and the player sees through it
// anyway. Rounded up rather than read live: this runs before the mod is bound to
// a build, and the number is a constant of the shipped game.
constexpr float kMinCollisionMargin = 0.06f;

// A distance key, refused rather than clamped. Clamping suits a value that is a
// position on a scale - smoothing runs 0 to 1 and either end is a real answer -
// but a distance out of range is a typo about a magnitude, and the two plausible
// typos both resolve to something worse than the default: a negative limit
// clamps to zero and kills the axis for the session, and an oversized collision
// standoff clamps to a value that still cancels every lean. The shipped default
// stands instead, and the correction is named either way.
float ReadDistanceKey(const cameraunlock::IniReader& r, const char* section, const char* key,
                      float fallback, float lo, float hi) {
    const std::string raw = guards::ReadRawValue(r, section, key);
    if (raw.empty()) return fallback;

    float value = 0.0f;
    if (!guards::ParseFloatStrict(raw, value)) {
        CollectWarning("[%s] %s=%s is not a number - using %.2f.",
                       section, key, raw.c_str(), fallback);
        return fallback;
    }
    if (!std::isfinite(value) || value < lo || value > hi) {
        CollectWarning("[%s] %s=%s is not a distance in %.2f-%.2f metres - using %.2f.",
                       section, key, raw.c_str(), lo, hi, fallback);
        return fallback;
    }
    return value;
}

// A virtual key code the hotkey poller can actually watch. A typo like
// ToggleKey=0x230 is outside what GetAsyncKeyState defines, so the binding can
// never fire and the key silently does nothing.
int ReadHotkey(const cameraunlock::IniReader& r, const char* key, int fallback) {
    const int vk = r.ReadHex("Hotkeys", key, fallback);
    if (guards::IsBindableVirtualKey(vk)) return vk;
    CollectWarning("[Hotkeys] %s=0x%X is not a key that can be polled (Ctrl, Shift and Alt "
                   "included, since the chords are built from them) - using 0x%X.",
                   key, vk, fallback);
    return fallback;
}

// Like the guards above, this cannot log directly - the file log does not exist
// yet during LoadFromIni - so the line is appended to warnings for
// Mod::Initialize to replay.
//
// The old smoothing value is deliberately NOT migrated into the new keys. The
// single Smoothing value carried a hidden 0.15 floor, so the number in an
// existing config does not mean what it used to: copying it across would hand a
// local user smoothing they never chose under the new semantics, and copying it
// into only one of the two keys would be a guess about which connection they
// were on.
void WarnRetiredSmoothingKey(const cameraunlock::IniReader& reader,
                             const char* section, const char* key) {
    if (guards::ReadRawValue(reader, section, key).empty()) return;
    CollectWarning("Config key [%s] %s has been retired and is IGNORED. Smoothing "
                   "is now two keys: LocalSmoothing (default 0, applies to a tracker on "
                   "this machine) and RemoteSmoothing (default 0.15, applies to a tracker "
                   "on the network). The old value is not migrated because the semantics "
                   "changed - it carried a hidden 0.15 floor that no longer exists. Set "
                   "the two new keys.",
                   section, key);
}

// BindAddress was read for as long as it existed and never applied: the shared
// receiver binds INADDR_ANY and takes no bind address, so a user who set
// 127.0.0.1 to keep the listener off the network got no such thing. A key that
// only looks like a control is worse than no key, so it is gone and its absence
// is stated rather than left for the user to discover.
void WarnRetiredBindAddress(const cameraunlock::IniReader& reader) {
    if (guards::ReadRawValue(reader, "Network", "BindAddress").empty()) return;
    CollectWarning("Config key [Network] BindAddress has been retired and is IGNORED. It "
                   "never bound anything: the receiver listens on every interface, so any "
                   "device on your network can send a pose to UDPPort. Remove the key.");
}

// Two actions on one key is not a binding the poller rejects - it registers both
// and fires both - so one press would toggle tracking and cycle the mode, which
// reads as the toggle not working.
//
// The repair is to drop the later binding rather than move it to its own
// default: that default may be the very key it collided with (ToggleKey=0x21
// against the shipped CycleModeKey=0x21 is one keystroke away), and reassigning
// there resolves the collision to itself while the log claims it was fixed. A
// key of 0 is not registered, so the action loses its nav binding and keeps its
// Ctrl+Shift chord.
void RejectDuplicateHotkeys(Config& config) {
    struct Binding { const char* name; int* key; };
    const Binding bindings[] = {
        { "ToggleKey",        &config.toggleKey },
        { "CycleModeKey",     &config.cycleModeKey },
        { "ToggleYawModeKey", &config.toggleYawModeKey },
    };
    for (int i = 1; i < 3; ++i) {
        for (int j = 0; j < i; ++j) {
            if (*bindings[i].key == 0 || *bindings[i].key != *bindings[j].key) continue;
            CollectWarning("[Hotkeys] %s and %s are both 0x%X, which would fire both "
                           "actions on one press - %s is unbound for this session. Its "
                           "Ctrl+Shift chord still works.",
                           bindings[j].name, bindings[i].name, *bindings[i].key,
                           bindings[i].name);
            *bindings[i].key = 0;
        }
    }
}

// Sensitivity multipliers and axis inversion were config keys once. They are
// gone rather than deprecated: the tracker owns the shape of the pose, so one
// profile in opentrack or the phone app behaves the same in every game, and an
// axis that arrives mirrored is a conversion at the engine boundary rather than
// something for the player to find. A user carrying an old file forward is told
// once, because a multiplier that silently stopped applying is worse than one
// that was never offered.
void WarnRetiredPoseShapingKeys(const cameraunlock::IniReader& reader) {
    struct Retired { const char* section; const char* key; };
    const Retired keys[] = {
        { "Sensitivity", "YawMultiplier" },   { "Sensitivity", "PitchMultiplier" },
        { "Sensitivity", "RollMultiplier" },  { "Sensitivity", "InvertYaw" },
        { "Sensitivity", "InvertPitch" },     { "Sensitivity", "InvertRoll" },
        { "Position", "SensitivityX" },       { "Position", "SensitivityY" },
        { "Position", "SensitivityZ" },       { "Position", "InvertX" },
        { "Position", "InvertY" },            { "Position", "InvertZ" },
    };
    for (const Retired& k : keys) {
        if (guards::ReadRawValue(reader, k.section, k.key).empty()) continue;
        CollectWarning("Config key [%s] %s has been retired and is IGNORED. The pose is "
                       "used exactly as the tracker sends it - set sensitivity and axis "
                       "direction in opentrack or your phone app instead, and one profile "
                       "then behaves the same in every game. Remove the key.",
                       k.section, k.key);
    }
}

}  // namespace

bool Config::LoadFromIni(const std::string& path) {
    cameraunlock::IniReader r;
    if (!r.Open(path)) return false;

    g_parseWarnings = &warnings;

    // Network. ReadInt answers 0 for a present-but-unparseable value rather than
    // the default, and a cast straight to unsigned short turns 70000 into 4464,
    // so the raw value is range-checked before it becomes a port.
    const int rawPort = r.ReadInt("Network", "UDPPort", udpPort);
    bool portValid = false;
    const unsigned short port = cameraunlock::NormalizeUdpPort(rawPort, udpPort, portValid);
    if (!portValid) {
        CollectWarning("[Network] UDPPort=%d is not a port in 1024-65535 - using %u.",
                       rawPort, static_cast<unsigned>(port));
    }
    udpPort = port;

    WarnRetiredBindAddress(r);

    // Smoothing. Validation, never a floor: a finite value inside [0,1] is taken
    // as written, so a deliberately configured 0.0 stays 0.0. The two fallbacks
    // differ on purpose - a malformed RemoteSmoothing must not drop back to the
    // LOCAL default, which would leave a phone on WiFi running with no smoothing
    // at all on raw network jitter - and each is this member's shipped default.
    localSmoothing  = ReadFloatKey(r, "Smoothing", "LocalSmoothing",  localSmoothing,  0.0f, 1.0f);
    remoteSmoothing = ReadFloatKey(r, "Smoothing", "RemoteSmoothing", remoteSmoothing, 0.0f, 1.0f);

    WarnRetiredSmoothingKey(r, "Smoothing", "Factor");

    // Position. A limit is refused rather than clamped to zero: a sign typo on a
    // limit means the magnitude, and clamping -0.30 to 0.0 kills the axis for the
    // session while reporting only that it was corrected.
    positionEnabled = ReadBoolKey(r, "Position", "Enabled", positionEnabled);
    constexpr float kMinLimit = 0.01f;
    constexpr float kMaxLimit = guards::kMaxPositionLimit;
    posLimitX     = ReadDistanceKey(r, "Position", "LimitX",     posLimitX,     kMinLimit, kMaxLimit);
    posLimitY     = ReadDistanceKey(r, "Position", "LimitY",     posLimitY,     kMinLimit, kMaxLimit);
    posLimitZ     = ReadDistanceKey(r, "Position", "LimitZ",     posLimitZ,     kMinLimit, kMaxLimit);
    posLimitZBack = ReadDistanceKey(r, "Position", "LimitZBack", posLimitZBack, kMinLimit, kMaxLimit);
    // No position smoothing key: position uses the same LocalSmoothing /
    // RemoteSmoothing pair as rotation.
    WarnRetiredSmoothingKey(r, "Position", "Smoothing");

    WarnRetiredPoseShapingKeys(r);

    // Camera collision. The margin is the one distance in this file with a hard
    // floor under it rather than a taste range: geometry closer to the eye than
    // the near plane is culled, so a standoff under it holds the eye off a wall
    // the player still sees through, which is the complaint the clamp exists to
    // answer.
    collisionEnabled = ReadBoolKey(r, "Collision", "CollisionEnabled", collisionEnabled);
    // A fixed range, not one derived from LimitZ. The two are independent keys,
    // so a small LimitZ against the near-clip floor would invert the bounds and
    // the guard would refuse every value including its own default, reporting an
    // impossible range and then using the number it had just called invalid.
    collisionMargin  = ReadDistanceKey(r, "Collision", "CollisionMargin", collisionMargin,
                                       kMinCollisionMargin, kMaxLimit);
    // Their relationship is reported instead of enforced. Both values are
    // legitimate on their own and the user chose them; silently rewriting one
    // would hide that. The clamp allows `hit distance - margin`, so a margin at
    // or above a travel limit means any surface the sweep does find cancels that
    // axis outright rather than shortening it - open ground is unaffected, which
    // is why this is a warning and not a refusal.
    const float smallestLimit =
        (std::min)((std::min)(posLimitX, posLimitY), (std::min)(posLimitZ, posLimitZBack));
    if (collisionEnabled && collisionMargin >= smallestLimit) {
        CollectWarning("[Collision] CollisionMargin=%.2f is not smaller than the smallest "
                       "[Position] limit (%.2f), so against near geometry that lean is cut "
                       "to nothing rather than shortened.",
                       collisionMargin, smallestLimit);
    }
    collisionReleaseSmoothing = ReadFloatKey(r, "Collision", "CollisionReleaseSmoothing",
                                             collisionReleaseSmoothing, 0.0f, 1.0f);

    // Hotkeys
    toggleKey        = ReadHotkey(r, "ToggleKey",        toggleKey);
    cycleModeKey     = ReadHotkey(r, "CycleModeKey",     cycleModeKey);
    toggleYawModeKey = ReadHotkey(r, "ToggleYawModeKey", toggleYawModeKey);
    RejectDuplicateHotkeys(*this);

    // General
    autoEnable = ReadBoolKey(r, "General", "AutoEnable", autoEnable);
    logToFile  = ReadBoolKey(r, "General", "LogToFile",  logToFile);
    worldSpaceYaw = ReadBoolKey(r, "General", "WorldSpaceYaw", worldSpaceYaw);
    logDiagnostics = ReadBoolKey(r, "General", "LogDiagnostics", logDiagnostics);

    g_parseWarnings = nullptr;
    return true;
}

bool WriteDefaultConfig(const std::string& path, int& error) {
    error = 0;
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        error = errno;
        return false;
    }

    const std::size_t length = std::strlen(kDefaultConfigIni);
    const bool wrote = std::fwrite(kDefaultConfigIni, 1, length, f) == length;
    if (!wrote) error = errno;
    const bool closed = std::fclose(f) == 0;
    if (wrote && !closed) error = errno;
    return wrote && closed;
}

} // namespace TWHT
