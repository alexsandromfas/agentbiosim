#include "systems/NeuralSystem.hpp"

#include "config/ParameterHelpers.hpp"
#include "neural/BrainFactory.hpp"
#include "perception/PerceptionResult.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <execution>
#include <numeric>
#include <random>

namespace agentbiosim::systems
{
using config::parameterDouble;
using config::parameterInt;

namespace
{
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
    config.parallelEnabled = config::parameterBool(parameters, "use_parallel_systems", true);
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

    // Phase 26: with no trace target the previous view becomes stale immediately
    // (viewer is hidden). With a target we capture during its forward below.
    if (traceTargetId_ == 0)
    {
        lastViewValid_ = false;
    }
    bool capturedTrace = false;

    std::vector<MovementControl> controls(agents.size());
    std::atomic<std::size_t> processed{0};

    // Phase 32: the per-agent forward is embarrassingly parallel — each agent
    // reads world/perception state (const), mutates only ITS OWN brain slot
    // (RNN/NEAT recurrent state) and writes its own controls[index] slot. There
    // is no RNG and no shared accumulation with order-dependent floating point,
    // so the results are bit-identical to the serial loop for any thread count.
    // `input` is a thread_local buffer (Divida 5: zero allocation after warmup).
    const auto forwardOne = [&](const std::size_t index) {
        const auto id = agents.idAt(index);
        const auto it = brainsByAgentId_.find(id.value);
        if (it == brainsByAgentId_.end())
        {
            return;
        }

        thread_local std::vector<double> input;
        if (usePerception)
        {
            const double* data = perception->inputForAgent(index);
            input.assign(data, data + perception->inputSize);
        }
        else
        {
            syntheticInputForAgent(agents, world, config, index, input);
        }

        std::vector<double> output;
        if (traceTargetId_ != 0 && id.value == traceTargetId_)
        {
            // Trace-capturing forward for the single selected agent. This is the
            // real forward (it still advances RNN/NEAT recurrent state); we just
            // also record the activations and rebuild the read-only view. Only
            // this one agent's thread touches the trace members.
            neural::ActivationTrace trace;
            output = executor_.forward(it->second.brain, input, &trace);
            lastView_ = neural::buildNeuralView(it->second.brain, trace, &input);
            lastTrace_ = std::move(trace);
            lastViewValid_ = true;
            capturedTrace = true;
            ++traceCount_;
        }
        else
        {
            output = executor_.forward(it->second.brain, input);
        }
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
        processed.fetch_add(1, std::memory_order_relaxed);
    };

    constexpr std::size_t kParallelThreshold = 128;
    if (config.parallelEnabled && agents.size() >= kParallelThreshold)
    {
        if (parallelIndices_.size() != agents.size())
        {
            parallelIndices_.resize(agents.size());
            std::iota(parallelIndices_.begin(), parallelIndices_.end(), std::size_t{0});
        }
        std::for_each(std::execution::par, parallelIndices_.begin(), parallelIndices_.end(),
                      forwardOne);
    }
    else
    {
        for (std::size_t index = 0; index < agents.size(); ++index)
        {
            forwardOne(index);
        }
    }
    lastStats_.agentsProcessed = processed.load(std::memory_order_relaxed);

    // Phase 26: a target that did not match any live agent this step (e.g. it
    // just died) leaves no fresh view — mark it stale so the UI hides it safely.
    if (traceTargetId_ != 0 && !capturedTrace)
    {
        lastViewValid_ = false;
    }
    lastStats_.brainCount = brainsByAgentId_.size();
    return controls;
}

bool NeuralSystem::inheritBrain(const std::uint64_t childAgentId,
                                const std::uint64_t parentAgentId,
                                const neural::BrainConfig& signatureConfig,
                                const double mutationRate,
                                const double mutationStrength,
                                std::mt19937_64& rng)
{
    neural::NeuralMutationConfig cfg;
    cfg.baseRate = mutationRate;
    cfg.baseStrength = mutationStrength;
    return inheritBrain(childAgentId, parentAgentId, signatureConfig, cfg, rng);
}

bool NeuralSystem::inheritBrain(const std::uint64_t childAgentId,
                                const std::uint64_t parentAgentId,
                                const neural::BrainConfig& signatureConfig,
                                const neural::NeuralMutationConfig& mutCfg,
                                std::mt19937_64& rng)
{
    const auto parentIt = brainsByAgentId_.find(parentAgentId);
    if (parentIt == brainsByAgentId_.end())
    {
        return false;
    }

    BrainSlot slot;
    // Deep copy via variant clone (no aliasing, no heap alloc per brain).
    slot.brain = neural::cloneOf(parentIt->second.brain);
    slot.signature = signatureConfig.architectureSignature();

    static_cast<void>(neural::mutateOf(slot.brain, mutCfg, rng));

    brainsByAgentId_[childAgentId] = std::move(slot);
    return true;
}

std::size_t NeuralSystem::resetForSpecies(const simulation::AgentStore& agents,
                                            const simulation::SpeciesId speciesId,
                                            const neural::BrainConfig& brainConfig,
                                            const std::uint64_t seed)
{
    const std::string signature = brainConfig.architectureSignature();
    std::size_t resetCount = 0;
    for (std::size_t index = 0; index < agents.size(); ++index)
    {
        if (agents.speciesIdAt(index) != speciesId) continue;
        const std::uint64_t agentId = agents.idAt(index).value;
        const auto it = brainsByAgentId_.find(agentId);
        if (it == brainsByAgentId_.end()) continue;
        std::mt19937_64 rng(seed + agentId * 0x9E3779B97F4A7C15ULL);
        neural::BrainCreationResult created = neural::BrainFactory::createBrain(brainConfig, rng);
        it->second.brain = std::move(created.brain);
        it->second.signature = signature;
        ++resetCount;
    }
    return resetCount;
}

void NeuralSystem::removeBrainFor(const std::uint64_t agentId) noexcept
{
    brainsByAgentId_.erase(agentId);
}

std::size_t NeuralSystem::resetBrainsBySeed(const simulation::AgentStore& agents,
                                            const simulation::SpeciesId speciesId,
                                            const std::uint64_t seed)
{
    std::size_t resetCount = 0;
    for (std::size_t index = 0; index < agents.size(); ++index)
    {
        if (agents.speciesIdAt(index) != speciesId) continue;
        const std::uint64_t agentId = agents.idAt(index).value;
        const auto it = brainsByAgentId_.find(agentId);
        if (it == brainsByAgentId_.end()) continue;
        const neural::BrainConfig cfg =
            std::visit([](const auto& b) { return b.config(); }, it->second.brain);
        std::mt19937_64 rng(seed + agentId * 0x9E3779B97F4A7C15ULL);
        neural::BrainCreationResult created = neural::BrainFactory::createBrain(cfg, rng);
        it->second.brain = std::move(created.brain);
        ++resetCount;
    }
    return resetCount;
}

std::vector<std::pair<std::uint64_t, neural::BrainSnapshot>> NeuralSystem::captureBrains() const
{
    std::vector<std::pair<std::uint64_t, neural::BrainSnapshot>> out;
    out.reserve(brainsByAgentId_.size());
    for (const auto& entry : brainsByAgentId_)
    {
        out.emplace_back(entry.first, neural::BrainSerializer::capture(entry.second.brain));
    }
    return out;
}

void NeuralSystem::restoreBrains(
    const std::vector<std::pair<std::uint64_t, neural::BrainSnapshot>>& brains)
{
    brainsByAgentId_.clear();
    for (const auto& entry : brains)
    {
        BrainSlot slot;
        slot.brain = neural::BrainSerializer::build(entry.second);
        slot.signature = neural::BrainSerializer::signatureOf(entry.second);
        brainsByAgentId_[entry.first] = std::move(slot);
    }
    // The selected-agent view (Phase 26) is stale after a load.
    lastViewValid_ = false;
}

std::array<std::size_t, 8> NeuralSystem::brainTypeCounts() const
{
    std::array<std::size_t, 8> counts{};
    for (const auto& entry : brainsByAgentId_)
    {
        const int type = static_cast<int>(neural::brainTypeOf(entry.second.brain));
        if (type >= 0 && type < 8) ++counts[static_cast<std::size_t>(type)];
    }
    return counts;
}

std::size_t NeuralSystem::approxBrainBytes() const
{
    std::size_t bytes = 0;
    for (const auto& entry : brainsByAgentId_)
    {
        bytes += std::visit([](const auto& b) { return b.parameterCount(); },
                            entry.second.brain) * sizeof(double);
    }
    return bytes;
}

bool NeuralSystem::captureBrain(const std::uint64_t agentId, neural::BrainSnapshot& out) const
{
    const auto it = brainsByAgentId_.find(agentId);
    if (it == brainsByAgentId_.end()) return false;
    out = neural::BrainSerializer::capture(it->second.brain);
    return true;
}

void NeuralSystem::loadBrain(const std::uint64_t agentId, const neural::BrainSnapshot& snapshot)
{
    BrainSlot slot;
    slot.brain = neural::BrainSerializer::build(snapshot);
    slot.signature = neural::BrainSerializer::signatureOf(snapshot);
    brainsByAgentId_[agentId] = std::move(slot);
}

void NeuralSystem::clear()
{
    brainsByAgentId_.clear();
    lastStats_ = {};
    // Phase 26: keep the trace target (the selection survives a soft reset) but
    // drop captured data so the viewer does not show a stale network.
    traceCount_ = 0;
    lastTrace_.clear();
    lastView_.clear();
    lastViewValid_ = false;
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

    for (std::size_t index = 0; index < agents.size(); ++index)
    {
        const std::uint64_t id = agents.idAt(index).value;
        auto it = brainsByAgentId_.find(id);
        if (it != brainsByAgentId_.end() && it->second.signature == signature)
        {
            continue;
        }

        std::mt19937_64 rng(config.seed + id * 0x9E3779B97F4A7C15ULL);
        neural::BrainCreationResult created = neural::BrainFactory::createBrain(config.brainConfig, rng);
        BrainSlot slot;
        slot.brain = std::move(created.brain);
        slot.signature = signature;
        brainsByAgentId_[id] = std::move(slot);
        ++lastStats_.brainsCreated;
        if (created.fallbackToMlp)
        {
            ++lastStats_.fallbacksToMlp;
        }
    }

    // Phase 32 (Divida 5): no per-step unordered_set — the AgentStore already
    // has an O(1) id index, so dead brains are pruned via agents.contains.
    for (auto it = brainsByAgentId_.begin(); it != brainsByAgentId_.end();)
    {
        if (!agents.contains(simulation::EntityId{it->first}))
        {
            it = brainsByAgentId_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void NeuralSystem::syntheticInputForAgent(const simulation::AgentStore& agents,
                                          const simulation::World& world,
                                          const NeuralSystemConfig& config,
                                          const std::size_t agentIndex,
                                          std::vector<double>& input) const
{
    const simulation::Vec2 position = agents.positionAt(agentIndex);
    const simulation::Vec2 minBounds = world.minBounds();
    const simulation::Vec2 maxBounds = world.maxBounds();
    input.assign(config.brainConfig.inputSize, 0.0);
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
}
} // namespace agentbiosim::systems
