#pragma once

#include "config/ParameterRegistry.hpp"
#include "sim/SimulationRunner.hpp"
#include "ui/Command.hpp"
#include "ui/PreferencesState.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <string>
#include <vector>

namespace agentbiosim::ui
{
// Phase 24: operational panels — Editor Genetico, Especies, Populacao,
// Substrato. Each is rendered as an independent draggable + scrollable window
// with its own header, content list and footer with action buttons.
//
// The class mirrors UiPreferencesPanel's mechanics (drag from header, scroll
// per window, close button, rounded corners) but with custom parameter filters
// and a distinct set of footer buttons per window.
class UiOperationalPanels
{
public:
    UiOperationalPanels() = default;
    void setFont(const sf::Font* font) noexcept { font_ = font; }

    bool handleMouseClick(int sx, int sy, sf::Vector2u viewport,
                            const config::ParameterRegistry& registry,
                            const sim::SimulationRunner& runner,
                            PreferencesState& state, CommandQueue& queue);

    void handleMouseMove(int sx, int sy, sf::Vector2u viewport,
                          PreferencesState& state, CommandQueue& queue);

    void handleMouseRelease(int sx, int sy, sf::Vector2u viewport,
                              PreferencesState& state, CommandQueue& queue);

    bool handleMouseWheel(int sx, int sy, sf::Vector2u viewport, float delta,
                            PreferencesState& state, CommandQueue& queue);

    void draw(sf::RenderTarget& target,
              const config::ParameterRegistry& registry,
              const sim::SimulationRunner& runner,
              const PreferencesState& state) const;

    [[nodiscard]] bool pointInsideAnyWindow(int sx, int sy, sf::Vector2u viewport,
                                                const PreferencesState& state) const noexcept;

    // Phase 24 selftest helpers: the registry-driven content lists per panel.
    [[nodiscard]] static std::vector<std::string> editorParameters();
    [[nodiscard]] static std::vector<std::string> populacaoParameters();
    [[nodiscard]] static std::vector<std::string> substratoParameters();

private:
    struct Rect { float x = 0.0F, y = 0.0F, w = 0.0F, h = 0.0F; };

    enum class Which : int { Editor = 0, Especies = 1, Populacao = 2, Substrato = 3 };

    [[nodiscard]] Rect rectFor(Which which, sf::Vector2u viewport,
                                  const PreferencesState& state) const noexcept;

    const sf::Font* font_ = nullptr;
};
} // namespace agentbiosim::ui
