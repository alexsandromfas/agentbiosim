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

struct ActivationTrace
{
    BrainType brainType = BrainType::Mlp;
    std::vector<ActivationLayer> layers;
    // Per-hidden-layer gate values; empty for non-gated brains.
    std::vector<std::vector<double>> gateValues;
    // Per-output shortcut contribution; empty for non-shortcut brains.
    std::vector<double> shortcutContribution;

    void clear()
    {
        brainType = BrainType::Mlp;
        layers.clear();
        gateValues.clear();
        shortcutContribution.clear();
    }
};
} // namespace agentbiosim::neural
