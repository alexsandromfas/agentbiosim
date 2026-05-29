#pragma once

#include "neural/BrainType.hpp"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace agentbiosim::neural
{
struct FutureNeuralConfig
{
    double gateInit = 1.0;
    double gateMin = 0.0;
    double gateMax = 2.0;
    double gateMutationRate = -1.0;
    double gateMutationStrength = -1.0;
    double shortcutInitStd = 0.05;
    double shortcutScale = 0.25;
    double shortcutMutationRate = -1.0;
    double shortcutMutationStrength = -1.0;
    double rnnRecurrentInitStd = 0.08;
    double rnnRecurrentScale = 0.35;
    double rnnMemoryDecay = 0.6;
    double rnnStateClip = 1.0;
    bool rnnResetStateOnCopy = true;
    double rnnMutationRate = -1.0;
    double rnnMutationStrength = -1.0;
};

struct BrainPerformanceConfig
{
    // Phase 14: renamed from useNumbaBrainForward/numbaBrainForwardMinBatch.
    // Parameter aliases preserved at the ParameterRegistry layer.
    bool useBatchForward = false;
    int batchForwardMinSize = 256;
    bool brainCacheDisabled = false;
    int brainCacheMaxEntries = 32;
    int brainCacheMaxMb = 512;
    bool brainCacheLog = false;
};

// Phase 16: configuration shared by NEAT common, simplified and recurrent variants.
// Recurrent-only fields (recurrentConnectionRate, memoryDecay, stateClip, resetStateOnCopy)
// are loaded for all NEAT types so saves remain consistent, but they only take effect
// when the brain type is RecurrentNeat (allowRecurrentEdges).
struct NeatConfig
{
    std::string initialTopology = "minimal";
    double weightInitStd = 0.6;
    double weightMutationRate = -1.0;
    double weightMutationStrength = -1.0;
    double addConnectionRate = 0.08;
    double addNodeRate = 0.03;
    double toggleConnectionRate = 0.01;
    double removeConnectionRate = 0.0;
    double resetWeightRate = 0.02;
    int maxHiddenNodes = 64;
    int maxConnections = 512;
    double recurrentConnectionRate = 0.12;
    double memoryDecay = 0.85;
    double stateClip = 1.0;
    bool resetStateOnCopy = true;
};

struct BrainConfig
{
    BrainType requestedType = BrainType::Mlp;
    BrainType type = BrainType::Mlp;
    std::size_t inputSize = 4;
    std::size_t outputSize = 2;
    std::vector<std::size_t> hiddenLayers{20, 20, 20, 20};
    double mutationRate = 0.05;
    double mutationStrength = 0.08;
    int structuralJitter = 0;
    double initStd = 1.0;
    bool randomBiases = true;
    bool fallbackToMlp = false;
    std::string fallbackReason;
    FutureNeuralConfig future;
    BrainPerformanceConfig performance;
    NeatConfig neat;

    [[nodiscard]] std::vector<std::size_t> layerSizes() const
    {
        std::vector<std::size_t> sizes;
        sizes.reserve(hiddenLayers.size() + 2U);
        sizes.push_back(inputSize);
        for (const std::size_t hidden : hiddenLayers)
        {
            if (hidden > 0U)
            {
                sizes.push_back(hidden);
            }
        }
        sizes.push_back(outputSize);
        return sizes;
    }

    [[nodiscard]] std::string architectureSignature() const
    {
        std::ostringstream out;
        out << brainTypeName(type) << ':' << inputSize << ':';
        for (const std::size_t hidden : hiddenLayers)
        {
            out << hidden << ',';
        }
        out << ':' << outputSize << ':' << initStd << ':' << (randomBiases ? 1 : 0);
        return out.str();
    }
};
} // namespace agentbiosim::neural
