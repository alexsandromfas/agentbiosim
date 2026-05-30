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

// Phase 23.1 hotfix: friendly Portuguese label for the parameter row in the
// preferences UI. Returns nullptr if no friendly label is registered (the UI
// then falls back to the raw name). ASCII-only on purpose — the SFML bitmap
// glyph cache that ships with segoeui.ttf renders Latin-1 correctly but using
// ASCII labels avoids encoding surprises across machines/locales.
[[nodiscard]] const char* prefsFriendlyLabel(const std::string& parameterName) noexcept;

// Phase 23.1 hotfix: returns true if the parameter should not appear in any
// preferences window. Hides legacy Numba aliases (the canonical C++ name is
// still shown), per-species parameters (Fase 24), and a few raw-string knobs
// without domains that the SFML-native panel cannot edit safely yet.
[[nodiscard]] bool prefsShouldHideParameter(const std::string& parameterName) noexcept;
} // namespace agentbiosim::config
