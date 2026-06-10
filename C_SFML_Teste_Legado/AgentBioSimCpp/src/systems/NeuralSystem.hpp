#pragma once

#include "config/ParameterRegistry.hpp"
#include "neural/ActivationTrace.hpp"
#include "neural/BrainConfig.hpp"
#include "neural/BrainExecutor.hpp"
#include "neural/BrainSerializer.hpp"
#include "neural/BrainVariant.hpp"
#include "neural/MLPBrain.hpp"
#include "neural/NeuralView.hpp"
#include "neural/NeuralMutationConfig.hpp"

#include <array>
#include <utility>
#include "simulation/AgentStore.hpp"
#include "simulation/World.hpp"
#include "systems/MovementSystem.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace agentbiosim::perception { struct PerceptionResult; }

namespace agentbiosim::systems
{
struct NeuralSystemConfig
{
    neural::BrainConfig brainConfig;
    double energyNormalizer = 400.0;
    std::uint64_t seed = 20260527U;
};

struct NeuralStats
{
    std::size_t agentsProcessed = 0;
    std::size_t brainCount = 0;
    std::size_t brainsCreated = 0;
    std::size_t fallbacksToMlp = 0;
    std::string requestedType;
    std::string activeType;
    bool usingPerception = false;
    std::size_t inputSize = 0;
};

class NeuralSystem
{
public:
    [[nodiscard]] static constexpr std::size_t syntheticInputSize() noexcept
    {
        return 4U;
    }

    [[nodiscard]] static std::size_t outputSizeForMovementMode(MovementMode mode) noexcept;
    [[nodiscard]] static NeuralSystemConfig fromRegistry(const config::ParameterRegistry& parameters,
                                                         const MovementConfig& movementConfig,
                                                         std::size_t inputSize);

    [[nodiscard]] std::vector<MovementControl> produceMovementControls(
        const simulation::AgentStore& agents,
        const simulation::World& world,
        const NeuralSystemConfig& config,
        const perception::PerceptionResult* perception = nullptr);

    // Phase 13/14: inject child brain by cloning the parent brain and mutating.
    // Mutation uses NeuralMutationConfig so gate/shortcut overrides are honored.
    bool inheritBrain(std::uint64_t childAgentId,
                      std::uint64_t parentAgentId,
                      const neural::BrainConfig& signatureConfig,
                      double mutationRate,
                      double mutationStrength,
                      std::mt19937_64& rng);
    bool inheritBrain(std::uint64_t childAgentId,
                      std::uint64_t parentAgentId,
                      const neural::BrainConfig& signatureConfig,
                      const neural::NeuralMutationConfig& mutCfg,
                      std::mt19937_64& rng);

    // Phase 17: reset (or recreate) brains of agents belonging to a specific species,
    // using the provided brain config. Returns the number of brains recreated.
    // Recurrent state is zeroed via cloneOf/createBrain. Deterministic when seed is set.
    std::size_t resetForSpecies(const simulation::AgentStore& agents,
                                 simulation::SpeciesId speciesId,
                                 const neural::BrainConfig& brainConfig,
                                 std::uint64_t seed);

    void removeBrainFor(std::uint64_t agentId) noexcept;

    void clear();

    [[nodiscard]] const NeuralStats& lastStats() const noexcept;
    [[nodiscard]] std::size_t brainCount() const noexcept;

    // Phase 26: neural viewer trace-on-demand. When a trace target is set, the
    // forward of that one agent also captures an ActivationTrace and builds a
    // read-only NeuralView; every other agent runs the cheap trace-free forward.
    // With no target (viewer hidden) nothing is captured and traceCount() stays
    // put, so the cost is zero. The agent id is the AgentStore EntityId value;
    // 0 means "no target".
    void setTraceTarget(std::uint64_t agentId) noexcept { traceTargetId_ = agentId; }
    void clearTraceTarget() noexcept { traceTargetId_ = 0; lastViewValid_ = false; }
    [[nodiscard]] std::uint64_t traceTarget() const noexcept { return traceTargetId_; }
    [[nodiscard]] std::uint64_t traceCount() const noexcept { return traceCount_; }
    [[nodiscard]] const neural::ActivationTrace& lastTrace() const noexcept { return lastTrace_; }
    [[nodiscard]] bool hasLastView() const noexcept { return lastViewValid_; }
    [[nodiscard]] const neural::NeuralView& lastView() const noexcept { return lastView_; }

    // Phase 28: persistence. Capture every agent's brain as a snapshot (keyed by
    // agent id) and restore them on load (rebuilding via BrainSerializer). The
    // ids must match the restored AgentStore ids so each brain maps to its agent.
    [[nodiscard]] std::vector<std::pair<std::uint64_t, neural::BrainSnapshot>> captureBrains() const;
    void restoreBrains(const std::vector<std::pair<std::uint64_t, neural::BrainSnapshot>>& brains);
    // Phase 28: single-agent export/import (does not clear the other brains).
    [[nodiscard]] bool captureBrain(std::uint64_t agentId, neural::BrainSnapshot& out) const;
    void loadBrain(std::uint64_t agentId, const neural::BrainSnapshot& snapshot);

    // Phase 30: developer-window counters. Both are O(brains) — call only when
    // the window is open. `brainTypeCounts` is indexed by neural::BrainType;
    // `approxBrainBytes` estimates memory as parameterCount * sizeof(double).
    [[nodiscard]] std::array<std::size_t, 8> brainTypeCounts() const;
    [[nodiscard]] std::size_t approxBrainBytes() const;

private:
    // Phase 14: BrainSlot holds a BrainVariant (no heap allocation per brain).
    // Future RNN/NEAT types extend the variant without changing BrainSlot.
    struct BrainSlot
    {
        neural::BrainVariant brain;
        std::string signature;
    };

    void syncBrains(const simulation::AgentStore& agents, const NeuralSystemConfig& config);
    [[nodiscard]] std::vector<double> syntheticInputForAgent(const simulation::AgentStore& agents,
                                                            const simulation::World& world,
                                                            const NeuralSystemConfig& config,
                                                            std::size_t agentIndex) const;

    neural::BrainExecutor executor_;
    std::unordered_map<std::uint64_t, BrainSlot> brainsByAgentId_;
    NeuralStats lastStats_{};

    // Phase 26: trace-on-demand state for the neural viewer.
    std::uint64_t traceTargetId_ = 0;
    std::uint64_t traceCount_ = 0;
    neural::ActivationTrace lastTrace_{};
    neural::NeuralView lastView_{};
    bool lastViewValid_ = false;
};
} // namespace agentbiosim::systems
