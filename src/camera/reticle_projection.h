#pragma once

#include <cameraunlock/math/vec3.h>

namespace TWHT {
namespace reticle_projection {

// Where the engine would have to draw a HUD sprite for it to sit on a given
// world direction, in the engine's own screen UV space. u runs [0, 1] left to
// right and v [0, v_max] bottom to top.
struct ScreenUv {
    float u = 0.0f;
    float v = 0.0f;
    // False when the direction points behind the eye, where a UV still solves
    // but means nothing.
    bool in_front = false;
};

// What to run the solve along, for one frame.
//
// With a hit this is the vector from the eye the frame was RENDERED from to the
// point the mouse is aiming at, so it carries the live depth. That is the only
// form that holds at every range once a lean has moved the render eye off the
// aim eye: a fixed or stale depth `d0` leaves an error of lean * (1/d0 - 1/d),
// which is zero at exactly one distance and grows either side of it - the
// reticle agreeing with the shot at one range and splaying apart at others.
//
// With a definite miss there is no depth to carry, and the clean aim direction
// is exact for rotation-only tracking and under two pixels out beyond the trace
// distance.
cameraunlock::math::Vec3 AimVector(bool aimHit,
                                   const cameraunlock::math::Vec3& aimPoint,
                                   const cameraunlock::math::Vec3& aimDirection,
                                   const cameraunlock::math::Vec3& renderEye);

// Finds the UV whose ray through the rendered camera runs along `direction`.
//
// The engine unprojects a UV as
//   ray = corner + 2*scale*u*axis_u + 2*scale*v*axis_v - origin
// so solving for a world direction D is one 3x3 system:
//   2*scale*u*axis_u + 2*scale*v*axis_v - k*D = origin - corner
// with k > 0 for anything in front of the eye. Deriving it from the engine's
// own basis rather than from Euler angles is what makes it agree with whatever
// the camera actually rendered - roll, world-space yaw and lean included -
// without re-encoding the rotation composition a second time.
//
// False when the basis is degenerate or the solve does not produce finite
// numbers, in which case `out` is left alone.
bool Solve(const cameraunlock::math::Vec3& cornerFromOrigin,
           const cameraunlock::math::Vec3& axisU,
           const cameraunlock::math::Vec3& axisV,
           float scale,
           const cameraunlock::math::Vec3& direction,
           ScreenUv& out);

}  // namespace reticle_projection
}  // namespace TWHT
