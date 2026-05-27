#include "simulation/AgentStore.hpp"

#include <algorithm>
#include <utility>

namespace agentbiosim::simulation
{
EntityId AgentStore::createAgent(const AgentSpawn& spawn)
{
    const EntityId id{nextId_++};
    const std::size_t index = ids_.size();

    ids_.push_back(id);
    x_.push_back(spawn.position.x);
    y_.push_back(spawn.position.y);
    vx_.push_back(spawn.velocity.x);
    vy_.push_back(spawn.velocity.y);
    angle_.push_back(spawn.angle);
    angularVelocity_.push_back(spawn.angularVelocity);
    radius_.push_back(spawn.radius);
    energy_.push_back(spawn.energy);
    age_.push_back(spawn.age);
    color_.push_back(spawn.color);
    speciesId_.push_back(spawn.speciesId);
    typeCode_.push_back(spawn.typeCode);
    bodyShape_.push_back(spawn.bodyShape);
    alive_.push_back(1U);
    indexById_.emplace(id.value, index);

    return id;
}

bool AgentStore::removeAgent(const EntityId id)
{
    const std::optional<std::size_t> index = indexOf(id);
    if (!index.has_value())
    {
        return false;
    }
    removeAtIndex(*index);
    return true;
}

void AgentStore::clear()
{
    ids_.clear();
    x_.clear();
    y_.clear();
    vx_.clear();
    vy_.clear();
    angle_.clear();
    angularVelocity_.clear();
    radius_.clear();
    energy_.clear();
    age_.clear();
    color_.clear();
    speciesId_.clear();
    typeCode_.clear();
    bodyShape_.clear();
    alive_.clear();
    indexById_.clear();
    nextId_ = 1;
}

std::size_t AgentStore::size() const noexcept
{
    return ids_.size();
}

bool AgentStore::empty() const noexcept
{
    return ids_.empty();
}

bool AgentStore::contains(const EntityId id) const
{
    return indexOf(id).has_value();
}

std::optional<std::size_t> AgentStore::indexOf(const EntityId id) const
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

std::optional<Vec2> AgentStore::getPosition(const EntityId id) const
{
    const std::optional<std::size_t> index = indexOf(id);
    if (!index.has_value())
    {
        return std::nullopt;
    }
    return positionAt(*index);
}

bool AgentStore::setPosition(const EntityId id, const Vec2 position)
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

EntityId AgentStore::idAt(const std::size_t index) const
{
    return ids_.at(index);
}

Vec2 AgentStore::positionAt(const std::size_t index) const
{
    return {x_.at(index), y_.at(index)};
}

Vec2 AgentStore::velocityAt(const std::size_t index) const
{
    return {vx_.at(index), vy_.at(index)};
}

double AgentStore::angleAt(const std::size_t index) const
{
    return angle_.at(index);
}

double AgentStore::angularVelocityAt(const std::size_t index) const
{
    return angularVelocity_.at(index);
}

double AgentStore::radiusAt(const std::size_t index) const
{
    return radius_.at(index);
}

double AgentStore::energyAt(const std::size_t index) const
{
    return energy_.at(index);
}

double AgentStore::ageAt(const std::size_t index) const
{
    return age_.at(index);
}

ColorRgb AgentStore::colorAt(const std::size_t index) const
{
    return color_.at(index);
}

SpeciesId AgentStore::speciesIdAt(const std::size_t index) const
{
    return speciesId_.at(index);
}

AgentTypeCode AgentStore::typeCodeAt(const std::size_t index) const
{
    return typeCode_.at(index);
}

BodyShapeCode AgentStore::bodyShapeAt(const std::size_t index) const
{
    return bodyShape_.at(index);
}

bool AgentStore::aliveAt(const std::size_t index) const
{
    return alive_.at(index) != 0U;
}

void AgentStore::setVelocity(const EntityId id, const Vec2 velocity)
{
    const std::optional<std::size_t> index = indexOf(id);
    if (!index.has_value())
    {
        return;
    }
    vx_[*index] = velocity.x;
    vy_[*index] = velocity.y;
}

void AgentStore::setPositionAt(const std::size_t index, const Vec2 position)
{
    x_.at(index) = position.x;
    y_.at(index) = position.y;
}

void AgentStore::setVelocityAt(const std::size_t index, const Vec2 velocity)
{
    vx_.at(index) = velocity.x;
    vy_.at(index) = velocity.y;
}

void AgentStore::setAngleAt(const std::size_t index, const double angle)
{
    angle_.at(index) = angle;
}

void AgentStore::setAngularVelocityAt(const std::size_t index, const double angularVelocity)
{
    angularVelocity_.at(index) = angularVelocity;
}

bool AgentStore::setEnergy(const EntityId id, const double energy)
{
    const std::optional<std::size_t> index = indexOf(id);
    if (!index.has_value())
    {
        return false;
    }
    setEnergyAt(*index, energy);
    return true;
}

double AgentStore::addEnergy(const EntityId id, const double delta, const double cap)
{
    const std::optional<std::size_t> index = indexOf(id);
    if (!index.has_value())
    {
        return 0.0;
    }
    return addEnergyAt(*index, delta, cap);
}

void AgentStore::setEnergyAt(const std::size_t index, const double energy)
{
    energy_.at(index) = std::max(0.0, energy);
}

double AgentStore::addEnergyAt(const std::size_t index, const double delta, const double cap)
{
    const double before = energy_.at(index);
    double next = std::max(0.0, before + delta);
    if (cap >= 0.0)
    {
        next = std::min(next, cap);
    }
    energy_.at(index) = next;
    return std::max(0.0, next - before);
}

void AgentStore::addAgeAt(const std::size_t index, const double deltaSeconds)
{
    age_.at(index) = std::max(0.0, age_.at(index) + deltaSeconds);
}

void AgentStore::removeAtIndex(const std::size_t index)
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
        angle_[index] = angle_[last];
        angularVelocity_[index] = angularVelocity_[last];
        radius_[index] = radius_[last];
        energy_[index] = energy_[last];
        age_[index] = age_[last];
        color_[index] = color_[last];
        speciesId_[index] = speciesId_[last];
        typeCode_[index] = typeCode_[last];
        bodyShape_[index] = bodyShape_[last];
        alive_[index] = alive_[last];
        indexById_[ids_[index].value] = index;
    }

    ids_.pop_back();
    x_.pop_back();
    y_.pop_back();
    vx_.pop_back();
    vy_.pop_back();
    angle_.pop_back();
    angularVelocity_.pop_back();
    radius_.pop_back();
    energy_.pop_back();
    age_.pop_back();
    color_.pop_back();
    speciesId_.pop_back();
    typeCode_.pop_back();
    bodyShape_.pop_back();
    alive_.pop_back();
    indexById_.erase(removedId.value);
}
} // namespace agentbiosim::simulation
