#include "ui/UiPanel.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace agentbiosim::ui
{
namespace
{
const sf::Color kBgMenu(20, 22, 28);
const sf::Color kBgToolbar(28, 30, 36);
const sf::Color kBgButton(40, 44, 52);
const sf::Color kBgButtonActive(70, 130, 200);
const sf::Color kBgButtonDisabled(30, 32, 38);
const sf::Color kTextLight(220, 224, 232);
const sf::Color kTextDim(120, 128, 138);
const sf::Color kAccent(130, 200, 250);
const sf::Color kDropdownBg(24, 26, 32, 244);
const sf::Color kDropdownHover(60, 80, 110);
const sf::Color kPanelBorder(70, 130, 200, 180);

void drawText(sf::RenderTarget& target, const sf::Font* font, const std::string& s,
               float x, float y, unsigned size, sf::Color color)
{
    if (font == nullptr) return;
    sf::Text t;
    t.setFont(*font);
    t.setString(s);
    t.setCharacterSize(size);
    t.setFillColor(color);
    t.setPosition(x, y);
    target.draw(t);
}
} // namespace

// Phase 22.1: per-menu dropdown content. Items marked enabled=false are
// rendered grayed-out and do not dispatch. Separators take a slot but ignore
// label/click.
std::vector<MenuDropdownItem> menuItemsForIndex(const int menuIndex) noexcept
{
    switch (menuIndex)
    {
    case 0: // Arquivo
        return {
            {"Novo (R)",        true,  false},
            {"",                false, true},
            {"Abrir...",        false, false},   // Fase 27
            {"Salvar",          false, false},   // Fase 27
            {"Salvar como...",  false, false},   // Fase 27
            {"Exportar",        false, false},   // Fase 27
            {"Importar",        false, false},   // Fase 27
            {"",                false, true},
            {"Sair",            true,  false}
        };
    case 1: // View
        return {
            {"Fit world (F)",          true,  false},
            {"Reset camera",           true,  false},
            {"",                       false, true},
            {"Render simples (T)",     true,  false},
            {"Vision debug (V)",       true,  false},
            {"Spatial hash overlay",   true,  false},
            {"Selection overlay",      true,  false},
            {"Tool overlay",           true,  false}
        };
    case 2: // Preferências - Phase 23.1: one item per window.
        return {
            {"Renderizar",                    true,  false},  // toggles render_enabled
            {"",                              false, true},
            {"Opcoes de simulacao",           true,  false},  // -> Simulation window
            {"Autosave",                      true,  false},  // -> Autosave window
            {"",                              false, true},
            {"Aparencia do ambiente",         true,  false},  // -> Appearance window
            {"Sistema de Visao",              true,  false},  // -> Vision window
            {"Redes neurais",                 true,  false},  // -> Neural window
            {"Fisica",                        true,  false},  // -> Physics window
            {"Performance",                   true,  false},  // -> Performance window
            {"",                              false, true},
            {"Substrato (Fase 24)",           true,  false}   // -> Substrate placeholder
        };
    case 3: // Genoma (Phase 24: now functional with operational windows)
        return {
            {"Editor Genetico",            true,  false},
            {"Especies / Labels",          true,  false},
            {"Populacao",                  true,  false},
            {"Substrato e Comida",         true,  false},
            {"",                           false, true},
            {"Importar Genoma (Fase 27)",  false, false},
            {"Exportar Genoma (Fase 27)",  false, false}
        };
    case 4: // Ajuda
        return {
            {"Atalhos (H)",            true,  false},
            {"Sobre...",               true,  false}
        };
    default:
        return {};
    }
}

bool dispatchMenuItem(const int menuIndex, const int itemIndex, CommandQueue& queue) noexcept
{
    const auto items = menuItemsForIndex(menuIndex);
    if (itemIndex < 0 || itemIndex >= static_cast<int>(items.size())) return false;
    const auto& item = items[static_cast<std::size_t>(itemIndex)];
    if (item.separator || !item.enabled) return false;

    // Phase 22.1: distinct dispatch per menu and per item index so that
    // Preferências != Ajuda and clicking a menu title never resets anything.
    switch (menuIndex)
    {
    case 0: // Arquivo
        if (itemIndex == 0) { queue.push(CmdNewSimulation{}); return true; }
        if (itemIndex == 8) { queue.push(CmdQuitApp{}); return true; }
        return false;
    case 1: // View
        switch (itemIndex)
        {
        case 0: queue.push(CmdFitWorldCamera{}); return true;
        case 1: queue.push(CmdResetCamera{}); return true;
        case 3: queue.push(CmdToggleSimpleRender{}); return true;
        case 4: queue.push(CmdToggleVisionDebug{}); return true;
        case 5: queue.push(CmdToggleSpatialHashOverlay{}); return true;
        case 6: queue.push(CmdToggleSelectionOverlay{}); return true;
        case 7: queue.push(CmdToggleToolOverlay{}); return true;
        default: return false;
        }
    case 2: // Preferências - Phase 23.1: each item opens its own window.
        switch (itemIndex)
        {
        case 0: queue.push(CmdToggleSimpleRender{}); return true;  // Renderizar (uses render_enabled toggle alias; real toggle is in App)
        case 2: queue.push(CmdOpenPreferencesWindow{
                    static_cast<int>(config::PrefsTab::Simulation)}); return true;
        case 3: queue.push(CmdOpenPreferencesWindow{
                    static_cast<int>(config::PrefsTab::Autosave)}); return true;
        case 5: queue.push(CmdOpenPreferencesWindow{
                    static_cast<int>(config::PrefsTab::Appearance)}); return true;
        case 6: queue.push(CmdOpenPreferencesWindow{
                    static_cast<int>(config::PrefsTab::Vision)}); return true;
        case 7: queue.push(CmdOpenPreferencesWindow{
                    static_cast<int>(config::PrefsTab::Neural)}); return true;
        case 8: queue.push(CmdOpenPreferencesWindow{
                    static_cast<int>(config::PrefsTab::Physics)}); return true;
        case 9: queue.push(CmdOpenPreferencesWindow{
                    static_cast<int>(config::PrefsTab::Performance)}); return true;
        case 11: queue.push(CmdOpenSubstratePlaceholder{}); return true;
        default: return false;
        }
    case 3: // Genoma — Phase 24
        switch (itemIndex)
        {
        case 0: queue.push(CmdOpenEditorGenetico{}); return true;
        case 1: queue.push(CmdOpenEspecies{});       return true;
        case 2: queue.push(CmdOpenPopulacao{});      return true;
        case 3: queue.push(CmdOpenSubstrato{});      return true;
        default: return false;
        }
    case 4: // Ajuda - Phase 23.1: opens dedicated Help window.
        if (itemIndex == 0) { queue.push(CmdOpenHelpWindow{}); return true; }
        if (itemIndex == 1) { queue.push(CmdToggleAboutPanel{}); return true; }
        return false;
    default:
        return false;
    }
}

bool UiPanel::pointInsidePanel(const int screenX, const int screenY,
                                  const UiState& uiState) const noexcept
{
    if (screenY < 0) return false;
    const float stripBottom = metrics_.menuBarHeight + metrics_.toolbarHeight;
    if (static_cast<float>(screenY) < stripBottom) return true;

    // Open dropdown.
    if (uiState.openMenuIndex >= 0 &&
        uiState.openMenuIndex < static_cast<int>(kMenuTitles.size()))
    {
        const float x = static_cast<float>(uiState.openMenuIndex) * metrics_.menuBarItemWidth;
        const float y = metrics_.menuBarHeight;
        const auto items = menuItemsForIndex(uiState.openMenuIndex);
        const float h = metrics_.menuDropdownItemHeight * static_cast<float>(items.size());
        const float w = metrics_.menuDropdownWidth;
        if (static_cast<float>(screenX) >= x && static_cast<float>(screenX) < x + w &&
            static_cast<float>(screenY) >= y && static_cast<float>(screenY) < y + h)
        {
            return true;
        }
    }

    // Visible placeholder overlays.
    if (uiState.showHelp || uiState.showPreferencesPlaceholder ||
        uiState.showAboutPanel || uiState.showGenomePlaceholder)
    {
        // Overlays are anchored just below the toolbar at fixed coordinates.
        // pointInsidePanel reports the strip-area only; the App layer handles
        // overlay clicks by closing them via Esc — overlays are read-only
        // placeholders in 22.1, so capturing all clicks would be wrong (the
        // user should still be able to interact with the canvas under them).
    }
    static_cast<void>(screenX);
    return false;
}

bool UiPanel::handleMouseClick(const int screenX, const int screenY,
                                  const sim::SimulationRunner& runner,
                                  UiState& uiState,
                                  CommandQueue& queue)
{
    static_cast<void>(runner);
    const float menuBottom = metrics_.menuBarHeight;
    const float toolbarBottom = menuBottom + metrics_.toolbarHeight;

    // -------- Menu bar click: toggles dropdown for that menu --------
    if (screenY >= 0 && static_cast<float>(screenY) < menuBottom)
    {
        const int slot = static_cast<int>(static_cast<float>(screenX) / metrics_.menuBarItemWidth);
        if (slot < 0 || slot >= static_cast<int>(kMenuTitles.size())) return true;
        // Click on a menu title toggles its dropdown. Clicking a different
        // title switches to that one.
        uiState.openMenuIndex = uiState.openMenuIndex == slot ? -1 : slot;
        return true;
    }

    // -------- Open dropdown click --------
    if (uiState.openMenuIndex >= 0 &&
        uiState.openMenuIndex < static_cast<int>(kMenuTitles.size()))
    {
        const float x = static_cast<float>(uiState.openMenuIndex) * metrics_.menuBarItemWidth;
        const float y = metrics_.menuBarHeight;
        const auto items = menuItemsForIndex(uiState.openMenuIndex);
        const float h = metrics_.menuDropdownItemHeight * static_cast<float>(items.size());
        const float w = metrics_.menuDropdownWidth;
        if (static_cast<float>(screenX) >= x && static_cast<float>(screenX) < x + w &&
            static_cast<float>(screenY) >= y && static_cast<float>(screenY) < y + h)
        {
            const int itemIdx = static_cast<int>(
                (static_cast<float>(screenY) - y) / metrics_.menuDropdownItemHeight);
            if (itemIdx >= 0 && itemIdx < static_cast<int>(items.size()))
            {
                if (dispatchMenuItem(uiState.openMenuIndex, itemIdx, queue))
                {
                    ++uiState.commandsProcessed;
                    uiState.openMenuIndex = -1;  // close dropdown on dispatch
                }
            }
            return true;
        }
    }

    // -------- Toolbar click --------
    if (static_cast<float>(screenY) >= menuBottom &&
        static_cast<float>(screenY) < toolbarBottom)
    {
        const int slot = static_cast<int>(static_cast<float>(screenX) / metrics_.toolbarButtonWidth);
        const int toolCount = static_cast<int>(kTools.size());
        if (slot >= 0 && slot < toolCount)
        {
            queue.push(CmdSetCanvasTool{kTools[static_cast<std::size_t>(slot)]});
        }
        else if (slot == toolCount + 0)
        {
            queue.push(CmdPauseToggle{});
        }
        else if (slot == toolCount + 1)
        {
            // Phase 22.1: Reset button uses CmdNewSimulation so the engine
            // treats it identically to File > Novo.
            queue.push(CmdNewSimulation{});
        }
        else if (slot == toolCount + 2)
        {
            queue.push(CmdFitWorldCamera{});
        }
        // Phase 23.2: velocity widget is a draggable slider on the toolbar.
        // Click anywhere on the track jumps to the clicked position and
        // starts a drag (App handles the MouseMoved to continue the drag).
        else if (slot >= toolCount + 3 && slot <= toolCount + 5)
        {
            const float trackX = uiState.velocitySliderTrackX;
            const float trackW = uiState.velocitySliderTrackW;
            if (trackW > 1.0F)
            {
                const float rel = std::clamp(
                    (static_cast<float>(screenX) - trackX) / trackW, 0.0F, 1.0F);
                const double minV = 0.1;
                const double maxV = 50.0;
                const double v = std::pow(10.0,
                    std::log10(minV) +
                    static_cast<double>(rel) * (std::log10(maxV) - std::log10(minV)));
                queue.push(CmdSetTimeScale{v});
                uiState.velocitySliderDragging = true;
            }
        }
        ++uiState.commandsProcessed;
        uiState.openMenuIndex = -1;  // any toolbar click closes open menus
        return true;
    }

    // Outside menu bar + toolbar but the user might have wanted to close an
    // open dropdown by clicking elsewhere — emit CloseAllMenus and pass through
    // so the canvas still receives the click.
    if (uiState.openMenuIndex >= 0) uiState.openMenuIndex = -1;
    return false;
}

void UiPanel::draw(sf::RenderTarget& target,
                     const sim::SimulationRunner& runner,
                     const UiState& uiState) const
{
    const float w = static_cast<float>(target.getSize().x);

    // -------- Menu bar --------
    if (uiState.showMenuBar)
    {
        sf::RectangleShape bar({w, metrics_.menuBarHeight});
        bar.setFillColor(kBgMenu);
        target.draw(bar);
        for (std::size_t i = 0; i < kMenuTitles.size(); ++i)
        {
            const float x = static_cast<float>(i) * metrics_.menuBarItemWidth;
            const bool open = uiState.openMenuIndex == static_cast<int>(i);
            if (open)
            {
                sf::RectangleShape hi({metrics_.menuBarItemWidth, metrics_.menuBarHeight});
                hi.setPosition(x, 0.0F);
                hi.setFillColor(kBgButtonActive);
                target.draw(hi);
            }
            drawText(target, font_, kMenuTitles[i],
                       x + 12.0F, 6.0F, 14U, open ? sf::Color::White : kTextLight);
        }
    }

    // -------- Toolbar --------
    if (uiState.showToolbar)
    {
        const float y = uiState.showMenuBar ? metrics_.menuBarHeight : 0.0F;
        sf::RectangleShape bar({w, metrics_.toolbarHeight});
        bar.setPosition(0.0F, y);
        bar.setFillColor(kBgToolbar);
        target.draw(bar);

        for (std::size_t i = 0; i < kTools.size(); ++i)
        {
            const float x = static_cast<float>(i) * metrics_.toolbarButtonWidth;
            sf::RectangleShape btn({metrics_.toolbarButtonWidth - 4.0F,
                                     metrics_.toolbarHeight - 6.0F});
            btn.setPosition(x + 2.0F, y + 3.0F);
            const bool active = (kTools[i] == uiState.activeTool);
            btn.setFillColor(active ? kBgButtonActive : kBgButton);
            btn.setOutlineThickness(active ? 1.6F : 0.0F);
            btn.setOutlineColor(kAccent);
            target.draw(btn);
            drawText(target, font_, canvasToolLabel(kTools[i]),
                       x + 8.0F, y + 11.0F, 13U,
                       active ? sf::Color::White : kTextLight);
        }

        // Phase 22.1: Play/Pause, Reset, Fit controls at the right side of
        // the toolbar (immediately after the 9 tools).
        const float ctrlsStart = static_cast<float>(kTools.size()) * metrics_.toolbarButtonWidth;
        struct Ctrl { const char* label; };
        const std::array<Ctrl, 3> ctrls{{
            {runner.paused() ? "Play" : "Pause"},
            {"Novo"},
            {"Fit"}
        }};
        for (std::size_t i = 0; i < ctrls.size(); ++i)
        {
            const float x = ctrlsStart + static_cast<float>(i) * metrics_.toolbarButtonWidth;
            sf::RectangleShape btn({metrics_.toolbarButtonWidth - 4.0F,
                                     metrics_.toolbarHeight - 6.0F});
            btn.setPosition(x + 2.0F, y + 3.0F);
            btn.setFillColor(kBgButton);
            target.draw(btn);
            drawText(target, font_, ctrls[i].label, x + 8.0F, y + 11.0F, 13U, kTextLight);
        }

        // Phase 23.2: velocity slider — draggable horizontal track + thumb.
        // Layout occupies 3 toolbar-button slots after Fit.
        const float velStart = ctrlsStart + static_cast<float>(ctrls.size()) *
                                                metrics_.toolbarButtonWidth;
        const float velW = metrics_.toolbarButtonWidth * 3.0F - 4.0F;
        // Label above the track.
        drawText(target, font_, "Velocidade",
                   velStart + 6.0F, y + 4.0F, 10U, kTextDim);
        std::ostringstream vs;
        vs << std::fixed << std::setprecision(2) << uiState.timeScale << "x";
        drawText(target, font_, vs.str(),
                   velStart + velW - 56.0F, y + 4.0F, 10U, kAccent);
        // Track.
        const float trackH = 6.0F;
        const float trackX = velStart + 6.0F;
        const float trackY = y + metrics_.toolbarHeight * 0.6F;
        const float trackW = velW - 12.0F;
        sf::RectangleShape track({trackW, trackH});
        track.setPosition(trackX, trackY);
        track.setFillColor(kBgButton);
        target.draw(track);
        // Thumb position from time_scale on log scale.
        const double minV = 0.1;
        const double maxV = 50.0;
        const double ts = std::clamp(uiState.timeScale, minV, maxV);
        const float rel = static_cast<float>(
            (std::log10(ts) - std::log10(minV)) /
            (std::log10(maxV) - std::log10(minV)));
        const float thumbX = trackX + rel * trackW - 6.0F;
        sf::RectangleShape thumb({12.0F, trackH + 8.0F});
        thumb.setPosition(thumbX, trackY - 4.0F);
        thumb.setFillColor(kAccent);
        target.draw(thumb);
        // Tick marks at 1x and 10x positions so the user gets a reference.
        for (double mark : {1.0, 10.0})
        {
            const float r = static_cast<float>(
                (std::log10(mark) - std::log10(minV)) /
                (std::log10(maxV) - std::log10(minV)));
            sf::RectangleShape tick({1.0F, 4.0F});
            tick.setPosition(trackX + r * trackW, trackY - 2.0F);
            tick.setFillColor(kTextDim);
            target.draw(tick);
        }
        // Phase 23.2: publish track geometry so App can map MouseMoved to ts.
        const_cast<UiState&>(uiState).velocitySliderTrackX = trackX;
        const_cast<UiState&>(uiState).velocitySliderTrackW = trackW;

        // Status text at the right edge — pushed flush right.
        std::ostringstream status;
        status << "tool: " << canvasToolLabel(uiState.activeTool)
                 << "  ag=" << runner.agents().size()
                 << "  f=" << runner.foods().size()
                 << "  o=" << runner.obstacles().size()
                 << "  sel=" << uiState.selection.size()
                 << "  step=" << runner.stats().stepsExecuted;
        drawText(target, font_, status.str(), w - 380.0F, y + 11.0F, 12U, kAccent);
    }

    // -------- Open dropdown --------
    if (uiState.openMenuIndex >= 0 &&
        uiState.openMenuIndex < static_cast<int>(kMenuTitles.size()))
    {
        const float x = static_cast<float>(uiState.openMenuIndex) * metrics_.menuBarItemWidth;
        const float y = metrics_.menuBarHeight;
        const auto items = menuItemsForIndex(uiState.openMenuIndex);
        const float h = metrics_.menuDropdownItemHeight * static_cast<float>(items.size());
        const float dw = metrics_.menuDropdownWidth;
        sf::RectangleShape bg({dw, h});
        bg.setPosition(x, y);
        bg.setFillColor(kDropdownBg);
        bg.setOutlineColor(kPanelBorder);
        bg.setOutlineThickness(1.0F);
        target.draw(bg);
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& it = items[i];
            const float iy = y + static_cast<float>(i) * metrics_.menuDropdownItemHeight;
            if (it.separator)
            {
                sf::RectangleShape sep({dw - 16.0F, 1.0F});
                sep.setPosition(x + 8.0F, iy + metrics_.menuDropdownItemHeight * 0.5F);
                sep.setFillColor(kTextDim);
                target.draw(sep);
                continue;
            }
            drawText(target, font_, it.label, x + 14.0F, iy + 6.0F, 13U,
                       it.enabled ? kTextLight : kTextDim);
        }
    }

    // -------- Help overlay --------
    if (uiState.showHelp)
    {
        const float panelX = 14.0F;
        const float panelY = metrics_.menuBarHeight + metrics_.toolbarHeight + 12.0F;
        const float panelW = 400.0F;
        const float panelH = 360.0F;
        sf::RectangleShape bg({panelW, panelH});
        bg.setPosition(panelX, panelY);
        bg.setFillColor(sf::Color(15, 17, 22, 232));
        bg.setOutlineColor(kPanelBorder);
        bg.setOutlineThickness(1.4F);
        target.draw(bg);
        const std::array<const char*, 18> lines{{
            "Atalhos - Fase 22.1",
            "",
            "Space        Play / Pause",
            "Esc          Limpar selecao / fechar menus",
            "Delete       Matar selecionados",
            "R            Reset simulacao",
            "F            Fit world",
            "T            Toggle render simples",
            "V            Toggle visao debug",
            "H            Toggle esta ajuda",
            "S / Q / L    Tool: Select / Rect / Lasso",
            "G / A        Tool: Add Food / Add Agent",
            "B / X        Tool: Brush / Eraser",
            "M / D        Tool: Move / Delete",
            "WASD / Setas Pan camera",
            "Scroll       Zoom (centrado no cursor)",
            "Right drag   Pan camera",
            "Shift/Ctrl   Selecao aditiva"
        }};
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            drawText(target, font_, lines[i], panelX + 14.0F,
                       panelY + 12.0F + static_cast<float>(i) * 18.0F, 12U,
                       i == 0 ? kAccent : kTextLight);
        }
    }

    // -------- Preferences placeholder --------
    if (uiState.showPreferencesPlaceholder)
    {
        const float panelX = 14.0F;
        const float panelY = metrics_.menuBarHeight + metrics_.toolbarHeight + 12.0F;
        const float panelW = 400.0F;
        const float panelH = 130.0F;
        sf::RectangleShape bg({panelW, panelH});
        bg.setPosition(panelX, panelY);
        bg.setFillColor(sf::Color(15, 17, 22, 232));
        bg.setOutlineColor(kPanelBorder);
        bg.setOutlineThickness(1.4F);
        target.draw(bg);
        const std::array<const char*, 5> lines{{
            "Preferencias - placeholder",
            "",
            "O painel completo de parametros entra na Fase 23.",
            "Por enquanto este overlay confirma que o menu",
            "Preferencias esta separado da Ajuda."
        }};
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            drawText(target, font_, lines[i], panelX + 14.0F,
                       panelY + 12.0F + static_cast<float>(i) * 18.0F, 12U,
                       i == 0 ? kAccent : kTextLight);
        }
    }

    // -------- About placeholder --------
    if (uiState.showAboutPanel)
    {
        const float panelX = 14.0F;
        const float panelY = metrics_.menuBarHeight + metrics_.toolbarHeight + 12.0F;
        const float panelW = 400.0F;
        const float panelH = 100.0F;
        sf::RectangleShape bg({panelW, panelH});
        bg.setPosition(panelX, panelY);
        bg.setFillColor(sf::Color(15, 17, 22, 232));
        bg.setOutlineColor(kPanelBorder);
        bg.setOutlineThickness(1.4F);
        target.draw(bg);
        const std::array<const char*, 4> lines{{
            "AgentBioSimCpp - Fase 22.1",
            "",
            "Migracao C++/SFML do AgentBioSim",
            "UI base + microfase 22.1 de correcao visual/usabilidade"
        }};
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            drawText(target, font_, lines[i], panelX + 14.0F,
                       panelY + 12.0F + static_cast<float>(i) * 18.0F, 12U,
                       i == 0 ? kAccent : kTextLight);
        }
    }

    // -------- Genome placeholder --------
    if (uiState.showGenomePlaceholder)
    {
        const float panelX = 14.0F;
        const float panelY = metrics_.menuBarHeight + metrics_.toolbarHeight + 12.0F;
        const float panelW = 400.0F;
        const float panelH = 110.0F;
        sf::RectangleShape bg({panelW, panelH});
        bg.setPosition(panelX, panelY);
        bg.setFillColor(sf::Color(15, 17, 22, 232));
        bg.setOutlineColor(kPanelBorder);
        bg.setOutlineThickness(1.4F);
        target.draw(bg);
        const std::array<const char*, 4> lines{{
            "Genoma - placeholder",
            "",
            "O editor genetico e o painel de especies entram na Fase 24/25.",
            "Este overlay confirma o menu separado da Ajuda."
        }};
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            drawText(target, font_, lines[i], panelX + 14.0F,
                       panelY + 12.0F + static_cast<float>(i) * 18.0F, 12U,
                       i == 0 ? kAccent : kTextLight);
        }
    }
}
} // namespace agentbiosim::ui
