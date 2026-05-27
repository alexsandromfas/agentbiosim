#include "systems/Phase8Diagnostics.hpp"

#include "systems/EnergySystem.hpp"
#include "systems/InteractionSystem.hpp"
#include "systems/MovementSystem.hpp"

#include "simulation/SpatialHash.hpp"
#include "simulation/World.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>

namespace agentbiosim::systems
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1.0e-6;

void addCheck(Phase8ValidationSummary& summary, const std::string& name, const bool condition)
{
    ++summary.checks;
    if (condition)
    {
        return;
    }
    summary.passed = false;
    summary.details += "FAILED " + name + "\n";
}

bool near(const double actual, const double expected, const double epsilon = kEpsilon)
{
    return std::abs(actual - expected) <= epsilon;
}

simulation::World makeWorld(const simulation::WorldShape shape = simulation::WorldShape::Rectangular)
{
    simulation::WorldConfig config;
    config.shape = shape;
    config.width = 100.0;
    config.height = 100.0;
    config.radius = 50.0;
    config.center = {50.0, 50.0};
    return simulation::World(config);
}

simulation::AgentSpawn agentAt(const double x, const double y, const double angle = 0.0, const double radius = 5.0)
{
    simulation::AgentSpawn spawn;
    spawn.position = {x, y};
    spawn.angle = angle;
    spawn.radius = radius;
    spawn.energy = 100.0;
    spawn.typeCode = simulation::AgentTypeCode::LegacyBacteria;
    return spawn;
}

simulation::FoodSpawn foodAt(const double x, const double y)
{
    simulation::FoodSpawn spawn;
    spawn.position = {x, y};
    spawn.radius = 3.0;
    spawn.energy = 25.0;
    spawn.initialEnergy = 25.0;
    spawn.kind = simulation::FoodKind::Instant;
    return spawn;
}

MovementConfig baseConfig()
{
    MovementConfig config;
    config.mode = MovementMode::Forward;
    config.maxSpeed = 10.0;
    config.maxTurn = 0.5;
    config.allowReverse = false;
    config.inertia = 1.0;
    config.smoothLocomotion = false;
    config.smoothLinearDragEnabled = false;
    config.smoothAngularDragEnabled = false;
    return config;
}

void populateAgents(simulation::AgentStore& agents, const int agentCount)
{
    agents.clear();
    std::mt19937 rng(20260527U + static_cast<std::uint32_t>(agentCount));
    std::uniform_real_distribution<double> xDistribution(20.0, 980.0);
    std::uniform_real_distribution<double> yDistribution(20.0, 680.0);
    std::uniform_real_distribution<double> angleDistribution(-kPi, kPi);

    for (int i = 0; i < agentCount; ++i)
    {
        simulation::AgentSpawn spawn;
        spawn.position = {xDistribution(rng), yDistribution(rng)};
        spawn.angle = angleDistribution(rng);
        spawn.radius = 5.0;
        spawn.energy = 100.0;
        static_cast<void>(agents.createAgent(spawn));
    }
}

std::vector<MovementControl> deterministicControls(const std::size_t count)
{
    std::vector<MovementControl> controls;
    controls.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const double phase = static_cast<double>(i % 41U) * 0.31;
        controls.push_back({1.0 + 0.4 * std::sin(phase), 0.5 * std::cos(phase), std::sin(phase * 1.7)});
    }
    return controls;
}
} // namespace

Phase8ValidationSummary runPhase8Validation()
{
    Phase8ValidationSummary summary;
    summary.passed = true;
    summary.movementModesTested = "forward, omni";

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(50.0, 50.0, 0.0)));
        const auto world = makeWorld();
        auto config = baseConfig();
        const std::vector<MovementControl> controls{{10.0, 0.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, config, &controls);
        summary.agentsTested += stats.agentsProcessed;
        addCheck(summary, "forward positive speed moves agent forward", agents.positionAt(0).x > 50.0 && near(agents.positionAt(0).y, 50.0));
        addCheck(summary, "forward max speed respected", std::hypot(agents.velocityAt(0).x, agents.velocityAt(0).y) <= config.maxSpeed + kEpsilon);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(50.0, 50.0, 0.0)));
        const auto world = makeWorld();
        auto config = baseConfig();
        const std::vector<MovementControl> controls{{0.0, 0.0, 10.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, config, &controls);
        summary.agentsTested += stats.agentsProcessed;
        addCheck(summary, "turn changes angle", agents.angleAt(0) > 0.0);
        addCheck(summary, "max turn respected", agents.angleAt(0) <= config.maxTurn + kEpsilon);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(50.0, 50.0, 0.0)));
        const auto world = makeWorld();
        auto config = baseConfig();
        config.allowReverse = false;
        const std::vector<MovementControl> controls{{-10.0, 0.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, config, &controls);
        summary.agentsTested += stats.agentsProcessed;
        addCheck(summary, "reverse disabled prevents signed reverse velocity", agents.velocityAt(0).x >= 0.0);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(50.0, 50.0, 0.0)));
        const auto world = makeWorld();
        auto config = baseConfig();
        config.allowReverse = true;
        const std::vector<MovementControl> controls{{-10.0, 0.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, config, &controls);
        summary.agentsTested += stats.agentsProcessed;
        addCheck(summary, "reverse enabled allows signed reverse velocity", agents.velocityAt(0).x < 0.0);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(6.0, 50.0, kPi)));
        const auto world = makeWorld();
        auto config = baseConfig();
        config.maxSpeed = 20.0;
        const std::vector<MovementControl> controls{{10.0, 0.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, config, &controls);
        summary.agentsTested += stats.agentsProcessed;
        addCheck(summary, "rectangular world contains moved agent", world.isInside(agents.positionAt(0), agents.radiusAt(0)));
        addCheck(summary, "rectangular wall collision reported", stats.wallCollisions == 1U);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(90.0, 50.0, 0.0)));
        const auto world = makeWorld(simulation::WorldShape::Circular);
        auto config = baseConfig();
        config.maxSpeed = 20.0;
        const std::vector<MovementControl> controls{{10.0, 0.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, config, &controls);
        summary.agentsTested += stats.agentsProcessed;
        addCheck(summary, "circular world contains moved agent", world.isInside(agents.positionAt(0), agents.radiusAt(0)));
        addCheck(summary, "circular wall collision reported", stats.wallCollisions == 1U);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(50.0, 50.0, 0.0)));
        const auto world = makeWorld();
        auto config = baseConfig();
        config.mode = MovementMode::Omni;
        config.maxSpeed = 10.0;
        const std::vector<MovementControl> controls{{0.0, 10.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, config, &controls);
        summary.agentsTested += stats.agentsProcessed;
        addCheck(summary, "omni strafe moves agent locally sideways", agents.positionAt(0).y > 50.0);
        addCheck(summary, "omni max speed respected", std::hypot(agents.velocityAt(0).x, agents.velocityAt(0).y) <= config.maxSpeed + kEpsilon);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(50.0, 50.0, 0.0)));
        const auto world = makeWorld();
        auto config = baseConfig();
        config.smoothLocomotion = true;
        config.smoothLinearInertia = true;
        config.smoothLinearDragEnabled = false;
        config.smoothMaxLinearAccel = 5.0;
        const std::vector<MovementControl> controls{{10.0, 0.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, config, &controls);
        summary.agentsTested += stats.agentsProcessed;
        addCheck(summary, "smooth linear acceleration limits velocity", std::hypot(agents.velocityAt(0).x, agents.velocityAt(0).y) <= 5.0 + kEpsilon);
    }

    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(20.0, 20.0, 0.0)));
        static_cast<void>(foods.createFood(foodAt(20.0, 20.0)));
        const auto world = makeWorld();
        auto movementConfig = baseConfig();
        movementConfig.maxSpeed = 0.0;
        const std::vector<MovementControl> controls{{0.0, 0.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, movementConfig, &controls);
        summary.agentsTested += stats.agentsProcessed;
        InteractionConfig interactionConfig;
        interactionConfig.useSpatial = false;
        const InteractionStats interactionStats = InteractionSystem{}.apply(agents, foods, nullptr, interactionConfig);
        addCheck(summary, "movement does not break phase 7 food interaction", interactionStats.foodsConsumed == 1U && foods.empty());
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(50.0, 50.0, 0.0)));
        const auto world = makeWorld();
        auto movementConfig = baseConfig();
        movementConfig.maxSpeed = 10.0;
        const std::vector<MovementControl> controls{{10.0, 0.0, 0.0}};
        const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0, movementConfig, &controls);
        summary.agentsTested += stats.agentsProcessed;
        EnergyConfig energyConfig;
        energyConfig.v0Cost = 0.0;
        energyConfig.vmaxCost = 10.0;
        energyConfig.vmaxRef = 10.0;
        energyConfig.energyCap = 400.0;
        static_cast<void>(EnergySystem{}.apply(agents, 1.0, energyConfig));
        addCheck(summary, "energy still reacts to movement velocity", agents.energyAt(0) < 100.0);
    }

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 8 validation checks passed. agentsTested=" << summary.agentsTested
                << " modes=" << summary.movementModesTested;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase8BenchmarkResult> runPhase8Microbenchmark()
{
    const int scenarios[] = {100, 300, 600, 1000};
    std::vector<Phase8BenchmarkResult> results;
    results.reserve(16);

    simulation::WorldConfig worldConfig;
    worldConfig.shape = simulation::WorldShape::Rectangular;
    worldConfig.width = 1000.0;
    worldConfig.height = 700.0;
    worldConfig.center = {500.0, 350.0};
    simulation::World world(worldConfig);

    for (const int agentCount : scenarios)
    {
        for (const MovementMode mode : {MovementMode::Forward, MovementMode::Omni})
        {
            for (const bool smooth : {false, true})
            {
                simulation::AgentStore agents;
                populateAgents(agents, agentCount);
                const std::vector<MovementControl> controls = deterministicControls(agents.size());

                MovementConfig config;
                config.mode = mode;
                config.maxSpeed = 300.0;
                config.maxTurn = kPi;
                config.allowReverse = false;
                config.inertia = 1.0;
                config.smoothLocomotion = smooth;
                config.smoothLinearInertia = true;
                config.smoothLinearDragEnabled = true;
                config.smoothAngularInertia = true;
                config.smoothAngularDragEnabled = true;

                const auto start = std::chrono::high_resolution_clock::now();
                const MovementStats stats = MovementSystem{}.apply(agents, world, 1.0 / 30.0, config, &controls);
                const auto end = std::chrono::high_resolution_clock::now();

                results.push_back({
                    agentCount,
                    MovementSystem::movementModeName(mode),
                    smooth,
                    std::chrono::duration<double, std::milli>(end - start).count(),
                    stats.agentsProcessed,
                    stats.maxSpeedObserved,
                    stats.wallCollisions,
                });
            }
        }
    }

    return results;
}
} // namespace agentbiosim::systems
