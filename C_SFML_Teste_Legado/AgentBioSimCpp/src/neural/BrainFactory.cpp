#include "neural/BrainFactory.hpp"

#include "config/ParameterHelpers.hpp"

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
    if (!isImplementedInPhase9(config.type))
    {
        config.fallbackToMlp = true;
        config.fallbackReason = std::string(brainTypeName(config.type)) + " is planned but not implemented in Phase 9";
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

    config.performance.useNumbaBrainForward = parameterBool(parameters, "use_numba_brain_forward", config.performance.useNumbaBrainForward);
    config.performance.numbaBrainForwardMinBatch = parameterInt(parameters, "numba_brain_forward_min_batch", config.performance.numbaBrainForwardMinBatch);
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
    result.instantiatedType = BrainType::Mlp;
    result.fallbackToMlp = config.fallbackToMlp || config.type != BrainType::Mlp;
    if (result.fallbackToMlp)
    {
        result.message = config.fallbackReason.empty() ? "non-MLP brain requested before implementation phase"
                                                       : config.fallbackReason;
    }
    result.mlp = std::make_unique<MLPBrain>(createMlp(config, rng));
    return result;
}

MLPBrain BrainFactory::createMlp(const BrainConfig& config, std::mt19937_64& rng)
{
    BrainConfig mlpConfig = config;
    mlpConfig.type = BrainType::Mlp;
    return MLPBrain(mlpConfig, rng);
}
} // namespace agentbiosim::neural
