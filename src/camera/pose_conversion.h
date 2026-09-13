#pragma once

#include <cameraunlock/math/quat4.h>
#include <cameraunlock/math/vec3.h>

namespace TWHT {
namespace pose_conversion {

// Every conversion between the tracker's pose and The Witness's camera frame,
// as pure functions. Nothing here reads game memory, writes to it, or logs -
// which is what lets the signs and the composition order be tested away from a
// running game, and they are the parts that have to stay verified.
//
// The engine's camera frame is +x forward, +y left, +z up (the tuning table
// names the eye offset `player_camera_forward` / `_left` / `_up`), and the
// world is z-up right-handed.

// The camera orientation the frame is rendered with, from the game's own
// orientation and the processed pose in degrees. A non-finite angle returns
// `cleanOrientation` unchanged rather than a NaN quaternion.
//
// In world-space yaw the head turn goes about the world's vertical rather than
// the camera's own, so looking up a slope and then turning the head sweeps the
// horizon instead of tipping it. The axis is the same vector either way here
// because the engine's camera-local up IS (0, 0, 1); what differs is which side
// of the game's rotation it composes on.
cameraunlock::math::Quat4 TrackedOrientation(
    const cameraunlock::math::Quat4& cleanOrientation,
    float yaw, float pitch, float roll, bool worldSpaceYaw);

// The world-space eye offset for one processed tracker position sample, in
// metres, scaled by `scale` (the zoom factor).
//
// The basis is horizon-locked and built from the CLEAN camera forward, so the
// offset follows the body rather than the head-rotated view. False when any
// component is not finite, and when the camera looks straight up or down and
// there is no horizontal forward to build the basis from; `offset` is left
// alone in both cases.
bool LeanOffset(const cameraunlock::math::Vec3& cleanForward,
                float x, float y, float z, float scale,
                cameraunlock::math::Vec3& offset);

// The factor the pose scales by so its screen displacement is what it would
// have been at the player's own field of view. Both arguments are vertical FOVs
// in degrees, read through the rules the engine's own getters apply. Exactly
// 1.0 in ordinary play, and 1.0 for any pair that cannot produce a meaningful
// ratio - an unreadable FOV means no compensation, never a guessed one.
float ZoomFactor(float fovVerticalLiveDeg, float fovVerticalBaseDeg);

}  // namespace pose_conversion
}  // namespace TWHT
