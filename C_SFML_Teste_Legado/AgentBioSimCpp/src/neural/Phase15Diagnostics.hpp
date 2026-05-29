#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::neural
{
struct Phase15ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase15BenchmarkResult
{
    std::string scenario;
    std::string brainType;
    std::string hiddenLayers;
    std::size_t inputSize = 0;
    std::size_t outputSize = 0;
    std::size_t stateSize = 0;
    int repeats = 0;
    double totalMilliseconds = 0.0;
    double averageForwardMicroseconds = 0.0;
    double averageCloneMicroseconds = 0.0;
    double averageMutationMicroseconds = 0.0;
    double averageResetMicroseconds = 0.0;
    double memoryDecay = 0.0;
    double stateClip = 0.0;
    bool resetStateOnCopy = false;
};

[[nodiscard]] Phase15ValidationSummary runPhase15Validation();
[[nodiscard]] std::vector<Phase15BenchmarkResult> runPhase15Microbenchmark();
} // namespace agentbiosim::neural
