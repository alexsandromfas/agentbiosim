#pragma once

#include "neural/BrainType.hpp"

#include <string>
#include <vector>

namespace agentbiosim::neural
{
struct ActivationLayer
{
    std::string name;
    std::vector<double> values;
    bool outputLayer = false;
};

// Phase 16: per-node activation snapshot for NEAT graph brains.
struct NeatNodeActivation
{
    int id = 0;
    int kind = 0;  // 0=input, 1=hidden, 2=output
    double layer = 0.0;
    double value = 0.0;
};

struct ActivationTrace
{
    BrainType brainType = BrainType::Mlp;
    std::vector<ActivationLayer> layers;
    // Per-hidden-layer gate values; empty for non-gated brains.
    std::vector<std::vector<double>> gateValues;
    // Per-output shortcut contribution; empty for non-shortcut brains.
    std::vector<double> shortcutContribution;
    // Phase 15: recurrent state before/after for RNN brains; empty for non-RNN brains.
    std::vector<double> recurrentStateBefore;
    std::vector<double> recurrentStateAfter;
    double recurrentMemoryDecay = 0.0;
    double recurrentStateClip = 0.0;
    // Phase 16: NEAT graph snapshot; empty for non-NEAT brains.
    std::vector<NeatNodeActivation> neatNodes;
    std::size_t neatConnectionCount = 0;
    std::size_t neatEnabledConnectionCount = 0;
    std::size_t neatRecurrentConnectionCount = 0;

    void clear()
    {
        brainType = BrainType::Mlp;
        layers.clear();
        gateValues.clear();
        shortcutContribution.clear();
        recurrentStateBefore.clear();
        recurrentStateAfter.clear();
        recurrentMemoryDecay = 0.0;
        recurrentStateClip = 0.0;
        neatNodes.clear();
        neatConnectionCount = 0;
        neatEnabledConnectionCount = 0;
        neatRecurrentConnectionCount = 0;
    }
};
} // namespace agentbiosim::neural
