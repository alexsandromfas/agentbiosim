#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
struct Phase13ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase13BenchmarkResult
{
    std::string scenario;
    std::size_t initialAgents = 0;
    std::size_t finalAgents = 0;
    std::size_t totalBirths = 0;
    int steps = 0;
    int repeats = 0;
    double totalMilliseconds = 0.0;
    double averageStepMicroseconds = 0.0;
    double microsecondsPerBirth = 0.0;
    bool mutationEnabled = false;
    std::string hiddenLayers;
};

[[nodiscard]] Phase13ValidationSummary runPhase13Validation();
[[nodiscard]] std::vector<Phase13BenchmarkResult> runPhase13Microbenchmark();
} // namespace agentbiosim::systems
