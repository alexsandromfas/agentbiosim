#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Phase 23 selftests for the parameters/preferences UI. Validates:
//   * ParameterRegistry mutation API (setValue clamping, enum domains,
//     restoreDefault), metadata application (RequiresReset / RebuildPerception
//     / RebuildBrains / PendingFuturePhase) and category->tab routing;
//   * UiPreferencesPanel headless helpers (tab filtering, search, effective
//     value with pending overrides, prefsApplyPending applying everything and
//     returning the union of flags);
//   * presence of the parameter set demanded by the Fase 23 prompt across
//     the 7 prefs tabs;
//   * regression: --phase22-selftest still PASS, Microfase 22.1 helpers still
//     functional (verified via flag combinations).
struct Phase23ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase23BenchmarkResult
{
    std::string scenario;
    int parameters = 0;
    int pending = 0;
    int applied = 0;
    double totalMilliseconds = 0.0;
    double averageOpMicroseconds = 0.0;
    std::string notes;
};

[[nodiscard]] Phase23ValidationSummary runPhase23Validation();
[[nodiscard]] std::vector<Phase23BenchmarkResult> runPhase23Microbenchmark();
} // namespace agentbiosim::systems
