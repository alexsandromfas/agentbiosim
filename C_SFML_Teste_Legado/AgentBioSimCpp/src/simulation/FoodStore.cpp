#include "simulation/FoodStore.hpp"

#include <algorithm>

namespace agentbiosim::simulation
{
EntityId FoodStore::createFood(const FoodSpawn& spawn)
{
    const EntityId id{nextId_++};
    const std::size_t index = ids_.size();

    ids_.push_back(id);
    x_.push_back(spawn.position.x);
    y_.push_back(spawn.position.y);
    vx_.push_back(0.0);
    vy_.push_back(0.0);
    radius_.push_back(spawn.radius);
    energy_.push_back(spawn.energy);
    initialEnergy_.push_back(spawn.initialEnergy);
    color_.push_back(spawn.color);
    kind_.push_back(spawn.kind);
    clusterId_.push_back(spawn.clusterId);
    alive_.push_back(1U);
    indexById_.emplace(id.value, index);

    return id;
}

void FoodStore::restore(const std::vector<EntityId>& ids, const std::vector<FoodSpawn>& spawns,
                        const std::vector<Vec2>& velocities, const std::uint64_t nextId,
                        const std::uint32_t nextClusterId)
{
    clear();
    const std::size_t count = std::min(ids.size(), spawns.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        const EntityId id = ids[i];
        const FoodSpawn& s = spawns[i];
        const std::size_t index = ids_.size();
        ids_.push_back(id);
        x_.push_back(s.position.x);
        y_.push_back(s.position.y);
        vx_.push_back(i < velocities.size() ? velocities[i].x : 0.0);
        vy_.push_back(i < velocities.size() ? velocities[i].y : 0.0);
        radius_.push_back(s.radius);
        energy_.push_back(s.energy);
        initialEnergy_.push_back(s.initialEnergy);
        color_.push_back(s.color);
        kind_.push_back(s.kind);
        clusterId_.push_back(s.clusterId);
        alive_.push_back(1U);
        indexById_.emplace(id.value, index);
    }
    nextId_ = nextId;
    nextClusterId_ = nextClusterId;
}

bool FoodStore::removeFood(const EntityId id)
{
    const std::optional<std::size_t> index = indexOf(id);
    if (!index.has_value())
    {
        return false;
    }
    removeAtIndex(*index);
    return true;
}

void FoodStore::clear()
{
    ids_.clear();
    x_.clear();
    y_.clear();
    vx_.clear();
    vy_.clear();
    radius_.clear();
    energy_.clear();
    initialEnergy_.clear();
    color_.clear();
    kind_.clear();
    clusterId_.clear();
    alive_.clear();
    indexById_.clear();
    nextId_ = 1;
    nextClusterId_ = 1;
}

std::size_t FoodStore::size() const noexcept
{
    return ids_.size();
}

bool FoodStore::empty() const noexcept
{
    return ids_.empty();
}

bool FoodStore::contains(const EntityId id) const
{
    return indexOf(id).has_value();
}

std::optional<std::size_t> FoodStore::indexOf(const EntityId id) const
{
    if (!id.isValid())
    {
        return std::nullopt;
    }
    const auto it = indexById_.find(id.value);
    if (it == indexById_.end())
    {
        return std::nullopt;
    }
    return it->second;
}

std::optional<Vec2> FoodStore::getPosition(const EntityId id) const
{
    const std::optional<std::size_t> index = indexOf(id);
    if (!index.has_value())
    {
        return std::nullopt;
    }
    return positionAt(*index);
}

bool FoodStore::setPosition(const EntityId id, const Vec2 position)
{
    const std::optional<std::size_t> index = indexOf(id);
    if (!index.has_value())
    {
        return false;
    }
    x_[*index] = position.x;
    y_[*index] = position.y;
    return true;
}

EntityId FoodStore::idAt(const std::size_t index) const
{
    return ids_.at(index);
}

Vec2 FoodStore::positionAt(const std::size_t index) const
{
    return {x_.at(index), y_.at(index)};
}

Vec2 FoodStore::velocityAt(const std::size_t index) const
{
    return {vx_.at(index), vy_.at(index)};
}

double FoodStore::radiusAt(const std::size_t index) const
{
    return radius_.at(index);
}

double FoodStore::energyAt(const std::size_t index) const
{
    return energy_.at(index);
}

double FoodStore::initialEnergyAt(const std::size_t index) const
{
    return initialEnergy_.at(index);
}

ColorRgb FoodStore::colorAt(const std::size_t index) const
{
    return color_.at(index);
}

FoodKind FoodStore::kindAt(const std::size_t index) const
{
    return kind_.at(index);
}

std::uint32_t FoodStore::clusterIdAt(const std::size_t index) const
{
    return clusterId_.at(index);
}

bool FoodStore::aliveAt(const std::size_t index) const
{
    return alive_.at(index) != 0U;
}

void FoodStore::setEnergyAt(const std::size_t index, const double energy)
{
    energy_.at(index) = energy;
}

void FoodStore::setRadiusAt(const std::size_t index, const double radius)
{
    radius_.at(index) = radius;
}

void FoodStore::setPositionAt(const std::size_t index, const Vec2 position)
{
    x_.at(index) = position.x;
    y_.at(index) = position.y;
}

void FoodStore::setVelocityAt(const std::size_t index, const Vec2 velocity)
{
    vx_.at(index) = velocity.x;
    vy_.at(index) = velocity.y;
}

std::uint32_t FoodStore::allocateClusterId()
{
    return nextClusterId_++;
}

void FoodStore::removeAtIndex(const std::size_t index)
{
    const std::size_t last = ids_.size() - 1U;
    const EntityId removedId = ids_[index];

    if (index != last)
    {
        ids_[index] = ids_[last];
        x_[index] = x_[last];
        y_[index] = y_[last];
        vx_[index] = vx_[last];
        vy_[index] = vy_[last];
        radius_[index] = radius_[last];
        energy_[index] = energy_[last];
        initialEnergy_[index] = initialEnergy_[last];
        color_[index] = color_[last];
        kind_[index] = kind_[last];
        clusterId_[index] = clusterId_[last];
        alive_[index] = alive_[last];
        indexById_[ids_[index].value] = index;
    }

    ids_.pop_back();
    x_.pop_back();
    y_.pop_back();
    vx_.pop_back();
    vy_.pop_back();
    radius_.pop_back();
    energy_.pop_back();
    initialEnergy_.pop_back();
    color_.pop_back();
    kind_.pop_back();
    clusterId_.pop_back();
    alive_.pop_back();
    indexById_.erase(removedId.value);
}
} // namespace agentbiosim::simulation
