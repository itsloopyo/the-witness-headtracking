#include "camera/pose_conversion.h"

#include <cameraunlock/camera/zoom_compensation.h>

#include <cmath>

namespace TWHT {
namespace pose_conversion {

using cameraunlock::math::Quat4;
using cameraunlock::math::Vec3;

namespace {

// --- Tracker to engine conversions, all of them, in one place ---------------
//
// The protocol carries no statement of what its positive directions mean, so
// none of these signs is derivable from the engine alone. They are the ones
// kingdom-come-deliverance-headtracking verified in game, carried across
// because CryEngine's camera is the same shape as this one: right-handed,
// z-up, orientation as a quaternion, with the camera's own forward, lateral and
// up axes as its columns. The two frames differ only in which local axis is
// forward, and a rotation about "up", "the lateral axis" or "forward" composes
// identically in both, so the same signs mean the same thing to a player.
//
// Yaw and roll arrive mirrored; pitch does not. Positive yaw turns the view
// right, positive pitch looks up, positive roll tips the horizon the way the
// head does.
constexpr float kYawAxisSign = -1.0f;
constexpr float kPitchAxisSign = -1.0f;
constexpr float kRollAxisSign = -1.0f;

// Sideways lean along the engine's LEFT axis. Positive tracker x moves the eye
// left, matching the ancestor's `-pose.x` along its right axis.
constexpr float kLeanLeftSign = 1.0f;
// The processor's forward lean is its negative z.
constexpr float kLeanForwardSign = -1.0f;
constexpr float kLeanUpSign = 1.0f;

// Below this the camera is looking straight up or down and its flattened
// forward vector has no direction left to build a horizon basis from.
constexpr float kMinimumFlatForward = 1e-4f;

constexpr float kPi = 3.14159265358979323846f;

Quat4 AngleAxis(float degrees, const Vec3& axis) {
    const float half = degrees * (kPi / 180.0f) * 0.5f;
    const float s = std::sin(half);
    return Quat4(axis.x * s, axis.y * s, axis.z * s, std::cos(half));
}

}  // namespace

Quat4 TrackedOrientation(const Quat4& cleanOrientation, float yaw, float pitch, float roll,
                         bool worldSpaceYaw) {
    // A non-finite angle makes sin and cos NaN, and the NaN then rides the whole
    // quaternion into the engine's camera state every frame, where it renders as
    // a dead view with nothing in the log to explain it. The pose is dropped for
    // this frame instead and the game's own orientation stands.
    if (!std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(roll)) {
        return cleanOrientation;
    }

    const Quat4 yawQ = AngleAxis(kYawAxisSign * yaw, Vec3(0.0f, 0.0f, 1.0f));
    const Quat4 pitchQ = AngleAxis(kPitchAxisSign * pitch, Vec3(0.0f, 1.0f, 0.0f));
    const Quat4 rollQ = AngleAxis(kRollAxisSign * roll, Vec3(1.0f, 0.0f, 0.0f));

    return worldSpaceYaw
        ? (yawQ * cleanOrientation * (pitchQ * rollQ)).Normalized()
        : (cleanOrientation * (yawQ * pitchQ * rollQ)).Normalized();
}

bool LeanOffset(const Vec3& cleanForward, float x, float y, float z, float scale,
                Vec3& offset) {
    // Same reason as the rotation above, and the same answer: a non-finite
    // component would move the eye to nowhere for the rest of the session.
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(scale)) {
        return false;
    }

    Vec3 flat(cleanForward.x, cleanForward.y, 0.0f);
    const float flatLen = flat.Magnitude();
    if (flatLen <= kMinimumFlatForward) return false;

    flat = flat * (1.0f / flatLen);
    const Vec3 left(-flat.y, flat.x, 0.0f);
    const Vec3 up(0.0f, 0.0f, 1.0f);

    offset = flat * (kLeanForwardSign * z * scale)
           + left * (kLeanLeftSign * x * scale)
           + up * (kLeanUpSign * y * scale);
    return true;
}

float ZoomFactor(float fovVerticalLiveDeg, float fovVerticalBaseDeg) {
    if (!(fovVerticalLiveDeg > 0.0f) || !(fovVerticalBaseDeg > 0.0f)) return 1.0f;
    if (!std::isfinite(fovVerticalLiveDeg) || !std::isfinite(fovVerticalBaseDeg)) return 1.0f;

    const float tanLive = std::tan(fovVerticalLiveDeg * 0.5f * (kPi / 180.0f));
    const float tanBase = std::tan(fovVerticalBaseDeg * 0.5f * (kPi / 180.0f));
    if (!(tanLive > 0.0f) || !(tanBase > 0.0f)) return 1.0f;

    return cameraunlock::camera::FovZoomFactor(tanLive, tanBase);
}

}  // namespace pose_conversion
}  // namespace TWHT
