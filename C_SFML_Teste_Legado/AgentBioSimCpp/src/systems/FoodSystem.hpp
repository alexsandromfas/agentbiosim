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
#include <vector>

namespace agentbiosim::systems
{
// LEGACY (test scaffolding only): the live runtime no longer chooses a replenish mode —
// chunk food uses a single growth strategy (see replenishToTarget). This enum + the
// parse/name helpers remain only so the Phase 19 food selftests/benchmark still build.
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
    // Chunk food parameters (used only when mode == Chunk).
    double biteSeconds = 6.0;
    double particleRadius = 5.0;
    double clusterRadius = 36.0; // the chunk radius: a chunk never grows past this
    // Phase 7 instant parameters.
    double instantMinRadius = 4.5;
    double instantMaxRadius = 5.0;
    simulation::ColorRgb color{220, 30, 30};
    // target/trim.
    bool trimExcessEnabled = true;
    int trimMaxPerStep = 5;
    std::uint64_t seed = 20260530ULL;
    // LEGACY (no longer read from the registry / not used by the live replenish; kept
    // for the food selftests). Particles are always glued; replenish is always growth.
    double particleSpacing = 0.0;
    FoodReplenishMode replenishMode = FoodReplenishMode::SpawnCluster;
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

    // Spawn one chunk particle at world position `pos`, tagged with `clusterId`. The
    // single-particle primitive used by chunk replenishment (and a future scavenger /
    // plant producer can reuse it to inject food anywhere). Returns the new id.
    [[nodiscard]] simulation::EntityId spawnChunkParticleAt(simulation::FoodStore& foods,
                                                            simulation::Vec2 pos,
                                                            const FoodSystemConfig& config,
                                                            std::uint32_t clusterId);

    // Spawn one whole chunk (glued particles packed within clusterRadius, no spacing,
    // no attraction) at a random valid centre. Returns the new cluster id, or 0 if no
    // valid centre was found. Bulk placement primitive (used for the initial fill).
    [[nodiscard]] std::uint32_t spawnCluster(simulation::FoodStore& foods,
                                             const simulation::World& world,
                                             const FoodSystemConfig& config,
                                             const simulation::ObstacleStore* obstacles = nullptr);

    // LEGACY (test scaffolding only). growExistingCluster now just spawns a new bounded
    // cluster; growExistingParticles is a no-op (chunk particles are eaten whole, not
    // nibbled). Kept so the Phase 19/20 selftests build.
    std::uint32_t growExistingCluster(simulation::FoodStore& foods, const simulation::World& world,
                                       const FoodSystemConfig& config,
                                       const simulation::ObstacleStore* obstacles = nullptr);
    std::size_t growExistingParticles(simulation::FoodStore& foods, const FoodSystemConfig& config);

private:
    [[nodiscard]] simulation::Vec2 randomPointInsideWorld(const simulation::World& world,
                                                          double radius);
    // Chunk mode keeps a fixed set of chunk centres ("sites"), count = target/capacity
    // (capacity = how many particles of `particleRadius` fill the `clusterRadius`).
    // Replenishment drops each missing particle into a site (within clusterRadius), in
    // round-robin — so chunks stay bounded by the radius, never fragment, and the work
    // is O(deficit) per step instead of scanning all food. Regenerated when the needed
    // count changes or after reseed/clear.
    void ensureChunkSites(const simulation::World& world, const FoodSystemConfig& config);

    std::mt19937_64 rng_{20260530ULL};
    std::vector<simulation::Vec2> chunkSites_;
    std::size_t chunkCursor_ = 0;
};
} // namespace agentbiosim::systems
