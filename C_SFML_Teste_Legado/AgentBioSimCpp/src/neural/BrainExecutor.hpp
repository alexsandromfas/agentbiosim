#pragma once

#include "neural/ActivationTrace.hpp"
#include "neural/MLPBrain.hpp"

#include <vector>

namespace agentbiosim::neural
{
class BrainExecutor
{
public:
    [[nodiscard]] std::vector<double> forward(const MLPBrain& brain,
                                              const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr) const
    {
        return brain.forward(input, trace);
    }
};
} // namespace agentbiosim::neural
