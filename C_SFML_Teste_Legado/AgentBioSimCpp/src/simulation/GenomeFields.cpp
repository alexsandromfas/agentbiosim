#include "simulation/GenomeFields.hpp"

#include <algorithm>
#include <cctype>
#include <variant>

namespace agentbiosim::simulation
{
namespace
{
// Tolerant extraction: the UI may push a double for an int slot, an int for a
// bool, etc. We coerce so a recognized field never silently drops a value.
double asNum(const config::ParameterValue& v, const double fallback)
{
    if (const auto* p = std::get_if<double>(&v)) return *p;
    if (const auto* p = std::get_if<int>(&v)) return static_cast<double>(*p);
    if (const auto* p = std::get_if<bool>(&v)) return *p ? 1.0 : 0.0;
    return fallback;
}
bool asFlag(const config::ParameterValue& v, const bool fallback)
{
    if (const auto* p = std::get_if<bool>(&v)) return *p;
    if (const auto* p = std::get_if<int>(&v)) return *p != 0;
    if (const auto* p = std::get_if<double>(&v)) return *p != 0.0;
    return fallback;
}
std::string asStr(const config::ParameterValue& v, const std::string& fallback)
{
    if (const auto* p = std::get_if<std::string>(&v)) return *p;
    return fallback;
}
// Same mapping bootstrapSpecies uses (ellipse default; circle/circular/circulo).
BodyShapeCode toShape(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (value == "circle" || value == "circular" || value == "circulo")
    {
        return BodyShapeCode::Circle;
    }
    return BodyShapeCode::Ellipse;
}
const char* shapeStr(const BodyShapeCode code) noexcept
{
    return code == BodyShapeCode::Circle ? "circle" : "ellipse";
}
ReproductionMode toReproMode(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return (value == "age" || value == "idade") ? ReproductionMode::Age : ReproductionMode::Energy;
}
const char* reproModeStr(const ReproductionMode mode) noexcept
{
    return mode == ReproductionMode::Age ? "age" : "energy";
}
int asInt(const config::ParameterValue& v, const int fallback)
{
    if (const auto* p = std::get_if<int>(&v)) return *p;
    if (const auto* p = std::get_if<double>(&v)) return static_cast<int>(*p);
    return fallback;
}
} // namespace

bool setGenomeField(GenomeRecord& g, const std::string& f, const config::ParameterValue& v)
{
    // Body / locomotion (only the body subset is on the genome in 34.1).
    if (f == "body_size")  { g.bodySize = std::max(0.1, asNum(v, g.bodySize)); return true; }
    if (f == "body_shape") { g.bodyShape = toShape(asStr(v, shapeStr(g.bodyShape))); return true; }
    // Locomotion (Fase 34.2: per-individual; read by MovementSystem).
    if (f == "max_speed") { g.maxSpeed = std::max(0.0, asNum(v, g.maxSpeed)); return true; }
    if (f == "max_turn")  { g.maxTurn = std::max(0.0, asNum(v, g.maxTurn)); return true; }
    if (f == "allow_reverse_locomotion") { g.allowReverse = asFlag(v, g.allowReverse); return true; }
    // Energy / reproduction.
    if (f == "initial_energy") { g.initialEnergy = std::max(0.0, asNum(v, g.initialEnergy)); return true; }
    if (f == "split_energy")   { g.splitEnergy = std::max(0.0, asNum(v, g.splitEnergy)); return true; }
    if (f == "energy_cap")     { g.energyCap = std::max(0.0, asNum(v, g.energyCap)); return true; }
    if (f == "death_energy")   { g.deathEnergy = std::max(0.0, asNum(v, g.deathEnergy)); return true; }
    // Metabolic costs (Fase 34.2: per-individual; read by EnergySystem).
    if (f == "metab_v0_cost")   { g.moveCostV0 = std::max(0.0, asNum(v, g.moveCostV0)); return true; }
    if (f == "metab_vmax_cost") { g.moveCostVmax = std::max(0.0, asNum(v, g.moveCostVmax)); return true; }
    if (f == "reproduction_min_age")  { g.reproductionMinAge = std::max(0.0, asNum(v, g.reproductionMinAge)); return true; }
    if (f == "reproduction_cooldown") { g.reproductionCooldown = std::max(0.0, asNum(v, g.reproductionCooldown)); return true; }
    if (f == "reproduction_mode") { g.reproductionMode = toReproMode(asStr(v, reproModeStr(g.reproductionMode))); return true; }
    if (f == "offspring_count")   { g.offspringCount = std::max(1, asInt(v, g.offspringCount)); return true; }
    // Mutation.
    if (f == "mutation_rate")     { g.mutationRate = std::clamp(asNum(v, g.mutationRate), 0.0, 1.0); return true; }
    if (f == "mutation_strength") { g.mutationStrength = std::max(0.0, asNum(v, g.mutationStrength)); return true; }
    // Diet.
    if (f == "diet_food")             { g.diet.eatFood = asFlag(v, g.diet.eatFood); return true; }
    if (f == "diet_agents")           { g.diet.eatAgents = asFlag(v, g.diet.eatAgents); return true; }
    if (f == "diet_same_label")       { g.diet.eatSameSpecies = asFlag(v, g.diet.eatSameSpecies); return true; }
    if (f == "diet_food_efficiency")  { g.diet.foodEfficiency = std::max(0.0, asNum(v, g.diet.foodEfficiency)); return true; }
    if (f == "diet_agent_efficiency") { g.diet.agentEfficiency = std::max(0.0, asNum(v, g.diet.agentEfficiency)); return true; }
    if (f == "corpse_to_food")        { g.diet.corpseToFood = asFlag(v, g.diet.corpseToFood); return true; }
    // Vision flags (what it perceives; geometry stays global in 34.1).
    if (f == "retina_see_food")          { g.vision.seeFood = asFlag(v, g.vision.seeFood); return true; }
    if (f == "retina_see_bacteria")      { g.vision.seeAgents = asFlag(v, g.vision.seeAgents); return true; }
    if (f == "retina_see_predators")     { g.vision.seePredators = asFlag(v, g.vision.seePredators); return true; }
    if (f == "retina_see_obstacles")     { g.vision.seeObstacles = asFlag(v, g.vision.seeObstacles); return true; }
    if (f == "retina_see_all")           { g.vision.seeAll = asFlag(v, g.vision.seeAll); return true; }
    if (f == "retina_see_through_walls") { g.vision.seeThroughWalls = asFlag(v, g.vision.seeThroughWalls); return true; }
    return false;
}

std::optional<config::ParameterValue> genomeFieldValue(const GenomeRecord& g, const std::string& f)
{
    if (f == "body_size")  return config::ParameterValue{g.bodySize};
    if (f == "body_shape") return config::ParameterValue{std::string(shapeStr(g.bodyShape))};
    if (f == "max_speed") return config::ParameterValue{g.maxSpeed};
    if (f == "max_turn")  return config::ParameterValue{g.maxTurn};
    if (f == "allow_reverse_locomotion") return config::ParameterValue{g.allowReverse};
    if (f == "initial_energy") return config::ParameterValue{g.initialEnergy};
    if (f == "split_energy")   return config::ParameterValue{g.splitEnergy};
    if (f == "energy_cap")     return config::ParameterValue{g.energyCap};
    if (f == "death_energy")   return config::ParameterValue{g.deathEnergy};
    if (f == "metab_v0_cost")   return config::ParameterValue{g.moveCostV0};
    if (f == "metab_vmax_cost") return config::ParameterValue{g.moveCostVmax};
    if (f == "reproduction_min_age")  return config::ParameterValue{g.reproductionMinAge};
    if (f == "reproduction_cooldown") return config::ParameterValue{g.reproductionCooldown};
    if (f == "reproduction_mode") return config::ParameterValue{std::string(reproModeStr(g.reproductionMode))};
    if (f == "offspring_count")   return config::ParameterValue{g.offspringCount};
    if (f == "mutation_rate")     return config::ParameterValue{g.mutationRate};
    if (f == "mutation_strength") return config::ParameterValue{g.mutationStrength};
    if (f == "diet_food")             return config::ParameterValue{g.diet.eatFood};
    if (f == "diet_agents")           return config::ParameterValue{g.diet.eatAgents};
    if (f == "diet_same_label")       return config::ParameterValue{g.diet.eatSameSpecies};
    if (f == "diet_food_efficiency")  return config::ParameterValue{g.diet.foodEfficiency};
    if (f == "diet_agent_efficiency") return config::ParameterValue{g.diet.agentEfficiency};
    if (f == "corpse_to_food")        return config::ParameterValue{g.diet.corpseToFood};
    if (f == "retina_see_food")          return config::ParameterValue{g.vision.seeFood};
    if (f == "retina_see_bacteria")      return config::ParameterValue{g.vision.seeAgents};
    if (f == "retina_see_predators")     return config::ParameterValue{g.vision.seePredators};
    if (f == "retina_see_obstacles")     return config::ParameterValue{g.vision.seeObstacles};
    if (f == "retina_see_all")           return config::ParameterValue{g.vision.seeAll};
    if (f == "retina_see_through_walls") return config::ParameterValue{g.vision.seeThroughWalls};
    return std::nullopt;
}

bool isGenomeField(const std::string& field) noexcept
{
    return genomeFieldValue(GenomeRecord{}, field).has_value();
}
} // namespace agentbiosim::simulation
