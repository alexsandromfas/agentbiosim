#pragma once

#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
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
} // namespace agentbiosim::perception
