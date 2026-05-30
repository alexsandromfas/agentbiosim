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
// Phase 22: SFML-native UI panel. Draws a top bar with menu titles, a toolbar
// row beneath it with tool buttons, and an optional help overlay. Returns
// commands via the queue when the user clicks on a button. It does not
// implement Dear ImGui — that integration is deferred (see status doc).
struct UiPanelMetrics
{
    float menuBarHeight = 24.0F;
    float toolbarHeight = 36.0F;
    float toolbarButtonWidth = 64.0F;
    float menuBarItemWidth = 110.0F;
};

class UiPanel
{
public:
    UiPanel() = default;

    // Optional: attach a font for text drawing. If no font is attached, panel
    // still draws rectangles but no labels.
    void setFont(const sf::Font* font) noexcept { font_ = font; }

    // Handles mouse-button events targeted at the UI area (menu bar/toolbar).
    // Returns true if the event was captured by UI (so InputRouter should
    // skip world-space interpretation of that click).
    bool handleMouseClick(int screenX, int screenY,
                            const sim::SimulationRunner& runner,
                            UiState& uiState,
                            CommandQueue& queue);

    // Draw the UI on top of the world render.
    void draw(sf::RenderTarget& target,
              const sim::SimulationRunner& runner,
              const UiState& uiState) const;

    [[nodiscard]] const UiPanelMetrics& metrics() const noexcept { return metrics_; }

    // Returns true if the screen point is inside the UI strip at the top.
    [[nodiscard]] bool pointInsidePanel(int screenX, int screenY) const noexcept;

private:
    UiPanelMetrics metrics_{};
    const sf::Font* font_ = nullptr;

    // Tool slots displayed in the toolbar (left to right).
    static constexpr std::array<CanvasTool, 10> kTools{{
        CanvasTool::Select,
        CanvasTool::RectangleSelect,
        CanvasTool::LassoSelect,
        CanvasTool::AddFood,
        CanvasTool::AddAgent,
        CanvasTool::PaintObstacle,
        CanvasTool::EraseObstacle,
        CanvasTool::Move,
        CanvasTool::Delete,
        CanvasTool::Pan
    }};
};
} // namespace agentbiosim::ui
