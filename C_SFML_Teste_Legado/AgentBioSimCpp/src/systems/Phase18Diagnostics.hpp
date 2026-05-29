#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
struct Phase18ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

struct Phase18BenchmarkResult
{
    std::string scenario;
    int agents = 0;
    int predators = 0;
    int foods = 0;
    int steps = 0;
    double totalMilliseconds = 0.0;
    double averageStepMicroseconds = 0.0;
    double averagePerAgentMicroseconds = 0.0;
    std::size_t foodsConsumed = 0;
    std::size_t predationEvents = 0;
    std::size_t corpsesToFoodSpawned = 0;
    double energyGainedByFood = 0.0;
    double energyGainedByPredation = 0.0;
    std::string notes;
};

[[nodiscard]] Phase18ValidationSummary runPhase18Validation();
[[nodiscard]] std::vector<Phase18BenchmarkResult> runPhase18Microbenchmark();
} // namespace agentbiosim::systems
