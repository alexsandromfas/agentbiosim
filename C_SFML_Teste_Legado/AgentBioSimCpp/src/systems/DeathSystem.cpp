#include "systems/DeathSystem.hpp"

#include "config/ParameterHelpers.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace agentbiosim::systems
{
using config::parameterBool;
using config::parameterDouble;
using config::parameterInt;

DeathConfig DeathSystem::fromRegistry(const config::ParameterRegistry& parameters)
{
    DeathConfig config;
    config.deathEnergy = parameterDouble(parameters, "bacteria_death_energy", config.deathEnergy);
    config.maxDeathsPerStep = std::max(0, parameterInt(parameters, "max_deaths_per_step", config.maxDeathsPerStep));
    config.corpseToFood = parameterBool(parameters, "bacteria_corpse_to_food", config.corpseToFood);
    return config;
}

DeathStats DeathSystem::apply(simulation::AgentStore& agents, const DeathConfig& config) const
{
    DeathStats stats;
    if (config.maxDeathsPerStep <= 0 || agents.empty())
    {
        return stats;
    }

    std::vector<std::pair<double, simulation::EntityId>> candidates;
    candidates.reserve(agents.size());
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents.aliveAt(i) && agents.energyAt(i) <= config.deathEnergy)
        {
            candidates.emplace_back(agents.energyAt(i), agents.idAt(i));
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });

    const std::size_t deathsToApply = std::min<std::size_t>(static_cast<std::size_t>(config.maxDeathsPerStep), candidates.size());
    for (std::size_t i = 0; i < deathsToApply; ++i)
    {
        if (agents.removeAgent(candidates[i].second))
        {
            ++stats.deaths;
        }
    }

    return stats;
}
} // namespace agentbiosim::systems
