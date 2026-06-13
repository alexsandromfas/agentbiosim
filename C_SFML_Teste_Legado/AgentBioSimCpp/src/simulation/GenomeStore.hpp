#pragma once

#include "neural/BrainConfig.hpp"
#include "simulation/DietConfig.hpp"
#include "simulation/EntityTypes.hpp"
#include "simulation/VisionConfig.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace agentbiosim::simulation
{
struct GenomeHandle
{
    GenomeId id = kInvalidGenomeId;

    [[nodiscard]] bool isValid() const noexcept
    {
        return id != kInvalidGenomeId;
    }
};

struct GenomeRecord
{
    GenomeId id = kInvalidGenomeId;
    GenomeId parentId = kInvalidGenomeId;
    std::uint32_t generation = 0;
    double bodySize = 9.0;
    BodyShapeCode bodyShape = BodyShapeCode::Ellipse;
    ColorRgb color{220, 220, 220};
    double mutationRate = 0.05;
    double mutationStrength = 0.08;
    double reproductionMinAge = 0.0;
    double reproductionCooldown = 0.0;
    double splitEnergy = 150.0;
    double initialEnergy = 100.0;
    double energyCap = 400.0;
    SpeciesId speciesId = 0;
    AgentTypeCode typeCode = AgentTypeCode::LegacyBacteria;
    neural::BrainConfig brainConfig;
    std::string speciesPrefix;
    // Phase 18: diet config is owned by genome (heritable via cloneFrom).
    DietConfig diet;
    // Microfase 32.5: vision targeting (what the agent perceives) owned by the
    // genome too, so each label can see food / organisms / everything. Retina
    // GEOMETRY stays global (it sizes the neural input).
    VisionConfig vision;
};

class GenomeStore
{
public:
    [[nodiscard]] GenomeHandle createGenome(GenomeRecord record);
    [[nodiscard]] GenomeHandle cloneFrom(GenomeId parentId);

    void clear();

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool contains(GenomeId id) const;

    [[nodiscard]] const GenomeRecord* find(GenomeId id) const;
    [[nodiscard]] GenomeRecord* find(GenomeId id);
    [[nodiscard]] const GenomeRecord& get(GenomeId id) const;

    [[nodiscard]] const std::vector<GenomeRecord>& records() const noexcept;

    // Phase 28: persistence.
    [[nodiscard]] GenomeId nextId() const noexcept { return nextId_; }
    void restore(std::vector<GenomeRecord> records, GenomeId nextId);

private:
    std::vector<GenomeRecord> records_;
    std::unordered_map<GenomeId, std::size_t> indexById_;
    GenomeId nextId_ = 1;
};
} // namespace agentbiosim::simulation
