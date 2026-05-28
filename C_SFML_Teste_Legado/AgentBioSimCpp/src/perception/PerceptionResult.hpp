#pragma once

#include <cstddef>
#include <vector>

namespace agentbiosim::perception
{
struct PerceptionResult
{
    std::vector<double> flatInputs;
    std::size_t inputSize = 0;
    std::size_t agentCount = 0;
    bool active = false;

    [[nodiscard]] const double* inputForAgent(const std::size_t index) const noexcept
    {
        return flatInputs.data() + index * inputSize;
    }
};
} // namespace agentbiosim::perception
