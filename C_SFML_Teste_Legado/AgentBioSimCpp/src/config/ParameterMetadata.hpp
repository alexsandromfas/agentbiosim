#pragma once

#include "config/ParameterRegistry.hpp"

#include <string>

namespace agentbiosim::config
{
// Phase 23: classify parameters by what must happen when they change. The
// default for every parameter is `ApplyFlag::Immediate`. This helper applies
// distinct flags (RequiresReset, RebuildPerception, RebuildBrains, etc.) to
// the subset of parameters whose changes need more than a refresh on the next
// frame. Kept as a separate function so the 200-entry `ParameterDefaults`
// file does not need to be touched for this metadata.
void applyPhase23ApplyFlags(ParameterRegistry& registry);

// Phase 23: pretty UI labels per category. The registry stores category as a
// lowercase identifier; the UI uses the prettier display label.
[[nodiscard]] const char* prefsCategoryDisplay(const std::string& category) noexcept;

// Phase 23: enumeration of preference tabs in display order. Tab indices are
// stable so the UI can persist the currently-active tab.
enum class PrefsTab : int
{
    Simulation = 0,
    Physics,
    Vision,
    Neural,
    Autosave,
    Appearance,
    Performance,
    Count
};

[[nodiscard]] const char* prefsTabLabel(PrefsTab tab) noexcept;

// Phase 23: returns the tab a registry category belongs to. Returns -1 when
// the parameter is not exposed via the preferences UI (e.g. species/* lives
// in the Fase 24 editor).
[[nodiscard]] int prefsTabForCategory(const std::string& registryCategory) noexcept;

// Phase 23.1 hotfix: friendly label for the parameter row in the preferences
// UI, localized to the active i18n language (Phase 25.2). Returns nullptr if no
// friendly label is registered (the UI then falls back to the raw name).
// ASCII-only on purpose — the SFML bitmap glyph cache that ships with
// segoeui.ttf renders Latin-1 correctly but using ASCII labels avoids encoding
// surprises across machines/locales.
[[nodiscard]] const char* prefsFriendlyLabel(const std::string& parameterName) noexcept;

// Phase 24.1 fix: friendly label by SUFFIX for species-prefixed parameters.
// The Editor Genetico shows parameters like `bacteria_body_size` /
// `predator_body_size`. After stripping the species prefix, this function
// returns "Tamanho do corpo" — both species share the same Portuguese label.
// Returns nullptr when the suffix is unknown.
[[nodiscard]] const char* prefsFriendlyLabelBySuffix(const std::string& suffix) noexcept;

// Phase 23.1 hotfix: returns true if the parameter should not appear in any
// preferences window. Hides legacy Numba aliases (the canonical C++ name is
// still shown), per-species parameters (Fase 24), and a few raw-string knobs
// without domains that the SFML-native panel cannot edit safely yet.
[[nodiscard]] bool prefsShouldHideParameter(const std::string& parameterName) noexcept;

// Phase 23.2 fix: the registry's `domains` field is used as tags for the
// dump/filter machinery (e.g. {"runtime", "neural"}), NOT as enum values.
// String parameters that the UI presents as combos need a dedicated list.
// Returns an empty vector when the parameter is not an enum.
[[nodiscard]] std::vector<std::string> prefsEnumValuesFor(const std::string& parameterName);

// Phase 25.1: localized display label for one enum value of a combo parameter.
// The canonical value (e.g. "circular") is stored in the registry; the UI shows
// the friendly label (e.g. "Circular"/"Circular") in the active language.
// Returns `value` unchanged when no translation is registered. Handles
// species-prefixed params by suffix.
[[nodiscard]] std::string prefsEnumDisplayLabel(const std::string& parameterName,
                                                  const std::string& value);

// Phase 25.2: curated, localized help text for a parameter's tooltip. This
// supersedes the registry's terse raw `description` with a fuller explanation
// (what the parameter controls and the practical effect of changing it) in the
// active language. Returns nullptr when no curated help exists for the
// parameter, in which case the caller falls back to the raw description.
// Species-prefixed parameters (bacteria_*, predator_*) are matched by suffix.
[[nodiscard]] const char* prefsParameterHelp(const std::string& parameterName) noexcept;

// Phase 25.1: number of decimal places to show for a Floating parameter in the
// UI. Integer-like doubles (world size, radii, spacing, sizes/angles/speeds)
// return 0 so the field shows whole numbers; fine-grained fractions (rates,
// std, decay, scale, strength) return 3; the default is 2.
[[nodiscard]] int prefsDecimalsFor(const std::string& parameterName) noexcept;

// Phase 23.2 fix: returns true if a parameter should appear in the neural
// network window given the currently-selected neural_network_type. Common
// knobs (the network type itself) always return true; per-architecture knobs
// (gated_*, shortcut_*, rnn_*, neat_*, proto_neat_*, recurrent_neat_*) are
// hidden when the corresponding architecture is NOT selected.
[[nodiscard]] bool prefsShouldShowNeuralParameterFor(
    const std::string& parameterName,
    const std::string& currentNetworkType) noexcept;
} // namespace agentbiosim::config
