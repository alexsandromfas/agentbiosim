#include "systems/FoodSystem.hpp"

#include "config/ParameterHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
constexpr double kTwoPi = 6.28318530717958647692;

simulation::ColorRgb toEntityColor(const config::ColorRgb color)
{
    return {static_cast<std::uint8_t>(std::clamp(color.r, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(color.g, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(color.b, 0, 255))};
}

simulation::FoodKind parseFoodKind(const std::string& value)
{
    return value == "chunk" ? simulation::FoodKind::Chunk : simulation::FoodKind::Instant;
}
} // namespace

FoodReplenishMode parseReplenishMode(const std::string& value) noexcept
{
    if (value == "grow_existing") return FoodReplenishMode::GrowExisting;
    if (value == "grow_particles") return FoodReplenishMode::GrowParticles;
    return FoodReplenishMode::SpawnCluster;
}

const char* replenishModeName(const FoodReplenishMode mode) noexcept
{
    switch (mode)
    {
    case FoodReplenishMode::GrowExisting: return "grow_existing";
    case FoodReplenishMode::GrowParticles: return "grow_particles";
    case FoodReplenishMode::SpawnCluster:
    default: return "spawn_cluster";
    }
}

FoodSystemConfig FoodSystem::fromRegistry(const config::ParameterRegistry& parameters)
{
    using config::parameterBool;
    using config::parameterColor;
    using config::parameterDouble;
    using config::parameterInt;
    using config::parameterString;

    FoodSystemConfig cfg;
    cfg.mode = parseFoodKind(parameterString(parameters, "food_mode", "instant"));
    cfg.target = std::max(0, parameterInt(parameters, "food_target", cfg.target));
    cfg.biteSeconds = std::max(0.0, parameterDouble(parameters, "food_bite_seconds", cfg.biteSeconds));
    cfg.particleRadius = std::max(0.1, parameterDouble(parameters, "food_piece_particle_radius", cfg.particleRadius));
    cfg.clusterRadius = std::max(0.1, parameterDouble(parameters, "food_piece_cluster_radius", cfg.clusterRadius));
    cfg.particleSpacing = std::max(0.0, parameterDouble(parameters, "food_piece_particle_spacing", cfg.particleSpacing));
    cfg.replenishMode = parseReplenishMode(parameterString(parameters, "food_piece_replenish_mode", "spawn_cluster"));
    // Food fix: a SINGLE food particle radius now drives both instant and chunk
    // food (food_piece_particle_radius). The old food_min_r/food_max_r spawn-radius
    // range was removed from the UI; instant pieces use one uniform radius.
    cfg.instantMinRadius = cfg.particleRadius;
    cfg.instantMaxRadius = cfg.particleRadius;
    cfg.color = toEntityColor(parameterColor(parameters, "food_color", {220, 30, 30}));
    cfg.trimExcessEnabled = parameterBool(parameters, "food_trim_excess_enabled", cfg.trimExcessEnabled);
    cfg.trimMaxPerStep = std::max(0, parameterInt(parameters, "food_trim_max_per_step", cfg.trimMaxPerStep));
    const int seedParam = parameterInt(parameters, "random_seed", -1);
    cfg.seed = seedParam >= 0 ? static_cast<std::uint64_t>(seedParam) : cfg.seed;
    return cfg;
}

void FoodSystem::reseed(const std::uint64_t seed)
{
    rng_.seed(seed);
}

simulation::Vec2 FoodSystem::randomPointInsideWorld(const simulation::World& world,
                                                     const double radius)
{
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    if (world.shape() == simulation::WorldShape::Circular)
    {
        const double a = unit(rng_) * kTwoPi;
        const double r = std::sqrt(unit(rng_)) * std::max(0.0, world.radius() - radius);
        const simulation::Vec2 c = world.center();
        return {c.x + std::cos(a) * r, c.y + std::sin(a) * r};
    }
    const double minX = radius;
    const double maxX = std::max(radius, world.width() - radius);
    const double minY = radius;
    const double maxY = std::max(radius, world.height() - radius);
    std::uniform_real_distribution<double> ux(minX, maxX);
    std::uniform_real_distribution<double> uy(minY, maxY);
    return {ux(rng_), uy(rng_)};
}

simulation::EntityId FoodSystem::spawnInstant(simulation::FoodStore& foods,
                                                const simulation::World& world,
                                                const FoodSystemConfig& config,
                                                const simulation::ObstacleStore* obstacles)
{
    std::uniform_real_distribution<double> radDist(config.instantMinRadius, config.instantMaxRadius);
    const double radius = radDist(rng_);
    const double energy = std::max(1.0e-9, radius * radius);
    // Phase 20: reject positions inside obstacles, retry up to 16 times.
    simulation::Vec2 pos = world.clampPosition(randomPointInsideWorld(world, radius), radius);
    if (obstacles != nullptr && !obstacles->empty())
    {
        constexpr int kMaxAttempts = 16;
        int attempts = 0;
        while (obstacles->overlapsCircle(pos, radius) && attempts < kMaxAttempts)
        {
            pos = world.clampPosition(randomPointInsideWorld(world, radius), radius);
            ++attempts;
        }
        if (obstacles->overlapsCircle(pos, radius))
        {
            return {0};
        }
    }
    simulation::FoodSpawn s;
    s.position = pos;
    s.radius = radius;
    s.energy = energy;
    s.initialEnergy = energy;
    s.color = config.color;
    s.kind = simulation::FoodKind::Instant;
    s.clusterId = 0;
    return foods.createFood(s);
}

std::uint32_t FoodSystem::spawnCluster(simulation::FoodStore& foods,
                                        const simulation::World& world,
                                        const FoodSystemConfig& config,
                                        const simulation::ObstacleStore* obstacles)
{
    const double pr = std::max(0.5, config.particleRadius);
    const double cr = std::max(pr, config.clusterRadius);
    const double spacing = std::max(0.0, config.particleSpacing);
    const double stride = std::max(2.0 * pr, 2.0 * pr + spacing);

    // Cluster center; clamp by the cluster bounding radius so all particles stay
    // inside the world. If obstacles cover the candidate center, retry.
    simulation::Vec2 center = world.clampPosition(randomPointInsideWorld(world, cr), cr);
    if (obstacles != nullptr && !obstacles->empty())
    {
        constexpr int kMaxAttempts = 16;
        int attempts = 0;
        while (obstacles->containsPoint(center) && attempts < kMaxAttempts)
        {
            center = world.clampPosition(randomPointInsideWorld(world, cr), cr);
            ++attempts;
        }
        if (obstacles->containsPoint(center)) return 0U;
    }

    const std::uint32_t clusterId = foods.allocateClusterId();
    const double area = 3.14159265358979323846 * cr * cr;
    const double cellArea = stride * stride;
    const int approxCount = static_cast<int>(std::max(1.0, std::round(area / std::max(1.0e-6, cellArea))));
    const int requestedCount = std::min(approxCount, std::max(1, config.target));

    int placed = 0;
    std::uniform_real_distribution<double> ang(0.0, kTwoPi);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    // Irregular blob outline: modulate the cluster radius per angle with a couple
    // of random harmonics (fixed per cluster) so blobs look organic and lumpy
    // instead of perfect discs.
    const double ph1 = ang(rng_);
    const double ph2 = ang(rng_);
    const double k1 = static_cast<double>(2 + (rng_() % 3U));  // 2..4 lobes
    const double k2 = static_cast<double>(3 + (rng_() % 4U));  // 3..6 finer lobes
    const auto blob = [&](const double a) {
        const double f = 1.0 + 0.28 * std::sin(k1 * a + ph1) + 0.16 * std::sin(k2 * a + ph2);
        return std::clamp(f, 0.25, 1.35);
    };
    for (int attempt = 0; attempt < requestedCount * 4 && placed < requestedCount; ++attempt)
    {
        const double a = ang(rng_);
        const double r = std::sqrt(uni(rng_)) * cr * blob(a);
        const simulation::Vec2 pos = world.clampPosition({center.x + std::cos(a) * r,
                                                           center.y + std::sin(a) * r}, pr);
        // Per-particle obstacle rejection. Particles that fall inside an obstacle
        // are silently dropped (the cluster ends up with fewer particles).
        if (obstacles != nullptr && !obstacles->empty() && obstacles->overlapsCircle(pos, pr))
        {
            continue;
        }
        simulation::FoodSpawn s;
        s.position = pos;
        s.radius = pr;
        s.energy = pr * pr;
        s.initialEnergy = pr * pr;
        s.color = config.color;
        s.kind = simulation::FoodKind::Chunk;
        s.clusterId = clusterId;
        static_cast<void>(foods.createFood(s));
        ++placed;
    }
    return clusterId;
}

std::uint32_t FoodSystem::growExistingCluster(simulation::FoodStore& foods,
                                                const simulation::World& world,
                                                const FoodSystemConfig& config,
                                                const simulation::ObstacleStore* obstacles)
{
    // Pick a RANDOM active cluster (not always the smallest id). Always growing the
    // same cluster was what made the whole field collapse into one giant blob.
    std::uint32_t bestId = 0;
    simulation::Vec2 bestCenter{0.0, 0.0};
    {
        std::map<std::uint32_t, std::vector<simulation::Vec2>> centersByCluster;
        for (std::size_t i = 0; i < foods.size(); ++i)
        {
            if (!foods.aliveAt(i)) continue;
            if (foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
            const std::uint32_t cid = foods.clusterIdAt(i);
            if (cid == 0) continue;
            centersByCluster[cid].push_back(foods.positionAt(i));
        }
        if (centersByCluster.empty())
        {
            // No cluster to grow yet — seed one.
            return spawnCluster(foods, world, config, obstacles);
        }
        std::uniform_int_distribution<std::size_t> pick(0, centersByCluster.size() - 1U);
        auto it = centersByCluster.begin();
        std::advance(it, pick(rng_));
        bestId = it->first;
        // Center of mass of the chosen cluster.
        double cx = 0.0, cy = 0.0;
        for (const auto& p : it->second) { cx += p.x; cy += p.y; }
        cx /= static_cast<double>(it->second.size());
        cy /= static_cast<double>(it->second.size());
        bestCenter = {cx, cy};
    }

    const double pr = std::max(0.5, config.particleRadius);
    const double cr = std::max(pr, config.clusterRadius);
    const double spacing = std::max(0.0, config.particleSpacing);
    const double stride = std::max(2.0 * pr, 2.0 * pr + spacing);
    const double targetCount =
        std::max(1.0, std::round(3.14159265358979323846 * cr * cr / (stride * stride)));
    constexpr int kPerCallCap = 8;
    const int toAdd = std::min(kPerCallCap, static_cast<int>(targetCount));

    std::uniform_real_distribution<double> ang(0.0, kTwoPi);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    int added = 0;
    for (int attempt = 0; attempt < toAdd * 4 && added < toAdd; ++attempt)
    {
        const double r = std::sqrt(uni(rng_)) * cr;
        const double a = ang(rng_);
        const simulation::Vec2 pos = world.clampPosition({bestCenter.x + std::cos(a) * r,
                                                            bestCenter.y + std::sin(a) * r}, pr);
        if (obstacles != nullptr && !obstacles->empty() && obstacles->overlapsCircle(pos, pr))
        {
            continue;
        }
        simulation::FoodSpawn s;
        s.position = pos;
        s.radius = pr;
        s.energy = pr * pr;
        s.initialEnergy = pr * pr;
        s.color = config.color;
        s.kind = simulation::FoodKind::Chunk;
        s.clusterId = bestId;
        static_cast<void>(foods.createFood(s));
        ++added;
    }
    return bestId;
}

std::size_t FoodSystem::growExistingParticles(simulation::FoodStore& foods,
                                                const FoodSystemConfig& config)
{
    std::size_t grown = 0;
    const double initialFraction = 1.0;
    // Refill partial chunks back to initial energy. Bounded by particle count.
    for (std::size_t i = 0; i < foods.size(); ++i)
    {
        if (!foods.aliveAt(i)) continue;
        if (foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
        const double e = foods.energyAt(i);
        const double e0 = foods.initialEnergyAt(i) * initialFraction;
        if (e < e0 - 1.0e-9)
        {
            foods.setEnergyAt(i, e0);
            ++grown;
        }
    }
    static_cast<void>(config);
    return grown;
}

FoodSystemStats FoodSystem::replenishToTarget(simulation::FoodStore& foods,
                                                const simulation::World& world,
                                                const FoodSystemConfig& config,
                                                const simulation::ObstacleStore* obstacles)
{
    FoodSystemStats stats;
    if (config.target <= 0) return stats;

    if (config.mode == simulation::FoodKind::Instant)
    {
        // Fill the FULL deficit toward the target each replenish call. The old code
        // capped this at `trim_max_per_step` (default 5) per step, so under heavy
        // consumption the food count stalled far below large targets (e.g. ~700 vs
        // a 20000 target) and that, in turn, capped the population. The per-call
        // work is bounded by the target (one-time fill) and then by consumption
        // (steady state); the REPLENISH RATE is governed by the caller's interval
        // (SimulationRunner gates this by food_replenish_interval).
        int spawned = 0;
        int rejected = 0;
        while (static_cast<int>(foods.size()) < config.target)
        {
            const auto id = spawnInstant(foods, world, config, obstacles);
            if (!id.isValid())
            {
                // Position rejected (obstacle); allow a few retries then stop to
                // avoid an infinite loop when the world is too crowded.
                ++stats.spawnsRejectedByObstacle;
                if (++rejected >= 32) break;
                continue;
            }
            ++spawned;
            ++stats.spawnedInstant;
        }
        return stats;
    }

    // Chunk modes build toward the target in CLUSTER-sized increments. A guard
    // caps cluster ops per call so a big target fills over a few replenish ticks
    // (the field grows as distinct blobs) instead of dumping everything at once.
    constexpr int kMaxClusterOpsPerCall = 12;
    int ops = 0;
    switch (config.replenishMode)
    {
    case FoodReplenishMode::SpawnCluster:
    {
        // New irregular clusters until the target is reached.
        while (static_cast<int>(foods.size()) < config.target && ops < kMaxClusterOpsPerCall)
        {
            const std::size_t before = foods.size();
            const auto cid = spawnCluster(foods, world, config, obstacles);
            const std::size_t after = foods.size();
            stats.spawnedChunkParticles += (after - before);
            if (after == before) break;  // could not place (obstacles) — avoid spin
            if (cid != 0) ++stats.clustersCreated;
            ++ops;
        }
        break;
    }
    case FoodReplenishMode::GrowExisting:
    {
        // Grow EXISTING clusters toward the target, but ~1 in 5 ops seeds a NEW
        // cluster so the field keeps gaining blobs instead of collapsing into a
        // single ever-growing one (the previous behavior always grew the same
        // smallest-id cluster).
        std::uniform_real_distribution<double> roll(0.0, 1.0);
        while (static_cast<int>(foods.size()) < config.target && ops < kMaxClusterOpsPerCall)
        {
            const std::size_t before = foods.size();
            std::uint32_t cid = 0;
            if (roll(rng_) < 0.2)
            {
                cid = spawnCluster(foods, world, config, obstacles);
                if (cid != 0 && foods.size() > before) ++stats.clustersCreated;
            }
            else
            {
                cid = growExistingCluster(foods, world, config, obstacles);
                if (cid != 0 && foods.size() > before) ++stats.clustersGrown;
            }
            const std::size_t after = foods.size();
            stats.spawnedChunkParticles += (after - before);
            if (after == before) break;
            ++ops;
        }
        break;
    }
    case FoodReplenishMode::GrowParticles:
    {
        // Maintenance: refill depleted particles, then top the field up with new
        // clusters toward the target.
        const std::size_t grown = growExistingParticles(foods, config);
        stats.particlesGrown += grown;
        while (static_cast<int>(foods.size()) < config.target && ops < kMaxClusterOpsPerCall)
        {
            const std::size_t before = foods.size();
            const auto cid = spawnCluster(foods, world, config, obstacles);
            if (foods.size() == before) break;
            if (cid != 0) ++stats.clustersCreated;
            stats.spawnedChunkParticles += (foods.size() - before);
            ++ops;
        }
        break;
    }
    }
    return stats;
}

std::size_t FoodSystem::trimExcess(simulation::FoodStore& foods, const FoodSystemConfig& config)
{
    if (!config.trimExcessEnabled || config.target <= 0) return 0;
    if (static_cast<int>(foods.size()) <= config.target) return 0;
    const int over = static_cast<int>(foods.size()) - config.target;
    const int maxRemove = std::min(over, std::max(0, config.trimMaxPerStep));
    std::size_t removed = 0;
    // Trim deterministically: remove highest-indexed entries first (newest).
    while (removed < static_cast<std::size_t>(maxRemove) && !foods.empty())
    {
        const std::size_t idx = foods.size() - 1U;
        const simulation::EntityId id = foods.idAt(idx);
        if (foods.removeFood(id))
        {
            ++removed;
        }
        else
        {
            break;
        }
    }
    return removed;
}

std::size_t FoodSystem::clearAll(simulation::FoodStore& foods)
{
    const std::size_t before = foods.size();
    foods.clear();
    return before;
}
} // namespace agentbiosim::systems
