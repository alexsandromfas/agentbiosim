#pragma once

#include "simulation/EntityTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace agentbiosim::simulation
{
inline constexpr SpeciesId kInvalidSpeciesId = 0;

// Phase 17: SpeciesRecord is the canonical metadata holder for a species.
// It does not own brains or perception - it only describes spawn metadata,
// limits, color, alias mapping and which GenomeId is the default for new
// agents of this species. The default genome lives in the GenomeStore.
struct SpeciesRecord
{
    SpeciesId id = kInvalidSpeciesId;
    std::string name;              // canonical name (e.g., "bacteria", "predator")
    std::string label;              // display label (e.g., "Bacteria", "Predator")
    std::string parameterPrefix;    // registry prefix used to read params
    ColorRgb color{220, 220, 220};
    int initialCount = 0;
    int minPopulation = 0;
    int maxPopulation = 0;
    bool showGraph = true;
    bool enabled = true;
    bool populationMinRescueEnabled = true;
    GenomeId defaultGenomeId = kInvalidGenomeId;
    std::vector<std::string> legacyAliases;
    AgentTypeCode typeCode = AgentTypeCode::Organism;
    BodyShapeCode bodyShape = BodyShapeCode::Ellipse;
};

// SpeciesStore is the headless registry of species. It has no dependency on
// SFML, UI, neural or perception. Aliases are case-insensitive and trimmed.
class SpeciesStore
{
public:
    [[nodiscard]] SpeciesId registerSpecies(SpeciesRecord record);

    void clear();

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] const SpeciesRecord* find(SpeciesId id) const;
    [[nodiscard]] SpeciesRecord* find(SpeciesId id);
    [[nodiscard]] const SpeciesRecord& get(SpeciesId id) const;
    [[nodiscard]] bool contains(SpeciesId id) const;

    // Alias and name lookup. Returns kInvalidSpeciesId when unresolved.
    [[nodiscard]] SpeciesId resolveAlias(const std::string& alias) const;
    [[nodiscard]] const SpeciesRecord* findByName(const std::string& name) const;

    // Mutation helpers used during bootstrap and admin.
    bool setDefaultGenome(SpeciesId id, GenomeId genome);
    bool setColor(SpeciesId id, ColorRgb color);
    bool setEnabled(SpeciesId id, bool enabled);
    bool addAlias(SpeciesId id, const std::string& alias);

    [[nodiscard]] const std::vector<SpeciesRecord>& records() const noexcept;

    // Find species by canonical name and return id (or kInvalidSpeciesId).
    [[nodiscard]] SpeciesId idByName(const std::string& name) const;

private:
    [[nodiscard]] static std::string normalize(std::string value);

    std::vector<SpeciesRecord> records_;
    std::unordered_map<SpeciesId, std::size_t> indexById_;
    std::unordered_map<std::string, SpeciesId> aliasIndex_;
    SpeciesId nextId_ = 1;
};
} // namespace agentbiosim::simulation
