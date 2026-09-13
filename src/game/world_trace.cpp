#include "pch.h"
#include "game/world_trace.h"

#include "game/engine.h"

namespace TWHT {
namespace world_trace {

using cameraunlock::camera::LeanObstruction;
using cameraunlock::math::Vec3;

namespace {

// The engine's gather/cast masks, taken from its own view-ray helper rather
// than guessed: 0x004c0080 selects the collision layers a world ray walks, and
// 0x00000b3c is the exclusion set that keeps triggers and non-blocking volumes
// out of the answer.
constexpr std::uint32_t kGatherMask   = 0x004c0080u;
constexpr std::uint32_t kIncludeMask  = 0xffffffffu;
constexpr std::uint32_t kExcludeMask  = 0x00000b3cu;

// Growable array header the gather fills in: count, capacity, heap block.
struct GatheredArray {
    std::int32_t count;
    std::int32_t capacity;
    void* data;
};

// The engine's ray hit record. Only two fields matter here; the rest carries
// the surface, material and entity the hit landed on.
constexpr std::size_t kHitRecordBytes = 0x80;
constexpr std::size_t kHitDistanceOffset = 0x00;
constexpr std::size_t kHitValidOffset = 0x24;

using GatherFn = void(*)(void* globals, const float* start, const float* end,
                         std::uint32_t mask, void* outArray,
                         std::uint32_t, std::uint32_t);
using InitHitFn = void*(*)(void* hit);
using CastFn = void(*)(const float* origin, const float* direction, float maxDistance,
                       std::uint32_t include, std::uint32_t exclude,
                       void* hit, void* gathered);
using FreeFn = void(*)(void** dataField);

} // namespace

Hit Cast(const Vec3& origin, const Vec3& direction, float maxDistance) {
    Hit result;

    const Engine& engine = TheEngine();
    if (!engine.IsBound()) return result;
    void* globals = engine.Globals();
    if (globals == nullptr) return result;
    if (!(maxDistance > 0.0f)) return result;

    const BuildProfile& p = engine.Profile();
    const auto gather = reinterpret_cast<GatherFn>(engine.Address(p.GatherEntitiesAlongSegment));
    const auto initHit = reinterpret_cast<InitHitFn>(engine.Address(p.InitRayHit));
    const auto cast = reinterpret_cast<CastFn>(engine.Address(p.CastRay));
    const auto freeArray = reinterpret_cast<FreeFn>(engine.Address(p.FreeGatheredArray));

    const float start[3] = { origin.x, origin.y, origin.z };
    const float end[3] = {
        origin.x + direction.x * maxDistance,
        origin.y + direction.y * maxDistance,
        origin.z + direction.z * maxDistance,
    };
    const float dir[3] = { direction.x, direction.y, direction.z };

    GatheredArray gathered = { 0, 0, nullptr };
    alignas(16) unsigned char hit[kHitRecordBytes] = {};

    gather(globals, start, end, kGatherMask, &gathered, 0, 0);
    initHit(hit);
    cast(start, dir, maxDistance, kIncludeMask, kExcludeMask, hit, &gathered);
    freeArray(&gathered.data);

    result.queried = true;
    if (hit[kHitValidOffset] != 0) {
        float distance = 0.0f;
        std::memcpy(&distance, hit + kHitDistanceOffset, sizeof(distance));
        if (distance >= 0.0f && distance <= maxDistance) {
            result.hit = true;
            result.distance = distance;
        }
    }
    return result;
}

LeanObstruction LeanQuery(void* /*context*/, const Vec3& start, const Vec3& direction,
                          float maxDistance) {
    LeanObstruction obstruction;

    // Overreach the lean so a surface the eye is about to come to rest against
    // is seen this frame rather than one lean later. Over-tracing costs
    // nothing: LeanClamp discards any hit beyond the lean it asked for.
    const Hit hit = Cast(start, direction, maxDistance * 2.0f);
    obstruction.queried = hit.queried;
    obstruction.blocked = hit.hit;
    obstruction.distance = hit.distance;
    return obstruction;
}

} // namespace world_trace
} // namespace TWHT
