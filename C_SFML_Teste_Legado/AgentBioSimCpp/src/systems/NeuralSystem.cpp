#include "systems/NeuralSystem.hpp"

#include "config/Parameter.hpp"
#include "neural/BrainFactory.hpp"
#include "perception/PerceptionResult.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_set>
#include <variant>

namespace agentbiosim::systems
{
namespace
{
double parameterDouble(const config::ParameterRegistry& parameters, const std::string& name, const double fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<double>(&definition->defaultValue))
    {
        return *value;
    }
    if (const auto* value = std::get_if<int>(&definition->defaultValue))
    {
        return static_cast<double>(*value);
    }
    return fallback;
}

int parameterInt(const config::ParameterRegistry& parameters, const std::string& name, const int fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<int>(&definition->defaultValue))
    {
        return *value;
    }
    if (const auto* value = std::get_if<double>(&definition->defaultValue))
    {
        return static_cast<int>(*value);
    }
    return fallback;
}

double normalizedCoordinate(const double value, const double minValue, const double maxValue)
{
    const double span = std::max(1.0e-9, maxValue - minValue);
    return std::clamp((value - minValue) / span, 0.0, 1.0);
}
} // namespace

std::size_t NeuralSystem::outputSizeForMovementMode(const MovementMode mode) noexcept
{
    return mode == MovementMode::Omni ? 3U : 2U;
}

NeuralSystemConfig NeuralSystem::fromRegistry(const config::ParameterRegistry& parameters,
                                              const MovementConfig& movementConfig,
                                              const std::size_t inputSize)
{
    NeuralSystemConfig config;
    const std::size_t outputSize = outputSizeForMovementMode(movementConfig.mode);
    const std::size_t effectiveInputSize = inputSize > 0 ? inputSize : syntheticInputSize();
    config.brainConfig = neural::BrainFactory::configFromRegistry(parameters, "bacteria", effectiveInputSize, outputSize);
    config.energyNormalizer = std::max(1.0e-9, parameterDouble(parameters, "bacteria_energy_cap", 400.0));
    const int seed = parameterInt(parameters, "random_seed", -1);
    config.seed = seed >= 0 ? static_cast<std::uint64_t>(seed) : 20260527ULL;
    return config;
}

std::vector<MovementControl> NeuralSystem::produceMovementControls(
    const simulation::AgentStore& agents,
    const simulation::World& world,
    const NeuralSystemConfig& config,
    const perception::PerceptionResult* perception)
{
    lastStats_ = {};
    lastStats_.requestedType = neural::brainTypeName(config.brainConfig.requestedType);
    lastStats_.activeType = neural::brainTypeName(config.brainConfig.type);
    lastStats_.inputSize = config.brainConfig.inputSize;

    const bool usePerception = perception != nullptr && perception->active &&
                               perception->inputSize > 0 && perception->agentCount == agents.size();
    lastStats_.usingPerception = usePerception;

    syncBrains(agents, config);

    std::vector<MovementControl> controls(agents.size());
    for (std::size_t index = 0; index < agents.size(); ++index)
    {
        const auto id = agents.idAt(index);
        const auto it = brainsByAgentId_.find(id.value);
        if (it == brainsByAgentId_.end() || it->second.brain == nullptr)
        {
            continue;
        }

        std::vector<double> input;
        if (usePerception)
        {
            const double* data = perception->inputForAgent(index);
            input.assign(data, data + perception->inputSize);
        }
        else
        {
            input = syntheticInputForAgent(agents, world, config, index);
        }

        const std::vector<double> output = executor_.forward(*it->second.brain, input);
        MovementControl control;
        if (output.size() >= 3U)
        {
            control.forward = output[0];
            control.strafe = output[1];
            control.turn = output[2];
        }
        else if (output.size() >= 2U)
        {
            control.forward = output[0];
            control.turn = output[1];
        }
        controls[index] = control;
        ++lastStats_.agentsProcessed;
    }
    lastStats_.brainCount = brainsByAgentId_.size();
    return controls;
}

void NeuralSystem::clear()
{
    brainsByAgentId_.clear();
    lastStats_ = {};
}

const NeuralStats& NeuralSystem::lastStats() const noexcept
{
    return lastStats_;
}

std::size_t NeuralSystem::brainCount() const noexcept
{
    return brainsByAgentId_.size();
}

void NeuralSystem::syncBrains(const simulation::AgentStore& agents, const NeuralSystemConfig& config)
{
    const std::string signature = config.brainConfig.architectureSignature();
    std::unordered_set<std::uint64_t> liveIds;
    liveIds.reserve(agents.size());

    for (std::size_t index = 0; index < agents.size(); ++index)
    {
        const std::uint64_t id = agents.idAt(index).value;
        liveIds.insert(id);
        auto it = brainsByAgentId_.find(id);
        if (it != brainsByAgentId_.end() && it->second.signature == signature && it->second.brain != nullptr)
        {
            continue;
        }

        std::mt19937_64 rng(config.seed + id * 0x9E3779B97F4A7C15ULL);
        neural::BrainCreationResult created = neural::BrainFactory::createBrain(config.brainConfig, rng);
        BrainSlot slot;
        slot.brain = std::move(created.mlp);
        slot.signature = signature;
        brainsByAgentId_[id] = std::move(slot);
        ++lastStats_.brainsCreated;
        if (created.fallbackToMlp)
        {
            ++lastStats_.fallbacksToMlp;
        }
    }

    for (auto it = brainsByAgentId_.begin(); it != brainsByAgentId_.end();)
    {
        if (liveIds.find(it->first) == liveIds.end())
        {
            it = brainsByAgentId_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::vector<double> NeuralSystem::syntheticInputForAgent(const simulation::AgentStore& agents,
                                                        const simulation::World& world,
                                                        const NeuralSystemConfig& config,
                                                        const std::size_t agentIndex) const
{
    const simulation::Vec2 position = agents.positionAt(agentIndex);
    const simulation::Vec2 minBounds = world.minBounds();
    const simulation::Vec2 maxBounds = world.maxBounds();
    std::vector<double> input(config.brainConfig.inputSize, 0.0);
    if (!input.empty())
    {
        input[0] = std::clamp(agents.energyAt(agentIndex) / std::max(1.0e-9, config.energyNormalizer), 0.0, 2.0);
    }
    if (input.size() > 1U)
    {
        input[1] = std::clamp(agents.ageAt(agentIndex) / 3600.0, 0.0, 1.0);
    }
    if (input.size() > 2U)
    {
        input[2] = normalizedCoordinate(position.x, minBounds.x, maxBounds.x);
    }
    if (input.size() > 3U)
    {
        input[3] = normalizedCoordinate(position.y, minBounds.y, maxBounds.y);
    }
    return input;
}
} // namespace agentbiosim::systems
