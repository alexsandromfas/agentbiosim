#pragma once

#include "config/Parameter.hpp"
#include "core/CanvasTool.hpp"
#include "simulation/EntityId.hpp"
#include "simulation/World.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace agentbiosim::core
{
// Phase 22: high-level engine commands. The InputRouter and UiPanel produce
// these. SimulationRunner consumes them via applyCommand(). Headless tests
// build commands directly and verify state changes; nothing about commands
// depends on SFML.
//
// Phase 25 (Divida 8): moved from agentbiosim::ui to the neutral
// agentbiosim::core layer. Command is passive data (a std::variant of POD
// structs). Moving it here lets sim::SimulationRunner consume commands without
// including ui/, so the engine no longer depends on the UI layer. The ui::
// names remain available transitionally via the thin shim in ui/Command.hpp.
struct CmdPauseToggle {};
struct CmdSetPaused { bool paused = true; };
struct CmdResetSimulation {};
struct CmdStepOnce {};
struct CmdSetTimeScale { double timeScale = 1.0; };

struct CmdFitWorldCamera {};
struct CmdSetCameraCenter { simulation::Vec2 worldCenter{}; };
struct CmdPanCameraScreen { double dx = 0.0; double dy = 0.0; };
struct CmdZoomCameraAt { double factor = 1.0; double screenX = 0.0; double screenY = 0.0; };

struct CmdSetCanvasTool { CanvasTool tool = CanvasTool::Select; };
struct CmdSelectAtWorldPoint { simulation::Vec2 world{}; bool additive = false; double pickRadius = 12.0; };
struct CmdSelectRect { simulation::Vec2 worldA{}; simulation::Vec2 worldB{}; bool additive = false; };
struct CmdSelectLasso { std::vector<simulation::Vec2> worldPolygon; bool additive = false; };
struct CmdClearSelection {};
struct CmdDeleteSelected {};
struct CmdMoveSelectedBy { double dx = 0.0; double dy = 0.0; };

struct CmdSpawnFoodAt { simulation::Vec2 world{}; double radius = 5.0; double energy = 25.0; };
struct CmdSpawnAgentAt { simulation::Vec2 world{}; double radius = 9.0; };
struct CmdPaintObstacleAt { simulation::Vec2 world{}; double brushRadius = 20.0; };
struct CmdEraseObstacleAt { simulation::Vec2 world{}; double eraseRadius = 30.0; };
struct CmdClearObstacles {};
struct CmdClearFood {};

struct CmdToggleSpatialHashOverlay {};
struct CmdToggleSelectionOverlay {};
struct CmdToggleToolOverlay {};
struct CmdToggleHelpPanel {};
struct CmdToggleSimpleRender {};
struct CmdToggleVisionDebug {};

// Phase 22.1 hotfix: new commands for menu items that previously had no
// dedicated dispatch (they were misrouted to Reset/Help in Phase 22). Each one
// triggers a distinct AppController action; SimulationRunner treats them as
// no-ops so the engine layer stays untouched.
struct CmdNewSimulation {};                  // File > Novo (explicit reset)
struct CmdQuitApp {};                        // File > Sair (closes window)
struct CmdTogglePreferencesPanel {};         // Preferências menu (Phase 23 placeholder)
struct CmdToggleAboutPanel {};               // Ajuda > Sobre
struct CmdToggleGenomePanel {};              // Genoma menu (Phase 24/25 placeholder)
struct CmdResetCamera {};                    // View > Reset camera (zoom 1, fit)
struct CmdCloseAllMenus {};                  // Esc/click-outside closes dropdowns

// Phase 22.1 hotfix: stroke variants. PaintObstacleAt/EraseObstacleAt remain
// for single-click stamps. The Stroke variants are emitted by InputRouter when
// the user drags with the mouse held down; they interpolate stamps between the
// previous and current world positions so the brush behaves like a paint
// program instead of a single-stamp click tool.
struct CmdPaintObstacleStroke
{
    simulation::Vec2 worldFrom{};
    simulation::Vec2 worldTo{};
    double brushRadius = 20.0;
};
struct CmdEraseObstacleStroke
{
    simulation::Vec2 worldFrom{};
    simulation::Vec2 worldTo{};
    double eraseRadius = 30.0;
};

// Phase 23: preferences window commands. Set/Apply/Revert/RestoreDefaults are
// dispatched by AppController against the ParameterRegistry. SetParameter
// queues a pending edit (does not touch the registry until Apply).
struct CmdOpenPreferences {};
struct CmdClosePreferences {};
struct CmdSetPreferencesTab { int tab = 0; };
struct CmdSetPreferencesSearch { std::string query; };
struct CmdSetParameterValue
{
    std::string name;
    config::ParameterValue value;
};
struct CmdApplyPreferences {};
struct CmdRevertPreferences {};
struct CmdRestoreDefaultsPreferences {};   // restores entire active tab
struct CmdRestoreParameterDefault { std::string name; };

// Phase 23.1: multi-window preferences. Each category has its own independent
// window that the operator opens/closes from the Preferencias dropdown.
struct CmdOpenPreferencesWindow  { int tab = 0; };
struct CmdClosePreferencesWindow { int tab = 0; };
struct CmdScrollPreferencesWindow { int tab = 0; int delta = 0; };
struct CmdOpenHelpWindow {};
struct CmdCloseHelpWindow {};
struct CmdOpenSubstratePlaceholder {};
struct CmdCloseSubstratePlaceholder {};

// Phase 23.1: popup state for combo / color picker.
struct CmdOpenPrefsPopup  { std::string popup; };  // "neural_combo" / "color:<name>"
struct CmdClosePrefsPopup {};

// Phase 23.1: relative time-scale step driven by the toolbar widget. The App
// reads the current value from the registry, multiplies by `factor`, clamps
// and writes back through ParameterRegistry::setValue.
struct CmdAdjustTimeScale { double factor = 1.0; };

// Phase 23.2 fix: drag a preferences window. -1 for tab and the help flag
// signal start/end of a drag respectively. New X/Y are absolute screen px.
struct CmdMovePreferencesWindow { int tab = 0; float x = 0.0F; float y = 0.0F; };
struct CmdMoveHelpWindow { float x = 0.0F; float y = 0.0F; };

// Phase 23.2 fix: inline text editor for numeric parameter cells.
struct CmdBeginEditParameter { std::string name; std::string initialBuffer; };
struct CmdCancelEditParameter {};
struct CmdCommitEditParameter {};   // App parses the current buffer

// Phase 23.2 fix: "Restaurar padroes" applies immediately instead of only
// populating pendingValues. Uses the same union-of-flags refresh path as
// the regular Apply.
struct CmdRestoreDefaultsAndApply {};

// Phase 24: operational windows. Each window is opened/closed independently;
// the panel routes clicks to its handlers and emits the apply commands.
struct CmdOpenEditorGenetico {};
struct CmdCloseEditorGenetico {};
struct CmdOpenEspecies {};
struct CmdCloseEspecies {};
struct CmdOpenPopulacao {};
struct CmdClosePopulacao {};
struct CmdOpenSubstrato {};
struct CmdCloseSubstrato {};
struct CmdScrollOperationalWindow { int which = 0; int delta = 0; };
struct CmdMoveOperationalWindow   { int which = 0; float x = 0.0F; float y = 0.0F; };

// Phase 24: apply commands for the operational windows.
struct CmdApplyGenomeToSelected {};   // re-spawn selected with current registry genome
struct CmdApplyGenomeToSpecies  {};   // bake registry params into species defaults (next reset)
struct CmdResetNeuralForSpecies { std::uint32_t speciesId = 0; };
struct CmdSelectAllOfSpecies    { std::uint32_t speciesId = 0; };
struct CmdAssignSelectedToSpecies { std::uint32_t speciesId = 0; };
struct CmdCreateSpeciesFromSelected {};
struct CmdApplyPopulation       {};   // honors bacteria_count etc. on next reset
struct CmdApplyEnvironment      {};   // reads world_w/h/shape/radius and resets
struct CmdClearAllFood          {};   // wipes FoodStore now (rescue continues working)

// Phase 24.2: persistent left dock + per-label (species) operations.
struct CmdSetDockTab            { int tab = 0; };
struct CmdScrollDock            { int delta = 0; };
struct CmdToggleLeftDock        {};
struct CmdRemoveSelectedFromSpecies { std::uint32_t speciesId = 0; };  // reassign to bacteria
struct CmdCycleSpeciesColor     { std::uint32_t speciesId = 0; };       // next palette color
struct CmdSetSpeciesShowGraph   { std::uint32_t speciesId = 0; bool show = true; };
struct CmdAdjustSpeciesPop      { std::uint32_t speciesId = 0; int field = 0; int delta = 0; };
struct CmdRemoveSpecies         { std::uint32_t speciesId = 0; };       // soft-disable + reassign
struct CmdBeginEditSpeciesName  { std::uint32_t speciesId = 0; std::string initialBuffer; };
struct CmdCommitEditSpeciesName {};
struct CmdCancelEditSpeciesName {};

// Phase 25 (Dear ImGui): direct species label/color edits. ImGui owns the text
// field and color picker state, so the begin/commit dance and the palette-cycle
// from the SFML dock are no longer needed — the widget emits the final value.
struct CmdSetSpeciesLabel  { std::uint32_t speciesId = 0; std::string label; };
struct CmdSetSpeciesColor  { std::uint32_t speciesId = 0; int r = 0; int g = 0; int b = 0; };

// Phase 28: save/load. App-level handlers (open a file dialog + do file I/O);
// the engine treats them as no-ops. Save uses the current path (prompting once
// if none); Save As always prompts; Load prompts to open a .agentbiosim.
struct CmdSaveSimulation {};
struct CmdSaveSimulationAs {};
struct CmdLoadSimulation {};
// Phase 28: single-organism export/import (Agente menu). Export saves the
// selected agent to a .organism file; import spawns one from a file.
struct CmdExportAgent {};
struct CmdImportAgent {};

// Phase 30: developer-window cost-isolation toggle. `section` follows
// core::ProfileSection indices (Perception..SpatialHash); -1 restores all
// systems to enabled. Handled by SimulationRunner::applyCommand.
struct CmdSetDevSystemEnabled { int section = -1; bool enabled = true; };

using Command = std::variant<
    CmdPauseToggle,
    CmdSetPaused,
    CmdResetSimulation,
    CmdStepOnce,
    CmdSetTimeScale,
    CmdFitWorldCamera,
    CmdSetCameraCenter,
    CmdPanCameraScreen,
    CmdZoomCameraAt,
    CmdSetCanvasTool,
    CmdSelectAtWorldPoint,
    CmdSelectRect,
    CmdSelectLasso,
    CmdClearSelection,
    CmdDeleteSelected,
    CmdMoveSelectedBy,
    CmdSpawnFoodAt,
    CmdSpawnAgentAt,
    CmdPaintObstacleAt,
    CmdEraseObstacleAt,
    CmdClearObstacles,
    CmdClearFood,
    CmdToggleSpatialHashOverlay,
    CmdToggleSelectionOverlay,
    CmdToggleToolOverlay,
    CmdToggleHelpPanel,
    CmdToggleSimpleRender,
    CmdToggleVisionDebug,
    CmdNewSimulation,
    CmdQuitApp,
    CmdTogglePreferencesPanel,
    CmdToggleAboutPanel,
    CmdToggleGenomePanel,
    CmdResetCamera,
    CmdCloseAllMenus,
    CmdPaintObstacleStroke,
    CmdEraseObstacleStroke,
    CmdOpenPreferences,
    CmdClosePreferences,
    CmdSetPreferencesTab,
    CmdSetPreferencesSearch,
    CmdSetParameterValue,
    CmdApplyPreferences,
    CmdRevertPreferences,
    CmdRestoreDefaultsPreferences,
    CmdRestoreParameterDefault,
    CmdOpenPreferencesWindow,
    CmdClosePreferencesWindow,
    CmdScrollPreferencesWindow,
    CmdOpenHelpWindow,
    CmdCloseHelpWindow,
    CmdOpenSubstratePlaceholder,
    CmdCloseSubstratePlaceholder,
    CmdOpenPrefsPopup,
    CmdClosePrefsPopup,
    CmdAdjustTimeScale,
    CmdMovePreferencesWindow,
    CmdMoveHelpWindow,
    CmdBeginEditParameter,
    CmdCancelEditParameter,
    CmdCommitEditParameter,
    CmdRestoreDefaultsAndApply,
    CmdOpenEditorGenetico,
    CmdCloseEditorGenetico,
    CmdOpenEspecies,
    CmdCloseEspecies,
    CmdOpenPopulacao,
    CmdClosePopulacao,
    CmdOpenSubstrato,
    CmdCloseSubstrato,
    CmdScrollOperationalWindow,
    CmdMoveOperationalWindow,
    CmdApplyGenomeToSelected,
    CmdApplyGenomeToSpecies,
    CmdResetNeuralForSpecies,
    CmdSelectAllOfSpecies,
    CmdAssignSelectedToSpecies,
    CmdCreateSpeciesFromSelected,
    CmdApplyPopulation,
    CmdApplyEnvironment,
    CmdClearAllFood,
    CmdSetDockTab,
    CmdScrollDock,
    CmdToggleLeftDock,
    CmdRemoveSelectedFromSpecies,
    CmdCycleSpeciesColor,
    CmdSetSpeciesShowGraph,
    CmdAdjustSpeciesPop,
    CmdRemoveSpecies,
    CmdBeginEditSpeciesName,
    CmdCommitEditSpeciesName,
    CmdCancelEditSpeciesName,
    CmdSetSpeciesLabel,
    CmdSetSpeciesColor,
    CmdSaveSimulation,
    CmdSaveSimulationAs,
    CmdLoadSimulation,
    CmdExportAgent,
    CmdImportAgent,
    CmdSetDevSystemEnabled
>;

// Phase 22: simple queue. Commands are produced by InputRouter / UI and
// consumed once per frame by SimulationRunner. The queue is intentionally
// trivial (push + drain) — Phase 28 will add undo/redo if needed.
class CommandQueue
{
public:
    void push(Command cmd) { queue_.push_back(std::move(cmd)); }

    [[nodiscard]] std::size_t size() const noexcept { return queue_.size(); }
    [[nodiscard]] bool empty() const noexcept { return queue_.empty(); }

    [[nodiscard]] std::vector<Command> drain()
    {
        std::vector<Command> out;
        out.swap(queue_);
        return out;
    }

    void clear() noexcept { queue_.clear(); }

private:
    std::vector<Command> queue_;
};
} // namespace agentbiosim::core
