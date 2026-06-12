#pragma once

#include "config/ParameterRegistry.hpp"
#include "core/Profiler.hpp"
#include "neural/ActivationTrace.hpp"
#include "neural/BrainConfig.hpp"
#include "neural/NeuralView.hpp"
#include "sim/SimulationSnapshot.hpp"
#include "perception/PerceptionSystem.hpp"
#include "perception/VisionDebug.hpp"
#include "systems/MetricsSystem.hpp"
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
#include "core/Command.hpp"

#include <array>
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

    // Microfase 31.1: apply the world geometry from the registry LIVE — no
    // reset. When the substrate shrinks, agents and food are pushed back inside
    // the new bounds (clamped); the spatial hash is rebuilt for the new grid.
    void applyWorldConfigLive();

    // Phase 28: persistence. `snapshot()` captures the full engine state (stores
    // + brains + world + counters); `restore()` replaces it. Parameters and
    // camera are persisted by the App layer alongside this snapshot.
    [[nodiscard]] SimulationSnapshot snapshot() const;
    void restore(const SimulationSnapshot& snapshot);

    // Phase 28: single-organism export/import (share a creature between sims).
    // `exportAgent` gathers an agent's genome + brain + body; `importAgent`
    // spawns a fresh organism from that data near `worldPos` and returns its id.
    [[nodiscard]] bool exportAgent(simulation::EntityId id, AgentExport& out) const;
    simulation::EntityId importAgent(const AgentExport& data, simulation::Vec2 worldPos);

    // Apply a high-level command. Returns true if recognised and applied.
    // Phase 25 (Divida 8): commands live in agentbiosim::core, not ui, so the
    // engine no longer depends on the UI layer.
    bool applyCommand(const core::Command& cmd);

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

    // Phase 24.2: species/label operations for the Labels tab. These are the
    // C++ analogue of the Python engine.assign_label_to_agents / create_label /
    // delete_label. SpeciesStore is the source of truth; agents carry a
    // speciesId + color that we keep in sync here.
    void assignSelectedToSpecies(const std::vector<simulation::EntityId>& ids,
                                  simulation::SpeciesId speciesId);
    void removeSelectedFromSpecies(const std::vector<simulation::EntityId>& ids);
    [[nodiscard]] simulation::SpeciesId createSpeciesFromSelected(
        const std::string& label, const std::vector<simulation::EntityId>& ids);
    bool setSpeciesColorAndRecolor(simulation::SpeciesId speciesId, simulation::ColorRgb color);
    bool cycleSpeciesColor(simulation::SpeciesId speciesId);
    // field: 0 = min, 1 = max, 2 = initial.
    bool adjustSpeciesPopulation(simulation::SpeciesId speciesId, int field, int delta);
    bool setSpeciesShowGraph(simulation::SpeciesId speciesId, bool show);
    bool setSpeciesLabel(simulation::SpeciesId speciesId, const std::string& label);
    bool removeSpeciesSafe(simulation::SpeciesId speciesId);
    [[nodiscard]] std::size_t countAgentsOfSpecies(simulation::SpeciesId speciesId) const;
    // Phase 31: "Resetar rede neural" for one species. Rebuilds each agent's
    // brain from its own slot config (correct architecture) with a step-varying
    // seed, so the nets are genuinely new — not the deterministic birth nets.
    // Returns the number of brains recreated.
    std::size_t resetNeuralForSpecies(simulation::SpeciesId speciesId);
    // Microfase 32.2: LIVE genetic-editor apply — no reset, no deletion.
    // `applyEditorGenomeToSpecies` overwrites the label's template genome AND the
    // personal genome of every living member with the editor values (registry
    // "bacteria_*" template), refreshing body size/shape and clamping energy to
    // the new cap in place; rescue spawns inherit the template from then on.
    // `applyEditorGenomeToAgents` does the same for the given agents only; each
    // keeps its own label. Brain weights survive (a structural change in the
    // global brain config is handled by NeuralSystem::syncBrains next step).
    // Both return the number of living agents updated.
    std::size_t applyEditorGenomeToSpecies(simulation::SpeciesId speciesId);
    std::size_t applyEditorGenomeToAgents(const std::vector<simulation::EntityId>& ids);
    // Mutable species access so the UI can read records for the Labels list.
    [[nodiscard]] simulation::SpeciesStore& speciesMutable() noexcept { return species_; }

    // Phase 22: small overlay knobs. Render flags live in UiState; the runner
    // does not own them. These two are runtime engine knobs the user can edit
    // through commands but the runner persists them itself.
    [[nodiscard]] bool spatialHashOverlay() const noexcept { return showSpatialHash_; }
    [[nodiscard]] bool simpleRender() const noexcept { return simpleRender_; }

    // Phase 26: neural viewer for the selected agent. Setting a target makes the
    // next step also capture an ActivationTrace + NeuralView for that one agent
    // (trace-on-demand); with no target nothing is captured and neuralTraceCount()
    // does not grow, so the cost is zero when the viewer is hidden.
    void setNeuralViewerTarget(simulation::EntityId id);
    void clearNeuralViewerTarget() noexcept { neuralSystem_.clearTraceTarget(); }
    [[nodiscard]] std::uint64_t neuralTraceCount() const noexcept { return neuralSystem_.traceCount(); }
    [[nodiscard]] bool hasSelectedNeuralView() const noexcept { return neuralSystem_.hasLastView(); }
    [[nodiscard]] const neural::NeuralView& selectedNeuralView() const noexcept { return neuralSystem_.lastView(); }
    [[nodiscard]] const neural::ActivationTrace& selectedNeuralTrace() const noexcept { return neuralSystem_.lastTrace(); }

    // Phase 26: selected-agent vision overlay, reusing the Phase 11/12 debug
    // rays. Setting a target makes the next step fill visionDebug() for that
    // agent only; with no target the perception step requests no debug data.
    void setVisionDebugTarget(simulation::EntityId id) noexcept { visionDebugTargetId_ = id.isValid() ? id.value : 0U; }
    void clearVisionDebugTarget() noexcept { visionDebugTargetId_ = 0U; visionDebug_.clear(); }
    [[nodiscard]] const perception::VisionDebugData& visionDebug() const noexcept { return visionDebug_; }

    // Phase 27: observability. The runner owns the profiler (instruments its own
    // step) and the metrics system; the UI reads them and App adds Render/Ui
    // scopes via profilerMutable(). Both are toggled from the registry each step.
    [[nodiscard]] const core::Profiler& profiler() const noexcept { return profiler_; }
    [[nodiscard]] core::Profiler& profilerMutable() noexcept { return profiler_; }
    [[nodiscard]] const systems::MetricsSystem& metrics() const noexcept { return metrics_; }

    // Phase 30: developer window support.
    // `setProfilerForced(true)` keeps the profiler on while the window is open
    // without touching the user's `profiler_enabled` preference.
    void setProfilerForced(const bool forced) noexcept { profilerForced_ = forced; }
    [[nodiscard]] bool profilerForced() const noexcept { return profilerForced_; }
    // Dev cost-isolation toggles: skip individual systems to see the us/step
    // delta live. Indices follow core::ProfileSection (Perception..SpatialHash).
    // DIAGNOSTIC ONLY — the simulation is not biologically meaningful with a
    // system off. All default to enabled; reset() does not touch them (the UI
    // restores them when the window closes). The flags are read at step time, so
    // toggling off and back on before the next step leaves the run untouched.
    static constexpr int kDevToggleCount = static_cast<int>(core::ProfileSection::SpatialHash) + 1;
    void setDevSystemEnabled(int section, bool enabled) noexcept;
    [[nodiscard]] bool devSystemEnabled(int section) const noexcept;
    void resetDevToggles() noexcept { for (bool& b : devSystemEnabled_) b = true; }
    [[nodiscard]] bool anyDevToggleOff() const noexcept;
    // Dev-window counters (O(brains); call only while the window is open).
    [[nodiscard]] std::array<std::size_t, 8> brainTypeCounts() const { return neuralSystem_.brainTypeCounts(); }
    [[nodiscard]] std::size_t approxBrainBytes() const { return neuralSystem_.approxBrainBytes(); }

private:
    void spawnInitial();
    void rebuildSpatial();
    void runOneStep(double dt);
    // Microfase 31.1: per-label population floor. When the rescue knob is on,
    // every enabled species below its minPopulation gets respawned up to it.
    void applyPopulationRescue();

    const config::ParameterRegistry& parameters_;

    simulation::World world_;
    simulation::AgentStore agents_;
    simulation::FoodStore foods_;
    simulation::GenomeStore genomes_;
    simulation::SpeciesStore species_;
    simulation::ObstacleStore obstacles_;
    simulation::SpatialHash spatialHash_;

    core::Profiler profiler_;
    systems::MetricsSystem metrics_;

    // Phase 30: developer-window state (profiler force + per-system toggles).
    bool profilerForced_ = false;
    bool devSystemEnabled_[kDevToggleCount] = {true, true, true, true, true,
                                                true, true, true, true, true};

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

    // Phase 26: selected-agent vision overlay state (0 = no target).
    perception::VisionDebugData visionDebug_{};
    std::uint64_t visionDebugTargetId_ = 0;
    std::uint64_t seed_ = 1337U;
    double timeScale_ = 1.0;

    SimulationRunnerStats stats_{};
};
} // namespace agentbiosim::sim
