#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Phase 28 selftest: `.agentbiosim` save/load. Headless (no UI). Validates that
// a save -> load reopens the simulation exactly as it was:
//   * the engine snapshot round-trips through the file (agents, food, obstacles,
//     species, genomes, world, counters);
//   * every brain type (MLP/Gated/Shortcut/Modulated/RNN/NEAT family) survives
//     the round-trip (weights/graph/recurrent state preserved — the "minds");
//   * a loaded simulation can keep stepping without crashing;
//   * parameters round-trip through the file.
// "Simple" save: it reopens the world as-is; it does NOT promise a bit-identical
// alternate timeline (RNG generator state is not persisted).
struct Phase28ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase28ValidationSummary runPhase28Validation();

struct Phase28DiagnosticsRow
{
    std::string brainType;
    std::size_t agents = 0;
    double saveMilliseconds = 0.0;
    double loadMilliseconds = 0.0;
    std::size_t fileBytes = 0;
};

[[nodiscard]] std::vector<Phase28DiagnosticsRow> runPhase28Diagnostics();
} // namespace agentbiosim::systems
