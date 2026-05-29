#include "neural/BrainFactory.hpp"

#include "config/ParameterHelpers.hpp"
#include "neural/NEATGraphBrain.hpp"
#include "neural/SimpleRNNBrain.hpp"

#include <algorithm>
#include <string>

namespace agentbiosim::neural
{
using config::parameterBool;
using config::parameterDouble;
using config::parameterInt;
using config::parameterString;

namespace
{
std::vector<std::size_t> hiddenLayersFromRegistry(const config::ParameterRegistry& parameters,
                                                  const std::string& speciesPrefix)
{
    const int hiddenCount = std::clamp(parameterInt(parameters, speciesPrefix + "_hidden_layers", 4), 0, 5);
    std::vector<std::size_t> hiddenLayers;
    hiddenLayers.reserve(static_cast<std::size_t>(hiddenCount));
    for (int index = 1; index <= hiddenCount; ++index)
    {
        const int fallback = index <= 4 ? 20 : 0;
        const int neurons = parameterInt(parameters, speciesPrefix + "_neurons_layer_" + std::to_string(index), fallback);
        if (neurons > 0)
        {
            hiddenLayers.push_back(static_cast<std::size_t>(neurons));
        }
    }
    return hiddenLayers;
}
} // namespace

BrainConfig BrainFactory::configFromRegistry(const config::ParameterRegistry& parameters,
                                             const std::string& speciesPrefix,
                                             const std::size_t inputSize,
                                             const std::size_t outputSize)
{
    BrainConfig config;
    config.requestedType = normalizeBrainType(parameterString(parameters, "neural_network_type", "mlp"));
    config.type = config.requestedType;
    if (!isImplementedInPhase16(config.type))
    {
        config.fallbackToMlp = true;
        config.fallbackReason = std::string(brainTypeName(config.type)) + " is planned but not implemented yet";
        config.type = BrainType::Mlp;
    }

    config.inputSize = std::max<std::size_t>(1U, inputSize);
    config.outputSize = std::max<std::size_t>(1U, outputSize);
    config.hiddenLayers = hiddenLayersFromRegistry(parameters, speciesPrefix);
    config.mutationRate = std::clamp(parameterDouble(parameters, speciesPrefix + "_mutation_rate", 0.05), 0.0, 1.0);
    config.mutationStrength = std::max(0.0, parameterDouble(parameters, speciesPrefix + "_mutation_strength", 0.08));
    config.structuralJitter = std::max(0, parameterInt(parameters, speciesPrefix + "_structural_jitter", 0));
    config.initStd = 1.0;
    config.randomBiases = true;

    config.future.gateInit = parameterDouble(parameters, "neural_gate_init", config.future.gateInit);
    config.future.gateMin = parameterDouble(parameters, "neural_gate_min", config.future.gateMin);
    config.future.gateMax = parameterDouble(parameters, "neural_gate_max", config.future.gateMax);
    config.future.gateMutationRate = parameterDouble(parameters, "neural_gate_mutation_rate", config.future.gateMutationRate);
    config.future.gateMutationStrength = parameterDouble(parameters, "neural_gate_mutation_strength", config.future.gateMutationStrength);
    config.future.shortcutInitStd = parameterDouble(parameters, "neural_shortcut_init_std", config.future.shortcutInitStd);
    config.future.shortcutScale = parameterDouble(parameters, "neural_shortcut_scale", config.future.shortcutScale);
    config.future.shortcutMutationRate = parameterDouble(parameters, "neural_shortcut_mutation_rate", config.future.shortcutMutationRate);
    config.future.shortcutMutationStrength = parameterDouble(parameters, "neural_shortcut_mutation_strength", config.future.shortcutMutationStrength);
    config.future.rnnRecurrentInitStd = parameterDouble(parameters, "neural_rnn_recurrent_init_std", config.future.rnnRecurrentInitStd);
    config.future.rnnRecurrentScale = parameterDouble(parameters, "neural_rnn_recurrent_scale", config.future.rnnRecurrentScale);
    config.future.rnnMemoryDecay = parameterDouble(parameters, "neural_rnn_memory_decay", config.future.rnnMemoryDecay);
    config.future.rnnStateClip = parameterDouble(parameters, "neural_rnn_state_clip", config.future.rnnStateClip);
    config.future.rnnResetStateOnCopy = parameterBool(parameters, "neural_rnn_reset_state_on_copy", config.future.rnnResetStateOnCopy);
    config.future.rnnMutationRate = parameterDouble(parameters, "neural_rnn_mutation_rate", config.future.rnnMutationRate);
    config.future.rnnMutationStrength = parameterDouble(parameters, "neural_rnn_mutation_strength", config.future.rnnMutationStrength);

    // Phase 16: NEAT-family config. Same shape for all three NEAT types; we just
    // pick the right parameter prefix based on the requested brain. Recurrent-only
    // params still get loaded for non-recurrent variants because the registry
    // defines them only under neural_recurrent_neat; non-recurrent prefixes fall
    // back to the struct defaults via the third argument.
    const char* neatPrefix = nullptr;
    if (config.requestedType == BrainType::Neat)
    {
        neatPrefix = "neural_neat";
    }
    else if (config.requestedType == BrainType::SimpleNeat)
    {
        neatPrefix = "neural_proto_neat";
    }
    else if (config.requestedType == BrainType::RecurrentNeat)
    {
        neatPrefix = "neural_recurrent_neat";
    }
    if (neatPrefix != nullptr)
    {
        const std::string p = neatPrefix;
        config.neat.initialTopology = parameterString(parameters, p + "_initial_topology", config.neat.initialTopology);
        config.neat.weightInitStd = parameterDouble(parameters, p + "_weight_init_std", config.neat.weightInitStd);
        config.neat.weightMutationRate = parameterDouble(parameters, p + "_weight_mutation_rate", config.neat.weightMutationRate);
        config.neat.weightMutationStrength = parameterDouble(parameters, p + "_weight_mutation_strength", config.neat.weightMutationStrength);
        config.neat.addConnectionRate = parameterDouble(parameters, p + "_add_connection_rate", config.neat.addConnectionRate);
        config.neat.addNodeRate = parameterDouble(parameters, p + "_add_node_rate", config.neat.addNodeRate);
        config.neat.toggleConnectionRate = parameterDouble(parameters, p + "_toggle_connection_rate", config.neat.toggleConnectionRate);
        config.neat.removeConnectionRate = parameterDouble(parameters, p + "_remove_connection_rate", config.neat.removeConnectionRate);
        config.neat.resetWeightRate = parameterDouble(parameters, p + "_reset_weight_rate", config.neat.resetWeightRate);
        config.neat.maxHiddenNodes = parameterInt(parameters, p + "_max_hidden_nodes", config.neat.maxHiddenNodes);
        config.neat.maxConnections = parameterInt(parameters, p + "_max_connections", config.neat.maxConnections);
        if (config.requestedType == BrainType::RecurrentNeat)
        {
            config.neat.recurrentConnectionRate = parameterDouble(parameters, p + "_recurrent_connection_rate", config.neat.recurrentConnectionRate);
            config.neat.memoryDecay = parameterDouble(parameters, p + "_memory_decay", config.neat.memoryDecay);
            config.neat.stateClip = parameterDouble(parameters, p + "_state_clip", config.neat.stateClip);
            config.neat.resetStateOnCopy = parameterBool(parameters, p + "_reset_state_on_copy", config.neat.resetStateOnCopy);
        }
    }

    // Phase 14: prefer renamed parameters; aliases below cover legacy names.
    config.performance.useBatchForward = parameterBool(parameters, "use_batch_forward", config.performance.useBatchForward);
    config.performance.batchForwardMinSize = parameterInt(parameters, "batch_forward_min_size", config.performance.batchForwardMinSize);
    config.performance.brainCacheDisabled = parameterBool(parameters, "brain_cache_disable", config.performance.brainCacheDisabled);
    config.performance.brainCacheMaxEntries = parameterInt(parameters, "brain_cache_max_entries", config.performance.brainCacheMaxEntries);
    config.performance.brainCacheMaxMb = parameterInt(parameters, "brain_cache_max_mb", config.performance.brainCacheMaxMb);
    config.performance.brainCacheLog = parameterBool(parameters, "brain_cache_log", config.performance.brainCacheLog);
    return config;
}

BrainCreationResult BrainFactory::createBrain(const BrainConfig& config, std::mt19937_64& rng)
{
    BrainCreationResult result;
    result.requestedType = config.requestedType;
    result.fallbackToMlp = config.fallbackToMlp;
    if (result.fallbackToMlp)
    {
        result.message = config.fallbackReason.empty()
                             ? "non-implemented brain requested; falling back to MLP"
                             : config.fallbackReason;
        result.brain = createMlp(config, rng);
        result.instantiatedType = BrainType::Mlp;
        return result;
    }

    switch (config.type)
    {
    case BrainType::GatedMlp:
        result.brain = GatedMLPBrain(config, rng);
        result.instantiatedType = BrainType::GatedMlp;
        break;
    case BrainType::ShortcutMlp:
        result.brain = ShortcutMLPBrain(config, rng);
        result.instantiatedType = BrainType::ShortcutMlp;
        break;
    case BrainType::ModulatedMlp:
        result.brain = ModulatedMLPBrain(config, rng);
        result.instantiatedType = BrainType::ModulatedMlp;
        break;
    case BrainType::SimpleRnn:
        result.brain = SimpleRNNBrain(config, rng);
        result.instantiatedType = BrainType::SimpleRnn;
        break;
    case BrainType::Neat:
        result.brain = NEATGraphBrain(config, rng);
        result.instantiatedType = BrainType::Neat;
        break;
    case BrainType::SimpleNeat:
        result.brain = NEATGraphBrain(config, rng);
        result.instantiatedType = BrainType::SimpleNeat;
        break;
    case BrainType::RecurrentNeat:
        result.brain = NEATGraphBrain(config, rng);
        result.instantiatedType = BrainType::RecurrentNeat;
        break;
    case BrainType::Mlp:
    default:
        result.brain = createMlp(config, rng);
        result.instantiatedType = BrainType::Mlp;
        break;
    }
    return result;
}

MLPBrain BrainFactory::createMlp(const BrainConfig& config, std::mt19937_64& rng)
{
    BrainConfig mlpConfig = config;
    mlpConfig.type = BrainType::Mlp;
    return MLPBrain(mlpConfig, rng);
}
} // namespace agentbiosim::neural
