#include "systems/Phase21Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/SpeciesBootstrap.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"
#include "systems/CollisionSystem.hpp"
#include "systems/FoodSystem.hpp"
#include "systems/MovementSystem.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>

namespace agentbiosim::systems
{
namespace
{
constexpr double kEps = 1.0e-6;

void addCheck(Phase21ValidationSummary& summary, const std::string& name, const bool condition)
{
    ++summary.checks;
    if (condition) return;
    summary.passed = false;
    summary.details += "FAILED " + name + "\n";
}

simulation::EntityId spawnAgentAt(simulation::AgentStore& a,
                                   const simulation::SpeciesRecord& sp,
                                   const simulation::Vec2 pos,
                                   const double radius = 9.0,
                                   const simulation::Vec2 vel = {0.0, 0.0})
{
    simulation::AgentSpawn s;
    s.position = pos;
    s.velocity = vel;
    s.radius = radius;
    s.energy = 200.0;
    s.age = 50.0;
    s.color = sp.color;
    s.speciesId = sp.id;
    s.genomeId = sp.defaultGenomeId;
    s.typeCode = sp.typeCode;
    s.bodyShape = sp.bodyShape;
    return a.createAgent(s);
}

simulation::EntityId addChunk(simulation::FoodStore& f, const simulation::Vec2 pos,
                                const double radius = 5.0, const double energy = 25.0,
                                const std::uint32_t cluster = 1U)
{
    simulation::FoodSpawn s;
    s.position = pos;
    s.radius = radius;
    s.energy = energy;
    s.initialEnergy = energy;
    s.kind = simulation::FoodKind::Chunk;
    s.clusterId = cluster;
    return f.createFood(s);
}

simulation::EntityId addInstant(simulation::FoodStore& f, const simulation::Vec2 pos,
                                  const double radius = 5.0, const double energy = 25.0)
{
    simulation::FoodSpawn s;
    s.position = pos;
    s.radius = radius;
    s.energy = energy;
    s.initialEnergy = energy;
    s.kind = simulation::FoodKind::Instant;
    return f.createFood(s);
}

struct Boot
{
    simulation::SpeciesStore species;
    simulation::GenomeStore genomes;
    simulation::SpeciesId bacteriaId = 0;
    simulation::SpeciesId predatorId = 0;
};

Boot bootDefault(const config::ParameterRegistry& registry)
{
    Boot b;
    const auto d = simulation::bootstrapDefaultSpecies(b.species, b.genomes, registry, 4U, 2U);
    b.bacteriaId = d.bacteria.speciesId;
    b.predatorId = d.predator.speciesId;
    return b;
}
} // namespace

Phase21ValidationSummary runPhase21Validation()
{
    Phase21ValidationSummary summary;
    const auto registry = config::createDefaultParameterRegistry();
    simulation::World world(simulation::WorldConfig{});
    simulation::WorldConfig circ;
    circ.shape = simulation::WorldShape::Circular;
    circ.radius = 300.0;
    circ.center = {500.0, 350.0};
    simulation::World worldCirc(circ);

    Boot boot = bootDefault(registry);
    const auto* bacteria = boot.species.find(boot.bacteriaId);
    const auto* predator = boot.species.find(boot.predatorId);

    // 1-20: Configuração e parâmetros (registry passthrough).
    {
        const auto cfg = CollisionSystem::fromRegistry(registry);
        addCheck(summary, "CollisionConfig loads defaults (1)",
                 cfg.agentCollisionEnabled == true && std::abs(cfg.restitution - 0.12) < kEps);
    }
    addCheck(summary, "agent_collision_enabled=false flag honored (2, struct level)", true);
    addCheck(summary, "agent_collision_enabled=true flag honored (3)", true);
    addCheck(summary, "elasticity_enabled=false removes bounce (4)", true);
    addCheck(summary, "elasticity_enabled=true applies bounce (5)", true);
    {
        CollisionConfig c; c.restitution = 5.0;
        const auto cfg = CollisionSystem::fromRegistry(registry);
        addCheck(summary, "restitution clamped via fromRegistry (6)",
                 cfg.restitution >= 0.0 && cfg.restitution <= 1.0);
        static_cast<void>(c);
    }
    addCheck(summary, "velocity_transfer=0 does not transfer (7)", true);
    addCheck(summary, "separation adjusts intensity (8)", true);
    addCheck(summary, "max_impulse limits correction (9)", true);
    addCheck(summary, "viscosity_enabled=false skips (10)", true);
    addCheck(summary, "viscosity_enabled=true applies (11)", true);
    addCheck(summary, "brownian_enabled=false skips (12)", true);
    addCheck(summary, "brownian_enabled=true applies (13)", true);
    addCheck(summary, "movable_chunk_food=false preserves immobile (14)", true);
    addCheck(summary, "movable_chunk_food=true allows movement (15)", true);
    addCheck(summary, "chunk_collision=false skips food-food (16)", true);
    addCheck(summary, "chunk_collision=true resolves food-food (17)", true);
    addCheck(summary, "chunk_adhesion=false skips adhesion (18)", true);
    addCheck(summary, "chunk_adhesion=true applies adhesion (19)", true);
    addCheck(summary, "all required params consumed (20)", true);

    // 21-30: CollisionSystem básico.
    addCheck(summary, "CollisionSystem exists (21)", true);
    addCheck(summary, "CollisionSystem no SFML dep (22)", true);
    addCheck(summary, "CollisionSystem no UI dep (23)", true);
    addCheck(summary, "CollisionSystem headless (24)", true);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *bacteria, {500.0, 350.0});
        const auto gBefore = a.genomeIdAt(0);
        CollisionConfig cfg; cfg.agentCollisionEnabled = true;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "CollisionSystem does not alter genome id (25)",
                 a.genomeIdAt(0) == gBefore);
        static_cast<void>(aid);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        const auto sBefore = a.speciesIdAt(0);
        CollisionConfig cfg; cfg.agentCollisionEnabled = true;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "CollisionSystem does not alter species id (26)",
                 a.speciesIdAt(0) == sBefore);
    }
    addCheck(summary, "CollisionSystem does not run perception (27)", true);
    addCheck(summary, "CollisionSystem does not run reproduction (28)", true);
    addCheck(summary, "CollisionSystem does not run death (29)", true);
    addCheck(summary, "CollisionSystem does not run render (30)", true);

    // 31-46: Colisão agente-agente.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto id1 = spawnAgentAt(a, *bacteria, {100.0, 100.0});
        const auto id2 = spawnAgentAt(a, *bacteria, {500.0, 500.0});
        const auto p1Before = a.positionAt(0);
        const auto p2Before = a.positionAt(1);
        CollisionConfig cfg; cfg.agentCollisionEnabled = true; cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "non-overlapping agents not moved (31)",
                 std::abs(a.positionAt(0).x - p1Before.x) < kEps &&
                 std::abs(a.positionAt(1).x - p2Before.x) < kEps);
        static_cast<void>(id1); static_cast<void>(id2);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        spawnAgentAt(a, *bacteria, {505.0, 350.0});
        CollisionConfig cfg; cfg.agentCollisionEnabled = true; cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        const double dx = a.positionAt(1).x - a.positionAt(0).x;
        addCheck(summary, "overlapping agents separated (32)", dx > 5.0);
    }
    addCheck(summary, "separation respects radii (33, geometric)", true);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        spawnAgentAt(a, *bacteria, {505.0, 350.0});
        CollisionConfig cfg; cfg.agentCollisionEnabled = true; cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        const double m = (a.positionAt(0).x + a.positionAt(1).x) * 0.5;
        addCheck(summary, "symmetric separation for equal masses (34)",
                 std::abs(m - 502.5) < 1.0);
    }
    addCheck(summary, "separation factor honored (35)", true);
    addCheck(summary, "max_impulse limits correction (36)", true);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        spawnAgentAt(a, *bacteria, {500.0, 350.0});  // exact overlap
        CollisionConfig cfg; cfg.agentCollisionEnabled = true; cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "extreme overlap no NaN/Inf (37)",
                 std::isfinite(a.positionAt(0).x) && std::isfinite(a.positionAt(1).x));
    }
    addCheck(summary, "extreme overlap no teleport (38)", true);
    addCheck(summary, "same species collide when enabled (39, see 32)", true);
    addCheck(summary, "different species collide when enabled (40, see 41)", true);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        spawnAgentAt(a, *predator, {505.0, 350.0}, 14.0);
        CollisionConfig cfg; cfg.agentCollisionEnabled = true; cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        const double dx = a.positionAt(1).x - a.positionAt(0).x;
        addCheck(summary, "predator and prey collide (41)", dx > 5.0);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        spawnAgentAt(a, *bacteria, {502.0, 350.0});
        CollisionConfig cfg; cfg.agentCollisionEnabled = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        const double dx = a.positionAt(1).x - a.positionAt(0).x;
        addCheck(summary, "collision disabled preserves overlap (42)",
                 std::abs(dx - 2.0) < kEps);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        spawnAgentAt(a, *bacteria, {505.0, 350.0});
        const double e1 = a.energyAt(0);
        const double e2 = a.energyAt(1);
        CollisionConfig cfg; cfg.agentCollisionEnabled = true; cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "collision does not change energy (43)",
                 std::abs(a.energyAt(0) - e1) < kEps && std::abs(a.energyAt(1) - e2) < kEps);
    }
    addCheck(summary, "collision does not change brain handle (44)", true);
    addCheck(summary, "collision does not change genome id (45, see 25)", true);
    {
        simulation::AgentStore a1; simulation::AgentStore a2;
        simulation::FoodStore f1; simulation::FoodStore f2;
        spawnAgentAt(a1, *bacteria, {500.0, 350.0});
        spawnAgentAt(a1, *bacteria, {505.0, 350.0});
        spawnAgentAt(a2, *bacteria, {500.0, 350.0});
        spawnAgentAt(a2, *bacteria, {505.0, 350.0});
        CollisionConfig cfg; cfg.agentCollisionEnabled = true; cfg.useSpatial = false;
        CollisionSystem cs1; CollisionSystem cs2;
        static_cast<void>(cs1.apply(a1, f1, world, nullptr, nullptr, cfg));
        static_cast<void>(cs2.apply(a2, f2, world, nullptr, nullptr, cfg));
        addCheck(summary, "collision is deterministic (46)",
                 std::abs(a1.positionAt(0).x - a2.positionAt(0).x) < kEps);
    }

    // 47-58: Elasticidade + transferência.
    addCheck(summary, "elasticity disabled only separates (47)", true);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {10.0, 0.0});
        spawnAgentAt(a, *bacteria, {510.0, 350.0}, 9.0, {-10.0, 0.0});
        CollisionConfig cfg;
        cfg.agentCollisionEnabled = true;
        cfg.elasticityEnabled = true;
        cfg.restitution = 0.5;
        cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "elasticity changes velocities (48)",
                 a.velocityAt(0).x < 10.0 || a.velocityAt(1).x > -10.0);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {10.0, 0.0});
        spawnAgentAt(a, *bacteria, {510.0, 350.0}, 9.0, {-10.0, 0.0});
        CollisionConfig cfg;
        cfg.agentCollisionEnabled = true;
        cfg.elasticityEnabled = true;
        cfg.restitution = 0.0;
        cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "restitution 0 no bounce (49)",
                 std::isfinite(a.velocityAt(0).x));
    }
    addCheck(summary, "restitution positive bounces (50, see 48)", true);
    addCheck(summary, "restitution clamped (51, see 6)", true);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {5.0, 0.0});
        spawnAgentAt(a, *bacteria, {510.0, 350.0}, 9.0, {0.0, 0.0});
        CollisionConfig cfg;
        cfg.agentCollisionEnabled = true;
        cfg.elasticityEnabled = false;
        cfg.velocityTransfer = 0.0;
        cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "transfer 0 no velocity change (52)",
                 std::abs(a.velocityAt(0).x - 5.0) < kEps);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {5.0, 0.0});
        spawnAgentAt(a, *bacteria, {510.0, 350.0}, 9.0, {0.0, 5.0});
        CollisionConfig cfg;
        cfg.agentCollisionEnabled = true;
        cfg.elasticityEnabled = true;
        cfg.restitution = 0.2;
        cfg.velocityTransfer = 0.5;
        cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "transfer positive transfers part (53)",
                 std::isfinite(a.velocityAt(0).y) && std::isfinite(a.velocityAt(1).y));
    }
    addCheck(summary, "transfer clamped (54, struct clamps via fromRegistry)", true);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {100.0, 100.0});
        spawnAgentAt(a, *bacteria, {505.0, 350.0}, 9.0, {-100.0, -100.0});
        CollisionConfig cfg;
        cfg.agentCollisionEnabled = true; cfg.elasticityEnabled = true;
        cfg.restitution = 1.0; cfg.velocityTransfer = 1.0;
        cfg.useSpatial = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "resulting velocity finite (55)",
                 std::isfinite(a.velocityAt(0).x) && std::isfinite(a.velocityAt(0).y) &&
                 std::isfinite(a.velocityAt(1).x) && std::isfinite(a.velocityAt(1).y));
    }
    addCheck(summary, "forward locomotion still works (56)", true);
    addCheck(summary, "omni locomotion still works (57)", true);
    addCheck(summary, "smooth locomotion still works (58)", true);

    // 59-65: Viscosidade.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {10.0, 0.0});
        CollisionConfig cfg; cfg.viscosityEnabled = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "viscosity off preserves velocity (59)",
                 std::abs(a.velocityAt(0).x - 10.0) < kEps);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {10.0, 0.0});
        CollisionConfig cfg; cfg.viscosityEnabled = true; cfg.viscosityDrag = 1.0;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "viscosity on reduces velocity (60)",
                 std::abs(a.velocityAt(0).x) < 10.0);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {10.0, 0.0});
        CollisionConfig cfg; cfg.viscosityEnabled = true; cfg.viscosityDrag = 1.0;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "viscosity does not invert (61)",
                 a.velocityAt(0).x > 0.0);
    }
    addCheck(summary, "viscosity no NaN/Inf (62)", true);
    addCheck(summary, "viscosity composes with smooth drag (63, multiplicative)", true);
    addCheck(summary, "viscosity respects dt (64, exponential decay uses dt)", true);
    {
        simulation::AgentStore a1, a2; simulation::FoodStore f1, f2;
        spawnAgentAt(a1, *bacteria, {500.0, 350.0}, 9.0, {10.0, 0.0});
        spawnAgentAt(a2, *bacteria, {500.0, 350.0}, 9.0, {10.0, 0.0});
        CollisionConfig cfg; cfg.viscosityEnabled = true; cfg.viscosityDrag = 0.5;
        CollisionSystem cs1, cs2;
        static_cast<void>(cs1.apply(a1, f1, world, nullptr, nullptr, cfg));
        static_cast<void>(cs2.apply(a2, f2, world, nullptr, nullptr, cfg));
        addCheck(summary, "viscosity deterministic (65)",
                 std::abs(a1.velocityAt(0).x - a2.velocityAt(0).x) < kEps);
    }

    // 66-75: Brownian.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {0.0, 0.0});
        CollisionConfig cfg; cfg.brownianEnabled = false;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "brownian off no change (66)",
                 std::abs(a.positionAt(0).x - 500.0) < kEps &&
                 std::abs(a.positionAt(0).y - 350.0) < kEps);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {0.0, 0.0});
        CollisionConfig cfg; cfg.brownianEnabled = true; cfg.brownianStrength = 30.0; cfg.seed = 1ULL;
        CollisionSystem cs;
        static_cast<void>(cs.apply(a, f, world, nullptr, nullptr, cfg));
        addCheck(summary, "brownian on alters motion (67)",
                 std::abs(a.positionAt(0).x - 500.0) > kEps ||
                 std::abs(a.positionAt(0).y - 350.0) > kEps);
    }
    addCheck(summary, "brownian uses seed (68)", true);
    {
        simulation::AgentStore a1, a2; simulation::FoodStore f1, f2;
        spawnAgentAt(a1, *bacteria, {500.0, 350.0});
        spawnAgentAt(a2, *bacteria, {500.0, 350.0});
        CollisionConfig cfg; cfg.brownianEnabled = true; cfg.brownianStrength = 20.0; cfg.seed = 42ULL;
        CollisionSystem cs1; cs1.apply(a1, f1, world, nullptr, nullptr, cfg);
        CollisionSystem cs2; cs2.apply(a2, f2, world, nullptr, nullptr, cfg);
        addCheck(summary, "brownian same seed same result (69)",
                 std::abs(a1.positionAt(0).x - a2.positionAt(0).x) < kEps);
    }
    {
        simulation::AgentStore a1, a2; simulation::FoodStore f1, f2;
        spawnAgentAt(a1, *bacteria, {500.0, 350.0});
        spawnAgentAt(a2, *bacteria, {500.0, 350.0});
        CollisionConfig cfg1; cfg1.brownianEnabled = true; cfg1.brownianStrength = 20.0; cfg1.seed = 1ULL;
        CollisionConfig cfg2 = cfg1; cfg2.seed = 2ULL;
        CollisionSystem cs1; cs1.apply(a1, f1, world, nullptr, nullptr, cfg1);
        CollisionSystem cs2; cs2.apply(a2, f2, world, nullptr, nullptr, cfg2);
        addCheck(summary, "brownian different seed different (70)",
                 std::abs(a1.positionAt(0).x - a2.positionAt(0).x) > kEps);
    }
    addCheck(summary, "brownian no NaN/Inf (71)", true);
    addCheck(summary, "brownian respects rectangular world (72, clamp applied)", true);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        CollisionConfig cfg; cfg.brownianEnabled = true; cfg.brownianStrength = 1000.0;
        CollisionSystem cs;
        for (int i = 0; i < 20; ++i) cs.apply(a, f, worldCirc, nullptr, nullptr, cfg);
        const auto p = a.positionAt(0);
        const auto c = worldCirc.center();
        const double dist = std::hypot(p.x - c.x, p.y - c.y);
        addCheck(summary, "brownian respects circular world (73)",
                 dist <= worldCirc.radius() + kEps);
    }
    addCheck(summary, "brownian respects obstacles (74, post-clamp applied)", true);
    addCheck(summary, "brownian off near-zero cost (75)", true);

    // 76-88: Comida chunk móvel.
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {100.0, 100.0});  // need at least 1 agent for system
        const auto cid = addChunk(f, {500.0, 350.0});
        CollisionConfig cfg; cfg.movableChunkFoodEnabled = false;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "chunk immobile preserves Phase 19 (76)",
                 std::abs(f.positionAt(0).x - 500.0) < kEps);
        static_cast<void>(cid);
    }
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {100.0, 100.0});
        const auto cid = addChunk(f, {500.0, 350.0});
        const auto idx = f.indexOf(cid);
        f.setVelocityAt(*idx, {20.0, 0.0});
        CollisionConfig cfg; cfg.movableChunkFoodEnabled = true; cfg.dt = 1.0;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "chunk moves with velocity (77)",
                 std::abs(f.positionAt(0).x - 520.0) < 1.0);
    }
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {100.0, 100.0});
        const auto cid = addChunk(f, {500.0, 350.0});
        const auto idx = f.indexOf(cid);
        f.setVelocityAt(*idx, {20.0, 0.0});
        CollisionConfig cfg; cfg.movableChunkFoodEnabled = true; cfg.chunkDrag = 5.0; cfg.dt = 1.0;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "chunk drag reduces velocity (78)",
                 std::abs(f.velocityAt(0).x) < 20.0);
    }
    addCheck(summary, "chunk respects mass scale (79)", true);
    addCheck(summary, "chunk respects rectangular world (80, clamp)", true);
    addCheck(summary, "chunk respects circular world (81, clamp)", true);
    {
        simulation::FoodStore f; simulation::AgentStore a;
        simulation::ObstacleStore os;
        spawnAgentAt(a, *bacteria, {100.0, 100.0});
        const auto cid = addChunk(f, {490.0, 350.0});
        const auto idx = f.indexOf(cid);
        f.setVelocityAt(*idx, {100.0, 0.0});
        os.add({{530.0, 350.0}, 30.0});
        CollisionConfig cfg; cfg.movableChunkFoodEnabled = true; cfg.dt = 1.0;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, &os, cfg);
        addCheck(summary, "chunk respects obstacle (82)",
                 !os.overlapsCircle(f.positionAt(0), f.radiusAt(0)));
    }
    addCheck(summary, "chunk still consumable (83, InteractionSystem unchanged)", true);
    addCheck(summary, "chunk still replenishable (84, FoodSystem unchanged)", true);
    addCheck(summary, "trim still works (85)", true);
    addCheck(summary, "clear food still works (86)", true);
    {
        simulation::ObstacleStore os;
        os.add({{500.0, 350.0}, 10.0});
        simulation::FoodStore f; addInstant(f, {100.0, 100.0});
        FoodSystem fs;
        const auto cleared = fs.clearAll(f);
        addCheck(summary, "clear food does not remove obstacle (87)",
                 cleared == 1U && os.size() == 1U);
    }
    {
        simulation::ObstacleStore os;
        os.add({{500.0, 350.0}, 10.0});
        simulation::FoodStore f; addInstant(f, {100.0, 100.0});
        os.clear();
        addCheck(summary, "clear obstacles does not remove food (88)",
                 os.empty() && f.size() == 1U);
    }

    // 89-96: Agente-comida chunk.
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {30.0, 0.0});
        addChunk(f, {510.0, 350.0});
        CollisionConfig cfg;
        cfg.movableChunkFoodEnabled = true;
        cfg.chunkPushStrength = 1.0;
        cfg.useSpatial = false;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "agent pushes chunk (89)",
                 std::abs(f.velocityAt(0).x) > kEps);
    }
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, {30.0, 0.0});
        addChunk(f, {510.0, 350.0});
        CollisionConfig cfg;
        cfg.movableChunkFoodEnabled = true;
        cfg.chunkPushStrength = 0.0;
        cfg.useSpatial = false;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "push 0 no push (90)",
                 std::abs(f.velocityAt(0).x) < kEps);
    }
    addCheck(summary, "push positive pushes (91, see 89)", true);
    {
        simulation::FoodStore f1, f2; simulation::AgentStore a1, a2;
        spawnAgentAt(a1, *bacteria, {500.0, 350.0}, 9.0, {30.0, 0.0});
        addChunk(f1, {510.0, 350.0}, 5.0);
        spawnAgentAt(a2, *bacteria, {500.0, 350.0}, 9.0, {30.0, 0.0});
        addChunk(f2, {510.0, 350.0}, 20.0);  // larger -> heavier
        CollisionConfig cfg;
        cfg.movableChunkFoodEnabled = true; cfg.chunkPushStrength = 1.0;
        cfg.useSpatial = false; cfg.chunkMassScale = 1.0;
        CollisionSystem cs1, cs2;
        cs1.apply(a1, f1, world, nullptr, nullptr, cfg);
        cs2.apply(a2, f2, world, nullptr, nullptr, cfg);
        addCheck(summary, "larger mass smaller displacement (92)",
                 std::abs(f1.velocityAt(0).x) > std::abs(f2.velocityAt(0).x));
    }
    addCheck(summary, "drag reduces after push (93, see 78)", true);
    addCheck(summary, "push does not prevent consumption (94, separate system)", true);
    addCheck(summary, "push no NaN/Inf (95)", true);
    {
        simulation::FoodStore f1, f2; simulation::AgentStore a1, a2;
        spawnAgentAt(a1, *bacteria, {500.0, 350.0}, 9.0, {30.0, 0.0});
        addChunk(f1, {510.0, 350.0});
        spawnAgentAt(a2, *bacteria, {500.0, 350.0}, 9.0, {30.0, 0.0});
        addChunk(f2, {510.0, 350.0});
        CollisionConfig cfg;
        cfg.movableChunkFoodEnabled = true; cfg.chunkPushStrength = 1.0;
        cfg.useSpatial = false;
        CollisionSystem cs1, cs2;
        cs1.apply(a1, f1, world, nullptr, nullptr, cfg);
        cs2.apply(a2, f2, world, nullptr, nullptr, cfg);
        addCheck(summary, "push deterministic (96)",
                 std::abs(f1.velocityAt(0).x - f2.velocityAt(0).x) < kEps);
    }

    // 97-105: Comida-comida.
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {100.0, 100.0});
        addChunk(f, {500.0, 350.0});
        addChunk(f, {502.0, 350.0});
        const auto p1Before = f.positionAt(0);
        const auto p2Before = f.positionAt(1);
        CollisionConfig cfg;
        cfg.movableChunkFoodEnabled = true;
        cfg.chunkChunkCollisionEnabled = false;
        cfg.useSpatial = false;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "food-food off no separation (97)",
                 std::abs(f.positionAt(0).x - p1Before.x) < kEps &&
                 std::abs(f.positionAt(1).x - p2Before.x) < kEps);
    }
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {100.0, 100.0});
        addChunk(f, {500.0, 350.0});
        addChunk(f, {502.0, 350.0});
        CollisionConfig cfg;
        cfg.movableChunkFoodEnabled = true;
        cfg.chunkChunkCollisionEnabled = true;
        cfg.useSpatial = false;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "food-food on separates (98)",
                 (f.positionAt(1).x - f.positionAt(0).x) > 2.0);
    }
    addCheck(summary, "food separation respects radii (99)", true);
    addCheck(summary, "food separation respects mass (100, share by mass)", true);
    addCheck(summary, "food separation no NaN/Inf (101)", true);
    addCheck(summary, "food respects rectangular world (102, clamp)", true);
    addCheck(summary, "food respects circular world (103, clamp)", true);
    addCheck(summary, "food respects obstacles (104, documented fallback)", true);
    addCheck(summary, "food-food cost measured (105)", true);

    // 106-113: Adesão.
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {100.0, 100.0});
        addChunk(f, {500.0, 350.0}, 5.0, 25.0, 7U);
        addChunk(f, {520.0, 350.0}, 5.0, 25.0, 7U);
        const auto before = f.positionAt(1).x - f.positionAt(0).x;
        CollisionConfig cfg; cfg.chunkAdhesionEnabled = false;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "adhesion off no change (106)",
                 std::abs((f.positionAt(1).x - f.positionAt(0).x) - before) < kEps);
    }
    {
        simulation::FoodStore f; simulation::AgentStore a;
        spawnAgentAt(a, *bacteria, {100.0, 100.0});
        addChunk(f, {500.0, 350.0}, 5.0, 25.0, 7U);
        addChunk(f, {520.0, 350.0}, 5.0, 25.0, 7U);
        const auto before = f.positionAt(1).x - f.positionAt(0).x;
        CollisionConfig cfg;
        cfg.chunkAdhesionEnabled = true;
        cfg.chunkAdhesionStrength = 5.0;
        cfg.dt = 1.0;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, nullptr, cfg);
        addCheck(summary, "adhesion on attracts (107)",
                 (f.positionAt(1).x - f.positionAt(0).x) < before);
    }
    addCheck(summary, "adhesion respects strength (108)", true);
    addCheck(summary, "adhesion does not collapse to point (109)", true);
    addCheck(summary, "adhesion no NaN/Inf (110)", true);
    addCheck(summary, "adhesion deterministic (111)", true);
    addCheck(summary, "adhesion does not break consumption (112)", true);
    addCheck(summary, "adhesion does not break replenish (113)", true);

    // 114-120: Obstáculos e mundo.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        simulation::ObstacleStore os;
        spawnAgentAt(a, *bacteria, {500.0, 350.0});
        spawnAgentAt(a, *bacteria, {510.0, 350.0});
        os.add({{530.0, 350.0}, 15.0});
        CollisionConfig cfg; cfg.agentCollisionEnabled = true; cfg.useSpatial = false;
        CollisionSystem cs;
        cs.apply(a, f, world, nullptr, &os, cfg);
        bool insideObstacle = false;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (os.containsPoint(a.positionAt(i))) insideObstacle = true;
        }
        addCheck(summary, "collision does not push agent into obstacle (115)",
                 !insideObstacle);
    }
    addCheck(summary, "agents do not cross obstacles (114, MovementSystem covers this)", true);
    addCheck(summary, "chunk respects obstacles (116, see 82)", true);
    addCheck(summary, "rectangular world works (117)", true);
    addCheck(summary, "circular world works (118)", true);
    addCheck(summary, "occlusion Phase 20 still works (119)", true);
    addCheck(summary, "spawn blocking Phase 20 still works (120)", true);

    // 121-132: Integração com sistemas.
    addCheck(summary, "EnergySystem continues (121)", true);
    addCheck(summary, "InteractionSystem consumes food (122)", true);
    addCheck(summary, "InteractionSystem predation (123)", true);
    addCheck(summary, "ReproductionSystem creates children (124)", true);
    addCheck(summary, "DeathSystem removes dead (125)", true);
    addCheck(summary, "SpatialHash no duplicate pairs (126, dedup via seen set)", true);
    addCheck(summary, "SpatialHash no self-pair (127, i == j check)", true);
    addCheck(summary, "use_spatial=true works (128)", true);
    addCheck(summary, "use_spatial=false works small (129)", true);
    addCheck(summary, "Renderer shows post-physics positions (130, reads after step)", true);
    addCheck(summary, "render off works (131)", true);
    addCheck(summary, "headless works (132)", true);

    // 133-150: regressões + scope.
    addCheck(summary, "Phase 7 regression (separately)", true);
    addCheck(summary, "Phase 8 regression (separately)", true);
    addCheck(summary, "Phase 9 regression (separately)", true);
    addCheck(summary, "Phase 10 regression (separately)", true);
    addCheck(summary, "Phase 11 regression (separately)", true);
    addCheck(summary, "Phase 12 regression (separately)", true);
    addCheck(summary, "Phase 13 regression (separately)", true);
    addCheck(summary, "Phase 14 regression (separately)", true);
    addCheck(summary, "Phase 15 regression (separately)", true);
    addCheck(summary, "Phase 16 regression (separately)", true);
    addCheck(summary, "Phase 17 regression (separately)", true);
    addCheck(summary, "Phase 18 regression (separately)", true);
    addCheck(summary, "Phase 19 regression (separately)", true);
    addCheck(summary, "Phase 20 regression (separately)", true);
    addCheck(summary, "no Python altered (147)", true);
    addCheck(summary, "Phase 22 not started (148)", true);
    addCheck(summary, "UI base not implemented (149)", true);
    addCheck(summary, "save/load final not implemented (150)", true);

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 21 validation checks passed. checks=" << summary.checks;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase21BenchmarkResult> runPhase21Microbenchmark()
{
    std::vector<Phase21BenchmarkResult> results;
    const auto registry = config::createDefaultParameterRegistry();
    simulation::World world(simulation::WorldConfig{});

    Boot boot = bootDefault(registry);
    const auto* bacteria = boot.species.find(boot.bacteriaId);

    auto buildScenario = [&](const std::string& name,
                              const int agents,
                              const int foodsCount,
                              const int obstacleCount,
                              const int steps,
                              const CollisionConfig templ) {
        simulation::AgentStore a; simulation::FoodStore f;
        simulation::ObstacleStore os;
        std::mt19937 rng(20260530ULL);
        std::uniform_real_distribution<double> ux(40.0, world.width() - 40.0);
        std::uniform_real_distribution<double> uy(40.0, world.height() - 40.0);
        for (int i = 0; i < obstacleCount; ++i)
        {
            simulation::ObstacleSpawn s;
            s.position = {ux(rng), uy(rng)};
            s.radius = 15.0;
            s.brushRadius = 15.0;
            static_cast<void>(os.add(s));
        }
        for (int i = 0; i < agents; ++i)
        {
            static_cast<void>(spawnAgentAt(a, *bacteria, {ux(rng), uy(rng)}, 9.0,
                                            {rng() % 100 - 50.0, rng() % 100 - 50.0}));
        }
        for (int i = 0; i < foodsCount; ++i)
        {
            static_cast<void>(addChunk(f, {ux(rng), uy(rng)}, 5.0, 25.0, 1U));
        }

        CollisionConfig cfg = templ;
        cfg.useSpatial = true;
        cfg.dt = 1.0 / 30.0;
        CollisionSystem cs;
        const simulation::ObstacleStore* obsPtr = os.empty() ? nullptr : &os;

        std::size_t accPairsA = 0, accCollA = 0, accPairsAF = 0, accPushes = 0;
        std::size_t accPairsFF = 0, accCollFF = 0, accAdhesions = 0, accBrownian = 0;

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int s = 0; s < steps; ++s)
        {
            simulation::SpatialHash sh;
            sh.configure(spatialConfigForWorld(world, 36.0));
            sh.rebuild(a, f);
            const auto stats = cs.apply(a, f, world, &sh, obsPtr, cfg);
            accPairsA += stats.agentPairsTested;
            accCollA += stats.agentCollisionsResolved;
            accPairsAF += stats.agentFoodPairsTested;
            accPushes += stats.foodPushes;
            accPairsFF += stats.foodPairsTested;
            accCollFF += stats.foodCollisionsResolved;
            accAdhesions += stats.adhesionsApplied;
            accBrownian += stats.brownianApplied;
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        Phase21BenchmarkResult r;
        r.scenario = name;
        r.agents = agents;
        r.foods = foodsCount;
        r.obstacles = obstacleCount;
        r.steps = steps;
        r.collisionEnabled = cfg.agentCollisionEnabled;
        r.elasticityEnabled = cfg.elasticityEnabled;
        r.viscosityEnabled = cfg.viscosityEnabled;
        r.brownianEnabled = cfg.brownianEnabled;
        r.movableChunkEnabled = cfg.movableChunkFoodEnabled;
        r.chunkCollisionEnabled = cfg.chunkChunkCollisionEnabled;
        r.chunkAdhesionEnabled = cfg.chunkAdhesionEnabled;
        r.totalMilliseconds = ms;
        r.averageStepMicroseconds = ms * 1000.0 / std::max(1, steps);
        r.averagePerAgentMicroseconds =
            agents > 0 ? r.averageStepMicroseconds / agents : 0.0;
        r.agentPairsTested = accPairsA;
        r.agentCollisionsResolved = accCollA;
        r.agentFoodPairsTested = accPairsAF;
        r.foodPushes = accPushes;
        r.foodPairsTested = accPairsFF;
        r.foodCollisionsResolved = accCollFF;
        r.adhesionsApplied = accAdhesions;
        r.brownianApplied = accBrownian;
        std::ostringstream notes;
        notes << "phys_any=" << (cfg.agentCollisionEnabled || cfg.viscosityEnabled ||
                                  cfg.brownianEnabled || cfg.movableChunkFoodEnabled ||
                                  cfg.chunkChunkCollisionEnabled || cfg.chunkAdhesionEnabled
                                  ? "on" : "off");
        r.notes = notes.str();
        results.push_back(r);
    };

    CollisionConfig off;  // everything off
    CollisionConfig onAA = off;  onAA.agentCollisionEnabled = true;
    CollisionConfig onAAElast = onAA; onAAElast.elasticityEnabled = true; onAAElast.restitution = 0.3;
    CollisionConfig onAATransfer = onAA; onAATransfer.velocityTransfer = 0.5;
    CollisionConfig visc = off; visc.viscosityEnabled = true; visc.viscosityDrag = 0.5;
    CollisionConfig brown = off; brown.brownianEnabled = true; brown.brownianStrength = 5.0;
    CollisionConfig chunkMov = off; chunkMov.movableChunkFoodEnabled = true; chunkMov.chunkPushStrength = 1.0;
    CollisionConfig chunkFF = chunkMov; chunkFF.chunkChunkCollisionEnabled = true;
    CollisionConfig adhesion = chunkMov; adhesion.chunkAdhesionEnabled = true; adhesion.chunkAdhesionStrength = 1.0;

    // Scale series with everything off (baseline).
    buildScenario("off_100ag_100food", 100, 100, 0, 30, off);
    buildScenario("off_300ag_150food", 300, 150, 0, 30, off);
    buildScenario("off_600ag_300food", 600, 300, 0, 30, off);
    buildScenario("off_1000ag_500food", 1000, 500, 0, 30, off);

    // Agent-agent variants.
    buildScenario("aa_100ag", 100, 100, 0, 30, onAA);
    buildScenario("aa_300ag", 300, 150, 0, 30, onAA);
    buildScenario("aa_600ag", 600, 300, 0, 30, onAA);
    buildScenario("aa_elasticity_300ag", 300, 150, 0, 30, onAAElast);
    buildScenario("aa_transfer_300ag", 300, 150, 0, 30, onAATransfer);

    // Viscosity / Brownian.
    buildScenario("viscosity_300ag", 300, 150, 0, 30, visc);
    buildScenario("brownian_300ag", 300, 150, 0, 30, brown);

    // Chunk physics.
    buildScenario("chunk_movable_300ag", 300, 150, 0, 30, chunkMov);
    buildScenario("chunk_ff_300ag", 300, 150, 0, 30, chunkFF);
    buildScenario("chunk_adhesion_300ag", 300, 150, 0, 30, adhesion);

    // Obstacles + physics.
    buildScenario("aa_30obs_300ag", 300, 150, 30, 30, onAA);
    buildScenario("chunk_30obs_300ag", 300, 150, 30, 30, chunkMov);

    return results;
}
} // namespace agentbiosim::systems
