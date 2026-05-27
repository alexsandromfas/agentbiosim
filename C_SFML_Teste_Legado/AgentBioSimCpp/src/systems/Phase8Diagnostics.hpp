#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
struct Phase8ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::size_t agentsTested = 0;
    std::string movementModesTested;
    std::string details;
};

struct Phase8BenchmarkResult
{
    int agents = 0;
    std::string movementMode;
    bool smoothLocomotion = false;
    double stepMilliseconds = 0.0;
    std::size_t agentsProcessed = 0;
    double maxSpeedObserved = 0.0;
    std::size_t wallCollisions = 0;
};

[[nodiscard]] Phase8ValidationSummary runPhase8Validation();
[[nodiscard]] std::vector<Phase8BenchmarkResult> runPhase8Microbenchmark();
} // namespace agentbiosim::systems
