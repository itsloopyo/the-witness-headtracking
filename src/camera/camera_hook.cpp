#include "pch.h"
#include "camera/camera_hook.h"

#include "camera/pose_conversion.h"
#include "camera/reticle_projection.h"
#include "core/debug_log.h"
#include "core/mod.h"
#include "game/build_profile.h"
#include "game/engine.h"
#include "game/world_trace.h"

#include <cameraunlock/camera/zoom_compensation.h>
#include <cameraunlock/hooks/hook_manager.h>
#include <cameraunlock/math/quat4.h>
#include <cameraunlock/math/vec3.h>

#include <cmath>
#include <intrin.h>

namespace TWHT {

using cameraunlock::math::Quat4;
using cameraunlock::math::Vec3;

namespace {

// How far the aim ray looks for the surface the reticle has to sit on. At this
// range a 0.30m lean displaces the aim point by around a pixel and a half across
// a 1920-wide frame, so a miss is projected as a pure direction instead.
constexpr float kAimTraceDistance = 250.0f;

// Below this the render eye and the aim eye are the same point, so the aim
// direction projects exactly at every depth and the frame needs no trace.
constexpr float kMinimumLean = 1e-4f;

// Seconds between LogDiagnostics lines. About twice a second, which is what the
// README and the ini comment promise.
constexpr float kDiagnosticLogInterval = 0.5f;

// Frames between the lean clamp's periodic state sample, on top of the line it
// writes whenever the state changes.
constexpr unsigned kLeanStateSampleFrames = 3600u;

// Metres the clean eye can move in one frame before the move is treated as a
// camera cut rather than as the player travelling. Set well above walking and
// falling speeds at any frame rate, so the cost of being wrong falls on the
// side that only discards a clamp allowance.
constexpr float kCameraCutDistance = 5.0f;

constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

// --- Hook state -------------------------------------------------------------

using SimUpdateFn = void(*)();
using RenderCameraSetupFn = void(*)();
using RenderCameraRefreshFn = std::uintptr_t(*)(void*);
using DrawSpriteFn = void(*)(void* texture, float* position, float width, float height,
                             float p5, float p6, float p7, float p8, std::uint32_t p9);
using DrawCircleFn = void(*)(float* position, std::uint32_t color, float radius, int segments);

SimUpdateFn g_origSimUpdate = nullptr;
RenderCameraSetupFn g_origRenderCameraSetup = nullptr;
RenderCameraRefreshFn g_origRenderCameraRefresh = nullptr;
DrawSpriteFn g_origDrawSprite = nullptr;
DrawCircleFn g_origDrawCircle = nullptr;

std::uintptr_t g_reticleDrawReturn = 0;
std::uintptr_t g_cursorDrawReturn = 0;
std::uintptr_t g_focusCircleDrawReturn = 0;

struct FrameState {
    // The refresh also updates puzzles. Inject only after the frame's refresh
    // returns, never during an internal camera rebuild.
    bool armed = false;
    bool applied = false;

    Vec3 cleanPosition;
    Quat4 cleanOrientation;
    Vec3 trackedPosition;
    Quat4 trackedOrientation;

    // Where the mouse is pointing, in the world. `aimPoint` is valid only when
    // `aimHit`; `aimDirection` always is.
    Vec3 aimDirection;
    Vec3 aimPoint;
    bool aimHit = false;

    // Where the reticle has to be drawn this frame for it to sit on the point
    // the mouse is aiming at, in the engine's screen UV space.
    bool reticleValid = false;
    bool reticleOffScreen = false;
    float reticleU = 0.0f;
    float reticleV = 0.0f;
    Engine::ScreenBasis cleanBasis;
    Engine::ScreenBasis renderBasis;
};

FrameState g_frame;

bool ProjectCursor(const float* position, reticle_projection::ScreenUv& uv) {
    const auto& clean = g_frame.cleanBasis;
    const Vec3 ray = (clean.corner - clean.origin
        + clean.axis_u * (2.0f * clean.scale * position[0])
        + clean.axis_v * (2.0f * clean.scale * position[1])).Normalized();
    const bool leaning = (g_frame.trackedPosition - clean.origin).SqrMagnitude()
                         > kMinimumLean * kMinimumLean;
    const auto hit = leaning ? world_trace::Cast(clean.origin, ray, kAimTraceDistance)
                             : world_trace::Hit{};
    const Vec3 direction = reticle_projection::AimVector(
        hit.hit, clean.origin + ray * hit.distance, ray, g_frame.trackedPosition);
    const auto& render = g_frame.renderBasis;
    return reticle_projection::Solve(render.corner - render.origin, render.axis_u,
                                    render.axis_v, render.scale, direction, uv)
        && uv.in_front && uv.u >= 0.0f && uv.u <= 1.0f
        && uv.v >= 0.0f && uv.v <= render.v_max;
}

// --- Diagnostics ------------------------------------------------------------

// A narrower field of view magnifies everything in the frame, head tracking
// included, so the pose is scaled to keep its screen displacement the same as
// it would be at the player's own FOV. Exactly 1.0 in ordinary play.
//
// Both numbers are the vertical FOV in degrees, read through the rules the
// engine's own getters apply - which is what makes the ratio meaningful. A
// horizontal number against a vertical one would still return a plausible
// factor, just one that is wrong by a constant everywhere, and the only symptom
// would be head tracking feeling weak in normal play. The gate is the log line
// below reading 1.0000 with nothing zoomed.
//
// The log fires on the first frame the camera updates rather than the first
// frame a pose arrives, so the basis can be checked with no tracker connected.
float ZoomFactorLoggedOnce(const Engine& engine) {
    const float fovLive = engine.GetFovVerticalLive();
    const float fovBase = engine.GetFovVerticalBase();
    const float factor = pose_conversion::ZoomFactor(fovLive, fovBase);

    static bool s_logged = false;
    if (!s_logged) {
        s_logged = true;
        HT_LOG("FOV basis: live=%.3f deg (vertical), base=%.3f deg (vertical), "
               "aspect=%d/%d, zoom factor=%.4f",
               fovLive, fovBase, engine.ScreenWidth(), engine.ScreenHeight(), factor);
    }
    return factor;
}

// Transitions alone cannot tell "the sweep runs and the room is open" from
// "the sweep is not running", and those need different fixes, so both flags are
// logged on change and sampled periodically. Only called while the clamp is
// enabled: with no query the two flags are both false every frame, which is
// byte-identical to the line a running clamp writes in an open room.
void ReportLeanState(bool inContact, bool queryFailed) {
    static bool s_haveState = false;
    static bool s_contact = false;
    static bool s_failed = false;
    static unsigned s_frames = 0;

    const bool changed = !s_haveState || inContact != s_contact || queryFailed != s_failed;
    const bool periodic = (++s_frames % kLeanStateSampleFrames) == 0u;
    if (!changed && !periodic) return;

    s_haveState = true;
    s_contact = inContact;
    s_failed = queryFailed;
    HT_LOG("Lean clamp: contact=%s queryFailed=%s", inContact ? "yes" : "no",
           queryFailed ? "yes" : "no");
}

// A lean moves the render eye off the aim eye, and from there only the impact
// POINT projects correctly at every range - so a cast that could not run means
// the frame falls back to the aim direction, which is right at one depth and
// drifts either side of it. Reported on the edge rather than every frame: it
// resolves as soon as a world is loaded, and a per-frame line would bury the
// log during a level change.
void ReportAimCastUnavailable(bool queried) {
    // Starts in the good state, so the first leaning frame of a session says
    // nothing rather than announcing a recovery from a failure that never
    // happened.
    static bool s_queried = true;
    if (queried == s_queried) return;
    s_queried = queried;
    if (!queried) {
        HT_LOG("Aim cast unavailable - the reticle falls back to the aim direction "
               "while the head is leaning, which is exact only at the range the "
               "shot lands.");
    } else {
        HT_LOG("Aim cast available again.");
    }
}

// Paced off the frame clock, not a frame count: a frame count writes this line
// four times as often on a 240Hz display as on a 60Hz one, and the cadence the
// docs quote has to hold on both.
void ReportPose(float yaw, float pitch, float roll, float zoomFactor,
                const world_trace::Hit& aim, float dt) {
    static float s_sinceLog = kDiagnosticLogInterval;
    s_sinceLog += dt;
    if (s_sinceLog < kDiagnosticLogInterval) return;
    s_sinceLog = 0.0f;

    // The applied rotation as an axis-angle in the camera's own frame. In
    // camera-local yaw a pure yaw reads as an axis of (0,0,+-1), a pure pitch as
    // (0,+-1,0) and a pure roll as (+-1,0,0). In world-space yaw - the default -
    // the yaw axis is the WORLD's vertical carried into the camera's frame, so
    // it reads as (0,0,+-1) only while the camera is level and tips toward
    // forward as the camera pitches. Euler angles would need their own
    // convention argued about first.
    const Quat4 cleanInverse = g_frame.cleanOrientation.Inverse();
    Quat4 delta = cleanInverse * g_frame.trackedOrientation;
    if (delta.w < 0.0f) delta = delta.Negated();
    const float sinHalf = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    const float angle = 2.0f * std::atan2(sinHalf, delta.w) * kRadToDeg;
    const float invSin = sinHalf > 1e-6f ? 1.0f / sinHalf : 0.0f;
    const Vec3 lean = cleanInverse.Rotate(g_frame.trackedPosition - g_frame.cleanPosition);

    HT_LOG("applied y/p/r=%.2f/%.2f/%.2f (zoom x%.4f)  %.2f deg about fwd/left/up "
           "%.2f/%.2f/%.2f  lean fwd/left/up=%.3f/%.3f/%.3f  aim=%s@%.2fm  "
           "reticle=%.4f,%.4f%s",
           yaw, pitch, roll, zoomFactor, angle,
           delta.x * invSin, delta.y * invSin, delta.z * invSin,
           lean.x, lean.y, lean.z, aim.hit ? "hit" : "miss", aim.distance,
           g_frame.reticleU, g_frame.reticleV,
           !g_frame.reticleValid ? " (no basis)"
                                 : (g_frame.reticleOffScreen ? " (hidden)" : ""));
}

}  // namespace

// --- Detours ----------------------------------------------------------------

void DetourSimUpdate() {
    // The simulation reads the camera state block all over - the interaction
    // pick's range test included - and the render camera setup left it holding
    // the head-tracked transform for the renderer. Put the clean one back so
    // aim stays on the mouse.
    if (g_frame.applied) {
        TheEngine().SetCameraStateTransform(g_frame.cleanPosition, g_frame.cleanOrientation);
        g_origRenderCameraSetup();
        g_frame.applied = false;
    }
    g_frame.armed = false;
    g_origSimUpdate();
    g_frame.armed = true;
}

std::uintptr_t DetourRenderCameraRefresh(void* camera) {
    const std::uintptr_t result = g_origRenderCameraRefresh(camera);
    if (g_frame.armed) {
        g_frame.armed = false;
        CameraHook::Instance().ApplyTracking(camera);
    }
    return result;
}

void DetourDrawSprite(void* texture, float* position, float width, float height,
                      float p5, float p6, float p7, float p8, std::uint32_t p9) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (position == nullptr
        || (caller != g_reticleDrawReturn && caller != g_cursorDrawReturn)
        || !g_frame.reticleValid) {
        g_origDrawSprite(texture, position, width, height, p5, p6, p7, p8, p9);
        return;
    }

    // Behind the eye or swung off the frame: drawing it clamped to an edge would
    // claim the aim lands somewhere it cannot.
    float u = g_frame.reticleU;
    float v = g_frame.reticleV;
    if (caller == g_cursorDrawReturn) {
        reticle_projection::ScreenUv uv;
        if (!ProjectCursor(position, uv)) return;
        u = uv.u;
        v = uv.v;
    } else if (g_frame.reticleOffScreen) {
        return;
    }

    // Put the caller's UV back afterwards. Whether this points at a per-call
    // temporary or at storage the HUD keeps is the caller's business, and if it
    // is kept then a frame that places no reticle - tracking toggled off, a
    // menu, a lost tracker - would otherwise leave the last tracked UV in it and
    // strand the reticle off centre with nothing in the log.
    const float saved[2] = { position[0], position[1] };
    position[0] = u;
    position[1] = v;
    g_origDrawSprite(texture, position, width, height, p5, p6, p7, p8, p9);
    position[0] = saved[0];
    position[1] = saved[1];
}

void DetourDrawCircle(float* position, std::uint32_t color, float radius, int segments) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (caller != g_focusCircleDrawReturn || !g_frame.reticleValid) {
        g_origDrawCircle(position, color, radius, segments);
        return;
    }

    reticle_projection::ScreenUv uv;
    if (!ProjectCursor(position, uv)) return;
    float projected[3] = { uv.u, uv.v, position[2] };
    g_origDrawCircle(projected, color, radius, segments);
}

// --- CameraHook -------------------------------------------------------------

CameraHook& CameraHook::Instance() {
    static CameraHook s;
    return s;
}

// The world-space eye offset for this frame's position sample, already cut to
// whatever the level leaves room for. Zero when position tracking is off, when
// the sample is not usable, or when the camera has no horizon to lean about.
//
// Every path that applies no lean resets the clamp, so an allowance the last
// room's wall cut down cannot ration the first lean taken in the next one.
Vec3 CameraHook::TrackedLean(const Vec3& cleanPosition, const Vec3& cleanForward,
                             float zoomFactor) {
    Mod& mod = Mod::Instance();

    // LeanOffset owns the validation of what comes out of the pipeline - a
    // non-finite component is refused there, alongside the degenerate basis -
    // so the boundary maths stays in the file the tests cover.
    Vec3 offset;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (!mod.GetPositionOffset(x, y, z)
        || !pose_conversion::LeanOffset(cleanForward, x, y, z, zoomFactor, offset)) {
        m_leanClamp.Reset();
        return Vec3();
    }

    // Unconditionally, including for a lean too small to reach anything: the
    // clamp drops its own allowance for one of those, which is what a head
    // passing back through centre has to do to the wall it was just held off.
    const bool clampEnabled = mod.IsCollisionClampEnabled();
    offset = m_leanClamp.Apply(cleanPosition, offset, mod.LastFrameDelta(),
                               clampEnabled ? &world_trace::LeanQuery : nullptr, nullptr);
    if (clampEnabled) {
        ReportLeanState(m_leanClamp.InContact(), m_leanClamp.LastQueryFailed());
    }
    return offset;
}

void CameraHook::ApplyTracking(void* camera) {
    Engine& engine = TheEngine();
    Mod& mod = Mod::Instance();
    const float zoomFactor = ZoomFactorLoggedOnce(engine);

    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    if (!mod.GetProcessedRotation(yaw, pitch, roll)) {
        g_frame.applied = false;
        g_frame.reticleValid = false;
        m_leanClamp.Reset();
        return;
    }

    const Vec3 cleanPos = engine.GetCameraPosition();
    const Quat4 cleanQuat = engine.GetCameraOrientation().Normalized();
    const Vec3 cleanForward = cleanQuat.Rotate(Vec3(1.0f, 0.0f, 0.0f));

    // A camera cut - a laser ending, the elevator, a mountain transition - puts
    // the eye somewhere unrelated in one frame. The clamp's allowance is a
    // scalar with no memory of which room measured it, so carrying it across
    // rations the first lean in the new room through the previous room's wall
    // for the length of the release ease. The engine has no cut signal to read,
    // and no walk, sprint or fall covers this in a frame.
    if (m_haveLastCleanPosition
        && (cleanPos - m_lastCleanPosition).SqrMagnitude() > kCameraCutDistance * kCameraCutDistance) {
        m_leanClamp.Reset();
    }
    m_lastCleanPosition = cleanPos;
    m_haveLastCleanPosition = true;

    if (zoomFactor != 1.0f) {
        yaw = cameraunlock::camera::ScaleAngleForZoom(yaw, zoomFactor);
        pitch = cameraunlock::camera::ScaleAngleForZoom(pitch, zoomFactor);
        // Roll rotates the picture about the view axis by the same angle at
        // every field of view, so scaling it would flatten a tilt the player is
        // holding and buy nothing.
    }

    const Quat4 trackedQuat =
        pose_conversion::TrackedOrientation(cleanQuat, yaw, pitch, roll,
                                            mod.IsWorldSpaceYaw());
    const Vec3 trackedPos = cleanPos + TrackedLean(cleanPos, cleanForward, zoomFactor);

    // What the mouse is aiming at, found along the CLEAN ray - the same one the
    // simulation's pick will use - so the reticle marks the interaction point
    // rather than wherever the head happens to be looking.
    //
    // Only needed while the render eye sits somewhere other than the aim eye.
    // Rotating the head does not move the eye, so with no lean the aim ray is
    // the SAME ray whatever the head is doing and its direction projects
    // exactly, at every depth, without a cast.
    const bool leaning = (trackedPos - cleanPos).SqrMagnitude() > kMinimumLean * kMinimumLean;
    const world_trace::Hit aim = leaning
        ? world_trace::Cast(cleanPos, cleanForward, kAimTraceDistance)
        : world_trace::Hit{};
    if (leaning) ReportAimCastUnavailable(aim.queried);

    g_frame.cleanPosition = cleanPos;
    g_frame.cleanOrientation = cleanQuat;
    g_frame.trackedPosition = trackedPos;
    g_frame.trackedOrientation = trackedQuat;
    g_frame.aimDirection = cleanForward;
    g_frame.aimHit = aim.hit;
    g_frame.aimPoint = cleanPos + cleanForward * aim.distance;

    engine.SetCameraPosition(trackedPos);
    engine.SetCameraOrientation(trackedQuat);
    g_origRenderCameraSetup();
    engine.SetCameraPosition(cleanPos);
    engine.SetCameraOrientation(cleanQuat);
    g_frame.applied = true;
    UpdateHudProjection(camera);

    if (mod.IsDiagnosticLoggingEnabled()) {
        ReportPose(yaw, pitch, roll, zoomFactor, aim, mod.LastFrameDelta());
    }
}

void CameraHook::UpdateHudProjection(void* camera) {
    Engine& engine = TheEngine();
    Engine::ScreenBasis basis;
    g_frame.reticleValid = false;
    if (!engine.GetScreenBasis(basis, camera)) return;

    // Keep the engine's basis clean for puzzle updates and world-space cursors.
    // Only the private projection basis follows the rendered view.
    const Quat4 delta = g_frame.trackedOrientation * g_frame.cleanOrientation.Inverse();
    const Vec3 cornerFromOrigin = delta.Rotate(basis.corner - basis.origin);
    g_frame.cleanBasis = basis;
    g_frame.renderBasis = basis;
    g_frame.renderBasis.corner = basis.origin + cornerFromOrigin;
    g_frame.renderBasis.axis_u = delta.Rotate(basis.axis_u);
    g_frame.renderBasis.axis_v = delta.Rotate(basis.axis_v);

    const Vec3 direction = reticle_projection::AimVector(
        g_frame.aimHit, g_frame.aimPoint, g_frame.aimDirection, g_frame.trackedPosition);

    reticle_projection::ScreenUv uv;
    if (reticle_projection::Solve(cornerFromOrigin, g_frame.renderBasis.axis_u,
                                  g_frame.renderBasis.axis_v, basis.scale,
                                  direction, uv)) {
        g_frame.reticleValid = true;
        g_frame.reticleOffScreen = !uv.in_front || uv.u < 0.0f || uv.u > 1.0f
                                   || uv.v < 0.0f || uv.v > basis.v_max;
        g_frame.reticleU = uv.u;
        g_frame.reticleV = uv.v;
    }
}

void CameraHook::ConfigureLeanClamp() {
    const Config& config = Mod::Instance().GetConfig();
    cameraunlock::camera::LeanClampSettings lean;
    lean.skin = config.collisionMargin;
    lean.release_smoothing = config.collisionReleaseSmoothing;
    m_leanClamp.SetSettings(lean);
    HT_LOG("Lean collision clamp %s (margin %.2fm, release %.2f).",
           Mod::Instance().IsCollisionClampEnabled() ? "enabled" : "disabled",
           lean.skin, lean.release_smoothing);
}

// Creates and enables every detour the mod needs, or none: a partial install
// would leave the engine running half a sandwich, so the first failure takes
// back the hooks created before it.
bool CameraHook::CreateHooks(const Engine& engine, const BuildProfile& profile) {
    using cameraunlock::hooks::HookManager;
    using cameraunlock::hooks::HookStatus;
    using cameraunlock::hooks::HookStatusToString;

    HookManager& hooks = HookManager::Instance();
    const HookStatus init = hooks.Initialize();
    if (init != HookStatus::Ok && init != HookStatus::ErrorAlreadyInitialized) {
        HT_LOG("ERROR: MinHook init failed (%s).", HookStatusToString(init));
        return false;
    }

    struct HookSpec {
        const char* name;
        std::uint32_t rva;
        void* detour;
        void** original;
    };
    const HookSpec specs[] = {
        { "sim update",            profile.SimUpdate,
          reinterpret_cast<void*>(&DetourSimUpdate),
          reinterpret_cast<void**>(&g_origSimUpdate) },
        { "render camera refresh", profile.RenderCameraRefresh,
          reinterpret_cast<void*>(&DetourRenderCameraRefresh),
          reinterpret_cast<void**>(&g_origRenderCameraRefresh) },
        { "HUD sprite draw",       profile.DrawSprite,
          reinterpret_cast<void*>(&DetourDrawSprite),
          reinterpret_cast<void**>(&g_origDrawSprite) },
        { "HUD focus circle",      profile.DrawCircle,
          reinterpret_cast<void*>(&DetourDrawCircle),
          reinterpret_cast<void**>(&g_origDrawCircle) },
    };

    for (const HookSpec& spec : specs) {
        void* target = reinterpret_cast<void*>(engine.Address(spec.rva));
        const HookStatus status = hooks.CreateHook(target, spec.detour, spec.original);
        if (status != HookStatus::Ok) {
            HT_LOG("ERROR: could not hook %s at +0x%X (%s). Removing the hooks "
                   "installed so far and staying dormant.",
                   spec.name, spec.rva, HookStatusToString(status));
            hooks.RemoveAllHooks();
            return false;
        }
    }

    const HookStatus enabled = hooks.EnableAllHooks();
    if (enabled != HookStatus::Ok) {
        HT_LOG("ERROR: could not enable the camera hooks (%s). Removing them and "
               "staying dormant.", HookStatusToString(enabled));
        hooks.RemoveAllHooks();
        return false;
    }
    return true;
}

void CameraHook::Install() {
    if (m_installed) return;

    HMODULE gameModule = GetModuleHandleW(nullptr);
    const BuildProfile* profile = ResolveBuildProfile(gameModule);
    if (profile == nullptr) {
        HT_LOG("Dormant: %s. No hooks installed; the game runs unmodified.",
               DescribeMismatch(gameModule));
        return;
    }
    HT_LOG("Build profile '%s' matched.", profile->Name);

    ConfigureLeanClamp();

    Engine& engine = TheEngine();
    if (!engine.Bind(gameModule, *profile)) {
        HT_LOG("ERROR: could not bind to the game module.");
        return;
    }

    g_reticleDrawReturn = engine.Address(profile->ReticleDrawReturn);
    g_cursorDrawReturn = engine.Address(profile->CursorDrawReturn);
    g_focusCircleDrawReturn = engine.Address(profile->FocusCircleDrawReturn);
    g_origRenderCameraSetup = reinterpret_cast<RenderCameraSetupFn>(
        engine.Address(profile->RenderCameraSetup));

    if (!CreateHooks(engine, *profile)) return;

    m_installed = true;
    HT_LOG("Camera hooks installed.");
}

}  // namespace TWHT
