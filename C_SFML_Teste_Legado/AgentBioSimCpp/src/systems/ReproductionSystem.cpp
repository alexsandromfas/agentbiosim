#include "systems/ReproductionSystem.hpp"

#include "config/ParameterHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = kPi * 2.0;

simulation::Vec2 clampToWorld(const simulation::World& world, const simulation::Vec2 pos,
                              const double radius)
{
    return world.clampPosition(pos, radius);
}

simulation::Vec2 findChildPosition(const simulation::World& world,
                                   const simulation::Vec2 parentPos,
                                   const double parentRadius,
                                   const double childRadius,
                                   const double spawnRadiusOffset,
                                   std::mt19937_64& rng)
{
    std::uniform_real_distribution<double> angleDist(0.0, kTwoPi);
    const double offset = (parentRadius + childRadius) * std::max(0.1, spawnRadiusOffset);
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const double angle = angleDist(rng);
        const simulation::Vec2 candidate{parentPos.x + std::cos(angle) * offset,
                                          parentPos.y + std::sin(angle) * offset};
        const simulation::Vec2 clamped = clampToWorld(world, candidate, childRadius);
        // Accept the first attempt that does not need significant clamping; otherwise fall
        // back to the clamped position which is always inside the world.
        const double dx = clamped.x - candidate.x;
        const double dy = clamped.y - candidate.y;
        if (dx * dx + dy * dy < 1.0e-6)
        {
            return clamped;
        }
    }
    return clampToWorld(world, parentPos, childRadius);
}
} // namespace

ReproductionConfig ReproductionSystem::fromRegistry(const config::ParameterRegistry& parameters,
                                                     const std::string& speciesPrefix,
                                                     const neural::BrainConfig& brainConfig)
{
    ReproductionConfig config;
    config.speciesPrefix = speciesPrefix;
    config.splitEnergy = std::max(0.0,
        config::parameterDouble(parameters, speciesPrefix + "_split_energy", 150.0));
    const double speciesMinAge = config::parameterDouble(parameters,
        speciesPrefix + "_reproduction_min_age", 0.0);
    const double globalMinAge = config::parameterDouble(parameters,
        "reproduction_min_age", 0.0);
    config.reproductionMinAge = std::max(0.0, std::max(speciesMinAge, globalMinAge));
    const double speciesCooldown = config::parameterDouble(parameters,
        speciesPrefix + "_reproduction_cooldown", 0.0);
    const double globalCooldown = config::parameterDouble(parameters,
        "reproduction_cooldown", 0.0);
    config.reproductionCooldown = std::max(0.0, std::max(speciesCooldown, globalCooldown));
    config.mutationRate = std::clamp(brainConfig.mutationRate, 0.0, 1.0);
    config.mutationStrength = std::max(0.0, brainConfig.mutationStrength);
    config.initialEnergy = std::max(0.0,
        config::parameterDouble(parameters, speciesPrefix + "_initial_energy", 100.0));
    config.energyCap = std::max(0.0,
        config::parameterDouble(parameters, speciesPrefix + "_energy_cap", 400.0));
    config.bodySize = std::max(0.1,
        config::parameterDouble(parameters, speciesPrefix + "_body_size", 9.0));
    config.maxPopulation = std::max(0,
        config::parameterInt(parameters, speciesPrefix + "_max_limit", 0));
    config.minPopulation = std::max(0,
        config::parameterInt(parameters, speciesPrefix + "_min_limit", 0));
    config.populationMinRescue = config::parameterBool(parameters,
        "population_min_rescue_enabled", true);
    const int seedParam = config::parameterInt(parameters, "random_seed", -1);
    config.seed = seedParam >= 0 ? static_cast<std::uint64_t>(seedParam) : 20260528ULL;
    return config;
}

ReproductionStats ReproductionSystem::apply(simulation::AgentStore& agents,
                                             simulation::GenomeStore& genomes,
                                             NeuralSystem& neuralSystem,
                                             const simulation::World& world,
                                             neural::BrainConfig brainSignatureConfig,
                                             const ReproductionConfig& config,
                                             const double dt)
{
    lastStats_ = {};
    if (!rngInitialized_)
    {
        rng_.seed(config.seed);
        rngInitialized_ = true;
    }

    const double safeDt = std::max(0.0, dt);

    // Tick cooldown down for everyone first (independent of reproduction event).
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents.aliveAt(i))
        {
            continue;
        }
        const double cd = agents.reproductionCooldownAt(i);
        if (cd > 0.0)
        {
            agents.setReproductionCooldownAt(i, std::max(0.0, cd - safeDt));
        }
    }

    if (!config.enabled)
    {
        return lastStats_;
    }

    // Snapshot indices to iterate. New agents pushed to the back during the loop
    // must not be re-evaluated this step.
    const std::size_t snapshotSize = agents.size();
    lastStats_.agentsConsidered = snapshotSize;

    std::vector<simulation::EntityId> parents;
    parents.reserve(snapshotSize / 4U + 1U);

    for (std::size_t i = 0; i < snapshotSize; ++i)
    {
        if (!agents.aliveAt(i))
        {
            continue;
        }
        if (agents.energyAt(i) < config.splitEnergy)
        {
            ++lastStats_.blockedByEnergy;
            continue;
        }
        if (agents.ageAt(i) < config.reproductionMinAge)
        {
            ++lastStats_.blockedByAge;
            continue;
        }
        if (agents.reproductionCooldownAt(i) > 0.0)
        {
            ++lastStats_.blockedByCooldown;
            continue;
        }
        parents.push_back(agents.idAt(i));
    }
    lastStats_.agentsEligible = parents.size();

    for (const simulation::EntityId parentId : parents)
    {
        if (config.maxPopulation > 0 &&
            agents.size() >= static_cast<std::size_t>(config.maxPopulation))
        {
            ++lastStats_.blockedByPopulation;
            continue;
        }

        const auto parentIndexOpt = agents.indexOf(parentId);
        if (!parentIndexOpt.has_value())
        {
            continue;
        }
        const std::size_t parentIndex = *parentIndexOpt;
        if (!agents.aliveAt(parentIndex))
        {
            continue;
        }

        // Halve the parent's energy and give the same to the child.
        const double parentEnergyBefore = agents.energyAt(parentIndex);
        const double childEnergy = parentEnergyBefore * 0.5;
        const double parentEnergyAfter = parentEnergyBefore - childEnergy;
        agents.setEnergyAt(parentIndex, parentEnergyAfter);
        agents.setReproductionCooldownAt(parentIndex, config.reproductionCooldown);

        // Genome inheritance (clone parent's genome record).
        const simulation::GenomeId parentGenomeId = agents.genomeIdAt(parentIndex);
        const simulation::GenomeHandle childGenomeHandle = genomes.cloneFrom(parentGenomeId);
        if (!childGenomeHandle.isValid())
        {
            continue;
        }

        // Compose child spawn.
        simulation::AgentSpawn spawn;
        const simulation::Vec2 parentPos = agents.positionAt(parentIndex);
        const double parentRadius = agents.radiusAt(parentIndex);
        const double childRadius = std::max(0.1, config.bodySize > 0.0 ? config.bodySize : parentRadius);
        spawn.position = findChildPosition(world, parentPos, parentRadius, childRadius,
                                            config.spawnRadiusOffset, rng_);
        spawn.angle = std::uniform_real_distribution<double>(-kPi, kPi)(rng_);
        spawn.radius = childRadius;
        spawn.energy = childEnergy;
        spawn.age = 0.0;
        spawn.reproductionCooldown = config.reproductionCooldown;
        spawn.color = agents.colorAt(parentIndex);
        spawn.speciesId = agents.speciesIdAt(parentIndex);
        spawn.genomeId = childGenomeHandle.id;
        spawn.typeCode = agents.typeCodeAt(parentIndex);
        spawn.bodyShape = agents.bodyShapeAt(parentIndex);

        const simulation::EntityId childId = agents.createAgent(spawn);

        // Phase 15: build NeuralMutationConfig honoring brain-config overrides for
        // gate / shortcut / recurrent mutation rates and strengths. -1 in the brain config
        // means "fallback to base" (preserves Python semantics).
        neural::NeuralMutationConfig mutCfg;
        mutCfg.baseRate = config.mutationRate;
        mutCfg.baseStrength = config.mutationStrength;
        mutCfg.gateRate = brainSignatureConfig.future.gateMutationRate;
        mutCfg.gateStrength = brainSignatureConfig.future.gateMutationStrength;
        mutCfg.shortcutRate = brainSignatureConfig.future.shortcutMutationRate;
        mutCfg.shortcutStrength = brainSignatureConfig.future.shortcutMutationStrength;
        mutCfg.recurrentRate = brainSignatureConfig.future.rnnMutationRate;
        mutCfg.recurrentStrength = brainSignatureConfig.future.rnnMutationStrength;
        const bool inherited = neuralSystem.inheritBrain(
            childId.value, parentId.value, brainSignatureConfig, mutCfg, rng_);
        if (inherited && config.mutationRate > 0.0 && config.mutationStrength > 0.0)
        {
            ++lastStats_.mutationsApplied;
        }

        ++lastStats_.birthsThisStep;
        ++totalBirths_;
    }

    return lastStats_;
}

const ReproductionStats& ReproductionSystem::lastStats() const noexcept
{
    return lastStats_;
}

std::uint64_t ReproductionSystem::totalBirths() const noexcept
{
    return totalBirths_;
}

void ReproductionSystem::resetCounters() noexcept
{
    totalBirths_ = 0;
    lastStats_ = {};
}

void ReproductionSystem::reseed(const std::uint64_t seed)
{
    rng_.seed(seed);
    rngInitialized_ = true;
}
} // namespace agentbiosim::systems
