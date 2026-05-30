#pragma once

#include "config/ParameterRegistry.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
enum class MovementMode
{
    Forward,
    Omni
};

enum class BodyShape
{
    Ellipse,
    Circle
};

struct MovementControl
{
    double forward = 0.0;
    double strafe = 0.0;
    double turn = 0.0;
};

struct MovementConfig
{
    MovementMode mode = MovementMode::Forward;
    BodyShape bodyShape = BodyShape::Ellipse;
    double maxSpeed = 300.0;
    double maxTurn = 3.14159265358979323846;
    bool allowReverse = false;
    double inertia = 1.0;
    bool smoothLocomotion = false;
    bool smoothLinearInertia = true;
    double smoothMaxLinearAccel = 900.0;
    bool smoothLinearDragEnabled = true;
    double smoothLinearDrag = 0.75;
    bool smoothAngularInertia = true;
    double smoothMaxAngularAccel = 12.566370614359172;
    bool smoothAngularDragEnabled = true;
    double smoothAngularDrag = 1.5;
    bool renderInterpolationEnabled = false;
};

struct MovementStats
{
    std::size_t agentsProcessed = 0;
    std::size_t wallCollisions = 0;
    std::size_t obstacleBlocks = 0;
    double distanceMoved = 0.0;
    double maxSpeedObserved = 0.0;
};

class MovementSystem
{
public:
    [[nodiscard]] static MovementConfig fromRegistry(const config::ParameterRegistry& parameters);
    [[nodiscard]] MovementStats apply(simulation::AgentStore& agents,
                                      const simulation::World& world,
                                      double dt,
                                      const MovementConfig& config,
                                      const std::vector<MovementControl>* controls = nullptr,
                                      const simulation::ObstacleStore* obstacles = nullptr) const;

    [[nodiscard]] static MovementMode normalizeMovementMode(const std::string& value);
    [[nodiscard]] static BodyShape normalizeBodyShape(const std::string& value);
    [[nodiscard]] static const char* movementModeName(MovementMode mode);
    [[nodiscard]] static const char* bodyShapeName(BodyShape shape);
};
} // namespace agentbiosim::systems
