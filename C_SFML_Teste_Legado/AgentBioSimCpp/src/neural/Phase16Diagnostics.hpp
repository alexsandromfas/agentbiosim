#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::neural
{
struct Phase16ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase16BenchmarkResult
{
    std::string scenario;
    std::string brainType;
    std::string topology;
    std::size_t inputSize = 0;
    std::size_t outputSize = 0;
    std::size_t hiddenNodes = 0;
    std::size_t connections = 0;
    std::size_t enabledConnections = 0;
    std::size_t recurrentConnections = 0;
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

[[nodiscard]] Phase16ValidationSummary runPhase16Validation();
[[nodiscard]] std::vector<Phase16BenchmarkResult> runPhase16Microbenchmark();
} // namespace agentbiosim::neural
