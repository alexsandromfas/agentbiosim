#include "systems/EnergySystem.hpp"

#include "config/ParameterHelpers.hpp"

#include <algorithm>
#include <cmath>

namespace agentbiosim::systems
{
EnergyConfig EnergySystem::fromRegistry(const config::ParameterRegistry& parameters)
{
    EnergyConfig config;
    config.v0Cost = config::parameterDouble(parameters, "bacteria_metab_v0_cost", config.v0Cost);
    config.vmaxCost = config::parameterDouble(parameters, "bacteria_metab_vmax_cost", config.vmaxCost);
    config.vmaxRef = std::max(1.0e-6, config::parameterDouble(parameters, "bacteria_max_speed", config.vmaxRef));
    config.energyCap = config::parameterDouble(parameters, "bacteria_energy_cap", config.energyCap);
    return config;
}

EnergyStats EnergySystem::apply(simulation::AgentStore& agents, const double dt, const EnergyConfig& config,
                                const simulation::GenomeStore* genomes) const
{
    EnergyStats stats;
    const double safeDt = std::max(0.0, dt);

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents.aliveAt(i))
        {
            continue;
        }
        // Fase 34.2: per-agent metabolic costs + speed reference (= maxSpeed) +
        // energy cap from the genome. A bacteria genome carries the old global
        // defaults, so this stays byte-identical.
        EnergyConfig cfg = config;
        if (genomes != nullptr)
        {
            if (const auto* g = genomes->find(agents.genomeIdAt(i)))
            {
                cfg.v0Cost = g->moveCostV0;
                cfg.vmaxCost = g->moveCostVmax;
                cfg.vmaxRef = std::max(1.0e-6, g->maxSpeed);
                cfg.energyCap = g->energyCap;
            }
        }
        const simulation::Vec2 velocity = agents.velocityAt(i);
        const double speed = std::hypot(velocity.x, velocity.y);
        const double energyBefore = agents.energyAt(i);
        const double cost = metabolicCostPerSecond(speed, cfg) * safeDt;
        agents.setEnergyAt(i, energyBefore - cost);
        if (cfg.energyCap >= 0.0 && agents.energyAt(i) > cfg.energyCap)
        {
            agents.setEnergyAt(i, cfg.energyCap);
        }
        agents.addAgeAt(i, safeDt);
        stats.energyConsumed += std::max(0.0, energyBefore - agents.energyAt(i));
        ++stats.agentsProcessed;
    }

    return stats;
}

double EnergySystem::metabolicCostPerSecond(const double speed, const EnergyConfig& config)
{
    const double vmaxRef = std::max(1.0e-6, config.vmaxRef);
    const double normalized = std::clamp(speed, 0.0, vmaxRef) / vmaxRef;
    return config.v0Cost + normalized * (config.vmaxCost - config.v0Cost);
}
} // namespace agentbiosim::systems
