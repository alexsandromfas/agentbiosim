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

    // Per-frame update: drives drag previews and pan when right mouse held.
    void update(sf::Vector2u viewportSize,
                 const render::Camera2D& camera,
                 UiState& uiState,
                 CommandQueue& queue);

    [[nodiscard]] bool panActive() const noexcept { return panActive_; }

private:
    // Internal state.
    bool panActive_ = false;
    sf::Vector2i lastMouseScreen_{0, 0};

    // Marquee/lasso drag.
    bool marqueeActive_ = false;
    simulation::Vec2 marqueeStartWorld_{};
    bool lassoActive_ = false;
    // Lasso accumulator stored in UiState::lasso.

    // Shift/Ctrl modifier capture.
    bool shift_ = false;
    bool ctrl_ = false;
};
} // namespace agentbiosim::ui
