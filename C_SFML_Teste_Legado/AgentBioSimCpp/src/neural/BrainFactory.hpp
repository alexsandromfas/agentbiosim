#pragma once

#include "config/ParameterRegistry.hpp"
#include "neural/BrainConfig.hpp"
#include "neural/BrainVariant.hpp"
#include "neural/MLPBrain.hpp"

#include <memory>
#include <random>
#include <string>

namespace agentbiosim::neural
{
struct BrainCreationResult
{
    BrainVariant brain;
    BrainType requestedType = BrainType::Mlp;
    BrainType instantiatedType = BrainType::Mlp;
    bool fallbackToMlp = false;
    std::string message;
};

class BrainFactory
{
public:
    [[nodiscard]] static BrainConfig configFromRegistry(const config::ParameterRegistry& parameters,
                                                        const std::string& speciesPrefix,
                                                        std::size_t inputSize,
                                                        std::size_t outputSize);

    [[nodiscard]] static BrainCreationResult createBrain(const BrainConfig& config, std::mt19937_64& rng);
    [[nodiscard]] static MLPBrain createMlp(const BrainConfig& config, std::mt19937_64& rng);
};
} // namespace agentbiosim::neural
