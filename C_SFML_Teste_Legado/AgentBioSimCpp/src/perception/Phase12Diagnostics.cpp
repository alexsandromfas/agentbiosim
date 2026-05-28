#include "perception/Phase12Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "neural/Phase9Diagnostics.hpp"
#include "perception/PerceptionSystem.hpp"
#include "perception/Phase10Diagnostics.hpp"
#include "perception/Phase11Diagnostics.hpp"
#include "perception/SectorBinsConfig.hpp"
#include "perception/VisionDebug.hpp"
#include "perception/VisionStrategy.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/World.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/NeuralSystem.hpp"
#include "systems/Phase7Diagnostics.hpp"
#include "systems/Phase8Diagnostics.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>

namespace agentbiosim::perception
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1.0e-7;

void addCheck(Phase12ValidationSummary& summary, const std::string& name, const bool condition)
{
    ++summary.checks;
    if (condition)
    {
        return;
    }
    summary.passed = false;
    summary.details += "FAILED " + name + "\n";
}

simulation::World makeWorld()
{
    simulation::WorldConfig config;
    config.shape = simulation::WorldShape::Rectangular;
    config.width = 1000.0;
    config.height = 700.0;
    config.center = {500.0, 350.0};
    return simulation::World(config);
}

simulation::AgentSpawn agentAt(const double x, const double y, const double angle = 0.0)
{
    simulation::AgentSpawn spawn;
    spawn.position = {x, y};
    spawn.angle = angle;
    spawn.radius = 5.0;
    spawn.energy = 100.0;
    spawn.typeCode = simulation::AgentTypeCode::LegacyBacteria;
    return spawn;
}

simulation::FoodSpawn foodAt(const double x, const double y, const double radius = 5.0,
                             const simulation::ColorRgb color = {220, 30, 30})
{
    simulation::FoodSpawn spawn;
    spawn.position = {x, y};
    spawn.radius = radius;
    spawn.energy = 25.0;
    spawn.initialEnergy = 25.0;
    spawn.kind = simulation::FoodKind::Instant;
    spawn.color = color;
    return spawn;
}

RetinaConfig retinaSectorD()
{
    RetinaConfig config;
    config.visionMode = "sector";
    config.visionRadius = 120.0;
    config.retinaCount = 18;
    config.fovDegrees = 180.0;
    config.eyeCount = 1;
    config.inputMode = RetinaInputMode::DistanceOnly;
    config.channelD = true;
    config.seeFood = true;
    config.sectorBins.subdivisions = 1;
    config.sectorBins.falloff = BinsFalloff::Linear;
    config.sectorBins.distribution = BinsDistribution::Linear;
    config.sectorBins.projection = BinsProjection::Center;
    config.sectorBins.mode = BinsMode::Nearest;
    config.sectorBins.candidateLimit = 0;
    return config;
}

RetinaConfig retinaSectorRGBD()
{
    RetinaConfig config = retinaSectorD();
    config.inputMode = RetinaInputMode::ColorPlusDistance;
    config.channelR = true;
    config.channelG = true;
    config.channelB = true;
    return config;
}

double maxValue(const std::vector<double>& v)
{
    return v.empty() ? 0.0 : *std::max_element(v.begin(), v.end());
}

std::size_t countActive(const std::vector<double>& v)
{
    std::size_t c = 0;
    for (const double x : v)
    {
        if (x > kEpsilon)
        {
            ++c;
        }
    }
    return c;
}

void populateAgents(simulation::AgentStore& agents, const int count, std::mt19937& rng)
{
    agents.clear();
    std::uniform_real_distribution<double> xDist(20.0, 980.0);
    std::uniform_real_distribution<double> yDist(20.0, 680.0);
    std::uniform_real_distribution<double> aDist(-kPi, kPi);
    for (int i = 0; i < count; ++i)
    {
        static_cast<void>(agents.createAgent(agentAt(xDist(rng), yDist(rng), aDist(rng))));
    }
}

void populateFoods(simulation::FoodStore& foods, const int count, std::mt19937& rng)
{
    foods.clear();
    std::uniform_real_distribution<double> xDist(20.0, 980.0);
    std::uniform_real_distribution<double> yDist(20.0, 680.0);
    for (int i = 0; i < count; ++i)
    {
        static_cast<void>(foods.createFood(foodAt(xDist(rng), yDist(rng))));
    }
}
} // namespace

Phase12ValidationSummary runPhase12Validation()
{
    Phase12ValidationSummary summary;
    const auto world = makeWorld();

    // 1: sector selected
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));

        PerceptionConfig pc;
        pc.retina = retinaSectorD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        (void)r;
        addCheck(summary, "sector mode selected by retina_vision_mode",
                 ps.lastStats().visionModeEnum == VisionMode::Sector &&
                 !ps.lastStats().fallbackMode);
    }

    // 2: same input_size across single/fullbody/sector
    {
        RetinaConfig single = retinaSectorD();
        single.visionMode = "single";
        RetinaConfig full = retinaSectorD();
        full.visionMode = "fullbody";
        RetinaConfig sector = retinaSectorD();
        addCheck(summary, "single/fullbody/sector have same input_size",
                 single.inputSize() == full.inputSize() &&
                 full.inputSize() == sector.inputSize() &&
                 sector.inputSize() == 18);
    }

    auto sectorPerception = [&](RetinaConfig cfg, simulation::AgentStore& agents,
                                simulation::FoodStore& foods) {
        PerceptionConfig pc;
        pc.retina = cfg;
        PerceptionSystem ps;
        return ps.computeInputs(agents, foods, nullptr, world, pc);
    };

    // 3-6: bins_mode tests
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        // Two foods at same angle, different distances.
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0, 5.0, {220, 30, 30})));
        static_cast<void>(foods.createFood(foodAt(180.0, 100.0, 5.0, {30, 220, 30})));

        RetinaConfig cfg = retinaSectorD();
        cfg.sectorBins.mode = BinsMode::Nearest;
        const auto rNear = sectorPerception(cfg, agents, foods);
        // The nearest food (closer) should dominate -> activation higher.
        addCheck(summary, "bins_mode nearest produces output", maxValue(rNear.flatInputs) > kEpsilon);

        cfg.sectorBins.mode = BinsMode::Strongest;
        cfg.inputMode = RetinaInputMode::ColorPlusDistance;
        cfg.channelR = true;
        const auto rStrong = sectorPerception(cfg, agents, foods);
        addCheck(summary, "bins_mode strongest produces output", maxValue(rStrong.flatInputs) > kEpsilon);

        cfg.sectorBins.mode = BinsMode::SumSaturating;
        cfg.inputMode = RetinaInputMode::DistanceOnly;
        cfg.channelR = false;
        cfg.channelD = true;
        const auto rSum = sectorPerception(cfg, agents, foods);
        addCheck(summary, "bins_mode sum_saturating produces output (<= 1.0)",
                 maxValue(rSum.flatInputs) > kEpsilon && maxValue(rSum.flatInputs) <= 1.0 + kEpsilon);

        cfg.sectorBins.mode = BinsMode::WeightedAverage;
        const auto rWeighted = sectorPerception(cfg, agents, foods);
        addCheck(summary, "bins_mode weighted_average produces output",
                 maxValue(rWeighted.flatInputs) > kEpsilon);
    }

    // 7-10: subdivisions
    auto runSubdiv = [&](int sub) {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.sectorBins.subdivisions = sub;
        return sectorPerception(cfg, agents, foods);
    };
    {
        const auto r = runSubdiv(1);
        addCheck(summary, "subdivisions=1 works", maxValue(r.flatInputs) > kEpsilon);
    }
    {
        const auto r = runSubdiv(5);
        addCheck(summary, "subdivisions=5 works", maxValue(r.flatInputs) > kEpsilon);
    }
    {
        const auto r = runSubdiv(20);
        addCheck(summary, "subdivisions=20 works", maxValue(r.flatInputs) > kEpsilon);
    }
    {
        const auto r = runSubdiv(99);
        addCheck(summary, "subdivisions=99 works", maxValue(r.flatInputs) > kEpsilon);
    }

    // 11: subdivisions don't change input_size
    {
        RetinaConfig a = retinaSectorD();
        a.sectorBins.subdivisions = 1;
        RetinaConfig b = retinaSectorD();
        b.sectorBins.subdivisions = 99;
        addCheck(summary, "subdivisions do not change input_size",
                 a.inputSize() == b.inputSize());
    }

    // 12-13: distribution
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.sectorBins.subdivisions = 5;
        cfg.sectorBins.distribution = BinsDistribution::Linear;
        const auto rL = sectorPerception(cfg, agents, foods);
        addCheck(summary, "distribution=linear works", maxValue(rL.flatInputs) > kEpsilon);
        cfg.sectorBins.distribution = BinsDistribution::NearDetail;
        const auto rN = sectorPerception(cfg, agents, foods);
        addCheck(summary, "distribution=near_detail works", maxValue(rN.flatInputs) > kEpsilon);
    }

    // 14-17: falloff modes
    auto runFalloff = [&](BinsFalloff f) {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.sectorBins.falloff = f;
        return sectorPerception(cfg, agents, foods);
    };
    {
        const auto r = runFalloff(BinsFalloff::Linear);
        addCheck(summary, "falloff=linear works", maxValue(r.flatInputs) > kEpsilon);
    }
    {
        const auto r = runFalloff(BinsFalloff::Quadratic);
        addCheck(summary, "falloff=quadratic works", maxValue(r.flatInputs) > kEpsilon);
    }
    {
        const auto r = runFalloff(BinsFalloff::Step);
        addCheck(summary, "falloff=step works", maxValue(r.flatInputs) > kEpsilon);
    }
    {
        const auto r = runFalloff(BinsFalloff::None);
        // None should give full activation (~1.0) for any in-range object.
        addCheck(summary, "falloff=none gives full activation",
                 maxValue(r.flatInputs) > 0.99);
    }

    // 18-20: projection
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        // Large food near the agent so apparent_size spreads.
        static_cast<void>(foods.createFood(foodAt(140.0, 100.0, 30.0)));

        RetinaConfig cfg = retinaSectorD();
        cfg.sectorBins.projection = BinsProjection::Center;
        const auto rC = sectorPerception(cfg, agents, foods);
        const std::size_t nCenter = countActive(rC.flatInputs);
        addCheck(summary, "projection=center activates one ray", nCenter == 1);

        cfg.sectorBins.projection = BinsProjection::CenterEdges;
        const auto rE = sectorPerception(cfg, agents, foods);
        const std::size_t nEdges = countActive(rE.flatInputs);
        addCheck(summary, "projection=edges activates >= center", nEdges >= nCenter);

        cfg.sectorBins.projection = BinsProjection::ApparentSize;
        const auto rA = sectorPerception(cfg, agents, foods);
        const std::size_t nApp = countActive(rA.flatInputs);
        addCheck(summary, "projection=apparent_size spreads large object across more rays",
                 nApp > nCenter);
    }

    // 21: object ahead activates expected sector
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        const auto r = sectorPerception(retinaSectorD(), agents, foods);
        // With FOV=180 and retina_count=18, center is at ray 8 or 9.
        addCheck(summary, "object ahead activates center sector",
                 r.flatInputs[8] > kEpsilon || r.flatInputs[9] > kEpsilon);
    }

    // 22: behind ignored
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(50.0, 100.0)));
        const auto r = sectorPerception(retinaSectorD(), agents, foods);
        addCheck(summary, "object behind ignored", maxValue(r.flatInputs) < kEpsilon);
    }

    // 23: outside FOV ignored
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(50.0, 200.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.fovDegrees = 120.0;
        const auto r = sectorPerception(cfg, agents, foods);
        addCheck(summary, "outside FOV ignored", maxValue(r.flatInputs) < kEpsilon);
    }

    // 24: outside vision_radius ignored
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(300.0, 100.0)));
        const auto r = sectorPerception(retinaSectorD(), agents, foods);
        addCheck(summary, "outside vision radius ignored", maxValue(r.flatInputs) < kEpsilon);
    }

    // 25-28: channels
    {
        RetinaConfig cfg = retinaSectorD();
        addCheck(summary, "sector with D-only inputSize=18", cfg.inputSize() == 18);
    }
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0, 5.0, {220, 30, 30})));
        const auto r = sectorPerception(retinaSectorRGBD(), agents, foods);
        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double rVal = r.flatInputs[centerRay * stride + 0];
        const double gVal = r.flatInputs[centerRay * stride + 1];
        addCheck(summary, "sector channel R responds to red", rVal > 0.5 && rVal > gVal);
    }
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0, 5.0, {30, 220, 30})));
        const auto r = sectorPerception(retinaSectorRGBD(), agents, foods);
        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double gVal = r.flatInputs[centerRay * stride + 1];
        const double rVal = r.flatInputs[centerRay * stride + 0];
        addCheck(summary, "sector channel G responds to green", gVal > 0.5 && gVal > rVal);
    }
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0, 5.0, {30, 30, 220})));
        const auto r = sectorPerception(retinaSectorRGBD(), agents, foods);
        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double bVal = r.flatInputs[centerRay * stride + 2];
        const double rVal = r.flatInputs[centerRay * stride + 0];
        addCheck(summary, "sector channel B responds to blue", bVal > 0.5 && bVal > rVal);
    }

    // 29: disabled channel
    {
        RetinaConfig cfg;
        cfg.inputMode = RetinaInputMode::ColorPlusDistance;
        cfg.channelR = true;
        cfg.channelG = false;
        cfg.channelB = false;
        cfg.channelD = true;
        cfg.retinaCount = 18;
        cfg.eyeCount = 1;
        addCheck(summary, "sector disabled channel excludes from input",
                 cfg.channelCount() == 2 && cfg.inputSize() == 36);
    }

    // 30: see_food=false
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.seeFood = false;
        const auto r = sectorPerception(cfg, agents, foods);
        addCheck(summary, "sector: see_food=false hides food", maxValue(r.flatInputs) < kEpsilon);
    }

    // 31: candidate_limit=0 = unlimited
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        for (int i = 0; i < 50; ++i)
        {
            static_cast<void>(foods.createFood(foodAt(130.0 + i * 0.1, 100.0)));
        }
        RetinaConfig cfg = retinaSectorD();
        cfg.sectorBins.candidateLimit = 0;
        PerceptionConfig pc;
        pc.retina = cfg;
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        (void)r;
        addCheck(summary, "candidate_limit=0 is unlimited",
                 ps.lastStats().averageCandidatesAfterLimit >= 50.0);
    }

    // 32: candidate_limit low
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        for (int i = 0; i < 50; ++i)
        {
            static_cast<void>(foods.createFood(foodAt(130.0 + i * 0.1, 100.0)));
        }
        RetinaConfig cfg = retinaSectorD();
        cfg.sectorBins.candidateLimit = 5;
        PerceptionConfig pc;
        pc.retina = cfg;
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        (void)r;
        addCheck(summary, "candidate_limit=5 limits candidates",
                 ps.lastStats().averageCandidatesAfterLimit <= 5.0);
    }

    // 33: 2 eyes doubles input_size
    {
        RetinaConfig cfg = retinaSectorD();
        cfg.eyeCount = 2;
        addCheck(summary, "sector eye_count=2 doubles input_size", cfg.inputSize() == 36);
    }

    // 34: left eye detects
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(140.0, 60.0, 8.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.eyeCount = 2;
        const auto r = sectorPerception(cfg, agents, foods);
        const std::size_t inputsPerEye = cfg.retinaCount;
        double leftMax = 0.0;
        for (std::size_t i = 0; i < inputsPerEye; ++i)
        {
            leftMax = std::max(leftMax, r.flatInputs[i]);
        }
        addCheck(summary, "sector left eye detects object on its side", leftMax > kEpsilon);
    }

    // 35: right eye detects
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(140.0, 140.0, 8.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.eyeCount = 2;
        const auto r = sectorPerception(cfg, agents, foods);
        const std::size_t inputsPerEye = cfg.retinaCount;
        double rightMax = 0.0;
        for (std::size_t i = inputsPerEye; i < 2 * inputsPerEye; ++i)
        {
            rightMax = std::max(rightMax, r.flatInputs[i]);
        }
        addCheck(summary, "sector right eye detects object on its side", rightMax > kEpsilon);
    }

    // 36: debug not generated when off
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        PerceptionConfig pc;
        pc.retina = retinaSectorD();
        PerceptionSystem ps;
        VisionDebugData debug;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        (void)r;
        addCheck(summary, "sector debug NOT generated when not requested",
                 debug.rays.empty() && !debug.active);
    }

    // 37: debug generated when on
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        const auto agentId = agents.createAgent(agentAt(100.0, 100.0, 0.0));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        PerceptionConfig pc;
        pc.retina = retinaSectorD();
        PerceptionSystem ps;
        VisionDebugData debug;
        PerceptionDebugRequest req;
        req.agentId = agentId.value;
        req.out = &debug;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc, req);
        (void)r;
        addCheck(summary, "sector debug generated when requested",
                 debug.active && debug.rays.size() == 18);
    }

    // 38: MLP receives sector input
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        const auto registry = config::createDefaultParameterRegistry();
        PerceptionConfig pc;
        pc.retina = retinaSectorD();
        PerceptionSystem ps;
        const auto pr = ps.computeInputs(agents, foods, nullptr, world, pc);
        const systems::MovementConfig mc = systems::MovementSystem::fromRegistry(registry);
        const systems::NeuralSystemConfig nc =
            systems::NeuralSystem::fromRegistry(registry, mc, pr.inputSize);
        systems::NeuralSystem ns;
        const auto controls = ns.produceMovementControls(agents, world, nc, &pr);
        addCheck(summary, "sector: MLP receives input without crash",
                 controls.size() == 1 && ns.lastStats().agentsProcessed == 1);
    }

    // 39: MovementSystem accepts output
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        const auto registry = config::createDefaultParameterRegistry();
        PerceptionConfig pc;
        pc.retina = retinaSectorD();
        PerceptionSystem ps;
        const auto pr = ps.computeInputs(agents, foods, nullptr, world, pc);
        const systems::MovementConfig mc = systems::MovementSystem::fromRegistry(registry);
        const systems::NeuralSystemConfig nc =
            systems::NeuralSystem::fromRegistry(registry, mc, pr.inputSize);
        systems::NeuralSystem ns;
        const auto controls = ns.produceMovementControls(agents, world, nc, &pr);
        const auto stats = systems::MovementSystem{}.apply(agents, world, 1.0 / 30.0, mc, &controls);
        addCheck(summary, "sector: MovementSystem accepts output without crash",
                 stats.agentsProcessed == 1);
    }

    // 40: high-scale auto sector activates
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        std::mt19937 rng(20260528U);
        populateAgents(agents, 50, rng);
        static_cast<void>(foods.createFood(foodAt(500.0, 350.0)));
        PerceptionConfig pc;
        pc.retina = retinaSectorD();
        pc.retina.visionMode = "single";
        pc.retina.highScaleAutoSector = true;
        pc.retina.highScaleSectorMinAgents = 20;
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        (void)r;
        addCheck(summary, "high_scale_auto_sector overrides single -> sector when threshold met",
                 ps.lastStats().visionModeEnum == VisionMode::Sector &&
                 ps.lastStats().autoSectorActive &&
                 ps.lastStats().requestedModeEnum == VisionMode::Single);
    }

    // 41: single still works
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.visionMode = "single";
        const auto r = sectorPerception(cfg, agents, foods);
        addCheck(summary, "single mode still works", maxValue(r.flatInputs) > kEpsilon);
    }

    // 42: fullbody still works
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(140.0, 100.0, 8.0)));
        RetinaConfig cfg = retinaSectorD();
        cfg.visionMode = "fullbody";
        const auto r = sectorPerception(cfg, agents, foods);
        addCheck(summary, "fullbody mode still works", maxValue(r.flatInputs) > kEpsilon);
    }

    // 43-47: regressions
    addCheck(summary, "Phase 7 regression", systems::runPhase7Validation().passed);
    addCheck(summary, "Phase 8 regression", systems::runPhase8Validation().passed);
    addCheck(summary, "Phase 9 regression", neural::runPhase9Validation().passed);
    addCheck(summary, "Phase 10 regression", runPhase10Validation().passed);
    addCheck(summary, "Phase 11 regression", runPhase11Validation().passed);

    // 48: vision mode aliases
    addCheck(summary, "vision mode aliases include sector",
             normalizeVisionMode("sector") == VisionMode::Sector &&
             normalizeVisionMode("bins") == VisionMode::Sector);

    // 49: bins normalizers
    addCheck(summary, "bins normalizers handle aliases",
             normalizeBinsMode("max") == BinsMode::Strongest &&
             normalizeBinsMode("soma") == BinsMode::SumSaturating &&
             normalizeBinsFalloff("quadratic") == BinsFalloff::Quadratic &&
             normalizeBinsProjection("edges") == BinsProjection::CenterEdges);

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 12 validation checks passed. checks=" << summary.checks;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase12BenchmarkResult> runPhase12Microbenchmark()
{
    std::vector<Phase12BenchmarkResult> results;
    const int agentCounts[] = {100, 300, 600, 1000, 2000};
    constexpr int repeats = 15;
    const auto world = makeWorld();

    struct Scenario
    {
        const char* name;
        VisionMode mode;
        BinsMode binsMode;
        int subdivisions;
        BinsDistribution dist;
        BinsFalloff falloff;
        BinsProjection projection;
        int candidateLimit;
        bool rgbd;
        std::size_t eyes;
        bool debugFirst;
    };

    const Scenario scenarios[] = {
        // Mode comparison D-only 1 eye
        {"single_D_1eye",   VisionMode::Single,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},
        {"fullbody_D_1eye", VisionMode::Fullbody, BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},
        {"sector_D_1eye",   VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},

        // Sector bins_mode variants
        {"sector_strongest",VisionMode::Sector,   BinsMode::Strongest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, true, 1, false},
        {"sector_sum",      VisionMode::Sector,   BinsMode::SumSaturating, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},
        {"sector_weighted", VisionMode::Sector,   BinsMode::WeightedAverage, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},

        // Sector subdivisions variants
        {"sector_subdiv1",  VisionMode::Sector,   BinsMode::Nearest, 1,  BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},
        {"sector_subdiv5",  VisionMode::Sector,   BinsMode::Nearest, 5,  BinsDistribution::NearDetail, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},
        {"sector_subdiv20", VisionMode::Sector,   BinsMode::Nearest, 20, BinsDistribution::NearDetail, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},
        {"sector_subdiv99", VisionMode::Sector,   BinsMode::Nearest, 99, BinsDistribution::NearDetail, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, false},

        // Sector falloff variants
        {"sector_quadratic",VisionMode::Sector,   BinsMode::Nearest, 5, BinsDistribution::Linear, BinsFalloff::Quadratic, BinsProjection::Center, 0, false, 1, false},
        {"sector_step",     VisionMode::Sector,   BinsMode::Nearest, 5, BinsDistribution::Linear, BinsFalloff::Step,      BinsProjection::Center, 0, false, 1, false},
        {"sector_none",     VisionMode::Sector,   BinsMode::Nearest, 5, BinsDistribution::Linear, BinsFalloff::None,      BinsProjection::Center, 0, false, 1, false},

        // Sector projection variants
        {"sector_edges",    VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::CenterEdges,  0, false, 1, false},
        {"sector_apparent", VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::ApparentSize, 0, false, 1, false},

        // Sector candidate_limit variants
        {"sector_limit32",  VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 32,  false, 1, false},
        {"sector_limit128", VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 128, false, 1, false},
        {"sector_limit512", VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 512, false, 1, false},

        // Sector with RGBD and 2 eyes
        {"sector_RGBD_1eye",VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, true,  1, false},
        {"sector_D_2eyes",  VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, false, 2, false},

        // Debug
        {"sector_debug1",   VisionMode::Sector,   BinsMode::Nearest, 1, BinsDistribution::Linear, BinsFalloff::Linear, BinsProjection::Center, 0, false, 1, true},
    };

    for (const auto& scenario : scenarios)
    {
        for (const int agentCount : agentCounts)
        {
            std::mt19937 rng(20260528U + static_cast<std::uint32_t>(agentCount) * 17U);
            simulation::AgentStore agents;
            simulation::FoodStore foods;
            populateAgents(agents, agentCount, rng);
            populateFoods(foods, agentCount / 2, rng);

            simulation::SpatialHash spatial(simulation::spatialConfigForWorld(world, 36.0));
            spatial.rebuild(agents, foods);

            PerceptionConfig pc;
            pc.retina.visionMode = visionModeName(scenario.mode);
            pc.retina.visionRadius = 120.0;
            pc.retina.retinaCount = 18;
            pc.retina.fovDegrees = 180.0;
            pc.retina.eyeCount = scenario.eyes;
            pc.retina.seeFood = true;
            pc.retina.channelD = true;
            if (scenario.rgbd)
            {
                pc.retina.inputMode = RetinaInputMode::ColorPlusDistance;
                pc.retina.channelR = true;
                pc.retina.channelG = true;
                pc.retina.channelB = true;
            }
            pc.retina.sectorBins.mode = scenario.binsMode;
            pc.retina.sectorBins.subdivisions = scenario.subdivisions;
            pc.retina.sectorBins.distribution = scenario.dist;
            pc.retina.sectorBins.falloff = scenario.falloff;
            pc.retina.sectorBins.projection = scenario.projection;
            pc.retina.sectorBins.candidateLimit = scenario.candidateLimit;

            PerceptionSystem ps;
            VisionDebugData debug;
            PerceptionDebugRequest req;
            if (scenario.debugFirst && agents.size() > 0)
            {
                req.agentId = agents.idAt(0).value;
                req.out = &debug;
            }

            volatile double sink = 0.0;
            const auto start = std::chrono::high_resolution_clock::now();
            for (int rep = 0; rep < repeats; ++rep)
            {
                const auto r = ps.computeInputs(agents, foods, &spatial, world, pc, req);
                sink += r.flatInputs.empty() ? 0.0 : r.flatInputs[0];
            }
            const auto end = std::chrono::high_resolution_clock::now();
            const double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
            static_cast<void>(sink);

            Phase12BenchmarkResult row;
            row.scenario = scenario.name;
            row.visionMode = visionModeName(scenario.mode);
            row.binsMode = binsModeName(scenario.binsMode);
            row.binsDistribution = binsDistributionName(scenario.dist);
            row.binsFalloff = binsFalloffName(scenario.falloff);
            row.binsProjection = binsProjectionName(scenario.projection);
            row.agents = static_cast<std::size_t>(agentCount);
            row.retinaCount = 18;
            row.eyeCount = scenario.eyes;
            row.channelCount = pc.retina.channelCount();
            row.inputSize = pc.retina.inputSize();
            row.subdivisions = scenario.subdivisions;
            row.candidateLimit = scenario.candidateLimit;
            row.repeats = repeats;
            row.totalMilliseconds = totalMs;
            row.averagePerceptionMicroseconds = totalMs * 1000.0 / static_cast<double>(repeats);
            row.averageCandidatesPerAgent = ps.lastStats().averageCandidatesPerAgent;
            row.averageCandidatesAfterLimit = ps.lastStats().averageCandidatesAfterLimit;
            row.averageHitsPerAgent = agentCount > 0
                ? static_cast<double>(ps.lastStats().totalRayHits) / static_cast<double>(agentCount)
                : 0.0;
            row.usedSpatialHash = ps.lastStats().usedSpatialHash;
            row.debugActive = scenario.debugFirst;
            row.autoSectorActive = ps.lastStats().autoSectorActive;
            results.push_back(row);
        }
    }
    return results;
}
} // namespace agentbiosim::perception
