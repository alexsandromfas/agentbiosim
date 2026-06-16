#include "simulation/SpeciesBootstrap.hpp"

#include "config/ParameterHelpers.hpp"
#include "neural/BrainFactory.hpp"

#include <algorithm>
#include <cctype>

namespace agentbiosim::simulation
{
namespace
{
ColorRgb toEntityColor(const config::ColorRgb color)
{
    return {static_cast<std::uint8_t>(std::clamp(color.r, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(color.g, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(color.b, 0, 255))};
}

BodyShapeCode toBodyShapeCode(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (value == "circle" || value == "circular" || value == "circulo")
    {
        return BodyShapeCode::Circle;
    }
    return BodyShapeCode::Ellipse;
}
} // namespace

void overwriteGenomeScalarsFromRegistry(GenomeRecord& genome,
                                        const config::ParameterRegistry& registry,
                                        const std::string& prefix)
{
    using config::parameterBool;
    using config::parameterDouble;
    using config::parameterString;

    const std::string& p = prefix;
    genome.bodySize = std::max(0.1, parameterDouble(registry, p + "_body_size", 9.0));
    genome.bodyShape = toBodyShapeCode(parameterString(registry, p + "_body_shape", "ellipse"));
    genome.mutationRate = std::clamp(parameterDouble(registry, p + "_mutation_rate", 0.05), 0.0, 1.0);
    genome.mutationStrength = std::max(0.0, parameterDouble(registry, p + "_mutation_strength", 0.08));
    genome.reproductionMinAge = std::max(0.0, parameterDouble(registry, p + "_reproduction_min_age", 0.0));
    genome.reproductionCooldown = std::max(0.0, parameterDouble(registry, p + "_reproduction_cooldown", 0.0));
    genome.splitEnergy = std::max(0.0, parameterDouble(registry, p + "_split_energy", 150.0));
    genome.initialEnergy = std::max(0.0, parameterDouble(registry, p + "_initial_energy", 100.0));
    genome.energyCap = std::max(0.0, parameterDouble(registry, p + "_energy_cap", 400.0));

    // Fase 34.2: locomotion / metabolic-cost / death-energy traits, copied from the
    // registry (per prefix) into the genome. The Movement/Energy/Death systems read
    // these PER AGENT from the genome from now on; the bacteria_* params are just the
    // factory defaults. Defaults here mirror the registry defaults so a bacteria
    // genome reads exactly the old global values (golden byte-identical).
    genome.maxSpeed = std::max(0.0, parameterDouble(registry, p + "_max_speed", 300.0));
    genome.maxTurn = std::max(0.0, parameterDouble(registry, p + "_max_turn", 3.14159265358979323846));
    genome.allowReverse = parameterBool(registry, p + "_allow_reverse_locomotion", false);
    genome.moveCostV0 = std::max(0.0, parameterDouble(registry, p + "_metab_v0_cost", 0.5));
    genome.moveCostVmax = std::max(0.0, parameterDouble(registry, p + "_metab_vmax_cost", 8.0));
    genome.deathEnergy = std::max(0.0, parameterDouble(registry, p + "_death_energy", 50.0));

    // Phase 18 diet semantics preserved: `diet_same_label` is the legacy Python
    // name for eatSameSpecies; predator prefix flips the eatFood/eatAgents defaults.
    const bool predatorPrefix = (p == "predator");
    genome.diet.eatFood = parameterBool(registry, p + "_diet_food",
                                         predatorPrefix ? false : true);
    genome.diet.eatAgents = parameterBool(registry, p + "_diet_agents",
                                           predatorPrefix ? true : false);
    genome.diet.eatSameSpecies = parameterBool(registry, p + "_diet_same_label", false);
    genome.diet.foodEfficiency = std::max(0.0,
        parameterDouble(registry, p + "_diet_food_efficiency", 1.0));
    genome.diet.agentEfficiency = std::max(0.0,
        parameterDouble(registry, p + "_diet_agent_efficiency", 0.7));
    genome.diet.corpseToFood = parameterBool(registry, p + "_corpse_to_food", false);

    // Microfase 32.5: vision targeting per label (mirrors the diet block). The
    // predator template defaults to also seeing organisms (it must perceive prey);
    // the registry usually carries explicit values, so the fallbacks only matter
    // for labels whose prefix lacks the key.
    genome.vision.seeFood = parameterBool(registry, p + "_retina_see_food", true);
    genome.vision.seeAgents = parameterBool(registry, p + "_retina_see_bacteria",
                                            predatorPrefix ? true : false);
    genome.vision.seePredators = parameterBool(registry, p + "_retina_see_predators", false);
    genome.vision.seeObstacles = parameterBool(registry, p + "_retina_see_obstacles", false);
    genome.vision.seeAll = parameterBool(registry, p + "_retina_see_all", false);
    genome.vision.seeThroughWalls = parameterBool(registry, p + "_retina_see_through_walls", true);
}

SpeciesBootstrapResult bootstrapSpecies(SpeciesStore& species,
                                         GenomeStore& genomes,
                                         const config::ParameterRegistry& registry,
                                         const SpeciesBootstrapSpec& spec,
                                         const std::size_t inputSize,
                                         const std::size_t outputSize)
{
    using config::parameterBool;
    using config::parameterColor;
    using config::parameterDouble;
    using config::parameterInt;
    using config::parameterString;

    SpeciesBootstrapResult result;
    const std::string p = spec.parameterPrefix;

    bool enabled = spec.enabledDefault;
    if (spec.requireFlag && !spec.enabledFlagName.empty())
    {
        enabled = parameterBool(registry, spec.enabledFlagName, spec.enabledDefault);
    }

    SpeciesRecord record;
    record.name = spec.name;
    record.label = spec.label;
    record.parameterPrefix = p;
    record.color = toEntityColor(parameterColor(registry, p + "_color", {220, 220, 220}));
    record.initialCount = std::max(0, parameterInt(registry, p + "_count", 0));
    record.minPopulation = std::max(0, parameterInt(registry, p + "_min_limit", 0));
    record.maxPopulation = std::max(0, parameterInt(registry, p + "_max_limit", 0));
    record.showGraph = parameterBool(registry, p + "_show_graph", spec.showGraph);
    record.populationMinRescueEnabled = parameterBool(registry,
        "population_min_rescue_enabled", true);
    record.enabled = enabled;
    record.legacyAliases = spec.aliases;
    record.typeCode = spec.typeCode;
    record.bodyShape = toBodyShapeCode(parameterString(registry, p + "_body_shape", "ellipse"));

    result.speciesId = species.registerSpecies(std::move(record));
    result.enabled = enabled;

    GenomeRecord genome;
    // Microfase 32.2: scalars + diet shared with the live editor-apply path.
    overwriteGenomeScalarsFromRegistry(genome, registry, p);
    genome.color = toEntityColor(parameterColor(registry, p + "_color", {220, 220, 220}));
    genome.speciesId = result.speciesId;
    genome.typeCode = spec.typeCode;
    genome.speciesPrefix = p;
    genome.brainConfig = neural::BrainFactory::configFromRegistry(registry, p, inputSize, outputSize);

    const DietConfig dietSnapshot = genome.diet;
    const GenomeHandle handle = genomes.createGenome(std::move(genome));
    result.genomeId = handle.id;
    species.setDefaultGenome(result.speciesId, handle.id);
    species.setDietSnapshot(result.speciesId, dietSnapshot);
    return result;
}

DefaultSpeciesBootstrap bootstrapDefaultSpecies(SpeciesStore& species,
                                                GenomeStore& genomes,
                                                const config::ParameterRegistry& registry,
                                                const std::size_t inputSize,
                                                const std::size_t outputSize)
{
    DefaultSpeciesBootstrap result;

    SpeciesBootstrapSpec bacteriaSpec;
    bacteriaSpec.name = "bacteria";
    bacteriaSpec.label = "Bacteria";
    bacteriaSpec.parameterPrefix = "bacteria";
    bacteriaSpec.aliases = {"organismo_1", "organismo_base", "label_bacteria", "labels_bacteria"};
    bacteriaSpec.typeCode = AgentTypeCode::LegacyBacteria;
    bacteriaSpec.requireFlag = false;
    bacteriaSpec.enabledDefault = true;
    bacteriaSpec.showGraph = true;
    result.bacteria = bootstrapSpecies(species, genomes, registry, bacteriaSpec, inputSize, outputSize);

    SpeciesBootstrapSpec predatorSpec;
    predatorSpec.name = "predator";
    predatorSpec.label = "Predator";
    predatorSpec.parameterPrefix = "predator";
    predatorSpec.aliases = {"predador", "predators", "label_predator", "labels_predator"};
    predatorSpec.typeCode = AgentTypeCode::LegacyPredator;
    predatorSpec.requireFlag = true;
    predatorSpec.enabledFlagName = "predators_enabled";
    predatorSpec.enabledDefault = false;
    predatorSpec.showGraph = true;
    result.predator = bootstrapSpecies(species, genomes, registry, predatorSpec, inputSize, outputSize);

    return result;
}
} // namespace agentbiosim::simulation
