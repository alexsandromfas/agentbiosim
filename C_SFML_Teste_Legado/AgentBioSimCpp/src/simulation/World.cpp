#include "simulation/World.hpp"

#include <algorithm>
#include <cmath>

namespace agentbiosim::simulation
{
namespace
{
double clampDouble(const double value, const double minValue, const double maxValue)
{
    return std::max(minValue, std::min(value, maxValue));
}

double length(const Vec2 value)
{
    return std::sqrt((value.x * value.x) + (value.y * value.y));
}

double wrapCoordinate(const double value, const double extent)
{
    if (extent <= 0.0)
    {
        return 0.0;
    }

    double wrapped = std::fmod(value, extent);
    if (wrapped < 0.0)
    {
        wrapped += extent;
    }
    return wrapped;
}
} // namespace

World::World(WorldConfig config)
{
    configure(config);
}

void World::configure(WorldConfig config)
{
    config.width = std::max(1.0, config.width);
    config.height = std::max(1.0, config.height);
    config.radius = std::max(10.0, config.radius);
    config_ = config;
}

const WorldConfig& World::config() const noexcept
{
    return config_;
}

WorldShape World::shape() const noexcept
{
    return config_.shape;
}

double World::width() const noexcept
{
    return config_.width;
}

double World::height() const noexcept
{
    return config_.height;
}

double World::radius() const noexcept
{
    return config_.radius;
}

Vec2 World::center() const noexcept
{
    return config_.center;
}

bool World::isInside(const Vec2 position, const double radius) const noexcept
{
    const double safeRadius = std::max(0.0, radius);
    if (config_.shape == WorldShape::Circular)
    {
        return length({position.x - config_.center.x, position.y - config_.center.y}) <= (config_.radius - safeRadius);
    }

    return position.x >= safeRadius && position.y >= safeRadius && position.x <= (config_.width - safeRadius) &&
           position.y <= (config_.height - safeRadius);
}

Vec2 World::clampPosition(const Vec2 position, const double radius) const noexcept
{
    const double safeRadius = std::max(0.0, radius);
    if (config_.shape == WorldShape::Circular)
    {
        const Vec2 fromCenter{position.x - config_.center.x, position.y - config_.center.y};
        const double distance = length(fromCenter);
        const double maxDistance = std::max(0.000001, config_.radius - safeRadius);
        if (distance <= maxDistance || distance <= 0.000001)
        {
            return position;
        }

        const double scale = maxDistance / distance;
        return {config_.center.x + (fromCenter.x * scale), config_.center.y + (fromCenter.y * scale)};
    }

    return {clampDouble(position.x, safeRadius, config_.width - safeRadius),
            clampDouble(position.y, safeRadius, config_.height - safeRadius)};
}

Vec2 World::wrapPosition(const Vec2 position) const noexcept
{
    if (config_.shape == WorldShape::Circular)
    {
        return clampPosition(position);
    }

    return {wrapCoordinate(position.x, config_.width), wrapCoordinate(position.y, config_.height)};
}

double World::distanceToWall(const Vec2 position) const noexcept
{
    if (config_.shape == WorldShape::Circular)
    {
        return config_.radius - length({position.x - config_.center.x, position.y - config_.center.y});
    }

    if (isInside(position))
    {
        const double left = position.x;
        const double right = config_.width - position.x;
        const double top = position.y;
        const double bottom = config_.height - position.y;
        return std::min(std::min(left, right), std::min(top, bottom));
    }

    const Vec2 clamped = clampPosition(position);
    return -length({position.x - clamped.x, position.y - clamped.y});
}

Vec2 World::minBounds() const noexcept
{
    if (config_.shape == WorldShape::Circular)
    {
        return {config_.center.x - config_.radius, config_.center.y - config_.radius};
    }

    return {0.0, 0.0};
}

Vec2 World::maxBounds() const noexcept
{
    if (config_.shape == WorldShape::Circular)
    {
        return {config_.center.x + config_.radius, config_.center.y + config_.radius};
    }

    return {config_.width, config_.height};
}
} // namespace agentbiosim::simulation
