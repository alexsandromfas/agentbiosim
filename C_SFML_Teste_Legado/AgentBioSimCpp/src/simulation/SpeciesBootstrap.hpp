#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpeciesStore.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::simulation
{
struct SpeciesBootstrapSpec
{
    std::string name;
    std::string label;
    std::string parameterPrefix;
    std::vector<std::string> aliases;
    AgentTypeCode typeCode = AgentTypeCode::Organism;
    bool requireFlag = false;
    std::string enabledFlagName;
    bool enabledDefault = true;
    bool showGraph = true;
};

struct SpeciesBootstrapResult
{
    SpeciesId speciesId = kInvalidSpeciesId;
    GenomeId genomeId = kInvalidGenomeId;
    bool enabled = true;
};

// Phase 17: bootstrap a species in SpeciesStore from registry params under a prefix,
// and create the species default genome in GenomeStore. The resulting species
// holds aliases (bacteria/predator + the legacy "label" terms) and a defaultGenomeId
// that ReproductionSystem and AgentStore use as the canonical lineage origin.
//
// `inputSize`/`outputSize` are used to seed BrainConfig dimensions; the actual
// runtime input size will be overridden by PerceptionSystem.
SpeciesBootstrapResult bootstrapSpecies(SpeciesStore& species,
                                         GenomeStore& genomes,
                                         const config::ParameterRegistry& registry,
                                         const SpeciesBootstrapSpec& spec,
                                         std::size_t inputSize,
                                         std::size_t outputSize);

// Bootstrap the two default canonical species (bacteria + predator). Predator
// only spawns when `predators_enabled` registry flag is true; the species record
// is still registered (with enabled=false) so aliases keep resolving.
struct DefaultSpeciesBootstrap
{
    SpeciesBootstrapResult bacteria;
    SpeciesBootstrapResult predator;
};

DefaultSpeciesBootstrap bootstrapDefaultSpecies(SpeciesStore& species,
                                                GenomeStore& genomes,
                                                const config::ParameterRegistry& registry,
                                                std::size_t inputSize,
                                                std::size_t outputSize);

// Microfase 32.2: overwrite the genome's editor-managed scalars (body, mutation,
// reproduction thresholds, energies, diet) from the registry params under
// `prefix`. Identity fields (id/parent/generation/speciesId/typeCode/prefix),
// color and brainConfig are NOT touched — the caller owns those. Shared by
// bootstrapSpecies and the live "Aplicar" path of the genetic editor.
void overwriteGenomeScalarsFromRegistry(GenomeRecord& genome,
                                        const config::ParameterRegistry& registry,
                                        const std::string& prefix);
} // namespace agentbiosim::simulation
