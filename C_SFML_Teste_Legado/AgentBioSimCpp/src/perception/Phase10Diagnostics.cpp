#include "perception/Phase10Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "perception/PerceptionSystem.hpp"
#include "perception/RetinaConfig.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/World.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/NeuralSystem.hpp"
#include "systems/Phase7Diagnostics.hpp"
#include "systems/Phase8Diagnostics.hpp"
#include "neural/Phase9Diagnostics.hpp"

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

void addCheck(Phase10ValidationSummary& summary, const std::string& name, const bool condition)
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

simulation::FoodSpawn foodAt(const double x, const double y, const simulation::ColorRgb color = {220, 30, 30})
{
    simulation::FoodSpawn spawn;
    spawn.position = {x, y};
    spawn.radius = 3.0;
    spawn.energy = 25.0;
    spawn.initialEnergy = 25.0;
    spawn.kind = simulation::FoodKind::Instant;
    spawn.color = color;
    return spawn;
}

RetinaConfig defaultRetinaD()
{
    RetinaConfig config;
    config.visionMode = "single";
    config.visionRadius = 120.0;
    config.retinaCount = 18;
    config.fovDegrees = 180.0;
    config.eyeCount = 1;
    config.inputMode = RetinaInputMode::DistanceOnly;
    config.channelD = true;
    config.channelR = false;
    config.channelG = false;
    config.channelB = false;
    config.seeFood = true;
    config.seeAgents = false;
    config.seePredators = false;
    return config;
}

RetinaConfig retinaRGBD()
{
    RetinaConfig config = defaultRetinaD();
    config.inputMode = RetinaInputMode::ColorPlusDistance;
    config.channelR = true;
    config.channelG = true;
    config.channelB = true;
    config.channelD = true;
    return config;
}

void populateAgents(simulation::AgentStore& agents, const int count, std::mt19937& rng)
{
    agents.clear();
    std::uniform_real_distribution<double> xDist(20.0, 980.0);
    std::uniform_real_distribution<double> yDist(20.0, 680.0);
    std::uniform_real_distribution<double> aDist(-kPi, kPi);
    for (int i = 0; i < count; ++i)
    {
        auto spawn = agentAt(xDist(rng), yDist(rng), aDist(rng));
        static_cast<void>(agents.createAgent(spawn));
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

Phase10ValidationSummary runPhase10Validation()
{
    Phase10ValidationSummary summary;

    // Test 1: input_size with D only
    {
        RetinaConfig config = defaultRetinaD();
        addCheck(summary, "input_size with D only = 18", config.inputSize() == 18);
    }

    // Test 2: input_size with R/G/B/D
    {
        RetinaConfig config = retinaRGBD();
        addCheck(summary, "input_size with RGBD = 72", config.inputSize() == 72);
    }

    // Test 3: food ahead activates expected retina
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(150.0, 100.0)));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = defaultRetinaD();
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        addCheck(summary, "food ahead produces non-zero input",
                 result.inputSize == 18 && result.flatInputs.size() == 18 &&
                 *std::max_element(result.flatInputs.begin(), result.flatInputs.end()) > kEpsilon);

        const std::size_t centerRay = 9;
        const double centerValue = result.flatInputs[centerRay];
        addCheck(summary, "food ahead activates center retina region", centerValue > kEpsilon);
    }

    // Test 4: food behind is ignored
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(50.0, 100.0)));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = defaultRetinaD();
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        const double maxVal = *std::max_element(result.flatInputs.begin(), result.flatInputs.end());
        addCheck(summary, "food behind agent is ignored", maxVal < kEpsilon);
    }

    // Test 5: food outside FOV is ignored
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(50.0, 200.0)));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = defaultRetinaD();
        percConfig.retina.fovDegrees = 120.0;
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        const double maxVal = *std::max_element(result.flatInputs.begin(), result.flatInputs.end());
        addCheck(summary, "food outside FOV is ignored", maxVal < kEpsilon);
    }

    // Test 6: food outside vision radius is ignored
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(300.0, 100.0)));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = defaultRetinaD();
        percConfig.retina.visionRadius = 120.0;
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        const double maxVal = *std::max_element(result.flatInputs.begin(), result.flatInputs.end());
        addCheck(summary, "food outside radius is ignored", maxVal < kEpsilon);
    }

    // Test 7: normalized distance is coherent
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(160.0, 100.0)));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = defaultRetinaD();
        percConfig.retina.visionRadius = 120.0;
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        const double maxVal = *std::max_element(result.flatInputs.begin(), result.flatInputs.end());
        const double distance = 60.0 - 3.0;
        const double expected = (120.0 - distance) / 120.0;
        addCheck(summary, "normalized distance is coherent",
                 maxVal > 0.0 && std::abs(maxVal - expected) < 0.05);
    }

    // Test 8: channel R responds to red food
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0, {220, 30, 30})));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = retinaRGBD();
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double rVal = result.flatInputs[centerRay * stride + 0];
        const double gVal = result.flatInputs[centerRay * stride + 1];
        addCheck(summary, "channel R responds to red food", rVal > 0.5 && rVal > gVal);
    }

    // Test 9: channel G responds to green object
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0, {30, 220, 30})));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = retinaRGBD();
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double gVal = result.flatInputs[centerRay * stride + 1];
        const double rVal = result.flatInputs[centerRay * stride + 0];
        addCheck(summary, "channel G responds to green food", gVal > 0.5 && gVal > rVal);
    }

    // Test 10: channel B responds to blue object
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0, {30, 30, 220})));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = retinaRGBD();
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        const std::size_t stride = 4;
        const std::size_t centerRay = 9;
        const double bVal = result.flatInputs[centerRay * stride + 2];
        const double rVal = result.flatInputs[centerRay * stride + 0];
        addCheck(summary, "channel B responds to blue food", bVal > 0.5 && bVal > rVal);
    }

    // Test 11: disabled channel doesn't enter vector
    {
        RetinaConfig config;
        config.inputMode = RetinaInputMode::ColorPlusDistance;
        config.channelR = true;
        config.channelG = false;
        config.channelB = false;
        config.channelD = true;
        config.retinaCount = 18;
        config.eyeCount = 1;
        addCheck(summary, "disabled channel excludes from input",
                 config.channelCount() == 2 && config.inputSize() == 36);
    }

    // Test 12: see_food=false makes food invisible
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0)));

        const auto world = makeWorld();
        PerceptionConfig percConfig;
        percConfig.retina = defaultRetinaD();
        percConfig.retina.seeFood = false;
        PerceptionSystem system;
        const auto result = system.computeInputs(agents, foods, nullptr, world, percConfig);

        const double maxVal = *std::max_element(result.flatInputs.begin(), result.flatInputs.end());
        addCheck(summary, "see_food=false hides food", maxVal < kEpsilon);
    }

    // Test 13: eye_count=2 doubles input_size
    {
        RetinaConfig config = defaultRetinaD();
        config.eyeCount = 2;
        addCheck(summary, "eye_count=2 doubles input_size", config.inputSize() == 36);
    }

    // Test 14: retina_count alters input_size
    {
        RetinaConfig config = defaultRetinaD();
        config.retinaCount = 9;
        addCheck(summary, "retina_count=9 gives input_size=9", config.inputSize() == 9);
    }

    // Test 15: MLP receives real input_size without crash
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0)));

        const auto world = makeWorld();
        const auto registry = config::createDefaultParameterRegistry();
        PerceptionConfig percConfig;
        percConfig.retina = defaultRetinaD();
        PerceptionSystem perceptionSystem;
        const auto perceptionResult = perceptionSystem.computeInputs(agents, foods, nullptr, world, percConfig);

        const systems::MovementConfig movConfig = systems::MovementSystem::fromRegistry(registry);
        const systems::NeuralSystemConfig neuralConfig = systems::NeuralSystem::fromRegistry(
            registry, movConfig, perceptionResult.inputSize);
        systems::NeuralSystem neuralSystem;
        const auto controls = neuralSystem.produceMovementControls(agents, world, neuralConfig, &perceptionResult);
        addCheck(summary, "MLP receives real input_size without crash",
                 controls.size() == 1 && neuralSystem.lastStats().agentsProcessed == 1);
    }

    // Test 16: MovementSystem receives output without crash
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(130.0, 100.0)));

        const auto world = makeWorld();
        const auto registry = config::createDefaultParameterRegistry();
        PerceptionConfig percConfig;
        percConfig.retina = defaultRetinaD();
        PerceptionSystem perceptionSystem;
        const auto perceptionResult = perceptionSystem.computeInputs(agents, foods, nullptr, world, percConfig);

        const systems::MovementConfig movConfig = systems::MovementSystem::fromRegistry(registry);
        const systems::NeuralSystemConfig neuralConfig = systems::NeuralSystem::fromRegistry(
            registry, movConfig, perceptionResult.inputSize);
        systems::NeuralSystem neuralSystem;
        const auto controls = neuralSystem.produceMovementControls(agents, world, neuralConfig, &perceptionResult);
        const auto movStats = systems::MovementSystem{}.apply(agents, world, 1.0 / 30.0, movConfig, &controls);
        addCheck(summary, "MovementSystem receives perception-driven output without crash",
                 movStats.agentsProcessed == 1);
    }

    // Test 17: Phase 7 regression
    {
        const auto phase7 = systems::runPhase7Validation();
        addCheck(summary, "Phase 7 regression", phase7.passed);
    }

    // Test 18: Phase 8 regression
    {
        const auto phase8 = systems::runPhase8Validation();
        addCheck(summary, "Phase 8 regression", phase8.passed);
    }

    // Test 19: Phase 9 regression
    {
        const auto phase9 = neural::runPhase9Validation();
        addCheck(summary, "Phase 9 regression", phase9.passed);
    }

    // Test 20: RetinaConfig from registry matches defaults
    {
        const auto registry = config::createDefaultParameterRegistry();
        const auto config = retinaConfigFromRegistry(registry, "bacteria");
        addCheck(summary, "RetinaConfig from registry uses correct defaults",
                 config.retinaCount == 18 && config.fovDegrees == 180.0 &&
                 config.eyeCount == 1 && config.visionRadius == 120.0 &&
                 config.seeFood && !config.seeAgents &&
                 config.inputMode == RetinaInputMode::DistanceOnly &&
                 config.inputSize() == 18);
    }

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 10 validation checks passed. checks=" << summary.checks;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase10BenchmarkResult> runPhase10Microbenchmark()
{
    std::vector<Phase10BenchmarkResult> results;
    const int agentCounts[] = {100, 300, 600, 1000};
    constexpr int repeats = 30;
    const auto world = makeWorld();

    struct Scenario
    {
        const char* name;
        std::size_t retinaCount;
        std::size_t eyeCount;
        RetinaInputMode mode;
        bool channelR, channelG, channelB, channelD;
    };

    const Scenario scenarios[] = {
        {"D_only_1eye", 18, 1, RetinaInputMode::DistanceOnly, false, false, false, true},
        {"RGBD_1eye", 18, 1, RetinaInputMode::ColorPlusDistance, true, true, true, true},
        {"D_only_2eyes", 18, 2, RetinaInputMode::DistanceOnly, false, false, false, true},
        {"RGBD_2eyes", 18, 2, RetinaInputMode::ColorPlusDistance, true, true, true, true},
    };

    for (const auto& scenario : scenarios)
    {
        for (const int agentCount : agentCounts)
        {
            std::mt19937 rng(20260527U + static_cast<std::uint32_t>(agentCount));
            simulation::AgentStore agents;
            simulation::FoodStore foods;
            populateAgents(agents, agentCount, rng);
            populateFoods(foods, agentCount / 2, rng);

            simulation::SpatialHash spatial(simulation::spatialConfigForWorld(world, 36.0));
            spatial.rebuild(agents, foods);

            PerceptionConfig percConfig;
            percConfig.retina = defaultRetinaD();
            percConfig.retina.retinaCount = scenario.retinaCount;
            percConfig.retina.eyeCount = scenario.eyeCount;
            percConfig.retina.inputMode = scenario.mode;
            percConfig.retina.channelR = scenario.channelR;
            percConfig.retina.channelG = scenario.channelG;
            percConfig.retina.channelB = scenario.channelB;
            percConfig.retina.channelD = scenario.channelD;

            PerceptionSystem system;
            volatile double sink = 0.0;
            const auto start = std::chrono::high_resolution_clock::now();
            for (int r = 0; r < repeats; ++r)
            {
                const auto result = system.computeInputs(agents, foods, &spatial, world, percConfig);
                sink += result.flatInputs.empty() ? 0.0 : result.flatInputs[0];
            }
            const auto end = std::chrono::high_resolution_clock::now();
            const double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
            static_cast<void>(sink);

            Phase10BenchmarkResult row;
            row.scenario = scenario.name;
            row.agents = static_cast<std::size_t>(agentCount);
            row.retinaCount = scenario.retinaCount;
            row.eyeCount = scenario.eyeCount;
            row.channelCount = percConfig.retina.channelCount();
            row.inputSize = percConfig.retina.inputSize();
            row.repeats = repeats;
            row.totalMilliseconds = totalMs;
            row.averagePerceptionMicroseconds = totalMs * 1000.0 / static_cast<double>(repeats);
            row.averageCandidatesPerAgent = system.lastStats().averageCandidatesPerAgent;
            row.usedSpatialHash = system.lastStats().usedSpatialHash;
            results.push_back(row);
        }
    }
    return results;
}
} // namespace agentbiosim::perception
