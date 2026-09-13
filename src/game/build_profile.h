#pragma once

#include <cstdint>

#include <cameraunlock/memory/pe_fingerprint.h>

namespace TWHT {

// Everything the camera hook needs from one shipped build of
// witness64_d3d11.exe, as RVAs from the module base.
//
// Append-only: a patched build gets a NEW profile at the top of
// kKnownProfiles, never an edit to an existing one, so a player who has not
// taken the patch keeps matching their old entry by PE fingerprint.
struct BuildProfile {
    const char* Name;
    cameraunlock::memory::PeFingerprint Fingerprint;

    // --- Functions we hook -------------------------------------------------
    // Per-frame world simulation. Runs the camera update and the interaction
    // pick, both of which must see the un-tracked camera.
    std::uint32_t SimUpdate;
    // Pushes the global camera transform into the render camera objects. Runs
    // after SimUpdate and before rendering, so everything it feeds - view
    // matrix, frustum culling, HUD projection - is what the player sees.
    std::uint32_t RenderCameraSetup;
    // Rebuilds the screen-ray basis AND updates puzzle picking and tracing.
    // Must finish on the clean camera before tracking enters the render state.
    std::uint32_t RenderCameraRefresh;
    // Textured-quad draw shared by every HUD sprite.
    std::uint32_t DrawSprite;
    std::uint32_t DrawCircle;

    // --- Return addresses inside the HUD draw ------------------------------
    // DrawSprite is shared, so the reticle is identified by which call site it
    // returns to rather than by texture identity.
    std::uint32_t ReticleDrawReturn;
    std::uint32_t CursorDrawReturn;
    std::uint32_t FocusCircleDrawReturn;

    // --- Functions we call -------------------------------------------------
    // Collects the entities whose bounds meet a segment into a growable array.
    std::uint32_t GatherEntitiesAlongSegment;
    // Resets a 0x70-byte ray hit record (distance to FLT_MAX, hit flag clear).
    std::uint32_t InitRayHit;
    // Nearest-hit ray cast over a gathered entity array. Pure geometry.
    std::uint32_t CastRay;
    // free() on the pointer field of a gathered array.
    std::uint32_t FreeGatheredArray;

    // --- Globals -----------------------------------------------------------
    std::uint32_t CameraPosition;    // 3 floats, world metres, z up
    std::uint32_t CameraOrientation; // 4 floats, quaternion xyzw
    // The copy RenderCameraSetup pushes out for the frame. Read all over the
    // simulation as well as the renderer, so it is put back clean before the
    // next simulation runs.
    std::uint32_t CameraStatePosition;
    std::uint32_t CameraStateOrientation;
    // Vertical FOV, all in degrees. The engine reaches these through two of its
    // own getters rather than reading them raw, and so does Engine, because the
    // rules are not guessable from the values: the live FOV is negative until
    // the first camera update and the projection substitutes the tuning table's
    // `fov_vertical` for it, and while the alternate setting is armed it is the
    // alternate - not `user_fov_vertical` - that the options slider writes.
    std::uint32_t FovVerticalLive;       // what the frame is rendered at
    std::uint32_t FovVerticalDefault;    // tuning table `fov_vertical`
    std::uint32_t FovVerticalSetting;    // `user_fov_vertical`, the FOV slider
    std::uint32_t FovVerticalAltActive;  // > 0 while the alternate setting is armed
    std::uint32_t FovVerticalAltSetting;
    std::uint32_t RenderCameraPtr;   // pointer to the object holding the screen basis
    std::uint32_t GlobalsPtr;        // pointer to the engine globals (entity manager)
    std::uint32_t DisplayPtr;        // pointer to the display object

    // --- Field offsets inside *RenderCameraPtr -----------------------------
    // Screen-ray basis: a UV (u, v) unprojects to
    //   normalize(Corner + 2*Scale*u*AxisU + 2*Scale*v*AxisV - Origin)
    // with u in [0, 1] left to right and v in [0, height/width] bottom to top.
    std::uint32_t CamScale;
    std::uint32_t CamOrigin;
    std::uint32_t CamCorner;
    std::uint32_t CamAxisU;
    std::uint32_t CamAxisV;
    // The v the top edge of the frame sits at, which the engine works out from
    // the FOV and the aspect it is actually projecting with and then clamps its
    // own cursor UV against. u runs to 1.0 by construction.
    std::uint32_t CamUvMaxV;

    // --- Field offsets inside *DisplayPtr ----------------------------------
    // The back buffer's size in pixels, which is the aspect the log line
    // reports the FOV basis against.
    std::uint32_t DisplayWidth;
    std::uint32_t DisplayHeight;
};

// Newest first. The top entry is the diagnostic primary: an unrecognised EXE is
// described relative to it.
extern const BuildProfile* const* KnownProfiles();
extern int KnownProfileCount();

// Matches the running module against the registry. Returns nullptr when no
// profile matches, leaving the mod dormant.
const BuildProfile* ResolveBuildProfile(void* moduleBase);

// One line describing why nothing matched, for the log.
const char* DescribeMismatch(void* moduleBase);

} // namespace TWHT
