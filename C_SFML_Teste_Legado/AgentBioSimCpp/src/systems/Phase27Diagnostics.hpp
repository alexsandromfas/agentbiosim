#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Phase 27 selftest: metrics, intelligence and the per-system profiler. Fully
// headless (no ImGui/SFML). Validates:
//   * metrics time series are produced and deterministic for a fixed seed
//     (global + per-species), and the latest sample matches the live state;
//   * the per-system profiler records each system once per step (spatialhash
//     twice), the inner sim sections sum to <= SimStep (overhead documented),
//     and disabled => nothing recorded (zero cost);
//   * the group "smart factor" (intelligence, conceptual parity) is computed;
//   * the logger level gating round-trips (cheap when off).
struct Phase27ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase27ValidationSummary runPhase27Validation();

// Phase 27 diagnostics: overhead of metrics/profiler on vs off, by agent count.
struct Phase27DiagnosticsRow
{
    std::size_t agents = 0;
    bool profilerOn = false;
    bool metricsOn = false;
    double stepMilliseconds = 0.0;
};

[[nodiscard]] std::vector<Phase27DiagnosticsRow> runPhase27Diagnostics();
} // namespace agentbiosim::systems
