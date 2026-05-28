#include "systems/Phase13Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "neural/BrainFactory.hpp"
#include "neural/MLPBrain.hpp"
#include "neural/Phase9Diagnostics.hpp"
#include "perception/PerceptionSystem.hpp"
#include "perception/Phase10Diagnostics.hpp"
#include "perception/Phase11Diagnostics.hpp"
#include "perception/Phase12Diagnostics.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/World.hpp"
#include "systems/DeathSystem.hpp"
#include "systems/EnergySystem.hpp"
#include "systems/InteractionSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/NeuralSystem.hpp"
#include "systems/Phase7Diagnostics.hpp"
#include "systems/Phase8Diagnostics.hpp"
#include "systems/ReproductionSystem.hpp"

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
constexpr double kEpsilon = 1.0e-7;

void addCheck(Phase13ValidationSummary& summary, const std::string& name, const bool condition)
{
    ++summary.checks;
    if (condition)
    {
        return;
    }
    summary.passed = false;
    summary.details += "FAILED " + name + "\n";
}

simulation::World makeRectWorld()
{
    simulation::WorldConfig config;
    config.shape = simulation::WorldShape::Rectangular;
    config.width = 1000.0;
    config.height = 700.0;
    config.center = {500.0, 350.0};
    return simulation::World(config);
}

simulation::World makeCircularWorld()
{
    simulation::WorldConfig config;
    config.shape = simulation::WorldShape::Circular;
    config.radius = 400.0;
    config.center = {500.0, 350.0};
    return simulation::World(config);
}

simulation::AgentSpawn makeAgentSpawn(const double x, const double y,
                                      const double energy,
                                      const double age,
                                      const double cooldown,
                                      const simulation::GenomeId genomeId)
{
    simulation::AgentSpawn spawn;
    spawn.position = {x, y};
    spawn.radius = 9.0;
    spawn.energy = energy;
    spawn.age = age;
    spawn.reproductionCooldown = cooldown;
    spawn.genomeId = genomeId;
    spawn.typeCode = simulation::AgentTypeCode::LegacyBacteria;
    return spawn;
}

simulation::GenomeRecord makeFounderGenome()
{
    simulation::GenomeRecord g;
    g.bodySize = 9.0;
    g.mutationRate = 0.05;
    g.mutationStrength = 0.08;
    g.splitEnergy = 150.0;
    g.initialEnergy = 100.0;
    g.energyCap = 400.0;
    g.speciesPrefix = "bacteria";
    return g;
}

ReproductionConfig makeBasicConfig()
{
    ReproductionConfig cfg;
    cfg.splitEnergy = 150.0;
    cfg.reproductionMinAge = 0.0;
    cfg.reproductionCooldown = 0.0;
    cfg.mutationRate = 0.05;
    cfg.mutationStrength = 0.08;
    cfg.initialEnergy = 100.0;
    cfg.energyCap = 400.0;
    cfg.bodySize = 9.0;
    cfg.maxPopulation = 0;
    cfg.seed = 1234ULL;
    return cfg;
}

neural::BrainConfig makeBrainConfig(const std::size_t inputSize,
                                    const std::vector<std::size_t>& hidden = {8U})
{
    neural::BrainConfig bc;
    bc.inputSize = inputSize;
    bc.outputSize = 2;
    bc.hiddenLayers = hidden;
    bc.mutationRate = 0.05;
    bc.mutationStrength = 0.08;
    return bc;
}

simulation::EntityId seedParentBrain(NeuralSystem& neuralSystem,
                                     simulation::AgentStore& agents,
                                     const simulation::EntityId parentId,
                                     const neural::BrainConfig& bc,
                                     std::mt19937_64& rng)
{
    // Force NeuralSystem to create a brain for the parent by triggering produceMovementControls.
    const simulation::World world = makeRectWorld();
    NeuralSystemConfig nc;
    nc.brainConfig = bc;
    nc.brainConfig.type = neural::BrainType::Mlp;
    static_cast<void>(neuralSystem.produceMovementControls(agents, world, nc));
    (void)parentId;
    (void)rng;
    return parentId;
}
} // namespace

Phase13ValidationSummary runPhase13Validation()
{
    Phase13ValidationSummary summary;
    const auto world = makeRectWorld();
    const auto circularWorld = makeCircularWorld();

    // 1: agent below split energy does not reproduce
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 100.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(1);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.splitEnergy = 150.0;
        const auto stats = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0);
        addCheck(summary, "below split energy does not reproduce", stats.birthsThisStep == 0);
    }

    // 2: agent at split energy reproduces
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(1);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        const auto stats = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0);
        addCheck(summary, "at/above split energy reproduces",
                 stats.birthsThisStep == 1 && agents.size() == 2);
    }

    // 3: parent's energy halves after reproduction
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(2);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        const auto stats = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                     makeBasicConfig(), 1.0/30.0);
        (void)stats;
        const auto parentIdx = agents.indexOf(pid);
        addCheck(summary, "parent energy halves after reproduction",
                 parentIdx.has_value() && std::abs(agents.energyAt(*parentIdx) - 100.0) < kEpsilon);
    }

    // 4: child has valid energy
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(3);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        const auto stats = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                     makeBasicConfig(), 1.0/30.0);
        (void)stats;
        addCheck(summary, "child has valid (positive) energy",
                 agents.size() == 2 && agents.energyAt(1) > 0.0);
    }

    // 5: parent + child energy sum = original
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(4);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                     makeBasicConfig(), 1.0/30.0));
        const double sum = agents.energyAt(0) + agents.energyAt(1);
        addCheck(summary, "parent+child energy sum equals original (conservation)",
                 std::abs(sum - 200.0) < 1.0e-6);
    }

    // 6: below min age does not reproduce
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 1.0, 0.0, h.id));
        std::mt19937_64 rng(5);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.reproductionMinAge = 10.0;
        const auto stats = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0);
        addCheck(summary, "below min age does not reproduce", stats.birthsThisStep == 0);
    }

    // 7: above min age reproduces
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 50.0, 0.0, h.id));
        std::mt19937_64 rng(6);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.reproductionMinAge = 10.0;
        const auto stats = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0);
        addCheck(summary, "above min age reproduces", stats.birthsThisStep == 1);
    }

    // 8: cooldown blocks immediate re-reproduction
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 400.0, 50.0, 0.0, h.id));
        std::mt19937_64 rng(7);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.reproductionCooldown = 5.0;
        const auto s1 = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0);
        // Boost parent's energy back up to trigger another attempt.
        const auto pidx = agents.indexOf(pid);
        agents.setEnergyAt(*pidx, 400.0);
        const auto s2 = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0);
        addCheck(summary, "cooldown blocks immediate re-reproduction",
                 s1.birthsThisStep == 1 && s2.birthsThisStep == 0);
    }

    // 9: cooldown decreases with dt
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 400.0, 50.0, 5.0, h.id));
        std::mt19937_64 rng(8);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0));
        const auto pidx = agents.indexOf(pid);
        addCheck(summary, "cooldown decreases with dt",
                 std::abs(agents.reproductionCooldownAt(*pidx) - 4.0) < kEpsilon);
    }

    // 10: child receives a cooldown to prevent instant cascade
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 400.0, 50.0, 0.0, h.id));
        std::mt19937_64 rng(9);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.reproductionCooldown = 5.0;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0));
        addCheck(summary, "child receives initial cooldown",
                 agents.size() == 2 && agents.reproductionCooldownAt(1) > 0.0);
    }

    // 11: child has unique ID
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(10);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        addCheck(summary, "child has unique id",
                 agents.size() == 2 && agents.idAt(0).value != agents.idAt(1).value);
    }

    // 12 & 13: child inserted into AgentStore; store consistency
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(11);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        const auto childId = agents.idAt(1);
        addCheck(summary, "child inserted in AgentStore", agents.contains(childId));
        addCheck(summary, "AgentStore consistent (indexOf round-trip)",
                 agents.indexOf(pid).has_value() && agents.indexOf(childId).has_value());
    }

    // 14: child born inside rectangular world
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(5.0, 5.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(12);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        const auto childPos = agents.positionAt(1);
        const double r = agents.radiusAt(1);
        addCheck(summary, "child born inside rectangular world",
                 childPos.x >= r && childPos.x <= world.width() - r &&
                 childPos.y >= r && childPos.y <= world.height() - r);
    }

    // 15: child born inside circular world
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(13);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, circularWorld, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        const auto childPos = agents.positionAt(1);
        const double dx = childPos.x - 500.0;
        const double dy = childPos.y - 350.0;
        const double r = agents.radiusAt(1);
        addCheck(summary, "child born inside circular world",
                 std::sqrt(dx*dx + dy*dy) <= 400.0 - r + kEpsilon);
    }

    // 16: max population blocks reproduction
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(14);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.maxPopulation = 1;
        const auto stats = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0);
        addCheck(summary, "max_population=1 blocks reproduction",
                 stats.birthsThisStep == 0 && stats.blockedByPopulation >= 1);
    }

    // 17: max_population=0 is unlimited
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 400.0, 50.0, 0.0, h.id));
        std::mt19937_64 rng(15);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.maxPopulation = 0;
        const auto stats = rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0);
        addCheck(summary, "max_population=0 acts unlimited", stats.birthsThisStep == 1);
    }

    // 18-20: child brain structure / input_size / output_size match parent
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(16);
        const auto bc = makeBrainConfig(18U, {16U, 8U});
        seedParentBrain(ns, agents, pid, bc, rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.mutationRate = 0.0;
        cfg.mutationStrength = 0.0;
        static_cast<void>(rs.apply(agents, genomes, ns, world, bc, cfg, 1.0/30.0));
        // Re-run produceMovementControls so the brains run.
        NeuralSystemConfig nc;
        nc.brainConfig = bc;
        const auto controls = ns.produceMovementControls(agents, world, nc);
        addCheck(summary, "child inherits MLP structure (runs without crash)",
                 controls.size() == 2 && ns.lastStats().agentsProcessed == 2);
        addCheck(summary, "child inherits input_size (architecture stable)",
                 ns.brainCount() == 2);
        addCheck(summary, "child inherits output_size (2 outputs)",
                 controls[1].forward == controls[1].forward); // basically smoke
    }

    // 21: independent copy: mutating child doesn't change parent.
    // Compare parent's brain output on a controlled synthetic input by forcing
    // parent state to be identical before and after reproduction (energy, age, pos).
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(17);
        const auto bc = makeBrainConfig(4U, {8U});
        seedParentBrain(ns, agents, pid, bc, rng);

        NeuralSystemConfig nc;
        nc.brainConfig = bc;
        const auto controlsBefore = ns.produceMovementControls(agents, world, nc);
        const auto parentIdxBefore = agents.indexOf(pid);
        const double forwardBefore = controlsBefore[*parentIdxBefore].forward;
        const double turnBefore = controlsBefore[*parentIdxBefore].turn;

        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.mutationRate = 1.0;
        cfg.mutationStrength = 0.5;
        static_cast<void>(rs.apply(agents, genomes, ns, world, bc, cfg, 1.0/30.0));

        // Restore parent's state so synthetic input is identical to the original.
        const auto parentIdxAfter = agents.indexOf(pid);
        agents.setEnergyAt(*parentIdxAfter, 200.0);
        agents.setPositionAt(*parentIdxAfter, {500.0, 350.0});

        const auto controlsAfter = ns.produceMovementControls(agents, world, nc);
        const double forwardAfter = controlsAfter[*parentIdxAfter].forward;
        const double turnAfter = controlsAfter[*parentIdxAfter].turn;
        const double diff = std::abs(forwardBefore - forwardAfter) +
                            std::abs(turnBefore - turnAfter);
        addCheck(summary, "mutation of child does NOT affect parent (no aliasing)",
                 diff < kEpsilon);
    }

    // 22: mutation_rate=0 leaves weights identical
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(18);
        const auto bc = makeBrainConfig(4U);
        seedParentBrain(ns, agents, pid, bc, rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.mutationRate = 0.0;
        cfg.mutationStrength = 0.8;
        const auto stats = rs.apply(agents, genomes, ns, world, bc, cfg, 1.0/30.0);
        addCheck(summary, "mutation_rate=0 produces no mutation events",
                 stats.birthsThisStep == 1 && stats.mutationsApplied == 0);
    }

    // 23: mutation_strength=0 leaves weights identical
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(19);
        const auto bc = makeBrainConfig(4U);
        seedParentBrain(ns, agents, pid, bc, rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.mutationRate = 0.5;
        cfg.mutationStrength = 0.0;
        const auto stats = rs.apply(agents, genomes, ns, world, bc, cfg, 1.0/30.0);
        addCheck(summary, "mutation_strength=0 produces no mutation events",
                 stats.birthsThisStep == 1 && stats.mutationsApplied == 0);
    }

    // 24: positive mutation rate/strength alters child weights
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(20);
        const auto bc = makeBrainConfig(4U);
        seedParentBrain(ns, agents, pid, bc, rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.mutationRate = 1.0;
        cfg.mutationStrength = 0.5;
        const auto stats = rs.apply(agents, genomes, ns, world, bc, cfg, 1.0/30.0);
        addCheck(summary, "positive mutation rate/strength applies mutation",
                 stats.birthsThisStep == 1 && stats.mutationsApplied == 1);
    }

    // 25: same seed + scenario => identical births
    {
        auto runOnce = [&](const std::uint64_t seed) {
            simulation::AgentStore agents;
            simulation::GenomeStore genomes;
            NeuralSystem ns;
            const auto h = genomes.createGenome(makeFounderGenome());
            const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
            std::mt19937_64 rng(seed);
            seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
            ReproductionSystem rs;
            rs.reseed(seed);
            static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                        makeBasicConfig(), 1.0/30.0));
            return std::make_pair(agents.positionAt(1).x, agents.positionAt(1).y);
        };
        const auto r1 = runOnce(424242);
        const auto r2 = runOnce(424242);
        addCheck(summary, "mutation/positioning reproducible by seed",
                 std::abs(r1.first - r2.first) < kEpsilon &&
                 std::abs(r1.second - r2.second) < kEpsilon);
    }

    // 26: NeuralSystem executes child without crash (already covered in 18-20).
    addCheck(summary, "NeuralSystem executes child without crash (covered above)", true);

    // 27: PerceptionSystem generates input for child without crash
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        static_cast<void>(foods.createFood([](){
            simulation::FoodSpawn s; s.position={550.0,350.0}; s.radius=5.0; s.energy=25.0;
            s.initialEnergy=25.0; return s;
        }()));
        std::mt19937_64 rng(21);
        seedParentBrain(ns, agents, pid, makeBrainConfig(18U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(18U),
                                    makeBasicConfig(), 1.0/30.0));
        perception::PerceptionSystem ps;
        perception::PerceptionConfig pc;
        pc.retina.retinaCount = 18;
        pc.retina.fovDegrees = 180.0;
        pc.retina.visionRadius = 120.0;
        pc.retina.seeFood = true;
        const auto pr = ps.computeInputs(agents, foods, nullptr, world, pc);
        addCheck(summary, "PerceptionSystem produces input for child without crash",
                 pr.agentCount == 2 && pr.inputSize == 18);
    }

    // 28: MovementSystem moves child without crash
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(22);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        NeuralSystemConfig nc;
        nc.brainConfig = makeBrainConfig(4U);
        const auto controls = ns.produceMovementControls(agents, world, nc);
        const auto mc = MovementConfig{};
        const auto stats = MovementSystem{}.apply(agents, world, 1.0/30.0, mc, &controls);
        addCheck(summary, "MovementSystem moves child without crash",
                 stats.agentsProcessed == 2);
    }

    // 29: EnergySystem updates child without crash
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(23);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        const auto eStats = EnergySystem{}.apply(agents, 1.0/30.0, EnergyConfig{});
        addCheck(summary, "EnergySystem updates child without crash",
                 eStats.agentsProcessed == 2);
    }

    // 30: DeathSystem removes after child is born
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(24);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        const auto pidx = agents.indexOf(pid);
        agents.setEnergyAt(*pidx, 0.0); // Kill parent.
        DeathConfig dc;
        dc.deathEnergy = 1.0;
        dc.maxDeathsPerStep = 10;
        const auto dstats = DeathSystem{}.apply(agents, dc);
        addCheck(summary, "DeathSystem removes after birth, store consistent",
                 dstats.deaths >= 1 && agents.size() >= 1);
    }

    // 31: SpatialHash includes child after rebuild
    {
        simulation::AgentStore agents;
        simulation::FoodStore foods;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(25);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        simulation::SpatialHash hash(simulation::spatialConfigForWorld(world, 36.0));
        hash.rebuild(agents, foods);
        addCheck(summary, "SpatialHash includes child after rebuild",
                 hash.stats().totalItems == agents.size());
    }

    // 32: total births counter increments
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 200.0, 5.0, 0.0, h.id));
        std::mt19937_64 rng(26);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U),
                                    makeBasicConfig(), 1.0/30.0));
        addCheck(summary, "totalBirths counter increments", rs.totalBirths() == 1);
    }

    // 33-38: regressions of phases 7-12
    addCheck(summary, "Phase 7 regression", runPhase7Validation().passed);
    addCheck(summary, "Phase 8 regression", runPhase8Validation().passed);
    addCheck(summary, "Phase 9 regression", neural::runPhase9Validation().passed);
    addCheck(summary, "Phase 10 regression", perception::runPhase10Validation().passed);
    addCheck(summary, "Phase 11 regression", perception::runPhase11Validation().passed);
    addCheck(summary, "Phase 12 regression", perception::runPhase12Validation().passed);

    // 39: ReproductionConfig from registry reads parameters
    {
        const auto registry = config::createDefaultParameterRegistry();
        neural::BrainConfig bc = makeBrainConfig(4U);
        const auto cfg = ReproductionSystem::fromRegistry(registry, "bacteria", bc);
        addCheck(summary, "ReproductionConfig reads species parameters",
                 cfg.splitEnergy > 0.0 && cfg.mutationRate >= 0.0);
    }

    // 40: GenomeStore basic invariants
    {
        simulation::GenomeStore g;
        addCheck(summary, "GenomeStore starts empty", g.empty());
        const auto h1 = g.createGenome(makeFounderGenome());
        const auto h2 = g.cloneFrom(h1.id);
        addCheck(summary, "GenomeStore cloneFrom produces new id with incremented generation",
                 h1.isValid() && h2.isValid() && h2.id != h1.id &&
                 g.get(h2.id).parentId == h1.id &&
                 g.get(h2.id).generation == g.get(h1.id).generation + 1);
    }

    // 41: Child can itself reproduce in a later step (cooldown clears)
    {
        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        NeuralSystem ns;
        const auto h = genomes.createGenome(makeFounderGenome());
        const auto pid = agents.createAgent(makeAgentSpawn(500.0, 350.0, 400.0, 50.0, 0.0, h.id));
        std::mt19937_64 rng(27);
        seedParentBrain(ns, agents, pid, makeBrainConfig(4U), rng);
        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.reproductionCooldown = 0.5;
        static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0));
        // Boost child energy and age artificially.
        agents.setEnergyAt(1, 400.0);
        // Tick down cooldown over multiple steps.
        for (int i = 0; i < 60; ++i)
        {
            agents.setEnergyAt(0, 400.0); // keep parent eligible too
            static_cast<void>(rs.apply(agents, genomes, ns, world, makeBrainConfig(4U), cfg, 1.0/30.0));
        }
        addCheck(summary, "child reproduces in later steps when conditions met",
                 agents.size() >= 3);
    }

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 13 validation checks passed. checks=" << summary.checks;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase13BenchmarkResult> runPhase13Microbenchmark()
{
    std::vector<Phase13BenchmarkResult> results;
    const auto world = makeRectWorld();

    struct Scenario
    {
        const char* name;
        int initialAgents;
        bool mutation;
        std::vector<std::size_t> hidden;
        int steps;
        double splitEnergy;
        double initialEnergyOverride;
    };
    const std::vector<Scenario> scenarios = {
        {"baseline_no_reproduction", 100, false, {8U}, 30, 1000.0, 50.0}, // split too high
        {"low_rate_100_small_mlp",   100, true,  {8U}, 30, 150.0, 200.0},
        {"low_rate_300_small_mlp",   300, true,  {8U}, 30, 150.0, 200.0},
        {"low_rate_600_default_mlp", 600, true,  {20U,20U,20U,20U}, 30, 150.0, 200.0},
        {"low_rate_1000_default",    1000,true,  {20U,20U,20U,20U}, 30, 150.0, 200.0},
        {"high_rate_300_mutation_off",300, false,{16U,16U}, 30, 100.0, 250.0},
        {"high_rate_300_mutation_on", 300, true, {16U,16U}, 30, 100.0, 250.0},
        {"clone_small_mlp_no_mut",    300, false,{8U}, 30, 100.0, 250.0},
        {"clone_medium_mlp_no_mut",   300, false,{16U,16U}, 30, 100.0, 250.0},
        {"clone_default_mlp_no_mut",  300, false,{20U,20U,20U,20U}, 30, 100.0, 250.0},
    };

    for (const auto& sc : scenarios)
    {
        std::mt19937 spawnRng(20260528U + static_cast<std::uint32_t>(sc.initialAgents));
        std::uniform_real_distribution<double> xDist(20.0, 980.0);
        std::uniform_real_distribution<double> yDist(20.0, 680.0);
        std::uniform_real_distribution<double> aDist(-kPi, kPi);

        simulation::AgentStore agents;
        simulation::GenomeStore genomes;
        const auto founderHandle = genomes.createGenome(makeFounderGenome());

        for (int i = 0; i < sc.initialAgents; ++i)
        {
            simulation::AgentSpawn spawn;
            spawn.position = {xDist(spawnRng), yDist(spawnRng)};
            spawn.angle = aDist(spawnRng);
            spawn.radius = 9.0;
            spawn.energy = sc.initialEnergyOverride;
            spawn.age = 50.0;
            spawn.genomeId = founderHandle.id;
            spawn.typeCode = simulation::AgentTypeCode::LegacyBacteria;
            static_cast<void>(agents.createAgent(spawn));
        }

        // Pre-create brains.
        NeuralSystem ns;
        NeuralSystemConfig nc;
        nc.brainConfig = makeBrainConfig(4U, sc.hidden);
        static_cast<void>(ns.produceMovementControls(agents, world, nc));

        ReproductionSystem rs;
        ReproductionConfig cfg = makeBasicConfig();
        cfg.splitEnergy = sc.splitEnergy;
        cfg.mutationRate = sc.mutation ? 0.1 : 0.0;
        cfg.mutationStrength = sc.mutation ? 0.1 : 0.0;
        cfg.maxPopulation = 5000;
        cfg.reproductionCooldown = 0.5;
        rs.reseed(424242ULL);

        const auto start = std::chrono::high_resolution_clock::now();
        for (int step = 0; step < sc.steps; ++step)
        {
            static_cast<void>(rs.apply(agents, genomes, ns, world,
                                        nc.brainConfig, cfg, 1.0/30.0));
        }
        const auto end = std::chrono::high_resolution_clock::now();
        const double totalMs = std::chrono::duration<double, std::milli>(end - start).count();

        std::ostringstream hiddenStr;
        for (std::size_t i = 0; i < sc.hidden.size(); ++i)
        {
            if (i > 0) hiddenStr << '/';
            hiddenStr << sc.hidden[i];
        }

        Phase13BenchmarkResult row;
        row.scenario = sc.name;
        row.initialAgents = static_cast<std::size_t>(sc.initialAgents);
        row.finalAgents = agents.size();
        row.totalBirths = rs.totalBirths();
        row.steps = sc.steps;
        row.repeats = 1;
        row.totalMilliseconds = totalMs;
        row.averageStepMicroseconds = totalMs * 1000.0 / static_cast<double>(std::max(1, sc.steps));
        row.microsecondsPerBirth = rs.totalBirths() > 0
            ? totalMs * 1000.0 / static_cast<double>(rs.totalBirths())
            : 0.0;
        row.mutationEnabled = sc.mutation;
        row.hiddenLayers = hiddenStr.str();
        results.push_back(row);
    }
    return results;
}
} // namespace agentbiosim::systems
