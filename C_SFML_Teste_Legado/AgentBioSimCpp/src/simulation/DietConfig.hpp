#pragma once

namespace agentbiosim::simulation
{
// Phase 18: dietary configuration owned by a GenomeRecord (so it inherits via
// `cloneFrom`) and mirrored on the SpeciesRecord as a snapshot for UI/diagnostics.
//
// Names:
//  - eatFood          <- bacteria_diet_food / predator_diet_food
//  - eatAgents        <- bacteria_diet_agents / predator_diet_agents
//  - eatSameSpecies   <- bacteria_diet_same_label / predator_diet_same_label
//                       (alias diet_same_species and diet_same_label resolve to this)
//  - foodEfficiency   <- bacteria_diet_food_efficiency / predator_diet_food_efficiency
//  - agentEfficiency  <- bacteria_diet_agent_efficiency / predator_diet_agent_efficiency
//  - corpseToFood     <- bacteria_corpse_to_food / predator_corpse_to_food
//
// Bacteria default: eatFood=true, eatAgents=false, eatSameSpecies=false,
//                   foodEfficiency=1.0, agentEfficiency=0.7, corpseToFood=false.
// Predator default: eatFood=false, eatAgents=true, eatSameSpecies=false,
//                   foodEfficiency=1.0, agentEfficiency=0.7, corpseToFood=false.
struct DietConfig
{
    bool eatFood = true;
    bool eatAgents = false;
    bool eatSameSpecies = false;
    double foodEfficiency = 1.0;
    double agentEfficiency = 0.7;
    bool corpseToFood = false;
};
} // namespace agentbiosim::simulation
