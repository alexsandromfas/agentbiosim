#include "ui/UiLeftDock.hpp"

#include "config/ParameterMetadata.hpp"
#include "ui/UiPreferencesPanel.hpp"  // prefsEffectiveValue

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <variant>

namespace agentbiosim::ui
{
namespace
{
const sf::Color kBg(20, 22, 30, 252);
const sf::Color kBgTabBar(28, 32, 42);
const sf::Color kBgTabActive(50, 90, 150);
const sf::Color kBgRow(28, 32, 40);
const sf::Color kBgRowAlt(24, 28, 36);
const sf::Color kBgRowDirty(60, 90, 130);
const sf::Color kBgSection(40, 55, 80);
const sf::Color kBgCard(30, 34, 44);
const sf::Color kTextLight(220, 224, 232);
const sf::Color kTextDim(120, 128, 138);
const sf::Color kTextDisabled(90, 96, 104);
const sf::Color kAccent(130, 200, 250);
const sf::Color kBgButton(46, 52, 64);
const sf::Color kBgButtonDisabled(34, 38, 46);
const sf::Color kBorder(70, 130, 200, 220);

constexpr float kTabBarH = 32.0F;
constexpr float kFooterH = 46.0F;
constexpr float kRowH = 30.0F;
constexpr float kScrollbarW = 8.0F;
constexpr float kCardH = 104.0F;

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
        sf::CircleShape corner(radius, 16);
        corner.setOrigin(radius, radius);
        corner.setPosition(cx, cy);
        corner.setFillColor(fill);
        target.draw(corner);
    }
}

void drawScrollbar(sf::RenderTarget& target, float x, float y, float h,
                     int rowCount, int visibleRows, int scrollOffset)
{
    if (rowCount <= visibleRows) return;
    sf::RectangleShape track({kScrollbarW, h});
    track.setPosition(x, y);
    track.setFillColor(sf::Color(40, 44, 56));
    target.draw(track);
    const float ratioVisible = static_cast<float>(visibleRows) / static_cast<float>(rowCount);
    const float thumbH = std::max(20.0F, h * ratioVisible);
    const float ratioPos = static_cast<float>(scrollOffset) /
                            static_cast<float>(std::max(1, rowCount - visibleRows));
    const float thumbY = y + (h - thumbH) * ratioPos;
    sf::RectangleShape thumb({kScrollbarW, thumbH});
    thumb.setPosition(x, thumbY);
    thumb.setFillColor(sf::Color(90, 130, 180));
    target.draw(thumb);
}

std::string fmtValue(const config::ParameterValue& v)
{
    return std::visit([](const auto& tv) -> std::string {
        using T = std::decay_t<decltype(tv)>;
        if constexpr (std::is_same_v<T, bool>) return tv ? "on" : "off";
        else if constexpr (std::is_same_v<T, int>) return std::to_string(tv);
        else if constexpr (std::is_same_v<T, double>)
        {
            std::ostringstream s; s << std::setprecision(6) << tv; return s.str();
        }
        else if constexpr (std::is_same_v<T, std::string>) return tv;
        else if constexpr (std::is_same_v<T, config::ColorRgb>)
            return std::to_string(tv.r) + "," + std::to_string(tv.g) + "," + std::to_string(tv.b);
    }, v);
}

// ---- parameter rows (Editor / Substrato), copied from the proven UiOperationalPanels ----

struct PanelRow
{
    bool isSection = false;
    const char* sectionLabel = "";
    std::string paramName;
};

std::string prettyLabel(const std::string& name, const std::string& speciesPrefix)
{
    const char* full = config::prefsFriendlyLabel(name);
    if (full != nullptr) return full;
    if (!speciesPrefix.empty() && name.rfind(speciesPrefix, 0) == 0)
    {
        std::string suffix = name.substr(speciesPrefix.size());
        if (!suffix.empty() && suffix[0] == '_') suffix = suffix.substr(1);
        const char* bySuffix = config::prefsFriendlyLabelBySuffix(suffix);
        if (bySuffix != nullptr) return bySuffix;
        return suffix;
    }
    return name;
}

std::vector<PanelRow> editorRowsFor(const std::string& species,
                                       const config::ParameterRegistry& reg)
{
    auto has = [&](const std::string& s) { return reg.find(species + s) != nullptr; };
    std::vector<PanelRow> rows;
    auto sec = [&](const char* l) { PanelRow r; r.isSection = true; r.sectionLabel = l; rows.push_back(r); };
    auto p = [&](const char* s) { if (has(s)) { PanelRow r; r.paramName = species + s; rows.push_back(r); } };
    sec("Corpo e movimento");
    p("_body_size"); p("_body_shape"); p("_max_speed"); p("_max_turn");
    p("_allow_reverse_locomotion"); p("_movement_mode");
    sec("Energia e metabolismo");
    p("_initial_energy"); p("_death_energy"); p("_split_energy"); p("_v0_cost");
    p("_vmax_cost"); p("_energy_cap"); p("_death_by_age_enabled"); p("_death_age");
    p("_corpse_to_food"); p("_reproduction_min_age"); p("_reproduction_cooldown");
    sec("Visao");
    p("_vision_radius"); p("_retina_count"); p("_retina_fov_degrees"); p("_eye_count");
    p("_eye_angle_degrees"); p("_see_food"); p("_see_agents"); p("_see_predators");
    p("_see_obstacles"); p("_see_through_walls"); p("_retina_channel_r");
    p("_retina_channel_g"); p("_retina_channel_b"); p("_retina_channel_d");
    p("_retina_input_mode");
    sec("Dieta");
    p("_diet_food"); p("_diet_agents"); p("_diet_same_label"); p("_food_efficiency");
    p("_agent_efficiency");
    sec("Rede neural");
    p("_hidden_layers"); p("_mutation_rate"); p("_mutation_strength");
    return rows;
}

std::vector<PanelRow> substratoRows(const config::ParameterRegistry& reg)
{
    std::vector<PanelRow> rows;
    auto sec = [&](const char* l) { PanelRow r; r.isSection = true; r.sectionLabel = l; rows.push_back(r); };
    auto p = [&](const char* n) { if (reg.find(n) != nullptr) { PanelRow r; r.paramName = n; rows.push_back(r); } };
    sec("Comida");
    p("food_mode"); p("food_target"); p("food_min_r"); p("food_max_r");
    p("food_replenish_interval"); p("food_color");
    sec("Comida em pedacos (chunk)");
    p("food_bite_seconds"); p("food_piece_particle_radius"); p("food_piece_cluster_radius");
    p("food_piece_particle_spacing"); p("food_piece_replenish_mode"); p("food_trim_max_per_step");
    sec("Substrato");
    p("substrate_shape"); p("world_w"); p("world_h"); p("substrate_radius");
    return rows;
}

void drawParamRow(sf::RenderTarget& target, const sf::Font* font,
                    const PanelRow& row, const config::ParameterRegistry& reg,
                    const PreferencesState& state, float x, float y, float w,
                    const std::string& speciesPrefix)
{
    if (row.isSection)
    {
        sf::RectangleShape bg({w, kRowH - 1.0F});
        bg.setPosition(x, y);
        bg.setFillColor(kBgSection);
        target.draw(bg);
        drawText(target, font, row.sectionLabel, x + 12.0F, y + 7.0F, 13U, kAccent);
        return;
    }
    const auto* def = reg.find(row.paramName);
    if (def == nullptr) return;
    const bool dirty = state.pendingValues.count(row.paramName) > 0U;
    sf::RectangleShape bg({w, kRowH - 1.0F});
    bg.setPosition(x, y);
    bg.setFillColor(dirty ? kBgRowDirty : kBgRow);
    target.draw(bg);
    drawText(target, font, prettyLabel(row.paramName, speciesPrefix),
               x + 12.0F, y + 7.0F, 13U, kTextLight);
    const config::ParameterValue current = prefsEffectiveValue(reg, state, row.paramName);
    const float valueX = x + w - 200.0F;
    const bool editing = state.editingParam == row.paramName;
    switch (def->type)
    {
    case config::ParameterType::Boolean:
    {
        const bool v = std::get<bool>(current);
        drawRoundedRect(target, valueX, y + 6.0F, 50.0F, kRowH - 14.0F, 6.0F, v ? kAccent : kBgButton);
        drawText(target, font, v ? "on" : "off", valueX + 12.0F, y + 7.0F, 12U,
                   v ? sf::Color::Black : kTextLight);
        break;
    }
    case config::ParameterType::Integer:
    case config::ParameterType::Floating:
    {
        if (editing)
        {
            drawRoundedRect(target, valueX, y + 5.0F, 180.0F, kRowH - 12.0F, 5.0F,
                              sf::Color(30, 50, 70), kAccent, 1.6F);
            drawText(target, font, state.editingBuffer + "_", valueX + 8.0F, y + 7.0F, 12U, kTextLight);
        }
        else
        {
            drawRoundedRect(target, valueX, y + 5.0F, 180.0F, kRowH - 12.0F, 5.0F, kBgButton);
            drawText(target, font, fmtValue(current), valueX + 12.0F, y + 7.0F, 12U, kTextLight);
            drawText(target, font, "(editar)", valueX + 128.0F, y + 8.0F, 10U, kTextDim);
        }
        break;
    }
    case config::ParameterType::String:
    {
        const std::string s = std::get<std::string>(current);
        const auto values = config::prefsEnumValuesFor(row.paramName);
        if (values.empty()) { drawText(target, font, s, valueX, y + 7.0F, 12U, kTextDim); }
        else
        {
            drawRoundedRect(target, valueX, y + 5.0F, 180.0F, kRowH - 12.0F, 5.0F, kBgButton);
            drawText(target, font, s + "  v", valueX + 12.0F, y + 7.0F, 12U, kTextLight);
        }
        break;
    }
    case config::ParameterType::ColorRgb:
    {
        const auto c = std::get<config::ColorRgb>(current);
        drawRoundedRect(target, valueX, y + 5.0F, 40.0F, kRowH - 12.0F, 4.0F,
                          sf::Color(static_cast<sf::Uint8>(std::clamp(c.r, 0, 255)),
                                      static_cast<sf::Uint8>(std::clamp(c.g, 0, 255)),
                                      static_cast<sf::Uint8>(std::clamp(c.b, 0, 255))),
                          kBorder, 1.0F);
        std::ostringstream s; s << "R" << c.r << " G" << c.g << " B" << c.b;
        drawText(target, font, s.str(), valueX + 50.0F, y + 8.0F, 11U, kTextLight);
        break;
    }
    }
}

bool handleParamRowClick(const PanelRow& row, const config::ParameterRegistry& reg,
                            const PreferencesState& state, float rx, float w, CommandQueue& queue)
{
    if (row.isSection) return false;
    const auto* def = reg.find(row.paramName);
    if (def == nullptr) return false;
    const float valueX = w - 200.0F;
    if (rx < valueX) return true;  // label area inert
    const config::ParameterValue current = prefsEffectiveValue(reg, state, row.paramName);
    switch (def->type)
    {
    case config::ParameterType::Boolean:
        queue.push(CmdSetParameterValue{row.paramName, !std::get<bool>(current)});
        return true;
    case config::ParameterType::Integer:
    case config::ParameterType::Floating:
        queue.push(CmdBeginEditParameter{row.paramName, fmtValue(current)});
        return true;
    case config::ParameterType::String:
        if (!config::prefsEnumValuesFor(row.paramName).empty())
            queue.push(CmdOpenPrefsPopup{"enum:" + row.paramName});
        return true;
    case config::ParameterType::ColorRgb:
        queue.push(CmdOpenPrefsPopup{"color:" + row.paramName});
        return true;
    }
    return true;
}

// ---- Labels tab: per-species card geometry (shared by draw + click) ----

struct LabelCardLayout
{
    float swatchX, swatchY, swatchSize;
    float nameX, nameY;
    // spinner groups: [minus, plus] x per field (min/max/inicial)
    float spinY;
    std::array<float, 3> spinMinusX;
    std::array<float, 3> spinPlusX;
    float spinBtnW;
    float graficoX, graficoY, graficoSize;
    // action buttons row
    float btnY, btnW, btnH;
    std::array<float, 6> btnX;  // Selecionar, Atribuir, Remover, Cor, Reset, Excluir
};

LabelCardLayout computeLabelCard(float x, float y, float w)
{
    LabelCardLayout L;
    L.swatchX = x + 10.0F; L.swatchY = y + 8.0F; L.swatchSize = 26.0F;
    L.nameX = x + 44.0F;   L.nameY = y + 8.0F;
    // spinners row
    L.spinY = y + 38.0F;
    L.spinBtnW = 22.0F;
    const float groupW = 132.0F;  // label + [-] val [+]
    for (int i = 0; i < 3; ++i)
    {
        const float gx = x + 10.0F + static_cast<float>(i) * groupW;
        L.spinMinusX[static_cast<std::size_t>(i)] = gx + 56.0F;
        L.spinPlusX[static_cast<std::size_t>(i)] = gx + 56.0F + L.spinBtnW + 34.0F;
    }
    L.graficoX = x + w - 30.0F; L.graficoY = y + 38.0F; L.graficoSize = 20.0F;
    // action buttons
    L.btnY = y + 70.0F; L.btnH = 24.0F;
    L.btnW = (w - 20.0F - 5.0F * 6.0F) / 6.0F;
    for (int i = 0; i < 6; ++i)
        L.btnX[static_cast<std::size_t>(i)] = x + 10.0F + static_cast<float>(i) * (L.btnW + 6.0F);
    return L;
}
} // namespace

// ---------------- public static helpers ----------------

std::vector<std::string> UiLeftDock::editorParameters()
{
    return {
        "bacteria_body_size", "bacteria_body_shape", "bacteria_max_speed",
        "bacteria_max_turn", "bacteria_allow_reverse_locomotion",
        "bacteria_movement_mode", "bacteria_initial_energy",
        "bacteria_death_energy", "bacteria_split_energy", "bacteria_v0_cost",
        "bacteria_vmax_cost", "bacteria_energy_cap",
        "bacteria_death_by_age_enabled", "bacteria_death_age",
        "bacteria_corpse_to_food", "bacteria_reproduction_min_age",
        "bacteria_reproduction_cooldown", "bacteria_vision_radius",
        "bacteria_retina_count", "bacteria_retina_fov_degrees",
        "bacteria_eye_count", "bacteria_eye_angle_degrees",
        "bacteria_retina_see_food", "bacteria_retina_see_bacteria", "bacteria_retina_see_predators",
        "bacteria_retina_see_obstacles", "bacteria_retina_see_all", "bacteria_retina_see_through_walls",
        "bacteria_retina_channel_r", "bacteria_retina_channel_g",
        "bacteria_retina_channel_b", "bacteria_retina_channel_d",
        "bacteria_retina_input_mode", "bacteria_diet_food",
        "bacteria_diet_agents", "bacteria_diet_same_label",
        "bacteria_food_efficiency", "bacteria_agent_efficiency",
        "bacteria_hidden_layers", "bacteria_mutation_rate",
        "bacteria_mutation_strength"
    };
}

std::vector<std::string> UiLeftDock::substratoParameters()
{
    return {"substrate_shape", "world_w", "world_h", "substrate_radius",
            "food_mode", "food_target", "food_min_r", "food_max_r",
            "food_replenish_interval", "food_color", "food_bite_seconds",
            "food_piece_particle_radius", "food_piece_cluster_radius",
            "food_piece_particle_spacing", "food_piece_replenish_mode",
            "food_trim_max_per_step"};
}

// ---------------- geometry ----------------

UiLeftDock::Rect UiLeftDock::dockRect(const sf::Vector2u vp, const float topStripH) const noexcept
{
    Rect r;
    r.x = 0.0F;
    r.y = topStripH;
    r.w = std::min(kDockW, static_cast<float>(vp.x) - 40.0F);
    r.h = static_cast<float>(vp.y) - topStripH;
    return r;
}

bool UiLeftDock::pointInsideDock(const int sx, const int sy, const sf::Vector2u vp,
                                    const float topStripH,
                                    const PreferencesState& state) const noexcept
{
    if (!state.dockVisible) return false;
    const Rect r = dockRect(vp, topStripH);
    return static_cast<float>(sx) >= r.x && static_cast<float>(sx) < r.x + r.w &&
              static_cast<float>(sy) >= r.y && static_cast<float>(sy) < r.y + r.h;
}

// ---------------- wheel ----------------

bool UiLeftDock::handleMouseWheel(const int sx, const int sy, const sf::Vector2u vp,
                                     const float topStripH, const float delta,
                                     PreferencesState& state, CommandQueue& queue)
{
    if (!pointInsideDock(sx, sy, vp, topStripH, state)) return false;
    queue.push(CmdScrollDock{static_cast<int>(delta * -3.0F)});
    return true;
}

// ---------------- click ----------------

bool UiLeftDock::handleMouseClick(const int sx, const int sy, const sf::Vector2u vp,
                                     const float topStripH,
                                     const config::ParameterRegistry& registry,
                                     const sim::SimulationRunner& runner,
                                     PreferencesState& state, CommandQueue& queue)
{
    if (!state.dockVisible) return false;
    const Rect r = dockRect(vp, topStripH);
    const float rx = static_cast<float>(sx) - r.x;
    const float ry = static_cast<float>(sy) - r.y;
    if (rx < 0.0F || ry < 0.0F || rx > r.w || ry > r.h) return false;

    // Tab bar.
    if (ry < kTabBarH)
    {
        const int tab = static_cast<int>(rx / (r.w / 3.0F));
        if (tab >= 0 && tab < 3) queue.push(CmdSetDockTab{tab});
        return true;
    }

    const float contentY = kTabBarH;
    const float contentH = r.h - kTabBarH - kFooterH;
    const float footerY = r.h - kFooterH;

    // ---- Editor / Substrato: parameter rows + footer ----
    if (state.dockActiveTab == 0 || state.dockActiveTab == 1)
    {
        const bool editor = state.dockActiveTab == 0;
        const auto rows = editor ? editorRowsFor("bacteria", registry) : substratoRows(registry);
        const std::string prefix = editor ? "bacteria" : "";

        if (ry >= footerY)
        {
            // Footer buttons.
            const float btnW = 104.0F, btnH = 30.0F, gap = 6.0F;
            const float btnY = footerY + (kFooterH - btnH) * 0.5F;
            std::vector<Command> cmds;
            if (editor)
                cmds = {Command{CmdApplyGenomeToSpecies{}}, Command{CmdApplyGenomeToSelected{}},
                        Command{CmdRevertPreferences{}}, Command{CmdRestoreDefaultsAndApply{}}};
            else
                cmds = {Command{CmdApplyEnvironment{}}, Command{CmdClearAllFood{}},
                        Command{CmdRevertPreferences{}}, Command{CmdRestoreDefaultsAndApply{}}};
            for (std::size_t i = 0; i < cmds.size(); ++i)
            {
                const float bx = 8.0F + static_cast<float>(i) * (btnW + gap);
                if (ry >= btnY && ry < btnY + btnH && rx >= bx && rx < bx + btnW)
                {
                    queue.push(cmds[i]);
                    return true;
                }
            }
            return true;
        }

        if (ry < contentY) return true;
        const int rowIdx = static_cast<int>((ry - contentY) / kRowH) + state.dockScroll;
        if (rowIdx < 0 || rowIdx >= static_cast<int>(rows.size())) return true;
        handleParamRowClick(rows[static_cast<std::size_t>(rowIdx)], registry, state,
                              rx, r.w - kScrollbarW, queue);
        return true;
    }

    // ---- Labels tab ----
    if (state.dockActiveTab == 2)
    {
        if (ry >= footerY)
        {
            // "+ Nova label com selecionados" — full-width footer button.
            queue.push(CmdCreateSpeciesFromSelected{});
            return true;
        }
        if (ry < contentY) return true;

        const auto& recs = runner.species().records();
        // Build the visible (enabled) species list.
        std::vector<std::size_t> visible;
        for (std::size_t i = 0; i < recs.size(); ++i)
            if (recs[i].enabled) visible.push_back(i);

        const int cardIdx = static_cast<int>((ry - contentY) / kCardH) + state.dockScroll;
        if (cardIdx < 0 || cardIdx >= static_cast<int>(visible.size())) return true;
        const auto& rec = recs[visible[static_cast<std::size_t>(cardIdx)]];
        const auto sid = static_cast<std::uint32_t>(rec.id);
        const bool isBacteria = (rec.name == "bacteria");

        const float cardY = contentY + static_cast<float>(cardIdx - state.dockScroll) * kCardH;
        const LabelCardLayout L = computeLabelCard(0.0F, cardY, r.w - kScrollbarW);

        // Name area -> begin edit.
        if (ry >= L.nameY && ry < L.nameY + 22.0F && rx >= L.nameX && rx < L.nameX + 200.0F)
        {
            queue.push(CmdBeginEditSpeciesName{sid, rec.label.empty() ? rec.name : rec.label});
            return true;
        }
        // Spinners (min/max/inicial).
        if (ry >= L.spinY && ry < L.spinY + L.spinBtnW)
        {
            for (int f = 0; f < 3; ++f)
            {
                const auto fi = static_cast<std::size_t>(f);
                if (rx >= L.spinMinusX[fi] && rx < L.spinMinusX[fi] + L.spinBtnW)
                { queue.push(CmdAdjustSpeciesPop{sid, f, -1}); return true; }
                if (rx >= L.spinPlusX[fi] && rx < L.spinPlusX[fi] + L.spinBtnW)
                { queue.push(CmdAdjustSpeciesPop{sid, f, +1}); return true; }
            }
            // Grafico checkbox.
            if (rx >= L.graficoX && rx < L.graficoX + L.graficoSize)
            { queue.push(CmdSetSpeciesShowGraph{sid, !rec.showGraph}); return true; }
        }
        // Action buttons row.
        if (ry >= L.btnY && ry < L.btnY + L.btnH)
        {
            for (int b = 0; b < 6; ++b)
            {
                const auto bi = static_cast<std::size_t>(b);
                if (rx >= L.btnX[bi] && rx < L.btnX[bi] + L.btnW)
                {
                    switch (b)
                    {
                    case 0: queue.push(CmdSelectAllOfSpecies{sid}); break;
                    case 1: queue.push(CmdAssignSelectedToSpecies{sid}); break;
                    case 2: queue.push(CmdRemoveSelectedFromSpecies{sid}); break;
                    case 3: queue.push(CmdCycleSpeciesColor{sid}); break;
                    case 4: break;  // Reset rede neural — DISABLED (Fase 25)
                    case 5: if (!isBacteria) queue.push(CmdRemoveSpecies{sid}); break;
                    default: break;
                    }
                    return true;
                }
            }
        }
        return true;
    }
    return true;
}

// ---------------- draw ----------------

void UiLeftDock::draw(sf::RenderTarget& target, const float topStripH,
                         const config::ParameterRegistry& registry,
                         const sim::SimulationRunner& runner,
                         const PreferencesState& state) const
{
    if (!state.dockVisible) return;
    const Rect r = dockRect(target.getSize(), topStripH);

    // Background.
    sf::RectangleShape bg({r.w, r.h});
    bg.setPosition(r.x, r.y);
    bg.setFillColor(kBg);
    target.draw(bg);

    // Tab bar.
    sf::RectangleShape tabBar({r.w, kTabBarH});
    tabBar.setPosition(r.x, r.y);
    tabBar.setFillColor(kBgTabBar);
    target.draw(tabBar);
    const std::array<const char*, 3> tabs{{"Editor Genetico", "Substrato", "Labels"}};
    const float tabW = r.w / 3.0F;
    for (int i = 0; i < 3; ++i)
    {
        const float tx = r.x + static_cast<float>(i) * tabW;
        const bool active = state.dockActiveTab == i;
        if (active)
        {
            sf::RectangleShape hi({tabW, kTabBarH});
            hi.setPosition(tx, r.y);
            hi.setFillColor(kBgTabActive);
            target.draw(hi);
        }
        drawText(target, font_, tabs[static_cast<std::size_t>(i)], tx + 10.0F, r.y + 8.0F, 12U,
                   active ? sf::Color::White : kTextLight);
    }

    const float contentY = r.y + kTabBarH;
    const float contentH = r.h - kTabBarH - kFooterH;
    const float footerY = r.y + r.h - kFooterH;

    // ---- Editor / Substrato ----
    if (state.dockActiveTab == 0 || state.dockActiveTab == 1)
    {
        const bool editor = state.dockActiveTab == 0;
        const auto rows = editor ? editorRowsFor("bacteria", registry) : substratoRows(registry);
        const std::string prefix = editor ? "bacteria" : "";
        const int visibleRows = static_cast<int>(contentH / kRowH);
        const int first = std::max(0, std::min(state.dockScroll,
            static_cast<int>(rows.size()) - visibleRows));
        for (int i = 0; i < visibleRows; ++i)
        {
            const int idx = first + i;
            if (idx >= static_cast<int>(rows.size())) break;
            drawParamRow(target, font_, rows[static_cast<std::size_t>(idx)], registry, state,
                           r.x, contentY + static_cast<float>(i) * kRowH, r.w - kScrollbarW, prefix);
        }
        drawScrollbar(target, r.x + r.w - kScrollbarW, contentY, contentH,
                        static_cast<int>(rows.size()), visibleRows, first);

        // Footer.
        sf::RectangleShape footer({r.w, kFooterH});
        footer.setPosition(r.x, footerY);
        footer.setFillColor(kBgTabBar);
        target.draw(footer);
        const float btnW = 104.0F, btnH = 30.0F, gap = 6.0F;
        const float btnY = footerY + (kFooterH - btnH) * 0.5F;
        struct B { const char* label; sf::Color color; };
        std::array<B, 4> btns = editor
            ? std::array<B, 4>{{ {"Aplic. especie", sf::Color(80, 150, 90)},
                                 {"Aplic. selec.", sf::Color(120, 130, 200)},
                                 {"Reverter", sf::Color(150, 90, 60)},
                                 {"Padroes", sf::Color(80, 90, 130)} }}
            : std::array<B, 4>{{ {"Aplic. ambiente", sf::Color(80, 150, 90)},
                                 {"Limpar comida", sf::Color(180, 100, 60)},
                                 {"Reverter", sf::Color(150, 90, 60)},
                                 {"Padroes", sf::Color(80, 90, 130)} }};
        for (std::size_t i = 0; i < btns.size(); ++i)
        {
            const float bx = r.x + 8.0F + static_cast<float>(i) * (btnW + gap);
            drawRoundedRect(target, bx, btnY, btnW, btnH, 7.0F, btns[i].color);
            drawText(target, font_, btns[i].label, bx + 6.0F, btnY + 7.0F, 11U, sf::Color::White);
        }
        return;
    }

    // ---- Labels ----
    if (state.dockActiveTab == 2)
    {
        const auto& recs = runner.species().records();
        std::vector<std::size_t> visible;
        for (std::size_t i = 0; i < recs.size(); ++i)
            if (recs[i].enabled) visible.push_back(i);

        const int visibleCards = static_cast<int>(contentH / kCardH);
        const int first = std::max(0, std::min(state.dockScroll,
            static_cast<int>(visible.size()) - visibleCards));
        for (int i = 0; i < visibleCards; ++i)
        {
            const int idx = first + i;
            if (idx >= static_cast<int>(visible.size())) break;
            const auto& rec = recs[visible[static_cast<std::size_t>(idx)]];
            const float cardY = contentY + static_cast<float>(i) * kCardH;
            const float cardW = r.w - kScrollbarW;
            drawRoundedRect(target, r.x + 4.0F, cardY + 2.0F, cardW - 8.0F, kCardH - 6.0F,
                              6.0F, (i % 2 == 0) ? kBgCard : kBgRowAlt, kBorder, 1.0F);
            const LabelCardLayout L = computeLabelCard(r.x, cardY, cardW);
            const bool isBacteria = (rec.name == "bacteria");

            // Swatch.
            drawRoundedRect(target, L.swatchX, L.swatchY, L.swatchSize, L.swatchSize, 4.0F,
                              sf::Color(rec.color.r, rec.color.g, rec.color.b), kBorder, 1.0F);
            // Name (or inline editor).
            if (state.editingSpeciesId == static_cast<std::uint32_t>(rec.id))
            {
                drawRoundedRect(target, L.nameX, L.nameY, 200.0F, 22.0F, 4.0F,
                                  sf::Color(30, 50, 70), kAccent, 1.4F);
                drawText(target, font_, state.editingSpeciesBuffer + "_",
                           L.nameX + 6.0F, L.nameY + 3.0F, 13U, kTextLight);
            }
            else
            {
                drawText(target, font_, rec.label.empty() ? rec.name : rec.label,
                           L.nameX, L.nameY, 14U, kTextLight);
            }
            // Count.
            const std::size_t n = runner.countAgentsOfSpecies(rec.id);
            std::ostringstream cnt; cnt << n << " individuos";
            drawText(target, font_, cnt.str(), r.x + cardW - 130.0F, L.nameY + 2.0F, 11U, kTextDim);

            // Spinners: Min / Max / Inicial.
            const std::array<const char*, 3> sl{{"Min", "Max", "Ini"}};
            const std::array<int, 3> sv{{rec.minPopulation, rec.maxPopulation, rec.initialCount}};
            for (int f = 0; f < 3; ++f)
            {
                const auto fi = static_cast<std::size_t>(f);
                const float lblX = L.spinMinusX[fi] - 46.0F;
                drawText(target, font_, sl[fi], lblX, L.spinY + 3.0F, 11U, kTextDim);
                drawRoundedRect(target, L.spinMinusX[fi], L.spinY, L.spinBtnW, L.spinBtnW, 4.0F, kBgButton);
                drawText(target, font_, "-", L.spinMinusX[fi] + 7.0F, L.spinY + 1.0F, 13U, kTextLight);
                drawText(target, font_, std::to_string(sv[fi]),
                           L.spinMinusX[fi] + L.spinBtnW + 4.0F, L.spinY + 3.0F, 11U, kTextLight);
                drawRoundedRect(target, L.spinPlusX[fi], L.spinY, L.spinBtnW, L.spinBtnW, 4.0F, kBgButton);
                drawText(target, font_, "+", L.spinPlusX[fi] + 6.0F, L.spinY + 1.0F, 13U, kTextLight);
            }
            // Grafico checkbox.
            drawRoundedRect(target, L.graficoX, L.graficoY, L.graficoSize, L.graficoSize, 4.0F,
                              rec.showGraph ? kAccent : kBgButton);
            if (rec.showGraph)
                drawText(target, font_, "x", L.graficoX + 5.0F, L.graficoY + 1.0F, 12U, sf::Color::Black);

            // Action buttons.
            const std::array<const char*, 6> bl{{"Selec.", "Atribuir", "Remover", "Cor", "Rede", "Excluir"}};
            for (int b = 0; b < 6; ++b)
            {
                const auto bi = static_cast<std::size_t>(b);
                const bool disabled = (b == 4) || (b == 5 && isBacteria);  // Reset rede / Excluir(bacteria)
                drawRoundedRect(target, L.btnX[bi], L.btnY, L.btnW, L.btnH, 5.0F,
                                  disabled ? kBgButtonDisabled : kBgButton);
                drawText(target, font_, bl[bi], L.btnX[bi] + 5.0F, L.btnY + 5.0F, 10U,
                           disabled ? kTextDisabled : kTextLight);
            }
            if (true)  // small caption under Rede button noting Fase 25
            {
                drawText(target, font_, "(Rede: Fase 25)", L.btnX[4] - 2.0F,
                           L.btnY + L.btnH + 1.0F, 9U, kTextDisabled);
            }
        }
        drawScrollbar(target, r.x + r.w - kScrollbarW, contentY, contentH,
                        static_cast<int>(visible.size()), visibleCards, first);

        // Footer: "+ Nova label com selecionados".
        sf::RectangleShape footer({r.w, kFooterH});
        footer.setPosition(r.x, footerY);
        footer.setFillColor(kBgTabBar);
        target.draw(footer);
        const float btnY = footerY + (kFooterH - 30.0F) * 0.5F;
        drawRoundedRect(target, r.x + 8.0F, btnY, r.w - 16.0F, 30.0F, 7.0F, sf::Color(80, 150, 90));
        drawText(target, font_, "+ Nova label com selecionados",
                   r.x + 16.0F, btnY + 7.0F, 13U, sf::Color::White);
    }
}
} // namespace agentbiosim::ui
