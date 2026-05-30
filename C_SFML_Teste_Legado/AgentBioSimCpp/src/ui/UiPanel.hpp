#pragma once

#include "sim/SimulationRunner.hpp"
#include "ui/CanvasTool.hpp"
#include "ui/Command.hpp"
#include "ui/UiState.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace agentbiosim::ui
{
// Phase 22.1 hotfix UI:
//   * top menu bar opens proper dropdowns (each menu item is clickable);
//   * Pan is removed from the toolbar (pan is right-mouse only);
//   * tool buttons are clearly aligned and the active tool is highlighted;
//   * a small status bar at the right of the toolbar shows population + tool;
//   * Help/Preferences/About are separate placeholder overlays with no
//     accidental routing between them (Phase 22 bug);
//   * brush radius slider next to the brush button.
struct UiPanelMetrics
{
    float menuBarHeight = 28.0F;
    float toolbarHeight = 40.0F;
    float toolbarButtonWidth = 70.0F;
    float menuBarItemWidth = 110.0F;
    float menuDropdownItemHeight = 26.0F;
    float menuDropdownWidth = 220.0F;
};

// Item shown in a menu dropdown. Each item is dispatched by its index inside
// the menu. enabled=false renders the item as disabled (gray) and ignores
// clicks; useful for Phase 23+ features that have not landed yet.
struct MenuDropdownItem
{
    const char* label;
    bool enabled;
    bool separator;  // if true, draws a divider and ignores label
};

class UiPanel
{
public:
    UiPanel() = default;

    void setFont(const sf::Font* font) noexcept { font_ = font; }

    // Handles mouse clicks on the UI area (menu bar + toolbar + open dropdown +
    // open placeholder panel). Returns true if the event was captured by UI.
    bool handleMouseClick(int screenX, int screenY,
                            const sim::SimulationRunner& runner,
                            UiState& uiState,
                            CommandQueue& queue);

    // Draw the UI on top of the world render.
    void draw(sf::RenderTarget& target,
              const sim::SimulationRunner& runner,
              const UiState& uiState) const;

    [[nodiscard]] const UiPanelMetrics& metrics() const noexcept { return metrics_; }

    // Phase 22.1: returns true if the point is inside any interactive panel
    // region (menu bar, toolbar, currently-open dropdown, or visible
    // placeholder overlay). Used by AppController to decide whether to route
    // a click to canvas or to the panel.
    [[nodiscard]] bool pointInsidePanel(int screenX, int screenY,
                                          const UiState& uiState) const noexcept;

private:
    UiPanelMetrics metrics_{};
    const sf::Font* font_ = nullptr;

    // Phase 22.1: Pan removed; toolbar now has 9 canvas tools. Pan stays in the
    // CanvasTool enum for compatibility with Phase 22 selftests but is never
    // exposed in the UI.
    static constexpr std::array<CanvasTool, 9> kTools{{
        CanvasTool::Select,
        CanvasTool::RectangleSelect,
        CanvasTool::LassoSelect,
        CanvasTool::AddFood,
        CanvasTool::AddAgent,
        CanvasTool::PaintObstacle,
        CanvasTool::EraseObstacle,
        CanvasTool::Move,
        CanvasTool::Delete
    }};

    // Phase 22.1: indexable menu titles (one entry per top-level menu).
    // Phase 23.1: View renamed to Exibir (mistura idioma corrigida).
    static constexpr std::array<const char*, 5> kMenuTitles{{
        "Arquivo", "Exibir", "Preferencias", "Genoma", "Ajuda"
    }};
};

// Phase 22.1: helpers exposed so headless tests can verify dropdown content
// without instancing a UiPanel.
[[nodiscard]] std::vector<MenuDropdownItem> menuItemsForIndex(int menuIndex) noexcept;
[[nodiscard]] bool dispatchMenuItem(int menuIndex, int itemIndex, CommandQueue& queue) noexcept;
} // namespace agentbiosim::ui
