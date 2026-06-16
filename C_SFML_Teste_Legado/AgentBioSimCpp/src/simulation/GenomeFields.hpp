#pragma once

#include "config/Parameter.hpp"
#include "simulation/GenomeStore.hpp"

#include <optional>
#include <string>

namespace agentbiosim::simulation
{
// Phase 34.1: the single bridge between a PER-INDIVIDUAL genome field (keyed by
// its registry SUFFIX, e.g. "body_size", "diet_food", "retina_see_food") and the
// GenomeRecord member that stores it.
//
// The granular live-apply command (core::CmdSetSpeciesGenomeField), the
// per-species genome editor UI, and a future per-individual mutation all go
// through here, so the field<->member mapping lives in exactly one place.
//
// SCOPE: only traits that ALREADY live on the GenomeRecord are fields here —
// body (size/shape), energy (initial/split/cap), reproduction (min age/cooldown),
// diet (eat flags + efficiencies + corpse->food), vision FLAGS ("what it sees")
// and mutation (rate/strength). Color is handled per-species by its own command
// (it also recolors the live agents) and is intentionally NOT a field here.
//
// Traits that are GLOBAL in 34.1 (speed/turn/locomotion, death energy/age,
// locomotion costs, vision GEOMETRY: radius/retina count/FOV/eyes/channels/mode,
// and the neural architecture) are NOT genome fields: setGenomeField returns
// false and genomeFieldValue returns nullopt for them. They migrate into the
// genome in Phase 34.2, reusing exactly this bridge.

// True when `field` names a per-individual genome field (editable live in 34.1).
[[nodiscard]] bool isGenomeField(const std::string& field) noexcept;

// Writes a single field into `genome` (clamping like the bootstrap path does).
// Returns true when `field` was recognized; false (and no change) otherwise, so
// a stray global field routed here is a safe no-op.
bool setGenomeField(GenomeRecord& genome, const std::string& field,
                    const config::ParameterValue& value);

// Reads the current value of a genome field as a ParameterValue, so the UI can
// seed its widgets from the species' genome. Returns nullopt for unknown fields.
[[nodiscard]] std::optional<config::ParameterValue> genomeFieldValue(
    const GenomeRecord& genome, const std::string& field);
} // namespace agentbiosim::simulation
