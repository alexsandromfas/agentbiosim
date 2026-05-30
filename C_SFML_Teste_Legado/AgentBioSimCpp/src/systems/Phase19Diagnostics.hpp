#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
struct Phase19ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase19BenchmarkResult
{
    std::string scenario;
    std::string foodMode;
    std::string replenishMode;
    int agents = 0;
    int foods = 0;
    int predators = 0;
    int steps = 0;
    double totalMilliseconds = 0.0;
    double averageStepMicroseconds = 0.0;
    double averagePerAgentMicroseconds = 0.0;
    std::size_t foodsConsumed = 0;
    std::size_t chunkBitesApplied = 0;
    std::size_t chunkParticlesDepleted = 0;
    std::size_t clustersCreated = 0;
    std::size_t clustersGrown = 0;
    std::size_t particlesGrown = 0;
    std::size_t trimmed = 0;
    std::size_t predationEvents = 0;
    double energyGainedByFood = 0.0;
    double biteSeconds = 0.0;
    std::string notes;
};

[[nodiscard]] Phase19ValidationSummary runPhase19Validation();
[[nodiscard]] std::vector<Phase19BenchmarkResult> runPhase19Microbenchmark();
} // namespace agentbiosim::systems
