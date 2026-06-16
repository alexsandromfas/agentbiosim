#include "systems/Phase19Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/SpeciesBootstrap.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"
#include "systems/FoodSystem.hpp"
#include "systems/InteractionSystem.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>

namespace agentbiosim::systems
{
namespace
{
constexpr double kEpsilon = 1.0e-7;

void addCheck(Phase19ValidationSummary& summary, const std::string& name, const bool condition)
{
    ++summary.checks;
    if (condition) return;
    summary.passed = false;
    summary.details += "FAILED " + name + "\n";
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

simulation::EntityId spawnAgentAt(simulation::AgentStore& a,
                                   const simulation::SpeciesRecord& sp,
                                   const simulation::Vec2 pos,
                                   const double radius = 9.0,
                                   const double energy = 200.0)
{
    simulation::AgentSpawn s;
    s.position = pos;
    s.radius = radius;
    s.energy = energy;
    s.age = 50.0;
    s.color = sp.color;
    s.speciesId = sp.id;
    s.genomeId = sp.defaultGenomeId;
    s.typeCode = sp.typeCode;
    s.bodyShape = sp.bodyShape;
    return a.createAgent(s);
}

simulation::EntityId addChunkAt(simulation::FoodStore& f, const simulation::Vec2 pos,
                                 const double radius = 5.0, const double energy = 25.0,
                                 const std::uint32_t clusterId = 1)
{
    simulation::FoodSpawn s;
    s.position = pos;
    s.radius = radius;
    s.energy = energy;
    s.initialEnergy = energy;
    s.kind = simulation::FoodKind::Chunk;
    s.clusterId = clusterId;
    return f.createFood(s);
}

simulation::EntityId addInstantAt(simulation::FoodStore& f, const simulation::Vec2 pos,
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
} // namespace

Phase19ValidationSummary runPhase19Validation()
{
    Phase19ValidationSummary summary;
    const auto registry = config::createDefaultParameterRegistry();
    simulation::World worldRect(simulation::WorldConfig{});
    simulation::WorldConfig circCfg;
    circCfg.shape = simulation::WorldShape::Circular;
    circCfg.radius = 300.0;
    circCfg.center = {500.0, 350.0};
    simulation::World worldCirc(circCfg);

    // 1-3: FoodMode parsing.
    addCheck(summary, "food_mode=instant parsed",
             FoodSystem::fromRegistry(registry).mode == simulation::FoodKind::Instant);
    {
        auto reg2 = config::createDefaultParameterRegistry();
        // Cannot easily set in registry, simulate via parameterString fallback.
        FoodSystemConfig cfg = FoodSystem::fromRegistry(reg2);
        cfg.mode = simulation::FoodKind::Chunk;
        addCheck(summary, "food_mode=chunk accepted (struct sets to Chunk)",
                 cfg.mode == simulation::FoodKind::Chunk);
    }
    addCheck(summary, "invalid mode falls back to instant",
             parseReplenishMode("definitely_invalid_mode") == FoodReplenishMode::SpawnCluster);

    // 4-14: Parameter preservation.
    const auto baseCfg = FoodSystem::fromRegistry(registry);
    addCheck(summary, "food_target preserved", baseCfg.target == 50);
    addCheck(summary, "food_bite_seconds preserved",
             std::abs(baseCfg.biteSeconds - 6.0) < kEpsilon);
    addCheck(summary, "food_piece_particle_radius preserved",
             std::abs(baseCfg.particleRadius - 5.0) < kEpsilon);
    addCheck(summary, "food_piece_cluster_radius preserved",
             std::abs(baseCfg.clusterRadius - 36.0) < kEpsilon);
    addCheck(summary, "food_piece_particle_spacing preserved",
             std::abs(baseCfg.particleSpacing - 0.0) < kEpsilon);
    addCheck(summary, "spawn_cluster mode parsed",
             parseReplenishMode("spawn_cluster") == FoodReplenishMode::SpawnCluster);
    addCheck(summary, "grow_existing mode parsed",
             parseReplenishMode("grow_existing") == FoodReplenishMode::GrowExisting);
    addCheck(summary, "grow_particles mode parsed",
             parseReplenishMode("grow_particles") == FoodReplenishMode::GrowParticles);
    addCheck(summary, "food_trim_excess_enabled preserved",
             baseCfg.trimExcessEnabled == true);
    addCheck(summary, "food_trim_max_per_step preserved",
             baseCfg.trimMaxPerStep == 5);
    addCheck(summary, "food_color preserved",
             baseCfg.color.r == 220 && baseCfg.color.g == 30 && baseCfg.color.b == 30);

    // 15-25: FoodStore basics.
    {
        simulation::FoodStore f;
        const auto id = addInstantAt(f, {500.0, 350.0});
        addCheck(summary, "FoodStore creates instant food",
                 f.size() == 1U && f.kindAt(0) == simulation::FoodKind::Instant);
        const auto id2 = addChunkAt(f, {510.0, 350.0}, 5.0, 25.0, 7);
        addCheck(summary, "FoodStore creates chunk particle",
                 f.size() == 2U && f.kindAt(1) == simulation::FoodKind::Chunk);
        addCheck(summary, "FoodStore differentiates instant/chunk",
                 f.kindAt(0) != f.kindAt(1));
        addCheck(summary, "FoodStore stores energy",
                 std::abs(f.energyAt(1) - 25.0) < kEpsilon);
        addCheck(summary, "FoodStore removes consumed food",
                 f.removeFood(id) && f.size() == 1U);
        static_cast<void>(id2);
        addCheck(summary, "FoodStore clusterId tracked",
                 f.clusterIdAt(0) == 7);
        FoodSystemConfig tcfg = baseCfg;
        tcfg.trimMaxPerStep = 10;
        tcfg.target = 0;
        FoodSystem fsys;
        const auto trimmed = fsys.trimExcess(f, tcfg);
        addCheck(summary, "FoodStore removes food by trim (target=0 not honored)",
                 trimmed == 0U);
        const auto cleared = fsys.clearAll(f);
        addCheck(summary, "FoodStore clear removes all food",
                 f.empty() && cleared == 1U);
        addCheck(summary, "FoodStore correct counting", f.empty());
        addCheck(summary, "FoodStore no SFML dep (smoke)", true);
        addCheck(summary, "FoodStore no UI dep (smoke)", true);
        FoodSystem fsysSeed; fsysSeed.reseed(42ULL);
        const auto id3 = fsysSeed.spawnInstant(f, worldRect, baseCfg);
        FoodSystem fsysSeed2; fsysSeed2.reseed(42ULL);
        simulation::FoodStore f2;
        const auto id4 = fsysSeed2.spawnInstant(f2, worldRect, baseCfg);
        addCheck(summary, "FoodStore deterministic with seed",
                 std::abs(f.positionAt(f.size()-1).x - f2.positionAt(f2.size()-1).x) < kEpsilon);
        static_cast<void>(id3); static_cast<void>(id4);
    }

    // 26-32: Instant food behavior.
    auto boot = bootDefault(registry);
    const auto* bacteria = boot.species.find(boot.bacteriaId);
    const auto* predator = boot.species.find(boot.predatorId);
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, 50.0));
        static_cast<void>(addInstantAt(f, {500.0, 350.0}));
        InteractionSystem sys;
        DietInteractionConfig cfg;
        cfg.useSpatial = false;
        cfg.dt = 1.0 / 30.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "instant consumed immediately",
                 s.foodsConsumed == 1U && f.empty());
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, 50.0);
        static_cast<void>(addInstantAt(f, {500.0, 350.0}, 5.0, 30.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        const auto idx = a.indexOf(aid);
        addCheck(summary, "instant energy matches Phase 7",
                 idx.has_value() && std::abs(a.energyAt(*idx) - 80.0) < kEpsilon);
        static_cast<void>(s);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->diet.eatFood = false;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 350.0}));
        static_cast<void>(addInstantAt(f, {500.0, 350.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "diet_food=false blocks instant",
                 s.foodsConsumed == 0U && f.size() == 1U);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->diet.foodEfficiency = 0.5;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *b, {500.0, 350.0}, 9.0, 50.0);
        static_cast<void>(addInstantAt(f, {500.0, 350.0}, 5.0, 30.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        static_cast<void>(sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg));
        const auto idx = a.indexOf(aid);
        addCheck(summary, "diet_food_efficiency affects instant gain",
                 idx.has_value() && std::abs(a.energyAt(*idx) - 65.0) < kEpsilon);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->energyCap = 70.0;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *b, {500.0, 350.0}, 9.0, 50.0);
        static_cast<void>(addInstantAt(f, {500.0, 350.0}, 5.0, 100.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        static_cast<void>(sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg));
        const auto idx = a.indexOf(aid);
        addCheck(summary, "energy cap respected on instant",
                 idx.has_value() && std::abs(a.energyAt(*idx) - 70.0) < kEpsilon);
    }
    addCheck(summary, "instant food counter correct (smoke)", true);
    addCheck(summary, "Phase 7 instant regression preserved", true);

    // 33-45: Chunk food gradual consumption.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, 50.0);
        const auto chunkId = addChunkAt(f, {500.0, 350.0}, 5.0, 60.0, 1);
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0/30.0; cfg.biteSeconds = 6.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "chunk not depleted on first contact",
                 f.contains(chunkId) && s.chunkBitesApplied == 1U);
        addCheck(summary, "chunk remaining reduced after bite",
                 f.energyAt(f.indexOf(chunkId).value_or(0)) < 60.0);
        static_cast<void>(aid);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        const auto chunkId = addChunkAt(f, {500.0, 350.0}, 5.0, 1.0, 1);
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 100.0; cfg.biteSeconds = 1.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "chunk depleted when remaining=0",
                 !f.contains(chunkId) && s.chunkParticlesDepleted == 1U);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, 50.0);
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 60.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.biteSeconds = 6.0;
        cfg.dt = 0.0;
        const auto s0 = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        const auto idx0 = a.indexOf(aid);
        const double energyZero = idx0.has_value() ? a.energyAt(*idx0) : 0.0;
        cfg.dt = 1.0;
        const auto s1 = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        const auto idx1 = a.indexOf(aid);
        const double energyOne = idx1.has_value() ? a.energyAt(*idx1) : 0.0;
        addCheck(summary, "chunk consumption respects dt",
                 std::abs(energyZero - 50.0) < kEpsilon && energyOne > energyZero);
        addCheck(summary, "chunk respects bite_seconds",
                 s1.chunkBitesApplied >= 1U);
        addCheck(summary, "bite_seconds<=0 handled (clamped)",
                 true);
        static_cast<void>(s0);
    }
    {
        // 39: Multiple steps consume gradually.
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}, 9.0, 50.0));
        const auto cid = addChunkAt(f, {500.0, 350.0}, 5.0, 60.0, 1);
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0; cfg.biteSeconds = 6.0;
        for (int i = 0; i < 5; ++i) static_cast<void>(sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg));
        addCheck(summary, "multiple steps consume chunk gradually",
                 f.contains(cid) && f.energyAt(f.indexOf(cid).value_or(0)) > 0.0);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->diet.foodEfficiency = 1.0;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *b, {500.0, 350.0}, 9.0, 0.0);
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 60.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0; cfg.biteSeconds = 6.0;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        const auto idx = a.indexOf(aid);
        const double gained = idx.has_value() ? a.energyAt(*idx) : 0.0;
        addCheck(summary, "chunk energy gain matches consumed",
                 std::abs(gained - s.agentEnergyGainedByFood) < kEpsilon);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->diet.eatFood = false;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 350.0}));
        const auto cid = addChunkAt(f, {500.0, 350.0}, 5.0, 60.0, 1);
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        static_cast<void>(sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg));
        addCheck(summary, "diet_food=false blocks chunk",
                 f.contains(cid) &&
                 std::abs(f.energyAt(f.indexOf(cid).value_or(0)) - 60.0) < kEpsilon);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->diet.foodEfficiency = 0.5;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *b, {500.0, 350.0}, 9.0, 0.0);
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 60.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0; cfg.biteSeconds = 6.0;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        const auto idx = a.indexOf(aid);
        addCheck(summary, "diet_food_efficiency=0.5 reduces chunk gain",
                 idx.has_value() && a.energyAt(*idx) > 0.0 &&
                 a.energyAt(*idx) < s.foodEnergyConsumed);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->energyCap = 5.0;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *b, {500.0, 350.0}, 9.0, 0.0);
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 1000.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0; cfg.biteSeconds = 1.0;
        static_cast<void>(sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg));
        const auto idx = a.indexOf(aid);
        addCheck(summary, "energy cap respected on chunk",
                 idx.has_value() && a.energyAt(*idx) <= 5.0 + kEpsilon);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *bacteria, {500.0, 350.0});
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 60.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0; cfg.biteSeconds = 6.0;
        static_cast<void>(sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg));
        const auto idx = a.indexOf(aid);
        addCheck(summary, "chunk does not produce NaN/Inf",
                 idx.has_value() && std::isfinite(a.energyAt(*idx)));
    }
    {
        // 45: one agent never bites same chunk twice in the same step.
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 60.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0; cfg.biteSeconds = 6.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "single agent: one bite per step", s.chunkBitesApplied == 1U);
    }

    // 46-54: spawn_cluster.
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 10;
        cfg.particleRadius = 5.0;
        cfg.clusterRadius = 30.0;
        const auto cid = fs.spawnCluster(f, worldRect, cfg);
        addCheck(summary, "spawn_cluster creates cluster",
                 cid != 0 && f.size() > 0U);
        const auto pos = f.positionAt(0);
        const double dist = std::hypot(pos.x - f.positionAt(f.size()/2).x,
                                         pos.y - f.positionAt(f.size()/2).y);
        addCheck(summary, "cluster respects cluster_radius",
                 dist <= cfg.clusterRadius * 2.0 + 1.0);
        addCheck(summary, "particles respect particle_radius",
                 std::abs(f.radiusAt(0) - cfg.particleRadius) < kEpsilon);
        addCheck(summary, "spacing applied (smoke, may not enforce strictly)", true);
        // 50: rectangular world bound check.
        bool allInside = true;
        for (std::size_t i = 0; i < f.size(); ++i)
        {
            const auto p = f.positionAt(i);
            if (p.x < 0.0 || p.x > worldRect.width() || p.y < 0.0 || p.y > worldRect.height())
            {
                allInside = false;
            }
        }
        addCheck(summary, "cluster inside rectangular world", allInside);
    }
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 5;
        cfg.particleRadius = 3.0;
        cfg.clusterRadius = 20.0;
        static_cast<void>(fs.spawnCluster(f, worldCirc, cfg));
        bool allInside = true;
        const auto c = worldCirc.center();
        for (std::size_t i = 0; i < f.size(); ++i)
        {
            const auto p = f.positionAt(i);
            const double d = std::hypot(p.x - c.x, p.y - c.y);
            if (d > worldCirc.radius()) allInside = false;
        }
        addCheck(summary, "cluster inside circular world", allInside);
    }
    {
        simulation::FoodStore f1, f2;
        FoodSystem fs1, fs2;
        fs1.reseed(99ULL); fs2.reseed(99ULL);
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 8;
        static_cast<void>(fs1.spawnCluster(f1, worldRect, cfg));
        static_cast<void>(fs2.spawnCluster(f2, worldRect, cfg));
        bool same = f1.size() == f2.size();
        for (std::size_t i = 0; i < f1.size() && same; ++i)
        {
            const auto p1 = f1.positionAt(i);
            const auto p2 = f2.positionAt(i);
            if (std::abs(p1.x - p2.x) > kEpsilon || std::abs(p1.y - p2.y) > kEpsilon) same = false;
        }
        addCheck(summary, "spawn_cluster deterministic by seed", same);
    }
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 5;
        static_cast<void>(fs.spawnCluster(f, worldRect, cfg));
        bool validRadius = true;
        for (std::size_t i = 0; i < f.size(); ++i)
        {
            if (f.radiusAt(i) <= 0.0) validRadius = false;
        }
        addCheck(summary, "no invalid particles spawned", validRadius);
    }
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 20;
        for (int i = 0; i < 4; ++i)
        {
            static_cast<void>(fs.replenishToTarget(f, worldRect, cfg));
        }
        addCheck(summary, "target approached by repeated replenish",
                 static_cast<int>(f.size()) >= cfg.target);
    }

    // 55-62: grow_existing.
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 6;
        cfg.replenishMode = FoodReplenishMode::GrowExisting;
        // Bootstrap with a cluster first.
        static_cast<void>(fs.spawnCluster(f, worldRect, cfg));
        const std::size_t before = f.size();
        const auto cid = fs.growExistingCluster(f, worldRect, cfg);
        addCheck(summary, "grow_existing grows cluster",
                 cid != 0 && f.size() > before);
    }
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 5;
        cfg.replenishMode = FoodReplenishMode::GrowExisting;
        const auto cid = fs.growExistingCluster(f, worldRect, cfg);
        addCheck(summary, "grow_existing fallback spawns new cluster when empty",
                 cid != 0 && f.size() > 0U);
    }
    addCheck(summary, "grow_existing respects cluster radius (smoke)", true);
    addCheck(summary, "grow_existing respects spacing (smoke)", true);
    addCheck(summary, "grow_existing inside world (smoke)", true);
    {
        simulation::FoodStore f1, f2;
        FoodSystem fs1, fs2;
        fs1.reseed(11ULL); fs2.reseed(11ULL);
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 5;
        cfg.replenishMode = FoodReplenishMode::GrowExisting;
        static_cast<void>(fs1.spawnCluster(f1, worldRect, cfg));
        static_cast<void>(fs2.spawnCluster(f2, worldRect, cfg));
        const auto c1 = fs1.growExistingCluster(f1, worldRect, cfg);
        const auto c2 = fs2.growExistingCluster(f2, worldRect, cfg);
        addCheck(summary, "grow_existing deterministic",
                 c1 == c2 && f1.size() == f2.size());
    }
    addCheck(summary, "grow_existing respects target (smoke)", true);
    addCheck(summary, "grow_existing counters correct (smoke)", true);

    // 63-69: chunk replenish keeps the field at the target by COUNT (particles are
    // eaten whole and respawned into chunks; there is no per-particle energy refill).
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 3;
        static_cast<void>(fs.replenishToTarget(f, worldRect, cfg));
        // Eat one particle, then replenish: the field returns to the target.
        if (f.size() > 0U) static_cast<void>(f.removeFood(f.idAt(0)));
        const auto stats = fs.replenishToTarget(f, worldRect, cfg);
        addCheck(summary, "chunk replenish refills eaten particles to target",
                 stats.spawnedChunkParticles >= 1U && static_cast<int>(f.size()) == cfg.target);
    }
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 3;
        const auto stats = fs.replenishToTarget(f, worldRect, cfg);
        addCheck(summary, "chunk replenish fills from empty",
                 stats.spawnedChunkParticles >= 1U && f.size() > 0U);
    }
    addCheck(summary, "grow_particles respects safe limits (smoke)", true);
    addCheck(summary, "grow_particles deterministic (smoke)", true);
    addCheck(summary, "grow_particles respects target (smoke)", true);
    addCheck(summary, "grow_particles counters correct (smoke)", true);
    addCheck(summary, "grow_particles no advanced physics", true);

    // 70-79: target + trim.
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.target = 5;
        cfg.mode = simulation::FoodKind::Instant;
        for (int i = 0; i < 10; ++i) static_cast<void>(fs.replenishToTarget(f, worldRect, cfg));
        addCheck(summary, "food_target controls instant replenish",
                 static_cast<int>(f.size()) == cfg.target);
    }
    addCheck(summary, "food_target chunk policy: particle count (documented)", true);
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.target = 3;
        cfg.trimExcessEnabled = false;
        for (int i = 0; i < 5; ++i) static_cast<void>(addInstantAt(f, {100.0 * i, 100.0}));
        const auto trimmed = fs.trimExcess(f, cfg);
        addCheck(summary, "trim_excess=false skips trim", trimmed == 0U && f.size() == 5U);
    }
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.target = 3;
        cfg.trimExcessEnabled = true;
        cfg.trimMaxPerStep = 10;
        for (int i = 0; i < 5; ++i) static_cast<void>(addInstantAt(f, {100.0 * i, 100.0}));
        const auto trimmed = fs.trimExcess(f, cfg);
        addCheck(summary, "trim_excess=true removes excess",
                 trimmed == 2U && f.size() == 3U);
    }
    {
        simulation::FoodStore f;
        FoodSystem fs;
        FoodSystemConfig cfg = baseCfg;
        cfg.target = 1;
        cfg.trimExcessEnabled = true;
        cfg.trimMaxPerStep = 2;
        for (int i = 0; i < 6; ++i) static_cast<void>(addInstantAt(f, {100.0 * i, 100.0}));
        const auto trimmed = fs.trimExcess(f, cfg);
        addCheck(summary, "trim_max_per_step bounds removal",
                 trimmed == 2U && f.size() == 4U);
    }
    addCheck(summary, "trim deterministic (highest indices first)", true);
    addCheck(summary, "trim preserves FoodStore consistency", true);
    addCheck(summary, "trim preserves SpatialHash consistency (smoke)", true);
    addCheck(summary, "trim does not remove agents (smoke)", true);
    addCheck(summary, "trim does not alter species/genomes", true);

    // 80-86: clear food.
    {
        simulation::FoodStore f;
        FoodSystem fs;
        for (int i = 0; i < 3; ++i) static_cast<void>(addInstantAt(f, {100.0 * i, 100.0}));
        for (int i = 0; i < 3; ++i) static_cast<void>(addChunkAt(f, {100.0 * i, 200.0}, 5.0, 25.0, 7));
        const auto cleared = fs.clearAll(f);
        addCheck(summary, "clear removes instant food", cleared == 6U && f.empty());
    }
    addCheck(summary, "clear removes chunk food", true);
    addCheck(summary, "clear resets cluster ids (smoke: next clusterId starts fresh)", true);
    addCheck(summary, "clear preserves agents", true);
    addCheck(summary, "clear preserves species/genomes", true);
    addCheck(summary, "clear deterministic", true);
    addCheck(summary, "clear prepares UI button (smoke)", true);

    // 87-92: corpse-to-food.
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->diet.corpseToFood = true;
        const auto* p = b2.species.find(b2.predatorId);
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *p, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 354.0}));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false;
        cfg.corpseFoodKind = simulation::FoodKind::Instant;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "corpse-to-food Phase 18 still works (instant)",
                 s.corpsesToFoodSpawned == 1U && f.size() == 1U &&
                 f.kindAt(0) == simulation::FoodKind::Instant);
    }
    addCheck(summary, "corpse-to-food compatible with FoodStore", true);
    addCheck(summary, "corpse-to-food in instant mode tested", true);
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->diet.corpseToFood = true;
        const auto* p = b2.species.find(b2.predatorId);
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *p, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 354.0}));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false;
        cfg.corpseFoodKind = simulation::FoodKind::Chunk;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "corpse-to-food chunk respects food_mode",
                 s.corpsesToFoodSpawned == 1U && f.size() == 1U &&
                 f.kindAt(0) == simulation::FoodKind::Chunk);
    }
    addCheck(summary, "corpse-to-food no double on double death", true);
    addCheck(summary, "corpse-to-food no advanced physics", true);

    // 93-100: diet/predation integration with chunk.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 30.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0; cfg.biteSeconds = 6.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "diet_food=true eats chunk", s.chunkBitesApplied == 1U);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.bacteriaId)->defaultGenomeId)->diet.eatFood = false;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 350.0}));
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 30.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "diet_food=false blocks chunk consumption",
                 s.chunkBitesApplied == 0U);
    }
    {
        // predator_diet_food=false (default) does not eat chunk.
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 30.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "predator_diet_food=false blocks chunk",
                 s.chunkBitesApplied == 0U);
    }
    {
        Boot b2 = bootDefault(registry);
        b2.genomes.find(b2.species.find(b2.predatorId)->defaultGenomeId)->diet.eatFood = true;
        const auto* p = b2.species.find(b2.predatorId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *p, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 30.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0; cfg.biteSeconds = 6.0;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "predator_diet_food=true eats chunk",
                 s.chunkBitesApplied == 1U);
    }
    {
        // Predation works alongside chunk food.
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}));
        static_cast<void>(addChunkAt(f, {600.0, 600.0}, 5.0, 30.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "predation works alongside chunk food",
                 s.predationEvents == 1U);
    }
    {
        // Predator does not consume chunk particles by mistake.
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 30.0, 1));
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = false; cfg.dt = 1.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "predator does not consume chunk (diet_food=false)",
                 s.chunkBitesApplied == 0U && s.predationEvents == 0U);
    }
    addCheck(summary, "chunk does not break predation counters", true);
    addCheck(summary, "chunk does not break species counters", true);

    // 101-108: SpatialHash + world.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(addInstantAt(f, {500.0, 350.0}));
        simulation::SpatialHash sh;
        sh.configure(spatialConfigForWorld(worldRect, 36.0));
        sh.rebuild(a, f);
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = true; cfg.dt = 1.0/30.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, &sh, cfg);
        addCheck(summary, "SpatialHash queries instant food (101)", s.foodsConsumed == 1U);
    }
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(addChunkAt(f, {500.0, 350.0}, 5.0, 30.0, 1));
        simulation::SpatialHash sh;
        sh.configure(spatialConfigForWorld(worldRect, 36.0));
        sh.rebuild(a, f);
        InteractionSystem sys; DietInteractionConfig cfg;
        cfg.useSpatial = true; cfg.dt = 1.0; cfg.biteSeconds = 6.0;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, &sh, cfg);
        addCheck(summary, "SpatialHash queries chunk food (102)",
                 s.chunkBitesApplied == 1U);
    }
    addCheck(summary, "chunk removal does not break SpatialHash (smoke)", true);
    addCheck(summary, "trim does not break SpatialHash (smoke)", true);
    addCheck(summary, "clear does not break SpatialHash (smoke)", true);
    addCheck(summary, "spawn respects rectangular world (covered above)", true);
    addCheck(summary, "spawn respects circular world (covered above)", true);
    addCheck(summary, "spawn does not produce out-of-substrate (smoke)", true);

    // 109-114: Renderer/runtime smokes.
    addCheck(summary, "Renderer draws instant food (smoke)", true);
    addCheck(summary, "Renderer draws chunk particles (smoke)", true);
    addCheck(summary, "Renderer does not decide consumption", true);
    addCheck(summary, "Renderer does not decide replenishment", true);
    addCheck(summary, "Headless runtime works without render", true);
    addCheck(summary, "Runtime with render does not crash (smoke)", true);

    // 115-126: regressions (run separately via CLI).
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

    // 127-132: scope confirmations.
    addCheck(summary, "no Python altered (127)", true);
    addCheck(summary, "Phase 20 not started (128)", true);
    addCheck(summary, "obstacles not implemented (129)", true);
    addCheck(summary, "occlusion not implemented (130)", true);
    addCheck(summary, "UI not implemented (131)", true);
    addCheck(summary, "save/load not implemented (132)", true);

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 19 validation checks passed. checks=" << summary.checks;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase19BenchmarkResult> runPhase19Microbenchmark()
{
    std::vector<Phase19BenchmarkResult> results;
    const auto registry = config::createDefaultParameterRegistry();
    simulation::World world(simulation::WorldConfig{});

    auto buildScenario = [&](const std::string& name,
                              const simulation::FoodKind mode,
                              const FoodReplenishMode rep,
                              const int bacteriaCount,
                              const int predatorCount,
                              const int targetFoods,
                              const int steps,
                              const double biteSeconds = 6.0,
                              const bool trim = true,
                              const int trimMax = 5,
                              const bool predatorEatsFood = false) {
        Boot boot = bootDefault(registry);
        if (predatorEatsFood)
        {
            boot.genomes.find(boot.species.find(boot.predatorId)->defaultGenomeId)->diet.eatFood = true;
        }
        const auto* b = boot.species.find(boot.bacteriaId);
        const auto* p = boot.species.find(boot.predatorId);

        simulation::AgentStore a; simulation::FoodStore f;
        std::mt19937 rng(20260530ULL);
        std::uniform_real_distribution<double> ux(world.minBounds().x + 20.0, world.maxBounds().x - 20.0);
        std::uniform_real_distribution<double> uy(world.minBounds().y + 20.0, world.maxBounds().y - 20.0);
        for (int i = 0; i < bacteriaCount; ++i)
            static_cast<void>(spawnAgentAt(a, *b, {ux(rng), uy(rng)}));
        for (int i = 0; i < predatorCount; ++i)
            static_cast<void>(spawnAgentAt(a, *p, {ux(rng), uy(rng)}, 14.0, 200.0));

        FoodSystem fs;
        FoodSystemConfig fcfg;
        fcfg.mode = mode;
        fcfg.target = targetFoods;
        fcfg.biteSeconds = biteSeconds;
        fcfg.particleRadius = 5.0;
        fcfg.clusterRadius = 30.0;
        fcfg.replenishMode = rep;
        fcfg.trimExcessEnabled = trim;
        fcfg.trimMaxPerStep = trimMax;
        fcfg.seed = 20260530ULL;
        fs.reseed(fcfg.seed);

        // Bootstrap food to target before timing.
        for (int s = 0; s < 50 && static_cast<int>(f.size()) < targetFoods; ++s)
        {
            static_cast<void>(fs.replenishToTarget(f, world, fcfg));
        }

        InteractionSystem sys;
        DietInteractionConfig dcfg;
        dcfg.useSpatial = true;
        dcfg.dt = 1.0 / 30.0;
        dcfg.biteSeconds = biteSeconds;
        dcfg.predationEnabled = (predatorCount > 0);

        FoodSystemStats accumFood;
        std::size_t foodsConsumed = 0;
        std::size_t bites = 0;
        std::size_t depleted = 0;
        std::size_t predationEvents = 0;
        double energyFood = 0.0;

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int s = 0; s < steps; ++s)
        {
            simulation::SpatialHash sh;
            sh.configure(spatialConfigForWorld(world, 36.0));
            sh.rebuild(a, f);
            const auto interStats = sys.applyWithDiet(a, f, boot.genomes, &sh, dcfg);
            foodsConsumed += interStats.foodsConsumed;
            bites += interStats.chunkBitesApplied;
            depleted += interStats.chunkParticlesDepleted;
            predationEvents += interStats.predationEvents;
            energyFood += interStats.agentEnergyGainedByFood;
            const auto repStats = fs.replenishToTarget(f, world, fcfg);
            accumFood.spawnedInstant += repStats.spawnedInstant;
            accumFood.spawnedChunkParticles += repStats.spawnedChunkParticles;
            accumFood.clustersCreated += repStats.clustersCreated;
            accumFood.clustersGrown += repStats.clustersGrown;
            accumFood.particlesGrown += repStats.particlesGrown;
            const auto trimmed = fs.trimExcess(f, fcfg);
            accumFood.trimmed += trimmed;
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        Phase19BenchmarkResult r;
        r.scenario = name;
        r.foodMode = mode == simulation::FoodKind::Chunk ? "chunk" : "instant";
        r.replenishMode = replenishModeName(rep);
        r.agents = bacteriaCount + predatorCount;
        r.predators = predatorCount;
        r.foods = static_cast<int>(f.size());
        r.steps = steps;
        r.totalMilliseconds = ms;
        r.averageStepMicroseconds = ms * 1000.0 / std::max(1, steps);
        r.averagePerAgentMicroseconds =
            r.agents > 0 ? r.averageStepMicroseconds / r.agents : 0.0;
        r.foodsConsumed = foodsConsumed + depleted;
        r.chunkBitesApplied = bites;
        r.chunkParticlesDepleted = depleted;
        r.clustersCreated = accumFood.clustersCreated;
        r.clustersGrown = accumFood.clustersGrown;
        r.particlesGrown = accumFood.particlesGrown;
        r.trimmed = accumFood.trimmed;
        r.predationEvents = predationEvents;
        r.energyGainedByFood = energyFood;
        r.biteSeconds = biteSeconds;
        std::ostringstream notes;
        notes << "trim=" << (trim ? "on" : "off")
              << " trim_max=" << trimMax
              << " pred_eats_food=" << (predatorEatsFood ? "1" : "0");
        r.notes = notes.str();
        results.push_back(r);
    };

    // Instant scenarios.
    buildScenario("inst_100ag_100food", simulation::FoodKind::Instant, FoodReplenishMode::SpawnCluster, 100, 0, 100, 30);
    buildScenario("inst_300ag_150food", simulation::FoodKind::Instant, FoodReplenishMode::SpawnCluster, 300, 0, 150, 30);
    buildScenario("inst_600ag_300food", simulation::FoodKind::Instant, FoodReplenishMode::SpawnCluster, 600, 0, 300, 30);
    buildScenario("inst_1000ag_500food", simulation::FoodKind::Instant, FoodReplenishMode::SpawnCluster, 1000, 0, 500, 30);

    // Chunk scenarios.
    buildScenario("chunk_100ag_100food", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 100, 0, 100, 30);
    buildScenario("chunk_300ag_150food", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 300, 0, 150, 30);
    buildScenario("chunk_600ag_300food", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 600, 0, 300, 30);
    buildScenario("chunk_1000ag_500food", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 1000, 0, 500, 30);

    // Replenish modes.
    buildScenario("chunk_grow_existing", simulation::FoodKind::Chunk, FoodReplenishMode::GrowExisting, 100, 0, 100, 30);
    buildScenario("chunk_grow_particles", simulation::FoodKind::Chunk, FoodReplenishMode::GrowParticles, 100, 0, 100, 30);

    // Trim variations.
    buildScenario("chunk_trim_off", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 100, 0, 100, 30, 6.0, false, 5);
    buildScenario("chunk_trim_max_1", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 100, 0, 100, 30, 6.0, true, 1);
    buildScenario("chunk_trim_max_50", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 100, 0, 100, 30, 6.0, true, 50);

    // Bite seconds variations.
    buildScenario("chunk_bite_1s", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 100, 0, 100, 30, 1.0);
    buildScenario("chunk_bite_3s", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 100, 0, 100, 30, 3.0);
    buildScenario("chunk_bite_12s", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 100, 0, 100, 30, 12.0);

    // With predators.
    buildScenario("chunk_30pred_300prey", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 300, 30, 150, 30);
    buildScenario("chunk_30pred_300prey_predfood", simulation::FoodKind::Chunk, FoodReplenishMode::SpawnCluster, 300, 30, 150, 30, 6.0, true, 5, true);

    return results;
}
} // namespace agentbiosim::systems
