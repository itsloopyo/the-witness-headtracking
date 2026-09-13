#pragma once

#include <cameraunlock/camera/lean_clamp.h>
#include <cameraunlock/math/vec3.h>

namespace TWHT {
namespace world_trace {

// The engine's own ray cast against level collision, reached through the four
// calls its `what does this ray hit` helper makes: gather the entities whose
// bounds meet the segment, reset a hit record, run the nearest-hit test over
// the gathered array, free the array. Using the engine's query rather than a
// second one of our own is what stops the clamp and the reticle disagreeing
// with what the player can walk into.
//
// Pure: the gather and the cast only read collision meshes. The interaction
// pick that sits above them in the engine is the part with side effects, and
// it is not on this path.

struct Hit {
    // False when the cast could not be performed at all - no build profile
    // bound, or no world loaded. Distinct from a definite no-hit, and kept
    // apart for the same reason LeanObstruction keeps them apart: a projection
    // that quietly fell back because the query was not running looks exactly
    // like one that ran and found open air.
    bool queried = false;
    bool hit = false;
    float distance = 0.0f;
};

// Casts from `origin` along the unit vector `direction`. Returns a miss when
// the mod is not bound to a build profile or the world is not loaded.
Hit Cast(const cameraunlock::math::Vec3& origin,
         const cameraunlock::math::Vec3& direction,
         float maxDistance);

// LeanQueryFn shape for cameraunlock::camera::LeanClamp. `context` is unused;
// the engine reaches its world through a global.
cameraunlock::camera::LeanObstruction LeanQuery(void* context,
                                                const cameraunlock::math::Vec3& start,
                                                const cameraunlock::math::Vec3& direction,
                                                float maxDistance);

} // namespace world_trace
} // namespace TWHT
