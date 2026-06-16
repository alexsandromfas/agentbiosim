#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpeciesStore.hpp"

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
    std::size_t blockedByMinPopulation = 0;
};

class DeathSystem
{
public:
    [[nodiscard]] static DeathConfig fromRegistry(const config::ParameterRegistry& parameters);
    // Fase 34.2: when `genomes` is provided, the starvation threshold (deathEnergy)
    // is read PER AGENT from its genome. nullptr => the global config threshold.
    [[nodiscard]] DeathStats apply(simulation::AgentStore& agents, const DeathConfig& config,
                                     const simulation::SpeciesStore* species = nullptr,
                                     const simulation::GenomeStore* genomes = nullptr) const;
};
} // namespace agentbiosim::systems
