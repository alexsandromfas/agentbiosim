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
// Phase 23 + 23.1 + 23.2 SFML-native preferences manager.
//
// Phase 23.2 fixes addressed:
//   * windows draggable via the header strip;
//   * help window draggable too;
//   * inline text editor for numeric parameters (click on value cell -> type
//     value -> Enter to commit, Esc to cancel);
//   * combo popups for ALL string enum params via prefsEnumValuesFor() (was
//     leaking the registry's `domains` field which actually stores tags);
//   * neural window filters parameters by the currently-selected
//     neural_network_type (prefsShouldShowNeuralParameterFor);
//   * footer buttons in PT-BR (Aplicar / Reverter / Restaurar padroes / Fechar);
//   * "Restaurar padroes" applies immediately (was only populating pending);
//   * X glyph centered on the close button;
//   * the dim internal-name subtitle below the friendly label was removed.
class UiPreferencesPanel
{
public:
    UiPreferencesPanel() = default;

    void setFont(const sf::Font* font) noexcept { font_ = font; }

    bool handleMouseClick(int screenX, int screenY,
                            sf::Vector2u viewport,
                            const config::ParameterRegistry& registry,
                            PreferencesState& state,
                            CommandQueue& queue);

    void handleMouseMove(int screenX, int screenY,
                          sf::Vector2u viewport,
                          PreferencesState& state,
                          CommandQueue& queue);

    void handleMouseRelease(int screenX, int screenY,
                              sf::Vector2u viewport,
                              PreferencesState& state,
                              CommandQueue& queue);

    bool handleMouseWheel(int screenX, int screenY,
                            sf::Vector2u viewport,
                            float delta,
                            PreferencesState& state,
                            CommandQueue& queue);

    void draw(sf::RenderTarget& target,
              const config::ParameterRegistry& registry,
              const PreferencesState& state) const;

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

    [[nodiscard]] WindowRect windowRectForTab(int tab, sf::Vector2u viewport,
                                                  const PreferencesState& state) const noexcept;
    [[nodiscard]] WindowRect helpRect(sf::Vector2u viewport,
                                          const PreferencesState& state) const noexcept;
    [[nodiscard]] WindowRect substrateRect(sf::Vector2u viewport) const noexcept;

    const sf::Font* font_ = nullptr;
};

[[nodiscard]] std::vector<std::string> prefsParametersForTab(
    const config::ParameterRegistry& registry, config::PrefsTab tab,
    const std::string& query);

// Phase 23.2: same as above but also filters neural-family parameters by the
// currently selected network type for the Neural tab.
[[nodiscard]] std::vector<std::string> prefsParametersForTabFiltered(
    const config::ParameterRegistry& registry,
    const PreferencesState& state,
    config::PrefsTab tab);

[[nodiscard]] config::ParameterValue prefsEffectiveValue(
    const config::ParameterRegistry& registry, const PreferencesState& state,
    const std::string& name);

[[nodiscard]] const char* prefsApplyFlagsLabel(unsigned int flags) noexcept;

unsigned int prefsApplyPending(config::ParameterRegistry& registry,
                                  PreferencesState& state);
} // namespace agentbiosim::ui
