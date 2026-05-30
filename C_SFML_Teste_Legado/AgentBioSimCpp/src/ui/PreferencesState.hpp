#pragma once

#include "config/Parameter.hpp"
#include "config/ParameterRegistry.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace agentbiosim::ui
{
// Phase 23: holds pending edits for the preferences window. The pattern is:
//   1. Operator changes a control. UiPreferencesPanel writes the new value to
//      `pendingValues[name]`.
//   2. The "Modificado" badge reads from pendingValues to know what is dirty.
//   3. Operator clicks "Aplicar" — drainPending() writes everything into the
//      ParameterRegistry via setValue(), and the AppController decides what
//      to refresh based on the union of applyFlags from the touched params.
//   4. Operator clicks "Reverter" — pendingValues is cleared without writing.
//   5. Operator clicks "Restaurar Defaults" — pendingValues is filled with
//      originalDefault for every parameter on the current tab.
struct PreferencesState
{
    bool open = false;
    int activeTab = 0;                       // 0..PrefsTab::Count-1
    int hoveredRowIndex = -1;                 // for tooltip rendering
    std::string searchQuery;                  // case-insensitive substring
    std::unordered_map<std::string, config::ParameterValue> pendingValues;

    // Counters for diagnostics.
    std::size_t appliedCount = 0;
    std::size_t revertedCount = 0;
    std::size_t restoredDefaultsCount = 0;
    std::size_t controlInteractions = 0;
};
} // namespace agentbiosim::ui
