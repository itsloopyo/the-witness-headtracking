// The ini is the one place raw user text becomes a float that reaches the
// camera transform and the network stack. Every case below is a value a person
// can type by accident, and each one used to be accepted in silence: a decimal
// comma read as zero, a NaN that no clamp rejects because every comparison
// against it is false, a negative limit that inverts the clamp bounds, a port
// that wraps into a different port.

#include "test_harness.h"

#include "core/config.h"

#include "default_config.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <windows.h>

namespace {

using twht_test::Check;
using twht_test::NearEqual;

std::string TempIniPath(const char* name) {
    char dir[MAX_PATH] = {};
    const DWORD len = GetTempPathA(MAX_PATH, dir);
    const std::string base = (len == 0 || len >= MAX_PATH) ? std::string(".\\") : std::string(dir, len);
    return base + "twht_" + std::to_string(GetCurrentProcessId()) + "_" + name + ".ini";
}

// Loads a config from `contents`, leaving nothing behind on disk. The defaults
// the returned Config starts from are the shipped ones, so a key the file omits
// or gets rejected reads back as the default.
TWHT::Config LoadIni(const char* name, const std::string& contents) {
    const std::string path = TempIniPath(name);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f != nullptr) {
        std::fwrite(contents.data(), 1, contents.size(), f);
        std::fclose(f);
    }

    TWHT::Config config;
    const bool loaded = config.LoadFromIni(path);
    std::remove(path.c_str());
    Check(loaded, "the test ini was written and opened");
    return config;
}

bool WarnsAbout(const TWHT::Config& config, const char* fragment) {
    for (const std::string& warning : config.warnings) {
        if (warning.find(fragment) != std::string::npos) return true;
    }
    return false;
}

void CleanValuesSurviveVerbatim() {
    const TWHT::Config config = LoadIni("clean",
        "[Network]\nUDPPort=6070\n"
        "[Smoothing]\nLocalSmoothing=0.0\nRemoteSmoothing=0.4\n"
        "[Position]\nEnabled=false\nLimitX=0.25\n"
        "[Hotkeys]\nToggleKey=0x24\n");

    Check(config.udpPort == 6070, "a valid port is taken as written");
    Check(config.positionEnabled == false, "a valid bool is taken as written");
    // Validation, never a floor: a deliberately configured 0.0 stays 0.0.
    Check(config.localSmoothing == 0.0f, "a configured zero smoothing stays zero");
    Check(NearEqual(config.remoteSmoothing, 0.4f), "a valid remote smoothing is taken as written");
    Check(NearEqual(config.posLimitX, 0.25f), "a valid position limit is taken as written");
    Check(config.toggleKey == 0x24, "a bindable hotkey is taken as written");
    Check(config.warnings.empty(), "a clean file produces no warnings");
}

void PartialNumbersAreRefused() {
    // A European decimal comma. The reader pins the C locale, so strtod stops at
    // the comma and yields 0.0 - inside every valid range, and silent.
    const TWHT::Config config = LoadIni("comma",
        "[Smoothing]\nRemoteSmoothing=0,4\n"
        "[Position]\nLimitZ=0,4\n");

    Check(NearEqual(config.remoteSmoothing, 0.15f),
          "a decimal comma leaves the smoothing default standing");
    Check(NearEqual(config.posLimitZ, 0.40f),
          "a decimal comma leaves the position limit default standing");
    Check(WarnsAbout(config, "RemoteSmoothing") && WarnsAbout(config, "LimitZ"),
          "both partial numbers are named in the warnings");
}

void NonFiniteValuesAreRefused() {
    // strtod parses "nan" and "inf", and no clamp rejects a NaN because both of
    // its comparisons are false.
    const TWHT::Config config = LoadIni("nonfinite",
        "[Smoothing]\nLocalSmoothing=nan\nRemoteSmoothing=inf\n"
        "[Position]\nLimitY=1e400\n"
        "[Collision]\nCollisionMargin=nan\nCollisionReleaseSmoothing=inf\n");

    Check(config.localSmoothing == 0.0f, "a NaN smoothing falls back to the default");
    Check(NearEqual(config.remoteSmoothing, 0.15f), "an infinite smoothing falls back too");
    Check(NearEqual(config.posLimitY, 0.20f), "an overflowed limit falls back to the default");
    Check(NearEqual(config.collisionMargin, 0.15f), "a NaN collision margin falls back");
    Check(NearEqual(config.collisionReleaseSmoothing, 0.9f),
          "an infinite release smoothing falls back");
    Check(config.warnings.size() >= 5, "every rejected value is reported");
}

void OutOfRangeValuesAreClamped() {
    // Smoothing is a position on a 0-1 scale, so an out-of-range value has an
    // obvious intent and clamping to the end of the scale honours it.
    const TWHT::Config config = LoadIni("range",
        "[Smoothing]\nLocalSmoothing=-0.5\nRemoteSmoothing=1.5\n");

    Check(config.localSmoothing == 0.0f, "a negative smoothing clamps to 0");
    Check(config.remoteSmoothing == 1.0f, "a smoothing above 1 clamps to 1");
}

void NegativePositionLimitsFallBack() {
    // A limit is a distance, and clamping -0.30 to 0.0 would kill the axis for
    // the session while reporting only that it had been corrected. A sign typo
    // on a limit means the magnitude, so the shipped default stands instead.
    const TWHT::Config config = LoadIni("neglimit", "[Position]\nLimitX=-0.30\n");

    Check(NearEqual(config.posLimitX, 0.30f),
          "a negative position limit falls back rather than disabling the axis");
    Check(WarnsAbout(config, "LimitX"), "the refused limit is reported");
}

// IniReader matches a bool against a fixed list of spellings and answers the
// caller's default for anything else, and GetPrivateProfileStringA leaves an
// inline comment attached to the value. Silently, until now.
void MalformedBoolsAreReported() {
    const TWHT::Config config = LoadIni("bools",
        "[Collision]\nCollisionEnabled=true ; confirmed on the shed wall\n"
        "[General]\nLogDiagnostics=TRue\nAutoEnable=no\n");

    Check(config.collisionEnabled == false, "a bool with a trailing comment falls back");
    Check(WarnsAbout(config, "CollisionEnabled"),
          "the bool the reader could not parse is reported rather than silently defaulted");
    Check(config.logDiagnostics == false, "a misspelled bool falls back");
    Check(WarnsAbout(config, "LogDiagnostics"), "the misspelled bool is reported");
    // Opposite the default, so this fails if the value was quietly dropped.
    Check(config.autoEnable == false, "a spelling the reader accepts is taken as written");
}

// The margin is the one distance here with a hard floor under it: geometry
// closer to the eye than the near plane is culled, so a standoff below it holds
// the eye off a wall the player still sees through.
void CollisionMarginIsHeldAboveTheNearPlane() {
    const TWHT::Config tooSmall = LoadIni("margin_low", "[Collision]\nCollisionMargin=0.02\n");
    Check(NearEqual(tooSmall.collisionMargin, 0.15f),
          "a standoff inside the near clip falls back rather than shipping a see-through wall");
    Check(WarnsAbout(tooSmall, "CollisionMargin"), "the refused standoff is reported");

    // A standoff at or above a travel limit means any surface the sweep finds
    // cancels that axis outright rather than shortening it. Both keys are
    // legitimate alone, so this is reported rather than rewritten - and only
    // when the clamp is on, since otherwise neither value does anything.
    const TWHT::Config tooBig = LoadIni("margin_high",
        "[Collision]\nCollisionEnabled=true\nCollisionMargin=1.5\n");
    Check(NearEqual(tooBig.collisionMargin, 1.5f), "the user's standoff is kept");
    Check(WarnsAbout(tooBig, "CollisionMargin") && WarnsAbout(tooBig, "limit"),
          "a standoff at or above a travel limit says that lean will be cut to nothing");

    // The same pair with the clamp off is not a problem worth a line.
    const TWHT::Config off = LoadIni("margin_off", "[Collision]\nCollisionMargin=1.5\n");
    Check(off.warnings.empty(), "an unused standoff is not reported");
}

// The poller registers every binding it is given and fires all of them, so two
// actions on one key is one press doing both - which reads as the toggle being
// broken rather than as a config mistake.
void DuplicateHotkeysAreRefused() {
    const TWHT::Config config = LoadIni("dupkeys", "[Hotkeys]\nCycleModeKey=0x23\n");

    Check(config.toggleKey == 0x23, "the first binding on the key keeps it");
    Check(config.cycleModeKey == 0, "the second is unbound rather than moved");
    Check(WarnsAbout(config, "CycleModeKey"), "the collision is reported");

    // Not moved to its own default, because that default can be the very key it
    // collided with - one keystroke from the shipped configuration.
    const TWHT::Config onto = LoadIni("dupkeys2", "[Hotkeys]\nToggleKey=0x21\n");
    Check(onto.toggleKey == 0x21, "the first binding keeps the key it asked for");
    Check(onto.cycleModeKey == 0,
          "the collision is not resolved by reassigning to the key it collided with");
}

void PortsOutsideTheRangeAreRefused() {
    // ReadInt answers 0 for a present-but-unparseable value rather than the
    // default, and a cast straight to unsigned short turns 70000 into 4464.
    const TWHT::Config wrapped = LoadIni("port_wrap", "[Network]\nUDPPort=70000\n");
    Check(wrapped.udpPort == 4242, "a port above 65535 falls back rather than wrapping");
    Check(WarnsAbout(wrapped, "UDPPort"), "the rejected port is reported");

    const TWHT::Config garbage = LoadIni("port_text", "[Network]\nUDPPort=notaport\n");
    Check(garbage.udpPort == 4242, "an unparseable port falls back rather than binding 0");

    const TWHT::Config privileged = LoadIni("port_low", "[Network]\nUDPPort=80\n");
    Check(privileged.udpPort == 4242, "a port below 1024 falls back");
}

void UnbindableHotkeysAreRefused() {
    const TWHT::Config config = LoadIni("hotkeys",
        "[Hotkeys]\nToggleKey=0x230\nCycleModeKey=0x11\n");

    Check(config.toggleKey == 0x23, "a key code outside 0x01-0xFE falls back to the default");
    Check(config.cycleModeKey == 0x21, "a modifier key falls back to the default");
    Check(WarnsAbout(config, "ToggleKey") && WarnsAbout(config, "CycleModeKey"),
          "both unbindable keys are reported");
}

void RetiredBindAddressIsReported() {
    // Read for as long as it existed and never applied - the receiver binds
    // every interface and takes no address. Retiring it in silence would leave
    // the user believing they had restricted who can send a pose.
    const TWHT::Config config = LoadIni("bindaddr", "[Network]\nBindAddress=127.0.0.1\n");
    Check(WarnsAbout(config, "BindAddress"), "a leftover BindAddress says it is ignored");
}

// Sensitivity and inversion were keys once. The pose is now used exactly as the
// tracker sends it, so a user carrying an old file forward has to be told the
// value stopped applying - a multiplier that silently does nothing is worse than
// one that was never offered.
void RetiredPoseShapingKeysAreReported() {
    const TWHT::Config config = LoadIni("shaping",
        "[Sensitivity]\nYawMultiplier=1.5\nInvertPitch=true\n"
        "[Position]\nSensitivityZ=2.0\nInvertZ=true\n");

    Check(WarnsAbout(config, "YawMultiplier") && WarnsAbout(config, "InvertPitch"),
          "a retired rotation key says it is ignored");
    Check(WarnsAbout(config, "SensitivityZ") && WarnsAbout(config, "InvertZ"),
          "a retired position key says it is ignored");
    Check(config.warnings.size() == 4, "each retired key is named once");
}

// The file the packager copies next to the game exe, and the only source of the
// defaults now that nothing generates one. It is maintained by hand, so every
// value in it is compared against the built-in default rather than a chosen few:
// an edit that moved LimitZ or RemoteSmoothing in the file alone would otherwise
// ship a default that disagrees with the header, the README and the docs, and
// pass every test on the way out.
void TheShippedIniMatchesTheBuiltInDefaults() {
    TWHT::Config config;
    const bool loaded = config.LoadFromIni(THEWITNESS_SHIPPED_INI);
    Check(loaded, "the shipped HeadTracking.ini opens");
    for (const std::string& warning : config.warnings) {
        std::cout << "    " << warning << std::endl;
    }
    Check(config.warnings.empty(), "the shipped HeadTracking.ini needs no corrections");

    // Every key by name first. A comparison against the defaults cannot see a
    // DELETED key - LoadFromIni just keeps the default for one that is absent -
    // and the ini is now the DLL's compiled seed, so a key dropped from it ships
    // a config missing a documented setting.
    const std::string text = [] {
        std::ifstream in(THEWITNESS_SHIPPED_INI, std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }();
    for (const char* key : { "UDPPort", "LocalSmoothing", "RemoteSmoothing", "Enabled",
                             "LimitX", "LimitY", "LimitZ", "LimitZBack",
                             "CollisionEnabled", "CollisionMargin", "CollisionReleaseSmoothing",
                             "ToggleKey", "CycleModeKey", "ToggleYawModeKey",
                             "AutoEnable", "LogToFile", "WorldSpaceYaw", "LogDiagnostics" }) {
        const bool present = text.find(std::string("\n") + key + "=") != std::string::npos;
        if (!present) std::cout << "    missing key: " << key << std::endl;
        Check(present, "the shipped ini still carries every documented key");
    }

    const TWHT::Config defaults;
    Check(config.udpPort == defaults.udpPort, "the port matches the built-in default");
    Check(config.localSmoothing == defaults.localSmoothing, "local smoothing matches");
    Check(config.remoteSmoothing == defaults.remoteSmoothing, "remote smoothing matches");
    Check(config.positionEnabled == defaults.positionEnabled, "position enable matches");
    Check(NearEqual(config.posLimitX, defaults.posLimitX), "the lateral limit matches");
    Check(NearEqual(config.posLimitY, defaults.posLimitY), "the vertical limit matches");
    Check(NearEqual(config.posLimitZ, defaults.posLimitZ), "the forward limit matches");
    Check(NearEqual(config.posLimitZBack, defaults.posLimitZBack), "the back limit matches");
    Check(config.collisionEnabled == defaults.collisionEnabled,
          "the shipped ini leaves the lean clamp off until it is confirmed in game");
    Check(NearEqual(config.collisionMargin, defaults.collisionMargin),
          "the collision standoff matches");
    Check(NearEqual(config.collisionReleaseSmoothing, defaults.collisionReleaseSmoothing),
          "the release smoothing matches");
    Check(config.toggleKey == defaults.toggleKey, "the toggle key matches");
    Check(config.cycleModeKey == defaults.cycleModeKey, "the cycle key matches");
    Check(config.toggleYawModeKey == defaults.toggleYawModeKey, "the yaw mode key matches");
    Check(config.autoEnable == defaults.autoEnable, "the startup enable matches");
    Check(config.logToFile == defaults.logToFile, "the log setting matches");
    Check(config.worldSpaceYaw == defaults.worldSpaceYaw, "the yaw mode matches");
    Check(config.logDiagnostics == defaults.logDiagnostics, "the diagnostic setting matches");
}

// The README reproduces HeadTracking.ini in full, which is the only reason a
// reader can trust what it says a key defaults to. Nothing kept the two in step:
// the copy is made by hand, and an edit to the ini that stopped at the ini left
// the documented default disagreeing with the shipped one, silently and for as
// long as it took somebody to notice.
void TheReadmeReproducesTheShippedIni() {
    const auto readFile = [](const char* path) {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    };
    // Trailing whitespace and line endings are not the point; the content is.
    const auto normalise = [](std::string text) {
        std::string out;
        out.reserve(text.size());
        for (const char c : text) {
            if (c != '\r') out += c;
        }
        while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
        return out;
    };

    const std::string ini = readFile(THEWITNESS_SHIPPED_INI);
    const std::string readme = readFile(THEWITNESS_README);
    Check(!ini.empty() && !readme.empty(), "the ini and the README both open");

    const std::string open = "```ini\n";
    const std::size_t start = readme.find(open);
    Check(start != std::string::npos, "the README carries an ini block");
    if (start == std::string::npos) return;

    const std::size_t bodyStart = start + open.size();
    const std::size_t end = readme.find("```", bodyStart);
    Check(end != std::string::npos, "the README's ini block is closed");
    if (end == std::string::npos) return;

    Check(normalise(readme.substr(bodyStart, end - bodyStart)) == normalise(ini),
          "the README's config block is the shipped HeadTracking.ini, verbatim");
}

// The DLL carries its own copy of HeadTracking.ini, generated by CMake, and lays
// it down when no config file is next to the game exe. Nothing else compares the
// two: every other test reads the ini off disk, so a seed that had diverged
// would ship while the file, the README and every assertion above stayed right.
//
// The divergence is not hypothetical - configure_file runs @ONLY substitution,
// so an @token@ in the ini would be blanked in the compiled copy alone.
void TheCompiledSeedIsTheShippedIni() {
    std::ifstream in(THEWITNESS_SHIPPED_INI, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    Check(buffer.str() == std::string(TWHT::kDefaultConfigIni),
          "the ini compiled into the DLL is the shipped HeadTracking.ini, byte for byte");
}

}  // namespace

void RunConfigTests() {
    std::cout << "config\n";
    CleanValuesSurviveVerbatim();
    PartialNumbersAreRefused();
    NonFiniteValuesAreRefused();
    OutOfRangeValuesAreClamped();
    NegativePositionLimitsFallBack();
    MalformedBoolsAreReported();
    CollisionMarginIsHeldAboveTheNearPlane();
    PortsOutsideTheRangeAreRefused();
    UnbindableHotkeysAreRefused();
    DuplicateHotkeysAreRefused();
    RetiredBindAddressIsReported();
    RetiredPoseShapingKeysAreReported();
    TheShippedIniMatchesTheBuiltInDefaults();
    TheReadmeReproducesTheShippedIni();
    TheCompiledSeedIsTheShippedIni();
}
