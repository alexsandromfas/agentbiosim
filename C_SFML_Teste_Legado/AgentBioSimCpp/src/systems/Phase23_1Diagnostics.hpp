#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Microfase 23.1 hotfix selftests for the preferences UI:
//   * Numba aliases hidden from the preference helpers;
//   * each Preferences menu item maps to a separate window command (no more
//     "Abrir Preferencias" placeholder);
//   * Help is opened by CmdOpenHelpWindow, not CmdToggleHelpPanel;
//   * friendly labels exist for the most user-visible parameters;
//   * neural combo + color popup commands are wired;
//   * velocity widget emits CmdAdjustTimeScale / CmdSetTimeScale;
//   * regression: --phase23-selftest still PASSes through the new helper
//     filter (parameters are still discoverable; numba ones just hidden).
struct Phase23_1ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase23_1BenchmarkResult
{
    std::string scenario;
    int windowsOpen = 0;
    int popups = 0;
    int parametersVisible = 0;
    double totalMilliseconds = 0.0;
    double averageOpMicroseconds = 0.0;
    std::string notes;
};

[[nodiscard]] Phase23_1ValidationSummary runPhase23_1Validation();
[[nodiscard]] std::vector<Phase23_1BenchmarkResult> runPhase23_1Microbenchmark();
} // namespace agentbiosim::systems
