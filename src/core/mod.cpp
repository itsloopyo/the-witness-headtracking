#include "pch.h"
#include "core/mod.h"
#include "core/debug_log.h"

#include "camera/camera_hook.h"

#include <cstring>

namespace TWHT {

Mod& Mod::Instance() {
    // Deliberately never destroyed. A function-local static registers its
    // destructor on the CRT's atexit table, and the CRT runs that table on
    // DLL_PROCESS_DETACH unconditionally, the process-terminating case
    // included. Mod's members join three worker threads, and joining under the
    // loader lock against threads the OS has already killed mid-lock is a hang
    // on quit with no crash dump to show for it.
    static Mod* s_instance = new Mod();
    return *s_instance;
}

static std::string DirectoryOf(HMODULE hModule) {
    char path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(hModule, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    std::string s(path, n);
    auto slash = s.find_last_of("\\/");
    return (slash == std::string::npos) ? std::string{} : s.substr(0, slash);
}

void Mod::ApplyConfigToSession() {
    // No sensitivity or inversion is set on either processor, for rotation or
    // position: the core defaults are 1:1 with no inversion, and that is the
    // pose the tracker sent. Shaping it is the tracker's job.

    // Position pipeline.
    cameraunlock::PositionSettings pos;
    pos.limit_x = m_config.posLimitX;
    // The clamp is [-limit_y_down, +limit_y] and limit_y_down carries its own
    // default, so mirror the one configured vertical limit the way
    // PositionSettings::Symmetric does. Left unset, raising LimitY widened the
    // upward budget only and downward travel stayed pinned at 0.20m.
    pos.limit_y = m_config.posLimitY;
    pos.limit_y_down = m_config.posLimitY;
    pos.limit_z = m_config.posLimitZ;
    pos.limit_z_back = m_config.posLimitZBack;
    // Through the session rather than straight at the processor: the session
    // owns the two smoothing values, and a raw SetSettings carries this struct's
    // own smoothing fields into the processor instead until the next frame
    // corrects it.
    m_session.SetPositionSettings(pos);
    // The core default (0.15) synthesises translation from head rotation to
    // cancel a webcam pivoting in front of the face. Our trackers report
    // position directly, so the term only injects phantom rotation-coupled
    // movement.
    m_session.GetPositionProcessor().SetTrackerPivotForward(0.0f);

    // Both smoothing parameters cover rotation and position; the session picks
    // between them per connection from the receiver's source-address check, so
    // a switch from a local OpenTrack instance to a phone on WiFi mid-session
    // needs no restart.
    m_session.SetLocalSmoothing(m_config.localSmoothing);
    m_session.SetRemoteSmoothing(m_config.remoteSmoothing);

    m_session.SetMode(m_config.positionEnabled
                          ? cameraunlock::TrackingMode::RotationAndPosition
                          : cameraunlock::TrackingMode::RotationOnly);
}

void Mod::StartReceiver() {
    m_receiver.SetLog([](const std::string& msg) {
        HT_LOG("UDP: %s", msg.c_str());
    });
    if (!m_receiver.Start(m_config.udpPort)) {
        // No cause named here: the line above this one carries the OS's own
        // reason for the failure, and a port conflict is only one of them.
        // Everything else in the mod stays up, and the receiver's supervisor
        // reclaims the port on its own once it frees.
        HT_LOG("UDP bind on port %u deferred; retry running.", m_config.udpPort);
    } else {
        HT_LOG("UDP receiver started on port %u.", m_config.udpPort);
    }
}

bool Mod::Initialize(HMODULE hModule) {
    m_gameDir = DirectoryOf(hModule);
    if (m_gameDir.empty()) return false;

    const std::string iniPath = m_gameDir + "\\" + kConfigFileName;
    int seedError = 0;
    const bool seedFailed = GetFileAttributesA(iniPath.c_str()) == INVALID_FILE_ATTRIBUTES
                            && !WriteDefaultConfig(iniPath, seedError);
    const bool configLoaded = m_config.LoadFromIni(iniPath);

    if (m_config.logToFile) OpenLogFile();
    HT_LOG("=== %s v%s ===", kModName, kModVersion);
    HT_LOG("Initialize: dir=%s", m_gameDir.c_str());
    if (seedFailed) {
        // Named separately from the read failure below, which it causes: "could
        // not read the ini" sends a user looking for a corrupt file when the
        // problem is a game directory they cannot write to.
        HT_LOG("WARN: could not write %s (%s).", iniPath.c_str(), std::strerror(seedError));
    }
    if (!configLoaded) {
        HT_LOG("WARN: could not read %s - using built-in defaults.", iniPath.c_str());
    }
    // Parse-time corrections, collected before the log existed.
    for (const std::string& w : m_config.warnings) {
        HT_LOG("%s", w.c_str());
    }

    ApplyConfigToSession();
    StartReceiver();

    CameraHook::Instance().Install();

    // Before the poller exists, or a key pressed in the gap is overwritten here.
    m_worldSpaceYaw.store(m_config.worldSpaceYaw, std::memory_order_relaxed);
    m_enabled.store(m_config.autoEnable, std::memory_order_release);

    m_hotkeys.Start(m_config);

    m_initialized.store(true, std::memory_order_release);

    HT_LOG("Initialized. Enabled=%s, WorldSpaceYaw=%s.",
           m_enabled.load() ? "true" : "false",
           m_config.worldSpaceYaw ? "true" : "false");
    return true;
}

void Mod::SetEnabled(bool enabled) {
    m_enabled.store(enabled, std::memory_order_release);
    HT_LOG("Enabled=%s", enabled ? "true" : "false");
}

void Mod::Toggle() { SetEnabled(!IsEnabled()); }

void Mod::CycleTrackingMode() {
    switch (m_session.CycleMode()) {
    case cameraunlock::TrackingMode::RotationAndPosition:
        HT_LOG("TrackingMode=rotation+position");
        break;
    case cameraunlock::TrackingMode::RotationOnly:
        HT_LOG("TrackingMode=rotation only");
        break;
    case cameraunlock::TrackingMode::PositionOnly:
        HT_LOG("TrackingMode=position only");
        break;
    }
}

void Mod::ToggleYawMode() {
    bool v = !m_worldSpaceYaw.load(std::memory_order_relaxed);
    m_worldSpaceYaw.store(v, std::memory_order_relaxed);
    HT_LOG("WorldSpaceYaw=%s", v ? "true" : "false");
}

// The receiver names the first packet and any change of tracker source, and the
// pose it holds is latched: a tracker that stops sending leaves the last pose
// applied with nothing said about it anywhere. That silence is what a player
// reports as "head tracking just stopped", so time the gap between packets and
// name both edges of it.
//
// Two seconds, off the frame clock rather than the wall clock: long enough that
// a network hiccup does not fill the log with a pair of lines every time, short
// enough to land in the log the player attaches.
void Mod::ReportTrackerSilence(float dt) {
    const int64_t receiveTs = m_receiver.GetLastReceiveTimestamp();
    if (receiveTs != m_lastReceiveTs) {
        m_lastReceiveTs = receiveTs;
        m_trackerSilence = 0.0f;
        if (!m_trackerSending) {
            m_trackerSending = true;
            HT_LOG("Tracker packets arriving.");
        }
        return;
    }
    if (!m_trackerSending) return;

    constexpr float kSilenceSeconds = 2.0f;
    m_trackerSilence += dt;
    if (m_trackerSilence >= kSilenceSeconds) {
        m_trackerSending = false;
        HT_LOG("No tracker packet for %.1fs - the last pose stays applied.",
               m_trackerSilence);
    }
}

bool Mod::GetProcessedRotation(float& yaw, float& pitch, float& roll) {
    if (!IsEnabled()) return false;

    const float dt = m_frameClock.Tick();
    m_lastDelta = dt;
    ReportTrackerSilence(dt);
    if (!m_session.Update(dt)) return false;

    return m_session.GetRotation(yaw, pitch, roll);
}

bool Mod::GetPositionOffset(float& x, float& y, float& z) const {
    return m_session.GetPositionOffset(x, y, z);
}

} // namespace TWHT
