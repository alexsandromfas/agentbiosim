#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Phase 24 selftests for the operational windows (Editor Genetico, Especies,
// Populacao, Substrato). The headless layer validates:
//   * each window has a distinct open/close command and matching state flag;
//   * the Editor Genetico shows the bacteria species parameters;
//   * Substrato has the world + food + chunk parameter list;
//   * Populacao has population knobs;
//   * the Genoma menu items map to the operational open commands;
//   * apply commands (CmdApplyGenomeToSpecies / CmdApplyPopulation / Cmd
//     ApplyEnvironment / CmdClearAllFood) exist and route correctly;
//   * SpeciesStore continues to expose Bacteria/Predator default records;
//   * GenomeStore is not regressed.
struct Phase24ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase24ValidationSummary runPhase24Validation();
} // namespace agentbiosim::systems
