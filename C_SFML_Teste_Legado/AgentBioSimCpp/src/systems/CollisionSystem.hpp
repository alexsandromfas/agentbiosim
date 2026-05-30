#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/SpatialHash.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <cstdint>
#include <random>

namespace agentbiosim::systems
{
// Phase 21: collision and optional physics. The entire system is off by default
// (per parameter defaults) so when nothing is enabled the call costs effectively
// zero (an early return for each disabled subsystem). When enabled, the system
// uses SpatialHash to find candidate pairs.
struct CollisionConfig
{
    // Agent-agent.
    bool agentCollisionEnabled = false;
    bool elasticityEnabled = false;
    double restitution = 0.12;        // clamped [0, 1]
    double velocityTransfer = 0.0;    // clamped [0, 1]
    double separation = 0.9;          // [0, 1] fraction of overlap resolved per step
    double maxImpulse = 900.0;        // clamps position / velocity correction per step

    // Global fluid effects.
    bool viscosityEnabled = false;
    double viscosityDrag = 0.0;       // per-second drag coefficient
    bool brownianEnabled = false;
    double brownianStrength = 0.0;    // velocity units per sqrt-second

    // Chunk food physics.
    bool movableChunkFoodEnabled = false;
    bool chunkChunkCollisionEnabled = false;
    bool chunkAdhesionEnabled = false;
    double chunkAdhesionStrength = 0.0;
    double chunkMassScale = 1.0;
    double chunkDrag = 0.0;
    double chunkPushStrength = 0.0;

    // Runtime.
    bool useSpatial = true;
    std::uint64_t seed = 20260530ULL;
    double dt = 1.0 / 30.0;
};

struct CollisionStats
{
    std::size_t agentPairsTested = 0;
    std::size_t agentCollisionsResolved = 0;
    std::size_t agentFoodPairsTested = 0;
    std::size_t foodPushes = 0;
    std::size_t foodPairsTested = 0;
    std::size_t foodCollisionsResolved = 0;
    std::size_t adhesionsApplied = 0;
    std::size_t brownianApplied = 0;
};

class CollisionSystem
{
public:
    [[nodiscard]] static CollisionConfig fromRegistry(const config::ParameterRegistry& parameters);

    // Phase 21: apply collision pipeline. Order of subphases inside apply:
    //  1. global viscosity drag on agents and movable chunks
    //  2. brownian impulse on agents (and movable chunks)
    //  3. chunk velocity integration (chunks advance by velocity * dt)
    //  4. agent-agent collision (separation + optional elasticity + transfer)
    //  5. agent-chunk collision (consumption ignored here; just push)
    //  6. chunk-chunk collision (separation)
    //  7. chunk-cluster adhesion
    //  8. obstacle post-clamp (agents rolled back into world / out of discs)
    [[nodiscard]] CollisionStats apply(simulation::AgentStore& agents,
                                        simulation::FoodStore& foods,
                                        const simulation::World& world,
                                        simulation::SpatialHash* spatial,
                                        const simulation::ObstacleStore* obstacles,
                                        const CollisionConfig& config);

private:
    std::mt19937_64 rng_{20260530ULL};
    bool rngInitialized_ = false;
};
} // namespace agentbiosim::systems
