#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::perception
{
struct Phase12ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase12BenchmarkResult
{
    std::string scenario;
    std::string visionMode;
    std::string binsMode;
    std::string binsDistribution;
    std::string binsFalloff;
    std::string binsProjection;
    std::size_t agents = 0;
    std::size_t retinaCount = 0;
    std::size_t eyeCount = 0;
    std::size_t channelCount = 0;
    std::size_t inputSize = 0;
    int subdivisions = 1;
    int candidateLimit = 0;
    int repeats = 0;
    double totalMilliseconds = 0.0;
    double averagePerceptionMicroseconds = 0.0;
    double averageCandidatesPerAgent = 0.0;
    double averageCandidatesAfterLimit = 0.0;
    double averageHitsPerAgent = 0.0;
    bool usedSpatialHash = false;
    bool debugActive = false;
    bool autoSectorActive = false;
};

[[nodiscard]] Phase12ValidationSummary runPhase12Validation();
[[nodiscard]] std::vector<Phase12BenchmarkResult> runPhase12Microbenchmark();
} // namespace agentbiosim::perception
