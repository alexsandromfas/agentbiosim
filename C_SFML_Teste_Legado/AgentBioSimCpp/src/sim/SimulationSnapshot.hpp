#pragma once

#include "neural/BrainSerializer.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace agentbiosim::sim
{
// Phase 28: a plain-data snapshot of the full engine state — everything needed
// to reopen a simulation exactly as it was saved. It is the neutral boundary
// between the runner (which produces/consumes it) and the io layer (which turns
// it into a .agentbiosim file). It deliberately does NOT carry RNG generator
// state: the "simple" save reopens the world as-is and continues from there, it
// does not promise a bit-identical alternate timeline.
struct SimulationSnapshot
{
    // Engine scalars.
    std::uint64_t seed = 1337;
    std::uint64_t stepsExecuted = 0;
    double timeScale = 1.0;
    bool paused = false;
    std::size_t foodEaten = 0;
    std::size_t deaths = 0;
    std::size_t births = 0;

    simulation::WorldConfig world;

    // Agents (+ their brains, keyed by agent id; ids are preserved exactly so
    // each brain maps back to its agent and agent->genome/species refs stay valid).
    std::vector<simulation::EntityId> agentIds;
    std::vector<simulation::AgentSpawn> agents;
    std::uint64_t nextAgentId = 1;
    std::vector<std::pair<std::uint64_t, neural::BrainSnapshot>> brains;

    // Food.
    std::vector<simulation::EntityId> foodIds;
    std::vector<simulation::FoodSpawn> foods;
    std::vector<simulation::Vec2> foodVelocities;
    std::uint64_t nextFoodId = 1;
    std::uint32_t nextClusterId = 1;

    // Obstacles.
    std::vector<simulation::ObstacleId> obstacleIds;
    std::vector<simulation::ObstacleSpawn> obstacles;
    std::uint32_t nextObstacleId = 1;

    // Species / labels and genomes (records carry everything they need).
    std::vector<simulation::SpeciesRecord> species;
    std::uint32_t nextSpeciesId = 1;
    std::vector<simulation::GenomeRecord> genomes;
    std::uint64_t nextGenomeId = 1;
};

// Phase 28: a single exported organism — its genome + its brain (the "mind") +
// body look. Used by the Agente > Exportar/Importar feature to share a creature
// between simulations. Importing best preserves the mind when the target sim
// uses the same neural type + vision config (same brain architecture).
struct AgentExport
{
    simulation::GenomeRecord genome;
    neural::BrainSnapshot brain;
    double radius = 9.0;
    simulation::ColorRgb color{220, 220, 220};
};
} // namespace agentbiosim::sim
