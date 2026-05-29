#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
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

// Phase 18: diet-aware interaction. The engine inspects each agent's genome to
// pick between food consumption and predation. A single struct carries all
// shared knobs; predation can be globally disabled via `predationEnabled`.
struct DietInteractionConfig
{
    bool useSpatial = true;
    bool predationEnabled = true;
    // Global cap respected when no genome is found for the predator (defensive).
    double defaultEnergyCap = 400.0;
};

struct DietInteractionStats
{
    std::size_t agentsProcessed = 0;
    std::size_t foodsConsumed = 0;
    std::size_t chunkFoodsSkipped = 0;
    std::size_t predationEvents = 0;
    std::size_t corpsesToFoodSpawned = 0;
    std::size_t blockedSameSpecies = 0;
    std::size_t blockedDietDisabled = 0;
    double foodEnergyConsumed = 0.0;
    double agentEnergyGainedByFood = 0.0;
    double agentEnergyGainedByPredation = 0.0;
};

class InteractionSystem
{
public:
    [[nodiscard]] static InteractionConfig fromRegistry(const config::ParameterRegistry& parameters);
    [[nodiscard]] static DietInteractionConfig dietConfigFromRegistry(const config::ParameterRegistry& parameters);

    // Phase 7 food-only path. Preserved unchanged for backward compatibility.
    [[nodiscard]] InteractionStats apply(simulation::AgentStore& agents,
                                         simulation::FoodStore& foods,
                                         simulation::SpatialHash* spatialHash,
                                         const InteractionConfig& config) const;

    // Phase 18 diet-aware path: each agent dispatches by its genome's DietConfig
    // between food consumption (if `eatFood`) and predation (if `eatAgents`).
    // Same-species kills are blocked unless `eatSameSpecies` is true. Predation
    // marks prey for removal; this method physically removes prey at the end of
    // the call via AgentStore::removeAgent (preserves swap-remove + spatial hash).
    // Corpse-to-food spawns instant food at the prey's last position.
    [[nodiscard]] DietInteractionStats applyWithDiet(simulation::AgentStore& agents,
                                                     simulation::FoodStore& foods,
                                                     const simulation::GenomeStore& genomes,
                                                     simulation::SpatialHash* spatialHash,
                                                     const DietInteractionConfig& config) const;

private:
    [[nodiscard]] static bool touching(const simulation::AgentStore& agents,
                                       std::size_t agentIndex,
                                       const simulation::FoodStore& foods,
                                       std::size_t foodIndex);
};
} // namespace agentbiosim::systems
