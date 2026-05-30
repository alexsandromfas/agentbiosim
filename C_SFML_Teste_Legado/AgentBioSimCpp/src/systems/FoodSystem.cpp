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
    cfg.instantMinRadius = std::max(0.1, parameterDouble(parameters, "food_min_r", cfg.instantMinRadius));
    cfg.instantMaxRadius = std::max(cfg.instantMinRadius, parameterDouble(parameters, "food_max_r", cfg.instantMaxRadius));
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
                                                const FoodSystemConfig& config)
{
    std::uniform_real_distribution<double> radDist(config.instantMinRadius, config.instantMaxRadius);
    const double radius = radDist(rng_);
    const double energy = std::max(1.0e-9, radius * radius);
    simulation::FoodSpawn s;
    s.position = world.clampPosition(randomPointInsideWorld(world, radius), radius);
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
                                        const FoodSystemConfig& config)
{
    const std::uint32_t clusterId = foods.allocateClusterId();
    const double pr = std::max(0.5, config.particleRadius);
    const double cr = std::max(pr, config.clusterRadius);
    const double spacing = std::max(0.0, config.particleSpacing);
    const double stride = std::max(2.0 * pr, 2.0 * pr + spacing);

    // Cluster center; clamp by the cluster bounding radius so all particles stay
    // inside the world. Phase 21 will add per-particle clamping during placement.
    const simulation::Vec2 center = world.clampPosition(randomPointInsideWorld(world, cr), cr);

    const double area = 3.14159265358979323846 * cr * cr;
    const double cellArea = stride * stride;
    const int approxCount = static_cast<int>(std::max(1.0, std::round(area / std::max(1.0e-6, cellArea))));
    const int requestedCount = std::min(approxCount, std::max(1, config.target));

    int placed = 0;
    std::uniform_real_distribution<double> ang(0.0, kTwoPi);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    for (int attempt = 0; attempt < requestedCount * 4 && placed < requestedCount; ++attempt)
    {
        const double r = std::sqrt(uni(rng_)) * cr;
        const double a = ang(rng_);
        const simulation::Vec2 pos = world.clampPosition({center.x + std::cos(a) * r,
                                                           center.y + std::sin(a) * r}, pr);
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
                                                const FoodSystemConfig& config)
{
    // Pick smallest active clusterId deterministically.
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
            // Fallback documented in status doc: spawn a new cluster.
            return spawnCluster(foods, world, config);
        }
        const auto it = centersByCluster.begin();
        bestId = it->first;
        // Center of mass.
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
                                                const FoodSystemConfig& config)
{
    FoodSystemStats stats;
    if (config.target <= 0) return stats;

    if (config.mode == simulation::FoodKind::Instant)
    {
        // Phase 7 instant path: spawn one-by-one until count reaches target.
        // Avoid spawning more than `trimMaxPerStep` to bound work per step.
        const int maxPerStep = std::max(1, config.trimMaxPerStep);
        int spawned = 0;
        while (static_cast<int>(foods.size()) < config.target && spawned < maxPerStep)
        {
            static_cast<void>(spawnInstant(foods, world, config));
            ++spawned;
            ++stats.spawnedInstant;
        }
        return stats;
    }

    // Chunk mode. Replenish gating depends on the mode: grow_particles is a
    // maintenance pass (refill depleted energy) and always runs; the cluster
    // spawn/grow modes only fire when below target.
    switch (config.replenishMode)
    {
    case FoodReplenishMode::SpawnCluster:
    {
        if (static_cast<int>(foods.size()) >= config.target) return stats;
        const std::size_t before = foods.size();
        const auto cid = spawnCluster(foods, world, config);
        const std::size_t after = foods.size();
        stats.spawnedChunkParticles += (after - before);
        if (cid != 0 && after > before) ++stats.clustersCreated;
        break;
    }
    case FoodReplenishMode::GrowExisting:
    {
        if (static_cast<int>(foods.size()) >= config.target) return stats;
        const std::size_t before = foods.size();
        const auto cid = growExistingCluster(foods, world, config);
        const std::size_t after = foods.size();
        stats.spawnedChunkParticles += (after - before);
        if (cid != 0)
        {
            // If a fresh cluster was spawned via fallback, it counts as created.
            // Otherwise it counts as grown.
            if (before == 0U && after > 0U) ++stats.clustersCreated;
            else ++stats.clustersGrown;
        }
        break;
    }
    case FoodReplenishMode::GrowParticles:
    {
        const std::size_t grown = growExistingParticles(foods, config);
        stats.particlesGrown += grown;
        if (grown == 0U && foods.empty())
        {
            // Fallback: bootstrap with a cluster so future calls have something to grow.
            const std::size_t before = foods.size();
            static_cast<void>(spawnCluster(foods, world, config));
            const std::size_t after = foods.size();
            stats.spawnedChunkParticles += (after - before);
            if (after > before) ++stats.clustersCreated;
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
