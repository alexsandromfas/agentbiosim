#pragma once

namespace agentbiosim::simulation
{
// Microfase 32.5: per-label vision targeting owned by a GenomeRecord (so it
// inherits via `cloneFrom`, exactly like DietConfig). These flags decide WHAT an
// agent perceives; they do NOT change the retina geometry (retina count, FOV,
// channels, input mode) which stays global because it defines the neural input
// size shared by a brain architecture.
//
// Names (per species/label prefix in the registry):
//  - seeFood          <- {prefix}_retina_see_food
//  - seeAgents        <- {prefix}_retina_see_bacteria (alias _retina_see_agents)
//  - seePredators     <- {prefix}_retina_see_predators
//  - seeObstacles     <- {prefix}_retina_see_obstacles
//  - seeAll           <- {prefix}_retina_see_all   (bypasses type filters: the
//                        agent perceives every nearby object and natural selection
//                        / RGB learning sort out what each one is)
//  - seeThroughWalls  <- {prefix}_retina_see_through_walls
//
// Bacteria default: seeFood=true, others false, seeThroughWalls=true.
// Predator default: seeFood + seeAgents (it must perceive its prey).
struct VisionConfig
{
    bool seeFood = true;
    bool seeAgents = false;
    bool seePredators = false;
    bool seeObstacles = false;
    bool seeAll = false;
    bool seeThroughWalls = true;
};
} // namespace agentbiosim::simulation
