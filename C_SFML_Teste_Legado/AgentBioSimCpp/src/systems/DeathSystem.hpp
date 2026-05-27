#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/AgentStore.hpp"

#include <cstddef>

namespace agentbiosim::systems
{
struct DeathConfig
{
    double deathEnergy = 50.0;
    int maxDeathsPerStep = 5;
    bool corpseToFood = false;
};

struct DeathStats
{
    std::size_t deaths = 0;
};

class DeathSystem
{
public:
    [[nodiscard]] static DeathConfig fromRegistry(const config::ParameterRegistry& parameters);
    [[nodiscard]] DeathStats apply(simulation::AgentStore& agents, const DeathConfig& config) const;
};
} // namespace agentbiosim::systems
