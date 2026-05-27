#pragma once

#include "simulation/EntityId.hpp"
#include "simulation/EntityTypes.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace agentbiosim::simulation
{
class AgentStore
{
public:
    [[nodiscard]] EntityId createAgent(const AgentSpawn& spawn);
    [[nodiscard]] bool removeAgent(EntityId id);

    void clear();

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool contains(EntityId id) const;
    [[nodiscard]] std::optional<std::size_t> indexOf(EntityId id) const;

    [[nodiscard]] std::optional<Vec2> getPosition(EntityId id) const;
    [[nodiscard]] bool setPosition(EntityId id, Vec2 position);

    [[nodiscard]] EntityId idAt(std::size_t index) const;
    [[nodiscard]] Vec2 positionAt(std::size_t index) const;
    [[nodiscard]] Vec2 velocityAt(std::size_t index) const;
    [[nodiscard]] double angleAt(std::size_t index) const;
    [[nodiscard]] double radiusAt(std::size_t index) const;
    [[nodiscard]] double energyAt(std::size_t index) const;
    [[nodiscard]] double ageAt(std::size_t index) const;
    [[nodiscard]] ColorRgb colorAt(std::size_t index) const;
    [[nodiscard]] SpeciesId speciesIdAt(std::size_t index) const;
    [[nodiscard]] AgentTypeCode typeCodeAt(std::size_t index) const;
    [[nodiscard]] bool aliveAt(std::size_t index) const;

    void setVelocity(EntityId id, Vec2 velocity);
    [[nodiscard]] bool setEnergy(EntityId id, double energy);
    [[nodiscard]] double addEnergy(EntityId id, double delta, double cap);
    void setEnergyAt(std::size_t index, double energy);
    [[nodiscard]] double addEnergyAt(std::size_t index, double delta, double cap);
    void addAgeAt(std::size_t index, double deltaSeconds);

private:
    void removeAtIndex(std::size_t index);

    std::vector<EntityId> ids_;
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> vx_;
    std::vector<double> vy_;
    std::vector<double> angle_;
    std::vector<double> radius_;
    std::vector<double> energy_;
    std::vector<double> age_;
    std::vector<ColorRgb> color_;
    std::vector<SpeciesId> speciesId_;
    std::vector<AgentTypeCode> typeCode_;
    std::vector<std::uint8_t> alive_;
    std::unordered_map<std::uint64_t, std::size_t> indexById_;
    std::uint64_t nextId_ = 1;
};
} // namespace agentbiosim::simulation
