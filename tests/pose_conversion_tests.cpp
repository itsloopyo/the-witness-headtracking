// Characterization tests for the tracker-to-engine boundary.
//
// Every number these lock was established against the drawn reticle on the
// 2019-01-23 Steam build, and none of it is derivable from the engine alone:
// the protocol says nothing about which way it calls positive, so the signs and
// the composition order are measurements, not deductions. They read as
// arbitrary, which is exactly why they get "tidied" - and a flipped sign is a
// coin flip in front of the player, not a build failure.
//
// The engine's camera frame is +x forward, +y left, +z up, world z-up
// right-handed.

#include "test_harness.h"

#include "camera/pose_conversion.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using cameraunlock::math::Quat4;
using cameraunlock::math::Vec3;
using twht_test::Check;
using twht_test::NearEqual;

constexpr float kPi = 3.14159265358979323846f;

float Radians(float degrees) { return degrees * (kPi / 180.0f); }

// Built here rather than reused from pose_conversion so the tests check the
// composition against an independent construction.
Quat4 Rotation(float degrees, const Vec3& axis) {
    const float half = Radians(degrees) * 0.5f;
    const float s = std::sin(half);
    return Quat4(axis.x * s, axis.y * s, axis.z * s, std::cos(half));
}

const Vec3 kForward(1.0f, 0.0f, 0.0f);
const Vec3 kLeft(0.0f, 1.0f, 0.0f);
const Vec3 kUp(0.0f, 0.0f, 1.0f);

bool NearVec(const Vec3& a, const Vec3& b, float eps = 1e-4f) {
    return NearEqual(a.x, b.x, eps) && NearEqual(a.y, b.y, eps) && NearEqual(a.z, b.z, eps);
}

void RotationSigns() {
    const Quat4 identity = Quat4::Identity();

    // Positive yaw turns the view right. +y is LEFT, so the forward vector's y
    // going negative is the view swinging right.
    const Vec3 yawed =
        TWHT::pose_conversion::TrackedOrientation(identity, 20.0f, 0.0f, 0.0f, false)
            .Rotate(kForward);
    Check(NearVec(yawed, Vec3(std::cos(Radians(20.0f)), -std::sin(Radians(20.0f)), 0.0f)),
          "positive yaw swings forward to the right");

    // Positive pitch looks up: +z is up.
    const Vec3 pitched =
        TWHT::pose_conversion::TrackedOrientation(identity, 0.0f, 15.0f, 0.0f, false)
            .Rotate(kForward);
    Check(NearVec(pitched, Vec3(std::cos(Radians(15.0f)), 0.0f, std::sin(Radians(15.0f)))),
          "positive pitch raises forward");

    // Positive roll turns about the camera's own forward axis, leaving forward
    // alone and tipping up toward the left.
    const Quat4 rolled =
        TWHT::pose_conversion::TrackedOrientation(identity, 0.0f, 0.0f, 25.0f, false);
    Check(NearVec(rolled.Rotate(kForward), kForward), "roll leaves the forward axis alone");
    Check(NearVec(rolled.Rotate(kUp),
                  Vec3(0.0f, std::sin(Radians(25.0f)), std::cos(Radians(25.0f)))),
          "positive roll tips up toward the left");
}

void RotationComposition() {
    // Yaw, then pitch, then roll - so a combined yaw and pitch puts forward at
    // the yaw of the pitched vector rather than orbiting.
    const Vec3 combined = TWHT::pose_conversion::TrackedOrientation(
                              Quat4::Identity(), 20.0f, 15.0f, 0.0f, false)
                              .Rotate(kForward);
    const float cp = std::cos(Radians(15.0f));
    Check(NearVec(combined, Vec3(cp * std::cos(Radians(20.0f)),
                                 -cp * std::sin(Radians(20.0f)),
                                 std::sin(Radians(15.0f)))),
          "yaw composes outside pitch");

    // With the game's camera at identity the two yaw modes cannot differ, which
    // is what makes the straight-down case below the one that tells them apart.
    const Quat4 world = TWHT::pose_conversion::TrackedOrientation(
        Quat4::Identity(), 20.0f, 15.0f, 25.0f, true);
    const Quat4 local = TWHT::pose_conversion::TrackedOrientation(
        Quat4::Identity(), 20.0f, 15.0f, 25.0f, false);
    Check(NearVec(world.Rotate(kForward), local.Rotate(kForward)),
          "yaw modes agree when the game camera is level and facing forward");
}

void WorldSpaceYawAtSteepPitch() {
    // The game camera looking straight down: forward is -z, so head yaw about
    // the world's vertical is a spin about the view axis and must not move the
    // aim direction. A camera-local yaw does move it, and that difference is
    // the whole point of the mode.
    const Quat4 lookingDown = Rotation(90.0f, kLeft);
    Check(NearVec(lookingDown.Rotate(kForward), Vec3(0.0f, 0.0f, -1.0f)),
          "test fixture: the game camera looks straight down");

    const Vec3 worldYawed = TWHT::pose_conversion::TrackedOrientation(
                                lookingDown, 20.0f, 0.0f, 0.0f, true)
                                .Rotate(kForward);
    Check(NearVec(worldYawed, Vec3(0.0f, 0.0f, -1.0f)),
          "world-space yaw spins the view about the axis it is already looking down");

    const Vec3 localYawed = TWHT::pose_conversion::TrackedOrientation(
                                lookingDown, 20.0f, 0.0f, 0.0f, false)
                                .Rotate(kForward);
    Check(NearVec(localYawed, Vec3(0.0f, -std::sin(Radians(20.0f)),
                                   -std::cos(Radians(20.0f)))),
          "camera-local yaw swings the view when looking straight down");
}

void LeanSigns() {
    Vec3 offset;

    Check(TWHT::pose_conversion::LeanOffset(kForward, 0.30f, 0.0f, 0.0f, 1.0f, offset)
              && NearVec(offset, kLeft * 0.30f),
          "positive tracker x leans the eye to the engine's left");

    // The processor's forward lean is its NEGATIVE z, so that is the one that
    // has to come out along the camera's forward axis.
    Check(TWHT::pose_conversion::LeanOffset(kForward, 0.0f, 0.0f, -0.40f, 1.0f, offset)
              && NearVec(offset, kForward * 0.40f),
          "negative tracker z leans the eye forward");
    Check(TWHT::pose_conversion::LeanOffset(kForward, 0.0f, 0.0f, 0.10f, 1.0f, offset)
              && NearVec(offset, kForward * -0.10f),
          "positive tracker z leans the eye back");

    Check(TWHT::pose_conversion::LeanOffset(kForward, 0.0f, 0.20f, 0.0f, 1.0f, offset)
              && NearVec(offset, kUp * 0.20f),
          "positive tracker y raises the eye");
}

void LeanBasisIsHorizonLocked() {
    Vec3 offset;

    // A camera pitched up 45 degrees: a forward lean still has to travel along
    // the ground, not up the view axis, or looking up would lift the eye.
    const Vec3 pitchedForward(std::cos(Radians(45.0f)), 0.0f, std::sin(Radians(45.0f)));
    Check(TWHT::pose_conversion::LeanOffset(pitchedForward, 0.0f, 0.0f, -0.40f, 1.0f, offset)
              && NearVec(offset, Vec3(0.40f, 0.0f, 0.0f)),
          "a forward lean stays horizontal under a pitched camera");

    // Facing along the engine's left axis: the lean basis turns with the body.
    Check(TWHT::pose_conversion::LeanOffset(kLeft, 0.30f, 0.0f, 0.0f, 1.0f, offset)
              && NearVec(offset, Vec3(-0.30f, 0.0f, 0.0f)),
          "the lean basis follows the body's heading");

    // Straight down: there is no horizontal forward to build the basis from,
    // and the out-param is left alone rather than filled with a guess.
    const Vec3 sentinel(9.0f, 9.0f, 9.0f);
    offset = sentinel;
    Check(!TWHT::pose_conversion::LeanOffset(Vec3(0.0f, 0.0f, -1.0f), 0.30f, 0.20f, -0.40f,
                                             1.0f, offset)
              && NearVec(offset, sentinel),
          "a camera looking straight down has no lean basis and writes nothing");
}

void LeanScalesWithZoom() {
    Vec3 offset;
    Check(TWHT::pose_conversion::LeanOffset(kForward, 0.30f, 0.20f, -0.40f, 0.5f, offset)
              && NearVec(offset, Vec3(0.20f, 0.15f, 0.10f)),
          "the zoom factor scales every lean axis linearly");
}

void ZoomFactors() {
    // The gate the doctrine states: exactly 1.0 whenever nothing is zoomed.
    Check(TWHT::pose_conversion::ZoomFactor(54.0f, 54.0f) == 1.0f,
          "an un-zoomed frame scales by exactly 1.0");

    // Both measured in game by holding the FOV override slot.
    Check(NearEqual(TWHT::pose_conversion::ZoomFactor(27.0f, 54.0f), 0.47118f, 1e-4f),
          "54 to 27 degrees vertical scales by 0.47118");
    Check(NearEqual(TWHT::pose_conversion::ZoomFactor(35.0f, 70.0f), 0.45029f, 1e-4f),
          "70 to 35 degrees vertical scales by 0.45029");

    // An unreadable FOV applies no compensation rather than a guessed one.
    const float notANumber = std::nanf("");
    Check(TWHT::pose_conversion::ZoomFactor(0.0f, 54.0f) == 1.0f, "a zero live FOV scales by 1.0");
    Check(TWHT::pose_conversion::ZoomFactor(54.0f, 0.0f) == 1.0f, "a zero base FOV scales by 1.0");
    Check(TWHT::pose_conversion::ZoomFactor(-1.0f, 54.0f) == 1.0f,
          "a negative live FOV scales by 1.0");
    Check(TWHT::pose_conversion::ZoomFactor(notANumber, 54.0f) == 1.0f, "a NaN live FOV scales by 1.0");
    Check(TWHT::pose_conversion::ZoomFactor(54.0f, notANumber) == 1.0f, "a NaN base FOV scales by 1.0");
}

// The pipeline validates what arrives on the wire, so a non-finite pose here
// means a number the user put in the ini reached the multipliers. Either way it
// must not become a NaN quaternion: that is written into the engine's camera
// every frame afterwards, and there is nothing in the log to explain the dead
// view it produces.
void NonFinitePoseIsRefused() {
    const float notANumber = std::nanf("");
    const float infinity = std::numeric_limits<float>::infinity();
    const Quat4 clean = Rotation(30.0f, kUp);

    for (bool worldYaw : {false, true}) {
        const Quat4 fromNan =
            TWHT::pose_conversion::TrackedOrientation(clean, notANumber, 15.0f, 0.0f, worldYaw);
        Check(NearVec(fromNan.Rotate(kForward), clean.Rotate(kForward)),
              "a NaN yaw leaves the game's own orientation standing");

        const Quat4 fromInf =
            TWHT::pose_conversion::TrackedOrientation(clean, 20.0f, infinity, 0.0f, worldYaw);
        Check(NearVec(fromInf.Rotate(kForward), clean.Rotate(kForward)),
              "an infinite pitch leaves the game's own orientation standing");
    }

    const Vec3 sentinel(9.0f, 9.0f, 9.0f);
    Vec3 offset = sentinel;
    Check(!TWHT::pose_conversion::LeanOffset(kForward, notANumber, 0.0f, 0.0f, 1.0f, offset)
              && NearVec(offset, sentinel),
          "a NaN lean component writes nothing");
    offset = sentinel;
    Check(!TWHT::pose_conversion::LeanOffset(kForward, 0.30f, 0.0f, 0.0f, notANumber, offset)
              && NearVec(offset, sentinel),
          "a NaN zoom factor writes nothing");
    offset = sentinel;
    Check(!TWHT::pose_conversion::LeanOffset(kForward, 0.0f, infinity, 0.0f, 1.0f, offset)
              && NearVec(offset, sentinel),
          "an infinite lean component writes nothing");
}

}  // namespace

void RunPoseConversionTests() {
    std::cout << "pose_conversion\n";
    RotationSigns();
    RotationComposition();
    WorldSpaceYawAtSteepPitch();
    LeanSigns();
    LeanBasisIsHorizonLocked();
    LeanScalesWithZoom();
    ZoomFactors();
    NonFinitePoseIsRefused();
}
