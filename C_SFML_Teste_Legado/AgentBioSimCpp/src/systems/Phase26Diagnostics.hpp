#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Phase 26 selftest: the selected-agent inspector and neural viewer. Fully
// headless (no ImGui/SFML). It validates the engine-side data path that the UI
// consumes:
//   * the ActivationTrace of the last forward matches the forward output, for
//     each brain type (MLP/Gated/Shortcut/Modulated/RNN/NEAT family);
//   * neural::buildNeuralView produces a correct read-only view per type
//     (columns, nodes, edges, recurrent/NEAT specifics);
//   * trace-on-demand: with a target set the runner captures a trace + view for
//     that one agent; with no target nothing is captured (count stays 0) — the
//     "viewer hidden costs nothing" guarantee;
//   * a selected agent exposes its genome (GenomeRecord) and a valid view;
//   * removing the selected/targeted agent clears safely (no crash, view stale).
struct Phase26ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase26ValidationSummary runPhase26Validation();

// Phase 26 diagnostics: cost of the viewer on vs off, per brain type and agent
// count. Confirms the trace target only adds work for the one targeted agent.
struct Phase26DiagnosticsRow
{
    std::string brainType;
    std::size_t agents = 0;
    bool viewerOn = false;
    double stepMilliseconds = 0.0;
    std::uint64_t traceCount = 0;
};

[[nodiscard]] std::vector<Phase26DiagnosticsRow> runPhase26Diagnostics();
} // namespace agentbiosim::systems
