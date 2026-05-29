#pragma once

#include "neural/ActivationTrace.hpp"
#include "neural/BrainVariant.hpp"
#include "neural/MLPBrain.hpp"

#include <vector>

namespace agentbiosim::neural
{
class BrainExecutor
{
public:
    // Legacy single-brain overload (Phase 9 compatibility).
    [[nodiscard]] std::vector<double> forward(const MLPBrain& brain,
                                              const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr) const
    {
        return brain.forward(input, trace);
    }

    // Phase 14: variant overload dispatches by brain type via std::visit.
    [[nodiscard]] std::vector<double> forward(const BrainVariant& brain,
                                              const std::vector<double>& input,
                                              ActivationTrace* trace = nullptr) const
    {
        return forwardOf(brain, input, trace);
    }
};
} // namespace agentbiosim::neural
