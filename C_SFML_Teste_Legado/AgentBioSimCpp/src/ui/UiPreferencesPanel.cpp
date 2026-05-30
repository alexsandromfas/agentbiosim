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
const sf::Color kBg(18, 20, 26, 244);
const sf::Color kBgHeader(28, 32, 42);
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
const sf::Color kCloseButton(170, 70, 70);
const sf::Color kCloseButtonHot(210, 90, 90);
const sf::Color kPopupBg(20, 22, 30, 248);
const sf::Color kScrollbarTrack(40, 44, 56);
const sf::Color kScrollbarThumb(90, 130, 180);

constexpr float kRowH = 32.0F;
constexpr float kHeaderH = 30.0F;
constexpr float kFooterH = 44.0F;
constexpr float kScrollbarW = 8.0F;
constexpr float kWindowW = 480.0F;
constexpr float kWindowH = 460.0F;

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

// ---------- helpers (also used by Phase23/23.1 selftests) ----------

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
        if (flags & config::ApplyFlag::RebuildBrains)     return "rebuild brains + reset";
        if (flags & config::ApplyFlag::RebuildPerception) return "rebuild visao + reset";
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

// ---------- window geometry ----------

UiPreferencesPanel::WindowRect UiPreferencesPanel::windowRectForTab(
    const int tab, const sf::Vector2u viewport) const noexcept
{
    // Cascade: each window is offset by 28 px so they do not overlap exactly.
    WindowRect r;
    r.w = std::min(kWindowW, static_cast<float>(viewport.x) - 80.0F);
    r.h = std::min(kWindowH, static_cast<float>(viewport.y) - 140.0F);
    r.x = 80.0F + static_cast<float>(tab) * 28.0F;
    r.y = 80.0F + static_cast<float>(tab) * 28.0F;
    if (r.x + r.w > static_cast<float>(viewport.x)) r.x = static_cast<float>(viewport.x) - r.w - 8.0F;
    if (r.y + r.h > static_cast<float>(viewport.y)) r.y = static_cast<float>(viewport.y) - r.h - 8.0F;
    return r;
}

UiPreferencesPanel::WindowRect UiPreferencesPanel::helpRect(
    const sf::Vector2u viewport) const noexcept
{
    WindowRect r;
    r.w = std::min(440.0F, static_cast<float>(viewport.x) - 80.0F);
    r.h = std::min(480.0F, static_cast<float>(viewport.y) - 140.0F);
    r.x = static_cast<float>(viewport.x) - r.w - 40.0F;
    r.y = 80.0F;
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
    if (state.helpWindowOpen && in(helpRect(viewport))) return true;
    if (state.substratePlaceholderOpen && in(substrateRect(viewport))) return true;
    for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
    {
        if (state.windowOpen[static_cast<std::size_t>(t)] && in(windowRectForTab(t, viewport)))
        {
            return true;
        }
    }
    // Popups always overlay everything when open.
    if (!state.openPopup.empty()) return true;
    return false;
}

// ---------- mouse wheel ----------

bool UiPreferencesPanel::handleMouseWheel(
    const int sx, const int sy, const sf::Vector2u viewport, const float delta,
    PreferencesState& state, CommandQueue& queue)
{
    const auto in = [sx, sy](const WindowRect& r) {
        return static_cast<float>(sx) >= r.x && static_cast<float>(sx) < r.x + r.w &&
                  static_cast<float>(sy) >= r.y && static_cast<float>(sy) < r.y + r.h;
    };
    // 1 wheel notch = 3 rows.
    const int step = static_cast<int>(delta * -3.0F);
    for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
    {
        if (state.windowOpen[static_cast<std::size_t>(t)] && in(windowRectForTab(t, viewport)))
        {
            queue.push(CmdScrollPreferencesWindow{t, step});
            return true;
        }
    }
    if (state.helpWindowOpen && in(helpRect(viewport)))
    {
        state.helpScroll = std::max(0, state.helpScroll + step);
        return true;
    }
    if (state.substratePlaceholderOpen && in(substrateRect(viewport))) return true;
    return false;
}

// ---------- click handling ----------

bool UiPreferencesPanel::handleMouseClick(
    const int sx, const int sy, const sf::Vector2u viewport,
    const config::ParameterRegistry& registry, PreferencesState& state,
    CommandQueue& queue)
{
    // -------- popup gets priority --------
    if (!state.openPopup.empty())
    {
        // Neural combo: a vertical list under the row that triggered it.
        if (state.openPopup == "neural_combo")
        {
            const auto* def = registry.find("neural_network_type");
            if (def == nullptr || def->domains.empty())
            {
                queue.push(CmdClosePrefsPopup{});
                return true;
            }
            const float w = 280.0F;
            const float itemH = 28.0F;
            const float h = static_cast<float>(def->domains.size()) * itemH;
            const float x = (static_cast<float>(viewport.x) - w) * 0.5F;
            const float y = (static_cast<float>(viewport.y) - h) * 0.5F;
            if (static_cast<float>(sx) >= x && static_cast<float>(sx) < x + w &&
                static_cast<float>(sy) >= y && static_cast<float>(sy) < y + h)
            {
                const std::size_t idx = static_cast<std::size_t>(
                    (static_cast<float>(sy) - y) / itemH);
                if (idx < def->domains.size())
                {
                    queue.push(CmdSetParameterValue{"neural_network_type", def->domains[idx]});
                }
            }
            queue.push(CmdClosePrefsPopup{});
            return true;
        }
        if (state.openPopup.rfind("color:", 0) == 0)
        {
            const std::string param = state.openPopup.substr(6);
            // Popup with 8 preset swatches + R/G/B steppers.
            const float w = 320.0F;
            const float h = 200.0F;
            const float x = (static_cast<float>(viewport.x) - w) * 0.5F;
            const float y = (static_cast<float>(viewport.y) - h) * 0.5F;
            if (static_cast<float>(sx) >= x && static_cast<float>(sx) < x + w &&
                static_cast<float>(sy) >= y && static_cast<float>(sy) < y + h)
            {
                const std::array<config::ColorRgb, 8> presets{{
                    {220, 220, 220}, {200, 90, 70}, {90, 180, 90}, {80, 140, 220},
                    {220, 200, 80}, {180, 90, 200}, {60, 200, 200}, {30, 32, 40}
                }};
                const float padX = x + 14.0F;
                const float padY = y + 36.0F;
                const float swSize = 32.0F;
                const float gap = 8.0F;
                for (std::size_t i = 0; i < presets.size(); ++i)
                {
                    const float sxi = padX + static_cast<float>(i) * (swSize + gap);
                    if (static_cast<float>(sx) >= sxi && static_cast<float>(sx) < sxi + swSize &&
                        static_cast<float>(sy) >= padY && static_cast<float>(sy) < padY + swSize)
                    {
                        queue.push(CmdSetParameterValue{param, presets[i]});
                        queue.push(CmdClosePrefsPopup{});
                        return true;
                    }
                }
                // Per-channel steppers
                config::ColorRgb cur{};
                const auto eff = prefsEffectiveValue(registry, state, param);
                if (const auto* v = std::get_if<config::ColorRgb>(&eff)) cur = *v;
                const float btnW = 28.0F;
                const float btnH = 26.0F;
                const float btnsY = y + 90.0F;
                const float gap2 = 6.0F;
                auto bx = [&](int ch, int side) {
                    return padX + static_cast<float>(ch) * (btnW * 2.0F + gap2 + 10.0F) +
                                 static_cast<float>(side) * btnW;
                };
                if (static_cast<float>(sy) >= btnsY && static_cast<float>(sy) < btnsY + btnH)
                {
                    auto stepCh = [](int v, bool inc) { return std::clamp(v + (inc ? 16 : -16), 0, 255); };
                    auto hitChannel = [&](int ch) -> bool {
                        const float minus = bx(ch, 0);
                        const float plus = bx(ch, 1);
                        if (static_cast<float>(sx) >= minus && static_cast<float>(sx) < minus + btnW)
                        {
                            if (ch == 0) cur.r = stepCh(cur.r, false);
                            if (ch == 1) cur.g = stepCh(cur.g, false);
                            if (ch == 2) cur.b = stepCh(cur.b, false);
                            return true;
                        }
                        if (static_cast<float>(sx) >= plus && static_cast<float>(sx) < plus + btnW)
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
                // Close button at top-right of the popup.
                const float closeX = x + w - 24.0F;
                if (static_cast<float>(sx) >= closeX && static_cast<float>(sy) >= y + 6.0F &&
                    static_cast<float>(sy) < y + 28.0F)
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
        const WindowRect r = helpRect(viewport);
        if (static_cast<float>(sx) >= r.x && static_cast<float>(sx) < r.x + r.w &&
            static_cast<float>(sy) >= r.y && static_cast<float>(sy) < r.y + r.h)
        {
            const float closeX = r.x + r.w - 24.0F;
            if (static_cast<float>(sx) >= closeX && static_cast<float>(sy) >= r.y + 6.0F &&
                static_cast<float>(sy) < r.y + 28.0F)
            {
                queue.push(CmdCloseHelpWindow{});
            }
            return true;
        }
    }

    // -------- substrate placeholder --------
    if (state.substratePlaceholderOpen)
    {
        const WindowRect r = substrateRect(viewport);
        if (static_cast<float>(sx) >= r.x && static_cast<float>(sx) < r.x + r.w &&
            static_cast<float>(sy) >= r.y && static_cast<float>(sy) < r.y + r.h)
        {
            queue.push(CmdCloseSubstratePlaceholder{});
            return true;
        }
    }

    // -------- per-tab windows --------
    for (int t = static_cast<int>(config::PrefsTab::Count) - 1; t >= 0; --t)
    {
        if (!state.windowOpen[static_cast<std::size_t>(t)]) continue;
        const WindowRect r = windowRectForTab(t, viewport);
        const float rx = static_cast<float>(sx) - r.x;
        const float ry = static_cast<float>(sy) - r.y;
        if (rx < 0.0F || ry < 0.0F || rx > r.w || ry > r.h) continue;

        // Close button at top right of the window.
        if (rx >= r.w - 24.0F && ry >= 6.0F && ry < 28.0F)
        {
            queue.push(CmdClosePreferencesWindow{t});
            return true;
        }

        // Footer: Apply / Revert / Defaults
        if (ry > r.h - kFooterH)
        {
            const float btnW = 100.0F;
            const float btnH = 28.0F;
            const float gap = 8.0F;
            const float btnY = r.h - kFooterH + (kFooterH - btnH) * 0.5F;
            // From right to left: Defaults | Revert | Apply
            const float defaultsX = r.w - btnW - gap;
            const float revertX = defaultsX - btnW - gap;
            const float applyX = revertX - btnW - gap;
            if (ry >= btnY && ry < btnY + btnH)
            {
                if (rx >= defaultsX && rx < defaultsX + btnW)
                {
                    queue.push(CmdRestoreDefaultsPreferences{});
                    return true;
                }
                if (rx >= revertX && rx < revertX + btnW)
                {
                    queue.push(CmdRevertPreferences{});
                    return true;
                }
                if (rx >= applyX && rx < applyX + btnW)
                {
                    queue.push(CmdApplyPreferences{});
                    return true;
                }
            }
            return true;
        }

        // Parameter row click.
        const float listY0 = kHeaderH;
        if (ry < listY0) return true;
        const auto names = prefsParametersForTab(registry,
            static_cast<config::PrefsTab>(t), "");
        const int scrollRows = state.windowScroll[static_cast<std::size_t>(t)];
        const int rowIdx = static_cast<int>((ry - listY0) / kRowH) + scrollRows;
        if (rowIdx < 0 || rowIdx >= static_cast<int>(names.size())) return true;
        const std::string& name = names[static_cast<std::size_t>(rowIdx)];
        const auto* def = registry.find(name);
        if (def == nullptr) return true;
        const auto current = prefsEffectiveValue(registry, state, name);

        // Value column is the right ~210px (before the close button strip).
        const float valueX = r.w - 220.0F;
        if (rx < valueX) return true;  // clicking the friendly label area is inert
        const float rowRelX = rx - valueX;
        const float btnW = 28.0F;
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
            else if (rowRelX >= btnW + 110.0F && rowRelX < btnW + 110.0F + btnW)
                pushChange(cur + step);
            return true;
        }
        case config::ParameterType::String:
        {
            if (def->domains.empty()) return true;
            if (name == "neural_network_type")
            {
                queue.push(CmdOpenPrefsPopup{"neural_combo"});
            }
            else
            {
                // For other enums keep the lightweight cycle behaviour.
                std::string cur = std::get<std::string>(current);
                std::size_t idx = 0;
                for (; idx < def->domains.size(); ++idx) if (def->domains[idx] == cur) break;
                idx = (idx + 1U) % def->domains.size();
                pushChange(def->domains[idx]);
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
    drawText(target, font, title, x + 12.0F, y + 6.0F, 14U, kAccent);
    // Close button.
    sf::RectangleShape close({18.0F, 18.0F});
    close.setPosition(x + w - 24.0F, y + 6.0F);
    close.setFillColor(kCloseButton);
    target.draw(close);
    drawText(target, font, "x", x + w - 21.0F, y + 5.0F, 14U, sf::Color::White);
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

    // -------- per-tab windows --------
    for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
    {
        if (!state.windowOpen[static_cast<std::size_t>(t)]) continue;
        const WindowRect r = windowRectForTab(t, vp);
        sf::RectangleShape bg({r.w, r.h});
        bg.setPosition(r.x, r.y);
        bg.setFillColor(kBg);
        bg.setOutlineColor(kBorder);
        bg.setOutlineThickness(1.4F);
        target.draw(bg);
        drawWindowHeader(target, font_, r.x, r.y, r.w,
            config::prefsTabLabel(static_cast<config::PrefsTab>(t)));

        const auto names = prefsParametersForTab(registry,
            static_cast<config::PrefsTab>(t), "");
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

            // Friendly label + internal name + apply-flags badge.
            const char* friendly = config::prefsFriendlyLabel(name);
            const std::string mainLabel = friendly != nullptr ? friendly : name;
            drawText(target, font_, mainLabel, r.x + 12.0F, y + 4.0F, 13U, kTextLight);
            if (friendly != nullptr)
            {
                drawText(target, font_, name, r.x + 12.0F, y + 18.0F, 10U, kTextDim);
            }
            drawText(target, font_, prefsApplyFlagsLabel(def->applyFlags),
                       r.x + r.w - 320.0F, y + 4.0F, 10U, kTextDim);

            // Value column.
            const float valueX = r.x + r.w - 220.0F;
            const auto effective = prefsEffectiveValue(registry, state, name);
            switch (def->type)
            {
            case config::ParameterType::Boolean:
            {
                const bool v = std::get<bool>(effective);
                sf::RectangleShape pill({44.0F, kRowH - 14.0F});
                pill.setPosition(valueX, y + 7.0F);
                pill.setFillColor(v ? kAccent : kBgButton);
                target.draw(pill);
                drawText(target, font_, v ? "on" : "off", valueX + 10.0F, y + 8.0F, 12U,
                           v ? sf::Color::Black : kTextLight);
                break;
            }
            case config::ParameterType::Integer:
            case config::ParameterType::Floating:
            {
                sf::RectangleShape minus({28.0F, kRowH - 14.0F});
                minus.setPosition(valueX, y + 7.0F);
                minus.setFillColor(kBgButton);
                target.draw(minus);
                drawText(target, font_, "-", valueX + 10.0F, y + 8.0F, 14U, kTextLight);
                const float labelW = def->type == config::ParameterType::Integer ? 70.0F : 110.0F;
                drawText(target, font_, fmtValue(effective), valueX + 36.0F, y + 8.0F, 12U, kTextLight);
                sf::RectangleShape plus({28.0F, kRowH - 14.0F});
                plus.setPosition(valueX + 28.0F + labelW, y + 7.0F);
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
                    sf::RectangleShape combo({200.0F, kRowH - 14.0F});
                    combo.setPosition(valueX, y + 7.0F);
                    combo.setFillColor(kBgButton);
                    target.draw(combo);
                    drawText(target, font_, s + "  v", valueX + 10.0F, y + 8.0F, 12U, kTextLight);
                }
                break;
            }
            case config::ParameterType::ColorRgb:
            {
                const auto c = std::get<config::ColorRgb>(effective);
                sf::RectangleShape sw({40.0F, kRowH - 14.0F});
                sw.setPosition(valueX, y + 7.0F);
                sw.setFillColor(sf::Color(
                    static_cast<sf::Uint8>(std::clamp(c.r, 0, 255)),
                    static_cast<sf::Uint8>(std::clamp(c.g, 0, 255)),
                    static_cast<sf::Uint8>(std::clamp(c.b, 0, 255))));
                sw.setOutlineThickness(1.0F);
                sw.setOutlineColor(kBorder);
                target.draw(sw);
                std::ostringstream s;
                s << "R" << c.r << " G" << c.g << " B" << c.b << "  ...";
                drawText(target, font_, s.str(), valueX + 48.0F, y + 8.0F, 11U, kTextLight);
                break;
            }
            }
        }

        // Scrollbar.
        drawScrollbar(target, r.x + r.w - kScrollbarW, r.y + kHeaderH,
                        listH, static_cast<int>(names.size()), visibleRows, firstRow);

        // Footer.
        const float footerY = r.y + r.h - kFooterH;
        sf::RectangleShape footer({r.w, kFooterH});
        footer.setPosition(r.x, footerY);
        footer.setFillColor(kBgHeader);
        target.draw(footer);
        const float btnW = 100.0F;
        const float btnH = 28.0F;
        const float gap = 8.0F;
        const float btnY = footerY + (kFooterH - btnH) * 0.5F;
        const float contentRight = r.x + r.w;
        struct Btn { const char* label; sf::Color color; };
        const std::array<Btn, 3> btns{{
            {"Aplicar",   sf::Color(80, 150, 90)},
            {"Reverter",  sf::Color(150, 90, 60)},
            {"Defaults",  sf::Color(80, 90, 130)},
        }};
        for (std::size_t i = 0; i < btns.size(); ++i)
        {
            const float x = contentRight - static_cast<float>(btns.size() - i) * (btnW + gap);
            sf::RectangleShape btn({btnW, btnH});
            btn.setPosition(x, btnY);
            btn.setFillColor(btns[i].color);
            target.draw(btn);
            drawText(target, font_, btns[i].label, x + 16.0F, btnY + 6.0F, 13U, sf::Color::White);
        }
    }

    // -------- Help window --------
    if (state.helpWindowOpen)
    {
        const WindowRect r = helpRect(vp);
        sf::RectangleShape bg({r.w, r.h});
        bg.setPosition(r.x, r.y);
        bg.setFillColor(kBg);
        bg.setOutlineColor(kBorder);
        bg.setOutlineThickness(1.4F);
        target.draw(bg);
        drawWindowHeader(target, font_, r.x, r.y, r.w, "Ajuda e atalhos");
        const std::array<const char*, 24> lines{{
            "Atalhos do canvas",
            "",
            "Space          Play / Pause",
            "Esc            Limpar selecao / fechar menus",
            "Delete         Matar selecionados",
            "R              Reset simulacao",
            "F              Fit world",
            "T              Toggle render simples",
            "V              Toggle visao debug",
            "H              Abrir/fechar esta janela",
            "S / Q / L      Tool: Select / Rect / Lasso",
            "G / A          Tool: Comida / Agente",
            "B / X          Tool: Brush / Apagar",
            "M / D          Tool: Move / Delete",
            "WASD / Setas   Pan camera",
            "Scroll         Zoom no canvas / Scroll na janela",
            "Right drag     Pan camera",
            "Shift/Ctrl     Selecao aditiva",
            "",
            "Menus",
            "Arquivo > Novo / Sair",
            "Exibir > toggles e overlays",
            "Preferencias > janelas Simulacao / Fisica / Visao /",
            "  Redes Neurais / Aparencia / Performance / Autosave"
        }};
        const int visible = static_cast<int>((r.h - kHeaderH - 10.0F) / 18.0F);
        const int firstRow = std::clamp(state.helpScroll, 0,
            static_cast<int>(lines.size()) - visible);
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
        sf::RectangleShape bg({r.w, r.h});
        bg.setPosition(r.x, r.y);
        bg.setFillColor(kBg);
        bg.setOutlineColor(kBorder);
        bg.setOutlineThickness(1.4F);
        target.draw(bg);
        drawWindowHeader(target, font_, r.x, r.y, r.w, "Substrato / Mundo");
        drawText(target, font_, "Configuracao de tipo de substrato vai aqui.",
                   r.x + 14.0F, r.y + 44.0F, 13U, kTextLight);
        drawText(target, font_, "O painel completo entra na Fase 24.",
                   r.x + 14.0F, r.y + 66.0F, 12U, kTextDim);
        drawText(target, font_, "(clique em qualquer lugar para fechar)",
                   r.x + 14.0F, r.y + 100.0F, 11U, kTextDim);
    }

    // -------- Popups (last, on top) --------
    if (state.openPopup == "neural_combo")
    {
        const auto* def = registry.find("neural_network_type");
        if (def != nullptr && !def->domains.empty())
        {
            const float w = 280.0F;
            const float itemH = 28.0F;
            const float h = static_cast<float>(def->domains.size()) * itemH;
            const float x = (static_cast<float>(vp.x) - w) * 0.5F;
            const float y = (static_cast<float>(vp.y) - h) * 0.5F;
            sf::RectangleShape bg({w, h});
            bg.setPosition(x, y);
            bg.setFillColor(kPopupBg);
            bg.setOutlineColor(kBorder);
            bg.setOutlineThickness(1.4F);
            target.draw(bg);
            const std::string cur = std::get<std::string>(
                prefsEffectiveValue(registry, state, "neural_network_type"));
            for (std::size_t i = 0; i < def->domains.size(); ++i)
            {
                const float ry = y + static_cast<float>(i) * itemH;
                const bool isCur = def->domains[i] == cur;
                if (isCur)
                {
                    sf::RectangleShape hi({w, itemH});
                    hi.setPosition(x, ry);
                    hi.setFillColor(kBgRowDirty);
                    target.draw(hi);
                }
                drawText(target, font_, def->domains[i], x + 14.0F, ry + 6.0F, 13U,
                           isCur ? sf::Color::White : kTextLight);
            }
        }
    }
    else if (state.openPopup.rfind("color:", 0) == 0)
    {
        const std::string param = state.openPopup.substr(6);
        const float w = 320.0F;
        const float h = 200.0F;
        const float x = (static_cast<float>(vp.x) - w) * 0.5F;
        const float y = (static_cast<float>(vp.y) - h) * 0.5F;
        sf::RectangleShape bg({w, h});
        bg.setPosition(x, y);
        bg.setFillColor(kPopupBg);
        bg.setOutlineColor(kBorder);
        bg.setOutlineThickness(1.4F);
        target.draw(bg);
        drawWindowHeader(target, font_, x, y, w, "Selecionar cor");

        const std::array<config::ColorRgb, 8> presets{{
            {220, 220, 220}, {200, 90, 70}, {90, 180, 90}, {80, 140, 220},
            {220, 200, 80}, {180, 90, 200}, {60, 200, 200}, {30, 32, 40}
        }};
        const float padX = x + 14.0F;
        const float padY = y + 36.0F;
        const float swSize = 32.0F;
        const float gap = 8.0F;
        for (std::size_t i = 0; i < presets.size(); ++i)
        {
            const float sxi = padX + static_cast<float>(i) * (swSize + gap);
            sf::RectangleShape sw({swSize, swSize});
            sw.setPosition(sxi, padY);
            sw.setFillColor(sf::Color(
                static_cast<sf::Uint8>(presets[i].r),
                static_cast<sf::Uint8>(presets[i].g),
                static_cast<sf::Uint8>(presets[i].b)));
            sw.setOutlineThickness(1.0F);
            sw.setOutlineColor(kBorder);
            target.draw(sw);
        }

        // Per-channel steppers
        const float btnW = 28.0F;
        const float btnH = 26.0F;
        const float btnsY = y + 90.0F;
        const float gap2 = 6.0F;
        config::ColorRgb cur{};
        const auto eff = prefsEffectiveValue(registry, state, param);
        if (const auto* v = std::get_if<config::ColorRgb>(&eff)) cur = *v;
        const std::array<const char*, 3> chLbl{{"R", "G", "B"}};
        for (int ch = 0; ch < 3; ++ch)
        {
            const float chx = padX + static_cast<float>(ch) * (btnW * 2.0F + gap2 + 10.0F);
            sf::RectangleShape minus({btnW, btnH});
            minus.setPosition(chx, btnsY);
            minus.setFillColor(kBgButton);
            target.draw(minus);
            drawText(target, font_, "-", chx + 10.0F, btnsY + 4.0F, 14U, kTextLight);
            sf::RectangleShape plus({btnW, btnH});
            plus.setPosition(chx + btnW, btnsY);
            plus.setFillColor(kBgButton);
            target.draw(plus);
            drawText(target, font_, "+", chx + btnW + 10.0F, btnsY + 4.0F, 14U, kTextLight);
            int val = ch == 0 ? cur.r : (ch == 1 ? cur.g : cur.b);
            std::ostringstream lbl;
            lbl << chLbl[static_cast<std::size_t>(ch)] << " " << val;
            drawText(target, font_, lbl.str(), chx + btnW * 2.0F + 6.0F, btnsY + 4.0F, 12U, kTextLight);
        }

        // Preview swatch.
        sf::RectangleShape preview({60.0F, 36.0F});
        preview.setPosition(x + w - 80.0F, y + h - 56.0F);
        preview.setFillColor(sf::Color(
            static_cast<sf::Uint8>(std::clamp(cur.r, 0, 255)),
            static_cast<sf::Uint8>(std::clamp(cur.g, 0, 255)),
            static_cast<sf::Uint8>(std::clamp(cur.b, 0, 255))));
        preview.setOutlineThickness(1.0F);
        preview.setOutlineColor(kBorder);
        target.draw(preview);
        drawText(target, font_, "preview", x + 14.0F, y + h - 32.0F, 11U, kTextDim);
    }
}
} // namespace agentbiosim::ui
