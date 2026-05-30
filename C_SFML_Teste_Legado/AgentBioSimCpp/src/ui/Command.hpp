#pragma once

#include "config/Parameter.hpp"
#include "simulation/EntityId.hpp"
#include "simulation/World.hpp"
#include "ui/CanvasTool.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace agentbiosim::ui
{
// Phase 22: high-level engine commands. The InputRouter and UiPanel produce
// these. SimulationRunner consumes them via applyCommand(). Headless tests
// build commands directly and verify state changes; nothing about commands
// depends on SFML.
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
    CmdRestoreParameterDefault
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
} // namespace agentbiosim::ui
