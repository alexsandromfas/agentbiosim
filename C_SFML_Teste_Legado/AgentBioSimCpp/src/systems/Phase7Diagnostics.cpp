#include "systems/Phase7Diagnostics.hpp"

#include "systems/DeathSystem.hpp"
#include "systems/EnergySystem.hpp"
#include "systems/InteractionSystem.hpp"

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
constexpr double kEpsilon = 1.0e-6;

void addCheck(Phase7ValidationSummary& summary, const std::string& name, const bool condition)
{
    ++summary.checks;
    if (condition)
    {
        return;
    }
    summary.passed = false;
    summary.details += "FAILED " + name + "\n";
}

bool near(const double actual, const double expected)
{
    return std::abs(actual - expected) <= kEpsilon;
}

simulation::AgentSpawn agentAt(const double x, const double y, const double energy = 100.0)
{
    simulation::AgentSpawn spawn;
    spawn.position = {x, y};
    spawn.radius = 5.0;
    spawn.energy = energy;
    spawn.typeCode = simulation::AgentTypeCode::LegacyBacteria;
    return spawn;
}

simulation::FoodSpawn foodAt(const double x, const double y, const double energy = 25.0)
{
    simulation::FoodSpawn spawn;
    spawn.position = {x, y};
    spawn.radius = 3.0;
    spawn.energy = energy;
    spawn.initialEnergy = energy;
    spawn.kind = simulation::FoodKind::Instant;
    return spawn;
}

simulation::SpatialHash buildHash(const simulation::AgentStore& agents, const simulation::FoodStore& foods)
{
    simulation::SpatialHash hash({20.0, {0.0, 0.0}, {200.0, 200.0}});
    hash.rebuild(agents, foods);
    return hash;
}

void populateBenchmarkStores(simulation::AgentStore& agents,
                             simulation::FoodStore& foods,
                             const int agentCount,
                             const int foodCount)
{
    agents.clear();
    foods.clear();

    std::mt19937 rng(20260527U + static_cast<std::uint32_t>(agentCount * 13 + foodCount));
    std::uniform_real_distribution<double> xDistribution(10.0, 990.0);
    std::uniform_real_distribution<double> yDistribution(10.0, 690.0);

    std::vector<simulation::Vec2> agentPositions;
    agentPositions.reserve(static_cast<std::size_t>(agentCount));
    for (int i = 0; i < agentCount; ++i)
    {
        const simulation::Vec2 position{xDistribution(rng), yDistribution(rng)};
        agentPositions.push_back(position);
        auto spawn = agentAt(position.x, position.y, 100.0);
        spawn.radius = 5.0;
        static_cast<void>(agents.createAgent(spawn));
    }

    const int overlapCount = std::min(foodCount, std::max(1, agentCount / 4));
    for (int i = 0; i < foodCount; ++i)
    {
        simulation::Vec2 position{xDistribution(rng), yDistribution(rng)};
        if (i < overlapCount && i < static_cast<int>(agentPositions.size()))
        {
            position = agentPositions[static_cast<std::size_t>(i)];
        }
        auto spawn = foodAt(position.x, position.y, 25.0);
        spawn.radius = 3.0;
        static_cast<void>(foods.createFood(spawn));
    }
}
} // namespace

Phase7ValidationSummary runPhase7Validation()
{
    Phase7ValidationSummary summary;
    summary.passed = true;

    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(20.0, 20.0, 40.0)));
        static_cast<void>(foods.createFood(foodAt(20.0, 20.0, 25.0)));
        simulation::SpatialHash hash = buildHash(agents, foods);
        InteractionConfig config;
        config.useSpatial = true;
        config.energyCap = 400.0;
        const InteractionStats stats = InteractionSystem{}.apply(agents, foods, &hash, config);
        summary.foodsConsumed += stats.foodsConsumed;
        addCheck(summary, "spatial touching food consumed", stats.foodsConsumed == 1U);
        addCheck(summary, "spatial consumed food removed", foods.size() == 0U);
        addCheck(summary, "spatial energy gained", near(agents.energyAt(0), 65.0));
    }

    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(10.0, 10.0, 40.0)));
        static_cast<void>(foods.createFood(foodAt(80.0, 80.0, 25.0)));
        InteractionConfig config;
        config.useSpatial = false;
        const InteractionStats stats = InteractionSystem{}.apply(agents, foods, nullptr, config);
        addCheck(summary, "far food not consumed", stats.foodsConsumed == 0U && foods.size() == 1U);
        addCheck(summary, "far food energy unchanged", near(agents.energyAt(0), 40.0));
    }

    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(30.0, 30.0, 40.0)));
        static_cast<void>(foods.createFood(foodAt(30.0, 30.0, 25.0)));
        InteractionConfig config;
        config.useSpatial = false;
        const InteractionStats stats = InteractionSystem{}.apply(agents, foods, nullptr, config);
        summary.foodsConsumed += stats.foodsConsumed;
        addCheck(summary, "fallback touching food consumed", stats.foodsConsumed == 1U);
        addCheck(summary, "fallback consumed food removed", foods.size() == 0U);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(10.0, 10.0, 100.0)));
        EnergyConfig config;
        config.v0Cost = 0.5;
        config.vmaxCost = 8.0;
        config.vmaxRef = 300.0;
        config.energyCap = 400.0;
        const EnergyStats stats = EnergySystem{}.apply(agents, 2.0, config);
        addCheck(summary, "energy processed one agent", stats.agentsProcessed == 1U);
        addCheck(summary, "idle energy cost applied", near(agents.energyAt(0), 99.0));
        addCheck(summary, "age advanced", near(agents.ageAt(0), 2.0));
    }

    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(20.0, 20.0, 95.0)));
        static_cast<void>(foods.createFood(foodAt(20.0, 20.0, 100.0)));
        InteractionConfig config;
        config.energyCap = 110.0;
        const InteractionStats stats = InteractionSystem{}.apply(agents, foods, nullptr, config);
        summary.foodsConsumed += stats.foodsConsumed;
        addCheck(summary, "energy cap respected", near(agents.energyAt(0), 110.0));
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(10.0, 10.0, 40.0)));
        DeathConfig config;
        config.deathEnergy = 50.0;
        config.maxDeathsPerStep = 5;
        const DeathStats stats = DeathSystem{}.apply(agents, config);
        summary.deaths += stats.deaths;
        addCheck(summary, "death below threshold", stats.deaths == 1U && agents.size() == 0U);
    }

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(10.0, 10.0, 10.0)));
        static_cast<void>(agents.createAgent(agentAt(20.0, 20.0, 20.0)));
        static_cast<void>(agents.createAgent(agentAt(30.0, 30.0, 30.0)));
        DeathConfig config;
        config.deathEnergy = 50.0;
        config.maxDeathsPerStep = 2;
        const DeathStats stats = DeathSystem{}.apply(agents, config);
        summary.deaths += stats.deaths;
        addCheck(summary, "max deaths per step respected", stats.deaths == 2U && agents.size() == 1U);
    }

    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(20.0, 20.0, 40.0)));
        auto chunk = foodAt(20.0, 20.0, 25.0);
        chunk.kind = simulation::FoodKind::Chunk;
        static_cast<void>(foods.createFood(chunk));
        const InteractionStats stats = InteractionSystem{}.apply(agents, foods, nullptr, {});
        addCheck(summary, "chunk food skipped in phase 7", stats.foodsConsumed == 0U && stats.chunkFoodsSkipped == 1U);
    }

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 7 validation checks passed. foodsConsumed=" << summary.foodsConsumed
                << " deaths=" << summary.deaths;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase7BenchmarkResult> runPhase7Microbenchmark()
{
    const int scenarios[][2] = {{100, 100}, {300, 150}, {600, 300}};
    std::vector<Phase7BenchmarkResult> results;
    results.reserve(6);

    for (const auto& scenario : scenarios)
    {
        for (const bool useSpatial : {true, false})
        {
            simulation::AgentStore agents;
            simulation::FoodStore foods;
            populateBenchmarkStores(agents, foods, scenario[0], scenario[1]);

            InteractionConfig interactionConfig;
            interactionConfig.useSpatial = useSpatial;
            interactionConfig.energyCap = 400.0;

            EnergyConfig energyConfig;
            energyConfig.v0Cost = 0.5;
            energyConfig.vmaxCost = 8.0;
            energyConfig.vmaxRef = 300.0;
            energyConfig.energyCap = 400.0;

            DeathConfig deathConfig;
            deathConfig.deathEnergy = 50.0;
            deathConfig.maxDeathsPerStep = 5;

            simulation::SpatialHash hash({36.0, {0.0, 0.0}, {1000.0, 700.0}});
            const auto start = std::chrono::high_resolution_clock::now();
            static_cast<void>(EnergySystem{}.apply(agents, 1.0 / 30.0, energyConfig));
            simulation::SpatialHash* hashPtr = nullptr;
            if (useSpatial)
            {
                hash.rebuild(agents, foods);
                hashPtr = &hash;
            }
            const InteractionStats interactionStats = InteractionSystem{}.apply(agents, foods, hashPtr, interactionConfig);
            const DeathStats deathStats = DeathSystem{}.apply(agents, deathConfig);
            const auto end = std::chrono::high_resolution_clock::now();

            results.push_back({
                scenario[0],
                scenario[1],
                useSpatial,
                std::chrono::duration<double, std::milli>(end - start).count(),
                interactionStats.foodsConsumed,
                deathStats.deaths,
            });
        }
    }

    return results;
}
} // namespace agentbiosim::systems
