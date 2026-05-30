#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
struct Phase22ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase22BenchmarkResult
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

[[nodiscard]] Phase22ValidationSummary runPhase22Validation();
[[nodiscard]] std::vector<Phase22BenchmarkResult> runPhase22Microbenchmark();
} // namespace agentbiosim::systems
