#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
struct Phase20ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase20BenchmarkResult
{
    std::string scenario;
    int agents = 0;
    int foods = 0;
    int obstacles = 0;
    int steps = 0;
    std::string visionMode;
    int retinaCount = 0;
    int eyeCount = 0;
    std::string channels;
    bool seeObstacles = false;
    bool seeThroughWalls = true;
    double totalMilliseconds = 0.0;
    double averageStepMicroseconds = 0.0;
    double averagePerAgentMicroseconds = 0.0;
    std::size_t obstacleBlocks = 0;
    std::size_t spawnsRejected = 0;
    std::size_t occlusionChecks = 0;
    std::size_t occludedCandidates = 0;
    std::size_t obstacleCandidates = 0;
    std::size_t predationEvents = 0;
    std::size_t foodsConsumed = 0;
    std::string notes;
};

[[nodiscard]] Phase20ValidationSummary runPhase20Validation();
[[nodiscard]] std::vector<Phase20BenchmarkResult> runPhase20Microbenchmark();
} // namespace agentbiosim::systems
