#pragma once

#include "config/ParameterRegistry.hpp"
#include "neural/BrainConfig.hpp"
#include "perception/PerceptionSystem.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"
#include "systems/CollisionSystem.hpp"
#include "systems/DeathSystem.hpp"
#include "systems/EnergySystem.hpp"
#include "systems/FoodSystem.hpp"
#include "systems/InteractionSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/NeuralSystem.hpp"
#include "systems/ReproductionSystem.hpp"
#include "ui/Command.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace agentbiosim::sim
{
// Phase 22: SimulationRunner is the engine-only orchestrator. It owns every
// store and system needed to step the simulation. It does NOT depend on SFML,
// Dear ImGui or any window/UI type — it is fully headless. App/AppController
// drives it by calling step(dt) per fixed timestep and applyCommand(cmd) for
// every user command produced by InputRouter / UiPanel. This resolves Debt 4
// by lifting the simulation orchestration out of App.cpp.
struct SimulationRunnerStats
{
    std::size_t foodEaten = 0;
    std::size_t deaths = 0;
    std::size_t births = 0;
    std::uint64_t stepsExecuted = 0;
    bool paused = false;
};

class SimulationRunner
{
public:
    // Phase 22: construct with an existing parameter registry. The runner
    // configures world, spatial hash and bootstraps species.
    explicit SimulationRunner(const config::ParameterRegistry& parameters);

    void initialize();

    // Step the simulation by `dt` if not paused.
    void step(double dt);

    void setPaused(bool paused) noexcept { paused_ = paused; }
    void togglePaused() noexcept { paused_ = !paused_; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    void requestStepOnce() noexcept { stepOnce_ = true; }

    // Reset: rebuilds the initial spawn (same seed) and clears the obstacle
    // store. Does not change registry/world configuration.
    void reset();

    // Apply a UI command. Returns true if recognised and applied.
    bool applyCommand(const ui::Command& cmd);

    // Read-only access for Renderer / PerceptionSystem / UI selection / etc.
    [[nodiscard]] const simulation::AgentStore& agents() const noexcept { return agents_; }
    [[nodiscard]] const simulation::FoodStore& foods() const noexcept { return foods_; }
    [[nodiscard]] const simulation::ObstacleStore& obstacles() const noexcept { return obstacles_; }
    [[nodiscard]] const simulation::SpatialHash& spatialHash() const noexcept { return spatialHash_; }
    [[nodiscard]] const simulation::World& world() const noexcept { return world_; }
    [[nodiscard]] const simulation::SpeciesStore& species() const noexcept { return species_; }
    [[nodiscard]] const simulation::GenomeStore& genomes() const noexcept { return genomes_; }
    [[nodiscard]] const SimulationRunnerStats& stats() const noexcept { return stats_; }

    // Read/write access for InputRouter command path (Renderer should not write).
    [[nodiscard]] simulation::AgentStore& agentsMutable() noexcept { return agents_; }
    [[nodiscard]] simulation::FoodStore& foodsMutable() noexcept { return foods_; }
    [[nodiscard]] simulation::ObstacleStore& obstaclesMutable() noexcept { return obstacles_; }

    // Find nearest alive agent under a world point within `pickRadius`.
    [[nodiscard]] simulation::EntityId pickAgentAt(simulation::Vec2 worldPoint,
                                                    double pickRadius) const;

    // Apply selection rect/lasso. Returns count of selected ids.
    std::size_t agentsInRect(simulation::Vec2 a, simulation::Vec2 b,
                              std::vector<simulation::EntityId>& out) const;
    std::size_t agentsInLasso(const std::vector<simulation::Vec2>& polygon,
                                std::vector<simulation::EntityId>& out) const;

    // Spawn helpers used by Add commands. Return invalid id when rejected.
    [[nodiscard]] simulation::EntityId spawnAgentDefaultAt(simulation::Vec2 worldPos);
    [[nodiscard]] simulation::EntityId spawnFoodAt(simulation::Vec2 worldPos,
                                                    double radius, double energy);
    void deleteAgents(const std::vector<simulation::EntityId>& ids);

    // Phase 22: small overlay knobs. Render flags live in UiState; the runner
    // does not own them. These two are runtime engine knobs the user can edit
    // through commands but the runner persists them itself.
    [[nodiscard]] bool spatialHashOverlay() const noexcept { return showSpatialHash_; }
    [[nodiscard]] bool simpleRender() const noexcept { return simpleRender_; }

private:
    void spawnInitial();
    void rebuildSpatial();
    void runOneStep(double dt);

    const config::ParameterRegistry& parameters_;

    simulation::World world_;
    simulation::AgentStore agents_;
    simulation::FoodStore foods_;
    simulation::GenomeStore genomes_;
    simulation::SpeciesStore species_;
    simulation::ObstacleStore obstacles_;
    simulation::SpatialHash spatialHash_;

    perception::PerceptionSystem perceptionSystem_;
    systems::MovementSystem movementSystem_;
    systems::NeuralSystem neuralSystem_;
    systems::EnergySystem energySystem_;
    systems::InteractionSystem interactionSystem_;
    systems::ReproductionSystem reproductionSystem_;
    systems::DeathSystem deathSystem_;
    systems::FoodSystem foodSystem_;
    systems::CollisionSystem collisionSystem_;

    bool paused_ = false;
    bool stepOnce_ = false;
    bool showSpatialHash_ = false;
    bool simpleRender_ = false;
    std::uint64_t seed_ = 1337U;
    double timeScale_ = 1.0;

    SimulationRunnerStats stats_{};
};
} // namespace agentbiosim::sim
