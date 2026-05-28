#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::neural
{
struct Phase9ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::size_t agentsTested = 0;
    std::size_t brainTypesRecognized = 0;
    std::string details;
};

struct Phase9BenchmarkResult
{
    std::string architecture;
    std::size_t inputSize = 0;
    std::size_t outputSize = 0;
    std::string hiddenLayers;
    int agents = 0;
    int forwards = 0;
    double totalMilliseconds = 0.0;
    double averageForwardMicroseconds = 0.0;
};

[[nodiscard]] Phase9ValidationSummary runPhase9Validation();
[[nodiscard]] std::vector<Phase9BenchmarkResult> runPhase9Microbenchmark();
} // namespace agentbiosim::neural
