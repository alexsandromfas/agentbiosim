#pragma once

#include "simulation/EntityTypes.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace agentbiosim::simulation
{
// Phase 20: ObstacleStore. Headless SoA store of disc obstacles. Disc is the
// minimum viable geometry that covers (1) movement blocking, (2) spawn
// blocking and (3) line-of-sight occlusion. More complex shapes (rect, polyline)
// would be future Phase 24 work.
//
// Each obstacle has an explicit `brushRadius` separate from the geometry radius
// to carry editor metadata for the future Phase 22/24 painter UI. For Phase 20
// the brush radius defaults to the geometry radius.
using ObstacleId = std::uint32_t;
inline constexpr ObstacleId kInvalidObstacleId = 0;

struct ObstacleSpawn
{
    Vec2 position{};
    double radius = 20.0;
    double brushRadius = 20.0;
    ColorRgb color{60, 60, 70};
};

class ObstacleStore
{
public:
    [[nodiscard]] ObstacleId add(const ObstacleSpawn& spawn);
    bool remove(ObstacleId id);
    void clear();

    // Phase 20: paint and erase helpers. Paint adds an obstacle at the brush
    // position with `brushRadius` as both the geometry and the brush metadata.
    // Erase removes every obstacle whose center is within `eraseRadius` of `pos`.
    // Returns the number of obstacles removed.
    [[nodiscard]] ObstacleId paint(Vec2 pos, double brushRadius, ColorRgb color = {60, 60, 70});
    [[nodiscard]] std::size_t eraseAt(Vec2 pos, double eraseRadius);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool contains(ObstacleId id) const;

    [[nodiscard]] Vec2 positionAt(std::size_t index) const;
    [[nodiscard]] double radiusAt(std::size_t index) const;
    [[nodiscard]] double brushRadiusAt(std::size_t index) const;
    [[nodiscard]] ColorRgb colorAt(std::size_t index) const;
    [[nodiscard]] ObstacleId idAt(std::size_t index) const;

    // Queries. Linear scan with cheap AABB pre-filter; fine up to ~1000
    // obstacles (measured in Phase 20 benchmark).
    [[nodiscard]] bool containsPoint(Vec2 pos) const noexcept;
    [[nodiscard]] bool overlapsCircle(Vec2 center, double radius) const noexcept;
    // Segment-disc intersection test. Returns true if the segment [a,b] hits
    // any obstacle disc. Used by perception for occlusion.
    [[nodiscard]] bool segmentBlocked(Vec2 a, Vec2 b) const noexcept;
    // Same as above but also writes the closest hit point into `outHit`.
    [[nodiscard]] bool segmentBlocked(Vec2 a, Vec2 b, Vec2& outHit) const noexcept;

    void queryRadius(Vec2 center, double radius, std::vector<std::size_t>& outIndices) const;

private:
    std::vector<ObstacleId> ids_;
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> radius_;
    std::vector<double> brushRadius_;
    std::vector<ColorRgb> color_;
    std::vector<std::uint8_t> alive_;
    ObstacleId nextId_ = 1;
};

// Convenience: clamp a position into the world AND push it out of obstacles.
// If the position cannot be moved to a free spot within `maxAttempts`, returns
// the original position (caller is expected to retry from a fresh sample).
[[nodiscard]] bool isPositionFree(const World& world, const ObstacleStore* obstacles,
                                   Vec2 pos, double radius) noexcept;
} // namespace agentbiosim::simulation
