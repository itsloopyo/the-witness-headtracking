#pragma once

#include "core/config.h"
#include "input/hotkey_handler.h"

#include <atomic>
#include <cstdint>
#include <string>

#include <cameraunlock/protocol/udp_receiver.h>
#include <cameraunlock/time/frame_clock.h>
#include <cameraunlock/tracking/head_tracking_session.h>

namespace TWHT {

class Mod {
public:
    static Mod& Instance();

    // There is no Shutdown. The DLL pins itself in DllMain and the game imports
    // it statically, so it is never unloaded and the only teardown is process
    // exit, which the OS handles. An orderly path would have to join three
    // worker threads and pull MinHook's trampolines out from under a live
    // render thread, both from inside the loader lock.
    bool Initialize(HMODULE hModule);

    bool IsInitialized() const { return m_initialized.load(std::memory_order_acquire); }
    bool IsEnabled()     const { return m_enabled.load    (std::memory_order_acquire); }

    void SetEnabled(bool enabled);
    void Toggle();
    void CycleTrackingMode();
    void ToggleYawMode();

    bool IsWorldSpaceYaw() const { return m_worldSpaceYaw.load(std::memory_order_relaxed); }

    const Config& GetConfig() const { return m_config; }

    // Runs the shared pipeline for this frame (interpolation -> deadzone ->
    // smooth -> sensitivity) and returns the processed yaw/pitch/roll in
    // degrees. False if tracking is disabled or no fresh data has arrived.
    // Call once per render frame.
    bool GetProcessedRotation(float& yaw, float& pitch, float& roll);

    // Processed head position offset in meters (camera-local) from the
    // latest GetProcessedRotation call. False when position tracking is
    // off or no position sample exists.
    bool GetPositionOffset(float& x, float& y, float& z) const;

    // Seconds covered by the most recent GetProcessedRotation call. The lean
    // clamp's release ease needs the same delta the pose pipeline used.
    float LastFrameDelta() const { return m_lastDelta; }

    bool IsCollisionClampEnabled() const { return m_config.collisionEnabled; }
    bool IsDiagnosticLoggingEnabled() const { return m_config.logDiagnostics; }

    Mod(const Mod&) = delete;
    Mod& operator=(const Mod&) = delete;

private:
    Mod()  = default;
    ~Mod() = default;

    void ApplyConfigToSession();
    void StartReceiver();
    void ReportTrackerSilence(float dt);

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_worldSpaceYaw{true};

    Config m_config;
    std::string m_gameDir;
    float m_lastDelta = 0.0f;
    // Render-thread only, alongside m_lastDelta. Starts "sending" so a session
    // where the tracker never starts gets the silence line rather than nothing.
    bool m_trackerSending = true;
    float m_trackerSilence = 0.0f;
    int64_t m_lastReceiveTs = 0;

    using Session = cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver>;
    // The session picks between LocalSmoothing and RemoteSmoothing from the
    // receiver's source-address check. That wiring is compile-time detected, so
    // a receiver without IsRemoteConnection() would silently pin every session
    // to the local value instead of failing to build.
    static_assert(Session::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection() for per-connection smoothing");

    cameraunlock::UdpReceiver m_receiver;
    Session m_session{m_receiver};
    cameraunlock::time::FrameClock m_frameClock;

    HotkeyHandler m_hotkeys;
};

} // namespace TWHT
