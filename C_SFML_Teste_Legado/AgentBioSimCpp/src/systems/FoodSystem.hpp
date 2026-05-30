#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/EntityTypes.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

namespace agentbiosim::systems
{
enum class FoodReplenishMode : std::uint8_t
{
    SpawnCluster = 0,
    GrowExisting = 1,
    GrowParticles = 2
};

[[nodiscard]] FoodReplenishMode parseReplenishMode(const std::string& value) noexcept;
[[nodiscard]] const char* replenishModeName(FoodReplenishMode mode) noexcept;

struct FoodSystemConfig
{
    simulation::FoodKind mode = simulation::FoodKind::Instant;
    int target = 50;
    // Phase 19: chunk food parameters. Used only when mode == Chunk.
    double biteSeconds = 6.0;
    double particleRadius = 5.0;
    double clusterRadius = 36.0;
    double particleSpacing = 0.0;
    FoodReplenishMode replenishMode = FoodReplenishMode::SpawnCluster;
    // Phase 7 instant parameters.
    double instantMinRadius = 4.5;
    double instantMaxRadius = 5.0;
    simulation::ColorRgb color{220, 30, 30};
    // Phase 19: target/trim.
    bool trimExcessEnabled = true;
    int trimMaxPerStep = 5;
    std::uint64_t seed = 20260530ULL;
};

struct FoodSystemStats
{
    std::size_t spawnedInstant = 0;
    std::size_t spawnedChunkParticles = 0;
    std::size_t clustersCreated = 0;
    std::size_t clustersGrown = 0;
    std::size_t particlesGrown = 0;
    std::size_t trimmed = 0;
    // Phase 20: counts spawn attempts rejected because the candidate position was
    // inside an obstacle. Useful for diagnostics and UI feedback.
    std::size_t spawnsRejectedByObstacle = 0;
};

// FoodSystem manages food spawn/replenishment/trim. It is headless (no SFML, no UI).
// `replenishToTarget` is the primary entry point called once per step (or per
// replenish interval). It spawns instant food or chunk clusters depending on
// the configured mode and replenishment strategy.
class FoodSystem
{
public:
    [[nodiscard]] static FoodSystemConfig fromRegistry(const config::ParameterRegistry& parameters);

    void reseed(std::uint64_t seed);

    // Phase 20: optional `obstacles` rejects positions that fall inside an obstacle.
    [[nodiscard]] FoodSystemStats replenishToTarget(simulation::FoodStore& foods,
                                                     const simulation::World& world,
                                                     const FoodSystemConfig& config,
                                                     const simulation::ObstacleStore* obstacles = nullptr);

    // Trim excess food (removes the highest-indexed alive items beyond target,
    // up to `trimMaxPerStep`). Returns number of items removed.
    [[nodiscard]] std::size_t trimExcess(simulation::FoodStore& foods,
                                          const FoodSystemConfig& config);

    // Remove all food (instant + chunk). Returns previous food count.
    [[nodiscard]] std::size_t clearAll(simulation::FoodStore& foods);

    // Spawn a single instant food at a random valid position. Deterministic with seed.
    // Returns kInvalidEntityId equivalent when no valid position found.
    [[nodiscard]] simulation::EntityId spawnInstant(simulation::FoodStore& foods,
                                                     const simulation::World& world,
                                                     const FoodSystemConfig& config,
                                                     const simulation::ObstacleStore* obstacles = nullptr);

    // Spawn a single chunk cluster (multiple particles) at a random valid position.
    // Returns the new cluster id, or 0 if no valid center was found.
    [[nodiscard]] std::uint32_t spawnCluster(simulation::FoodStore& foods,
                                              const simulation::World& world,
                                              const FoodSystemConfig& config,
                                              const simulation::ObstacleStore* obstacles = nullptr);

    // Grow an existing cluster by adding particles near it (deterministic pick).
    // Returns the cluster id grown, or 0 if no existing cluster was found.
    std::uint32_t growExistingCluster(simulation::FoodStore& foods,
                                       const simulation::World& world,
                                       const FoodSystemConfig& config,
                                       const simulation::ObstacleStore* obstacles = nullptr);

    // Grow existing chunk particles (refill remaining energy). Returns count grown.
    std::size_t growExistingParticles(simulation::FoodStore& foods,
                                       const FoodSystemConfig& config);

private:
    [[nodiscard]] simulation::Vec2 randomPointInsideWorld(const simulation::World& world,
                                                          double radius);

    std::mt19937_64 rng_{20260530ULL};
};
} // namespace agentbiosim::systems
