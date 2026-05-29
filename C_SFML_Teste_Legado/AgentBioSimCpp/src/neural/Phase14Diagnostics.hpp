#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::neural
{
struct Phase14ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase14BenchmarkResult
{
    std::string scenario;
    std::string brainType;
    std::string hiddenLayers;
    std::size_t inputSize = 0;
    std::size_t outputSize = 0;
    std::size_t agents = 0;
    int repeats = 0;
    double totalMilliseconds = 0.0;
    double averageForwardMicroseconds = 0.0;
    double averageCloneMicroseconds = 0.0;
    double averageMutationMicroseconds = 0.0;
};

[[nodiscard]] Phase14ValidationSummary runPhase14Validation();
[[nodiscard]] std::vector<Phase14BenchmarkResult> runPhase14Microbenchmark();
} // namespace agentbiosim::neural
