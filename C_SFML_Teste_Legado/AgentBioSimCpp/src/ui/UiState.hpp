#pragma once

#include "simulation/World.hpp"
#include "ui/CanvasTool.hpp"
#include "ui/PreferencesState.hpp"
#include "ui/SelectionState.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace agentbiosim::ui
{
// Phase 22: UiState holds purely visual / interaction state. It does not own
// stores. App, AppController and InputRouter share this struct.
struct CanvasRect
{
    simulation::Vec2 startWorld{};
    simulation::Vec2 endWorld{};
    bool active = false;
};

struct CanvasLasso
{
    std::vector<simulation::Vec2> points;
    bool active = false;
};

struct UiState
{
    // Tool currently active.
    CanvasTool activeTool = CanvasTool::Select;

    // Selection ids (stable EntityId).
    SelectionState selection;

    // Marquee rectangle/lasso when the user is dragging.
    CanvasRect marquee;
    CanvasLasso lasso;

    // Panel visibility toggles.
    bool showMenuBar = true;
    bool showToolbar = true;
    bool showHelp = false;
    bool showSelectionOverlay = true;
    bool showToolOverlay = true;

    // Phase 22.1: placeholder panels for menus whose features land in future
    // phases. Each one toggles independently so the user can clearly tell that
    // Preferências != Ajuda (a Phase 22 bug we fixed in 22.1).
    bool showPreferencesPlaceholder = false;
    bool showAboutPanel = false;
    bool showGenomePlaceholder = false;

    // Phase 22.1: which dropdown is currently open (-1 = none, 0..4 = menu idx).
    // Clicks on a menu title toggle this; clicks outside close it.
    int openMenuIndex = -1;

    // Phase 22.1: App watches this; UiPanel sets it via CmdQuitApp.
    bool quitRequested = false;

    // Time scale knob (1.0 = real-time).
    double timeScale = 1.0;

    // Simple counters for diagnostics (UI-side, not engine).
    std::size_t commandsProcessed = 0;
    std::size_t mouseEvents = 0;
    std::size_t keyEvents = 0;

    // Last mouse position in world coords (filled by InputRouter).
    simulation::Vec2 lastMouseWorld{};
    bool lastMouseValid = false;

    // Painter knob (brush radius for PaintObstacle/EraseObstacle).
    double brushRadius = 20.0;

    // Phase 26: selected-agent inspector + neural viewer.
    // `agentPanelOpen` is the retractable panel toggle (menu Agente). The panel
    // only shows when an agent is also selected.
    bool agentPanelOpen = true;
    // Neural viewer layout: false = fixed node spacing (scrolls), true = fill panel.
    bool neuralViewerFillLayout = false;
    // Toggle for the selected-agent vision overlay (Phase 11/12 rays).
    bool selectedVisionOverlay = false;
    // Cross-frame signals written by ImGuiUi and consumed by App to drive the
    // engine's trace target (so the trace is only captured while the Rede Neural
    // tab is actually visible — zero cost otherwise).
    bool neuralTraceActive = false;
    simulation::EntityId neuralTraceAgent{};

    // Phase 27: metrics/profiler window toggle.
    bool showMetricsWindow = false;

    // Phase 23: preferences window state. Owned by UiState so the main loop
    // and the InputRouter can both inspect/react to it (e.g. InputRouter
    // skips canvas tools while preferences is open and captures the click).
    PreferencesState preferences;

    // Phase 23.2: toolbar velocity slider drag state. UiPanel sets the track
    // geometry every frame so the App can map mouse-move events to time_scale
    // on a log scale.
    bool velocitySliderDragging = false;
    float velocitySliderTrackX = 0.0F;
    float velocitySliderTrackW = 1.0F;
};
} // namespace agentbiosim::ui
