#include "pch.h"
#include "game/build_profile.h"

namespace TWHT {

// Steam build, EXE dated 2019-01-23. Every address below is an RVA into
// witness64_d3d11.exe, which is linked at 0x140000000 and is not relocated in
// practice; the fingerprint is what routes, not the base.
//
// Where the field names came from: the engine registers its tuning variables by
// name at load time (All.variables), and that table is what identifies the
// camera globals - `fov_vertical`, `z_near` and `player_camera_up` all resolve
// to fields of one struct, and the camera transform sits next to the state the
// same code writes.
extern const BuildProfile kSteamProfile_20190123 = {
    .Name = "steam-win64-20190123",
    .Fingerprint = { 0x5C48CF09u, 0x04702000u, 0x00000000u },

    .SimUpdate          = 0x0006CBB0u,
    .RenderCameraSetup  = 0x002C9380u,
    .RenderCameraRefresh = 0x001D3E60u,
    .DrawSprite         = 0x00259760u,
    .DrawCircle         = 0x001CFB50u,

    .ReticleDrawReturn  = 0x001CF1FEu,
    .CursorDrawReturn   = 0x001CF0EFu,
    .FocusCircleDrawReturn = 0x001CF03Du,

    .GatherEntitiesAlongSegment = 0x001934C0u,
    .InitRayHit                 = 0x002538A0u,
    .CastRay                    = 0x00254FF0u,
    .FreeGatheredArray          = 0x002E7F90u,

    .CameraPosition         = 0x006304C0u,
    .CameraOrientation      = 0x006304D0u,
    .CameraStatePosition    = 0x0062D578u,
    .CameraStateOrientation = 0x0062D584u,
    .FovVerticalLive        = 0x006208C0u,
    // The tuning block at 0x62D0A0 plus the `fov_vertical` field's 0x150. Its
    // neighbour at +0x154 is `z_near`, and the screen basis is built from 2 *
    // that, which is what confirms the block base.
    .FovVerticalDefault     = 0x0062D1F0u,
    .FovVerticalSetting     = 0x006208BCu,
    .FovVerticalAltActive   = 0x006208CCu,
    .FovVerticalAltSetting  = 0x006208D0u,
    .RenderCameraPtr        = 0x0062D4D0u,
    // The first qword of the tuning block is the pointer the engine's own ray
    // helper passes as `globals`, so this is dereferenced rather than used as
    // an address - unlike FovVerticalDefault above, which is a field within the
    // same block.
    .GlobalsPtr             = 0x0062D0A0u,
    .DisplayPtr             = 0x0469A5B0u,

    .CamScale  = 0x94u,
    .CamOrigin = 0x9Cu,
    .CamCorner = 0xA8u,
    .CamAxisU  = 0xB4u,
    .CamAxisV  = 0xC0u,
    .CamUvMaxV = 0xD0u,

    .DisplayWidth  = 0x08u,
    .DisplayHeight = 0x0Cu,
};

namespace {
const BuildProfile* const kProfiles[] = {
    &kSteamProfile_20190123,
};
} // namespace

const BuildProfile* const* KnownProfiles() { return kProfiles; }
int KnownProfileCount() { return static_cast<int>(sizeof(kProfiles) / sizeof(kProfiles[0])); }

} // namespace TWHT
