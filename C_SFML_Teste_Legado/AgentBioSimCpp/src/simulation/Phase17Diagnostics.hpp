#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::simulation
{
struct Phase17ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase17BenchmarkResult
{
    std::string scenario;
    int speciesCount = 0;
    int agents = 0;
    std::string brainType;
    int repeats = 0;
    double totalMilliseconds = 0.0;
    double averageOperationMicroseconds = 0.0;
    double averagePerAgentMicroseconds = 0.0;
    std::string notes;
};

[[nodiscard]] Phase17ValidationSummary runPhase17Validation();
[[nodiscard]] std::vector<Phase17BenchmarkResult> runPhase17Microbenchmark();
} // namespace agentbiosim::simulation
