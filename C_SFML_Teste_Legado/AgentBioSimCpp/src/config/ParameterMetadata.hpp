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
} // namespace agentbiosim::config
