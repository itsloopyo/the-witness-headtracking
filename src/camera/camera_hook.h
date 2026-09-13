#pragma once

#include <cstdint>

#include <cameraunlock/camera/lean_clamp.h>
#include <cameraunlock/math/vec3.h>

namespace TWHT {

class Engine;
struct BuildProfile;

// Head tracking injection for The Witness.
//
// Both simulation and camera refresh run interaction logic. Tracking enters
// the render cameras after refresh and is removed before the next simulation.
//
// There is no gameplay gate to write. The engine skips the whole
// simulate-and-set-up-cameras block while the pause menu owns the frame, so
// none of these hooks runs and the view holds still on its own.
class CameraHook {
public:
    static CameraHook& Instance();

    void Install();

    bool IsActive() const { return m_installed; }

    CameraHook(const CameraHook&) = delete;
    CameraHook& operator=(const CameraHook&) = delete;

private:
    CameraHook() = default;
    ~CameraHook() = default;

    friend std::uintptr_t DetourRenderCameraRefresh(void*);

    void ApplyTracking(void* camera);
    void UpdateHudProjection(void* camera);

    cameraunlock::math::Vec3 TrackedLean(const cameraunlock::math::Vec3& cleanPosition,
                                         const cameraunlock::math::Vec3& cleanForward,
                                         float zoomFactor);

    void ConfigureLeanClamp();
    bool CreateHooks(const Engine& engine, const BuildProfile& profile);

    bool m_installed = false;
    cameraunlock::camera::LeanClamp m_leanClamp;

    // Previous frame's clean eye, for spotting a camera cut. The clamp's
    // allowance is a scalar with no memory of the room that measured it.
    cameraunlock::math::Vec3 m_lastCleanPosition;
    bool m_haveLastCleanPosition = false;
};

} // namespace TWHT
