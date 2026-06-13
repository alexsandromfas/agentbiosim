#include "systems/DeathSystem.hpp"

#include "config/ParameterHelpers.hpp"

#include <algorithm>
#include <unordered_map>
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

DeathStats DeathSystem::apply(simulation::AgentStore& agents, const DeathConfig& config,
                                const simulation::SpeciesStore* species) const
{
    DeathStats stats;
    if (config.maxDeathsPerStep <= 0 || agents.empty())
    {
        return stats;
    }

    // Microfase 32.4: per-species live population, used to enforce the
    // minPopulation floor. `projected` is decremented as deaths are applied so a
    // single step's batch can never push a species below its floor (the old draft
    // counted once and over-killed when several members starved at the same step).
    std::unordered_map<simulation::SpeciesId, std::size_t> projected;
    if (species != nullptr)
    {
        for (std::size_t i = 0; i < agents.size(); ++i)
        {
            if (agents.aliveAt(i))
            {
                ++projected[agents.speciesIdAt(i)];
            }
        }
    }

    // Collect starvation candidates with their species so the floor check can run
    // per label after sorting by energy (weakest dies first).
    struct Candidate
    {
        double energy;
        simulation::EntityId id;
        simulation::SpeciesId species;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(agents.size());
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (agents.aliveAt(i) && agents.energyAt(i) <= config.deathEnergy)
        {
            candidates.push_back({agents.energyAt(i), agents.idAt(i), agents.speciesIdAt(i)});
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& left, const Candidate& right) {
        return left.energy < right.energy;
    });

    const std::size_t maxDeaths = static_cast<std::size_t>(config.maxDeathsPerStep);
    for (const Candidate& cand : candidates)
    {
        if (stats.deaths >= maxDeaths)
        {
            break;
        }
        // Microfase 32.4: block death while the label sits at (or below) its floor.
        // Population is maintained by NOT dying + reproduction, never by respawn.
        if (species != nullptr)
        {
            const auto* rec = species->find(cand.species);
            if (rec != nullptr && rec->minPopulation > 0 &&
                projected[cand.species] <= static_cast<std::size_t>(rec->minPopulation))
            {
                ++stats.blockedByMinPopulation;
                continue;
            }
        }
        if (agents.removeAgent(cand.id))
        {
            ++stats.deaths;
            if (species != nullptr && projected[cand.species] > 0)
            {
                --projected[cand.species];
            }
        }
    }

    return stats;
}
} // namespace agentbiosim::systems
