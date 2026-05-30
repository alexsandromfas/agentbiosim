#pragma once

#include "render/Camera2D.hpp"
#include "sim/SimulationRunner.hpp"
#include "simulation/World.hpp"
#include "ui/Command.hpp"
#include "ui/UiState.hpp"

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include <cstdint>

namespace agentbiosim::ui
{
// Phase 22: InputRouter translates SFML events into engine commands. It does
// not render. It does not mutate stores directly. It uses Camera2D to convert
// screen coordinates into world coordinates and pushes the resulting Command
// into a CommandQueue. The router is also responsible for selection marquee
// state (dragging start/end in world coords) since that is purely input state.
//
// Phase 22.1 hotfix:
//   * pan is now event-driven via MouseMoved (was polled via sf::Mouse::
//     getPosition which returned desktop coords and broke after maximize);
//   * brush/eraser support drag strokes (interpolated stamps between mouse
//     positions) so they behave like paint tools instead of single-click
//     stamps. Stamp spacing is brushRadius * 0.6 to overlap slightly.
class InputRouter
{
public:
    // viewportSize is the current SFML window size in pixels. Pass the same
    // value used when calling Renderer::render so coordinate conversions match.
    void handleEvent(const sf::Event& ev,
                      sf::Vector2u viewportSize,
                      const render::Camera2D& camera,
                      const sim::SimulationRunner& runner,
                      UiState& uiState,
                      CommandQueue& queue);

    // Per-frame update: drives any state that does not depend on a specific
    // SFML event (intentionally minimal in 22.1 — pan is event-driven now).
    void update(sf::Vector2u viewportSize,
                 const render::Camera2D& camera,
                 UiState& uiState,
                 CommandQueue& queue);

    [[nodiscard]] bool panActive() const noexcept { return panActive_; }
    [[nodiscard]] bool paintActive() const noexcept { return paintActive_; }
    [[nodiscard]] bool eraserActive() const noexcept { return eraserActive_; }

    // Phase 22.1 hotfix: stamp spacing for brush/eraser stroke. Public so tests
    // and AppController can use the same constant.
    static constexpr double kBrushSpacingFactor = 0.6;

private:
    // Pan state (right-mouse-button drag).
    bool panActive_ = false;
    sf::Vector2i lastMouseScreen_{0, 0};

    // Marquee/lasso drag.
    bool marqueeActive_ = false;
    simulation::Vec2 marqueeStartWorld_{};
    bool lassoActive_ = false;
    // Lasso accumulator stored in UiState::lasso.

    // Phase 22.1 hotfix: brush/eraser stroke. Records the last stamp world
    // position so MouseMoved can interpolate from there.
    bool paintActive_ = false;
    bool eraserActive_ = false;
    simulation::Vec2 lastStampWorld_{};

    // Shift/Ctrl modifier capture.
    bool shift_ = false;
    bool ctrl_ = false;
};
} // namespace agentbiosim::ui
