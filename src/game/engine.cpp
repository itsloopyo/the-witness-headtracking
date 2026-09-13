#include "pch.h"
#include "game/engine.h"

namespace TWHT {

using cameraunlock::math::Quat4;
using cameraunlock::math::Vec3;

namespace {

const float* FloatsAt(std::uintptr_t addr) { return reinterpret_cast<const float*>(addr); }
float* MutableFloatsAt(std::uintptr_t addr) { return reinterpret_cast<float*>(addr); }

Vec3 ReadVec3(std::uintptr_t addr) {
    const float* f = FloatsAt(addr);
    return Vec3(f[0], f[1], f[2]);
}

} // namespace

Engine& TheEngine() {
    static Engine s;
    return s;
}

bool Engine::Bind(void* moduleBase, const BuildProfile& profile) {
    m_base = reinterpret_cast<std::uintptr_t>(moduleBase);
    m_profile = &profile;
    return m_base != 0;
}

Vec3 Engine::GetCameraPosition() const {
    return ReadVec3(m_base + m_profile->CameraPosition);
}

Quat4 Engine::GetCameraOrientation() const {
    const float* f = FloatsAt(m_base + m_profile->CameraOrientation);
    return Quat4(f[0], f[1], f[2], f[3]);
}

void Engine::SetCameraPosition(const Vec3& v) {
    float* f = MutableFloatsAt(m_base + m_profile->CameraPosition);
    f[0] = v.x;
    f[1] = v.y;
    f[2] = v.z;
}

void Engine::SetCameraOrientation(const Quat4& q) {
    float* f = MutableFloatsAt(m_base + m_profile->CameraOrientation);
    f[0] = q.x;
    f[1] = q.y;
    f[2] = q.z;
    f[3] = q.w;
}

void Engine::SetCameraStateTransform(const Vec3& position, const Quat4& orientation) {
    float* p = MutableFloatsAt(m_base + m_profile->CameraStatePosition);
    p[0] = position.x;
    p[1] = position.y;
    p[2] = position.z;
    float* q = MutableFloatsAt(m_base + m_profile->CameraStateOrientation);
    q[0] = orientation.x;
    q[1] = orientation.y;
    q[2] = orientation.z;
    q[3] = orientation.w;
}

float Engine::GetFovVerticalLive() const {
    // The negative case is the engine's own, not a guard against the
    // impossible: the live FOV holds -1 until the first camera update runs, and
    // the projection substitutes the tuning table's `fov_vertical` for it over
    // exactly that window.
    const float live = *FloatsAt(m_base + m_profile->FovVerticalLive);
    if (live < 0.0f) return *FloatsAt(m_base + m_profile->FovVerticalDefault);
    return live;
}

float Engine::GetFovVerticalBase() const {
    // Arming the alternate setting moves the options slider's write target onto
    // it, so `user_fov_vertical` goes stale from that point and reading it raw
    // would put a wrong FOV under the zoom ratio for the rest of the session.
    if (*FloatsAt(m_base + m_profile->FovVerticalAltActive) > 0.0f) {
        return *FloatsAt(m_base + m_profile->FovVerticalAltSetting);
    }
    return *FloatsAt(m_base + m_profile->FovVerticalSetting);
}

// Zero before the display object exists, which is the first frames of a
// launch. Only the FOV log line reads these, and it prints the zeros rather
// than waiting for a size it does not need.
int Engine::ReadDisplayInt(std::uint32_t fieldOffset) const {
    const std::uintptr_t display = *reinterpret_cast<std::uintptr_t*>(m_base + m_profile->DisplayPtr);
    if (display == 0) return 0;
    return *reinterpret_cast<const int*>(display + fieldOffset);
}

int Engine::ScreenWidth() const { return ReadDisplayInt(m_profile->DisplayWidth); }

int Engine::ScreenHeight() const { return ReadDisplayInt(m_profile->DisplayHeight); }

void* Engine::Globals() const {
    return *reinterpret_cast<void**>(m_base + m_profile->GlobalsPtr);
}

bool Engine::GetScreenBasis(ScreenBasis& out, void* camera) const {
    const std::uintptr_t cam = camera != nullptr
        ? reinterpret_cast<std::uintptr_t>(camera)
        : *reinterpret_cast<std::uintptr_t*>(m_base + m_profile->RenderCameraPtr);
    if (cam == 0) return false;
    out.scale  = *FloatsAt(cam + m_profile->CamScale);
    out.origin = ReadVec3(cam + m_profile->CamOrigin);
    out.corner = ReadVec3(cam + m_profile->CamCorner);
    out.axis_u = ReadVec3(cam + m_profile->CamAxisU);
    out.axis_v = ReadVec3(cam + m_profile->CamAxisV);
    out.v_max  = *FloatsAt(cam + m_profile->CamUvMaxV);
    return out.scale > 0.0f && out.v_max > 0.0f;
}

} // namespace TWHT
