#pragma once

#include "config/ParameterRegistry.hpp"
#include "perception/PerceptionResult.hpp"
#include "perception/RetinaConfig.hpp"
#include "perception/SceneQuery.hpp"
#include "perception/VisionDebug.hpp"
#include "perception/VisionStrategy.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/VisionConfig.hpp"
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

    // Microfase 32.5: when `genomes` is provided, the per-agent vision targeting
    // (seeFood/seeAgents/seePredators/seeObstacles/seeAll/seeThroughWalls) is read
    // from each agent's genome, so labels can perceive different things. Retina
    // GEOMETRY stays global (config.retina) since it sizes the neural input. When
    // `genomes` is nullptr the global config.retina flags apply to all agents
    // (legacy behavior, used by standalone perception selftests).
    [[nodiscard]] PerceptionResult computeInputs(const simulation::AgentStore& agents,
                                                  const simulation::FoodStore& foods,
                                                  simulation::SpatialHash* spatial,
                                                  const simulation::World& world,
                                                  const PerceptionConfig& config,
                                                  const PerceptionDebugRequest& debugRequest = {},
                                                  const simulation::ObstacleStore* obstacles = nullptr,
                                                  const simulation::GenomeStore* genomes = nullptr);

    [[nodiscard]] const PerceptionStats& lastStats() const noexcept;

private:
    PerceptionStats lastStats_{};
    std::vector<VisibleCandidate> candidateBuffer_;
    // Phase 32: reusable [0..N) index range for std::execution::par.
    std::vector<std::size_t> parallelIndices_;
    // Microfase 32.5: per-agent vision targeting, resolved once (serial) from each
    // agent's genome before the parallel loop. Read-only inside the loop, so it
    // stays bit-identical + parallel-safe. Empty => use the global config flags.
    std::vector<simulation::VisionConfig> perAgentVision_;

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
