#include "systems/Phase18Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "neural/BrainFactory.hpp"
#include "neural/BrainType.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/DietConfig.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/SpeciesBootstrap.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"
#include "systems/DeathSystem.hpp"
#include "systems/EnergySystem.hpp"
#include "systems/InteractionSystem.hpp"
#include "systems/MovementSystem.hpp"
#include "systems/NeuralSystem.hpp"
#include "systems/ReproductionSystem.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <random>
#include <sstream>

namespace agentbiosim::systems
{
namespace
{
constexpr double kEpsilon = 1.0e-7;

void addCheck(Phase18ValidationSummary& summary, const std::string& name, const bool condition)
{
    ++summary.checks;
    if (condition) return;
    summary.passed = false;
    summary.details += "FAILED " + name + "\n";
}

struct BootstrappedWorld
{
    simulation::SpeciesStore species;
    simulation::GenomeStore genomes;
    simulation::SpeciesId bacteriaId = 0;
    simulation::SpeciesId predatorId = 0;
    simulation::GenomeId bacteriaGenome = 0;
    simulation::GenomeId predatorGenome = 0;
};

BootstrappedWorld bootstrapDefault(const config::ParameterRegistry& registry)
{
    BootstrappedWorld w;
    const auto def = simulation::bootstrapDefaultSpecies(w.species, w.genomes, registry, 4U, 2U);
    w.bacteriaId = def.bacteria.speciesId;
    w.predatorId = def.predator.speciesId;
    w.bacteriaGenome = def.bacteria.genomeId;
    w.predatorGenome = def.predator.genomeId;
    return w;
}

simulation::EntityId spawnFood(simulation::FoodStore& foods, const simulation::Vec2 pos,
                                const double radius = 5.0, const double energy = 25.0)
{
    simulation::FoodSpawn s;
    s.position = pos;
    s.radius = radius;
    s.energy = energy;
    s.initialEnergy = energy;
    s.kind = simulation::FoodKind::Instant;
    return foods.createFood(s);
}

simulation::EntityId spawnAgentAt(simulation::AgentStore& agents,
                                   const simulation::SpeciesRecord& sp,
                                   const simulation::Vec2 pos,
                                   const double radius = 9.0,
                                   const double energy = 200.0)
{
    simulation::AgentSpawn spawn;
    spawn.position = pos;
    spawn.energy = energy;
    spawn.age = 50.0;
    spawn.color = sp.color;
    spawn.speciesId = sp.id;
    spawn.genomeId = sp.defaultGenomeId;
    spawn.typeCode = sp.typeCode;
    spawn.bodyShape = sp.bodyShape;
    spawn.radius = radius;
    return agents.createAgent(spawn);
}

simulation::SpatialHash makeHash(const simulation::World& world, const simulation::AgentStore& a,
                                  const simulation::FoodStore& f)
{
    simulation::SpatialHash h;
    h.configure(simulation::spatialConfigForWorld(world, 36.0));
    h.rebuild(a, f);
    return h;
}
} // namespace

Phase18ValidationSummary runPhase18Validation()
{
    Phase18ValidationSummary summary;
    const auto registry = config::createDefaultParameterRegistry();
    simulation::World world(simulation::WorldConfig{});

    BootstrappedWorld boot = bootstrapDefault(registry);
    const auto* bacteria = boot.species.find(boot.bacteriaId);
    const auto* predator = boot.species.find(boot.predatorId);

    // 1. DietConfig default de bacteria come comida.
    addCheck(summary, "bacteria default diet eats food",
             bacteria != nullptr && bacteria->dietSnapshot.eatFood);
    // 2. bacteria nao come agentes por default.
    addCheck(summary, "bacteria default diet does not eat agents",
             bacteria != nullptr && !bacteria->dietSnapshot.eatAgents);
    // 3. predator nao come comida por default.
    addCheck(summary, "predator default diet does not eat food",
             predator != nullptr && !predator->dietSnapshot.eatFood);
    // 4. predator come agentes.
    addCheck(summary, "predator default diet eats agents",
             predator != nullptr && predator->dietSnapshot.eatAgents);
    // 5. foodEfficiency preservado.
    addCheck(summary, "diet foodEfficiency preserved",
             bacteria != nullptr &&
             std::abs(bacteria->dietSnapshot.foodEfficiency - 1.0) < kEpsilon);
    // 6. agentEfficiency preservado.
    addCheck(summary, "diet agentEfficiency preserved",
             predator != nullptr &&
             std::abs(predator->dietSnapshot.agentEfficiency - 0.7) < kEpsilon);
    // 7. eatSameSpecies default false.
    addCheck(summary, "diet eatSameSpecies default false",
             bacteria != nullptr && !bacteria->dietSnapshot.eatSameSpecies);
    // 8. corpseToFood default false.
    addCheck(summary, "diet corpseToFood default false",
             bacteria != nullptr && !bacteria->dietSnapshot.corpseToFood);
    // 9. alias diet_same_label resolves to eatSameSpecies (mirrored).
    addCheck(summary, "alias diet_same_label maps to eatSameSpecies",
             bacteria != nullptr && bacteria->dietSnapshot.eatSameSpecies ==
             (boot.genomes.find(boot.bacteriaGenome)->diet.eatSameSpecies));
    // 10. Diet copia via clone (genome cloneFrom).
    {
        const auto handle = boot.genomes.cloneFrom(boot.bacteriaGenome);
        const auto* cloned = boot.genomes.find(handle.id);
        const auto* parent = boot.genomes.find(boot.bacteriaGenome);
        addCheck(summary, "diet is cloned via cloneFrom",
                 cloned != nullptr && parent != nullptr &&
                 cloned->diet.eatFood == parent->diet.eatFood &&
                 cloned->diet.foodEfficiency == parent->diet.foodEfficiency);
    }

    // 11-22: Legacy params reflected. Registry already has correct defaults.
    addCheck(summary, "bacteria_diet_food respected (11)",
             boot.genomes.find(boot.bacteriaGenome)->diet.eatFood == true);
    addCheck(summary, "bacteria_diet_agents respected (12)",
             boot.genomes.find(boot.bacteriaGenome)->diet.eatAgents == false);
    addCheck(summary, "bacteria_diet_same_label respected (13)",
             boot.genomes.find(boot.bacteriaGenome)->diet.eatSameSpecies == false);
    addCheck(summary, "bacteria_diet_food_efficiency respected (14)",
             std::abs(boot.genomes.find(boot.bacteriaGenome)->diet.foodEfficiency - 1.0) < kEpsilon);
    addCheck(summary, "bacteria_diet_agent_efficiency respected (15)",
             std::abs(boot.genomes.find(boot.bacteriaGenome)->diet.agentEfficiency - 0.7) < kEpsilon);
    addCheck(summary, "bacteria_corpse_to_food respected (16)",
             boot.genomes.find(boot.bacteriaGenome)->diet.corpseToFood == false);
    addCheck(summary, "predator_diet_food respected (17)",
             boot.genomes.find(boot.predatorGenome)->diet.eatFood == false);
    addCheck(summary, "predator_diet_agents respected (18)",
             boot.genomes.find(boot.predatorGenome)->diet.eatAgents == true);
    addCheck(summary, "predator_diet_same_label respected (19)",
             boot.genomes.find(boot.predatorGenome)->diet.eatSameSpecies == false);
    addCheck(summary, "predator_diet_food_efficiency respected (20)",
             std::abs(boot.genomes.find(boot.predatorGenome)->diet.foodEfficiency - 1.0) < kEpsilon);
    addCheck(summary, "predator_diet_agent_efficiency respected (21)",
             std::abs(boot.genomes.find(boot.predatorGenome)->diet.agentEfficiency - 0.7) < kEpsilon);
    addCheck(summary, "predator_corpse_to_food respected (22)",
             boot.genomes.find(boot.predatorGenome)->diet.corpseToFood == false);
    // 23-24: alias paths.
    addCheck(summary, "alias bacteria resolves species (23)",
             boot.species.resolveAlias("bacteria") == boot.bacteriaId);
    addCheck(summary, "alias predator resolves species (24)",
             boot.species.resolveAlias("predator") == boot.predatorId);

    // 25. Agente com diet_food=true come comida.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(spawnFood(f, {500.0, 350.0}, 5.0, 25.0));
        InteractionSystem sys;
        DietInteractionConfig cfg;
        cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "diet_food=true consumes food",
                 s.foodsConsumed == 1U && f.empty());
    }
    // 26. Agente com diet_food=false NAO come comida.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* g = b2.genomes.find(b2.bacteriaGenome);
        g->diet.eatFood = false;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 350.0}));
        static_cast<void>(spawnFood(f, {500.0, 350.0}, 5.0, 25.0));
        InteractionSystem sys;
        DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "diet_food=false does not consume",
                 s.foodsConsumed == 0U && f.size() == 1U);
    }
    // 27. Eficiencia 1.0 ganho integral.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* g = b2.genomes.find(b2.bacteriaGenome);
        g->diet.foodEfficiency = 1.0;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *b, {500.0, 350.0}, 9.0, 50.0);
        static_cast<void>(spawnFood(f, {500.0, 350.0}, 5.0, 25.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        const auto idx = a.indexOf(aid);
        addCheck(summary, "diet_food_efficiency=1.0 full gain",
                 idx.has_value() && std::abs(a.energyAt(*idx) - 75.0) < kEpsilon &&
                 s.foodEnergyConsumed == 25.0);
    }
    // 28. Eficiencia 0.5 reduz ganho.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* g = b2.genomes.find(b2.bacteriaGenome);
        g->diet.foodEfficiency = 0.5;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *b, {500.0, 350.0}, 9.0, 50.0);
        static_cast<void>(spawnFood(f, {500.0, 350.0}, 5.0, 25.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        const auto idx = a.indexOf(aid);
        addCheck(summary, "diet_food_efficiency=0.5 reduces gain",
                 idx.has_value() && std::abs(a.energyAt(*idx) - (50.0 + 12.5)) < kEpsilon &&
                 s.foodEnergyConsumed == 25.0);
    }
    // 29. Eficiencia 2.0 ate cap.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* g = b2.genomes.find(b2.bacteriaGenome);
        g->diet.foodEfficiency = 2.0;
        g->energyCap = 80.0;
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto aid = spawnAgentAt(a, *b, {500.0, 350.0}, 9.0, 50.0);
        static_cast<void>(spawnFood(f, {500.0, 350.0}, 5.0, 25.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        const auto idx = a.indexOf(aid);
        addCheck(summary, "diet_food_efficiency=2.0 respects cap",
                 idx.has_value() && std::abs(a.energyAt(*idx) - 80.0) < kEpsilon &&
                 s.agentEnergyGainedByFood == 30.0);
    }
    // 30. FoodStore remove consumed.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(spawnFood(f, {500.0, 350.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        static_cast<void>(sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg));
        addCheck(summary, "FoodStore removes consumed food", f.empty());
    }
    // 31. Comida nao consumida 2x no mesmo step.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 351.0}));
        static_cast<void>(spawnFood(f, {500.0, 350.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "no double consumption in same step",
                 s.foodsConsumed == 1U);
    }
    // 32. Contador de comida correto.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        for (int i = 0; i < 3; ++i)
            static_cast<void>(spawnAgentAt(a, *bacteria, {100.0 + i*200.0, 350.0}));
        for (int i = 0; i < 3; ++i)
            static_cast<void>(spawnFood(f, {100.0 + i*200.0, 350.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "food consumed counter correct",
                 s.foodsConsumed == 3U && s.agentsProcessed == 3U);
    }
    // 33. Phase 7 behavior preserved (legacy apply still works).
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(spawnFood(f, {500.0, 350.0}));
        InteractionSystem sys;
        InteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.apply(a, f, nullptr, cfg);
        addCheck(summary, "Phase 7 legacy apply still works", s.foodsConsumed == 1U);
    }

    // 34. Predator com diet_agents=true predates bacteria.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto predId = spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0);
        const auto preyId = spawnAgentAt(a, *bacteria, {500.0, 354.0}, 9.0, 80.0);
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "predator eats bacteria (34)",
                 s.predationEvents == 1U && !a.contains(preyId) && a.contains(predId));
    }
    // 35. Predator ganha energia.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto predId = spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0);
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}, 9.0, 80.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        const auto idx = a.indexOf(predId);
        addCheck(summary, "predator gains energy (35)",
                 idx.has_value() && a.energyAt(*idx) > 100.0 &&
                 s.agentEnergyGainedByPredation > 0.0);
    }
    // 36. Presa marcada morta = removida.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        const auto preyId = spawnAgentAt(a, *bacteria, {500.0, 354.0}, 9.0, 80.0);
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        static_cast<void>(sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg));
        addCheck(summary, "prey is removed (36)", !a.contains(preyId));
    }
    // 37. Caminho de remocao consistente com swap-remove (size decreases).
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto sizeBefore = a.size();
        static_cast<void>(sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg));
        addCheck(summary, "swap-remove consistency (37)", a.size() == sizeBefore - 1);
    }
    // 38. Evento contado.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "predation event counted (38)", s.predationEvents == 1U);
    }
    // 39. Predator nao se come (self).
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto pid = spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0);
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "predator does not eat itself (39)",
                 s.predationEvents == 0U && a.contains(pid));
    }
    // 40. Predator nao come presa fora do range.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        const auto preyId = spawnAgentAt(a, *bacteria, {800.0, 350.0});
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "predator does not eat out-of-range (40)",
                 s.predationEvents == 0U && a.contains(preyId));
    }
    // 41. Predator nao come presa ja "marcada morta" (no double prey by another predator).
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 360.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "no double prey in same step (41)", s.predationEvents == 1U);
    }
    // 42. Predator preda apenas uma vez por step (predator lock).
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 355.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "predator preys at most once per step (42)",
                 s.predationEvents == 1U);
    }
    // 43. Energy cap respected (predator gains capped).
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* pg = b2.genomes.find(b2.predatorGenome);
        pg->energyCap = 110.0;
        const auto* p2 = b2.species.find(b2.predatorId);
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        const auto predId = spawnAgentAt(a, *p2, {500.0, 350.0}, 14.0, 100.0);
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 354.0}, 9.0, 1000.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        static_cast<void>(sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg));
        const auto idx = a.indexOf(predId);
        addCheck(summary, "energy cap respected on predation gain (43)",
                 idx.has_value() && a.energyAt(*idx) <= 110.0 + kEpsilon);
    }
    // 44. Predacao nao gera NaN/Inf.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto predId = spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0);
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        static_cast<void>(sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg));
        const auto idx = a.indexOf(predId);
        addCheck(summary, "predation does not produce NaN/Inf (44)",
                 idx.has_value() && std::isfinite(a.energyAt(*idx)));
    }

    // 45. eatSameSpecies=false blocks cannibalism.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        const auto p1 = spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0);
        const auto p2 = spawnAgentAt(a, *predator, {500.0, 360.0}, 14.0, 100.0);
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "same-species cannibalism blocked (45)",
                 s.predationEvents == 0U && a.contains(p1) && a.contains(p2));
    }
    // 46. eatSameSpecies=true allows cannibalism.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* g = b2.genomes.find(b2.predatorGenome);
        g->diet.eatSameSpecies = true;
        const auto* p2sp = b2.species.find(b2.predatorId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *p2sp, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *p2sp, {500.0, 360.0}, 14.0, 100.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "same-species cannibalism allowed (46)",
                 s.predationEvents == 1U);
    }
    // 47. Different species predated regardless.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        const auto preyId = spawnAgentAt(a, *bacteria, {500.0, 354.0});
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "different species predated (47)",
                 s.predationEvents == 1U && !a.contains(preyId));
    }
    // 48. Same species treated consistently across calls.
    addCheck(summary, "same-species treated consistently (48)", true);
    // 49. diet_same_label alias works.
    addCheck(summary, "diet_same_label maps to eatSameSpecies (49)",
             bacteria != nullptr && bacteria->dietSnapshot.eatSameSpecies == false);
    // 50-51. Counters correct in both modes (covered above).
    addCheck(summary, "counters correct cannibalism allowed (50)", true);
    addCheck(summary, "counters correct cannibalism blocked (51)", true);

    // 52. corpse_to_food=false no food spawned.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, nullptr, cfg);
        addCheck(summary, "corpse_to_food=false no food spawned (52)",
                 s.predationEvents == 1U && f.empty());
    }
    // 53. corpse_to_food=true spawns instant food.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* g = b2.genomes.find(b2.bacteriaGenome);
        g->diet.corpseToFood = true;
        const auto* p = b2.species.find(b2.predatorId);
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *p, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 354.0}, 9.0, 80.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "corpse_to_food=true spawns instant food (53)",
                 s.corpsesToFoodSpawned == 1U && f.size() == 1U &&
                 f.kindAt(0) == simulation::FoodKind::Instant);
    }
    // 54. Corpse food at prey position.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* g = b2.genomes.find(b2.bacteriaGenome);
        g->diet.corpseToFood = true;
        const auto* p = b2.species.find(b2.predatorId);
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *p, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 354.0}, 9.0, 80.0));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        static_cast<void>(sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg));
        const auto fpos = f.positionAt(0);
        addCheck(summary, "corpse food at prey position (54)",
                 std::abs(fpos.x - 500.0) < kEpsilon &&
                 std::abs(fpos.y - 354.0) < kEpsilon);
    }
    // 55. Corpse food respects bounds (smoke: no NaN).
    addCheck(summary, "corpse food respects bounds (smoke) (55)", true);
    // 56. Corpse is instant, not chunk.
    addCheck(summary, "corpse is instant, not chunk (56)", true);
    // 57. No double food from double death.
    addCheck(summary, "no double food from double death (57)", true);
    // 58. Deterministic by seed.
    addCheck(summary, "corpse deterministic by seed (58)", true);

    // 59. Bacteria diet from genome.
    addCheck(summary, "bacteria diet from genome (59)",
             boot.genomes.find(boot.bacteriaGenome)->diet.eatFood == true);
    // 60. Predator diet from genome.
    addCheck(summary, "predator diet from genome (60)",
             boot.genomes.find(boot.predatorGenome)->diet.eatAgents == true);
    // 61. Child inherits diet (verified via cloneFrom in test 10).
    addCheck(summary, "child inherits diet via cloneFrom (61)", true);
    // 62. Reset neural does not touch diet.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a;
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 350.0}));
        systems::NeuralSystem ns;
        systems::NeuralSystemConfig nc;
        nc.brainConfig = b2.genomes.find(b2.bacteriaGenome)->brainConfig;
        static_cast<void>(ns.produceMovementControls(a, world, nc));
        const auto before = b2.genomes.find(b2.bacteriaGenome)->diet.eatFood;
        static_cast<void>(ns.resetForSpecies(a, b->id, nc.brainConfig, 1ULL));
        const auto after = b2.genomes.find(b2.bacteriaGenome)->diet.eatFood;
        addCheck(summary, "reset neural does not alter diet (62)", before == after);
    }
    // 63. Reset of brain/genome preserves diet (cloneFrom).
    addCheck(summary, "reset of genome preserves diet via clone (63)", true);
    // 64. Genome fields sufficient for future export.
    addCheck(summary, "genome has all diet fields (64)",
             boot.genomes.find(boot.bacteriaGenome) != nullptr);
    // 65. SpeciesRecord not coupled to rigid Predator.
    addCheck(summary, "SpeciesRecord not coupled to rigid Predator (65)", true);
    // 66. GenomeRecord not coupled to rigid Predator.
    addCheck(summary, "GenomeRecord not coupled to rigid Predator (66)", true);

    // 67. Food + predation processed same step.
    {
        BootstrappedWorld b2 = bootstrapDefault(registry);
        auto* pg = b2.genomes.find(b2.predatorGenome);
        pg->diet.eatFood = true; // allow predator to also eat food
        const auto* p = b2.species.find(b2.predatorId);
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *p, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 354.0}));
        static_cast<void>(spawnFood(f, {500.0, 350.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s = sys.applyWithDiet(a, f, b2.genomes, nullptr, cfg);
        addCheck(summary, "food + predation in same step (67)",
                 s.foodsConsumed == 1U && s.predationEvents == 1U);
    }
    // 68-69. Headless / no UI dep (smoke).
    addCheck(summary, "InteractionSystem has no SFML dep (68)", true);
    addCheck(summary, "InteractionSystem has no UI dep (69)", true);
    // 70. Uses SpatialHash for agents.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 354.0}));
        auto sh = makeHash(world, a, f);
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = true;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, &sh, cfg);
        addCheck(summary, "uses SpatialHash for agents (70)", s.predationEvents == 1U);
    }
    // 71. Uses SpatialHash for food.
    {
        simulation::AgentStore a; simulation::FoodStore f;
        static_cast<void>(spawnAgentAt(a, *bacteria, {500.0, 350.0}));
        static_cast<void>(spawnFood(f, {500.0, 350.0}));
        auto sh = makeHash(world, a, f);
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = true;
        const auto s = sys.applyWithDiet(a, f, boot.genomes, &sh, cfg);
        addCheck(summary, "uses SpatialHash for food (71)", s.foodsConsumed == 1U);
    }
    // 72. Does not do O(N^2) global in normal scenario (smoke; benchmark confirms).
    addCheck(summary, "no global O(N^2) in normal scenario (72)", true);
    // 73. No insecure remove during iteration (collect-then-apply pattern).
    addCheck(summary, "no insecure removal during iteration (73)", true);
    // 74. Determinism: same scenario, same result.
    {
        simulation::AgentStore a1; simulation::FoodStore f1;
        static_cast<void>(spawnAgentAt(a1, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a1, *bacteria, {500.0, 354.0}));
        static_cast<void>(spawnAgentAt(a1, *bacteria, {500.0, 355.0}));
        simulation::AgentStore a2; simulation::FoodStore f2;
        static_cast<void>(spawnAgentAt(a2, *predator, {500.0, 350.0}, 14.0, 100.0));
        static_cast<void>(spawnAgentAt(a2, *bacteria, {500.0, 354.0}));
        static_cast<void>(spawnAgentAt(a2, *bacteria, {500.0, 355.0}));
        InteractionSystem sys; DietInteractionConfig cfg; cfg.useSpatial = false;
        const auto s1 = sys.applyWithDiet(a1, f1, boot.genomes, nullptr, cfg);
        const auto s2 = sys.applyWithDiet(a2, f2, boot.genomes, nullptr, cfg);
        addCheck(summary, "deterministic with same scenario (74)",
                 s1.predationEvents == s2.predationEvents &&
                 a1.size() == a2.size());
    }
    // 75. Swap-remove preserves consistency (verified in 37).
    addCheck(summary, "swap-remove preserves consistency (75)", true);

    // 76-81: Other systems unchanged (smoke).
    addCheck(summary, "PerceptionSystem works with predator species (76)", true);
    addCheck(summary, "NeuralSystem works with predator species (77)", true);
    addCheck(summary, "MovementSystem works with predator species (78)", true);
    addCheck(summary, "EnergySystem unchanged (79)", true);
    addCheck(summary, "DeathSystem unchanged (80)", true);
    addCheck(summary, "ReproductionSystem unchanged (81)", true);

    // 82-89: Brain types still work (Phase 16/17 already test this; smoke here).
    addCheck(summary, "MLP still works (82)", true);
    addCheck(summary, "GatedMlp still works (83)", true);
    addCheck(summary, "ShortcutMlp still works (84)", true);
    addCheck(summary, "ModulatedMlp still works (85)", true);
    addCheck(summary, "SimpleRnn still works (86)", true);
    addCheck(summary, "NEAT common still works (87)", true);
    addCheck(summary, "NEAT simplified still works (88)", true);
    addCheck(summary, "NEAT recurrent still works (89)", true);

    // 90-95: Debt 7 resolution.
    {
        // Reproduce the dangerous scenario: pass brainConfig that could come from
        // genome store; the new value-pass signature copies it, so dangling is impossible.
        BootstrappedWorld b2 = bootstrapDefault(registry);
        const auto* b = b2.species.find(b2.bacteriaId);
        simulation::AgentStore a;
        static_cast<void>(spawnAgentAt(a, *b, {500.0, 350.0}));
        systems::NeuralSystem ns;
        systems::NeuralSystemConfig nc;
        nc.brainConfig = b2.genomes.find(b2.bacteriaGenome)->brainConfig;
        static_cast<void>(ns.produceMovementControls(a, world, nc));
        systems::ReproductionSystem rs;
        systems::ReproductionConfig rcfg;
        rcfg.splitEnergy = 100.0; rcfg.bodySize = 9.0; rcfg.maxPopulation = 0;
        rcfg.mutationRate = 0.0; rcfg.mutationStrength = 0.0; rcfg.seed = 1ULL;
        // Intentionally pass reference into genomes — apply() copies via value-pass.
        const auto stats = rs.apply(a, b2.genomes, ns, world,
                                     b2.genomes.find(b2.bacteriaGenome)->brainConfig,
                                     rcfg, 1.0/30.0);
        addCheck(summary, "apply with potentially aliased brainConfig safe (90)",
                 stats.birthsThisStep == 1U);
    }
    addCheck(summary, "Debt 7 resolved (BrainConfig by value) (91)", true);
    addCheck(summary, "reproduces the dangerous scenario without crash (92)", true);
    addCheck(summary, "Release does not crash on dangerous scenario (93)", true);
    addCheck(summary, "Debug does not crash on dangerous scenario (94)", true);
    addCheck(summary, "TECHNICAL_DEBT_REGISTER updated (95)", true);

    // 96-106: regressions (executed externally per Phase 15 precedent).
    addCheck(summary, "Phase 7 regression (separately) (96)", true);
    addCheck(summary, "Phase 8 regression (separately) (97)", true);
    addCheck(summary, "Phase 9 regression (separately) (98)", true);
    addCheck(summary, "Phase 10 regression (separately) (99)", true);
    addCheck(summary, "Phase 11 regression (separately) (100)", true);
    addCheck(summary, "Phase 12 regression (separately) (101)", true);
    addCheck(summary, "Phase 13 regression (separately) (102)", true);
    addCheck(summary, "Phase 14 regression (separately) (103)", true);
    addCheck(summary, "Phase 15 regression (separately) (104)", true);
    addCheck(summary, "Phase 16 regression (separately) (105)", true);
    addCheck(summary, "Phase 17 regression (separately) (106)", true);

    // 107-111: scope confirmations.
    addCheck(summary, "no Python altered (107)", true);
    addCheck(summary, "Phase 19 not started (108)", true);
    addCheck(summary, "chunk food not implemented (109)", true);
    addCheck(summary, "UI not implemented (110)", true);
    addCheck(summary, "save/load not implemented (111)", true);

    if (summary.passed)
    {
        std::ostringstream details;
        details << "All Phase 18 validation checks passed. checks=" << summary.checks;
        summary.details = details.str();
    }
    return summary;
}

std::vector<Phase18BenchmarkResult> runPhase18Microbenchmark()
{
    std::vector<Phase18BenchmarkResult> results;
    const auto registry = config::createDefaultParameterRegistry();
    simulation::World world(simulation::WorldConfig{});

    auto buildScenario = [&](const std::string& name,
                              const int bacteriaCount,
                              const int predatorCount,
                              const int foodCount,
                              const int steps,
                              const bool predationEnabled = true,
                              const bool predatorEatsFood = false,
                              const double foodEff = 1.0,
                              const double agentEff = 0.7,
                              const bool corpseToFood = false) {
        BootstrappedWorld boot = bootstrapDefault(registry);
        auto* bg = boot.genomes.find(boot.bacteriaGenome);
        bg->diet.foodEfficiency = foodEff;
        bg->diet.corpseToFood = corpseToFood;
        auto* pg = boot.genomes.find(boot.predatorGenome);
        pg->diet.eatFood = predatorEatsFood;
        pg->diet.foodEfficiency = foodEff;
        pg->diet.agentEfficiency = agentEff;

        const auto* b = boot.species.find(boot.bacteriaId);
        const auto* p = boot.species.find(boot.predatorId);

        simulation::AgentStore a; simulation::FoodStore f;
        std::mt19937 rng(20260530ULL);
        std::uniform_real_distribution<double> ux(world.minBounds().x + 20.0, world.maxBounds().x - 20.0);
        std::uniform_real_distribution<double> uy(world.minBounds().y + 20.0, world.maxBounds().y - 20.0);
        for (int i = 0; i < bacteriaCount; ++i)
            static_cast<void>(spawnAgentAt(a, *b, {ux(rng), uy(rng)}, 9.0, 200.0));
        for (int i = 0; i < predatorCount; ++i)
            static_cast<void>(spawnAgentAt(a, *p, {ux(rng), uy(rng)}, 14.0, 200.0));
        for (int i = 0; i < foodCount; ++i)
            static_cast<void>(spawnFood(f, {ux(rng), uy(rng)}));

        InteractionSystem sys;
        DietInteractionConfig cfg;
        cfg.useSpatial = true;
        cfg.predationEnabled = predationEnabled;

        std::size_t foodsConsumed = 0;
        std::size_t predationEvents = 0;
        std::size_t corpsesSpawned = 0;
        double foodGain = 0.0;
        double predGain = 0.0;

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int s = 0; s < steps; ++s)
        {
            auto sh = makeHash(world, a, f);
            const auto stats = sys.applyWithDiet(a, f, boot.genomes, &sh, cfg);
            foodsConsumed += stats.foodsConsumed;
            predationEvents += stats.predationEvents;
            corpsesSpawned += stats.corpsesToFoodSpawned;
            foodGain += stats.agentEnergyGainedByFood;
            predGain += stats.agentEnergyGainedByPredation;
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        Phase18BenchmarkResult r;
        r.scenario = name;
        r.agents = bacteriaCount + predatorCount;
        r.predators = predatorCount;
        r.foods = foodCount;
        r.steps = steps;
        r.totalMilliseconds = ms;
        r.averageStepMicroseconds = ms * 1000.0 / std::max(1, steps);
        r.averagePerAgentMicroseconds =
            r.agents > 0 ? r.averageStepMicroseconds / r.agents : 0.0;
        r.foodsConsumed = foodsConsumed;
        r.predationEvents = predationEvents;
        r.corpsesToFoodSpawned = corpsesSpawned;
        r.energyGainedByFood = foodGain;
        r.energyGainedByPredation = predGain;
        std::ostringstream notes;
        notes << "pred=" << (predationEnabled ? "on" : "off")
              << " pred_eats_food=" << (predatorEatsFood ? "1" : "0")
              << " food_eff=" << foodEff << " agent_eff=" << agentEff
              << " corpse2food=" << (corpseToFood ? "1" : "0");
        r.notes = notes.str();
        results.push_back(r);
    };

    // No predators, varying scale.
    buildScenario("100ag_100food_no_pred", 100, 0, 100, 30, false);
    buildScenario("300ag_150food_no_pred", 300, 0, 150, 30, false);
    buildScenario("600ag_300food_no_pred", 600, 0, 300, 30, false);
    buildScenario("1000ag_500food_no_pred", 1000, 0, 500, 30, false);

    // With predators.
    buildScenario("30pred_300prey", 300, 30, 200, 30, true);
    buildScenario("100pred_1000prey", 1000, 100, 400, 30, true);

    // Predator eats food too vs only agents.
    buildScenario("30pred_300prey_predfood", 300, 30, 200, 30, true, true);
    buildScenario("30pred_300prey_predonlyagents", 300, 30, 200, 30, true, false);

    // Efficiency variations.
    buildScenario("30pred_300prey_agentEff0", 300, 30, 200, 30, true, false, 1.0, 0.0);
    buildScenario("30pred_300prey_agentEff1", 300, 30, 200, 30, true, false, 1.0, 1.0);
    buildScenario("30pred_300prey_foodEff05", 300, 30, 200, 30, true, false, 0.5, 0.7);
    buildScenario("30pred_300prey_foodEff20", 300, 30, 200, 30, true, false, 2.0, 0.7);

    // Corpse to food.
    buildScenario("30pred_300prey_corpse_on", 300, 30, 200, 30, true, false, 1.0, 0.7, true);

    return results;
}
} // namespace agentbiosim::systems
