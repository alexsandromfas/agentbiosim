#include "simulation/ObstacleStore.hpp"

#include <algorithm>
#include <cmath>

namespace agentbiosim::simulation
{
ObstacleId ObstacleStore::add(const ObstacleSpawn& spawn)
{
    const ObstacleId id = nextId_++;
    ids_.push_back(id);
    x_.push_back(spawn.position.x);
    y_.push_back(spawn.position.y);
    radius_.push_back(std::max(0.1, spawn.radius));
    brushRadius_.push_back(std::max(0.1, spawn.brushRadius));
    color_.push_back(spawn.color);
    alive_.push_back(1U);
    return id;
}

bool ObstacleStore::remove(const ObstacleId id)
{
    for (std::size_t i = 0; i < ids_.size(); ++i)
    {
        if (ids_[i] != id) continue;
        const std::size_t last = ids_.size() - 1U;
        if (i != last)
        {
            ids_[i] = ids_[last];
            x_[i] = x_[last];
            y_[i] = y_[last];
            radius_[i] = radius_[last];
            brushRadius_[i] = brushRadius_[last];
            color_[i] = color_[last];
            alive_[i] = alive_[last];
        }
        ids_.pop_back();
        x_.pop_back();
        y_.pop_back();
        radius_.pop_back();
        brushRadius_.pop_back();
        color_.pop_back();
        alive_.pop_back();
        return true;
    }
    return false;
}

void ObstacleStore::clear()
{
    ids_.clear();
    x_.clear();
    y_.clear();
    radius_.clear();
    brushRadius_.clear();
    color_.clear();
    alive_.clear();
    nextId_ = 1;
}

ObstacleId ObstacleStore::paint(const Vec2 pos, const double brushRadius, const ColorRgb color)
{
    ObstacleSpawn s;
    s.position = pos;
    s.radius = brushRadius;
    s.brushRadius = brushRadius;
    s.color = color;
    return add(s);
}

std::size_t ObstacleStore::eraseAt(const Vec2 pos, const double eraseRadius)
{
    const double r2 = eraseRadius * eraseRadius;
    std::size_t removed = 0;
    // Iterate from the back so swap-removal does not invalidate earlier indices.
    for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(ids_.size()) - 1; i >= 0; --i)
    {
        const std::size_t idx = static_cast<std::size_t>(i);
        const double dx = x_[idx] - pos.x;
        const double dy = y_[idx] - pos.y;
        if (dx * dx + dy * dy <= r2)
        {
            if (remove(ids_[idx])) ++removed;
        }
    }
    return removed;
}

std::size_t ObstacleStore::size() const noexcept
{
    return ids_.size();
}

bool ObstacleStore::empty() const noexcept
{
    return ids_.empty();
}

bool ObstacleStore::contains(const ObstacleId id) const
{
    for (const auto v : ids_)
    {
        if (v == id) return true;
    }
    return false;
}

Vec2 ObstacleStore::positionAt(const std::size_t index) const
{
    return {x_.at(index), y_.at(index)};
}

double ObstacleStore::radiusAt(const std::size_t index) const
{
    return radius_.at(index);
}

double ObstacleStore::brushRadiusAt(const std::size_t index) const
{
    return brushRadius_.at(index);
}

ColorRgb ObstacleStore::colorAt(const std::size_t index) const
{
    return color_.at(index);
}

ObstacleId ObstacleStore::idAt(const std::size_t index) const
{
    return ids_.at(index);
}

bool ObstacleStore::containsPoint(const Vec2 pos) const noexcept
{
    for (std::size_t i = 0; i < ids_.size(); ++i)
    {
        const double dx = pos.x - x_[i];
        const double dy = pos.y - y_[i];
        const double r = radius_[i];
        if (dx * dx + dy * dy <= r * r) return true;
    }
    return false;
}

bool ObstacleStore::overlapsCircle(const Vec2 center, const double radius) const noexcept
{
    for (std::size_t i = 0; i < ids_.size(); ++i)
    {
        const double dx = center.x - x_[i];
        const double dy = center.y - y_[i];
        const double r = radius_[i] + radius;
        if (dx * dx + dy * dy <= r * r) return true;
    }
    return false;
}

bool ObstacleStore::segmentBlocked(const Vec2 a, const Vec2 b) const noexcept
{
    Vec2 hit{};
    return segmentBlocked(a, b, hit);
}

bool ObstacleStore::segmentBlocked(const Vec2 a, const Vec2 b, Vec2& outHit) const noexcept
{
    if (ids_.empty()) return false;
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double segLen2 = dx * dx + dy * dy;
    if (segLen2 <= 1.0e-18) return false;
    const double invSegLen2 = 1.0 / segLen2;

    double bestT = 1.0;
    bool hitAny = false;
    Vec2 bestHit{a.x, a.y};
    for (std::size_t i = 0; i < ids_.size(); ++i)
    {
        // Closest point on segment to obstacle center, parameterized as t in [0,1].
        const double cx = x_[i] - a.x;
        const double cy = y_[i] - a.y;
        const double t = std::clamp((cx * dx + cy * dy) * invSegLen2, 0.0, 1.0);
        const double px = a.x + dx * t;
        const double py = a.y + dy * t;
        const double ex = px - x_[i];
        const double ey = py - y_[i];
        const double r = radius_[i];
        if (ex * ex + ey * ey <= r * r)
        {
            if (t < bestT)
            {
                bestT = t;
                bestHit = {px, py};
                hitAny = true;
            }
        }
    }
    if (hitAny) outHit = bestHit;
    return hitAny;
}

void ObstacleStore::queryRadius(const Vec2 center, const double radius,
                                  std::vector<std::size_t>& outIndices) const
{
    outIndices.clear();
    for (std::size_t i = 0; i < ids_.size(); ++i)
    {
        const double dx = center.x - x_[i];
        const double dy = center.y - y_[i];
        const double r = radius + radius_[i];
        if (dx * dx + dy * dy <= r * r) outIndices.push_back(i);
    }
}

bool isPositionFree(const World& world, const ObstacleStore* obstacles,
                    const Vec2 pos, const double radius) noexcept
{
    if (world.shape() == WorldShape::Circular)
    {
        const Vec2 c = world.center();
        const double rr = world.radius() - radius;
        if (rr <= 0.0) return false;
        const double dx = pos.x - c.x;
        const double dy = pos.y - c.y;
        if (dx * dx + dy * dy > rr * rr) return false;
    }
    else
    {
        if (pos.x < radius || pos.x > world.width() - radius) return false;
        if (pos.y < radius || pos.y > world.height() - radius) return false;
    }
    if (obstacles == nullptr || obstacles->empty()) return true;
    return !obstacles->overlapsCircle(pos, radius);
}
} // namespace agentbiosim::simulation
