#include "systems/InteractionSystem.hpp"

#include "config/ParameterHelpers.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace agentbiosim::systems
{
using config::parameterBool;
using config::parameterDouble;

InteractionConfig InteractionSystem::fromRegistry(const config::ParameterRegistry& parameters)
{
    InteractionConfig config;
    config.useSpatial = parameterBool(parameters, "use_spatial", config.useSpatial);
    config.dietFood = parameterBool(parameters, "bacteria_diet_food", config.dietFood);
    config.foodEfficiency = parameterDouble(parameters, "bacteria_diet_food_efficiency", config.foodEfficiency);
    config.energyCap = parameterDouble(parameters, "bacteria_energy_cap", config.energyCap);
    return config;
}

DietInteractionConfig InteractionSystem::dietConfigFromRegistry(const config::ParameterRegistry& parameters)
{
    DietInteractionConfig config;
    config.useSpatial = parameterBool(parameters, "use_spatial", config.useSpatial);
    config.predationEnabled = parameterBool(parameters, "predators_enabled", config.predationEnabled);
    config.defaultEnergyCap = parameterDouble(parameters, "bacteria_energy_cap", config.defaultEnergyCap);
    return config;
}

InteractionStats InteractionSystem::apply(simulation::AgentStore& agents,
                                          simulation::FoodStore& foods,
                                          simulation::SpatialHash* spatialHash,
                                          const InteractionConfig& config) const
{
    InteractionStats stats;
    if (!config.dietFood || agents.empty() || foods.empty())
    {
        return stats;
    }

    const bool useSpatial = config.useSpatial && spatialHash != nullptr && !spatialHash->empty();
    std::vector<simulation::SpatialItem> candidates;
    std::vector<simulation::EntityId> consumedFoodIds;
    std::unordered_set<std::uint64_t> consumedFoodSet;

    for (std::size_t agentIndex = 0; agentIndex < agents.size(); ++agentIndex)
    {
        if (!agents.aliveAt(agentIndex))
        {
            continue;
        }
        ++stats.agentsProcessed;

        bool consumedThisAgent = false;
        const simulation::Vec2 agentPosition = agents.positionAt(agentIndex);
        const double agentRadius = agents.radiusAt(agentIndex);

        if (useSpatial)
        {
            spatialHash->queryRadiusInto(agentPosition.x, agentPosition.y, agentRadius, candidates);
            for (const simulation::SpatialItem& candidate : candidates)
            {
                if (candidate.entityType != simulation::SpatialEntityType::Food ||
                    consumedFoodSet.find(candidate.id.value) != consumedFoodSet.end())
                {
                    continue;
                }
                const std::optional<std::size_t> foodIndex = foods.indexOf(candidate.id);
                if (!foodIndex.has_value() || !foods.aliveAt(*foodIndex) || !touching(agents, agentIndex, foods, *foodIndex))
                {
                    continue;
                }
                if (foods.kindAt(*foodIndex) != simulation::FoodKind::Instant)
                {
                    ++stats.chunkFoodsSkipped;
                    continue;
                }
                const double foodEnergy = std::max(0.0, foods.energyAt(*foodIndex));
                const double gained = agents.addEnergyAt(agentIndex, foodEnergy * std::max(0.0, config.foodEfficiency), config.energyCap);
                stats.foodEnergyConsumed += foodEnergy;
                stats.agentEnergyGained += gained;
                consumedFoodIds.push_back(candidate.id);
                consumedFoodSet.insert(candidate.id.value);
                ++stats.foodsConsumed;
                consumedThisAgent = true;
                break;
            }
        }
        else
        {
            for (std::size_t foodIndex = 0; foodIndex < foods.size(); ++foodIndex)
            {
                const simulation::EntityId foodId = foods.idAt(foodIndex);
                if (!foods.aliveAt(foodIndex) || consumedFoodSet.find(foodId.value) != consumedFoodSet.end() ||
                    !touching(agents, agentIndex, foods, foodIndex))
                {
                    continue;
                }
                if (foods.kindAt(foodIndex) != simulation::FoodKind::Instant)
                {
                    ++stats.chunkFoodsSkipped;
                    continue;
                }
                const double foodEnergy = std::max(0.0, foods.energyAt(foodIndex));
                const double gained = agents.addEnergyAt(agentIndex, foodEnergy * std::max(0.0, config.foodEfficiency), config.energyCap);
                stats.foodEnergyConsumed += foodEnergy;
                stats.agentEnergyGained += gained;
                consumedFoodIds.push_back(foodId);
                consumedFoodSet.insert(foodId.value);
                ++stats.foodsConsumed;
                consumedThisAgent = true;
                break;
            }
        }

        if (consumedThisAgent && foods.empty())
        {
            break;
        }
    }

    for (const simulation::EntityId id : consumedFoodIds)
    {
        static_cast<void>(foods.removeFood(id));
    }

    return stats;
}

bool InteractionSystem::touching(const simulation::AgentStore& agents,
                                 const std::size_t agentIndex,
                                 const simulation::FoodStore& foods,
                                 const std::size_t foodIndex)
{
    const simulation::Vec2 agentPosition = agents.positionAt(agentIndex);
    const simulation::Vec2 foodPosition = foods.positionAt(foodIndex);
    const double dx = foodPosition.x - agentPosition.x;
    const double dy = foodPosition.y - agentPosition.y;
    const double radiusSum = agents.radiusAt(agentIndex) + foods.radiusAt(foodIndex);
    return dx * dx + dy * dy <= radiusSum * radiusSum;
}

namespace
{
struct PredationEvent
{
    std::uint64_t predatorId = 0;
    std::uint64_t preyId = 0;
    double energyGain = 0.0;
    simulation::Vec2 preyPosition{};
    double preyInitialEnergy = 0.0;
    double preyBodyRadius = 0.0;
    simulation::ColorRgb preyColor{};
    bool preyCorpseToFood = false;
};

bool touchingAgents(const simulation::AgentStore& agents,
                    const std::size_t aIndex,
                    const std::size_t bIndex)
{
    const simulation::Vec2 pa = agents.positionAt(aIndex);
    const simulation::Vec2 pb = agents.positionAt(bIndex);
    const double dx = pb.x - pa.x;
    const double dy = pb.y - pa.y;
    const double rsum = agents.radiusAt(aIndex) + agents.radiusAt(bIndex);
    return dx * dx + dy * dy <= rsum * rsum;
}

const simulation::DietConfig* dietOf(const simulation::AgentStore& agents,
                                      const std::size_t agentIndex,
                                      const simulation::GenomeStore& genomes)
{
    const simulation::GenomeId gid = agents.genomeIdAt(agentIndex);
    const simulation::GenomeRecord* g = genomes.find(gid);
    return g == nullptr ? nullptr : &g->diet;
}

double energyCapOf(const simulation::AgentStore& agents,
                    const std::size_t agentIndex,
                    const simulation::GenomeStore& genomes,
                    const double fallback)
{
    const simulation::GenomeId gid = agents.genomeIdAt(agentIndex);
    const simulation::GenomeRecord* g = genomes.find(gid);
    return g == nullptr ? fallback : g->energyCap;
}
} // namespace

DietInteractionStats InteractionSystem::applyWithDiet(simulation::AgentStore& agents,
                                                       simulation::FoodStore& foods,
                                                       const simulation::GenomeStore& genomes,
                                                       simulation::SpatialHash* spatialHash,
                                                       const DietInteractionConfig& config) const
{
    DietInteractionStats stats;
    if (agents.empty()) return stats;

    const bool useSpatial = config.useSpatial && spatialHash != nullptr && !spatialHash->empty();
    std::vector<simulation::SpatialItem> candidates;
    std::vector<simulation::EntityId> consumedFoodIds;
    std::unordered_set<std::uint64_t> consumedFoodSet;
    std::unordered_set<std::uint64_t> markedDead;
    std::unordered_set<std::uint64_t> predatorsLockedThisStep;
    std::vector<PredationEvent> predationEvents;
    predationEvents.reserve(agents.size() / 4U + 1U);

    // Pre-snapshot agent count so newly spawned (corpse-to-food, future ops)
    // are not iterated this step.
    const std::size_t snapshotSize = agents.size();

    for (std::size_t agentIndex = 0; agentIndex < snapshotSize; ++agentIndex)
    {
        if (!agents.aliveAt(agentIndex)) continue;
        const std::uint64_t agentId = agents.idAt(agentIndex).value;
        if (markedDead.find(agentId) != markedDead.end()) continue;

        const simulation::DietConfig* diet = dietOf(agents, agentIndex, genomes);
        if (diet == nullptr)
        {
            ++stats.blockedDietDisabled;
            continue;
        }
        ++stats.agentsProcessed;

        const simulation::Vec2 agentPos = agents.positionAt(agentIndex);
        const double agentRadius = agents.radiusAt(agentIndex);
        const double energyCap = energyCapOf(agents, agentIndex, genomes, config.defaultEnergyCap);

        // 1. Food consumption.
        if (diet->eatFood && !foods.empty())
        {
            bool consumedFood = false;
            if (useSpatial)
            {
                spatialHash->queryRadiusInto(agentPos.x, agentPos.y, agentRadius, candidates);
                for (const simulation::SpatialItem& cand : candidates)
                {
                    if (cand.entityType != simulation::SpatialEntityType::Food) continue;
                    if (consumedFoodSet.find(cand.id.value) != consumedFoodSet.end()) continue;
                    const auto foodIndex = foods.indexOf(cand.id);
                    if (!foodIndex.has_value() || !foods.aliveAt(*foodIndex)) continue;
                    if (!touching(agents, agentIndex, foods, *foodIndex)) continue;
                    if (foods.kindAt(*foodIndex) != simulation::FoodKind::Instant)
                    {
                        ++stats.chunkFoodsSkipped;
                        continue;
                    }
                    const double foodEnergy = std::max(0.0, foods.energyAt(*foodIndex));
                    const double gained = agents.addEnergyAt(agentIndex,
                        foodEnergy * std::max(0.0, diet->foodEfficiency), energyCap);
                    stats.foodEnergyConsumed += foodEnergy;
                    stats.agentEnergyGainedByFood += gained;
                    consumedFoodIds.push_back(cand.id);
                    consumedFoodSet.insert(cand.id.value);
                    ++stats.foodsConsumed;
                    consumedFood = true;
                    break;
                }
            }
            else
            {
                for (std::size_t fi = 0; fi < foods.size(); ++fi)
                {
                    const simulation::EntityId fid = foods.idAt(fi);
                    if (consumedFoodSet.find(fid.value) != consumedFoodSet.end()) continue;
                    if (!foods.aliveAt(fi)) continue;
                    if (!touching(agents, agentIndex, foods, fi)) continue;
                    if (foods.kindAt(fi) != simulation::FoodKind::Instant)
                    {
                        ++stats.chunkFoodsSkipped;
                        continue;
                    }
                    const double foodEnergy = std::max(0.0, foods.energyAt(fi));
                    const double gained = agents.addEnergyAt(agentIndex,
                        foodEnergy * std::max(0.0, diet->foodEfficiency), energyCap);
                    stats.foodEnergyConsumed += foodEnergy;
                    stats.agentEnergyGainedByFood += gained;
                    consumedFoodIds.push_back(fid);
                    consumedFoodSet.insert(fid.value);
                    ++stats.foodsConsumed;
                    consumedFood = true;
                    break;
                }
            }
            // Food consumption does not block predation this step (predator can still hunt).
            static_cast<void>(consumedFood);
        }

        // 2. Predation.
        if (!config.predationEnabled || !diet->eatAgents) continue;
        if (predatorsLockedThisStep.find(agentId) != predatorsLockedThisStep.end()) continue;

        const simulation::SpeciesId predatorSpecies = agents.speciesIdAt(agentIndex);

        // Deterministic prey selection: pick the candidate with smallest agent ID
        // that satisfies all constraints (lives, touches, not self, species rules,
        // not already marked dead, not already eaten this step).
        std::uint64_t bestPreyId = 0;
        std::size_t bestPreyIndex = static_cast<std::size_t>(-1);
        double bestPreyEnergy = 0.0;

        auto consider = [&](const std::size_t preyIndex) {
            if (preyIndex == agentIndex) return;
            if (!agents.aliveAt(preyIndex)) return;
            const std::uint64_t preyId = agents.idAt(preyIndex).value;
            if (markedDead.find(preyId) != markedDead.end()) return;
            if (!diet->eatSameSpecies &&
                agents.speciesIdAt(preyIndex) == predatorSpecies)
            {
                // Track block stat once per attempt; cheap counter.
                ++stats.blockedSameSpecies;
                return;
            }
            if (!touchingAgents(agents, agentIndex, preyIndex)) return;
            if (bestPreyId == 0U || preyId < bestPreyId)
            {
                bestPreyId = preyId;
                bestPreyIndex = preyIndex;
                bestPreyEnergy = agents.energyAt(preyIndex);
            }
        };

        if (useSpatial)
        {
            spatialHash->queryRadiusInto(agentPos.x, agentPos.y, agentRadius * 2.0, candidates);
            for (const simulation::SpatialItem& cand : candidates)
            {
                if (cand.entityType != simulation::SpatialEntityType::Agent) continue;
                const auto preyIndex = agents.indexOf(cand.id);
                if (!preyIndex.has_value()) continue;
                consider(*preyIndex);
            }
        }
        else
        {
            for (std::size_t pi = 0; pi < snapshotSize; ++pi)
            {
                consider(pi);
            }
        }

        if (bestPreyIndex == static_cast<std::size_t>(-1)) continue;

        // Energy gain: prey's current energy times agentEfficiency.
        const double gain = std::max(0.0, bestPreyEnergy) *
                            std::max(0.0, diet->agentEfficiency);
        // Mark predator + prey for this step.
        markedDead.insert(bestPreyId);
        predatorsLockedThisStep.insert(agentId);

        PredationEvent ev;
        ev.predatorId = agentId;
        ev.preyId = bestPreyId;
        ev.energyGain = gain;
        ev.preyPosition = agents.positionAt(bestPreyIndex);
        ev.preyBodyRadius = agents.radiusAt(bestPreyIndex);
        ev.preyInitialEnergy = std::max(1.0, bestPreyEnergy);
        ev.preyColor = agents.colorAt(bestPreyIndex);
        const simulation::DietConfig* preyDiet = dietOf(agents, bestPreyIndex, genomes);
        ev.preyCorpseToFood = preyDiet != nullptr ? preyDiet->corpseToFood : false;
        predationEvents.push_back(ev);
    }

    // Apply predation: gain energy first, then remove prey and spawn corpse food.
    for (const PredationEvent& ev : predationEvents)
    {
        const auto predIndex = agents.indexOf({ev.predatorId});
        if (predIndex.has_value() && agents.aliveAt(*predIndex))
        {
            const double cap = energyCapOf(agents, *predIndex, genomes, config.defaultEnergyCap);
            const double gained = agents.addEnergyAt(*predIndex, ev.energyGain, cap);
            stats.agentEnergyGainedByPredation += gained;
        }
        if (agents.removeAgent({ev.preyId}))
        {
            ++stats.predationEvents;
            if (ev.preyCorpseToFood)
            {
                simulation::FoodSpawn corpse;
                corpse.position = ev.preyPosition;
                corpse.radius = std::max(1.0, ev.preyBodyRadius * 0.5);
                corpse.energy = ev.preyInitialEnergy;
                corpse.initialEnergy = ev.preyInitialEnergy;
                corpse.color = ev.preyColor;
                corpse.kind = simulation::FoodKind::Instant;
                static_cast<void>(foods.createFood(corpse));
                ++stats.corpsesToFoodSpawned;
            }
        }
    }

    for (const simulation::EntityId fid : consumedFoodIds)
    {
        static_cast<void>(foods.removeFood(fid));
    }

    return stats;
}
} // namespace agentbiosim::systems
