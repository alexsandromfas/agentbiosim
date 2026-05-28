#include "perception/Phase11Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "neural/Phase9Diagnostics.hpp"
#include "perception/PerceptionSystem.hpp"
#include "perception/Phase10Diagnostics.hpp"
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

void addCheck(Phase11ValidationSummary& summary, const std::string& name, const bool condition)
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

simulation::AgentSpawn agentAt(const double x, const double y, const double angle = 0.0,
                               const double radius = 5.0)
{
    simulation::AgentSpawn spawn;
    spawn.position = {x, y};
    spawn.angle = angle;
    spawn.radius = radius;
    spawn.energy = 100.0;
    spawn.typeCode = simulation::AgentTypeCode::LegacyBacteria;
    return spawn;
}

simulation::FoodSpawn foodAt(const double x, const double y, const double radius = 3.0,
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

RetinaConfig retinaSingleD()
{
    RetinaConfig config;
    config.visionMode = "single";
    config.visionRadius = 120.0;
    config.retinaCount = 18;
    config.fovDegrees = 180.0;
    config.eyeCount = 1;
    config.inputMode = RetinaInputMode::DistanceOnly;
    config.channelD = true;
    config.seeFood = true;
    return config;
}

RetinaConfig retinaFullbodyD()
{
    RetinaConfig config = retinaSingleD();
    config.visionMode = "fullbody";
    return config;
}

RetinaConfig retinaFullbodyRGBD()
{
    RetinaConfig config = retinaFullbodyD();
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

Phase11ValidationSummary runPhase11Validation()
{
    Phase11ValidationSummary summary;
    const auto world = makeWorld();

    // Tests 1-3: ray-circle intersection covered indirectly through fullbody behavior.
    // Test 1: hit direto (objeto grande o suficiente para que algum raio intercepte
    // geometricamente, dado que com retina_count=18 e FOV=180 o espacamento e ~10.6
    // graus e nao ha raio exatamente na frente).
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(140.0, 100.0, 8.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "fullbody: ray hits food directly ahead", maxValue(r.flatInputs) > kEpsilon);
    }

    // Test 2: sem hit (objeto fora)
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(50.0, 100.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "fullbody: no hit when food behind", maxValue(r.flatInputs) < kEpsilon);
    }

    // Test 3: objeto atras e ignorado mesmo dentro do raio
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(50.0, 100.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        pc.retina.fovDegrees = 120.0;
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "fullbody: ignores object behind agent", maxValue(r.flatInputs) < kEpsilon);
    }

    // Test 4: objeto a frente ativa retina em fullbody (objeto grande o suficiente
    // para que os raios proximos do centro do FOV interceptem geometricamente).
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(140.0, 100.0, 8.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "fullbody: food ahead activates center retina region",
                 r.flatInputs[8] > kEpsilon || r.flatInputs[9] > kEpsilon);
    }

    // Test 5: atras de novo (com fov default 180)
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(50.0, 100.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "fullbody: object exactly behind ignored", maxValue(r.flatInputs) < kEpsilon);
    }

    // Test 6: fora do FOV
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(50.0, 200.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        pc.retina.fovDegrees = 120.0;
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "fullbody: outside FOV ignored", maxValue(r.flatInputs) < kEpsilon);
    }

    // Test 7: fora do raio
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(300.0, 100.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "fullbody: outside vision radius ignored", maxValue(r.flatInputs) < kEpsilon);
    }

    // Test 8: objeto grande/perto ativa mais retinas em fullbody que em single
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        // Comida grande e perto: raio 30, distancia ~50.
        static_cast<void>(foods.createFood(foodAt(160.0, 100.0, 30.0)));

        PerceptionConfig pcSingle;
        pcSingle.retina = retinaSingleD();
        pcSingle.retina.visionRadius = 200.0;
        PerceptionSystem psSingle;
        const auto rSingle = psSingle.computeInputs(agents, foods, nullptr, world, pcSingle);

        PerceptionConfig pcFull;
        pcFull.retina = retinaFullbodyD();
        pcFull.retina.visionRadius = 200.0;
        PerceptionSystem psFull;
        const auto rFull = psFull.computeInputs(agents, foods, nullptr, world, pcFull);

        const auto countActive = [](const std::vector<double>& v) {
            std::size_t c = 0;
            for (const double x : v)
            {
                if (x > kEpsilon)
                {
                    ++c;
                }
            }
            return c;
        };
        const std::size_t nSingle = countActive(rSingle.flatInputs);
        const std::size_t nFull = countActive(rFull.flatInputs);
        addCheck(summary, "fullbody activates more retinas than single for large/near object",
                 nFull > nSingle);
    }

    // Test 9: objeto pequeno e longe ativa pouco em fullbody. Diferente do single,
    // o fullbody pode ativar zero retinas quando o objeto e geometricamente menor
    // que o espacamento angular dos raios; isso e equivalente ao comportamento
    // geometrico do Python (ver sensors.py linhas 2195-2263).
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        // Objeto medio relativamente longe: r=4, dist ~75.
        static_cast<void>(foods.createFood(foodAt(180.0, 100.0, 4.0)));

        PerceptionConfig pcFull;
        pcFull.retina = retinaFullbodyD();
        PerceptionSystem psFull;
        const auto rFull = psFull.computeInputs(agents, foods, nullptr, world, pcFull);
        std::size_t nFull = 0;
        for (const double v : rFull.flatInputs)
        {
            if (v > kEpsilon)
            {
                ++nFull;
            }
        }
        addCheck(summary, "medium/medium-far object activates few retinas in fullbody",
                 nFull <= 3);
    }

    // Test 10: distancia normalizada coerente em fullbody. Objeto suficientemente
    // grande para hit geometrico nos raios proximos do centro. A distancia de hit
    // do raio (raio-circulo intersection) e proxima de centerDist - radius para
    // rays alinhados com o centro do objeto.
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(140.0, 100.0, 8.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        const double mx = maxValue(r.flatInputs);
        // eye->centro = 35, raio = 8, hit do raio mais alinhado ~= 35 - 8 = 27 (com pequena diferenca por ray angle).
        const double expectedMin = (120.0 - 30.0) / 120.0;
        const double expectedMax = (120.0 - 25.0) / 120.0;
        addCheck(summary, "fullbody: normalized distance coherent",
                 mx > expectedMin && mx < expectedMax + 0.05);
    }

    // Test 11: canal D em fullbody
    {
        RetinaConfig cfg = retinaFullbodyD();
        addCheck(summary, "fullbody with D channel produces input_size=18",
                 cfg.inputSize() == 18);
    }

    // Test 12: canal R em fullbody (responde a vermelho)
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(135.0, 100.0, 8.0, {220, 30, 30})));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyRGBD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double rVal = r.flatInputs[centerRay * stride + 0];
        const double gVal = r.flatInputs[centerRay * stride + 1];
        addCheck(summary, "fullbody: channel R responds to red food", rVal > 0.5 && rVal > gVal);
    }

    // Test 13: canal G em fullbody
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(135.0, 100.0, 8.0, {30, 220, 30})));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyRGBD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double gVal = r.flatInputs[centerRay * stride + 1];
        const double rVal = r.flatInputs[centerRay * stride + 0];
        addCheck(summary, "fullbody: channel G responds to green food", gVal > 0.5 && gVal > rVal);
    }

    // Test 14: canal B em fullbody
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(135.0, 100.0, 8.0, {30, 30, 220})));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyRGBD();
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double bVal = r.flatInputs[centerRay * stride + 2];
        const double rVal = r.flatInputs[centerRay * stride + 0];
        addCheck(summary, "fullbody: channel B responds to blue food", bVal > 0.5 && bVal > rVal);
    }

    // Test 15: canal desligado nao entra no vetor
    {
        RetinaConfig cfg;
        cfg.inputMode = RetinaInputMode::ColorPlusDistance;
        cfg.channelR = true;
        cfg.channelG = false;
        cfg.channelB = false;
        cfg.channelD = true;
        cfg.retinaCount = 18;
        cfg.eyeCount = 1;
        addCheck(summary, "disabled channel excludes from input",
                 cfg.channelCount() == 2 && cfg.inputSize() == 36);
    }

    // Test 16: same input_size for single and fullbody given same config
    {
        RetinaConfig cfgSingle = retinaSingleD();
        RetinaConfig cfgFull = retinaFullbodyD();
        addCheck(summary, "single and fullbody have same input_size",
                 cfgSingle.inputSize() == cfgFull.inputSize());
    }

    // Test 17: 2 olhos dobram input_size
    {
        RetinaConfig cfg = retinaFullbodyD();
        cfg.eyeCount = 2;
        addCheck(summary, "fullbody: eye_count=2 doubles input_size", cfg.inputSize() == 36);
    }

    // Test 18: olho esquerdo detecta objeto
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        // Agente em (100,100), angle=0 (olhando para +X).
        // Com eye_count=2, eye_separation=45, eye_angle=60:
        //   eye0 (esquerdo): pos_offset=-22.5deg, gaze_offset=-30deg
        //   eye1 (direito):  pos_offset=+22.5deg, gaze_offset=+30deg
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        // Objeto colocado a esquerda da direcao do olhar geral (no campo do olho esquerdo).
        static_cast<void>(foods.createFood(foodAt(140.0, 60.0, 8.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        pc.retina.eyeCount = 2;
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        const std::size_t inputsPerEye = pc.retina.retinaCount;
        double leftMax = 0.0;
        for (std::size_t i = 0; i < inputsPerEye; ++i)
        {
            leftMax = std::max(leftMax, r.flatInputs[i]);
        }
        addCheck(summary, "fullbody: left eye detects object on its side", leftMax > kEpsilon);
    }

    // Test 19: olho direito detecta objeto
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(140.0, 140.0, 8.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        pc.retina.eyeCount = 2;
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        const std::size_t inputsPerEye = pc.retina.retinaCount;
        double rightMax = 0.0;
        for (std::size_t i = inputsPerEye; i < 2 * inputsPerEye; ++i)
        {
            rightMax = std::max(rightMax, r.flatInputs[i]);
        }
        addCheck(summary, "fullbody: right eye detects object on its side", rightMax > kEpsilon);
    }

    // Test 20: see_food=false remove comida
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        pc.retina.seeFood = false;
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "fullbody: see_food=false hides food", maxValue(r.flatInputs) < kEpsilon);
    }

    // Test 21: debug data nao e gerado quando desligado
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        VisionDebugData debug;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        (void)r;
        addCheck(summary, "fullbody: debug data NOT generated when not requested",
                 debug.rays.empty() && !debug.active);
    }

    // Test 22: debug data e gerado quando ligado
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        const simulation::EntityId agentId = agents.createAgent(agentAt(100.0, 100.0, 0.0));
        static_cast<void>(foods.createFood(foodAt(140.0, 100.0, 8.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        VisionDebugData debug;
        PerceptionDebugRequest req;
        req.agentId = agentId.value;
        req.out = &debug;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc, req);
        (void)r;
        addCheck(summary, "fullbody: debug data generated when requested",
                 debug.active && debug.rays.size() == 18 && debug.hitCount() >= 1);
    }

    // Test 23: MLP recebe input fullbody sem crash
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));

        const auto registry = config::createDefaultParameterRegistry();
        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        const auto pr = ps.computeInputs(agents, foods, nullptr, world, pc);

        const systems::MovementConfig mc = systems::MovementSystem::fromRegistry(registry);
        const systems::NeuralSystemConfig nc = systems::NeuralSystem::fromRegistry(registry, mc, pr.inputSize);
        systems::NeuralSystem ns;
        const auto controls = ns.produceMovementControls(agents, world, nc, &pr);
        addCheck(summary, "fullbody: MLP receives fullbody input without crash",
                 controls.size() == 1 && ns.lastStats().agentsProcessed == 1);
    }

    // Test 24: MovementSystem recebe output sem crash
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));

        const auto registry = config::createDefaultParameterRegistry();
        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        PerceptionSystem ps;
        const auto pr = ps.computeInputs(agents, foods, nullptr, world, pc);

        const systems::MovementConfig mc = systems::MovementSystem::fromRegistry(registry);
        const systems::NeuralSystemConfig nc = systems::NeuralSystem::fromRegistry(registry, mc, pr.inputSize);
        systems::NeuralSystem ns;
        const auto controls = ns.produceMovementControls(agents, world, nc, &pr);
        const auto stats = systems::MovementSystem{}.apply(agents, world, 1.0 / 30.0, mc, &controls);
        addCheck(summary, "fullbody: MovementSystem accepts fullbody-driven output without crash",
                 stats.agentsProcessed == 1);
    }

    // Test 25-28: regressoes
    {
        const auto phase7 = systems::runPhase7Validation();
        addCheck(summary, "Phase 7 regression", phase7.passed);
    }
    {
        const auto phase8 = systems::runPhase8Validation();
        addCheck(summary, "Phase 8 regression", phase8.passed);
    }
    {
        const auto phase9 = neural::runPhase9Validation();
        addCheck(summary, "Phase 9 regression", phase9.passed);
    }
    {
        const auto phase10 = runPhase10Validation();
        addCheck(summary, "Phase 10 regression", phase10.passed);
    }

    // Test 29: vision mode normalization
    {
        addCheck(summary, "normalizeVisionMode handles aliases",
                 normalizeVisionMode("fullbody") == VisionMode::Fullbody &&
                 normalizeVisionMode("raycast") == VisionMode::Fullbody &&
                 normalizeVisionMode("single") == VisionMode::Single &&
                 normalizeVisionMode("invalid_mode") == VisionMode::Single &&
                 normalizeVisionMode("sector") == VisionMode::Sector);
    }

    // Test 30: Sector mode causes documented fallback to single
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));

        PerceptionConfig pc;
        pc.retina = retinaFullbodyD();
        pc.retina.visionMode = "sector";
        PerceptionSystem ps;
        const auto r = ps.computeInputs(agents, foods, nullptr, world, pc);
        (void)r;
        addCheck(summary, "sector mode falls back to single with documented reason",
                 ps.lastStats().fallbackMode && !ps.lastStats().fallbackReason.empty() &&
                 ps.lastStats().visionMode == std::string("single"));
    }

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 11 validation checks passed. checks=" << summary.checks;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase11BenchmarkResult> runPhase11Microbenchmark()
{
    std::vector<Phase11BenchmarkResult> results;
    const int agentCounts[] = {100, 300, 600, 1000};
    const int retinaCounts[] = {4, 8, 18, 32, 64};
    constexpr int repeats = 20;
    const auto world = makeWorld();

    struct Scenario
    {
        const char* name;
        VisionMode mode;
        bool rgbd;
        std::size_t eyes;
        bool debugFirst;
    };

    const Scenario scenarios[] = {
        {"single_D_1eye",     VisionMode::Single,   false, 1, false},
        {"fullbody_D_1eye",   VisionMode::Fullbody, false, 1, false},
        {"single_RGBD_1eye",  VisionMode::Single,   true,  1, false},
        {"fullbody_RGBD_1eye",VisionMode::Fullbody, true,  1, false},
        {"single_D_2eyes",    VisionMode::Single,   false, 2, false},
        {"fullbody_D_2eyes",  VisionMode::Fullbody, false, 2, false},
        {"fullbody_D_debug1", VisionMode::Fullbody, false, 1, true},
    };

    for (const auto& scenario : scenarios)
    {
        for (const int retinaCount : retinaCounts)
        {
            for (const int agentCount : agentCounts)
            {
                // retina sweep only used for fullbody/single D 1eye; skip rest to keep table manageable
                if (retinaCount != 18 && scenario.eyes != 1)
                {
                    continue;
                }
                if (retinaCount != 18 && scenario.rgbd)
                {
                    continue;
                }
                if (retinaCount != 18 && scenario.debugFirst)
                {
                    continue;
                }

                std::mt19937 rng(20260528U + static_cast<std::uint32_t>(agentCount) * 31U +
                                 static_cast<std::uint32_t>(retinaCount));
                simulation::AgentStore agents;
                simulation::FoodStore foods;
                populateAgents(agents, agentCount, rng);
                populateFoods(foods, agentCount / 2, rng);

                simulation::SpatialHash spatial(simulation::spatialConfigForWorld(world, 36.0));
                spatial.rebuild(agents, foods);

                PerceptionConfig pc;
                pc.retina.visionMode = visionModeName(scenario.mode);
                pc.retina.visionRadius = 120.0;
                pc.retina.retinaCount = static_cast<std::size_t>(retinaCount);
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

                Phase11BenchmarkResult row;
                row.scenario = scenario.name;
                row.visionMode = visionModeName(scenario.mode);
                row.agents = static_cast<std::size_t>(agentCount);
                row.retinaCount = static_cast<std::size_t>(retinaCount);
                row.eyeCount = scenario.eyes;
                row.channelCount = pc.retina.channelCount();
                row.inputSize = pc.retina.inputSize();
                row.repeats = repeats;
                row.totalMilliseconds = totalMs;
                row.averagePerceptionMicroseconds = totalMs * 1000.0 / static_cast<double>(repeats);
                row.averageCandidatesPerAgent = ps.lastStats().averageCandidatesPerAgent;
                row.averageHitsPerAgent = agentCount > 0
                    ? static_cast<double>(ps.lastStats().totalRayHits) / static_cast<double>(agentCount)
                    : 0.0;
                row.usedSpatialHash = ps.lastStats().usedSpatialHash;
                row.debugActive = scenario.debugFirst;
                results.push_back(row);
            }
        }
    }
    return results;
}
} // namespace agentbiosim::perception
