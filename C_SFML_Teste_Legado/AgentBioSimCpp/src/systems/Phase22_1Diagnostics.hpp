#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Phase 22.1 hotfix selftests. Verify the menu dropdown routing, brush stroke
// interpolation, eraser drag, selection visual hooks, screen-to-world stability
// across resize and the toolbar without Pan. Headless via SimulationRunner +
// CommandQueue + UiPanel helpers; no SFML window.
struct Phase22_1ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase22_1BenchmarkResult
{
    std::string scenario;
    int agents = 0;
    int foods = 0;
    int obstacles = 0;
    int steps = 0;
    double totalMilliseconds = 0.0;
    double averageStepMicroseconds = 0.0;
    std::size_t commandsApplied = 0;
    std::size_t selectionSize = 0;
    std::string notes;
};

[[nodiscard]] Phase22_1ValidationSummary runPhase22_1Validation();
[[nodiscard]] std::vector<Phase22_1BenchmarkResult> runPhase22_1Microbenchmark();
} // namespace agentbiosim::systems
