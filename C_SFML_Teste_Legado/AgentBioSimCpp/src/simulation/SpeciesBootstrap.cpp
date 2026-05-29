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
    genome.bodySize = std::max(0.1, parameterDouble(registry, p + "_body_size", 9.0));
    genome.bodyShape = toBodyShapeCode(parameterString(registry, p + "_body_shape", "ellipse"));
    genome.color = toEntityColor(parameterColor(registry, p + "_color", {220, 220, 220}));
    genome.mutationRate = std::clamp(parameterDouble(registry, p + "_mutation_rate", 0.05), 0.0, 1.0);
    genome.mutationStrength = std::max(0.0, parameterDouble(registry, p + "_mutation_strength", 0.08));
    genome.reproductionMinAge = std::max(0.0, parameterDouble(registry, p + "_reproduction_min_age", 0.0));
    genome.reproductionCooldown = std::max(0.0, parameterDouble(registry, p + "_reproduction_cooldown", 0.0));
    genome.splitEnergy = std::max(0.0, parameterDouble(registry, p + "_split_energy", 150.0));
    genome.initialEnergy = std::max(0.0, parameterDouble(registry, p + "_initial_energy", 100.0));
    genome.energyCap = std::max(0.0, parameterDouble(registry, p + "_energy_cap", 400.0));
    genome.speciesId = result.speciesId;
    genome.typeCode = spec.typeCode;
    genome.speciesPrefix = p;
    genome.brainConfig = neural::BrainFactory::configFromRegistry(registry, p, inputSize, outputSize);

    const GenomeHandle handle = genomes.createGenome(std::move(genome));
    result.genomeId = handle.id;
    species.setDefaultGenome(result.speciesId, handle.id);
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
