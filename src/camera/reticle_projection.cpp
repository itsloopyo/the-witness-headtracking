#include "camera/reticle_projection.h"

#include <cmath>

namespace TWHT {
namespace reticle_projection {

using cameraunlock::math::Vec3;

namespace {

// Below this the three columns are coplanar and the ray has no unique UV.
constexpr float kSingularDeterminant = 1e-9f;

float Determinant(const Vec3& a, const Vec3& b, const Vec3& c) {
    return a.x * (b.y * c.z - b.z * c.y)
         - b.x * (a.y * c.z - a.z * c.y)
         + c.x * (a.y * b.z - a.z * b.y);
}

}  // namespace

Vec3 AimVector(bool aimHit, const Vec3& aimPoint, const Vec3& aimDirection,
               const Vec3& renderEye) {
    return aimHit ? (aimPoint - renderEye) : aimDirection;
}

bool Solve(const Vec3& cornerFromOrigin, const Vec3& axisU, const Vec3& axisV, float scale,
           const Vec3& direction, ScreenUv& out) {
    const Vec3 c1 = axisU * (2.0f * scale);
    const Vec3 c2 = axisV * (2.0f * scale);
    const Vec3 c3 = direction * -1.0f;
    const Vec3 rhs = cornerFromOrigin * -1.0f;

    const float d = Determinant(c1, c2, c3);
    if (std::fabs(d) < kSingularDeterminant) return false;

    const float inv = 1.0f / d;
    const float u = Determinant(rhs, c2, c3) * inv;
    const float v = Determinant(c1, rhs, c3) * inv;
    const float k = Determinant(c1, c2, rhs) * inv;

    if (!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(k)) return false;
    out.u = u;
    out.v = v;
    out.in_front = k > 0.0f;
    return true;
}

}  // namespace reticle_projection
}  // namespace TWHT
