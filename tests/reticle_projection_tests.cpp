// Characterization tests for the reticle solve.
//
// The engine unprojects a screen UV as
//   normalize(corner + 2*scale*u*axis_u + 2*scale*v*axis_v - origin)
// and the mod has to run that backwards to put the reticle on the point the
// mouse is aiming at. The fixture below rebuilds the basis the engine's own
// refresh writes - from z_near, the vertical FOV and the aspect - so the
// expected UVs come out of the projection geometry rather than out of the
// solver being tested.

#include "test_harness.h"

#include "camera/pose_conversion.h"
#include "camera/reticle_projection.h"

#include <cameraunlock/math/quat4.h>

#include <cmath>
#include <iostream>

namespace {

using cameraunlock::math::Quat4;
using cameraunlock::math::Vec3;
using twht_test::Check;
using twht_test::NearEqual;

constexpr float kPi = 3.14159265358979323846f;

float Radians(float degrees) { return degrees * (kPi / 180.0f); }

// The Witness's near plane, its default vertical FOV and a 16:9 window.
constexpr float kZNear = 0.0598f;
constexpr float kFovVertical = 54.0f;
constexpr float kAspect = 16.0f / 9.0f;

// The engine builds the basis on a plane at TWICE the near distance, not at it:
// `.lab/NOTES.md` records vExtent as 2*z_near*tan(fov/2) and it is the frame's
// HALF height, so the plane carrying it sits at 2*z_near for the half-angle to
// come back out as fov/2.
//
// What the tests below can and cannot see: every expectation is a ratio of this
// distance to `scale`, and `scale` is derived from it, so the distance cancels
// and no assertion here pins the value on its own. What the earlier fixture got
// wrong was not the anchor in isolation but the pairing - it anchored at z_near
// while taking vExtent as 2*z_near*tan(fov/2), an inconsistency that does not
// cancel, and it described a 91 degree frame labelled 54. That is what the
// measured cross-check catches.
constexpr float kCornerDistance = 2.0f * kZNear;

// The basis as the engine builds it, with the camera at the origin looking
// along +x. Screen right is -y (the engine's +y is LEFT) and screen up is +z.
struct Basis {
    Vec3 cornerFromOrigin;
    Vec3 axisU;
    Vec3 axisV;
    float scale = 0.0f;
    float vMax = 0.0f;
};

Basis MakeBasis(float fovVerticalDegrees = kFovVertical) {
    const float vExtent = kCornerDistance * std::tan(Radians(fovVerticalDegrees) * 0.5f);
    Basis b;
    b.scale = vExtent * kAspect;
    b.vMax = 1.0f / kAspect;
    b.axisU = Vec3(0.0f, -1.0f, 0.0f);
    b.axisV = Vec3(0.0f, 0.0f, 1.0f);
    // The (u=0, v=0) corner sits on the basis plane, half a frame down and half
    // a frame toward screen left.
    b.cornerFromOrigin = Vec3(kCornerDistance, 0.0f, 0.0f)
                       - b.axisU * b.scale
                       - b.axisV * vExtent;
    return b;
}

// The private HUD projection basis rotates with tracking. The engine's own
// screen-ray basis stays clean for interaction.
Basis TrackedBasis(const Basis& clean, float yaw, float pitch, float roll, bool worldSpaceYaw) {
    const Quat4 cleanQuat(0.0f, 0.0f, 0.0f, 1.0f);
    const Quat4 tracked = TWHT::pose_conversion::TrackedOrientation(
        cleanQuat, yaw, pitch, roll, worldSpaceYaw);

    Basis b = clean;
    b.cornerFromOrigin = tracked.Rotate(clean.cornerFromOrigin);
    b.axisU = tracked.Rotate(clean.axisU);
    b.axisV = tracked.Rotate(clean.axisV);
    return b;
}

// Where the reticle lands for an aim point the mouse is on, given a render eye
// the lean has moved away from the shot eye.
//
// Routed through AimVector rather than subtracting here, so these run the
// decision the mod runs. Doing the subtraction in the fixture would leave the
// impact-point-versus-fixed-depth choice untested while looking like it was
// covered, which is exactly how the lean cases below were first written.
bool SolveAim(const Basis& b, const Vec3& aimPoint, const Vec3& renderEye,
              TWHT::reticle_projection::ScreenUv& uv, bool aimHit = true) {
    const Vec3 aimDirection = Vec3(1.0f, 0.0f, 0.0f);
    const Vec3 direction = TWHT::reticle_projection::AimVector(aimHit, aimPoint,
                                                              aimDirection, renderEye);
    return TWHT::reticle_projection::Solve(b.cornerFromOrigin, b.axisU, b.axisV, b.scale,
                                           direction, uv);
}

bool Solve(const Basis& b, const Vec3& direction, TWHT::reticle_projection::ScreenUv& uv) {
    return TWHT::reticle_projection::Solve(b.cornerFromOrigin, b.axisU, b.axisV, b.scale,
                                           direction, uv);
}

void RestingUv() {
    const Basis b = MakeBasis();
    TWHT::reticle_projection::ScreenUv uv;
    Check(Solve(b, Vec3(1.0f, 0.0f, 0.0f), uv), "the forward direction solves");
    Check(NearEqual(uv.u, 0.5f) && NearEqual(uv.v, 0.5f * b.vMax),
          "the camera forward lands on the reticle's resting UV");
    Check(uv.in_front, "the camera forward is in front of the eye");
}

void OffAxisDirections() {
    const Basis b = MakeBasis();
    TWHT::reticle_projection::ScreenUv uv;

    // A direction swung toward screen right by 10 degrees crosses the near
    // plane z_near*tan(10) to the right of centre, and one frame width spans
    // 2*scale there.
    const float yaw = Radians(10.0f);
    const float expectedU = 0.5f + kCornerDistance * std::tan(yaw) / (2.0f * b.scale);
    Check(Solve(b, Vec3(std::cos(yaw), -std::sin(yaw), 0.0f), uv)
              && NearEqual(uv.u, expectedU) && NearEqual(uv.v, 0.5f * b.vMax),
          "a direction to the right moves u only");

    const float pitch = Radians(8.0f);
    const float expectedV = 0.5f * b.vMax + kCornerDistance * std::tan(pitch) / (2.0f * b.scale);
    Check(Solve(b, Vec3(std::cos(pitch), 0.0f, std::sin(pitch)), uv)
              && NearEqual(uv.u, 0.5f) && NearEqual(uv.v, expectedV),
          "a direction above centre moves v only");

    // Direction, not point: scaling a direction cannot move where it lands.
    TWHT::reticle_projection::ScreenUv scaled;
    Check(Solve(b, Vec3(std::cos(yaw), -std::sin(yaw), 0.0f) * 250.0f, scaled)
              && NearEqual(scaled.u, expectedU) && NearEqual(scaled.v, 0.5f * b.vMax),
          "the solve depends on the direction's bearing, not its length");
}

void BehindTheEye() {
    const Basis b = MakeBasis();
    TWHT::reticle_projection::ScreenUv uv;
    // A UV still solves for a direction behind the eye, which is why the caller
    // needs the flag rather than a range test: the numbers look ordinary.
    Check(Solve(b, Vec3(-1.0f, 0.0f, 0.0f), uv), "a direction behind the eye still solves");
    Check(!uv.in_front, "a direction behind the eye is flagged");
}

void DegenerateBasis() {
    Basis b = MakeBasis();
    b.scale = 0.0f;

    TWHT::reticle_projection::ScreenUv uv;
    uv.u = 9.0f;
    uv.v = 9.0f;
    uv.in_front = true;
    Check(!Solve(b, Vec3(1.0f, 0.0f, 0.0f), uv), "a zero-scale basis does not solve");
    Check(uv.u == 9.0f && uv.v == 9.0f && uv.in_front,
          "a failed solve leaves the caller's UV alone");
}


// --- The six litmus tests -----------------------------------------------------
//
// Each one runs the join the mod runs: rotate the basis by the orientation
// pose_conversion composes, then solve through it. What they lock is that the
// reticle stays glued to the point the mouse is aiming at whatever the head
// does - so a projection re-derived from Euler angles, or a rotation
// composition quietly reordered, fails here rather than in someone's game.

// 1. Pure roll leaves the aim point where it is. The eye has not moved and the
//    direction has not changed; only the frame around it turned.
void RollAloneLeavesTheReticleCentred() {
    const Basis clean = MakeBasis();
    const Basis b = TrackedBasis(clean, 0.0f, 0.0f, 25.0f, false);

    TWHT::reticle_projection::ScreenUv uv;
    Check(SolveAim(b, Vec3(30.0f, 0.0f, 0.0f), Vec3(), uv), "a rolled basis solves");
    Check(NearEqual(uv.u, 0.5f) && NearEqual(uv.v, 0.5f * b.vMax),
          "pure roll leaves the reticle at centre");
}

// 2. Pure pitch moves it straight down the screen and nowhere across it.
void PitchAloneMovesTheReticleVertically() {
    const Basis clean = MakeBasis();
    const Basis b = TrackedBasis(clean, 0.0f, 15.0f, 0.0f, false);

    TWHT::reticle_projection::ScreenUv uv;
    Check(SolveAim(b, Vec3(30.0f, 0.0f, 0.0f), Vec3(), uv), "a pitched basis solves");
    Check(NearEqual(uv.u, 0.5f), "pure pitch does not move the reticle sideways");
    // Pitching the view up puts the aim point below centre.
    Check(uv.v < 0.5f * b.vMax, "pitching up drops the reticle below centre");
}

// 3. Pitch and roll together: the offset from centre rotates about centre by the
//    roll angle and keeps its length. A projection that rotates the offset the
//    wrong way, or not at all, drifts horizontally here - the failure AGENTS
//    records as the one that shipped.
void PitchWithRollRotatesTheOffsetAboutCentre() {
    const Basis clean = MakeBasis();
    const Vec3 aim(30.0f, 0.0f, 0.0f);

    TWHT::reticle_projection::ScreenUv pitchOnly;
    Check(SolveAim(TrackedBasis(clean, 0.0f, 15.0f, 0.0f, false), aim, Vec3(), pitchOnly),
          "the pitch-only basis solves");

    TWHT::reticle_projection::ScreenUv both;
    Check(SolveAim(TrackedBasis(clean, 0.0f, 15.0f, 25.0f, false), aim, Vec3(), both),
          "the pitch-and-roll basis solves");

    // u and v are already isotropic: both are a distance on the basis plane over
    // the same 2*scale, and vMax being 1/aspect is the frame being shorter than
    // it is wide rather than v carrying a different scale. So the offset is
    // compared as it stands, with no aspect correction - putting one in is how
    // this test first claimed the code was wrong.
    const float px = pitchOnly.u - 0.5f;
    const float py = pitchOnly.v - 0.5f * clean.vMax;
    const float bx = both.u - 0.5f;
    const float by = both.v - 0.5f * clean.vMax;

    Check(NearEqual(std::sqrt(px * px + py * py), std::sqrt(bx * bx + by * by)),
          "roll does not change how far the reticle sits from centre");

    // By MINUS the roll. The camera rolled the picture, so a world-fixed point
    // turns the other way within it, and that counter-rotation is precisely what
    // keeps the reticle over the spot the mouse is aiming at while the horizon
    // tips. A projection that rotated it the other way would agree at roll 0 and
    // drift to twice the error at every angle either side.
    const float roll = Radians(25.0f);
    Check(NearEqual(bx, px * std::cos(roll) + py * std::sin(roll))
              && NearEqual(by, -px * std::sin(roll) + py * std::cos(roll)),
          "roll turns the offset about centre by exactly minus the roll angle");
}

// 4. World-space yaw with the camera looking straight down is a pure spin about
//    the view axis, so the point straight below stays dead centre. A projection
//    that treats head yaw as camera-local sweeps the reticle through an arc.
void WorldYawLookingDownLeavesTheReticleCentred() {
    // Looking down: forward is -z, screen right stays -y, screen up becomes +x.
    Basis clean = MakeBasis();
    clean.axisU = Vec3(0.0f, -1.0f, 0.0f);
    clean.axisV = Vec3(1.0f, 0.0f, 0.0f);
    const float vExtent = clean.scale * clean.vMax;
    clean.cornerFromOrigin = Vec3(0.0f, 0.0f, -kCornerDistance)
                           - clean.axisU * clean.scale
                           - clean.axisV * vExtent;

    // The camera's own rotation, taking +x forward onto -z. TrackedBasis composes
    // world yaw on the outside, which is what this mode does.
    const Quat4 cleanQuat = Quat4(0.0f, std::sin(Radians(45.0f)), 0.0f,
                                  std::cos(Radians(45.0f)));
    const Quat4 tracked = TWHT::pose_conversion::TrackedOrientation(
        cleanQuat, 20.0f, 0.0f, 0.0f, true);
    const Quat4 delta = tracked * cleanQuat.Inverse();

    Basis b = clean;
    b.cornerFromOrigin = delta.Rotate(clean.cornerFromOrigin);
    b.axisU = delta.Rotate(clean.axisU);
    b.axisV = delta.Rotate(clean.axisV);

    TWHT::reticle_projection::ScreenUv uv;
    Check(SolveAim(b, Vec3(0.0f, 0.0f, -30.0f), Vec3(), uv), "the world-yawed basis solves");
    Check(NearEqual(uv.u, 0.5f) && NearEqual(uv.v, 0.5f * b.vMax),
          "yawing the head while looking straight down leaves the reticle at centre");
}

// 5 and 6. The release gate. With the head leaned and the rotation centred, the
// render eye is not the shot eye, so the reticle has to be solved from the
// IMPACT POINT rather than the aim direction. A fixed, smoothed or stale depth
// agrees at exactly one range and splays either side of it, which is why each
// lean is checked twice: within arm's reach and across the room. Both go
// through AimVector, the function the hook calls, so a depth dropped there
// fails here.
void ALeanKeepsTheReticleOnTheImpactPointAtEveryRange() {
    const Basis b = MakeBasis();  // rotation centred, so the basis is unrotated
    const Vec3 lean(0.0f, 0.30f, 0.0f);  // 0.30m along the engine's LEFT axis

    for (float range : { 0.5f, 30.0f }) {
        const Vec3 aimPoint(range, 0.0f, 0.0f);  // straight down the clean aim ray

        TWHT::reticle_projection::ScreenUv uv;
        Check(SolveAim(b, aimPoint, lean, uv), "the leaned solve succeeds");

        // Solved from the leaned eye, the impact point sits off centre by the
        // parallax the lean introduces at that range - and it is that offset,
        // not a fixed one, that keeps the reticle over the hole.
        const float expectedU = 0.5f + kCornerDistance * (lean.y / range) / (2.0f * b.scale);
        Check(NearEqual(uv.u, expectedU),
              "a lateral lean puts the reticle on the impact point, at this range");
        Check(NearEqual(uv.v, 0.5f * b.vMax), "a lateral lean does not move it vertically");
    }
}

void AVerticalLeanKeepsTheReticleOnTheImpactPointAtEveryRange() {
    const Basis b = MakeBasis();
    const Vec3 lean(0.0f, 0.0f, 0.20f);  // 0.20m up

    for (float range : { 0.5f, 30.0f }) {
        const Vec3 aimPoint(range, 0.0f, 0.0f);

        TWHT::reticle_projection::ScreenUv uv;
        Check(SolveAim(b, aimPoint, lean, uv), "the leaned solve succeeds");

        const float expectedV =
            0.5f * b.vMax - kCornerDistance * (lean.z / range) / (2.0f * b.scale);
        Check(NearEqual(uv.u, 0.5f), "a vertical lean does not move the reticle sideways");
        Check(NearEqual(uv.v, expectedV),
              "a vertical lean puts the reticle on the impact point, at this range");
    }
}

// The other half of the gate: that a MISS falls back to the aim direction, and
// that the hit path genuinely depends on where the render eye is. Without this,
// an AimVector that ignored `renderEye` entirely would still pass the two lean
// tests at the one range where the parallax happens to vanish.
void TheAimVectorCarriesDepthOnlyWhenThereIsAHit() {
    const Vec3 aimPoint(30.0f, 0.0f, 0.0f);
    const Vec3 aimDirection(1.0f, 0.0f, 0.0f);
    const Vec3 leanedEye(0.0f, 0.30f, 0.0f);

    const Vec3 miss = TWHT::reticle_projection::AimVector(false, aimPoint, aimDirection,
                                                          leanedEye);
    Check(NearEqual(miss.x, 1.0f) && NearEqual(miss.y, 0.0f) && NearEqual(miss.z, 0.0f),
          "a definite miss projects the clean aim direction, unchanged by the lean");

    const Vec3 hit = TWHT::reticle_projection::AimVector(true, aimPoint, aimDirection,
                                                         leanedEye);
    Check(NearEqual(hit.x, 30.0f) && NearEqual(hit.y, -0.30f),
          "a hit projects from the render eye to the impact point, carrying the depth");

    // Same aim point, a different eye: the vector has to move, or the depth is
    // not being carried and the reticle would agree at one range only.
    const Vec3 other = TWHT::reticle_projection::AimVector(true, aimPoint, aimDirection,
                                                           Vec3(0.0f, -0.30f, 0.0f));
    Check(!NearEqual(hit.y, other.y),
          "moving the render eye moves the vector, so the depth is live rather than fixed");
}

// The measured cross-check. `.lab/NOTES.md` records a 20 degree yaw at a 70
// degree vertical FOV landing at u 0.3538, which is ndc 0.2924. That number came
// from the game rather than from this fixture, so it is what says the fixture
// describes the frame the engine actually renders rather than a self-consistent
// one of its own - the previous fixture answers 0.1462 here.
void TheFixtureMatchesTheFrameMeasuredInGame() {
    const Basis b = MakeBasis(70.0f);
    const float yaw = Radians(20.0f);

    TWHT::reticle_projection::ScreenUv uv;
    Check(Solve(b, Vec3(std::cos(yaw), -std::sin(yaw), 0.0f), uv), "the 70 degree basis solves");
    Check(NearEqual(2.0f * (uv.u - 0.5f), 0.2924f),
          "a 20 degree yaw at a 70 degree vertical FOV lands where the game put it");
}

}  // namespace

void RunReticleProjectionTests() {
    std::cout << "reticle_projection\n";
    RestingUv();
    OffAxisDirections();
    BehindTheEye();
    DegenerateBasis();
    TheFixtureMatchesTheFrameMeasuredInGame();
    TheAimVectorCarriesDepthOnlyWhenThereIsAHit();
    RollAloneLeavesTheReticleCentred();
    PitchAloneMovesTheReticleVertically();
    PitchWithRollRotatesTheOffsetAboutCentre();
    WorldYawLookingDownLeavesTheReticleCentred();
    ALeanKeepsTheReticleOnTheImpactPointAtEveryRange();
    AVerticalLeanKeepsTheReticleOnTheImpactPointAtEveryRange();
}
