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
    [[nodiscard]] double angularVelocityAt(std::size_t index) const;
    [[nodiscard]] double radiusAt(std::size_t index) const;
    [[nodiscard]] double energyAt(std::size_t index) const;
    [[nodiscard]] double ageAt(std::size_t index) const;
    [[nodiscard]] double reproductionCooldownAt(std::size_t index) const;
    [[nodiscard]] ColorRgb colorAt(std::size_t index) const;
    [[nodiscard]] SpeciesId speciesIdAt(std::size_t index) const;
    [[nodiscard]] GenomeId genomeIdAt(std::size_t index) const;
    [[nodiscard]] AgentTypeCode typeCodeAt(std::size_t index) const;
    [[nodiscard]] BodyShapeCode bodyShapeAt(std::size_t index) const;
    [[nodiscard]] bool aliveAt(std::size_t index) const;

    void setVelocity(EntityId id, Vec2 velocity);
    void setPositionAt(std::size_t index, Vec2 position);
    void setVelocityAt(std::size_t index, Vec2 velocity);
    void setAngleAt(std::size_t index, double angle);
    void setAngularVelocityAt(std::size_t index, double angularVelocity);
    [[nodiscard]] bool setEnergy(EntityId id, double energy);
    [[nodiscard]] double addEnergy(EntityId id, double delta, double cap);
    void setEnergyAt(std::size_t index, double energy);
    [[nodiscard]] double addEnergyAt(std::size_t index, double delta, double cap);
    void addAgeAt(std::size_t index, double deltaSeconds);
    void setReproductionCooldownAt(std::size_t index, double cooldown);
    void addReproductionCooldownAt(std::size_t index, double deltaSeconds);
    void setGenomeIdAt(std::size_t index, GenomeId genomeId);
    // Phase 24.2: species reassignment + recolor for the Labels tab.
    void setSpeciesIdAt(std::size_t index, SpeciesId speciesId);
    void setColorAt(std::size_t index, ColorRgb color);

    // Phase 28: persistence. `nextId()` is the next id to allocate; `restore`
    // replaces all live agents from a save (ids preserved exactly), so agent ->
    // genome/species references and the per-id brain map stay valid.
    [[nodiscard]] std::uint64_t nextId() const noexcept { return nextId_; }
    void restore(const std::vector<EntityId>& ids, const std::vector<AgentSpawn>& spawns,
                 std::uint64_t nextId);

private:
    void removeAtIndex(std::size_t index);

    std::vector<EntityId> ids_;
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> vx_;
    std::vector<double> vy_;
    std::vector<double> angle_;
    std::vector<double> angularVelocity_;
    std::vector<double> radius_;
    std::vector<double> energy_;
    std::vector<double> age_;
    std::vector<double> reproductionCooldown_;
    std::vector<ColorRgb> color_;
    std::vector<SpeciesId> speciesId_;
    std::vector<GenomeId> genomeId_;
    std::vector<AgentTypeCode> typeCode_;
    std::vector<BodyShapeCode> bodyShape_;
    std::vector<std::uint8_t> alive_;
    std::unordered_map<std::uint64_t, std::size_t> indexById_;
    std::uint64_t nextId_ = 1;
};
} // namespace agentbiosim::simulation
