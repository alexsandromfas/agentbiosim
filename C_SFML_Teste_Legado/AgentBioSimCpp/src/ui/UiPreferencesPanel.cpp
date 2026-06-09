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
const sf::Color kBg(20, 22, 30, 246);
const sf::Color kBgHeader(34, 40, 52);
const sf::Color kBgRow(28, 32, 40);
const sf::Color kBgRowAlt(24, 28, 36);
const sf::Color kBgRowDirty(60, 90, 130);
const sf::Color kBgRowPending(80, 50, 28);
const sf::Color kTextLight(220, 224, 232);
const sf::Color kTextDim(120, 128, 138);
const sf::Color kAccent(130, 200, 250);
const sf::Color kBgButton(40, 44, 54);
const sf::Color kBorder(70, 130, 200, 220);
const sf::Color kCloseButton(170, 70, 70);
const sf::Color kPopupBg(20, 22, 30, 250);
const sf::Color kScrollbarTrack(40, 44, 56);
const sf::Color kScrollbarThumb(90, 130, 180);
const sf::Color kEditingBg(30, 50, 70);

constexpr float kRowH = 30.0F;
constexpr float kHeaderH = 32.0F;
constexpr float kFooterH = 46.0F;
constexpr float kScrollbarW = 8.0F;
constexpr float kWindowW = 520.0F;
constexpr float kWindowH = 480.0F;
constexpr float kCloseBtnSize = 22.0F;

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

// Phase 23.2: rounded-corner panel. SFML lacks native rounded rects so we
// approximate with the main rectangle + four small circles. Simple but works.
void drawRoundedRect(sf::RenderTarget& target, float x, float y, float w, float h,
                       float radius, sf::Color fill, sf::Color outline = sf::Color::Transparent,
                       float outlineThickness = 0.0F)
{
    radius = std::clamp(radius, 0.0F, std::min(w, h) * 0.5F);
    sf::RectangleShape body({w - 2.0F * radius, h});
    body.setPosition(x + radius, y);
    body.setFillColor(fill);
    if (outlineThickness > 0.0F)
    {
        body.setOutlineColor(outline);
        body.setOutlineThickness(outlineThickness);
    }
    target.draw(body);
    sf::RectangleShape sides({w, h - 2.0F * radius});
    sides.setPosition(x, y + radius);
    sides.setFillColor(fill);
    target.draw(sides);
    for (int i = 0; i < 4; ++i)
    {
        const float cx = (i & 1) ? x + w - radius : x + radius;
        const float cy = (i & 2) ? y + h - radius : y + radius;
        sf::CircleShape corner(radius, 24);
        corner.setOrigin(radius, radius);
        corner.setPosition(cx, cy);
        corner.setFillColor(fill);
        target.draw(corner);
    }
}

std::string fmtValue(const config::ParameterValue& v)
{
    return std::visit([](const auto& tv) -> std::string {
        using T = std::decay_t<decltype(tv)>;
        if constexpr (std::is_same_v<T, bool>) return tv ? "on" : "off";
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
} // namespace

// ---------- public helpers ----------

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
        if (config::prefsShouldHideParameter(d.name)) continue;
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
        out.push_back(d.name);
    }
    return out;
}

std::vector<std::string> prefsParametersForTabFiltered(
    const config::ParameterRegistry& registry,
    const PreferencesState& state,
    const config::PrefsTab tab)
{
    auto names = prefsParametersForTab(registry, tab, state.searchQuery);
    if (tab == config::PrefsTab::Appearance)
    {
        // Phase 25.2: the language selector is the most consequential appearance
        // setting (it relabels everything), so pin it to the top of the tab
        // regardless of registration order.
        const auto it = std::find(names.begin(), names.end(), "ui_language");
        if (it != names.end() && it != names.begin())
        {
            std::rotate(names.begin(), it, it + 1);
        }
    }
    if (tab == config::PrefsTab::Neural)
    {
        // Use the pending edit of neural_network_type if any; else the
        // currently-active value from the registry.
        std::string currentType = "mlp";
        const auto* tdef = registry.find("neural_network_type");
        if (tdef != nullptr && std::holds_alternative<std::string>(tdef->defaultValue))
        {
            currentType = std::get<std::string>(tdef->defaultValue);
        }
        const auto it = state.pendingValues.find("neural_network_type");
        if (it != state.pendingValues.end() &&
            std::holds_alternative<std::string>(it->second))
        {
            currentType = std::get<std::string>(it->second);
        }
        names.erase(std::remove_if(names.begin(), names.end(),
            [&](const std::string& n) {
                return !config::prefsShouldShowNeuralParameterFor(n, currentType);
            }), names.end());
    }
    return names;
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
        if (flags & config::ApplyFlag::RebuildBrains)     return "rebuild + reset";
        if (flags & config::ApplyFlag::RebuildPerception) return "rebuild + reset";
        return "requer reset";
    }
    if (flags & config::ApplyFlag::RefreshRenderer) return "imediato";
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

// ---------- geometry ----------

UiPreferencesPanel::WindowRect UiPreferencesPanel::windowRectForTab(
    const int tab, const sf::Vector2u viewport,
    const PreferencesState& state) const noexcept
{
    WindowRect r;
    r.w = std::min(kWindowW, static_cast<float>(viewport.x) - 40.0F);
    r.h = std::min(kWindowH, static_cast<float>(viewport.y) - 100.0F);
    const auto idx = static_cast<std::size_t>(tab);
    if (state.windowX[idx] >= 0.0F)
    {
        r.x = state.windowX[idx];
        r.y = state.windowY[idx];
    }
    else
    {
        r.x = 80.0F + static_cast<float>(tab) * 32.0F;
        r.y = 80.0F + static_cast<float>(tab) * 26.0F;
    }
    if (r.x + r.w > static_cast<float>(viewport.x)) r.x = static_cast<float>(viewport.x) - r.w - 4.0F;
    if (r.y + r.h > static_cast<float>(viewport.y)) r.y = static_cast<float>(viewport.y) - r.h - 4.0F;
    r.x = std::max(0.0F, r.x);
    r.y = std::max(0.0F, r.y);
    return r;
}

UiPreferencesPanel::WindowRect UiPreferencesPanel::helpRect(
    const sf::Vector2u viewport, const PreferencesState& state) const noexcept
{
    WindowRect r;
    r.w = std::min(460.0F, static_cast<float>(viewport.x) - 40.0F);
    r.h = std::min(480.0F, static_cast<float>(viewport.y) - 100.0F);
    if (state.helpWindowX >= 0.0F)
    {
        r.x = state.helpWindowX;
        r.y = state.helpWindowY;
    }
    else
    {
        r.x = static_cast<float>(viewport.x) - r.w - 40.0F;
        r.y = 80.0F;
    }
    if (r.x + r.w > static_cast<float>(viewport.x)) r.x = static_cast<float>(viewport.x) - r.w - 4.0F;
    if (r.y + r.h > static_cast<float>(viewport.y)) r.y = static_cast<float>(viewport.y) - r.h - 4.0F;
    r.x = std::max(0.0F, r.x);
    r.y = std::max(0.0F, r.y);
    return r;
}

UiPreferencesPanel::WindowRect UiPreferencesPanel::substrateRect(
    const sf::Vector2u viewport) const noexcept
{
    WindowRect r;
    r.w = std::min(420.0F, static_cast<float>(viewport.x) - 80.0F);
    r.h = 160.0F;
    r.x = (static_cast<float>(viewport.x) - r.w) * 0.5F;
    r.y = (static_cast<float>(viewport.y) - r.h) * 0.5F;
    return r;
}

bool UiPreferencesPanel::pointInsideAnyWindow(
    const int sx, const int sy, const sf::Vector2u viewport,
    const PreferencesState& state) const noexcept
{
    const auto in = [sx, sy](const WindowRect& r) {
        return static_cast<float>(sx) >= r.x && static_cast<float>(sx) < r.x + r.w &&
                  static_cast<float>(sy) >= r.y && static_cast<float>(sy) < r.y + r.h;
    };
    if (state.helpWindowOpen && in(helpRect(viewport, state))) return true;
    if (state.substratePlaceholderOpen && in(substrateRect(viewport))) return true;
    for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
    {
        if (state.windowOpen[static_cast<std::size_t>(t)] &&
            in(windowRectForTab(t, viewport, state)))
        {
            return true;
        }
    }
    if (!state.openPopup.empty()) return true;
    return false;
}

// ---------- mouse handlers ----------

bool UiPreferencesPanel::handleMouseWheel(
    const int sx, const int sy, const sf::Vector2u viewport, const float delta,
    PreferencesState& state, CommandQueue& queue)
{
    const auto in = [sx, sy](const WindowRect& r) {
        return static_cast<float>(sx) >= r.x && static_cast<float>(sx) < r.x + r.w &&
                  static_cast<float>(sy) >= r.y && static_cast<float>(sy) < r.y + r.h;
    };
    const int step = static_cast<int>(delta * -3.0F);
    for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
    {
        if (state.windowOpen[static_cast<std::size_t>(t)] &&
            in(windowRectForTab(t, viewport, state)))
        {
            queue.push(CmdScrollPreferencesWindow{t, step});
            return true;
        }
    }
    if (state.helpWindowOpen && in(helpRect(viewport, state)))
    {
        state.helpScroll = std::max(0, state.helpScroll + step);
        return true;
    }
    if (state.substratePlaceholderOpen && in(substrateRect(viewport))) return true;
    return false;
}

void UiPreferencesPanel::handleMouseMove(
    const int sx, const int sy, const sf::Vector2u viewport,
    PreferencesState& state, CommandQueue& queue)
{
    static_cast<void>(viewport);
    if (state.draggingTab >= 0)
    {
        queue.push(CmdMovePreferencesWindow{state.draggingTab,
            static_cast<float>(sx) - state.dragOffsetX,
            static_cast<float>(sy) - state.dragOffsetY});
    }
    else if (state.draggingHelp)
    {
        queue.push(CmdMoveHelpWindow{static_cast<float>(sx) - state.dragOffsetX,
            static_cast<float>(sy) - state.dragOffsetY});
    }
}

void UiPreferencesPanel::handleMouseRelease(
    const int, const int, const sf::Vector2u,
    PreferencesState& state, CommandQueue&)
{
    state.draggingTab = -1;
    state.draggingHelp = false;
}

bool UiPreferencesPanel::handleMouseClick(
    const int sx, const int sy, const sf::Vector2u viewport,
    const config::ParameterRegistry& registry, PreferencesState& state,
    CommandQueue& queue)
{
    const float frx = static_cast<float>(sx);
    const float fry = static_cast<float>(sy);

    // -------- popup gets priority --------
    if (!state.openPopup.empty())
    {
        if (state.openPopup == "neural_combo" || state.openPopup.rfind("enum:", 0) == 0)
        {
            std::string paramName;
            if (state.openPopup == "neural_combo") paramName = "neural_network_type";
            else paramName = state.openPopup.substr(5);
            const auto values = config::prefsEnumValuesFor(paramName);
            if (values.empty()) { queue.push(CmdClosePrefsPopup{}); return true; }
            const float w = 300.0F;
            const float itemH = 30.0F;
            const float h = static_cast<float>(values.size()) * itemH;
            const float x = (static_cast<float>(viewport.x) - w) * 0.5F;
            const float y = (static_cast<float>(viewport.y) - h) * 0.5F;
            if (frx >= x && frx < x + w && fry >= y && fry < y + h)
            {
                const std::size_t idx = static_cast<std::size_t>((fry - y) / itemH);
                if (idx < values.size())
                {
                    queue.push(CmdSetParameterValue{paramName, values[idx]});
                }
            }
            queue.push(CmdClosePrefsPopup{});
            return true;
        }
        if (state.openPopup.rfind("color:", 0) == 0)
        {
            const std::string param = state.openPopup.substr(6);
            const float w = 320.0F;
            const float h = 220.0F;
            const float x = (static_cast<float>(viewport.x) - w) * 0.5F;
            const float y = (static_cast<float>(viewport.y) - h) * 0.5F;
            if (frx >= x && frx < x + w && fry >= y && fry < y + h)
            {
                const std::array<config::ColorRgb, 8> presets{{
                    {220, 220, 220}, {200, 90, 70}, {90, 180, 90}, {80, 140, 220},
                    {220, 200, 80}, {180, 90, 200}, {60, 200, 200}, {30, 32, 40}
                }};
                const float padX = x + 14.0F;
                const float padY = y + 44.0F;
                const float swSize = 32.0F;
                const float gap = 4.0F;
                for (std::size_t i = 0; i < presets.size(); ++i)
                {
                    const float sxi = padX + static_cast<float>(i) * (swSize + gap);
                    if (frx >= sxi && frx < sxi + swSize && fry >= padY && fry < padY + swSize)
                    {
                        queue.push(CmdSetParameterValue{param, presets[i]});
                        queue.push(CmdClosePrefsPopup{});
                        return true;
                    }
                }
                config::ColorRgb cur{};
                const auto eff = prefsEffectiveValue(registry, state, param);
                if (const auto* v = std::get_if<config::ColorRgb>(&eff)) cur = *v;
                const float btnW = 28.0F;
                const float btnH = 26.0F;
                const float btnsY = y + 100.0F;
                auto bx = [&](int ch, int side) {
                    return padX + static_cast<float>(ch) * (btnW * 2.0F + 36.0F) +
                                 static_cast<float>(side) * btnW;
                };
                if (fry >= btnsY && fry < btnsY + btnH)
                {
                    auto stepCh = [](int v, bool inc) { return std::clamp(v + (inc ? 16 : -16), 0, 255); };
                    auto hitChannel = [&](int ch) -> bool {
                        const float minus = bx(ch, 0);
                        const float plus = bx(ch, 1);
                        if (frx >= minus && frx < minus + btnW)
                        {
                            if (ch == 0) cur.r = stepCh(cur.r, false);
                            if (ch == 1) cur.g = stepCh(cur.g, false);
                            if (ch == 2) cur.b = stepCh(cur.b, false);
                            return true;
                        }
                        if (frx >= plus && frx < plus + btnW)
                        {
                            if (ch == 0) cur.r = stepCh(cur.r, true);
                            if (ch == 1) cur.g = stepCh(cur.g, true);
                            if (ch == 2) cur.b = stepCh(cur.b, true);
                            return true;
                        }
                        return false;
                    };
                    if (hitChannel(0) || hitChannel(1) || hitChannel(2))
                    {
                        queue.push(CmdSetParameterValue{param, cur});
                        return true;
                    }
                }
                const float closeX = x + w - 28.0F;
                if (frx >= closeX && fry >= y + 5.0F && fry < y + 27.0F)
                {
                    queue.push(CmdClosePrefsPopup{});
                    return true;
                }
                return true;
            }
            queue.push(CmdClosePrefsPopup{});
            return true;
        }
        queue.push(CmdClosePrefsPopup{});
        return true;
    }

    // -------- help window --------
    if (state.helpWindowOpen)
    {
        const WindowRect r = helpRect(viewport, state);
        if (frx >= r.x && frx < r.x + r.w && fry >= r.y && fry < r.y + r.h)
        {
            const float closeX = r.x + r.w - 28.0F;
            if (frx >= closeX && fry >= r.y + 5.0F && fry < r.y + 27.0F)
            {
                queue.push(CmdCloseHelpWindow{});
                return true;
            }
            // Drag from header strip (excluding the close button).
            if (fry < r.y + kHeaderH && frx < r.x + r.w - 32.0F)
            {
                state.draggingHelp = true;
                state.dragOffsetX = frx - r.x;
                state.dragOffsetY = fry - r.y;
            }
            return true;
        }
    }

    // -------- substrate placeholder --------
    if (state.substratePlaceholderOpen)
    {
        const WindowRect r = substrateRect(viewport);
        if (frx >= r.x && frx < r.x + r.w && fry >= r.y && fry < r.y + r.h)
        {
            queue.push(CmdCloseSubstratePlaceholder{});
            return true;
        }
    }

    // -------- per-tab windows (front-most first) --------
    for (int t = static_cast<int>(config::PrefsTab::Count) - 1; t >= 0; --t)
    {
        if (!state.windowOpen[static_cast<std::size_t>(t)]) continue;
        const WindowRect r = windowRectForTab(t, viewport, state);
        const float rx = frx - r.x;
        const float ry = fry - r.y;
        if (rx < 0.0F || ry < 0.0F || rx > r.w || ry > r.h) continue;

        // Close button at top-right.
        if (rx >= r.w - 28.0F && ry >= 5.0F && ry < 27.0F)
        {
            queue.push(CmdClosePreferencesWindow{t});
            return true;
        }

        // Drag from header strip (excluding the close button).
        if (ry < kHeaderH && rx < r.w - 32.0F)
        {
            state.draggingTab = t;
            state.dragOffsetX = rx;
            state.dragOffsetY = ry;
            return true;
        }

        // Footer buttons (Aplicar / Reverter / Restaurar / Fechar).
        if (ry > r.h - kFooterH)
        {
            const float btnW = 110.0F;
            const float btnH = 30.0F;
            const float gap = 8.0F;
            const float btnY = r.h - kFooterH + (kFooterH - btnH) * 0.5F;
            const float fecharX = r.w - btnW - gap;
            const float restaurarX = fecharX - btnW - gap;
            const float reverterX = restaurarX - btnW - gap;
            const float aplicarX = reverterX - btnW - gap;
            if (ry >= btnY && ry < btnY + btnH)
            {
                if (rx >= aplicarX && rx < aplicarX + btnW)
                {
                    queue.push(CmdApplyPreferences{});
                    return true;
                }
                if (rx >= reverterX && rx < reverterX + btnW)
                {
                    queue.push(CmdRevertPreferences{});
                    return true;
                }
                if (rx >= restaurarX && rx < restaurarX + btnW)
                {
                    queue.push(CmdRestoreDefaultsAndApply{});
                    return true;
                }
                if (rx >= fecharX && rx < fecharX + btnW)
                {
                    queue.push(CmdClosePreferencesWindow{t});
                    return true;
                }
            }
            return true;
        }

        // Parameter row click.
        const float listY0 = kHeaderH;
        if (ry < listY0) return true;
        const auto names = prefsParametersForTabFiltered(registry, state,
            static_cast<config::PrefsTab>(t));
        const int scrollRows = state.windowScroll[static_cast<std::size_t>(t)];
        const int rowIdx = static_cast<int>((ry - listY0) / kRowH) + scrollRows;
        if (rowIdx < 0 || rowIdx >= static_cast<int>(names.size())) return true;
        const std::string& name = names[static_cast<std::size_t>(rowIdx)];
        const auto* def = registry.find(name);
        if (def == nullptr) return true;
        const auto current = prefsEffectiveValue(registry, state, name);

        const float valueX = r.w - 220.0F;
        if (rx < valueX) return true;

        auto pushChange = [&](const config::ParameterValue& v) {
            queue.push(CmdSetParameterValue{name, v});
        };

        switch (def->type)
        {
        case config::ParameterType::Boolean:
        {
            const bool cur = std::get<bool>(current);
            pushChange(!cur);
            return true;
        }
        case config::ParameterType::Integer:
        case config::ParameterType::Floating:
        {
            // Phase 23.2: clicking the value cell starts inline editing.
            const std::string init = fmtValue(current);
            queue.push(CmdBeginEditParameter{name, init});
            return true;
        }
        case config::ParameterType::String:
        {
            // Phase 23.2: combo for any param with a known enum list. The
            // registry's `domains` field stores tags, not enum values, so
            // we must use prefsEnumValuesFor here.
            const auto values = config::prefsEnumValuesFor(name);
            if (values.empty()) return true;
            if (name == "neural_network_type")
            {
                queue.push(CmdOpenPrefsPopup{"neural_combo"});
            }
            else
            {
                queue.push(CmdOpenPrefsPopup{"enum:" + name});
            }
            return true;
        }
        case config::ParameterType::ColorRgb:
        {
            queue.push(CmdOpenPrefsPopup{"color:" + name});
            return true;
        }
        }
        return true;
    }
    return false;
}

// ---------- drawing ----------

namespace
{
void drawWindowHeader(sf::RenderTarget& target, const sf::Font* font,
                        float x, float y, float w, const char* title)
{
    sf::RectangleShape hd({w, kHeaderH});
    hd.setPosition(x, y);
    hd.setFillColor(kBgHeader);
    target.draw(hd);
    drawText(target, font, title, x + 14.0F, y + 7.0F, 14U, kAccent);
    // Phase 23.2: close button as a rounded square with the X glyph centered.
    constexpr float pad = 5.0F;
    const float bx = x + w - kCloseBtnSize - pad;
    const float by = y + (kHeaderH - kCloseBtnSize) * 0.5F;
    drawRoundedRect(target, bx, by, kCloseBtnSize, kCloseBtnSize, 4.0F, kCloseButton);
    // Center the "x" glyph. Glyph offset varies per font; we use empirically
    // chosen offsets that look centered with segoeui/arial at size 14.
    drawText(target, font, "x", bx + 6.0F, by + 1.0F, 14U, sf::Color::White);
}

void drawScrollbar(sf::RenderTarget& target, float x, float y, float h,
                     int rowCount, int visibleRows, int scrollOffset)
{
    if (rowCount <= visibleRows) return;
    sf::RectangleShape track({kScrollbarW, h});
    track.setPosition(x, y);
    track.setFillColor(kScrollbarTrack);
    target.draw(track);
    const float ratioVisible = static_cast<float>(visibleRows) / static_cast<float>(rowCount);
    const float thumbH = std::max(20.0F, h * ratioVisible);
    const float ratioPos = static_cast<float>(scrollOffset) /
                            static_cast<float>(std::max(1, rowCount - visibleRows));
    const float thumbY = y + (h - thumbH) * ratioPos;
    sf::RectangleShape thumb({kScrollbarW, thumbH});
    thumb.setPosition(x, thumbY);
    thumb.setFillColor(kScrollbarThumb);
    target.draw(thumb);
}
} // namespace

void UiPreferencesPanel::draw(sf::RenderTarget& target,
                                  const config::ParameterRegistry& registry,
                                  const PreferencesState& state) const
{
    const sf::Vector2u vp = target.getSize();

    for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
    {
        if (!state.windowOpen[static_cast<std::size_t>(t)]) continue;
        const WindowRect r = windowRectForTab(t, vp, state);
        drawRoundedRect(target, r.x, r.y, r.w, r.h, 8.0F, kBg, kBorder, 1.4F);
        drawWindowHeader(target, font_, r.x, r.y, r.w,
            config::prefsTabLabel(static_cast<config::PrefsTab>(t)));

        const auto names = prefsParametersForTabFiltered(registry, state,
            static_cast<config::PrefsTab>(t));
        const float listH = r.h - kHeaderH - kFooterH;
        const int visibleRows = static_cast<int>(listH / kRowH);
        const int scrollOffset = state.windowScroll[static_cast<std::size_t>(t)];
        const int firstRow = std::max(0,
            std::min(scrollOffset, static_cast<int>(names.size()) - visibleRows));

        for (int i = 0; i < visibleRows; ++i)
        {
            const int rowIdx = firstRow + i;
            if (rowIdx >= static_cast<int>(names.size())) break;
            const auto& name = names[static_cast<std::size_t>(rowIdx)];
            const auto* def = registry.find(name);
            if (def == nullptr) continue;
            const float y = r.y + kHeaderH + static_cast<float>(i) * kRowH;
            const bool dirty = state.pendingValues.count(name) > 0U;
            const bool pending = (def->applyFlags & config::ApplyFlag::PendingFuturePhase) != 0U;
            sf::RectangleShape row({r.w - kScrollbarW, kRowH - 1.0F});
            row.setPosition(r.x, y);
            row.setFillColor(pending ? kBgRowPending : (dirty ? kBgRowDirty :
                (i % 2 == 0 ? kBgRow : kBgRowAlt)));
            target.draw(row);

            // Phase 23.2: friendly label only — internal name removed.
            const char* friendly = config::prefsFriendlyLabel(name);
            const std::string mainLabel = friendly != nullptr ? friendly : name;
            drawText(target, font_, mainLabel, r.x + 12.0F, y + 7.0F, 13U, kTextLight);
            drawText(target, font_, prefsApplyFlagsLabel(def->applyFlags),
                       r.x + r.w - 320.0F, y + 9.0F, 10U, kTextDim);

            const float valueX = r.x + r.w - 220.0F;
            const auto effective = prefsEffectiveValue(registry, state, name);
            const bool editingThis = state.editingParam == name;
            switch (def->type)
            {
            case config::ParameterType::Boolean:
            {
                const bool v = std::get<bool>(effective);
                drawRoundedRect(target, valueX, y + 6.0F, 50.0F, kRowH - 14.0F, 6.0F,
                                  v ? kAccent : kBgButton);
                drawText(target, font_, v ? "on" : "off", valueX + 12.0F, y + 7.0F, 12U,
                           v ? sf::Color::Black : kTextLight);
                break;
            }
            case config::ParameterType::Integer:
            case config::ParameterType::Floating:
            {
                if (editingThis)
                {
                    drawRoundedRect(target, valueX, y + 5.0F, 200.0F, kRowH - 12.0F, 5.0F,
                                      kEditingBg, kAccent, 1.6F);
                    drawText(target, font_, state.editingBuffer + "_",
                               valueX + 8.0F, y + 7.0F, 12U, kTextLight);
                }
                else
                {
                    drawRoundedRect(target, valueX, y + 5.0F, 200.0F, kRowH - 12.0F, 5.0F,
                                      kBgButton);
                    drawText(target, font_, fmtValue(effective),
                               valueX + 12.0F, y + 7.0F, 12U, kTextLight);
                    drawText(target, font_, "(clique p/ editar)",
                               valueX + 100.0F, y + 8.0F, 10U, kTextDim);
                }
                break;
            }
            case config::ParameterType::String:
            {
                const std::string s = std::get<std::string>(effective);
                const auto values = config::prefsEnumValuesFor(name);
                if (values.empty())
                {
                    drawText(target, font_, s, valueX, y + 7.0F, 12U, kTextDim);
                }
                else
                {
                    drawRoundedRect(target, valueX, y + 5.0F, 200.0F, kRowH - 12.0F, 5.0F,
                                      kBgButton);
                    drawText(target, font_, s + "  v", valueX + 12.0F, y + 7.0F, 12U, kTextLight);
                }
                break;
            }
            case config::ParameterType::ColorRgb:
            {
                const auto c = std::get<config::ColorRgb>(effective);
                drawRoundedRect(target, valueX, y + 5.0F, 40.0F, kRowH - 12.0F, 4.0F,
                                  sf::Color(
                                      static_cast<sf::Uint8>(std::clamp(c.r, 0, 255)),
                                      static_cast<sf::Uint8>(std::clamp(c.g, 0, 255)),
                                      static_cast<sf::Uint8>(std::clamp(c.b, 0, 255))),
                                  kBorder, 1.0F);
                std::ostringstream s;
                s << "R" << c.r << " G" << c.g << " B" << c.b;
                drawText(target, font_, s.str(), valueX + 50.0F, y + 8.0F, 11U, kTextLight);
                break;
            }
            }
        }

        drawScrollbar(target, r.x + r.w - kScrollbarW, r.y + kHeaderH,
                        listH, static_cast<int>(names.size()), visibleRows, firstRow);

        // Footer with Portuguese button labels.
        const float footerY = r.y + r.h - kFooterH;
        sf::RectangleShape footer({r.w, kFooterH});
        footer.setPosition(r.x, footerY);
        footer.setFillColor(kBgHeader);
        target.draw(footer);
        const float btnW = 110.0F;
        const float btnH = 30.0F;
        const float gap = 8.0F;
        const float btnY = footerY + (kFooterH - btnH) * 0.5F;
        const float contentRight = r.x + r.w;
        struct Btn { const char* label; sf::Color color; };
        const std::array<Btn, 4> btns{{
            {"Aplicar",          sf::Color(80, 150, 90)},
            {"Reverter",         sf::Color(150, 90, 60)},
            {"Restaurar padroes",sf::Color(80, 90, 130)},
            {"Fechar",           sf::Color(50, 60, 75)},
        }};
        for (std::size_t i = 0; i < btns.size(); ++i)
        {
            const float x = contentRight - static_cast<float>(btns.size() - i) * (btnW + gap);
            drawRoundedRect(target, x, btnY, btnW, btnH, 8.0F, btns[i].color);
            drawText(target, font_, btns[i].label, x + 10.0F, btnY + 7.0F, 12U, sf::Color::White);
        }
    }

    // -------- Help window --------
    if (state.helpWindowOpen)
    {
        const WindowRect r = helpRect(vp, state);
        drawRoundedRect(target, r.x, r.y, r.w, r.h, 8.0F, kBg, kBorder, 1.4F);
        drawWindowHeader(target, font_, r.x, r.y, r.w, "Ajuda e atalhos");
        const std::array<const char*, 24> lines{{
            "Atalhos do canvas",
            "",
            "Space          Play / Pause",
            "Esc            Limpar selecao / fechar janelas",
            "Delete         Matar selecionados",
            "R              Reset simulacao",
            "F              Fit world",
            "T              Toggle render simples",
            "V              Toggle visao debug",
            "H              Abrir/fechar esta janela",
            "S / Q / L      Tool: Selecao / Rect / Laco",
            "G / A          Tool: Comida / Agente",
            "B / X          Tool: Pincel / Apagar",
            "M / D          Tool: Mover / Excluir",
            "WASD / Setas   Pan camera",
            "Scroll         Zoom no canvas / Scroll na janela",
            "Right drag     Pan camera",
            "Shift/Ctrl     Selecao aditiva",
            "",
            "Janelas",
            "Arraste pelo titulo para mover a janela.",
            "Clique no valor numerico para editar.",
            "Combos abrem dropdown ao clicar.",
            "Restaurar padroes aplica imediatamente."
        }};
        const int visible = static_cast<int>((r.h - kHeaderH - 10.0F) / 18.0F);
        const int firstRow = std::clamp(state.helpScroll, 0,
            std::max(0, static_cast<int>(lines.size()) - visible));
        for (int i = 0; i < visible && firstRow + i < static_cast<int>(lines.size()); ++i)
        {
            const auto* l = lines[static_cast<std::size_t>(firstRow + i)];
            drawText(target, font_, l, r.x + 14.0F,
                       r.y + kHeaderH + 8.0F + static_cast<float>(i) * 18.0F, 12U,
                       i == 0 ? kAccent : kTextLight);
        }
        drawScrollbar(target, r.x + r.w - kScrollbarW - 4.0F, r.y + kHeaderH,
                        r.h - kHeaderH, static_cast<int>(lines.size()), visible, firstRow);
    }

    // -------- Substrate placeholder --------
    if (state.substratePlaceholderOpen)
    {
        const WindowRect r = substrateRect(vp);
        drawRoundedRect(target, r.x, r.y, r.w, r.h, 8.0F, kBg, kBorder, 1.4F);
        drawWindowHeader(target, font_, r.x, r.y, r.w, "Substrato / Mundo");
        drawText(target, font_, "Configuracao de tipo de substrato vai aqui.",
                   r.x + 14.0F, r.y + 44.0F, 13U, kTextLight);
        drawText(target, font_, "O painel completo entra na Fase 24.",
                   r.x + 14.0F, r.y + 66.0F, 12U, kTextDim);
        drawText(target, font_, "(clique em qualquer lugar para fechar)",
                   r.x + 14.0F, r.y + 100.0F, 11U, kTextDim);
    }

    // -------- Popups (last, on top) --------
    if (state.openPopup == "neural_combo" || state.openPopup.rfind("enum:", 0) == 0)
    {
        std::string paramName;
        if (state.openPopup == "neural_combo") paramName = "neural_network_type";
        else paramName = state.openPopup.substr(5);
        const auto values = config::prefsEnumValuesFor(paramName);
        if (!values.empty())
        {
            const float w = 300.0F;
            const float itemH = 30.0F;
            const float h = static_cast<float>(values.size()) * itemH;
            const float x = (static_cast<float>(vp.x) - w) * 0.5F;
            const float y = (static_cast<float>(vp.y) - h) * 0.5F;
            drawRoundedRect(target, x, y, w, h, 8.0F, kPopupBg, kBorder, 1.4F);
            const std::string cur = std::get<std::string>(
                prefsEffectiveValue(registry, state, paramName));
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                const float ry = y + static_cast<float>(i) * itemH;
                const bool isCur = values[i] == cur;
                if (isCur)
                {
                    sf::RectangleShape hi({w, itemH});
                    hi.setPosition(x, ry);
                    hi.setFillColor(kBgRowDirty);
                    target.draw(hi);
                }
                drawText(target, font_, values[i], x + 16.0F, ry + 7.0F, 13U,
                           isCur ? sf::Color::White : kTextLight);
            }
        }
    }
    else if (state.openPopup.rfind("color:", 0) == 0)
    {
        const std::string param = state.openPopup.substr(6);
        const float w = 320.0F;
        const float h = 220.0F;
        const float x = (static_cast<float>(vp.x) - w) * 0.5F;
        const float y = (static_cast<float>(vp.y) - h) * 0.5F;
        drawRoundedRect(target, x, y, w, h, 8.0F, kPopupBg, kBorder, 1.4F);
        drawWindowHeader(target, font_, x, y, w, "Selecionar cor");

        const std::array<config::ColorRgb, 8> presets{{
            {220, 220, 220}, {200, 90, 70}, {90, 180, 90}, {80, 140, 220},
            {220, 200, 80}, {180, 90, 200}, {60, 200, 200}, {30, 32, 40}
        }};
        const float padX = x + 14.0F;
        const float padY = y + 44.0F;
        const float swSize = 32.0F;
        const float gap = 4.0F;
        for (std::size_t i = 0; i < presets.size(); ++i)
        {
            const float sxi = padX + static_cast<float>(i) * (swSize + gap);
            drawRoundedRect(target, sxi, padY, swSize, swSize, 4.0F,
                              sf::Color(static_cast<sf::Uint8>(presets[i].r),
                                          static_cast<sf::Uint8>(presets[i].g),
                                          static_cast<sf::Uint8>(presets[i].b)),
                              kBorder, 1.0F);
        }

        const float btnW = 28.0F;
        const float btnH = 26.0F;
        const float btnsY = y + 100.0F;
        config::ColorRgb cur{};
        const auto eff = prefsEffectiveValue(registry, state, param);
        if (const auto* v = std::get_if<config::ColorRgb>(&eff)) cur = *v;
        const std::array<const char*, 3> chLbl{{"R", "G", "B"}};
        for (int ch = 0; ch < 3; ++ch)
        {
            const float chx = padX + static_cast<float>(ch) * (btnW * 2.0F + 36.0F);
            drawRoundedRect(target, chx, btnsY, btnW, btnH, 4.0F, kBgButton);
            drawText(target, font_, "-", chx + 10.0F, btnsY + 4.0F, 14U, kTextLight);
            drawRoundedRect(target, chx + btnW, btnsY, btnW, btnH, 4.0F, kBgButton);
            drawText(target, font_, "+", chx + btnW + 10.0F, btnsY + 4.0F, 14U, kTextLight);
            int val = ch == 0 ? cur.r : (ch == 1 ? cur.g : cur.b);
            std::ostringstream lbl;
            lbl << chLbl[static_cast<std::size_t>(ch)] << " " << val;
            drawText(target, font_, lbl.str(), chx + btnW * 2.0F + 6.0F, btnsY + 4.0F, 12U, kTextLight);
        }

        drawRoundedRect(target, x + w - 80.0F, y + h - 56.0F, 60.0F, 36.0F, 6.0F,
                          sf::Color(
                              static_cast<sf::Uint8>(std::clamp(cur.r, 0, 255)),
                              static_cast<sf::Uint8>(std::clamp(cur.g, 0, 255)),
                              static_cast<sf::Uint8>(std::clamp(cur.b, 0, 255))),
                          kBorder, 1.0F);
        drawText(target, font_, "preview", x + 14.0F, y + h - 32.0F, 11U, kTextDim);
    }
}
} // namespace agentbiosim::ui
