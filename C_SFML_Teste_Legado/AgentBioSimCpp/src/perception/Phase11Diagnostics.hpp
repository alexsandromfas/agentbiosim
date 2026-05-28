#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::perception
{
struct Phase11ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase11BenchmarkResult
{
    std::string scenario;
    std::string visionMode;
    std::size_t agents = 0;
    std::size_t retinaCount = 0;
    std::size_t eyeCount = 0;
    std::size_t channelCount = 0;
    std::size_t inputSize = 0;
    int repeats = 0;
    double totalMilliseconds = 0.0;
    double averagePerceptionMicroseconds = 0.0;
    double averageCandidatesPerAgent = 0.0;
    double averageHitsPerAgent = 0.0;
    bool usedSpatialHash = false;
    bool debugActive = false;
};

[[nodiscard]] Phase11ValidationSummary runPhase11Validation();
[[nodiscard]] std::vector<Phase11BenchmarkResult> runPhase11Microbenchmark();
} // namespace agentbiosim::perception
