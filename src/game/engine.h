#pragma once

#include <cstdint>

#include <cameraunlock/math/quat4.h>
#include <cameraunlock/math/vec3.h>

#include "game/build_profile.h"

namespace TWHT {

// Typed access to the engine state the camera hook reads and writes.
//
// The engine is z-up and right-handed, and its camera's local axes are
// +x forward, +y left, +z up - which is why the tuning table spells the eye
// offset `player_camera_forward` / `player_camera_left` / `player_camera_up`.
// Every conversion between that frame and the tracker's happens here or in
// camera_hook.cpp, once, at the boundary.
class Engine {
public:
    // The screen-ray basis the engine unprojects through. A UV (u, v) maps to
    //   normalize(corner + 2*scale*u*axis_u + 2*scale*v*axis_v - origin)
    // with u across the width and v across the height, both from the
    // bottom-left. u runs to 1.0 and v to `v_max`; the reticle's resting UV is
    // (0.5, 0.5 * v_max).
    struct ScreenBasis {
        cameraunlock::math::Vec3 origin;
        cameraunlock::math::Vec3 corner;
        cameraunlock::math::Vec3 axis_u;
        cameraunlock::math::Vec3 axis_v;
        float scale = 0.0f;
        // The top edge's v. The engine derives it from the aspect its own
        // projection divides by, which is not always the window's - it forces
        // 1.0 when rendering square - so it is read rather than recomputed.
        float v_max = 0.0f;
    };

    bool Bind(void* moduleBase, const BuildProfile& profile);
    bool IsBound() const { return m_base != 0; }

    const BuildProfile& Profile() const { return *m_profile; }

    cameraunlock::math::Vec3 GetCameraPosition() const;
    cameraunlock::math::Quat4 GetCameraOrientation() const;
    void SetCameraPosition(const cameraunlock::math::Vec3& v);
    void SetCameraOrientation(const cameraunlock::math::Quat4& q);

    void SetCameraStateTransform(const cameraunlock::math::Vec3& position,
                                 const cameraunlock::math::Quat4& orientation);

    // Vertical FOV in degrees, both through the same rules the engine's own
    // getters apply. Live is what this frame is rendered at; base is the
    // player's un-zoomed setting, so the ratio between them is 1.0 whenever the
    // game is not zooming.
    float GetFovVerticalLive() const;
    float GetFovVerticalBase() const;

    int ScreenWidth() const;
    int ScreenHeight() const;

    // False when the render camera object has not been created yet (before the
    // first frame of a loaded world). `camera` is the object the refresh hook
    // was handed; passing null reads it from the global.
    bool GetScreenBasis(ScreenBasis& out, void* camera = nullptr) const;

    void* Globals() const;

    // Function pointers, resolved once at bind time.
    std::uintptr_t Address(std::uint32_t rva) const { return m_base + rva; }

private:
    int ReadDisplayInt(std::uint32_t fieldOffset) const;

    std::uintptr_t m_base = 0;
    const BuildProfile* m_profile = nullptr;
};

Engine& TheEngine();

} // namespace TWHT
