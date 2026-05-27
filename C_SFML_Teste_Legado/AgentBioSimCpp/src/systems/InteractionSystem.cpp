#include "systems/InteractionSystem.hpp"

#include "config/Parameter.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
double parameterDouble(const config::ParameterRegistry& parameters, const std::string& name, const double fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<double>(&definition->defaultValue))
    {
        return *value;
    }
    if (const auto* value = std::get_if<int>(&definition->defaultValue))
    {
        return static_cast<double>(*value);
    }
    return fallback;
}

bool parameterBool(const config::ParameterRegistry& parameters, const std::string& name, const bool fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<bool>(&definition->defaultValue))
    {
        return *value;
    }
    return fallback;
}
} // namespace

InteractionConfig InteractionSystem::fromRegistry(const config::ParameterRegistry& parameters)
{
    InteractionConfig config;
    config.useSpatial = parameterBool(parameters, "use_spatial", config.useSpatial);
    config.dietFood = parameterBool(parameters, "bacteria_diet_food", config.dietFood);
    config.foodEfficiency = parameterDouble(parameters, "bacteria_diet_food_efficiency", config.foodEfficiency);
    config.energyCap = parameterDouble(parameters, "bacteria_energy_cap", config.energyCap);
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
} // namespace agentbiosim::systems
