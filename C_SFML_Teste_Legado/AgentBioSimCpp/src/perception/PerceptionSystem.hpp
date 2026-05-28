#pragma once

#include "config/ParameterRegistry.hpp"
#include "perception/RetinaConfig.hpp"
#include "perception/SceneQuery.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/World.hpp"

#include "perception/PerceptionResult.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::perception
{
struct PerceptionConfig
{
    RetinaConfig retina;
};

struct PerceptionStats
{
    std::size_t agentsProcessed = 0;
    std::size_t totalCandidatesQueried = 0;
    double averageCandidatesPerAgent = 0.0;
    std::string visionMode;
    std::size_t inputSize = 0;
    std::size_t channelCount = 0;
    std::size_t retinaCount = 0;
    std::size_t eyeCount = 0;
    bool usedSpatialHash = false;
};

class PerceptionSystem
{
public:
    [[nodiscard]] static PerceptionConfig fromRegistry(const config::ParameterRegistry& registry,
                                                       const std::string& speciesPrefix);

    [[nodiscard]] PerceptionResult computeInputs(const simulation::AgentStore& agents,
                                                  const simulation::FoodStore& foods,
                                                  simulation::SpatialHash* spatial,
                                                  const simulation::World& world,
                                                  const PerceptionConfig& config);

    [[nodiscard]] const PerceptionStats& lastStats() const noexcept;

private:
    PerceptionStats lastStats_{};
    std::vector<VisibleCandidate> candidateBuffer_;
};
} // namespace agentbiosim::perception
