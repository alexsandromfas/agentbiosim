#pragma once

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
    std::vector<ActivationLayer> layers;

    void clear()
    {
        layers.clear();
    }
};
} // namespace agentbiosim::neural
