#include "systems/ReproductionSystem.hpp"

#include "config/ParameterHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
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
    // Microfase 32.2: the live runner honors each parent's genome; the global
    // registry knobs stay as floors so a per-label genome cannot undercut them.
    config.honorGenome = true;
    config.globalMinAgeFloor = std::max(0.0, globalMinAge);
    const double speciesCooldown = config::parameterDouble(parameters,
        speciesPrefix + "_reproduction_cooldown", 0.0);
    const double globalCooldown = config::parameterDouble(parameters,
        "reproduction_cooldown", 0.0);
    config.reproductionCooldown = std::max(0.0, std::max(speciesCooldown, globalCooldown));
    config.globalCooldownFloor = std::max(0.0, globalCooldown);
    config.mutationRate = std::clamp(brainConfig.mutationRate, 0.0, 1.0);
    config.mutationStrength = std::max(0.0, brainConfig.mutationStrength);
    config.initialEnergy = std::max(0.0,
        config::parameterDouble(parameters, speciesPrefix + "_initial_energy", 100.0));
    config.energyCap = std::max(0.0,
        config::parameterDouble(parameters, speciesPrefix + "_energy_cap", 400.0));
    config.bodySize = std::max(0.1,
        config::parameterDouble(parameters, speciesPrefix + "_body_size", 9.0));
    // Fase 34.3: fallback strategy/litter (the live path honors each parent's genome).
    config.reproductionMode =
        config::parameterString(parameters, speciesPrefix + "_reproduction_mode", "energy") == "age"
            ? simulation::ReproductionMode::Age
            : simulation::ReproductionMode::Energy;
    config.offspringCount = std::max(1,
        config::parameterInt(parameters, speciesPrefix + "_offspring_count", 1));
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
                                             const double dt,
                                             const simulation::SpeciesStore* species)
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
        // Microfase 32.2: with honorGenome the parent's OWN genome sets its
        // split threshold and minimum age (global registry min age = floor).
        // Fallback to the config values when the agent has no genome record.
        double effSplitEnergy = config.splitEnergy;
        double effMinAge = config.reproductionMinAge;
        simulation::ReproductionMode effMode = config.reproductionMode;
        if (config.honorGenome)
        {
            if (const auto* g = genomes.find(agents.genomeIdAt(i)))
            {
                effSplitEnergy = std::max(0.0, g->splitEnergy);
                effMinAge = std::max(g->reproductionMinAge, config.globalMinAgeFloor);
                effMode = g->reproductionMode;
            }
        }
        // Fase 34.3: the energy gate applies ONLY in energy mode. In age mode the
        // organism reproduces from age + cooldown alone (no energy requirement).
        if (effMode == simulation::ReproductionMode::Energy && agents.energyAt(i) < effSplitEnergy)
        {
            ++lastStats_.blockedByEnergy;
            continue;
        }
        if (agents.ageAt(i) < effMinAge)
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

    // Microfase 31.1: per-label counts for the per-species cap. One pass; each
    // birth below increments its label so the cap holds within the same step.
    std::unordered_map<simulation::SpeciesId, std::size_t> countBySpecies;
    if (species != nullptr)
    {
        countBySpecies.reserve(species->records().size() * 2U);
        for (std::size_t i = 0; i < agents.size(); ++i)
        {
            if (agents.aliveAt(i)) ++countBySpecies[agents.speciesIdAt(i)];
        }
    }

    for (const simulation::EntityId parentId : parents)
    {
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

        // Microfase 32.2 + Fase 34.3: per-parent reproduction genetics + STRATEGY.
        // Copy the scalars BEFORE any cloneFrom — the clone can realloc the genome
        // record vector and dangle a GenomeRecord pointer (same hazard as Debt 7).
        double effCooldown = config.reproductionCooldown;
        double childBodySize = config.bodySize;
        double effMutationRate = config.mutationRate;
        double effMutationStrength = config.mutationStrength;
        double effInitialEnergy = config.initialEnergy;
        simulation::ReproductionMode effMode = config.reproductionMode;
        int offspring = config.offspringCount;
        if (config.honorGenome)
        {
            const simulation::GenomeId gid = agents.genomeIdAt(parentIndex);
            if (const auto* pg = genomes.find(gid))
            {
                effCooldown = std::max(pg->reproductionCooldown, config.globalCooldownFloor);
                childBodySize = pg->bodySize;
                effMutationRate = std::clamp(pg->mutationRate, 0.0, 1.0);
                effMutationStrength = std::max(0.0, pg->mutationStrength);
                effInitialEnergy = std::max(0.0, pg->initialEnergy);
                effMode = pg->reproductionMode;
                offspring = std::max(1, pg->offspringCount);
            }
        }

        // Population cap -> how many children actually fit (PER LABEL with a species
        // store; else the global total). Computing the slot count up front lets a whole
        // litter respect the cap. With offspring=1 this reduces to the old single check.
        const simulation::SpeciesId childSpecies = agents.speciesIdAt(parentIndex);
        int slots = offspring;
        if (species != nullptr)
        {
            const auto* rec = species->find(childSpecies);
            if (rec != nullptr && rec->maxPopulation > 0)
                slots = std::min(slots, std::max(0, rec->maxPopulation -
                                                        static_cast<int>(countBySpecies[childSpecies])));
            else if (rec == nullptr && config.maxPopulation > 0)
                slots = std::min(slots, std::max(0, config.maxPopulation - static_cast<int>(agents.size())));
        }
        else if (config.maxPopulation > 0)
        {
            slots = std::min(slots, std::max(0, config.maxPopulation - static_cast<int>(agents.size())));
        }
        if (slots <= 0)
        {
            ++lastStats_.blockedByPopulation;
            continue;
        }
        const int actualChildren = slots;

        // Energy per mode. ENERGY: split the parent's energy among it and the children
        // (each gets 1/(N+1); for N=1 this is the legacy halve -> golden byte-identical).
        // AGE: children get a fresh initialEnergy and the parent KEEPS its energy
        // (reproduction has no energy cost), so reproduction is fully decoupled from food.
        const double parentEnergyBefore = agents.energyAt(parentIndex);
        double childEnergy = effInitialEnergy;
        if (effMode == simulation::ReproductionMode::Energy)
        {
            childEnergy = parentEnergyBefore / static_cast<double>(actualChildren + 1);
            agents.setEnergyAt(parentIndex,
                               parentEnergyBefore - childEnergy * static_cast<double>(actualChildren));
        }
        agents.setReproductionCooldownAt(parentIndex, effCooldown);

        // Parent attributes are stable across createAgent (it appends; indices keep).
        const simulation::Vec2 parentPos = agents.positionAt(parentIndex);
        const double parentRadius = agents.radiusAt(parentIndex);
        const simulation::ColorRgb parentColor = agents.colorAt(parentIndex);
        const simulation::AgentTypeCode parentType = agents.typeCodeAt(parentIndex);
        const simulation::BodyShapeCode parentShape = agents.bodyShapeAt(parentIndex);
        const simulation::GenomeId parentGenomeId = agents.genomeIdAt(parentIndex);
        const double childRadius = std::max(0.1, childBodySize > 0.0 ? childBodySize : parentRadius);

        for (int c = 0; c < actualChildren; ++c)
        {
            // Genome inheritance (clone parent's genome record) — per child.
            const simulation::GenomeHandle childGenomeHandle = genomes.cloneFrom(parentGenomeId);
            if (!childGenomeHandle.isValid())
            {
                continue;
            }
            simulation::AgentSpawn spawn;
            spawn.position = findChildPosition(world, parentPos, parentRadius, childRadius,
                                                config.spawnRadiusOffset, rng_);
            spawn.angle = std::uniform_real_distribution<double>(-kPi, kPi)(rng_);
            spawn.radius = childRadius;
            spawn.energy = childEnergy;
            spawn.age = 0.0;
            spawn.reproductionCooldown = effCooldown;
            spawn.color = parentColor;
            spawn.speciesId = childSpecies;
            spawn.genomeId = childGenomeHandle.id;
            spawn.typeCode = parentType;
            spawn.bodyShape = parentShape;

            const simulation::EntityId childId = agents.createAgent(spawn);
            if (species != nullptr) ++countBySpecies[childSpecies];

            // Phase 15: NeuralMutationConfig honoring brain-config overrides for gate /
            // shortcut / recurrent mutation rates and strengths. -1 means "fallback".
            neural::NeuralMutationConfig mutCfg;
            mutCfg.baseRate = effMutationRate;
            mutCfg.baseStrength = effMutationStrength;
            mutCfg.gateRate = brainSignatureConfig.future.gateMutationRate;
            mutCfg.gateStrength = brainSignatureConfig.future.gateMutationStrength;
            mutCfg.shortcutRate = brainSignatureConfig.future.shortcutMutationRate;
            mutCfg.shortcutStrength = brainSignatureConfig.future.shortcutMutationStrength;
            mutCfg.recurrentRate = brainSignatureConfig.future.rnnMutationRate;
            mutCfg.recurrentStrength = brainSignatureConfig.future.rnnMutationStrength;
            const bool inherited = neuralSystem.inheritBrain(
                childId.value, parentId.value, brainSignatureConfig, mutCfg, rng_);
            if (inherited && effMutationRate > 0.0 && effMutationStrength > 0.0)
            {
                ++lastStats_.mutationsApplied;
            }
            ++lastStats_.birthsThisStep;
            ++totalBirths_;
        }
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
