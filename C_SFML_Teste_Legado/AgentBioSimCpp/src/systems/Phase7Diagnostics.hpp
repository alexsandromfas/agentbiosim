#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
struct Phase7ValidationSummary
{
    bool passed = false;
    int checks = 0;
    std::size_t foodsConsumed = 0;
    std::size_t deaths = 0;
    std::string details;
};

struct Phase7BenchmarkResult
{
    int agents = 0;
    int foods = 0;
    bool useSpatial = false;
    double stepMilliseconds = 0.0;
    std::size_t foodsConsumed = 0;
    std::size_t deaths = 0;
};

[[nodiscard]] Phase7ValidationSummary runPhase7Validation();
[[nodiscard]] std::vector<Phase7BenchmarkResult> runPhase7Microbenchmark();
} // namespace agentbiosim::systems
