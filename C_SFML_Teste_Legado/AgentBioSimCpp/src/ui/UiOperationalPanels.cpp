#include "ui/UiOperationalPanels.hpp"

#include "config/ParameterHelpers.hpp"
#include "config/ParameterMetadata.hpp"
#include "ui/UiPreferencesPanel.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include <algorithm>
#include <array>
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
const sf::Color kBgSection(40, 55, 80);
const sf::Color kTextLight(220, 224, 232);
const sf::Color kTextDim(120, 128, 138);
const sf::Color kAccent(130, 200, 250);
const sf::Color kBgButton(40, 44, 54);
const sf::Color kBorder(70, 130, 200, 220);
const sf::Color kCloseButton(170, 70, 70);

constexpr float kRowH = 30.0F;
constexpr float kHeaderH = 32.0F;
constexpr float kFooterH = 46.0F;
constexpr float kScrollbarW = 8.0F;
constexpr float kWindowW = 560.0F;
constexpr float kWindowH = 520.0F;
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

void drawWindowHeader(sf::RenderTarget& target, const sf::Font* font,
                        float x, float y, float w, const char* title)
{
    sf::RectangleShape hd({w, kHeaderH});
    hd.setPosition(x, y);
    hd.setFillColor(kBgHeader);
    target.draw(hd);
    drawText(target, font, title, x + 14.0F, y + 7.0F, 14U, kAccent);
    constexpr float pad = 5.0F;
    const float bx = x + w - kCloseBtnSize - pad;
    const float by = y + (kHeaderH - kCloseBtnSize) * 0.5F;
    drawRoundedRect(target, bx, by, kCloseBtnSize, kCloseBtnSize, 4.0F, kCloseButton);
    drawText(target, font, "x", bx + 6.0F, by + 1.0F, 14U, sf::Color::White);
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

// Phase 24: row entries — either a parameter row or a section divider.
struct PanelRow
{
    bool isSection = false;
    const char* sectionLabel = "";
    std::string paramName;
};

// Phase 24: friendly label helper that falls back to the raw name. We strip
// the species prefix from the displayed label so the user sees just the field.
std::string prettyLabel(const std::string& name, const std::string& speciesPrefix)
{
    const char* f = config::prefsFriendlyLabel(name);
    if (f != nullptr) return f;
    if (!speciesPrefix.empty() && name.rfind(speciesPrefix, 0) == 0)
    {
        std::string trimmed = name.substr(speciesPrefix.size());
        if (!trimmed.empty() && trimmed[0] == '_') trimmed = trimmed.substr(1);
        return trimmed;
    }
    return name;
}

// Phase 24: build the editor genetico parameter list for a given species.
// Sections roughly match the Python "Editor Genetico" page (Corpo, Energia,
// Visao, Dieta, Rede neural). Items only appear if the underlying registry
// parameter exists; everything else is filtered out.
std::vector<PanelRow> editorRowsFor(const std::string& species,
                                       const config::ParameterRegistry& reg)
{
    auto has = [&](const std::string& suffix) {
        return reg.find(species + suffix) != nullptr;
    };
    std::vector<PanelRow> rows;
    auto sec = [&](const char* l) {
        PanelRow r; r.isSection = true; r.sectionLabel = l; rows.push_back(r);
    };
    auto p = [&](const char* suffix) {
        if (has(suffix))
        {
            PanelRow r; r.paramName = species + suffix; rows.push_back(r);
        }
    };
    sec("Corpo e movimento");
    p("_body_size");
    p("_body_shape");
    p("_max_speed");
    p("_max_turn");
    p("_allow_reverse_locomotion");
    p("_movement_mode");
    sec("Energia e metabolismo");
    p("_initial_energy");
    p("_death_energy");
    p("_split_energy");
    p("_v0_cost");
    p("_vmax_cost");
    p("_energy_cap");
    p("_death_by_age_enabled");
    p("_death_age");
    p("_corpse_to_food");
    p("_reproduction_min_age");
    p("_reproduction_cooldown");
    sec("Visao");
    p("_vision_radius");
    p("_retina_count");
    p("_retina_fov_degrees");
    p("_eye_count");
    p("_eye_angle_degrees");
    p("_see_food");
    p("_see_agents");
    p("_see_predators");
    p("_see_obstacles");
    p("_see_through_walls");
    p("_retina_channel_r");
    p("_retina_channel_g");
    p("_retina_channel_b");
    p("_retina_channel_d");
    p("_retina_input_mode");
    sec("Dieta");
    p("_diet_food");
    p("_diet_agents");
    p("_diet_same_label");
    p("_food_efficiency");
    p("_agent_efficiency");
    sec("Rede neural");
    p("_hidden_layers");
    p("_mutation_rate");
    p("_mutation_strength");
    return rows;
}

std::vector<PanelRow> populacaoRows(const config::ParameterRegistry& reg)
{
    std::vector<PanelRow> rows;
    auto sec = [&](const char* l) {
        PanelRow r; r.isSection = true; r.sectionLabel = l; rows.push_back(r);
    };
    auto p = [&](const char* name) {
        if (reg.find(name) != nullptr)
        {
            PanelRow r; r.paramName = name; rows.push_back(r);
        }
    };
    sec("Quantidade inicial por especie");
    p("bacteria_count");
    p("predator_count");
    sec("Limites populacionais");
    p("bacteria_min_limit");
    p("bacteria_max_limit");
    p("predator_min_limit");
    p("predator_max_limit");
    p("predators_enabled");
    sec("Regras gerais");
    p("population_min_rescue_enabled");
    p("max_deaths_per_step");
    return rows;
}

std::vector<PanelRow> substratoRows(const config::ParameterRegistry& reg)
{
    std::vector<PanelRow> rows;
    auto sec = [&](const char* l) {
        PanelRow r; r.isSection = true; r.sectionLabel = l; rows.push_back(r);
    };
    auto p = [&](const char* name) {
        if (reg.find(name) != nullptr)
        {
            PanelRow r; r.paramName = name; rows.push_back(r);
        }
    };
    sec("Mundo / substrato");
    p("substrate_shape");
    p("world_w");
    p("world_h");
    p("substrate_radius");
    sec("Comida");
    p("food_mode");
    p("food_target");
    p("food_min_r");
    p("food_max_r");
    p("food_replenish_interval");
    p("food_color");
    sec("Comida em pedacos (chunk)");
    p("food_bite_seconds");
    p("food_piece_particle_radius");
    p("food_piece_cluster_radius");
    p("food_piece_particle_spacing");
    p("food_piece_replenish_mode");
    p("food_trim_max_per_step");
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
    const float valueX = x + w - 220.0F;
    const bool editing = state.editingParam == row.paramName;
    switch (def->type)
    {
    case config::ParameterType::Boolean:
    {
        const bool v = std::get<bool>(current);
        drawRoundedRect(target, valueX, y + 6.0F, 50.0F, kRowH - 14.0F, 6.0F,
                          v ? kAccent : kBgButton);
        drawText(target, font, v ? "on" : "off", valueX + 12.0F, y + 7.0F, 12U,
                   v ? sf::Color::Black : kTextLight);
        break;
    }
    case config::ParameterType::Integer:
    case config::ParameterType::Floating:
    {
        if (editing)
        {
            drawRoundedRect(target, valueX, y + 5.0F, 200.0F, kRowH - 12.0F, 5.0F,
                              sf::Color(30, 50, 70), kAccent, 1.6F);
            drawText(target, font, state.editingBuffer + "_",
                       valueX + 8.0F, y + 7.0F, 12U, kTextLight);
        }
        else
        {
            drawRoundedRect(target, valueX, y + 5.0F, 200.0F, kRowH - 12.0F, 5.0F, kBgButton);
            drawText(target, font, fmtValue(current),
                       valueX + 12.0F, y + 7.0F, 12U, kTextLight);
            drawText(target, font, "(editar)",
                       valueX + 150.0F, y + 8.0F, 10U, kTextDim);
        }
        break;
    }
    case config::ParameterType::String:
    {
        const std::string s = std::get<std::string>(current);
        const auto values = config::prefsEnumValuesFor(row.paramName);
        if (values.empty())
        {
            drawText(target, font, s, valueX, y + 7.0F, 12U, kTextDim);
        }
        else
        {
            drawRoundedRect(target, valueX, y + 5.0F, 200.0F, kRowH - 12.0F, 5.0F, kBgButton);
            drawText(target, font, s + "  v", valueX + 12.0F, y + 7.0F, 12U, kTextLight);
        }
        break;
    }
    case config::ParameterType::ColorRgb:
    {
        const auto c = std::get<config::ColorRgb>(current);
        drawRoundedRect(target, valueX, y + 5.0F, 40.0F, kRowH - 12.0F, 4.0F,
                          sf::Color(
                              static_cast<sf::Uint8>(std::clamp(c.r, 0, 255)),
                              static_cast<sf::Uint8>(std::clamp(c.g, 0, 255)),
                              static_cast<sf::Uint8>(std::clamp(c.b, 0, 255))),
                          kBorder, 1.0F);
        std::ostringstream s;
        s << "R" << c.r << " G" << c.g << " B" << c.b;
        drawText(target, font, s.str(), valueX + 50.0F, y + 8.0F, 11U, kTextLight);
        break;
    }
    }
}

// Phase 24: per-row click for value cell. Returns true if consumed.
bool handleParamRowClick(const PanelRow& row, const config::ParameterRegistry& reg,
                            const PreferencesState& state, float rx, float ry,
                            float w, CommandQueue& queue)
{
    if (row.isSection) return false;
    const auto* def = reg.find(row.paramName);
    if (def == nullptr) return false;
    const float valueX = w - 220.0F;
    if (rx < valueX) return true;  // label area inert
    const config::ParameterValue current = prefsEffectiveValue(reg, state, row.paramName);
    static_cast<void>(ry);
    switch (def->type)
    {
    case config::ParameterType::Boolean:
    {
        const bool cur = std::get<bool>(current);
        queue.push(CmdSetParameterValue{row.paramName, !cur});
        return true;
    }
    case config::ParameterType::Integer:
    case config::ParameterType::Floating:
    {
        queue.push(CmdBeginEditParameter{row.paramName, fmtValue(current)});
        return true;
    }
    case config::ParameterType::String:
    {
        const auto values = config::prefsEnumValuesFor(row.paramName);
        if (values.empty()) return true;
        queue.push(CmdOpenPrefsPopup{"enum:" + row.paramName});
        return true;
    }
    case config::ParameterType::ColorRgb:
    {
        queue.push(CmdOpenPrefsPopup{"color:" + row.paramName});
        return true;
    }
    }
    return true;
}

} // namespace

// ---------------- public static helpers ----------------

std::vector<std::string> UiOperationalPanels::editorParameters()
{
    // Phase 24: the same names the runtime registry registers for the bacteria
    // species. Used by the Fase 24 selftest to assert presence; not all may
    // exist in every build but the canonical set is documented here.
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
        "bacteria_see_food", "bacteria_see_agents", "bacteria_see_predators",
        "bacteria_see_obstacles", "bacteria_see_through_walls",
        "bacteria_retina_channel_r", "bacteria_retina_channel_g",
        "bacteria_retina_channel_b", "bacteria_retina_channel_d",
        "bacteria_retina_input_mode", "bacteria_diet_food",
        "bacteria_diet_agents", "bacteria_diet_same_label",
        "bacteria_food_efficiency", "bacteria_agent_efficiency",
        "bacteria_hidden_layers", "bacteria_mutation_rate",
        "bacteria_mutation_strength"
    };
}

std::vector<std::string> UiOperationalPanels::populacaoParameters()
{
    return {"bacteria_count", "predator_count", "bacteria_min_limit",
            "bacteria_max_limit", "predator_min_limit", "predator_max_limit",
            "predators_enabled", "population_min_rescue_enabled",
            "max_deaths_per_step"};
}

std::vector<std::string> UiOperationalPanels::substratoParameters()
{
    return {"substrate_shape", "world_w", "world_h", "substrate_radius",
            "food_mode", "food_target", "food_min_r", "food_max_r",
            "food_replenish_interval", "food_color", "food_bite_seconds",
            "food_piece_particle_radius", "food_piece_cluster_radius",
            "food_piece_particle_spacing", "food_piece_replenish_mode",
            "food_trim_max_per_step"};
}

// ---------------- geometry ----------------

UiOperationalPanels::Rect UiOperationalPanels::rectFor(
    const Which which, const sf::Vector2u vp,
    const PreferencesState& state) const noexcept
{
    Rect r;
    r.w = std::min(kWindowW, static_cast<float>(vp.x) - 40.0F);
    r.h = std::min(kWindowH, static_cast<float>(vp.y) - 100.0F);
    float defX = 120.0F + static_cast<float>(static_cast<int>(which)) * 30.0F;
    float defY = 100.0F + static_cast<float>(static_cast<int>(which)) * 24.0F;
    switch (which)
    {
    case Which::Editor:
        r.x = state.editorX >= 0.0F ? state.editorX : defX;
        r.y = state.editorY >= 0.0F ? state.editorY : defY;
        break;
    case Which::Especies:
        r.x = state.especiesX >= 0.0F ? state.especiesX : defX;
        r.y = state.especiesY >= 0.0F ? state.especiesY : defY;
        r.h = std::min(420.0F, r.h);
        break;
    case Which::Populacao:
        r.x = state.populacaoX >= 0.0F ? state.populacaoX : defX;
        r.y = state.populacaoY >= 0.0F ? state.populacaoY : defY;
        r.h = std::min(380.0F, r.h);
        break;
    case Which::Substrato:
        r.x = state.substratoX >= 0.0F ? state.substratoX : defX;
        r.y = state.substratoY >= 0.0F ? state.substratoY : defY;
        break;
    }
    if (r.x + r.w > static_cast<float>(vp.x)) r.x = static_cast<float>(vp.x) - r.w - 4.0F;
    if (r.y + r.h > static_cast<float>(vp.y)) r.y = static_cast<float>(vp.y) - r.h - 4.0F;
    r.x = std::max(0.0F, r.x);
    r.y = std::max(0.0F, r.y);
    return r;
}

bool UiOperationalPanels::pointInsideAnyWindow(
    const int sx, const int sy, const sf::Vector2u vp,
    const PreferencesState& state) const noexcept
{
    auto in = [sx, sy](const Rect& r) {
        return static_cast<float>(sx) >= r.x && static_cast<float>(sx) < r.x + r.w &&
                  static_cast<float>(sy) >= r.y && static_cast<float>(sy) < r.y + r.h;
    };
    if (state.editorOpen    && in(rectFor(Which::Editor,    vp, state))) return true;
    if (state.especiesOpen  && in(rectFor(Which::Especies,  vp, state))) return true;
    if (state.populacaoOpen && in(rectFor(Which::Populacao, vp, state))) return true;
    if (state.substratoOpen && in(rectFor(Which::Substrato, vp, state))) return true;
    return false;
}

// ---------------- mouse handlers ----------------

bool UiOperationalPanels::handleMouseWheel(
    const int sx, const int sy, const sf::Vector2u vp, const float delta,
    PreferencesState& state, CommandQueue& queue)
{
    const int step = static_cast<int>(delta * -3.0F);
    auto check = [&](Which w, bool open) {
        if (!open) return false;
        const Rect r = rectFor(w, vp, state);
        if (static_cast<float>(sx) >= r.x && static_cast<float>(sx) < r.x + r.w &&
            static_cast<float>(sy) >= r.y && static_cast<float>(sy) < r.y + r.h)
        {
            queue.push(CmdScrollOperationalWindow{static_cast<int>(w), step});
            return true;
        }
        return false;
    };
    return check(Which::Editor, state.editorOpen)
        || check(Which::Especies, state.especiesOpen)
        || check(Which::Populacao, state.populacaoOpen)
        || check(Which::Substrato, state.substratoOpen);
}

void UiOperationalPanels::handleMouseMove(
    const int sx, const int sy, const sf::Vector2u vp,
    PreferencesState& state, CommandQueue& queue)
{
    static_cast<void>(vp);
    if (state.draggingOperational >= 0)
    {
        queue.push(CmdMoveOperationalWindow{state.draggingOperational,
            static_cast<float>(sx) - state.dragOffsetX,
            static_cast<float>(sy) - state.dragOffsetY});
    }
}

void UiOperationalPanels::handleMouseRelease(
    const int, const int, const sf::Vector2u,
    PreferencesState& state, CommandQueue&)
{
    state.draggingOperational = -1;
}

bool UiOperationalPanels::handleMouseClick(
    const int sx, const int sy, const sf::Vector2u vp,
    const config::ParameterRegistry& registry, const sim::SimulationRunner& runner,
    PreferencesState& state, CommandQueue& queue)
{
    static_cast<void>(runner);
    const float frx = static_cast<float>(sx);
    const float fry = static_cast<float>(sy);

    auto handleWindow = [&](Which which, bool open,
                              const std::vector<PanelRow>& rows,
                              int scrollOffset,
                              const std::string& speciesPrefix,
                              const std::vector<const char*>& footerLabels,
                              const std::vector<Command>& footerCommands) -> bool {
        if (!open) return false;
        const Rect r = rectFor(which, vp, state);
        const float rx = frx - r.x;
        const float ry = fry - r.y;
        if (rx < 0.0F || ry < 0.0F || rx > r.w || ry > r.h) return false;

        // Close button.
        if (rx >= r.w - 28.0F && ry >= 5.0F && ry < 27.0F)
        {
            switch (which)
            {
            case Which::Editor:    queue.push(CmdCloseEditorGenetico{}); break;
            case Which::Especies:  queue.push(CmdCloseEspecies{});       break;
            case Which::Populacao: queue.push(CmdClosePopulacao{});      break;
            case Which::Substrato: queue.push(CmdCloseSubstrato{});      break;
            }
            return true;
        }
        // Drag from header.
        if (ry < kHeaderH && rx < r.w - 32.0F)
        {
            state.draggingOperational = static_cast<int>(which);
            state.dragOffsetX = rx;
            state.dragOffsetY = ry;
            return true;
        }
        // Footer.
        if (ry > r.h - kFooterH)
        {
            const float btnW = 130.0F;
            const float btnH = 28.0F;
            const float gap = 8.0F;
            const float btnY = r.h - kFooterH + (kFooterH - btnH) * 0.5F;
            for (std::size_t i = 0; i < footerLabels.size(); ++i)
            {
                const float bx = static_cast<float>(i) * (btnW + gap) + gap;
                if (ry >= btnY && ry < btnY + btnH && rx >= bx && rx < bx + btnW)
                {
                    queue.push(footerCommands[i]);
                    return true;
                }
            }
            return true;
        }
        // Parameter row click.
        const float listY0 = kHeaderH;
        if (ry < listY0) return true;
        const int rowIdx = static_cast<int>((ry - listY0) / kRowH) + scrollOffset;
        if (rowIdx < 0 || rowIdx >= static_cast<int>(rows.size())) return true;
        return handleParamRowClick(rows[static_cast<std::size_t>(rowIdx)],
            registry, state, rx, ry, r.w, queue);
    };

    // Editor Genetico — clicks for value cells + footer.
    {
        const auto rows = editorRowsFor("bacteria", registry);
        if (handleWindow(Which::Editor, state.editorOpen, rows, state.editorScroll,
                           "bacteria",
                           {"Aplicar a especie", "Aplicar selecionados", "Reverter", "Defaults"},
                           {ui::CmdApplyGenomeToSpecies{},
                            ui::CmdApplyGenomeToSelected{},
                            ui::CmdRevertPreferences{},
                            ui::CmdRestoreDefaultsAndApply{}}))
            return true;
    }
    // Especies: handled below (different content - list of species, not params).
    if (state.especiesOpen)
    {
        const Rect r = rectFor(Which::Especies, vp, state);
        const float rx = frx - r.x;
        const float ry = fry - r.y;
        if (rx >= 0.0F && rx < r.w && ry >= 0.0F && ry < r.h)
        {
            if (rx >= r.w - 28.0F && ry >= 5.0F && ry < 27.0F)
            {
                queue.push(CmdCloseEspecies{});
                return true;
            }
            if (ry < kHeaderH && rx < r.w - 32.0F)
            {
                state.draggingOperational = static_cast<int>(Which::Especies);
                state.dragOffsetX = rx;
                state.dragOffsetY = ry;
                return true;
            }
            // Footer with 2 buttons: Criar nova com selecionados / Atribuir
            if (ry > r.h - kFooterH)
            {
                const float btnW = 200.0F;
                const float btnH = 28.0F;
                const float gap = 10.0F;
                const float btnY = r.h - kFooterH + (kFooterH - btnH) * 0.5F;
                if (ry >= btnY && ry < btnY + btnH)
                {
                    if (rx >= gap && rx < gap + btnW)
                    {
                        queue.push(CmdCreateSpeciesFromSelected{});
                        return true;
                    }
                    if (rx >= gap + btnW + gap && rx < gap + btnW + gap + btnW)
                    {
                        // Default to assigning to species index 0
                        const auto& sp = runner.species();
                        if (sp.size() > 0U)
                        {
                            queue.push(CmdAssignSelectedToSpecies{
                                static_cast<std::uint32_t>(static_cast<std::uint32_t>(sp.records()[0].id))});
                        }
                        return true;
                    }
                }
                return true;
            }
            // Row: each species row has 3 mini-buttons (Selecionar / Reset rede / x)
            const float listY0 = kHeaderH;
            const float speciesRowH = 56.0F;
            const int rowIdx = static_cast<int>((ry - listY0) / speciesRowH) +
                state.especiesScroll;
            if (rowIdx >= 0 && rowIdx < static_cast<int>(runner.species().size()))
            {
                const auto& sp = runner.species();
                const auto id = sp.records()[static_cast<std::size_t>(rowIdx)].id;
                const float btnsX = r.w - 320.0F;
                const float btnW = 90.0F;
                const float btnH = 24.0F;
                const float btnY2 = listY0 + static_cast<float>(rowIdx -
                    state.especiesScroll) * speciesRowH + 16.0F;
                if (ry >= btnY2 && ry < btnY2 + btnH)
                {
                    if (rx >= btnsX && rx < btnsX + btnW)
                    {
                        queue.push(CmdSelectAllOfSpecies{
                            static_cast<std::uint32_t>(id)});
                        return true;
                    }
                    if (rx >= btnsX + btnW + 10.0F && rx < btnsX + 2.0F * btnW + 10.0F)
                    {
                        queue.push(CmdResetNeuralForSpecies{
                            static_cast<std::uint32_t>(id)});
                        return true;
                    }
                    if (rx >= btnsX + 2.0F * btnW + 20.0F && rx < btnsX + 3.0F * btnW + 20.0F)
                    {
                        queue.push(CmdAssignSelectedToSpecies{
                            static_cast<std::uint32_t>(id)});
                        return true;
                    }
                }
            }
            return true;
        }
    }
    {
        const auto rows = populacaoRows(registry);
        if (handleWindow(Which::Populacao, state.populacaoOpen, rows,
                           state.populacaoScroll, "",
                           {"Aplicar populacao", "Reverter", "Defaults"},
                           {ui::CmdApplyPopulation{}, ui::CmdRevertPreferences{},
                            ui::CmdRestoreDefaultsAndApply{}}))
            return true;
    }
    {
        const auto rows = substratoRows(registry);
        if (handleWindow(Which::Substrato, state.substratoOpen, rows,
                           state.substratoScroll, "",
                           {"Aplicar ambiente", "Limpar comida", "Reverter", "Defaults"},
                           {ui::CmdApplyEnvironment{}, ui::CmdClearAllFood{},
                            ui::CmdRevertPreferences{}, ui::CmdRestoreDefaultsAndApply{}}))
            return true;
    }
    return false;
}

// ---------------- drawing ----------------

void UiOperationalPanels::draw(sf::RenderTarget& target,
                                  const config::ParameterRegistry& registry,
                                  const sim::SimulationRunner& runner,
                                  const PreferencesState& state) const
{
    const sf::Vector2u vp = target.getSize();

    auto drawWindow = [&](Which which, bool open, const char* title,
                            const std::vector<PanelRow>& rows, int scrollOffset,
                            const std::string& speciesPrefix,
                            const std::vector<const char*>& footerLabels,
                            const std::vector<sf::Color>& footerColors) {
        if (!open) return;
        const Rect r = rectFor(which, vp, state);
        drawRoundedRect(target, r.x, r.y, r.w, r.h, 8.0F, kBg, kBorder, 1.4F);
        drawWindowHeader(target, font_, r.x, r.y, r.w, title);

        const float listH = r.h - kHeaderH - kFooterH;
        const int visible = static_cast<int>(listH / kRowH);
        const int first = std::max(0,
            std::min(scrollOffset, static_cast<int>(rows.size()) - visible));
        for (int i = 0; i < visible; ++i)
        {
            const int rowIdx = first + i;
            if (rowIdx >= static_cast<int>(rows.size())) break;
            const float y = r.y + kHeaderH + static_cast<float>(i) * kRowH;
            drawParamRow(target, font_, rows[static_cast<std::size_t>(rowIdx)],
                           registry, state, r.x, y, r.w - kScrollbarW, speciesPrefix);
        }
        drawScrollbar(target, r.x + r.w - kScrollbarW, r.y + kHeaderH,
                        listH, static_cast<int>(rows.size()), visible, first);

        const float footerY = r.y + r.h - kFooterH;
        sf::RectangleShape footer({r.w, kFooterH});
        footer.setPosition(r.x, footerY);
        footer.setFillColor(kBgHeader);
        target.draw(footer);
        const float btnW = 130.0F;
        const float btnH = 28.0F;
        const float gap = 8.0F;
        const float btnY = footerY + (kFooterH - btnH) * 0.5F;
        for (std::size_t i = 0; i < footerLabels.size(); ++i)
        {
            const float x = r.x + gap + static_cast<float>(i) * (btnW + gap);
            drawRoundedRect(target, x, btnY, btnW, btnH, 8.0F, footerColors[i]);
            drawText(target, font_, footerLabels[i], x + 8.0F, btnY + 7.0F, 12U,
                       sf::Color::White);
        }
    };

    // Editor Genético
    if (state.editorOpen)
    {
        drawWindow(Which::Editor, true, "Editor Genetico (bacteria)",
                     editorRowsFor("bacteria", registry), state.editorScroll, "bacteria",
                     {"Aplicar a especie", "Aplicar selecionados", "Reverter", "Defaults"},
                     {sf::Color(80, 150, 90), sf::Color(120, 130, 200),
                      sf::Color(150, 90, 60), sf::Color(80, 90, 130)});
    }

    // Especies window (custom content)
    if (state.especiesOpen)
    {
        const Rect r = rectFor(Which::Especies, vp, state);
        drawRoundedRect(target, r.x, r.y, r.w, r.h, 8.0F, kBg, kBorder, 1.4F);
        drawWindowHeader(target, font_, r.x, r.y, r.w, "Especies / Labels");
        const float listH = r.h - kHeaderH - kFooterH;
        const float speciesRowH = 56.0F;
        const int visible = static_cast<int>(listH / speciesRowH);
        const auto& sp = runner.species();
        const int first = std::max(0,
            std::min(state.especiesScroll, static_cast<int>(sp.size()) - visible));
        for (int i = 0; i < visible; ++i)
        {
            const int rowIdx = first + i;
            if (rowIdx >= static_cast<int>(sp.size())) break;
            const float y = r.y + kHeaderH + static_cast<float>(i) * speciesRowH;
            const auto idx = static_cast<std::size_t>(rowIdx);
            sf::RectangleShape bg({r.w - kScrollbarW, speciesRowH - 2.0F});
            bg.setPosition(r.x, y);
            bg.setFillColor(i % 2 == 0 ? kBgRow : kBgRowAlt);
            target.draw(bg);
            const auto& rec = sp.records()[idx];
            const auto c = rec.color;
            drawRoundedRect(target, r.x + 8.0F, y + 8.0F, 28.0F, 28.0F, 4.0F,
                              sf::Color(c.r, c.g, c.b), kBorder, 1.0F);
            drawText(target, font_, rec.label.empty() ? rec.name : rec.label,
                       r.x + 44.0F, y + 6.0F, 14U, kTextLight);
            std::ostringstream info;
            info << "limite [" << rec.minPopulation << "-" << rec.maxPopulation << "]";
            drawText(target, font_, info.str(),
                       r.x + 44.0F, y + 30.0F, 11U, kTextDim);
            // 3 mini buttons on the right: Selecionar / Reset rede / Atribuir
            const float btnsX = r.x + r.w - 320.0F;
            const float btnW = 90.0F;
            const float btnH = 24.0F;
            const float btnY = y + 16.0F;
            drawRoundedRect(target, btnsX, btnY, btnW, btnH, 6.0F, sf::Color(70, 130, 180));
            drawText(target, font_, "Selecionar", btnsX + 6.0F, btnY + 5.0F, 11U,
                       sf::Color::White);
            drawRoundedRect(target, btnsX + btnW + 10.0F, btnY, btnW, btnH, 6.0F,
                              sf::Color(180, 100, 60));
            drawText(target, font_, "Reset rede", btnsX + btnW + 16.0F, btnY + 5.0F, 11U,
                       sf::Color::White);
            drawRoundedRect(target, btnsX + 2.0F * btnW + 20.0F, btnY, btnW, btnH, 6.0F,
                              sf::Color(80, 150, 90));
            drawText(target, font_, "Atribuir sel.", btnsX + 2.0F * btnW + 26.0F,
                       btnY + 5.0F, 11U, sf::Color::White);
        }
        drawScrollbar(target, r.x + r.w - kScrollbarW, r.y + kHeaderH,
                        listH, static_cast<int>(sp.size()), visible, first);
        // Footer
        const float footerY = r.y + r.h - kFooterH;
        sf::RectangleShape footer({r.w, kFooterH});
        footer.setPosition(r.x, footerY);
        footer.setFillColor(kBgHeader);
        target.draw(footer);
        const float btnW = 200.0F;
        const float btnH = 28.0F;
        const float gap = 10.0F;
        const float btnY = footerY + (kFooterH - btnH) * 0.5F;
        drawRoundedRect(target, r.x + gap, btnY, btnW, btnH, 8.0F,
                          sf::Color(80, 150, 90));
        drawText(target, font_, "Nova com selecionados",
                   r.x + gap + 16.0F, btnY + 7.0F, 12U, sf::Color::White);
        drawRoundedRect(target, r.x + gap + btnW + gap, btnY, btnW, btnH, 8.0F,
                          sf::Color(120, 130, 200));
        drawText(target, font_, "Atribuir aos selecionados",
                   r.x + gap + btnW + gap + 8.0F, btnY + 7.0F, 12U, sf::Color::White);
    }

    if (state.populacaoOpen)
    {
        drawWindow(Which::Populacao, true, "Populacao",
                     populacaoRows(registry), state.populacaoScroll, "",
                     {"Aplicar populacao", "Reverter", "Defaults"},
                     {sf::Color(80, 150, 90), sf::Color(150, 90, 60),
                      sf::Color(80, 90, 130)});
    }
    if (state.substratoOpen)
    {
        drawWindow(Which::Substrato, true, "Substrato e Comida",
                     substratoRows(registry), state.substratoScroll, "",
                     {"Aplicar ambiente", "Limpar comida", "Reverter", "Defaults"},
                     {sf::Color(80, 150, 90), sf::Color(180, 100, 60),
                      sf::Color(150, 90, 60), sf::Color(80, 90, 130)});
    }
}
} // namespace agentbiosim::ui
