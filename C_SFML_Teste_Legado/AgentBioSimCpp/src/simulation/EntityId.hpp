#pragma once

#include <cstdint>

namespace agentbiosim::simulation
{
struct EntityId
{
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return value != 0;
    }

    [[nodiscard]] static constexpr EntityId invalid() noexcept
    {
        return EntityId{};
    }
};

[[nodiscard]] constexpr bool operator==(EntityId lhs, EntityId rhs) noexcept
{
    return lhs.value == rhs.value;
}

[[nodiscard]] constexpr bool operator!=(EntityId lhs, EntityId rhs) noexcept
{
    return !(lhs == rhs);
}
} // namespace agentbiosim::simulation
