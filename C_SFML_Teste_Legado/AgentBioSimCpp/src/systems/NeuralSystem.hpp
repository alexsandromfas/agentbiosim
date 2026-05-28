#pragma once

#include "config/ParameterRegistry.hpp"
#include "neural/BrainConfig.hpp"
#include "neural/BrainExecutor.hpp"
#include "neural/MLPBrain.hpp"
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

    // Phase 13: inject child brain by cloning the parent brain and mutating.
    // The architecture signature recorded matches the provided config to prevent
    // syncBrains from recreating it as a fresh random brain.
    bool inheritBrain(std::uint64_t childAgentId,
                      std::uint64_t parentAgentId,
                      const neural::BrainConfig& signatureConfig,
                      double mutationRate,
                      double mutationStrength,
                      std::mt19937_64& rng);

    void removeBrainFor(std::uint64_t agentId) noexcept;

    void clear();

    [[nodiscard]] const NeuralStats& lastStats() const noexcept;
    [[nodiscard]] std::size_t brainCount() const noexcept;

private:
    struct BrainSlot
    {
        std::unique_ptr<neural::MLPBrain> brain;
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
};
} // namespace agentbiosim::systems
