#pragma once

#include <cstdint>
#include <vector>

namespace agentbiosim::neural
{
struct BrainState
{
    std::uint64_t revision = 0;
    std::vector<double> recurrentState;

    [[nodiscard]] bool hasRecurrentState() const noexcept
    {
        return !recurrentState.empty();
    }
};
} // namespace agentbiosim::neural
