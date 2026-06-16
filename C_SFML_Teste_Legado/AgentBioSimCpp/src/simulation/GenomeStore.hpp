#pragma once

#include "neural/BrainConfig.hpp"
#include "simulation/DietConfig.hpp"
#include "simulation/EntityTypes.hpp"
#include "simulation/VisionConfig.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    // Fase 34.3: reproduction strategy (per individual). Energy = legacy (reach
    // split_energy, splits energy with the child). Age = by age+cooldown only, no
    // energy gate/cost (child gets a fresh initialEnergy). offspringCount = how many
    // children per reproduction event (default 1 = legacy "split in two").
    ReproductionMode reproductionMode = ReproductionMode::Energy;
    int offspringCount = 1;
    double splitEnergy = 150.0;
    double initialEnergy = 100.0;
    double energyCap = 400.0;
    // Fase 34.2: locomotion + metabolic-cost + death-energy traits, now PER
    // INDIVIDUAL (they were global bacteria_* params read by the systems). Defaults
    // equal the old global defaults so the per-genome reads stay byte-identical.
    double maxSpeed = 300.0;
    double maxTurn = 3.14159265358979323846;
    bool allowReverse = false;
    double moveCostV0 = 0.5;   // metab cost/s at zero speed (<- *_metab_v0_cost)
    double moveCostVmax = 8.0; // metab cost/s at max speed (<- *_metab_vmax_cost)
    double deathEnergy = 50.0; // starvation threshold (<- *_death_energy)
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

    // Garbage-collect genomes (Microfase 32.6): keep only the records whose id is in
    // `keep` (the live agents' genomes + each label/species template), dropping the
    // rest. Offspring clone a fresh genome per birth (ReproductionSystem) and nothing
    // frees it when the organism dies, so without this the store grows unbounded
    // (a real RAM + save-size leak). Removal is by value/id only: `nextId_` is never
    // rewound (ids are never reused) and all access is by id, so pruning unreferenced
    // genomes cannot change any living organism's genome or the simulation result.
    // Returns how many records were removed.
    std::size_t retain(const std::unordered_set<GenomeId>& keep);

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
