#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/EntityTypes.hpp"
#include "simulation/FoodStore.hpp"
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

    [[nodiscard]] FoodSystemStats replenishToTarget(simulation::FoodStore& foods,
                                                     const simulation::World& world,
                                                     const FoodSystemConfig& config);

    // Trim excess food (removes the highest-indexed alive items beyond target,
    // up to `trimMaxPerStep`). Returns number of items removed.
    [[nodiscard]] std::size_t trimExcess(simulation::FoodStore& foods,
                                          const FoodSystemConfig& config);

    // Remove all food (instant + chunk). Returns previous food count.
    [[nodiscard]] std::size_t clearAll(simulation::FoodStore& foods);

    // Spawn a single instant food at a random valid position. Deterministic with seed.
    [[nodiscard]] simulation::EntityId spawnInstant(simulation::FoodStore& foods,
                                                     const simulation::World& world,
                                                     const FoodSystemConfig& config);

    // Spawn a single chunk cluster (multiple particles) at a random valid position.
    [[nodiscard]] std::uint32_t spawnCluster(simulation::FoodStore& foods,
                                              const simulation::World& world,
                                              const FoodSystemConfig& config);

    // Grow an existing cluster by adding particles near it (deterministic pick).
    // Returns the cluster id grown, or 0 if no existing cluster was found.
    std::uint32_t growExistingCluster(simulation::FoodStore& foods,
                                       const simulation::World& world,
                                       const FoodSystemConfig& config);

    // Grow existing chunk particles (refill remaining energy). Returns count grown.
    std::size_t growExistingParticles(simulation::FoodStore& foods,
                                       const FoodSystemConfig& config);

private:
    [[nodiscard]] simulation::Vec2 randomPointInsideWorld(const simulation::World& world,
                                                          double radius);

    std::mt19937_64 rng_{20260530ULL};
};
} // namespace agentbiosim::systems
