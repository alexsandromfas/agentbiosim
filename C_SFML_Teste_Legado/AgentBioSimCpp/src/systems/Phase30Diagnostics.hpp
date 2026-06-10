#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Phase 30 selftest: the Developer Window's data sources, fully headless (no
// ImGui). Validates:
//   * the per-system numbers the window reads (runner profiler) match the Phase
//     29 benchmark for the same scenario/seed (same top-cost system; percentages
//     within a documented margin — timing is noisy, identity is not);
//   * per-system percentages + overhead sum to ~100% of the step;
//   * dev cost-isolation toggles: toggling systems off and back on BEFORE the
//     next step leaves the simulation bit-identical (flags only read at step
//     time); a disabled system genuinely skips its work;
//   * the embedded benchmark path (bench::runScenario) is reproducible and uses
//     fully isolated state;
//   * the profiler force flag works without the registry param (headless).
struct Phase30ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase30ValidationSummary runPhase30Validation();

// Overhead of the window's data collection: profiler forced on vs off across
// agent scales (the window itself is presentation-only; hidden = zero calls).
struct Phase30DiagnosticsRow
{
    std::size_t agents = 0;
    bool profilerForced = false;
    double stepMilliseconds = 0.0;
};

[[nodiscard]] std::vector<Phase30DiagnosticsRow> runPhase30Diagnostics();
} // namespace agentbiosim::systems
