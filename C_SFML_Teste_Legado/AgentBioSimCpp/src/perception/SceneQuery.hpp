#pragma once

#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/SpatialHash.hpp"

#include <cstdint>
#include <vector>

namespace agentbiosim::perception
{
struct VisibleCandidate
{
    double x = 0.0;
    double y = 0.0;
    double radius = 0.0;
    double colorR = 0.0;
    double colorG = 0.0;
    double colorB = 0.0;
    simulation::SpatialEntityType entityType = simulation::SpatialEntityType::Food;
    int typeCode = 0;
    std::uint64_t entityId = 0;
};

void queryVisibleCandidates(double searchX, double searchY, double searchRadius,
                            bool seeFood, bool seeAgents, bool seePredators, bool seeAll,
                            std::uint64_t ignoreAgentId,
                            simulation::SpatialHash* spatial,
                            const simulation::AgentStore& agents,
                            const simulation::FoodStore& foods,
                            std::vector<VisibleCandidate>& out);

// Phase 20: append obstacles within `searchRadius` of (searchX, searchY) as
// VisibleCandidates with entityType=Obstacle. The caller controls whether the
// retina actually sees them via `seeObstacles`. Obstacles default to a neutral
// gray sensorial color taken from their stored color.
void appendObstacleCandidates(double searchX, double searchY, double searchRadius,
                               const simulation::ObstacleStore& obstacles,
                               std::vector<VisibleCandidate>& out);

// Phase 20: returns true if the line segment from `(ax,ay)` to `(bx,by)` is
// blocked by any obstacle disc in `obstacles`. Used by the vision strategies
// when `seeThroughWalls=false`.
[[nodiscard]] bool isOccludedByObstacles(double ax, double ay, double bx, double by,
                                          const simulation::ObstacleStore& obstacles) noexcept;
} // namespace agentbiosim::perception
