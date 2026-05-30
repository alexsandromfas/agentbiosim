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
// Phase 23: SFML-native preferences window. Renders a left tab column + main
// content area with a header (tab name, search) and a vertical list of
// parameter rows ending in three buttons: Aplicar / Reverter / Defaults.
//
// Controls per type:
//   bool   - clicking the row toggles the pending value.
//   int    - "[-]  value  [+]" steppers with clamping into the param range.
//   float  - same as int with a smaller step inferred from the range.
//   enum   - clicking the row cycles to the next allowed domain value.
//   color  - "[-] [+]" per channel (R / G / B).
//   string - read-only display (text input is out of scope for SFML-native).
//
// The panel never mutates the registry directly; it pushes commands
// (CmdSetParameterValue, CmdApplyPreferences, CmdRevertPreferences,
// CmdRestoreDefaultsPreferences, CmdRestoreParameterDefault) so the engine
// stays free of UI knowledge and headless tests can drive the same logic.
class UiPreferencesPanel
{
public:
    UiPreferencesPanel() = default;

    void setFont(const sf::Font* font) noexcept { font_ = font; }

    // Returns true if the click was consumed by the preferences panel.
    bool handleMouseClick(int screenX, int screenY,
                            const config::ParameterRegistry& registry,
                            PreferencesState& state,
                            CommandQueue& queue);

    void draw(sf::RenderTarget& target,
              const config::ParameterRegistry& registry,
              const PreferencesState& state) const;

    // True if the screen point is inside the preferences window. AppController
    // uses this to keep canvas tools from receiving the click.
    [[nodiscard]] bool pointInside(int screenX, int screenY,
                                      const PreferencesState& state) const noexcept;

private:
    struct Layout
    {
        float windowX = 0.0F;
        float windowY = 0.0F;
        float windowW = 0.0F;
        float windowH = 0.0F;
        float tabColW = 180.0F;
        float headerH = 44.0F;
        float footerH = 50.0F;
        float rowH = 30.0F;
        float gutterX = 14.0F;
    };

    [[nodiscard]] Layout computeLayout(sf::Vector2u viewport) const noexcept;

    const sf::Font* font_ = nullptr;
};

// Phase 23 helpers exposed for headless selftests.

// Returns the list of registry parameter names that belong to the given tab,
// filtered by query (case-insensitive substring across name/desc/aliases).
[[nodiscard]] std::vector<std::string> prefsParametersForTab(
    const config::ParameterRegistry& registry, config::PrefsTab tab,
    const std::string& query);

// Returns the value the panel should display for a parameter (pending edit
// if dirty, otherwise the registry's current value).
[[nodiscard]] config::ParameterValue prefsEffectiveValue(
    const config::ParameterRegistry& registry, const PreferencesState& state,
    const std::string& name);

// Phase 23: short label for the apply-flags badge. "imediato" / "rebuild" /
// "reset" / "pendente Fase 27", etc.
[[nodiscard]] const char* prefsApplyFlagsLabel(unsigned int flags) noexcept;

// Phase 23: writes every pending edit into the registry via setValue.
// Returns the union of applyFlags of every parameter that was actually
// changed. AppController uses the result to decide whether a reset is needed.
unsigned int prefsApplyPending(config::ParameterRegistry& registry,
                                  PreferencesState& state);
} // namespace agentbiosim::ui
