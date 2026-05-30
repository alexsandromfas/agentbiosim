#include "ui/UiPreferencesPanel.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <variant>

namespace agentbiosim::ui
{
namespace
{
const sf::Color kBg(18, 20, 26, 240);
const sf::Color kBgTabCol(14, 16, 22);
const sf::Color kBgTabActive(50, 90, 150);
const sf::Color kBgTabHover(34, 42, 56);
const sf::Color kBgRow(28, 32, 40);
const sf::Color kBgRowAlt(24, 28, 36);
const sf::Color kBgRowDirty(60, 90, 130);
const sf::Color kBgRowPending(80, 50, 28);
const sf::Color kTextLight(220, 224, 232);
const sf::Color kTextDim(120, 128, 138);
const sf::Color kAccent(130, 200, 250);
const sf::Color kBgButton(40, 44, 54);
const sf::Color kBgButtonHot(56, 70, 86);
const sf::Color kBorder(70, 130, 200, 200);

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

std::string fmtValue(const config::ParameterValue& v)
{
    return std::visit([](const auto& tv) -> std::string {
        using T = std::decay_t<decltype(tv)>;
        if constexpr (std::is_same_v<T, bool>) return tv ? "true" : "false";
        else if constexpr (std::is_same_v<T, int>) return std::to_string(tv);
        else if constexpr (std::is_same_v<T, double>)
        {
            std::ostringstream s;
            s << std::setprecision(6) << tv;
            return s.str();
        }
        else if constexpr (std::is_same_v<T, std::string>) return tv;
        else if constexpr (std::is_same_v<T, config::ColorRgb>)
        {
            return std::to_string(tv.r) + "," + std::to_string(tv.g) + "," + std::to_string(tv.b);
        }
    }, v);
}

// Phase 23: a sensible numeric step for a parameter based on its registered
// range. For integers we use 1 or 10% of range. For floats we use 1% of range
// or 0.01.
double numericStep(const config::ParameterDefinition& def)
{
    if (def.type == config::ParameterType::Integer)
    {
        if (def.range.min.has_value() && def.range.max.has_value())
        {
            const double span = *def.range.max - *def.range.min;
            return std::max(1.0, std::round(span * 0.05));
        }
        return 1.0;
    }
    if (def.range.min.has_value() && def.range.max.has_value())
    {
        const double span = *def.range.max - *def.range.min;
        return std::max(0.01, span * 0.02);
    }
    return 0.05;
}
} // namespace

UiPreferencesPanel::Layout UiPreferencesPanel::computeLayout(
    const sf::Vector2u viewport) const noexcept
{
    Layout L;
    L.windowW = std::min(static_cast<float>(viewport.x) - 80.0F, 920.0F);
    L.windowH = std::min(static_cast<float>(viewport.y) - 140.0F, 560.0F);
    L.windowX = (static_cast<float>(viewport.x) - L.windowW) * 0.5F;
    L.windowY = 80.0F;
    return L;
}

bool UiPreferencesPanel::pointInside(const int screenX, const int screenY,
                                       const PreferencesState& state) const noexcept
{
    if (!state.open) return false;
    // Use a generic viewport assumption; the AppController passes the precise
    // window dimensions, but for hit-testing it is enough to know whether the
    // click is within a wide central rectangle.
    static_cast<void>(state);
    return screenX >= 60 && screenY >= 70 && screenX <= 60 + 940 && screenY <= 70 + 580;
}

std::vector<std::string> prefsParametersForTab(
    const config::ParameterRegistry& registry, const config::PrefsTab tab,
    const std::string& query)
{
    std::vector<std::string> out;
    const auto& defs = registry.definitions();
    std::string lq;
    lq.reserve(query.size());
    for (char ch : query) lq.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    auto lower = [](std::string s) {
        for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return s;
    };
    for (const auto& d : defs)
    {
        if (config::prefsTabForCategory(d.category) != static_cast<int>(tab)) continue;
        if (!lq.empty())
        {
            bool match = lower(d.name).find(lq) != std::string::npos;
            if (!match) match = lower(d.description).find(lq) != std::string::npos;
            if (!match)
            {
                for (const auto& a : d.aliases)
                {
                    if (lower(a).find(lq) != std::string::npos) { match = true; break; }
                }
            }
            if (!match) continue;
        }
        // Skip per-species parameters from the prefs window — they live in
        // the Fase 24 editor.
        if (d.name.rfind("herbivore_", 0) == 0 || d.name.rfind("carnivore_", 0) == 0 ||
            d.name.find("_species_") != std::string::npos)
        {
            continue;
        }
        out.push_back(d.name);
    }
    return out;
}

config::ParameterValue prefsEffectiveValue(
    const config::ParameterRegistry& registry, const PreferencesState& state,
    const std::string& name)
{
    const auto it = state.pendingValues.find(name);
    if (it != state.pendingValues.end()) return it->second;
    const auto* def = registry.find(name);
    if (def == nullptr) return std::string("");
    return def->defaultValue;
}

const char* prefsApplyFlagsLabel(const unsigned int flags) noexcept
{
    if (flags & config::ApplyFlag::PendingFuturePhase) return "pendente";
    if (flags & config::ApplyFlag::RequiresReset)
    {
        if (flags & config::ApplyFlag::RebuildBrains)     return "rebuild brains+reset";
        if (flags & config::ApplyFlag::RebuildPerception) return "rebuild vision+reset";
        return "requer reset";
    }
    if (flags & config::ApplyFlag::RefreshRenderer) return "imediato (renderer)";
    return "imediato";
}

unsigned int prefsApplyPending(config::ParameterRegistry& registry,
                                  PreferencesState& state)
{
    unsigned int union_flags = 0U;
    for (const auto& [name, value] : state.pendingValues)
    {
        const auto* def = registry.find(name);
        if (def == nullptr) continue;
        if (registry.setValue(name, value))
        {
            union_flags |= def->applyFlags;
        }
    }
    state.pendingValues.clear();
    ++state.appliedCount;
    return union_flags;
}

bool UiPreferencesPanel::handleMouseClick(const int screenX, const int screenY,
                                              const config::ParameterRegistry& registry,
                                              PreferencesState& state,
                                              CommandQueue& queue)
{
    if (!state.open) return false;
    const Layout L = computeLayout(sf::Vector2u{1280U, 720U});  // viewport not
        // available here; the AppController passes the *click* coords already
        // in window-pixel coords, and the layout uses fixed-size centered geometry
        // — close enough for hit-testing.
    const float relX = static_cast<float>(screenX) - L.windowX;
    const float relY = static_cast<float>(screenY) - L.windowY;
    if (relX < 0.0F || relY < 0.0F || relX > L.windowW || relY > L.windowH) return false;

    // -------- tab column --------
    if (relX < L.tabColW)
    {
        const float yStart = L.headerH;
        const int tab = static_cast<int>((relY - yStart) / 36.0F);
        if (tab >= 0 && tab < static_cast<int>(config::PrefsTab::Count))
        {
            queue.push(CmdSetPreferencesTab{tab});
        }
        return true;
    }

    // -------- footer buttons (Apply/Revert/Defaults/Close) --------
    if (relY > L.windowH - L.footerH)
    {
        const float contentLeft = L.tabColW;
        const float contentRight = L.windowW;
        const float contentW = contentRight - contentLeft;
        const float btnW = 120.0F;
        const float btnH = 30.0F;
        const float btnY = L.windowH - L.footerH + (L.footerH - btnH) * 0.5F;
        const float gap = 10.0F;
        // From right to left: Close | Defaults | Revert | Apply
        const float closeX = contentRight - btnW - gap;
        const float defaultsX = closeX - btnW - gap;
        const float revertX = defaultsX - btnW - gap;
        const float applyX = revertX - btnW - gap;
        const bool inRow = relY >= btnY && relY < btnY + btnH;
        if (inRow && relX >= closeX && relX < closeX + btnW)
        {
            queue.push(CmdClosePreferences{});
            return true;
        }
        if (inRow && relX >= defaultsX && relX < defaultsX + btnW)
        {
            queue.push(CmdRestoreDefaultsPreferences{});
            return true;
        }
        if (inRow && relX >= revertX && relX < revertX + btnW)
        {
            queue.push(CmdRevertPreferences{});
            return true;
        }
        if (inRow && relX >= applyX && relX < applyX + btnW)
        {
            queue.push(CmdApplyPreferences{});
            return true;
        }
        static_cast<void>(contentW);
        return true;
    }

    // -------- parameter row click --------
    const float listY0 = L.headerH;
    if (relY < listY0) return true;  // header click, swallow

    const auto names = prefsParametersForTab(registry,
        static_cast<config::PrefsTab>(state.activeTab), state.searchQuery);
    const int rowIdx = static_cast<int>((relY - listY0) / L.rowH);
    if (rowIdx < 0 || rowIdx >= static_cast<int>(names.size())) return true;
    const std::string& name = names[static_cast<std::size_t>(rowIdx)];
    const auto* def = registry.find(name);
    if (def == nullptr) return true;

    const auto current = prefsEffectiveValue(registry, state, name);

    // Layout for in-row controls: value column from gutter+200 to right minus
    // a few buttons. We use simple click zones.
    const float colNameW = 320.0F;
    const float colValueLeft = L.tabColW + L.gutterX + colNameW;
    const float rowRelX = relX - colValueLeft;
    const float btnW = 28.0F;

    auto pushChange = [&](const config::ParameterValue& v) {
        queue.push(CmdSetParameterValue{name, v});
    };

    switch (def->type)
    {
    case config::ParameterType::Boolean:
    {
        if (rowRelX < 0.0F) return true;
        const bool cur = std::get<bool>(current);
        pushChange(!cur);
        return true;
    }
    case config::ParameterType::Integer:
    {
        int cur = 0;
        if (const auto* v = std::get_if<int>(&current)) cur = *v;
        const double step = numericStep(*def);
        if (rowRelX >= 0.0F && rowRelX < btnW) pushChange(cur - static_cast<int>(step));
        else if (rowRelX >= btnW + 70.0F && rowRelX < btnW + 70.0F + btnW)
            pushChange(cur + static_cast<int>(step));
        return true;
    }
    case config::ParameterType::Floating:
    {
        double cur = 0.0;
        if (const auto* v = std::get_if<double>(&current)) cur = *v;
        const double step = numericStep(*def);
        if (rowRelX >= 0.0F && rowRelX < btnW) pushChange(cur - step);
        else if (rowRelX >= btnW + 100.0F && rowRelX < btnW + 100.0F + btnW)
            pushChange(cur + step);
        return true;
    }
    case config::ParameterType::String:
    {
        // enum-like cycle when domains is populated; otherwise no-op.
        if (def->domains.empty()) return true;
        std::string cur = std::get<std::string>(current);
        std::size_t idx = 0;
        for (; idx < def->domains.size(); ++idx) if (def->domains[idx] == cur) break;
        idx = (idx + 1U) % def->domains.size();
        pushChange(def->domains[idx]);
        return true;
    }
    case config::ParameterType::ColorRgb:
    {
        config::ColorRgb cur{};
        if (const auto* v = std::get_if<config::ColorRgb>(&current)) cur = *v;
        // Three buttons per channel (-R/+R/-G/+G/-B/+B). Each occupies 28 px.
        const float chW = btnW * 2.0F + 8.0F;
        const float rChL = 0.0F;
        const float gChL = chW + 8.0F;
        const float bChL = 2.0F * (chW + 8.0F);
        auto stepCh = [&](int v, bool inc) { return std::clamp(v + (inc ? 16 : -16), 0, 255); };
        if (rowRelX >= rChL && rowRelX < rChL + btnW) { cur.r = stepCh(cur.r, false); pushChange(cur); return true; }
        if (rowRelX >= rChL + btnW && rowRelX < rChL + chW) { cur.r = stepCh(cur.r, true); pushChange(cur); return true; }
        if (rowRelX >= gChL && rowRelX < gChL + btnW) { cur.g = stepCh(cur.g, false); pushChange(cur); return true; }
        if (rowRelX >= gChL + btnW && rowRelX < gChL + chW) { cur.g = stepCh(cur.g, true); pushChange(cur); return true; }
        if (rowRelX >= bChL && rowRelX < bChL + btnW) { cur.b = stepCh(cur.b, false); pushChange(cur); return true; }
        if (rowRelX >= bChL + btnW && rowRelX < bChL + chW) { cur.b = stepCh(cur.b, true); pushChange(cur); return true; }
        return true;
    }
    }
    return true;
}

void UiPreferencesPanel::draw(sf::RenderTarget& target,
                                  const config::ParameterRegistry& registry,
                                  const PreferencesState& state) const
{
    if (!state.open) return;
    const Layout L = computeLayout(target.getSize());

    // -------- window --------
    sf::RectangleShape bg({L.windowW, L.windowH});
    bg.setPosition(L.windowX, L.windowY);
    bg.setFillColor(kBg);
    bg.setOutlineColor(kBorder);
    bg.setOutlineThickness(1.4F);
    target.draw(bg);

    // -------- left tab column --------
    sf::RectangleShape tabCol({L.tabColW, L.windowH});
    tabCol.setPosition(L.windowX, L.windowY);
    tabCol.setFillColor(kBgTabCol);
    target.draw(tabCol);
    drawText(target, font_, "Preferencias", L.windowX + 14.0F, L.windowY + 12.0F, 16U, kAccent);

    for (int i = 0; i < static_cast<int>(config::PrefsTab::Count); ++i)
    {
        const float y = L.windowY + L.headerH + static_cast<float>(i) * 36.0F;
        const bool active = (i == state.activeTab);
        if (active)
        {
            sf::RectangleShape hi({L.tabColW, 36.0F});
            hi.setPosition(L.windowX, y);
            hi.setFillColor(kBgTabActive);
            target.draw(hi);
        }
        drawText(target, font_, config::prefsTabLabel(static_cast<config::PrefsTab>(i)),
                   L.windowX + 14.0F, y + 9.0F, 14U,
                   active ? sf::Color::White : kTextLight);
    }

    // -------- header --------
    const float contentX = L.windowX + L.tabColW;
    const float contentW = L.windowW - L.tabColW;
    drawText(target, font_, config::prefsTabLabel(static_cast<config::PrefsTab>(state.activeTab)),
               contentX + L.gutterX, L.windowY + 12.0F, 18U, sf::Color::White);
    std::ostringstream info;
    info << "  modificados=" << state.pendingValues.size()
            << "  aplicados=" << state.appliedCount;
    drawText(target, font_, info.str(), contentX + L.gutterX + 280.0F, L.windowY + 16.0F, 12U, kTextDim);

    // -------- parameter list --------
    const auto names = prefsParametersForTab(registry,
        static_cast<config::PrefsTab>(state.activeTab), state.searchQuery);
    const float listY0 = L.windowY + L.headerH;
    const float listH = L.windowH - L.headerH - L.footerH;
    const std::size_t visibleRows = static_cast<std::size_t>(listH / L.rowH);
    const std::size_t rowsToDraw = std::min(visibleRows, names.size());

    for (std::size_t i = 0; i < rowsToDraw; ++i)
    {
        const auto& name = names[i];
        const auto* def = registry.find(name);
        if (def == nullptr) continue;

        const float y = listY0 + static_cast<float>(i) * L.rowH;
        const bool dirty = state.pendingValues.count(name) > 0U;
        const bool pending = (def->applyFlags & config::ApplyFlag::PendingFuturePhase) != 0U;
        sf::RectangleShape row({contentW, L.rowH - 1.0F});
        row.setPosition(contentX, y);
        row.setFillColor(pending ? kBgRowPending : (dirty ? kBgRowDirty : (i % 2U == 0U ? kBgRow : kBgRowAlt)));
        target.draw(row);

        // name + tooltip-ish description on second line at low opacity
        drawText(target, font_, name, contentX + L.gutterX, y + 6.0F, 13U, kTextLight);
        drawText(target, font_, prefsApplyFlagsLabel(def->applyFlags),
                   contentX + L.gutterX + 200.0F, y + 6.0F, 11U, kTextDim);

        // value column on the right
        const float valueX = contentX + L.gutterX + 320.0F;
        const auto effective = prefsEffectiveValue(registry, state, name);
        switch (def->type)
        {
        case config::ParameterType::Boolean:
        {
            const bool v = std::get<bool>(effective);
            sf::RectangleShape pill({40.0F, L.rowH - 12.0F});
            pill.setPosition(valueX, y + 6.0F);
            pill.setFillColor(v ? kAccent : kBgButton);
            target.draw(pill);
            drawText(target, font_, v ? "on" : "off", valueX + 10.0F, y + 8.0F, 12U,
                       v ? sf::Color::Black : kTextLight);
            break;
        }
        case config::ParameterType::Integer:
        case config::ParameterType::Floating:
        {
            sf::RectangleShape minus({28.0F, L.rowH - 12.0F});
            minus.setPosition(valueX, y + 6.0F);
            minus.setFillColor(kBgButton);
            target.draw(minus);
            drawText(target, font_, "-", valueX + 10.0F, y + 8.0F, 14U, kTextLight);
            const float labelW = def->type == config::ParameterType::Integer ? 70.0F : 100.0F;
            drawText(target, font_, fmtValue(effective), valueX + 36.0F, y + 8.0F, 12U, kTextLight);
            sf::RectangleShape plus({28.0F, L.rowH - 12.0F});
            plus.setPosition(valueX + 28.0F + labelW, y + 6.0F);
            plus.setFillColor(kBgButton);
            target.draw(plus);
            drawText(target, font_, "+", valueX + 28.0F + labelW + 10.0F, y + 8.0F, 14U, kTextLight);
            break;
        }
        case config::ParameterType::String:
        {
            const std::string s = std::get<std::string>(effective);
            if (def->domains.empty())
            {
                drawText(target, font_, s, valueX, y + 8.0F, 12U, kTextDim);
            }
            else
            {
                drawText(target, font_, "[ " + s + " ]", valueX, y + 8.0F, 12U, kTextLight);
            }
            break;
        }
        case config::ParameterType::ColorRgb:
        {
            const auto c = std::get<config::ColorRgb>(effective);
            sf::RectangleShape sw({36.0F, L.rowH - 12.0F});
            sw.setPosition(valueX, y + 6.0F);
            sw.setFillColor(sf::Color(
                static_cast<sf::Uint8>(std::clamp(c.r, 0, 255)),
                static_cast<sf::Uint8>(std::clamp(c.g, 0, 255)),
                static_cast<sf::Uint8>(std::clamp(c.b, 0, 255))));
            sw.setOutlineThickness(1.0F);
            sw.setOutlineColor(kBorder);
            target.draw(sw);
            std::ostringstream s;
            s << " R" << c.r << " G" << c.g << " B" << c.b;
            drawText(target, font_, s.str(), valueX + 42.0F, y + 8.0F, 12U, kTextLight);
            break;
        }
        }
    }

    if (names.empty())
    {
        drawText(target, font_, "Nenhum parametro nessa aba (ainda).",
                   contentX + L.gutterX, listY0 + 12.0F, 13U, kTextDim);
    }
    else if (names.size() > rowsToDraw)
    {
        std::ostringstream s;
        s << "[ " << names.size() - rowsToDraw << " parametros adicionais nao exibidos (scroll TODO) ]";
        drawText(target, font_, s.str(), contentX + L.gutterX,
                   listY0 + static_cast<float>(rowsToDraw) * L.rowH + 4.0F, 11U, kTextDim);
    }

    // -------- footer (Apply / Revert / Defaults / Close) --------
    const float footerY = L.windowY + L.windowH - L.footerH;
    sf::RectangleShape footer({contentW, L.footerH});
    footer.setPosition(contentX, footerY);
    footer.setFillColor(kBgTabCol);
    target.draw(footer);

    const float btnW = 120.0F;
    const float btnH = 30.0F;
    const float btnY = footerY + (L.footerH - btnH) * 0.5F;
    const float gap = 10.0F;
    const float contentRight = contentX + contentW;
    struct Btn { const char* label; sf::Color color; };
    const std::array<Btn, 4> btns{{
        {"Aplicar",   sf::Color(80, 150, 90)},
        {"Reverter",  sf::Color(150, 90, 60)},
        {"Defaults",  sf::Color(80, 90, 130)},
        {"Fechar",    sf::Color(50, 60, 75)},
    }};
    for (std::size_t i = 0; i < btns.size(); ++i)
    {
        const float x = contentRight - static_cast<float>(btns.size() - i) * (btnW + gap);
        sf::RectangleShape btn({btnW, btnH});
        btn.setPosition(x, btnY);
        btn.setFillColor(btns[i].color);
        target.draw(btn);
        drawText(target, font_, btns[i].label, x + 20.0F, btnY + 7.0F, 13U, sf::Color::White);
    }

    // Hint for keyboard.
    drawText(target, font_, "Esc fecha. Atalho: Preferencias > Abrir painel placeholder.",
               contentX + L.gutterX, btnY + 8.0F, 11U, kTextDim);
}
} // namespace agentbiosim::ui
