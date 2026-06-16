#pragma once

#include <cstddef>
#include <string>

namespace agentbiosim::systems
{
// Fase 34.1 selftest: per-species genome editor backend. Headless (no
// window/ImGui), proving the paradigm that fixes the original pain:
//   * setSpeciesGenomeField writes ONLY the requested field (the other genome
//     traits of the species are untouched) — on the template AND every living
//     member of the species;
//   * editing one species never touches another species' template/members;
//   * a diet field keeps the species' dietSnapshot in sync;
//   * body_size refreshes each member's radius live;
//   * the "+" tab: createSpeciesDefault spawns 5 organisms of a fresh species,
//     while createSpeciesFromSelected reassigns the selection (no spawn);
//   * determinism: the same field edits over the same seed yield the same state.
struct Phase34ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase34ValidationSummary runPhase34Validation();
} // namespace agentbiosim::systems
