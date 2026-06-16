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
    cfg.chunkRoaming = parameterString(parameters, "food_chunk_mode", "fixed") == "roaming";
    cfg.target = std::max(0, parameterInt(parameters, "food_target", cfg.target));
    cfg.biteSeconds = std::max(0.0, parameterDouble(parameters, "food_bite_seconds", cfg.biteSeconds));
    cfg.particleRadius = std::max(0.1, parameterDouble(parameters, "food_piece_particle_radius", cfg.particleRadius));
    cfg.clusterRadius = std::max(0.1, parameterDouble(parameters, "food_piece_cluster_radius", cfg.clusterRadius));
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
    chunkSites_.clear();
    chunkCursor_ = 0;
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

simulation::EntityId FoodSystem::spawnChunkParticleAt(simulation::FoodStore& foods,
                                                      const simulation::Vec2 pos,
                                                      const FoodSystemConfig& config,
                                                      const std::uint32_t clusterId)
{
    const double pr = std::max(0.1, config.particleRadius);
    const double e = std::max(1.0e-9, pr * pr);
    simulation::FoodSpawn s;
    s.position = pos;
    s.radius = pr;
    s.energy = e;
    s.initialEnergy = e;
    s.color = config.color;
    s.kind = simulation::FoodKind::Chunk;
    s.clusterId = clusterId;
    return foods.createFood(s);
}

std::uint32_t FoodSystem::spawnCluster(simulation::FoodStore& foods, const simulation::World& world,
                                        const FoodSystemConfig& config,
                                        const simulation::ObstacleStore* obstacles)
{
    const double pr = std::max(0.1, config.particleRadius);
    const double cr = std::max(pr, config.clusterRadius);
    simulation::Vec2 center = world.clampPosition(randomPointInsideWorld(world, cr), cr);
    if (obstacles != nullptr && !obstacles->empty())
    {
        int attempts = 0;
        while (obstacles->containsPoint(center) && attempts < 16)
        {
            center = world.clampPosition(randomPointInsideWorld(world, cr), cr);
            ++attempts;
        }
        if (obstacles->containsPoint(center)) return 0U;
    }
    const std::uint32_t clusterId = foods.allocateClusterId();
    // Glued packing: particle count ~ capacity (area ratio); particles placed within
    // the cluster radius so the chunk is bounded by it (never larger).
    const double capacity = std::max(1.0, (cr * cr) / (pr * pr));
    const int count = std::min(static_cast<int>(capacity), std::max(1, config.target));
    std::uniform_real_distribution<double> ang(0.0, kTwoPi);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    for (int i = 0; i < count; ++i)
    {
        const double r = std::sqrt(uni(rng_)) * cr;
        const double a = ang(rng_);
        const simulation::Vec2 pos =
            world.clampPosition({center.x + std::cos(a) * r, center.y + std::sin(a) * r}, pr);
        if (obstacles != nullptr && !obstacles->empty() && obstacles->overlapsCircle(pos, pr))
        {
            continue;
        }
        static_cast<void>(spawnChunkParticleAt(foods, pos, config, clusterId));
    }
    return clusterId;
}

std::uint32_t FoodSystem::growExistingCluster(simulation::FoodStore& foods,
                                                const simulation::World& world,
                                                const FoodSystemConfig& config,
                                                const simulation::ObstacleStore* obstacles)
{
    // LEGACY shim: just place a new bounded cluster (the old "grow near existing" scan
    // was the perf cost; the live replenish no longer uses this path).
    return spawnCluster(foods, world, config, obstacles);
}

std::size_t FoodSystem::growExistingParticles(simulation::FoodStore& foods,
                                                const FoodSystemConfig& config)
{
    static_cast<void>(foods);
    static_cast<void>(config);
    return 0U; // chunk particles are eaten whole; nothing to refill
}

void FoodSystem::ensureChunkSites(const simulation::World& world, const FoodSystemConfig& config)
{
    const double pr = std::max(0.1, config.particleRadius);
    const double cr = std::max(pr, config.clusterRadius);
    // Capacity ~ how many particles of radius pr pack into a chunk of radius cr (area
    // ratio). The number of chunk sites = ceil(target / capacity), so the chunk radius
    // controls chunk SIZE and the count follows from the food target.
    const double capacity = std::max(1.0, (cr * cr) / (pr * pr));
    const int needed =
        std::max(1, static_cast<int>(std::ceil(static_cast<double>(config.target) / capacity)));
    if (static_cast<int>(chunkSites_.size()) == needed) return;
    chunkSites_.clear();
    chunkSites_.reserve(static_cast<std::size_t>(needed));
    for (int i = 0; i < needed; ++i)
    {
        chunkSites_.push_back(world.clampPosition(randomPointInsideWorld(world, cr), cr));
    }
    chunkCursor_ = 0;
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

    // Chunk mode — two behaviours selected by food_chunk_mode:
    // ROAMING ("itinerante"): fill the field UP TO the target every step (so the food
    // count tracks the target, exactly like the fixed mode), BUT route every refilled
    // particle into a BRAND-NEW chunk at a FRESH random centre instead of topping up
    // fixed sites. So the food an organism just ate reappears ELSEWHERE, not where it was
    // camping — the field keeps ~target particles but they relocate. O(deficit) per step.
    // Old piles are never refilled: they shrink as eaten and vanish; fresh piles appear
    // at new spots. New chunks obey particleRadius + clusterRadius. Deterministic (rng_).
    if (config.chunkRoaming)
    {
        const double prR = std::max(0.1, config.particleRadius);
        const double crR = std::max(prR, config.clusterRadius);
        const double capacity = std::max(1.0, (crR * crR) / (prR * prR));
        std::uniform_real_distribution<double> angR(0.0, kTwoPi);
        std::uniform_real_distribution<double> uniR(0.0, 1.0);
        int rejected = 0;
        while (static_cast<int>(foods.size()) < config.target)
        {
            // Fresh random centre for this chunk (never a reused site -> relocation).
            simulation::Vec2 center = world.clampPosition(randomPointInsideWorld(world, crR), crR);
            if (obstacles != nullptr && !obstacles->empty())
            {
                int attempts = 0;
                while (obstacles->containsPoint(center) && attempts < 16)
                {
                    center = world.clampPosition(randomPointInsideWorld(world, crR), crR);
                    ++attempts;
                }
                if (obstacles->containsPoint(center))
                {
                    ++stats.spawnsRejectedByObstacle;
                    if (++rejected >= 64) break;  // crowded world — avoid an infinite spin
                    continue;
                }
            }
            const std::uint32_t cid = foods.allocateClusterId();
            const int batch = std::min(static_cast<int>(capacity),
                                       config.target - static_cast<int>(foods.size()));
            const std::size_t before = foods.size();
            for (int k = 0; k < batch && static_cast<int>(foods.size()) < config.target; ++k)
            {
                const double rr = std::sqrt(uniR(rng_)) * crR;
                const double aa = angR(rng_);
                const simulation::Vec2 pos = world.clampPosition(
                    {center.x + std::cos(aa) * rr, center.y + std::sin(aa) * rr}, prR);
                if (obstacles != nullptr && !obstacles->empty() && obstacles->overlapsCircle(pos, prR))
                {
                    ++stats.spawnsRejectedByObstacle;
                    continue;
                }
                static_cast<void>(spawnChunkParticleAt(foods, pos, config, cid));
                ++stats.spawnedChunkParticles;
            }
            if (foods.size() > before) { ++stats.clustersCreated; rejected = 0; }
            else if (++rejected >= 64) break;  // no particle placed (all blocked) — stop
        }
        return stats;
    }

    // FIXED ("fixo", legacy/default): keep the field at the target by dropping each
    // MISSING particle into a fixed chunk site (round-robin), at a random spot within
    // clusterRadius. The chunk is bounded by its radius (never fragments past it) and the
    // work is O(deficit) per step — no full-food scan, no per-call cluster map.
    ensureChunkSites(world, config);
    if (chunkSites_.empty()) return stats;
    const double pr = std::max(0.1, config.particleRadius);
    const double cr = std::max(pr, config.clusterRadius);
    std::uniform_real_distribution<double> ang(0.0, kTwoPi);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    int rejected = 0;
    while (static_cast<int>(foods.size()) < config.target)
    {
        const std::size_t siteIdx = chunkCursor_ % chunkSites_.size();
        ++chunkCursor_;
        const simulation::Vec2 site = chunkSites_[siteIdx];
        const double rr = std::sqrt(uni(rng_)) * cr;
        const double aa = ang(rng_);
        const simulation::Vec2 pos =
            world.clampPosition({site.x + std::cos(aa) * rr, site.y + std::sin(aa) * rr}, pr);
        if (obstacles != nullptr && !obstacles->empty() && obstacles->overlapsCircle(pos, pr))
        {
            ++stats.spawnsRejectedByObstacle;
            if (++rejected >= 64) break; // world too crowded — avoid an infinite spin
            continue;
        }
        static_cast<void>(
            spawnChunkParticleAt(foods, pos, config, static_cast<std::uint32_t>(siteIdx) + 1U));
        ++stats.spawnedChunkParticles;
        rejected = 0;
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
