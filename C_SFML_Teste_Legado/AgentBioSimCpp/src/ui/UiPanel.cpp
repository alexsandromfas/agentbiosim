#include "ui/UiPanel.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include <array>
#include <sstream>

namespace agentbiosim::ui
{
namespace
{
const sf::Color kBgMenu(20, 22, 28);
const sf::Color kBgToolbar(28, 30, 36);
const sf::Color kBgButton(40, 44, 52);
const sf::Color kBgButtonActive(70, 130, 200);
const sf::Color kBgButtonHover(60, 64, 72);
const sf::Color kTextLight(220, 220, 220);
const sf::Color kAccent(130, 200, 250);

struct MenuItem
{
    const char* label;
};

constexpr std::array<MenuItem, 5> kMenuItems{{
    {"Arquivo"}, {"View"}, {"Preferencias"}, {"Agente/Genoma"}, {"Ajuda"}
}};

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

bool UiPanel::pointInsidePanel(const int screenX, const int screenY) const noexcept
{
    static_cast<void>(screenX);
    return screenY >= 0 && static_cast<float>(screenY) <
        (metrics_.menuBarHeight + metrics_.toolbarHeight);
}

bool UiPanel::handleMouseClick(const int screenX, const int screenY,
                                  const sim::SimulationRunner& runner,
                                  UiState& uiState,
                                  CommandQueue& queue)
{
    static_cast<void>(runner);
    const float menuY = metrics_.menuBarHeight;
    if (screenY >= 0 && static_cast<float>(screenY) < menuY)
    {
        // Menu bar click. Each item is a fixed-width slot.
        const int slot = static_cast<int>(static_cast<float>(screenX) / metrics_.menuBarItemWidth);
        if (slot < 0 || slot >= static_cast<int>(kMenuItems.size())) return true;
        switch (slot)
        {
        case 0:  // Arquivo: only Novo (reset) and Sair are non-stubs in Phase 22.
            queue.push(CmdResetSimulation{});
            break;
        case 1:  // View: toggle simple render as a representative action.
            queue.push(CmdToggleSimpleRender{});
            break;
        case 2:  // Preferencias: stub - toggle help to give visible feedback.
            queue.push(CmdToggleHelpPanel{});
            break;
        case 3:  // Agente/Genoma: stub - select-agent tool.
            queue.push(CmdSetCanvasTool{CanvasTool::AddAgent});
            break;
        case 4:  // Ajuda: toggle help overlay.
            queue.push(CmdToggleHelpPanel{});
            break;
        default: break;
        }
        return true;
    }
    if (screenY >= static_cast<int>(menuY) &&
        static_cast<float>(screenY) < menuY + metrics_.toolbarHeight)
    {
        // Toolbar click.
        const int slot = static_cast<int>(static_cast<float>(screenX) / metrics_.toolbarButtonWidth);
        if (slot >= 0 && slot < static_cast<int>(kTools.size()))
        {
            queue.push(CmdSetCanvasTool{kTools[static_cast<std::size_t>(slot)]});
        }
        else if (slot == static_cast<int>(kTools.size()))
        {
            // Play/Pause slot.
            queue.push(CmdPauseToggle{});
        }
        else if (slot == static_cast<int>(kTools.size()) + 1)
        {
            // Reset slot.
            queue.push(CmdResetSimulation{});
        }
        else if (slot == static_cast<int>(kTools.size()) + 2)
        {
            // Fit world.
            queue.push(CmdFitWorldCamera{});
        }
        ++uiState.commandsProcessed;
        return true;
    }
    return false;
}

void UiPanel::draw(sf::RenderTarget& target,
                     const sim::SimulationRunner& runner,
                     const UiState& uiState) const
{
    const float w = static_cast<float>(target.getSize().x);

    if (uiState.showMenuBar)
    {
        sf::RectangleShape bar({w, metrics_.menuBarHeight});
        bar.setFillColor(kBgMenu);
        target.draw(bar);
        for (std::size_t i = 0; i < kMenuItems.size(); ++i)
        {
            const float x = static_cast<float>(i) * metrics_.menuBarItemWidth;
            drawText(target, font_, kMenuItems[i].label,
                       x + 8.0F, 4.0F, 13U, kTextLight);
        }
    }

    if (uiState.showToolbar)
    {
        const float y = uiState.showMenuBar ? metrics_.menuBarHeight : 0.0F;
        sf::RectangleShape bar({w, metrics_.toolbarHeight});
        bar.setPosition(0.0F, y);
        bar.setFillColor(kBgToolbar);
        target.draw(bar);

        // Tool buttons.
        for (std::size_t i = 0; i < kTools.size(); ++i)
        {
            const float x = static_cast<float>(i) * metrics_.toolbarButtonWidth;
            sf::RectangleShape btn({metrics_.toolbarButtonWidth - 4.0F,
                                     metrics_.toolbarHeight - 6.0F});
            btn.setPosition(x + 2.0F, y + 3.0F);
            const bool active = (kTools[i] == uiState.activeTool);
            btn.setFillColor(active ? kBgButtonActive : kBgButton);
            target.draw(btn);
            drawText(target, font_, canvasToolLabel(kTools[i]),
                       x + 6.0F, y + 9.0F, 12U,
                       active ? sf::Color::White : kTextLight);
        }

        // Play/Pause + Reset + Fit slots at the right.
        const float ctrlsStart = static_cast<float>(kTools.size()) * metrics_.toolbarButtonWidth;
        struct Ctrl { const char* label; };
        const std::array<Ctrl, 3> ctrls{{
            {runner.paused() ? "Play" : "Pause"},
            {"Reset"},
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
            drawText(target, font_, ctrls[i].label, x + 6.0F, y + 9.0F, 12U, kTextLight);
        }

        // Status text at the right edge of the toolbar.
        std::ostringstream status;
        status << "ag=" << runner.agents().size()
                 << "  f=" << runner.foods().size()
                 << "  o=" << runner.obstacles().size()
                 << "  sel=" << uiState.selection.size()
                 << "  step=" << runner.stats().stepsExecuted;
        drawText(target, font_, status.str(), w - 320.0F, y + 9.0F, 12U, kAccent);
    }

    // Help overlay.
    if (uiState.showHelp)
    {
        const float panelX = 12.0F;
        const float panelY = metrics_.menuBarHeight + metrics_.toolbarHeight + 12.0F;
        const float panelW = 380.0F;
        const float panelH = 320.0F;
        sf::RectangleShape bg({panelW, panelH});
        bg.setPosition(panelX, panelY);
        bg.setFillColor(sf::Color(15, 17, 22, 230));
        bg.setOutlineColor(kAccent);
        bg.setOutlineThickness(1.0F);
        target.draw(bg);
        const std::array<const char*, 16> lines{{
            "Atalhos Fase 22",
            "Space        Play/Pause",
            "Esc          Limpar selecao / cancelar",
            "Delete       Matar selecionados",
            "R            Reset simulacao",
            "F            Fit world",
            "T            Toggle render simples",
            "V            Toggle visao debug",
            "H            Toggle esta ajuda",
            "S            Tool: Select",
            "Q            Tool: Rect Select",
            "L            Tool: Lasso",
            "G            Tool: Add Food",
            "A            Tool: Add Agent",
            "B / X        Tool: Brush / Eraser",
            "Scroll/WASD  Zoom / Pan"
        }};
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            drawText(target, font_, lines[i], panelX + 12.0F,
                       panelY + 12.0F + static_cast<float>(i) * 18.0F, 12U,
                       i == 0 ? kAccent : kTextLight);
        }
    }
}
} // namespace agentbiosim::ui
