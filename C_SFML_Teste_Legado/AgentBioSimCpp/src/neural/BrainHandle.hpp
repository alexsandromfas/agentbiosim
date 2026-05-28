#pragma once

#include <cstdint>

namespace agentbiosim::neural
{
struct BrainHandle
{
    std::uint64_t value = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return value != 0;
    }
};
} // namespace agentbiosim::neural
