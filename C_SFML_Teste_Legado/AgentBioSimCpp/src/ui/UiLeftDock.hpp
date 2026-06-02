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
// Phase 24.2: persistent LEFT DOCK panel that replaces the floating operational
// windows. It mirrors the Python left-side QTabWidget exactly: a fixed-width
// panel docked to the left edge (below the top menu/toolbar strip) with three
// tabs in order — "Editor Genetico", "Substrato", "Labels".
//
// It is an SFML overlay: the world canvas keeps rendering full-window behind it,
// and the dock consumes any mouse event whose point is inside its rectangle
// (via pointInsideDock), exactly like the prefs windows did, so canvas tools do
// not fire underneath. This keeps the Microfase 22.1 screen->world conversion
// untouched.
//
// All actions are emitted as commands; the engine applies them. The Labels tab
// reads species records from the SimulationRunner (read-only) and emits the
// Phase 24.2 per-species commands (assign/create/remove/recolor/...).
class UiLeftDock
{
public:
    UiLeftDock() = default;
    void setFont(const sf::Font* font) noexcept { font_ = font; }

    static constexpr float kDockW = 460.0F;

    [[nodiscard]] bool visible(const PreferencesState& state) const noexcept
    {
        return state.dockVisible;
    }

    // Returns the dock rectangle in screen pixels (below the top strip).
    struct Rect { float x = 0.0F, y = 0.0F, w = 0.0F, h = 0.0F; };
    [[nodiscard]] Rect dockRect(sf::Vector2u viewport, float topStripH) const noexcept;

    [[nodiscard]] bool pointInsideDock(int sx, int sy, sf::Vector2u viewport,
                                          float topStripH,
                                          const PreferencesState& state) const noexcept;

    bool handleMouseClick(int sx, int sy, sf::Vector2u viewport, float topStripH,
                            const config::ParameterRegistry& registry,
                            const sim::SimulationRunner& runner,
                            PreferencesState& state, CommandQueue& queue);

    bool handleMouseWheel(int sx, int sy, sf::Vector2u viewport, float topStripH,
                            float delta, PreferencesState& state, CommandQueue& queue);

    void draw(sf::RenderTarget& target, float topStripH,
              const config::ParameterRegistry& registry,
              const sim::SimulationRunner& runner,
              const PreferencesState& state) const;

    // Phase 24 selftest helpers (kept stable across the restructure).
    [[nodiscard]] static std::vector<std::string> editorParameters();
    [[nodiscard]] static std::vector<std::string> substratoParameters();

private:
    const sf::Font* font_ = nullptr;
};
} // namespace agentbiosim::ui
