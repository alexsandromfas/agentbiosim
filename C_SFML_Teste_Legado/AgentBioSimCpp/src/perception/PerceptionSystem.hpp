#pragma once

#include "config/ParameterRegistry.hpp"
#include "perception/PerceptionResult.hpp"
#include "perception/RetinaConfig.hpp"
#include "perception/SceneQuery.hpp"
#include "perception/VisionDebug.hpp"
#include "perception/VisionStrategy.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace agentbiosim::perception
{
struct PerceptionConfig
{
    RetinaConfig retina;
    // Phase 32: per-agent perception is independent (const world reads, output
    // into a disjoint slice, no RNG) — parallel execution is bit-identical.
    bool parallelEnabled = true;
};

struct PerceptionStats
{
    std::size_t agentsProcessed = 0;
    std::size_t totalCandidatesQueried = 0;
    std::size_t totalCandidatesAfterLimit = 0;
    double averageCandidatesPerAgent = 0.0;
    double averageCandidatesAfterLimit = 0.0;
    std::string visionMode;
    VisionMode visionModeEnum = VisionMode::Single;
    VisionMode requestedModeEnum = VisionMode::Single;
    std::size_t inputSize = 0;
    std::size_t channelCount = 0;
    std::size_t retinaCount = 0;
    std::size_t eyeCount = 0;
    std::size_t totalRayHits = 0;
    bool usedSpatialHash = false;
    bool fallbackMode = false;
    std::string fallbackReason;
    bool autoSectorActive = false;
    // Phase 20: occlusion + obstacle telemetry.
    std::size_t obstacleCandidates = 0;
    std::size_t occlusionChecks = 0;
    std::size_t occludedCandidates = 0;
};

struct PerceptionDebugRequest
{
    std::uint64_t agentId = 0;
    VisionDebugData* out = nullptr;
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
                                                  const PerceptionConfig& config,
                                                  const PerceptionDebugRequest& debugRequest = {},
                                                  const simulation::ObstacleStore* obstacles = nullptr);

    [[nodiscard]] const PerceptionStats& lastStats() const noexcept;

private:
    PerceptionStats lastStats_{};
    std::vector<VisibleCandidate> candidateBuffer_;
    // Phase 32: reusable [0..N) index range for std::execution::par.
    std::vector<std::size_t> parallelIndices_;

    struct RayCache
    {
        std::size_t retinaCount = 0;
        double halfFovRad = 0.0;
        std::vector<double> relCos;
        std::vector<double> relSin;
    };
    RayCache rayCache_{};

    void ensureRayCache(std::size_t retinaCount, double halfFovRad);
};
} // namespace agentbiosim::perception
