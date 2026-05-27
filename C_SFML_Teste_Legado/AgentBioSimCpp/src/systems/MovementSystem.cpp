#include "systems/MovementSystem.hpp"

#include "config/Parameter.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <variant>

namespace agentbiosim::systems
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = kPi * 2.0;

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

std::string parameterString(const config::ParameterRegistry& parameters, const std::string& name, const std::string& fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<std::string>(&definition->defaultValue))
    {
        return *value;
    }
    return fallback;
}

std::string normalizedText(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

double sigmoid(const double value)
{
    if (value >= 0.0)
    {
        return 1.0 / (1.0 + std::exp(-value));
    }
    const double expValue = std::exp(value);
    return expValue / (1.0 + expValue);
}

double normalizeAngle(double angle)
{
    angle = std::fmod(angle + kPi, kTwoPi);
    if (angle < 0.0)
    {
        angle += kTwoPi;
    }
    return angle - kPi;
}

double dragDecay(const double drag, const double dt)
{
    const double safeDrag = std::max(0.0, drag);
    if (safeDrag <= 0.0 || dt <= 0.0)
    {
        return 1.0;
    }
    return std::exp(-safeDrag * dt);
}

void clampVelocity(simulation::Vec2& velocity, const double maxSpeed)
{
    const double safeMaxSpeed = std::max(0.0, maxSpeed);
    if (safeMaxSpeed <= 0.0)
    {
        velocity = {};
        return;
    }
    const double speed = std::hypot(velocity.x, velocity.y);
    if (speed > safeMaxSpeed && speed > 1.0e-12)
    {
        const double scale = safeMaxSpeed / speed;
        velocity.x *= scale;
        velocity.y *= scale;
    }
}

MovementControl syntheticControl(const std::size_t index, const simulation::AgentStore& agents)
{
    const double phase = static_cast<double>((index % 37U)) * 0.37 + agents.ageAt(index) * 0.5;
    MovementControl control;
    control.forward = 1.25 + 0.35 * std::sin(phase);
    control.strafe = 0.65 * std::sin(phase * 0.7 + 0.4);
    control.turn = std::sin(phase * 1.3);
    return control;
}

MovementControl controlAt(const std::size_t index,
                          const simulation::AgentStore& agents,
                          const std::vector<MovementControl>* controls)
{
    if (controls != nullptr && index < controls->size())
    {
        return (*controls)[index];
    }
    return syntheticControl(index, agents);
}

bool handleWallCollision(simulation::Vec2& position,
                         simulation::Vec2& velocity,
                         const double radius,
                         const simulation::World& world)
{
    bool collided = false;
    if (world.shape() == simulation::WorldShape::Circular)
    {
        const simulation::Vec2 center = world.center();
        const double dx = position.x - center.x;
        const double dy = position.y - center.y;
        const double distance = std::hypot(dx, dy);
        const double maxDistance = std::max(1.0e-6, world.radius() - radius);
        if (distance > maxDistance && distance > 1.0e-12)
        {
            const double nx = dx / distance;
            const double ny = dy / distance;
            position.x = center.x + nx * maxDistance;
            position.y = center.y + ny * maxDistance;
            const double radialVelocity = velocity.x * nx + velocity.y * ny;
            velocity.x -= 1.5 * radialVelocity * nx;
            velocity.y -= 1.5 * radialVelocity * ny;
            collided = true;
        }
        return collided;
    }

    if (position.x - radius < 0.0)
    {
        position.x = radius;
        velocity.x *= -0.5;
        collided = true;
    }
    else if (position.x + radius > world.width())
    {
        position.x = world.width() - radius;
        velocity.x *= -0.5;
        collided = true;
    }

    if (position.y - radius < 0.0)
    {
        position.y = radius;
        velocity.y *= -0.5;
        collided = true;
    }
    else if (position.y + radius > world.height())
    {
        position.y = world.height() - radius;
        velocity.y *= -0.5;
        collided = true;
    }

    return collided;
}

double updateSmoothRotation(const double angle,
                            double& angularVelocity,
                            const double steerCommand,
                            const double dt,
                            const MovementConfig& config)
{
    const double targetOmega = steerCommand * config.maxTurn;
    double omega = angularVelocity;
    if (config.smoothAngularInertia)
    {
        const double maxDelta = std::max(0.0, config.smoothMaxAngularAccel) * std::max(0.0, dt);
        const double delta = std::clamp(targetOmega - omega, -maxDelta, maxDelta);
        omega += delta;
    }
    else
    {
        omega = targetOmega;
    }

    if (config.smoothAngularDragEnabled)
    {
        omega *= dragDecay(config.smoothAngularDrag, dt);
    }
    omega = std::clamp(omega, -config.maxTurn, config.maxTurn);
    angularVelocity = omega;
    return normalizeAngle(angle + omega * dt);
}

void updateSmoothVelocity(simulation::Vec2& velocity,
                          const simulation::Vec2 desiredVelocity,
                          const double dt,
                          const MovementConfig& config)
{
    if (config.smoothLinearInertia)
    {
        simulation::Vec2 delta{desiredVelocity.x - velocity.x, desiredVelocity.y - velocity.y};
        const double distance = std::hypot(delta.x, delta.y);
        const double maxDelta = std::max(0.0, config.smoothMaxLinearAccel) * std::max(0.0, dt);
        if (distance > maxDelta && distance > 1.0e-12)
        {
            const double scale = maxDelta / distance;
            delta.x *= scale;
            delta.y *= scale;
        }
        velocity.x += delta.x;
        velocity.y += delta.y;
    }
    else
    {
        velocity = desiredVelocity;
    }

    if (config.smoothLinearDragEnabled)
    {
        const double decay = dragDecay(config.smoothLinearDrag, dt);
        velocity.x *= decay;
        velocity.y *= decay;
    }
}
} // namespace

MovementConfig MovementSystem::fromRegistry(const config::ParameterRegistry& parameters)
{
    MovementConfig config;
    config.mode = normalizeMovementMode(parameterString(parameters, "bacteria_movement_mode", "forward"));
    config.bodyShape = normalizeBodyShape(parameterString(parameters, "bacteria_body_shape", "ellipse"));
    config.maxSpeed = std::max(0.0, parameterDouble(parameters, "bacteria_max_speed", config.maxSpeed));
    config.maxTurn = std::max(0.0, parameterDouble(parameters, "bacteria_max_turn", config.maxTurn));
    config.allowReverse = parameterBool(parameters, "bacteria_allow_reverse_locomotion", config.allowReverse) ||
                          parameterBool(parameters, "allow_reverse_locomotion", false);
    config.inertia = std::max(0.0, parameterDouble(parameters, "agents_inertia", config.inertia));
    config.smoothLocomotion = parameterBool(parameters, "smooth_locomotion_enabled", config.smoothLocomotion);
    config.smoothLinearInertia = parameterBool(parameters, "smooth_linear_inertia_enabled", config.smoothLinearInertia);
    config.smoothMaxLinearAccel = std::max(0.0, parameterDouble(parameters, "smooth_max_linear_accel", config.smoothMaxLinearAccel));
    config.smoothLinearDragEnabled = parameterBool(parameters, "smooth_linear_drag_enabled", config.smoothLinearDragEnabled);
    config.smoothLinearDrag = std::max(0.0, parameterDouble(parameters, "smooth_linear_drag", config.smoothLinearDrag));
    config.smoothAngularInertia = parameterBool(parameters, "smooth_angular_inertia_enabled", config.smoothAngularInertia);
    config.smoothMaxAngularAccel = std::max(0.0, parameterDouble(parameters, "smooth_max_angular_accel", config.smoothMaxAngularAccel));
    config.smoothAngularDragEnabled = parameterBool(parameters, "smooth_angular_drag_enabled", config.smoothAngularDragEnabled);
    config.smoothAngularDrag = std::max(0.0, parameterDouble(parameters, "smooth_angular_drag", config.smoothAngularDrag));
    config.renderInterpolationEnabled = parameterBool(parameters, "render_interpolation_enabled", config.renderInterpolationEnabled);
    return config;
}

MovementStats MovementSystem::apply(simulation::AgentStore& agents,
                                    const simulation::World& world,
                                    const double dt,
                                    const MovementConfig& config,
                                    const std::vector<MovementControl>* controls) const
{
    MovementStats stats;
    const double safeDt = std::max(0.0, dt);

    for (std::size_t index = 0; index < agents.size(); ++index)
    {
        if (!agents.aliveAt(index))
        {
            continue;
        }

        const MovementControl control = controlAt(index, agents, controls);
        double forwardCommand = 0.0;
        double strafeCommand = 0.0;
        double steerCommand = 0.0;

        if (config.mode == MovementMode::Omni)
        {
            forwardCommand = std::tanh(control.forward);
            strafeCommand = std::tanh(control.strafe);
            steerCommand = std::tanh(control.turn);
            const double magnitude = std::hypot(forwardCommand, strafeCommand);
            if (magnitude > 1.0)
            {
                forwardCommand /= magnitude;
                strafeCommand /= magnitude;
            }
        }
        else
        {
            forwardCommand = config.allowReverse ? std::tanh(control.forward) : sigmoid(control.forward);
            steerCommand = std::tanh(control.turn);
        }

        double angle = agents.angleAt(index);
        double angularVelocity = agents.angularVelocityAt(index);
        if (config.smoothLocomotion)
        {
            angle = updateSmoothRotation(angle, angularVelocity, steerCommand, safeDt, config);
        }
        else
        {
            angularVelocity = steerCommand * config.maxTurn;
            angle = normalizeAngle(angle + angularVelocity * safeDt);
        }

        const double cosAngle = std::cos(angle);
        const double sinAngle = std::sin(angle);
        const simulation::Vec2 desiredVelocity{
            (cosAngle * forwardCommand - sinAngle * strafeCommand) * config.maxSpeed,
            (sinAngle * forwardCommand + cosAngle * strafeCommand) * config.maxSpeed,
        };

        simulation::Vec2 velocity = agents.velocityAt(index);
        if (config.smoothLocomotion)
        {
            updateSmoothVelocity(velocity, desiredVelocity, safeDt, config);
        }
        else if (config.inertia <= 1.0)
        {
            velocity = desiredVelocity;
        }
        else
        {
            const double alpha = std::min(1.0, 1.0 / std::max(1.0e-12, config.inertia));
            velocity.x += (desiredVelocity.x - velocity.x) * alpha;
            velocity.y += (desiredVelocity.y - velocity.y) * alpha;
        }
        clampVelocity(velocity, config.maxSpeed);

        const simulation::Vec2 previousPosition = agents.positionAt(index);
        simulation::Vec2 position{
            previousPosition.x + velocity.x * safeDt,
            previousPosition.y + velocity.y * safeDt,
        };

        if (handleWallCollision(position, velocity, agents.radiusAt(index), world))
        {
            ++stats.wallCollisions;
        }
        clampVelocity(velocity, config.maxSpeed);

        agents.setPositionAt(index, position);
        agents.setVelocityAt(index, velocity);
        agents.setAngleAt(index, angle);
        agents.setAngularVelocityAt(index, angularVelocity);

        ++stats.agentsProcessed;
        stats.distanceMoved += std::hypot(position.x - previousPosition.x, position.y - previousPosition.y);
        stats.maxSpeedObserved = std::max(stats.maxSpeedObserved, std::hypot(velocity.x, velocity.y));
    }

    return stats;
}

MovementMode MovementSystem::normalizeMovementMode(const std::string& value)
{
    const std::string text = normalizedText(value);
    if (text == "omni" || text == "4way" || text == "four_way" || text == "quatro_direcoes" ||
        text == "quatro direcoes")
    {
        return MovementMode::Omni;
    }
    return MovementMode::Forward;
}

BodyShape MovementSystem::normalizeBodyShape(const std::string& value)
{
    const std::string text = normalizedText(value);
    if (text == "circle" || text == "circular" || text == "circulo")
    {
        return BodyShape::Circle;
    }
    return BodyShape::Ellipse;
}

const char* MovementSystem::movementModeName(const MovementMode mode)
{
    return mode == MovementMode::Omni ? "omni" : "forward";
}

const char* MovementSystem::bodyShapeName(const BodyShape shape)
{
    return shape == BodyShape::Circle ? "circle" : "ellipse";
}
} // namespace agentbiosim::systems
