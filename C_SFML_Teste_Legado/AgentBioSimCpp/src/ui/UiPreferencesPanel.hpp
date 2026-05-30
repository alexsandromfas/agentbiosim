#pragma once

#include "config/Parameter.hpp"
#include "config/ParameterMetadata.hpp"
#include "config/ParameterRegistry.hpp"
#include "ui/Command.hpp"
#include "ui/PreferencesState.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderTarget.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::ui
{
// Phase 23 + 23.1 hotfix: SFML-native preferences manager.
//
// Phase 23 used a single tabbed window for everything. The Microfase 23.1
// reworks it into:
//   * one independent window per category, cascaded so multiple can be open;
//   * each window has its own scroll offset (mouse wheel over the window
//     scrolls it; the canvas only zooms when no prefs window is under the
//     cursor — that routing lives in InputRouter::handleEvent);
//   * a popup overlay for the neural network combo and for the color picker;
//   * a separate Help window (was an overlay in 22.1);
//   * a substrate placeholder window that documents the Fase 24 destination.
//
// Controls per type:
//   bool   - clicking the row toggles the pending value.
//   int    - "[-]  value  [+]" steppers with clamping into the param range.
//   float  - same as int, smaller step inferred from the range.
//   enum   - clicking the value opens a combo popup (used for
//            neural_network_type and other string params with domains).
//   color  - clicking the swatch opens a color popup with 8 preset swatches
//            + per-channel +/-16 buttons.
//   string - read-only display (no SFML-native text input yet).
class UiPreferencesPanel
{
public:
    UiPreferencesPanel() = default;

    void setFont(const sf::Font* font) noexcept { font_ = font; }

    // Returns true if the click was consumed by the preferences UI. Multi-window
    // hit-testing iterates open windows; a click inside any window is consumed.
    bool handleMouseClick(int screenX, int screenY,
                            sf::Vector2u viewport,
                            const config::ParameterRegistry& registry,
                            PreferencesState& state,
                            CommandQueue& queue);

    // Mouse-wheel scroll. Returns true if a prefs window was under the cursor
    // and consumed the wheel (so the canvas should NOT zoom for this event).
    bool handleMouseWheel(int screenX, int screenY,
                            sf::Vector2u viewport,
                            float delta,
                            PreferencesState& state,
                            CommandQueue& queue);

    void draw(sf::RenderTarget& target,
              const config::ParameterRegistry& registry,
              const PreferencesState& state) const;

    // Phase 23.1: returns true if the screen point is inside any open
    // preferences window (used by the AppController to route mouse events
    // before InputRouter sees them).
    [[nodiscard]] bool pointInsideAnyWindow(int screenX, int screenY,
                                                sf::Vector2u viewport,
                                                const PreferencesState& state) const noexcept;

private:
    struct WindowRect
    {
        float x = 0.0F;
        float y = 0.0F;
        float w = 0.0F;
        float h = 0.0F;
    };

    [[nodiscard]] WindowRect windowRectForTab(int tab, sf::Vector2u viewport) const noexcept;
    [[nodiscard]] WindowRect helpRect(sf::Vector2u viewport) const noexcept;
    [[nodiscard]] WindowRect substrateRect(sf::Vector2u viewport) const noexcept;

    const sf::Font* font_ = nullptr;
};

// Phase 23 helpers exposed for headless selftests.

[[nodiscard]] std::vector<std::string> prefsParametersForTab(
    const config::ParameterRegistry& registry, config::PrefsTab tab,
    const std::string& query);

[[nodiscard]] config::ParameterValue prefsEffectiveValue(
    const config::ParameterRegistry& registry, const PreferencesState& state,
    const std::string& name);

[[nodiscard]] const char* prefsApplyFlagsLabel(unsigned int flags) noexcept;

unsigned int prefsApplyPending(config::ParameterRegistry& registry,
                                  PreferencesState& state);
} // namespace agentbiosim::ui
