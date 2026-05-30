#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
struct Phase21ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase21BenchmarkResult
{
    std::string scenario;
    int agents = 0;
    int foods = 0;
    int obstacles = 0;
    int steps = 0;
    bool collisionEnabled = false;
    bool elasticityEnabled = false;
    bool viscosityEnabled = false;
    bool brownianEnabled = false;
    bool movableChunkEnabled = false;
    bool chunkCollisionEnabled = false;
    bool chunkAdhesionEnabled = false;
    double totalMilliseconds = 0.0;
    double averageStepMicroseconds = 0.0;
    double averagePerAgentMicroseconds = 0.0;
    std::size_t agentPairsTested = 0;
    std::size_t agentCollisionsResolved = 0;
    std::size_t agentFoodPairsTested = 0;
    std::size_t foodPushes = 0;
    std::size_t foodPairsTested = 0;
    std::size_t foodCollisionsResolved = 0;
    std::size_t adhesionsApplied = 0;
    std::size_t brownianApplied = 0;
    std::string notes;
};

[[nodiscard]] Phase21ValidationSummary runPhase21Validation();
[[nodiscard]] std::vector<Phase21BenchmarkResult> runPhase21Microbenchmark();
} // namespace agentbiosim::systems
