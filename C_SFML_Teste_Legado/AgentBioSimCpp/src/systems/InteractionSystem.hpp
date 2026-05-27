#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/SpatialHash.hpp"

#include <cstddef>

namespace agentbiosim::systems
{
struct InteractionConfig
{
    bool useSpatial = true;
    bool dietFood = true;
    double foodEfficiency = 1.0;
    double energyCap = 400.0;
};

struct InteractionStats
{
    std::size_t agentsProcessed = 0;
    std::size_t foodsConsumed = 0;
    std::size_t chunkFoodsSkipped = 0;
    double foodEnergyConsumed = 0.0;
    double agentEnergyGained = 0.0;
};

class InteractionSystem
{
public:
    [[nodiscard]] static InteractionConfig fromRegistry(const config::ParameterRegistry& parameters);

    [[nodiscard]] InteractionStats apply(simulation::AgentStore& agents,
                                         simulation::FoodStore& foods,
                                         simulation::SpatialHash* spatialHash,
                                         const InteractionConfig& config) const;

private:
    [[nodiscard]] static bool touching(const simulation::AgentStore& agents,
                                       std::size_t agentIndex,
                                       const simulation::FoodStore& foods,
                                       std::size_t foodIndex);
};
} // namespace agentbiosim::systems
