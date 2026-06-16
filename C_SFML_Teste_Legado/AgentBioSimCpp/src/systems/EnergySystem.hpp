#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/GenomeStore.hpp"

#include <cstddef>

namespace agentbiosim::systems
{
struct EnergyConfig
{
    double v0Cost = 0.5;
    double vmaxCost = 8.0;
    double vmaxRef = 300.0;
    double energyCap = 400.0;
};

struct EnergyStats
{
    std::size_t agentsProcessed = 0;
    double energyConsumed = 0.0;
};

class EnergySystem
{
public:
    [[nodiscard]] static EnergyConfig fromRegistry(const config::ParameterRegistry& parameters);
    // Fase 34.2: when `genomes` is provided, the metabolic costs (v0/vmax), the
    // speed reference (= the agent's maxSpeed) and the energy cap are read PER AGENT
    // from its genome. nullptr => the global config applies (legacy selftests).
    [[nodiscard]] EnergyStats apply(simulation::AgentStore& agents, double dt, const EnergyConfig& config,
                                    const simulation::GenomeStore* genomes = nullptr) const;

private:
    [[nodiscard]] static double metabolicCostPerSecond(double speed, const EnergyConfig& config);
};
} // namespace agentbiosim::systems
