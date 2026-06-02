#pragma once

#include "config/Parameter.hpp"
#include "config/ParameterMetadata.hpp"
#include "config/ParameterRegistry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace agentbiosim::ui
{
// Phase 23 + 23.1: pending edits + per-window open/scroll state.
//
// Phase 23 used a single `open` flag + activeTab — the entire preferences
// window was a single tabbed panel. The Microfase 23.1 splits it into one
// independent window per category (so the user can have Fisica and Aparencia
// open side-by-side) and adds:
//   * scroll offset per window;
//   * popup state (-1 / "neural_combo" / "color:<param>" / "help") so that
//     the neural network selector is a real combo dropdown instead of a
//     click-cycle and color values open a swatch popup;
//   * a separate Help window flag (was an overlay in 22.1).
struct PreferencesState
{
    // Phase 23.1: independent open/scroll per window.
    std::array<bool, static_cast<std::size_t>(config::PrefsTab::Count)> windowOpen{};
    std::array<int, static_cast<std::size_t>(config::PrefsTab::Count)> windowScroll{};

    // Phase 23.2 fix: per-window screen position so windows are draggable.
    // -1 sentinel means "use cascaded default in the panel" (no explicit drag yet).
    std::array<float, static_cast<std::size_t>(config::PrefsTab::Count)> windowX{};
    std::array<float, static_cast<std::size_t>(config::PrefsTab::Count)> windowY{};
    int draggingTab = -1;   // -1 = nothing being dragged
    float dragOffsetX = 0.0F;
    float dragOffsetY = 0.0F;

    // Phase 23.2 fix: help window draggable position.
    float helpWindowX = -1.0F;
    float helpWindowY = -1.0F;
    bool draggingHelp = false;

    // Phase 23.2 fix: inline text editor for numeric parameters. The user
    // clicks the value cell of an int/float row to start editing; keystrokes
    // build `editingBuffer`; Enter commits via CmdSetParameterValue and Esc
    // cancels.
    std::string editingParam;
    std::string editingBuffer;

    // Phase 23.1: kept for headless tests that referenced the old API.
    int activeTab = 0;

    // Phase 23: case-insensitive substring search; Phase 23.1 keeps it for the
    // CmdSetPreferencesSearch command path (the panel does not yet expose a
    // search field graphically but the helper works headless).
    std::string searchQuery;

    // Phase 23.1: which popup is currently open. Empty = none.
    //   "neural_combo"        — combo dropdown for neural_network_type
    //   "color:<param_name>"  — color swatch popup for that color parameter
    std::string openPopup;

    // Phase 23.1: separate Help window.
    bool helpWindowOpen = false;
    int helpScroll = 0;

    // Phase 23.1: substrate placeholder (Fase 24) — UI shows a window with a
    // "configurar na Fase 24" message so the user knows where it will land.
    bool substratePlaceholderOpen = false;

    // Phase 24: operational windows (Editor Genetico / Especies / Populacao /
    // Substrato). Each is independent, draggable and has its own scroll.
    bool editorOpen = false;
    bool especiesOpen = false;
    bool populacaoOpen = false;
    bool substratoOpen = false;
    float editorX = -1.0F, editorY = -1.0F;
    float especiesX = -1.0F, especiesY = -1.0F;
    float populacaoX = -1.0F, populacaoY = -1.0F;
    float substratoX = -1.0F, substratoY = -1.0F;
    int editorScroll = 0;
    int especiesScroll = 0;
    int populacaoScroll = 0;
    int substratoScroll = 0;
    // 0=editor, 1=especies, 2=populacao, 3=substrato. -1 = none.
    int draggingOperational = -1;

    // Phase 24.2: persistent LEFT DOCK PANEL (replaces the floating operational
    // windows). Mirrors the Python left tab panel with 3 tabs: Editor Genetico,
    // Substrato, Labels. dockActiveTab 0=Editor 1=Substrato 2=Labels.
    bool dockVisible = true;
    int dockActiveTab = 0;
    int dockScroll = 0;  // shared; reset to 0 when the active tab changes.

    // Phase 24.2: inline edit of a species/label name in the Labels tab.
    // 0 = not editing. Separate from editingParam (registry-param editor).
    std::uint32_t editingSpeciesId = 0;
    std::string editingSpeciesBuffer;

    // Dirty pending edits keyed by registry parameter name.
    std::unordered_map<std::string, config::ParameterValue> pendingValues;

    // Counters for diagnostics.
    std::size_t appliedCount = 0;
    std::size_t revertedCount = 0;
    std::size_t restoredDefaultsCount = 0;
    std::size_t controlInteractions = 0;

    // Phase 23.1 compatibility: returns true if any window is open.
    [[nodiscard]] bool anyOpen() const noexcept
    {
        if (helpWindowOpen || substratePlaceholderOpen) return true;
        if (editorOpen || especiesOpen || populacaoOpen || substratoOpen) return true;
        for (const bool b : windowOpen) if (b) return true;
        return false;
    }

    // Phase 23.2: zero-initialize windowX/Y to sentinel -1.
    PreferencesState() noexcept
    {
        for (auto& v : windowX) v = -1.0F;
        for (auto& v : windowY) v = -1.0F;
    }
};
} // namespace agentbiosim::ui
