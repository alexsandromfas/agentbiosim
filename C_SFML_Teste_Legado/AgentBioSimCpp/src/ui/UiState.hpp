#pragma once

#include "simulation/World.hpp"
#include "ui/CanvasTool.hpp"
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
};
} // namespace agentbiosim::ui
