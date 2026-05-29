#include "neural/Phase9Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "neural/ActivationTrace.hpp"
#include "neural/BrainFactory.hpp"
#include "neural/BrainType.hpp"
#include "systems/EnergySystem.hpp"
#include "systems/InteractionSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/NeuralSystem.hpp"
#include "systems/Phase7Diagnostics.hpp"
#include "systems/Phase8Diagnostics.hpp"

#include "simulation/FoodStore.hpp"
#include "simulation/World.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <sstream>

namespace agentbiosim::neural
{
namespace
{
constexpr double kEpsilon = 1.0e-7;

void addCheck(Phase9ValidationSummary& summary, const std::string& name, const bool condition)
{
    ++summary.checks;
    if (condition)
    {
        return;
    }
    summary.passed = false;
    summary.details += "FAILED " + name + "\n";
}

std::string hiddenLayerText(const std::vector<std::size_t>& hiddenLayers)
{
    std::ostringstream out;
    for (std::size_t index = 0; index < hiddenLayers.size(); ++index)
    {
        if (index > 0U)
        {
            out << '/';
        }
        out << hiddenLayers[index];
    }
    return out.str();
}

BrainConfig smallConfig(const std::string& name, const std::vector<std::size_t>& hiddenLayers, const std::size_t outputSize)
{
    (void)name;
    BrainConfig config;
    config.inputSize = systems::NeuralSystem::syntheticInputSize();
    config.outputSize = outputSize;
    config.hiddenLayers = hiddenLayers;
    config.mutationRate = 0.05;
    config.mutationStrength = 0.08;
    config.initStd = 1.0;
    config.randomBiases = true;
    return config;
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

simulation::AgentSpawn agentAt(const double x, const double y)
{
    simulation::AgentSpawn spawn;
    spawn.position = {x, y};
    spawn.radius = 5.0;
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

void populateAgents(simulation::AgentStore& agents, const int agentCount)
{
    agents.clear();
    std::mt19937 rng(20260527U + static_cast<std::uint32_t>(agentCount));
    std::uniform_real_distribution<double> xDistribution(20.0, 980.0);
    std::uniform_real_distribution<double> yDistribution(20.0, 680.0);
    std::uniform_real_distribution<double> angleDistribution(-3.14159265358979323846, 3.14159265358979323846);
    for (int index = 0; index < agentCount; ++index)
    {
        simulation::AgentSpawn spawn = agentAt(xDistribution(rng), yDistribution(rng));
        spawn.angle = angleDistribution(rng);
        static_cast<void>(agents.createAgent(spawn));
    }
}
} // namespace

Phase9ValidationSummary runPhase9Validation()
{
    Phase9ValidationSummary summary;
    summary.passed = true;

    const BrainType futureTypes[] = {
        normalizeBrainType("mlp"),
        normalizeBrainType("gated_mlp"),
        normalizeBrainType("shortcut_mlp"),
        normalizeBrainType("modulated_mlp"),
        normalizeBrainType("simple_rnn"),
        normalizeBrainType("neat_common"),
        normalizeBrainType("neat_simplified"),
        normalizeBrainType("neat_recurrent"),
    };
    summary.brainTypesRecognized = sizeof(futureTypes) / sizeof(futureTypes[0]);
    addCheck(summary, "BrainType recognizes MLP", futureTypes[0] == BrainType::Mlp && isImplementedInPhase9(futureTypes[0]));
    addCheck(summary, "BrainType recognizes future types without implementing them",
             futureTypes[1] == BrainType::GatedMlp && !isImplementedInPhase9(futureTypes[1]) &&
                 futureTypes[2] == BrainType::ShortcutMlp && futureTypes[3] == BrainType::ModulatedMlp &&
                 futureTypes[4] == BrainType::SimpleRnn && futureTypes[5] == BrainType::Neat &&
                 futureTypes[6] == BrainType::SimpleNeat && futureTypes[7] == BrainType::RecurrentNeat);

    const auto registry = config::createDefaultParameterRegistry();
    const systems::MovementConfig movementConfig = systems::MovementSystem::fromRegistry(registry);
    const systems::NeuralSystemConfig neuralConfig = systems::NeuralSystem::fromRegistry(registry, movementConfig, systems::NeuralSystem::syntheticInputSize());
    addCheck(summary, "BrainConfig uses synthetic input size", neuralConfig.brainConfig.inputSize == systems::NeuralSystem::syntheticInputSize());
    addCheck(summary, "BrainConfig forward output size is two", neuralConfig.brainConfig.outputSize == 2U);
    addCheck(summary, "BrainConfig reads default hidden layers", neuralConfig.brainConfig.hiddenLayers == std::vector<std::size_t>({20U, 20U, 20U, 20U}));
    addCheck(summary, "BrainConfig reads mutation parameters",
             std::abs(neuralConfig.brainConfig.mutationRate - 0.05) <= kEpsilon &&
                 std::abs(neuralConfig.brainConfig.mutationStrength - 0.08) <= kEpsilon);
    const BrainConfig predatorConfig = BrainFactory::configFromRegistry(registry, "predator", systems::NeuralSystem::syntheticInputSize(), 2U);
    addCheck(summary, "BrainConfig can read predator neural defaults for future species",
             predatorConfig.hiddenLayers == std::vector<std::size_t>({16U, 8U}) &&
                 std::abs(predatorConfig.mutationRate - 0.05) <= kEpsilon);

    std::mt19937_64 rngA(1234U);
    MLPBrain brainA(neuralConfig.brainConfig, rngA);
    const std::vector<double> input{0.25, 0.0, 0.5, 0.5};
    ActivationTrace trace;
    const std::vector<double> output = brainA.forward(input, &trace);
    addCheck(summary, "MLP creates correct layer sizes", brainA.layerSizes() == std::vector<std::size_t>({4U, 20U, 20U, 20U, 20U, 2U}));
    addCheck(summary, "MLP forward output size", output.size() == 2U);
    addCheck(summary, "ActivationTrace exposes dense layers", trace.layers.size() == brainA.layerSizes().size() - 1U);
    addCheck(summary, "Hidden layers use tanh range",
             std::all_of(trace.layers.begin(), trace.layers.end() - 1, [](const ActivationLayer& layer) {
                 return std::all_of(layer.values.begin(), layer.values.end(), [](const double value) {
                     return value >= -1.0 - kEpsilon && value <= 1.0 + kEpsilon;
                 });
             }));

    std::mt19937_64 rngB(1234U);
    MLPBrain brainB(neuralConfig.brainConfig, rngB);
    const std::vector<double> outputB = brainB.forward(input);
    addCheck(summary, "MLP deterministic with same seed and input", output == outputB);

    BrainConfig unsupportedConfig = neuralConfig.brainConfig;
    unsupportedConfig.requestedType = BrainType::GatedMlp;
    unsupportedConfig.type = BrainType::GatedMlp;
    unsupportedConfig.fallbackToMlp = true;
    unsupportedConfig.fallbackReason = "test future type";
    std::mt19937_64 futureTypeRng(1235U);
    const BrainCreationResult fallbackBrain = BrainFactory::createBrain(unsupportedConfig, futureTypeRng);
    addCheck(summary, "BrainFactory falls back cleanly for future types",
             fallbackBrain.fallbackToMlp &&
                 std::holds_alternative<MLPBrain>(fallbackBrain.brain) &&
                 fallbackBrain.instantiatedType == BrainType::Mlp);

    BrainConfig linearConfig;
    linearConfig.inputSize = 1U;
    linearConfig.outputSize = 1U;
    linearConfig.hiddenLayers = {};
    linearConfig.randomBiases = false;
    std::mt19937_64 rngLinear(44U);
    MLPBrain linearBrain(linearConfig, rngLinear);
    const bool testLayerSet = linearBrain.setLayerForTesting(0U, {10.0}, {0.0});
    const std::vector<double> linearOutput = linearBrain.forward({1.0});
    addCheck(summary, "Output layer is linear", testLayerSet && linearOutput.size() == 1U && linearOutput[0] > 1.0);

    const double beforeNoMutation = brainA.checksum();
    std::mt19937_64 noMutationRng(77U);
    const bool noMutationChanged = brainA.mutate(0.0, 1.0, noMutationRng);
    addCheck(summary, "mutation_rate zero does not alter weights", !noMutationChanged && std::abs(brainA.checksum() - beforeNoMutation) <= kEpsilon);

    std::mt19937_64 mutationRng(78U);
    const bool mutationChanged = brainA.mutate(1.0, 0.1, mutationRng);
    addCheck(summary, "mutation_rate positive alters weights", mutationChanged && std::abs(brainA.checksum() - beforeNoMutation) > kEpsilon);

    const MLPBrain cloned = brainA.clone();
    addCheck(summary, "clone preserves outputs", cloned.forward(input) == brainA.forward(input));

    std::mt19937_64 resizeRng(79U);
    const bool resized = brainA.resizeInput(6U, resizeRng);
    addCheck(summary, "resizeInput changes input size", resized && brainA.inputSize() == 6U && brainA.forward({0.1, 0.2, 0.3, 0.4, 0.5, 0.6}).size() == 2U);

    {
        simulation::AgentStore agents;
        static_cast<void>(agents.createAgent(agentAt(100.0, 100.0)));
        const simulation::World world = makeWorld();
        systems::NeuralSystem neuralSystem;
        const std::vector<systems::MovementControl> controls = neuralSystem.produceMovementControls(agents, world, neuralConfig);
        addCheck(summary, "NeuralSystem produces one MovementControl", controls.size() == 1U && neuralSystem.lastStats().agentsProcessed == 1U);
        systems::MovementConfig moveConfig = movementConfig;
        moveConfig.maxSpeed = 10.0;
        const systems::MovementStats movementStats = systems::MovementSystem{}.apply(agents, world, 1.0 / 30.0, moveConfig, &controls);
        summary.agentsTested += movementStats.agentsProcessed;
        addCheck(summary, "MLP output feeds MovementSystem without crash", movementStats.agentsProcessed == 1U);
    }

    {
        systems::MovementConfig omniMovement = movementConfig;
        omniMovement.mode = systems::MovementMode::Omni;
        const systems::NeuralSystemConfig omniConfig = systems::NeuralSystem::fromRegistry(registry, omniMovement, systems::NeuralSystem::syntheticInputSize());
        addCheck(summary, "Omni movement receives three outputs", omniConfig.brainConfig.outputSize == 3U);
    }

    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        static_cast<void>(agents.createAgent(agentAt(20.0, 20.0)));
        static_cast<void>(foods.createFood(foodAt(20.0, 20.0)));
        systems::InteractionConfig interactionConfig;
        interactionConfig.useSpatial = false;
        const systems::InteractionStats interactionStats = systems::InteractionSystem{}.apply(agents, foods, nullptr, interactionConfig);
        addCheck(summary, "Phase 7 interaction still works", interactionStats.foodsConsumed == 1U && foods.empty());
    }

    {
        const systems::Phase8ValidationSummary phase8 = systems::runPhase8Validation();
        addCheck(summary, "Phase 8 locomotion validation still passes", phase8.passed);
    }

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 9 validation checks passed. brainTypesRecognized="
                << summary.brainTypesRecognized << " agentsTested=" << summary.agentsTested
                << " inputSize=" << systems::NeuralSystem::syntheticInputSize() << " outputSize=2";
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase9BenchmarkResult> runPhase9Microbenchmark()
{
    const std::vector<std::pair<std::string, std::vector<std::size_t>>> architectures{
        {"small", {8U}},
        {"medium", {16U, 16U}},
        {"default_bacteria", {20U, 20U, 20U, 20U}},
    };
    const int agentCounts[] = {100, 300, 600, 1000};
    constexpr int repeats = 100;

    std::vector<Phase9BenchmarkResult> results;
    results.reserve(architectures.size() * 4U);
    for (const auto& architecture : architectures)
    {
        for (const int agentCount : agentCounts)
        {
            simulation::AgentStore agents;
            populateAgents(agents, agentCount);
            std::vector<std::vector<double>> inputs;
            inputs.reserve(agents.size());
            for (std::size_t index = 0; index < agents.size(); ++index)
            {
                const simulation::Vec2 position = agents.positionAt(index);
                inputs.push_back({
                    std::clamp(agents.energyAt(index) / 400.0, 0.0, 2.0),
                    std::clamp(agents.ageAt(index) / 3600.0, 0.0, 1.0),
                    std::clamp(position.x / 1000.0, 0.0, 1.0),
                    std::clamp(position.y / 700.0, 0.0, 1.0),
                });
            }

            std::vector<MLPBrain> brains;
            brains.reserve(agents.size());
            for (std::size_t index = 0; index < agents.size(); ++index)
            {
                BrainConfig config = smallConfig(architecture.first, architecture.second, 2U);
                std::mt19937_64 rng(20260527ULL + static_cast<std::uint64_t>(index));
                brains.emplace_back(config, rng);
            }

            volatile double sink = 0.0;
            const auto start = std::chrono::high_resolution_clock::now();
            for (int repeat = 0; repeat < repeats; ++repeat)
            {
                for (std::size_t index = 0; index < brains.size(); ++index)
                {
                    const std::vector<double> out = brains[index].forward(inputs[index]);
                    sink += out.empty() ? 0.0 : out[0];
                }
            }
            const auto end = std::chrono::high_resolution_clock::now();
            const double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
            static_cast<void>(sink);

            const int forwards = agentCount * repeats;
            results.push_back({
                architecture.first,
                systems::NeuralSystem::syntheticInputSize(),
                2U,
                hiddenLayerText(architecture.second),
                agentCount,
                forwards,
                totalMs,
                totalMs * 1000.0 / static_cast<double>(std::max(1, forwards)),
            });
        }
    }
    return results;
}
} // namespace agentbiosim::neural
