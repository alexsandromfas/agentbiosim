#include "systems/CollisionSystem.hpp"

#include "config/ParameterHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
constexpr double kEps = 1.0e-9;

double clamp(const double v, const double lo, const double hi) noexcept
{
    return std::max(lo, std::min(hi, v));
}

double chunkMassOf(const simulation::FoodStore& f, const std::size_t i,
                    const double scale)
{
    // Mass scales with area (radius^2) so larger chunks resist pushes more.
    const double r = std::max(0.1, f.radiusAt(i));
    return std::max(kEps, scale * r * r);
}

void clampToWorld(const simulation::World& world, simulation::Vec2& pos, const double radius)
{
    pos = world.clampPosition(pos, radius);
}

void pushOutOfObstacles(const simulation::ObstacleStore* obstacles,
                         simulation::Vec2& pos, const double radius)
{
    if (obstacles == nullptr || obstacles->empty()) return;
    if (!obstacles->overlapsCircle(pos, radius)) return;
    // Conservative fallback: revert the move by zero displacement is impossible
    // without history; instead push outward from each overlapping obstacle along
    // the shortest separation vector. Iterate a small number of times.
    for (int iter = 0; iter < 4; ++iter)
    {
        std::vector<std::size_t> overlapping;
        obstacles->queryRadius(pos, radius, overlapping);
        if (overlapping.empty()) return;
        for (const std::size_t oi : overlapping)
        {
            const auto op = obstacles->positionAt(oi);
            const double r = obstacles->radiusAt(oi) + radius;
            const double dx = pos.x - op.x;
            const double dy = pos.y - op.y;
            const double d2 = dx * dx + dy * dy;
            if (d2 <= kEps)
            {
                pos.x += r;  // arbitrary deterministic direction
                continue;
            }
            const double d = std::sqrt(d2);
            if (d < r)
            {
                const double overlap = (r - d) + 0.01;
                pos.x += dx / d * overlap;
                pos.y += dy / d * overlap;
            }
        }
    }
}
} // namespace

CollisionConfig CollisionSystem::fromRegistry(const config::ParameterRegistry& parameters)
{
    using config::parameterBool;
    using config::parameterDouble;
    using config::parameterInt;

    CollisionConfig c;
    c.agentCollisionEnabled = parameterBool(parameters, "agent_collision_enabled", c.agentCollisionEnabled);
    c.elasticityEnabled = parameterBool(parameters, "agent_collision_elasticity_enabled", c.elasticityEnabled);
    c.restitution = clamp(parameterDouble(parameters, "agent_collision_restitution", c.restitution), 0.0, 1.0);
    c.velocityTransfer = clamp(parameterDouble(parameters, "agent_collision_velocity_transfer", c.velocityTransfer), 0.0, 1.0);
    c.separation = clamp(parameterDouble(parameters, "agent_collision_separation", c.separation), 0.0, 1.0);
    c.maxImpulse = std::max(0.0, parameterDouble(parameters, "agent_collision_max_impulse", c.maxImpulse));

    c.viscosityEnabled = parameterBool(parameters, "global_viscosity_enabled", c.viscosityEnabled);
    c.viscosityDrag = std::max(0.0, parameterDouble(parameters, "global_viscosity_drag", c.viscosityDrag));
    c.brownianEnabled = parameterBool(parameters, "brownian_motion_enabled", c.brownianEnabled);
    c.brownianStrength = std::max(0.0, parameterDouble(parameters, "brownian_motion_strength", c.brownianStrength));

    c.movableChunkFoodEnabled = parameterBool(parameters, "movable_chunk_food_enabled", c.movableChunkFoodEnabled);
    c.chunkChunkCollisionEnabled = parameterBool(parameters, "chunk_food_collision_enabled", c.chunkChunkCollisionEnabled);
    c.chunkAdhesionEnabled = parameterBool(parameters, "chunk_food_adhesion_enabled", c.chunkAdhesionEnabled);
    c.chunkAdhesionStrength = std::max(0.0, parameterDouble(parameters, "chunk_food_adhesion_strength", c.chunkAdhesionStrength));
    c.chunkMassScale = std::max(kEps, parameterDouble(parameters, "chunk_food_mass_scale", c.chunkMassScale));
    c.chunkDrag = std::max(0.0, parameterDouble(parameters, "chunk_food_drag", c.chunkDrag));
    c.chunkPushStrength = std::max(0.0, parameterDouble(parameters, "chunk_food_push_strength", c.chunkPushStrength));

    c.useSpatial = parameterBool(parameters, "use_spatial", c.useSpatial);
    const int seedParam = parameterInt(parameters, "random_seed", -1);
    if (seedParam >= 0) c.seed = static_cast<std::uint64_t>(seedParam);
    return c;
}

CollisionStats CollisionSystem::apply(simulation::AgentStore& agents,
                                       simulation::FoodStore& foods,
                                       const simulation::World& world,
                                       simulation::SpatialHash* spatial,
                                       const simulation::ObstacleStore* obstacles,
                                       const CollisionConfig& config)
{
    CollisionStats stats;

    // Fast path: nothing enabled.
    const bool anyEnabled =
        config.agentCollisionEnabled || config.viscosityEnabled ||
        config.brownianEnabled || config.movableChunkFoodEnabled ||
        config.chunkChunkCollisionEnabled || config.chunkAdhesionEnabled;
    if (!anyEnabled || agents.empty()) return stats;

    if (!rngInitialized_)
    {
        rng_.seed(config.seed);
        rngInitialized_ = true;
    }

    const double dt = std::max(0.0, config.dt);
    const bool useSpatial = config.useSpatial && spatial != nullptr && !spatial->empty();

    // 1. Global viscosity drag (on agents).
    if (config.viscosityEnabled && config.viscosityDrag > 0.0)
    {
        const double drag = std::exp(-config.viscosityDrag * dt);
        for (std::size_t i = 0; i < agents.size(); ++i)
        {
            if (!agents.aliveAt(i)) continue;
            const simulation::Vec2 v = agents.velocityAt(i);
            agents.setVelocityAt(i, {v.x * drag, v.y * drag});
        }
    }

    // 1b. Chunk drag (always tied to movable chunks).
    if (config.movableChunkFoodEnabled && config.chunkDrag > 0.0)
    {
        const double drag = std::exp(-config.chunkDrag * dt);
        for (std::size_t i = 0; i < foods.size(); ++i)
        {
            if (!foods.aliveAt(i) || foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
            const simulation::Vec2 v = foods.velocityAt(i);
            foods.setVelocityAt(i, {v.x * drag, v.y * drag});
        }
    }

    // 2. Brownian motion (deterministic via rng_, seeded once).
    if (config.brownianEnabled && config.brownianStrength > 0.0)
    {
        std::normal_distribution<double> noise(0.0, config.brownianStrength);
        for (std::size_t i = 0; i < agents.size(); ++i)
        {
            if (!agents.aliveAt(i)) continue;
            const simulation::Vec2 v = agents.velocityAt(i);
            const simulation::Vec2 p = agents.positionAt(i);
            const double dvx = noise(rng_) * std::sqrt(dt);
            const double dvy = noise(rng_) * std::sqrt(dt);
            simulation::Vec2 newPos{p.x + dvx, p.y + dvy};
            clampToWorld(world, newPos, agents.radiusAt(i));
            agents.setPositionAt(i, newPos);
            agents.setVelocityAt(i, {v.x + dvx * 0.1, v.y + dvy * 0.1});
            ++stats.brownianApplied;
        }
        if (config.movableChunkFoodEnabled)
        {
            for (std::size_t i = 0; i < foods.size(); ++i)
            {
                if (!foods.aliveAt(i) || foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
                const simulation::Vec2 p = foods.positionAt(i);
                const double dvx = noise(rng_) * std::sqrt(dt) * 0.5;
                const double dvy = noise(rng_) * std::sqrt(dt) * 0.5;
                simulation::Vec2 newPos{p.x + dvx, p.y + dvy};
                clampToWorld(world, newPos, foods.radiusAt(i));
                foods.setPositionAt(i, newPos);
            }
        }
    }

    // 3. Chunk velocity integration.
    if (config.movableChunkFoodEnabled && dt > 0.0)
    {
        for (std::size_t i = 0; i < foods.size(); ++i)
        {
            if (!foods.aliveAt(i) || foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
            const simulation::Vec2 v = foods.velocityAt(i);
            const simulation::Vec2 p = foods.positionAt(i);
            simulation::Vec2 newPos{p.x + v.x * dt, p.y + v.y * dt};
            // World + obstacle clamp.
            clampToWorld(world, newPos, foods.radiusAt(i));
            if (obstacles != nullptr) pushOutOfObstacles(obstacles, newPos, foods.radiusAt(i));
            foods.setPositionAt(i, newPos);
        }
    }

    // 4. Agent-agent collision (separation + optional elasticity/transfer).
    if (config.agentCollisionEnabled)
    {
        const double maxImpulse = std::max(0.0, config.maxImpulse);
        const double sep = clamp(config.separation, 0.0, 1.0);
        const double rest = config.elasticityEnabled ? clamp(config.restitution, 0.0, 1.0) : 0.0;
        const double transfer = clamp(config.velocityTransfer, 0.0, 1.0);

        // Deterministic unique-pair set keyed by sorted entity ids.
        std::unordered_set<std::uint64_t> seenPair;
        auto pairKey = [](std::uint64_t a, std::uint64_t b) {
            if (a > b) std::swap(a, b);
            return (a << 32) ^ b;
        };

        auto resolvePair = [&](const std::size_t i, const std::size_t j) {
            if (i == j) return;
            if (!agents.aliveAt(i) || !agents.aliveAt(j)) return;
            const auto idA = agents.idAt(i).value;
            const auto idB = agents.idAt(j).value;
            const auto key = pairKey(idA, idB);
            if (!seenPair.insert(key).second) return;
            ++stats.agentPairsTested;
            simulation::Vec2 pa = agents.positionAt(i);
            simulation::Vec2 pb = agents.positionAt(j);
            const double ra = agents.radiusAt(i);
            const double rb = agents.radiusAt(j);
            const double dx = pb.x - pa.x;
            const double dy = pb.y - pa.y;
            const double d2 = dx * dx + dy * dy;
            const double rsum = ra + rb;
            if (d2 >= rsum * rsum) return;
            ++stats.agentCollisionsResolved;
            const double d = std::sqrt(std::max(d2, kEps));
            double nx, ny;
            if (d > kEps)
            {
                nx = dx / d;
                ny = dy / d;
            }
            else
            {
                // Degenerate: pick deterministic direction.
                nx = 1.0; ny = 0.0;
            }
            const double overlap = rsum - d;
            const double correction = std::min(overlap * sep, maxImpulse * 0.5);
            pa.x -= nx * correction;
            pa.y -= ny * correction;
            pb.x += nx * correction;
            pb.y += ny * correction;
            clampToWorld(world, pa, ra);
            clampToWorld(world, pb, rb);
            if (obstacles != nullptr)
            {
                pushOutOfObstacles(obstacles, pa, ra);
                pushOutOfObstacles(obstacles, pb, rb);
            }
            agents.setPositionAt(i, pa);
            agents.setPositionAt(j, pb);

            if (rest > 0.0 || transfer > 0.0)
            {
                const simulation::Vec2 va = agents.velocityAt(i);
                const simulation::Vec2 vb = agents.velocityAt(j);
                // Relative velocity along normal.
                const double rvx = vb.x - va.x;
                const double rvy = vb.y - va.y;
                const double rvn = rvx * nx + rvy * ny;
                if (rvn < 0.0)  // approaching
                {
                    const double j_imp = -(1.0 + rest) * rvn * 0.5;  // equal masses
                    const double clampedJ = clamp(j_imp, -maxImpulse, maxImpulse);
                    simulation::Vec2 vaNew{va.x - nx * clampedJ, va.y - ny * clampedJ};
                    simulation::Vec2 vbNew{vb.x + nx * clampedJ, vb.y + ny * clampedJ};
                    // Tangential velocity transfer.
                    if (transfer > 0.0)
                    {
                        const double tx = -ny;
                        const double ty = nx;
                        const double rvt = rvx * tx + rvy * ty;
                        const double tShare = clamp(rvt * 0.5 * transfer, -maxImpulse, maxImpulse);
                        vaNew.x += tx * tShare;
                        vaNew.y += ty * tShare;
                        vbNew.x -= tx * tShare;
                        vbNew.y -= ty * tShare;
                    }
                    agents.setVelocityAt(i, vaNew);
                    agents.setVelocityAt(j, vbNew);
                }
            }
        };

        if (useSpatial)
        {
            std::vector<simulation::SpatialItem> cand;
            for (std::size_t i = 0; i < agents.size(); ++i)
            {
                if (!agents.aliveAt(i)) continue;
                const auto p = agents.positionAt(i);
                const double r = agents.radiusAt(i);
                spatial->queryRadiusInto(p.x, p.y, r * 2.0, cand);
                for (const auto& c : cand)
                {
                    if (c.entityType != simulation::SpatialEntityType::Agent) continue;
                    const auto j = agents.indexOf(c.id);
                    if (!j.has_value()) continue;
                    resolvePair(i, *j);
                }
            }
        }
        else
        {
            for (std::size_t i = 0; i < agents.size(); ++i)
            {
                if (!agents.aliveAt(i)) continue;
                for (std::size_t j = i + 1; j < agents.size(); ++j)
                {
                    resolvePair(i, j);
                }
            }
        }
    }

    // 5. Agent-chunk collision (push).
    if (config.movableChunkFoodEnabled && config.chunkPushStrength > 0.0)
    {
        const double push = config.chunkPushStrength;
        const double maxImpulse = std::max(0.0, config.maxImpulse);
        if (useSpatial)
        {
            std::vector<simulation::SpatialItem> cand;
            for (std::size_t i = 0; i < agents.size(); ++i)
            {
                if (!agents.aliveAt(i)) continue;
                const auto p = agents.positionAt(i);
                const double r = agents.radiusAt(i);
                spatial->queryRadiusInto(p.x, p.y, r * 2.5, cand);
                for (const auto& c : cand)
                {
                    if (c.entityType != simulation::SpatialEntityType::Food) continue;
                    const auto fi = foods.indexOf(c.id);
                    if (!fi.has_value()) continue;
                    if (foods.kindAt(*fi) != simulation::FoodKind::Chunk) continue;
                    ++stats.agentFoodPairsTested;
                    const auto fp = foods.positionAt(*fi);
                    const double fr = foods.radiusAt(*fi);
                    const double dx = fp.x - p.x;
                    const double dy = fp.y - p.y;
                    const double rsum = r + fr;
                    if (dx * dx + dy * dy >= rsum * rsum) continue;
                    const double d = std::sqrt(std::max(dx*dx + dy*dy, kEps));
                    const double nx = d > kEps ? dx / d : 1.0;
                    const double ny = d > kEps ? dy / d : 0.0;
                    const simulation::Vec2 va = agents.velocityAt(i);
                    const double speed = std::sqrt(va.x * va.x + va.y * va.y);
                    const double mass = chunkMassOf(foods, *fi, config.chunkMassScale);
                    const double impulse = std::min(push * speed / std::max(kEps, mass), maxImpulse);
                    const simulation::Vec2 fv = foods.velocityAt(*fi);
                    foods.setVelocityAt(*fi, {fv.x + nx * impulse, fv.y + ny * impulse});
                    ++stats.foodPushes;
                }
            }
        }
        else
        {
            for (std::size_t i = 0; i < agents.size(); ++i)
            {
                if (!agents.aliveAt(i)) continue;
                const auto p = agents.positionAt(i);
                const double r = agents.radiusAt(i);
                for (std::size_t fi = 0; fi < foods.size(); ++fi)
                {
                    if (!foods.aliveAt(fi) || foods.kindAt(fi) != simulation::FoodKind::Chunk) continue;
                    const auto fp = foods.positionAt(fi);
                    const double fr = foods.radiusAt(fi);
                    const double dx = fp.x - p.x;
                    const double dy = fp.y - p.y;
                    const double rsum = r + fr;
                    if (dx * dx + dy * dy >= rsum * rsum) continue;
                    ++stats.agentFoodPairsTested;
                    const double d = std::sqrt(std::max(dx*dx + dy*dy, kEps));
                    const double nx = d > kEps ? dx / d : 1.0;
                    const double ny = d > kEps ? dy / d : 0.0;
                    const simulation::Vec2 va = agents.velocityAt(i);
                    const double speed = std::sqrt(va.x * va.x + va.y * va.y);
                    const double mass = chunkMassOf(foods, fi, config.chunkMassScale);
                    const double impulse = std::min(push * speed / std::max(kEps, mass), maxImpulse);
                    const simulation::Vec2 fv = foods.velocityAt(fi);
                    foods.setVelocityAt(fi, {fv.x + nx * impulse, fv.y + ny * impulse});
                    ++stats.foodPushes;
                }
            }
        }
    }

    // 6. Chunk-chunk collision (separation only).
    if (config.chunkChunkCollisionEnabled && !foods.empty())
    {
        std::unordered_set<std::uint64_t> seenFP;
        auto fpKey = [](std::uint64_t a, std::uint64_t b) {
            if (a > b) std::swap(a, b);
            return (a << 32) ^ b;
        };
        auto resolveChunkPair = [&](const std::size_t i, const std::size_t j) {
            if (i == j) return;
            if (!foods.aliveAt(i) || !foods.aliveAt(j)) return;
            if (foods.kindAt(i) != simulation::FoodKind::Chunk ||
                foods.kindAt(j) != simulation::FoodKind::Chunk) return;
            const auto idA = foods.idAt(i).value;
            const auto idB = foods.idAt(j).value;
            const auto key = fpKey(idA, idB);
            if (!seenFP.insert(key).second) return;
            ++stats.foodPairsTested;
            simulation::Vec2 pa = foods.positionAt(i);
            simulation::Vec2 pb = foods.positionAt(j);
            const double ra = foods.radiusAt(i);
            const double rb = foods.radiusAt(j);
            const double dx = pb.x - pa.x;
            const double dy = pb.y - pa.y;
            const double rsum = ra + rb;
            if (dx * dx + dy * dy >= rsum * rsum) return;
            ++stats.foodCollisionsResolved;
            const double d = std::sqrt(std::max(dx*dx + dy*dy, kEps));
            const double nx = d > kEps ? dx / d : 1.0;
            const double ny = d > kEps ? dy / d : 0.0;
            const double overlap = rsum - d;
            const double ma = chunkMassOf(foods, i, config.chunkMassScale);
            const double mb = chunkMassOf(foods, j, config.chunkMassScale);
            const double mSum = ma + mb;
            const double shareA = mb / mSum;
            const double shareB = ma / mSum;
            pa.x -= nx * overlap * shareA;
            pa.y -= ny * overlap * shareA;
            pb.x += nx * overlap * shareB;
            pb.y += ny * overlap * shareB;
            clampToWorld(world, pa, ra);
            clampToWorld(world, pb, rb);
            foods.setPositionAt(i, pa);
            foods.setPositionAt(j, pb);
        };

        if (useSpatial)
        {
            std::vector<simulation::SpatialItem> cand;
            for (std::size_t i = 0; i < foods.size(); ++i)
            {
                if (!foods.aliveAt(i) || foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
                const auto p = foods.positionAt(i);
                const double r = foods.radiusAt(i);
                spatial->queryRadiusInto(p.x, p.y, r * 2.0, cand);
                for (const auto& c : cand)
                {
                    if (c.entityType != simulation::SpatialEntityType::Food) continue;
                    const auto j = foods.indexOf(c.id);
                    if (!j.has_value()) continue;
                    resolveChunkPair(i, *j);
                }
            }
        }
        else
        {
            for (std::size_t i = 0; i < foods.size(); ++i)
            {
                if (!foods.aliveAt(i) || foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
                for (std::size_t j = i + 1; j < foods.size(); ++j)
                {
                    resolveChunkPair(i, j);
                }
            }
        }
    }

    // 7. Chunk-cluster adhesion (light attraction toward cluster center of mass).
    if (config.chunkAdhesionEnabled && config.chunkAdhesionStrength > 0.0 && !foods.empty())
    {
        std::map<std::uint32_t, std::pair<simulation::Vec2, std::size_t>> centers;
        for (std::size_t i = 0; i < foods.size(); ++i)
        {
            if (!foods.aliveAt(i) || foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
            const auto cid = foods.clusterIdAt(i);
            if (cid == 0U) continue;
            auto& entry = centers[cid];
            entry.first.x += foods.positionAt(i).x;
            entry.first.y += foods.positionAt(i).y;
            ++entry.second;
        }
        for (auto& kv : centers)
        {
            if (kv.second.second == 0U) continue;
            kv.second.first.x /= static_cast<double>(kv.second.second);
            kv.second.first.y /= static_cast<double>(kv.second.second);
        }
        const double strength = config.chunkAdhesionStrength;
        for (std::size_t i = 0; i < foods.size(); ++i)
        {
            if (!foods.aliveAt(i) || foods.kindAt(i) != simulation::FoodKind::Chunk) continue;
            const auto cid = foods.clusterIdAt(i);
            if (cid == 0U) continue;
            const auto& center = centers[cid].first;
            const auto p = foods.positionAt(i);
            const double dx = center.x - p.x;
            const double dy = center.y - p.y;
            const double d2 = dx * dx + dy * dy;
            if (d2 < kEps) continue;
            const double d = std::sqrt(d2);
            // Cap so adhesion never collapses everything into a single point.
            const double pull = std::min(d, strength * dt * 30.0);
            const double inv = 1.0 / d;
            simulation::Vec2 newPos{p.x + dx * inv * pull * 0.05,
                                     p.y + dy * inv * pull * 0.05};
            clampToWorld(world, newPos, foods.radiusAt(i));
            foods.setPositionAt(i, newPos);
            ++stats.adhesionsApplied;
        }
    }

    // 8. Final clamp into world + push out of obstacles for agents.
    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents.aliveAt(i)) continue;
        simulation::Vec2 p = agents.positionAt(i);
        clampToWorld(world, p, agents.radiusAt(i));
        pushOutOfObstacles(obstacles, p, agents.radiusAt(i));
        agents.setPositionAt(i, p);
    }
    return stats;
}
} // namespace agentbiosim::systems
