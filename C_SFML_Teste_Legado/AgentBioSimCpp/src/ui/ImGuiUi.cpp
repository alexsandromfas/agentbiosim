#include "ui/ImGuiUi.hpp"

#include "config/Parameter.hpp"
#include "config/ParameterMetadata.hpp"
#include "config/ParameterHelpers.hpp"
#include "core/AssetPath.hpp"
#include "core/Profiler.hpp"
#include "i18n/Locale.hpp"
#include "neural/BrainType.hpp"
#include "neural/NeuralView.hpp"
#include "sim/SimulationRunner.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpeciesStore.hpp"
#include "systems/MetricsSystem.hpp"
#include "ui/ImGuiTheme.hpp"
#include "ui/UiLeftDock.hpp"          // editorParameters()/substratoParameters() (model)
#include "ui/UiPreferencesPanel.hpp"  // prefs* model free functions

#include <imgui.h>
#include <imgui-SFML.h>
#include <imgui_stdlib.h>

#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace agentbiosim::ui
{
// Phase 25.2: `tr(ptbr, en)` resolves to the string for the active UI language.
// Brought into scope so the inline UI strings below read tr("Arquivo", "File").
using i18n::tr;

namespace
{
// Phase 25.2: canvas tool labels localized at the call site. The stable lowercase
// id stays in core::canvasToolName; this is purely the display label, so it lives
// in the UI layer and never pollutes core with i18n.
const char* toolLabel(const core::CanvasTool t) noexcept
{
    switch (t)
    {
    case core::CanvasTool::Select:          return tr("Selecao", "Select");
    case core::CanvasTool::RectangleSelect: return tr("Rect", "Rect");
    case core::CanvasTool::LassoSelect:     return tr("Laco", "Lasso");
    case core::CanvasTool::AddFood:         return tr("Comida", "Food");
    case core::CanvasTool::AddAgent:        return tr("Agente", "Agent");
    case core::CanvasTool::PaintObstacle:   return tr("Pincel", "Brush");
    case core::CanvasTool::EraseObstacle:   return tr("Apagar", "Erase");
    case core::CanvasTool::Move:            return tr("Mover", "Move");
    case core::CanvasTool::Delete:          return tr("Excluir", "Delete");
    case core::CanvasTool::None:
    case core::CanvasTool::Pan:             break;
    }
    return core::canvasToolLabel(t);
}
// Reset overhaul: true when a brain-architecture change (network type / topology /
// hidden layers) is sitting unapplied in the preferences edit buffer. The neural
// warning and the "brains rebuilt" apply-feedback only appear when one of these is
// dirty — never just because the user opened the tab.
bool hasPendingNeuralArchChange(const PreferencesState& prefs)
{
    static constexpr const char* kKeys[] = {
        "neural_network_type",
        "neural_neat_initial_topology",
        "neural_proto_neat_initial_topology",
        "neural_recurrent_neat_initial_topology",
        "bacteria_hidden_layers",
    };
    for (const char* k : kKeys)
    {
        if (prefs.pendingValues.count(k) > 0U) return true;
    }
    return false;
}

// --- value extraction from the effective ParameterValue ----------------------
bool asBool(const config::ParameterValue& v)
{
    if (const auto* p = std::get_if<bool>(&v)) return *p;
    if (const auto* p = std::get_if<int>(&v)) return *p != 0;
    return false;
}
int asInt(const config::ParameterValue& v)
{
    if (const auto* p = std::get_if<int>(&v)) return *p;
    if (const auto* p = std::get_if<double>(&v)) return static_cast<int>(*p);
    if (const auto* p = std::get_if<bool>(&v)) return *p ? 1 : 0;
    return 0;
}
double asDouble(const config::ParameterValue& v)
{
    if (const auto* p = std::get_if<double>(&v)) return *p;
    if (const auto* p = std::get_if<int>(&v)) return static_cast<double>(*p);
    return 0.0;
}
std::string asString(const config::ParameterValue& v)
{
    if (const auto* p = std::get_if<std::string>(&v)) return *p;
    return std::string{};
}
config::ColorRgb asColor(const config::ParameterValue& v)
{
    if (const auto* p = std::get_if<config::ColorRgb>(&v)) return *p;
    return config::ColorRgb{0, 0, 0};
}

// Friendly PT-BR label: direct map, then species-suffix map, then raw name.
std::string friendlyLabelFor(const std::string& name)
{
    if (const char* f = config::prefsFriendlyLabel(name)) return f;
    static const char* kPrefixes[] = {"bacteria_", "predator_"};
    for (const char* p : kPrefixes)
    {
        const std::string pre = p;
        if (name.size() > pre.size() && name.compare(0, pre.size(), pre) == 0)
        {
            const std::string suffix = name.substr(pre.size());
            if (const char* s = config::prefsFriendlyLabelBySuffix(suffix)) return s;
        }
    }
    return name;
}

// One parameter row inside a 2-column table (col 0 = label, col 1 = control).
// Emits CmdSetParameterValue on change (queues a pending edit; Apply commits).
// Phase 25.1: numeric params are plain TEXT BOXES (no +/- step buttons); enums
// are dropdowns showing PT-BR labels (canonical value stored); floats use a
// per-parameter number of decimals; the control fills the fixed narrow column.
void drawParamRow(const config::ParameterRegistry& reg, const PreferencesState& prefs,
                  const std::string& name, core::CommandQueue& queue)
{
    const auto* def = reg.find(name);
    if (def == nullptr) return;
    const config::ParameterValue eff = prefsEffectiveValue(reg, prefs, name);
    const std::string label = friendlyLabelFor(name);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label.c_str());
    if (ImGui::IsItemHovered())
    {
        // Phase 25.2: prefer the curated, localized help; fall back to the
        // registry's terse raw description when no curated entry exists.
        const char* help = config::prefsParameterHelp(name);
        const char* tip = help != nullptr
            ? help
            : (def->description.empty() ? nullptr : def->description.c_str());
        if (tip != nullptr) ImGui::SetTooltip("%s", tip);
    }

    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(name.c_str());
    ImGui::SetNextItemWidth(-1.0F);

    switch (def->type)
    {
    case config::ParameterType::Boolean:
    {
        bool b = asBool(eff);
        if (ImGui::Checkbox("##v", &b))
        {
            queue.push(core::CmdSetParameterValue{name, b});
        }
        break;
    }
    case config::ParameterType::Integer:
    {
        int v = asInt(eff);
        ImGui::InputInt("##v", &v, 0, 0);  // step=0 -> plain text box, no +/- buttons
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            queue.push(core::CmdSetParameterValue{name, v});
        }
        break;
    }
    case config::ParameterType::Floating:
    {
        float f = static_cast<float>(asDouble(eff));
        char fmt[8];
        std::snprintf(fmt, sizeof(fmt), "%%.%df", config::prefsDecimalsFor(name));
        ImGui::InputFloat("##v", &f, 0.0F, 0.0F, fmt);  // no +/- buttons
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            queue.push(core::CmdSetParameterValue{name, static_cast<double>(f)});
        }
        break;
    }
    case config::ParameterType::String:
    {
        const auto options = config::prefsEnumValuesFor(name);
        const std::string cur = asString(eff);
        if (!options.empty())
        {
            const std::string curLabel = config::prefsEnumDisplayLabel(name, cur);
            if (ImGui::BeginCombo("##v", curLabel.c_str()))
            {
                for (const auto& opt : options)
                {
                    const bool sel = (opt == cur);
                    const std::string optLabel = config::prefsEnumDisplayLabel(name, opt);
                    if (ImGui::Selectable(optLabel.c_str(), sel))
                    {
                        // ui_language is applied instantly and in isolation by the
                        // App command handler (Phase 25.2), so it needs no separate
                        // Apply; every other enum stays a pending edit as usual.
                        queue.push(core::CmdSetParameterValue{name, opt});
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
        {
            // Non-enum strings are shown read-only (most are hidden by the model).
            std::string buf = cur;
            ImGui::InputText("##v", &buf, ImGuiInputTextFlags_ReadOnly);
        }
        break;
    }
    case config::ParameterType::ColorRgb:
    {
        const config::ColorRgb cc = asColor(eff);
        float col[3] = {static_cast<float>(cc.r) / 255.0F,
                        static_cast<float>(cc.g) / 255.0F,
                        static_cast<float>(cc.b) / 255.0F};
        if (ImGui::ColorEdit3("##v", col, ImGuiColorEditFlags_NoInputs))
        {
            const config::ColorRgb out{static_cast<int>(col[0] * 255.0F + 0.5F),
                                        static_cast<int>(col[1] * 255.0F + 0.5F),
                                        static_cast<int>(col[2] * 255.0F + 0.5F)};
            queue.push(core::CmdSetParameterValue{name, out});
        }
        break;
    }
    }
    ImGui::PopID();
}

// Render a list of parameters as a 2-column table: label (stretch) + value
// (fixed, narrow ~1/3 width). Keeps controls compact and avoids the
// label/control overlap seen in the Performance window.
void drawParamTable(const char* tableId, const config::ParameterRegistry& reg,
                    const PreferencesState& prefs, const std::vector<std::string>& names,
                    core::CommandQueue& queue)
{
    if (names.empty()) return;
    if (ImGui::BeginTable(tableId, 2, ImGuiTableFlags_PadOuterX))
    {
        ImGui::TableSetupColumn("p", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthFixed, 148.0F);
        for (const auto& n : names)
        {
            drawParamRow(reg, prefs, n, queue);
        }
        ImGui::EndTable();
    }
}

// Phase 25.3: square icon button. When `active`, the frame is tinted with the
// theme accent (mirrors the old text toolButton). A short hover delay reveals a
// tooltip. If the icon texture is missing the button falls back to a compact
// text label so the toolbar still works. `id` keeps the ImGui id stable and
// unique independently of the (localized) tooltip.
constexpr float kToolIconSize = 24.0F;

bool iconButton(const sf::Texture* tex, const char* id, const char* tooltip,
                bool active, const char* fallbackLabel)
{
    ImGui::PushID(id);
    int pushed = 0;
    if (active)
    {
        const ThemeAccent a = themeAccent();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(a.r, a.g, a.b, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(a.r, a.g, a.b, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(a.r, a.g, a.b, 1.0F));
        pushed = 3;
    }
    bool clicked = false;
    if (tex != nullptr)
    {
        clicked = ImGui::ImageButton(*tex, sf::Vector2f(kToolIconSize, kToolIconSize), 4);
    }
    else
    {
        clicked = ImGui::Button(fallbackLabel, ImVec2(kToolIconSize + 12.0F, kToolIconSize + 8.0F));
    }
    if (pushed) ImGui::PopStyleColor(pushed);
    if (tooltip != nullptr && tooltip[0] != '\0')
    {
        ImGui::SetItemTooltip("%s", tooltip);
    }
    ImGui::PopID();
    return clicked;
}

// Labels tab: one block per enabled species — swatch, editable name, count,
// graph toggle, min/max/initial spinners and action buttons.
void drawLabelsTab(const config::ParameterRegistry& reg, const sim::SimulationRunner& runner,
                   const PreferencesState& prefs, core::CommandQueue& queue)
{
    ImGui::BeginChild("##labelsscroll", ImVec2(0.0F, -84.0F));
    const auto& records = runner.species().records();
    for (const auto& rec : records)
    {
        if (!rec.enabled) continue;
        const std::uint32_t sid = static_cast<std::uint32_t>(rec.id);
        ImGui::PushID(static_cast<int>(sid));

        float col[3] = {static_cast<float>(rec.color.r) / 255.0F,
                        static_cast<float>(rec.color.g) / 255.0F,
                        static_cast<float>(rec.color.b) / 255.0F};
        if (ImGui::ColorEdit3("##color", col, ImGuiColorEditFlags_NoInputs))
        {
            queue.push(core::CmdSetSpeciesColor{sid,
                static_cast<int>(col[0] * 255.0F + 0.5F),
                static_cast<int>(col[1] * 255.0F + 0.5F),
                static_cast<int>(col[2] * 255.0F + 0.5F)});
        }
        ImGui::SameLine();
        std::string nameBuf = rec.label.empty() ? rec.name : rec.label;
        ImGui::SetNextItemWidth(-1.0F);
        ImGui::InputText("##name", &nameBuf);
        if (ImGui::IsItemDeactivatedAfterEdit() && !nameBuf.empty())
        {
            queue.push(core::CmdSetSpeciesLabel{sid, nameBuf});
        }

        ImGui::TextDisabled(tr("%zu individuos", "%zu individuals"),
                            runner.countAgentsOfSpecies(rec.id));

        bool sg = rec.showGraph;
        if (ImGui::Checkbox(tr("Grafico", "Graph"), &sg))
        {
            queue.push(core::CmdSetSpeciesShowGraph{sid, sg});
        }

        // Phase 25.1: Min / Max / Inicial as plain text boxes (no +/- buttons).
        // The runner command takes a delta, so commit new-minus-current.
        const auto popField = [&](const char* lbl, int field, int value) {
            ImGui::PushID(field);
            ImGui::SetNextItemWidth(64.0F);
            int v = value;
            ImGui::InputInt(lbl, &v, 0, 0);
            if (ImGui::IsItemDeactivatedAfterEdit() && v != value)
            {
                queue.push(core::CmdAdjustSpeciesPop{sid, field, v - value});
            }
            ImGui::PopID();
        };
        popField(tr("Min", "Min"), 0, rec.minPopulation);
        ImGui::SameLine();
        popField(tr("Max", "Max"), 1, rec.maxPopulation);
        ImGui::SameLine();
        popField(tr("Ini", "Init"), 2, rec.initialCount);

        if (ImGui::Button(tr("Selecionar", "Select"))) queue.push(core::CmdSelectAllOfSpecies{sid});
        ImGui::SameLine();
        if (ImGui::Button(tr("Atribuir selecao", "Assign selection"))) queue.push(core::CmdAssignSelectedToSpecies{sid});
        ImGui::SameLine();
        if (ImGui::Button(tr("Remover selecao", "Remove selection"))) queue.push(core::CmdRemoveSelectedFromSpecies{sid});

        // Phase 31: per-species neural reset is live (drops the species' brains;
        // fresh ones are recreated deterministically on the next step).
        if (ImGui::Button(tr("Resetar rede", "Reset network")))
        {
            queue.push(core::CmdResetNeuralForSpecies{sid});
        }
        ImGui::SetItemTooltip("%s", tr("Substitui os cerebros desta especie por redes novas aleatorias.",
                                       "Replaces this species' brains with fresh random networks."));
        ImGui::SameLine();
        if (ImGui::Button(tr("Excluir", "Delete"))) queue.push(core::CmdRemoveSpecies{sid});

        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Separator();
    {
        const auto eff = prefsEffectiveValue(reg, prefs, "population_min_rescue_enabled");
        bool resc = asBool(eff);
        if (ImGui::Checkbox(tr("Resgate de populacao minima", "Minimum population rescue"), &resc))
        {
            queue.push(core::CmdSetParameterValue{"population_min_rescue_enabled", resc});
        }
    }
    if (ImGui::Button(tr("+ Nova label com os selecionados", "+ New label from selected")))
    {
        queue.push(core::CmdCreateSpeciesFromSelected{});
    }
}
// --- Phase 26: neural viewer rendering -------------------------------------
// Diverging fill for a node's activation: dark when ~0, green for positive,
// red for negative (mirrors the Python neural_viewer palette).
ImU32 activationColor(const double a)
{
    const float t = std::clamp(static_cast<float>(a), -1.0F, 1.0F);
    const float m = std::abs(t);
    const ImVec4 mid(0.16F, 0.20F, 0.26F, 1.0F);
    const ImVec4 pos(0.37F, 0.83F, 0.59F, 1.0F);
    const ImVec4 neg(0.92F, 0.41F, 0.49F, 1.0F);
    const ImVec4& c = (t >= 0.0F) ? pos : neg;
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(mid.x + (c.x - mid.x) * m, mid.y + (c.y - mid.y) * m,
               mid.z + (c.z - mid.z) * m, 1.0F));
}

// Edge color by weight sign; alpha grows with magnitude.
ImU32 weightColor(const double w)
{
    const float m = std::clamp(static_cast<float>(std::abs(w)), 0.0F, 1.5F) / 1.5F;
    const ImVec4 pos(0.37F, 0.83F, 0.59F, 1.0F);
    const ImVec4 neg(0.92F, 0.41F, 0.49F, 1.0F);
    const ImVec4& c = (w >= 0.0) ? pos : neg;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, 0.14F + 0.55F * m));
}

const char* neuralTypeLabel(const neural::BrainType type)
{
    switch (type)
    {
    case neural::BrainType::Mlp:           return tr("MLP padrao", "Standard MLP");
    case neural::BrainType::GatedMlp:      return tr("MLP com portao", "Gated MLP");
    case neural::BrainType::ShortcutMlp:   return tr("MLP com atalho", "Shortcut MLP");
    case neural::BrainType::ModulatedMlp:  return tr("MLP modulada", "Modulated MLP");
    case neural::BrainType::SimpleRnn:     return tr("RNN simples", "Simple RNN");
    case neural::BrainType::Neat:          return tr("NEAT comum", "Common NEAT");
    case neural::BrainType::SimpleNeat:    return tr("NEAT simplificada", "Simplified NEAT");
    case neural::BrainType::RecurrentNeat: return tr("NEAT recorrente", "Recurrent NEAT");
    }
    return "MLP";
}

// Draw the network graph into a (scrollable when fixed) canvas child filling the
// available region. Nodes are positioned by column/row; edges colored by weight.
void drawNeuralCanvas(const neural::NeuralView& view, const bool fill)
{
    const int columns = std::max(1, view.columnCount);
    std::vector<int> rowsPerCol(static_cast<std::size_t>(columns), 1);
    for (const auto& n : view.nodes)
    {
        if (n.column >= 0 && n.column < columns)
        {
            rowsPerCol[static_cast<std::size_t>(n.column)] =
                std::max(rowsPerCol[static_cast<std::size_t>(n.column)], n.row + 1);
        }
    }
    int maxRows = 1;
    for (const int r : rowsPerCol) maxRows = std::max(maxRows, r);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(avail.x, 80.0F);
    avail.y = std::max(avail.y, 80.0F);

    float colSpacing = 0.0F;
    float contentW = avail.x;
    float contentH = avail.y;
    if (fill)
    {
        colSpacing = (columns > 1) ? (contentW - 40.0F) / static_cast<float>(columns - 1) : 0.0F;
    }
    else
    {
        colSpacing = 120.0F;
        const float rowSpacing = 26.0F;
        contentW = std::max(avail.x, 40.0F + colSpacing * static_cast<float>(columns - 1));
        contentH = std::max(avail.y, 24.0F + rowSpacing * static_cast<float>(maxRows));
    }
    const float radius = std::clamp(std::min(colSpacing * 0.16F, (contentH / static_cast<float>(maxRows)) * 0.4F), 3.0F, 9.0F);

    ImGui::BeginChild("##neuralcanvas", avail, false,
                      fill ? ImGuiWindowFlags_None : ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + contentW, origin.y + contentH), IM_COL32(14, 18, 24, 255));

    const auto nodePos = [&](const neural::NeuralViewNode& n) -> ImVec2 {
        const int col = std::clamp(n.column, 0, columns - 1);
        const float x = origin.x + 20.0F +
                        (columns > 1 ? static_cast<float>(col) * colSpacing : contentW * 0.5F);
        const int rc = std::max(1, rowsPerCol[static_cast<std::size_t>(col)]);
        const float colH = contentH - 20.0F;
        const float y = origin.y + 10.0F + (static_cast<float>(n.row) + 0.5F) * (colH / static_cast<float>(rc));
        return ImVec2(x, y);
    };

    const int nodeCount = static_cast<int>(view.nodes.size());
    for (const auto& e : view.edges)
    {
        if (!e.enabled) continue;
        if (e.fromNode < 0 || e.fromNode >= nodeCount || e.toNode < 0 || e.toNode >= nodeCount) continue;
        const ImVec2 a = nodePos(view.nodes[static_cast<std::size_t>(e.fromNode)]);
        const ImVec2 b = nodePos(view.nodes[static_cast<std::size_t>(e.toNode)]);
        const float thickness = e.recurrent
            ? 1.6F
            : 1.0F + std::clamp(static_cast<float>(std::abs(e.weight)), 0.0F, 2.0F);
        const ImU32 col = e.recurrent ? IM_COL32(242, 181, 87, 170) : weightColor(e.weight);
        dl->AddLine(a, b, col, thickness);
    }
    for (const auto& n : view.nodes)
    {
        const ImVec2 p = nodePos(n);
        dl->AddCircleFilled(p, radius, activationColor(n.activation));
        const ImU32 ring = n.kind == 0 ? IM_COL32(105, 151, 255, 255)
                          : n.kind == 2 ? IM_COL32(242, 181, 87, 255)
                                        : IM_COL32(96, 116, 136, 255);
        dl->AddCircle(p, radius, ring, 0, 1.5F);
    }

    ImGui::Dummy(ImVec2(contentW, contentH));
    ImGui::EndChild();
}

// Compact strip of the RNN recurrent state (one bar per state unit, signed).
void drawRecurrentStrip(const neural::NeuralView& view)
{
    if (!view.hasRecurrentState || view.recurrentState.empty()) return;
    ImGui::TextDisabled("%s", tr("Estado recorrente", "Recurrent state"));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = std::max(80.0F, ImGui::GetContentRegionAvail().x);
    const float height = 22.0F;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(14, 18, 24, 255));
    const std::size_t count = view.recurrentState.size();
    const float midY = origin.y + height * 0.5F;
    dl->AddLine(ImVec2(origin.x, midY), ImVec2(origin.x + width, midY), IM_COL32(60, 72, 88, 255), 1.0F);
    const float barW = width / static_cast<float>(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const float t = std::clamp(static_cast<float>(view.recurrentState[i]), -1.0F, 1.0F);
        const float x0 = origin.x + static_cast<float>(i) * barW;
        const float h = (height * 0.5F - 2.0F) * std::abs(t);
        const ImVec2 a(x0 + 0.5F, t >= 0.0F ? midY - h : midY);
        const ImVec2 b(x0 + barW - 0.5F, t >= 0.0F ? midY : midY + h);
        dl->AddRectFilled(a, b, activationColor(t));
    }
    ImGui::Dummy(ImVec2(width, height));
}
} // namespace

void ImGuiUi::loadIcons()
{
    if (iconsLoaded_) return;
    iconsLoaded_ = true;  // attempt once; missing files fall back to text labels

    // key -> file in Assets/novos_icones/. The key is the stable name the
    // toolbar uses; the file is whatever the artist named it.
    static const struct { const char* key; const char* file; } kIconFiles[] = {
        {"pause",    "pause.png"},
        {"play",     "play.png"},
        {"reset",    "stop.png"},
        {"select",   "Selection.png"},
        {"rect",     "Square.png"},
        {"lasso",    "Lasso.png"},
        {"food",     "food.png"},
        {"agent",    "icon.png"},
        {"brush",    "lapis.png"},
        {"erase",    "limpar.png"},
        {"move",     "move.png"},
        {"delete",   "lixeira.png"},
        {"fit",      "fit.png"},
        {"obstacle", "obstaculo.png"},
    };

    const std::string exeDir = core::executableDir();
    for (const auto& entry : kIconFiles)
    {
        const std::string rel = std::string("Assets/novos_icones/") + entry.file;
        // Try next to the executable first (CMake copies Assets there), then the
        // current working directory as a fallback for ad-hoc launches.
        const std::string candidates[] = {exeDir + rel, rel};
        for (const auto& path : candidates)
        {
            sf::Texture tex;
            if (tex.loadFromFile(path))
            {
                tex.setSmooth(true);
                icons_[entry.key] = std::move(tex);
                break;
            }
        }
    }

}

const sf::Texture* ImGuiUi::icon(const char* key) const
{
    const auto it = icons_.find(key);
    return it == icons_.end() ? nullptr : &it->second;
}

void ImGuiUi::drawAgentInspector(const config::ParameterRegistry& registry,
                                 const sim::SimulationRunner& runner,
                                 UiState& state,
                                 core::CommandQueue& queue)
{
    static_cast<void>(registry);
    static_cast<void>(queue);

    // Default: the viewer is not requesting a trace this frame. Only the visible
    // Rede Neural tab below turns it on, so the engine trace stays off otherwise.
    state.neuralTraceActive = false;
    if (!state.agentPanelOpen) return;

    // Primary inspected agent = the first selected id that is still alive.
    simulation::EntityId selId{};
    for (const auto id : state.selection.ids())
    {
        if (runner.agents().contains(id)) { selId = id; break; }
    }
    if (!selId.isValid()) return;  // nothing selected -> panel stays hidden
    const auto idxOpt = runner.agents().indexOf(selId);
    if (!idxOpt.has_value()) return;
    const std::size_t idx = *idxOpt;

    const auto num = [](double v) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", v);
        return std::string(buf);
    };

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(372.0F, 540.0F), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 384.0F, 96.0F), ImGuiCond_FirstUseEver);
    bool open = true;
    const std::string title = std::string(tr("Agente", "Agent")) + "###agentpanel";
    if (ImGui::Begin(title.c_str(), &open))
    {
        if (ImGui::BeginTabBar("##agenttabs"))
        {
            if (ImGui::BeginTabItem(tr("Genoma", "Genome")))
            {
                const auto speciesId = runner.agents().speciesIdAt(idx);
                const auto* sp = runner.species().find(speciesId);
                const auto* g = runner.genomes().find(runner.agents().genomeIdAt(idx));
                const auto color = runner.agents().colorAt(idx);
                const auto pos = runner.agents().positionAt(idx);

                if (ImGui::BeginTable("##genome", 2, ImGuiTableFlags_SizingStretchProp |
                                                          ImGuiTableFlags_BordersInnerH))
                {
                    const auto row = [](const char* key, const std::string& value) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(key);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(value.c_str());
                    };
                    row(tr("Especie", "Species"),
                        sp != nullptr ? (sp->label.empty() ? sp->name : sp->label)
                                      : std::to_string(static_cast<unsigned>(speciesId)));
                    row(tr("Energia", "Energy"), num(runner.agents().energyAt(idx)));
                    row(tr("Idade (s)", "Age (s)"), num(runner.agents().ageAt(idx)));
                    row(tr("Raio", "Radius"), num(runner.agents().radiusAt(idx)));
                    row(tr("Posicao", "Position"), num(pos.x) + ", " + num(pos.y));
                    row(tr("Cor (RGB)", "Color (RGB)"),
                        std::to_string(color.r) + ", " + std::to_string(color.g) + ", " +
                            std::to_string(color.b));
                    if (g != nullptr)
                    {
                        row(tr("Tipo de rede", "Brain type"), neuralTypeLabel(g->brainConfig.type));
                        row(tr("Geracao", "Generation"), std::to_string(g->generation));
                        row(tr("Tamanho do corpo", "Body size"), num(g->bodySize));
                        row(tr("Energia p/ reproduzir", "Split energy"), num(g->splitEnergy));
                        row(tr("Energia inicial", "Initial energy"), num(g->initialEnergy));
                        row(tr("Energia maxima", "Energy cap"), num(g->energyCap));
                        row(tr("Taxa de mutacao", "Mutation rate"), num(g->mutationRate));
                        row(tr("Forca de mutacao", "Mutation strength"), num(g->mutationStrength));
                        row(tr("Come comida", "Eats food"),
                            g->diet.eatFood ? tr("sim", "yes") : tr("nao", "no"));
                        row(tr("Come organismos", "Eats organisms"),
                            g->diet.eatAgents ? tr("sim", "yes") : tr("nao", "no"));
                    }
                    ImGui::EndTable();
                }
                ImGui::TextDisabled("%s", tr("Somente leitura — edite no Editor Genetico.",
                                             "Read-only — edit in the Genetic Editor."));
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(tr("Rede Neural", "Neural Network")))
            {
                // This tab is visible: ask the engine to capture this agent's
                // trace (App reads these signals and sets the runner target).
                state.neuralTraceActive = true;
                state.neuralTraceAgent = selId;

                ImGui::Checkbox(tr("Preencher painel", "Fill panel"), &state.neuralViewerFillLayout);
                ImGui::SameLine();
                ImGui::Checkbox(tr("Mostrar visao", "Show vision"), &state.selectedVisionOverlay);

                if (runner.hasSelectedNeuralView())
                {
                    const neural::NeuralView& view = runner.selectedNeuralView();
                    ImGui::TextDisabled("%s", neuralTypeLabel(view.type));
                    if (neural::isNeatFamily(view.type))
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("  |  %zu/%zu con  %zu rec", view.enabledConnectionCount,
                                            view.connectionCount, view.recurrentConnectionCount);
                    }
                    if (view.hasRecurrentState)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("  |  decay %.2f  clip %.2f", view.memoryDecay, view.stateClip);
                    }
                    ImGui::Separator();
                    if (view.hasRecurrentState && !view.recurrentState.empty())
                    {
                        drawRecurrentStrip(view);
                        ImGui::Separator();
                    }
                    drawNeuralCanvas(view, state.neuralViewerFillLayout);
                }
                else
                {
                    ImGui::TextDisabled("%s", tr("Calculando a rede (aguarde 1 passo)...",
                                                 "Computing the network (wait one step)..."));
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    if (!open) state.agentPanelOpen = false;
}

void ImGuiUi::drawMetricsWindow(const config::ParameterRegistry& registry,
                                const sim::SimulationRunner& runner,
                                UiState& state,
                                core::CommandQueue& queue)
{
    if (!state.showMetricsWindow) return;

    ImGui::SetNextWindowSize(ImVec2(470.0F, 560.0F), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(90.0F, 110.0F), ImGuiCond_FirstUseEver);
    bool open = true;
    const std::string title = std::string(tr("Metricas", "Metrics")) + "###metrics";
    if (ImGui::Begin(title.c_str(), &open))
    {
        // Live engine toggles. The App applies these knobs immediately (no
        // pending edit), so the checkbox just emits the value command.
        const auto boolToggle = [&](const char* param, const char* label) {
            bool value = config::parameterBool(registry, param, false);
            if (ImGui::Checkbox(label, &value))
            {
                queue.push(core::CmdSetParameterValue{param, value});
            }
        };
        boolToggle("metrics_enabled", tr("Coletar metricas", "Collect metrics"));
        ImGui::SameLine();
        boolToggle("profiler_enabled", tr("Profiler por sistema", "Per-system profiler"));
        ImGui::Separator();

        if (ImGui::BeginTabBar("##metricstabs"))
        {
            if (ImGui::BeginTabItem(tr("Series", "Series")))
            {
                const systems::MetricsSystem& m = runner.metrics();
                if (m.sampleCount() == 0)
                {
                    ImGui::TextDisabled("%s", tr("Sem dados — ative a coleta de metricas.",
                                                 "No data — enable metrics collection."));
                }
                else
                {
                    const auto plot = [&](systems::MetricField field, const char* label) {
                        const std::vector<float> s = m.series(field);
                        char overlay[24];
                        std::snprintf(overlay, sizeof(overlay), "%.1f", s.empty() ? 0.0F : s.back());
                        ImGui::TextUnformatted(label);
                        ImGui::PlotLines((std::string("##") + label).c_str(), s.data(),
                                         static_cast<int>(s.size()), 0, overlay, FLT_MAX, FLT_MAX,
                                         ImVec2(-1.0F, 58.0F));
                    };
                    plot(systems::MetricField::Population, tr("Populacao", "Population"));
                    plot(systems::MetricField::FoodCount, tr("Comida", "Food"));
                    plot(systems::MetricField::MeanEnergy, tr("Energia media", "Mean energy"));
                    plot(systems::MetricField::SmartFactor, tr("Inteligencia (grupo)", "Intelligence (group)"));
                    plot(systems::MetricField::Births, tr("Nascimentos/passo", "Births/step"));
                    plot(systems::MetricField::Deaths, tr("Mortes/passo", "Deaths/step"));
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(tr("Especies", "Species")))
            {
                const systems::MetricsSystem& m = runner.metrics();
                if (m.speciesMetrics().empty())
                {
                    ImGui::TextDisabled("%s", tr("Sem dados.", "No data."));
                }
                for (const auto& sm : m.speciesMetrics())
                {
                    const auto* sp = runner.species().find(sm.id);
                    const std::string name = sp != nullptr
                        ? (sp->label.empty() ? sp->name : sp->label)
                        : std::to_string(static_cast<unsigned>(sm.id));
                    const std::vector<float> pop(sm.population.begin(), sm.population.end());
                    char overlay[24];
                    std::snprintf(overlay, sizeof(overlay), "%.0f", pop.empty() ? 0.0F : pop.back());
                    ImGui::TextUnformatted(name.c_str());
                    ImGui::PlotLines((std::string("##pop") + name).c_str(), pop.data(),
                                     static_cast<int>(pop.size()), 0, overlay, FLT_MAX, FLT_MAX,
                                     ImVec2(-1.0F, 50.0F));
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(tr("Profiler", "Profiler")))
            {
                const core::Profiler& p = runner.profiler();
                if (p.stepCount() == 0)
                {
                    ImGui::TextDisabled("%s", tr("Ative o profiler para medir por sistema.",
                                                 "Enable the profiler to measure per system."));
                }
                else
                {
                    if (ImGui::BeginTable("##proftbl", 4,
                                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                              ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableSetupColumn(tr("Sistema", "System"));
                        ImGui::TableSetupColumn(tr("us/passo", "us/step"));
                        ImGui::TableSetupColumn("%");
                        ImGui::TableSetupColumn(tr("cham.", "calls"));
                        ImGui::TableHeadersRow();
                        for (int i = 0; i < static_cast<int>(core::ProfileSection::Count); ++i)
                        {
                            const auto section = static_cast<core::ProfileSection>(i);
                            if (section == core::ProfileSection::SimStep) continue;
                            const core::Profiler::Stat& st = p.stat(section);
                            if (st.calls == 0) continue;
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(core::profileSectionName(section));
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%.2f", p.averageUs(section));
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%.1f", p.percentOfStep(section));
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%llu", static_cast<unsigned long long>(st.calls));
                        }
                        ImGui::EndTable();
                    }
                    const double overheadUsPerStep = static_cast<double>(p.overheadAccumNs()) /
                        1000.0 / static_cast<double>(p.stepCount() == 0 ? 1 : p.stepCount());
                    ImGui::Separator();
                    ImGui::Text("%s: %.2f us/%s", tr("Passo total", "Total step"),
                                p.averageUs(core::ProfileSection::SimStep),
                                tr("passo", "step"));
                    ImGui::Text("%s: %.2f us/%s", tr("Glue (overhead)", "Glue (overhead)"),
                                overheadUsPerStep, tr("passo", "step"));
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    if (!open) state.showMetricsWindow = false;
}

void ImGuiUi::draw(const config::ParameterRegistry& registry,
                   const sim::SimulationRunner& runner,
                   UiState& state,
                   core::CommandQueue& queue,
                   const ImGuiFrameInfo& info)
{
    loadIcons();  // Phase 25.3: one-time icon load (GL context is live here)
    PreferencesState& prefs = state.preferences;
    const ImGuiIO& io = ImGui::GetIO();
    const float vpW = io.DisplaySize.x;
    const float vpH = io.DisplaySize.y;

    // ---------------------------------------------------------------- menu bar
    float menuBarH = 0.0F;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu(tr("Arquivo", "File")))
        {
            if (ImGui::MenuItem(tr("Novo", "New"))) queue.push(core::CmdNewSimulation{});
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Abrir Simulacao...", "Open Simulation...")))
                queue.push(core::CmdLoadSimulation{});
            if (ImGui::MenuItem(tr("Salvar Simulacao", "Save Simulation")))
                queue.push(core::CmdSaveSimulation{});
            if (ImGui::MenuItem(tr("Salvar Como...", "Save As...")))
                queue.push(core::CmdSaveSimulationAs{});
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Sair", "Quit"))) queue.push(core::CmdQuitApp{});
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Exibir", "View")))
        {
            if (ImGui::MenuItem(tr("Painel lateral", "Side panel"), nullptr, prefs.dockVisible))
                queue.push(core::CmdToggleLeftDock{});
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Ajustar mundo (Fit)", "Fit world"))) queue.push(core::CmdFitWorldCamera{});
            if (ImGui::MenuItem(tr("Resetar camera", "Reset camera"))) queue.push(core::CmdResetCamera{});
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Render simples", "Simple render"), nullptr, info.simpleRender))
                queue.push(core::CmdToggleSimpleRender{});
            if (ImGui::MenuItem(tr("Overlay spatial hash", "Spatial hash overlay"))) queue.push(core::CmdToggleSpatialHashOverlay{});
            if (ImGui::MenuItem(tr("Debug de visao", "Vision debug"))) queue.push(core::CmdToggleVisionDebug{});
            if (ImGui::MenuItem(tr("Overlay de selecao", "Selection overlay"), nullptr, state.showSelectionOverlay))
                queue.push(core::CmdToggleSelectionOverlay{});
            ImGui::Separator();
            if (ImGui::MenuItem(tr("Metricas e profiler", "Metrics & profiler"), nullptr, state.showMetricsWindow))
                state.showMetricsWindow = !state.showMetricsWindow;
            if (ImGui::MenuItem(tr("Janela do Desenvolvedor", "Developer Window"), nullptr, state.showDevWindow))
                state.showDevWindow = !state.showDevWindow;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Preferencias", "Preferences")))
        {
            for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
            {
                if (ImGui::MenuItem(config::prefsTabLabel(static_cast<config::PrefsTab>(t))))
                {
                    queue.push(core::CmdOpenPreferencesWindow{t});
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Agente", "Agent")))
        {
            if (ImGui::MenuItem(tr("Painel do agente", "Agent panel"), nullptr, state.agentPanelOpen))
            {
                state.agentPanelOpen = !state.agentPanelOpen;
            }
            const bool hasSelection = !state.selection.empty();
            ImGui::BeginDisabled(!hasSelection);
            if (ImGui::MenuItem(tr("Visualizador neural", "Neural viewer")))
            {
                state.agentPanelOpen = true;  // panel hosts the Rede Neural tab
            }
            ImGui::EndDisabled();
            if (!hasSelection)
            {
                ImGui::TextDisabled("%s", tr("(selecione um agente)", "(select an agent)"));
            }
            ImGui::Separator();
            ImGui::BeginDisabled(!hasSelection);
            if (ImGui::MenuItem(tr("Exportar organismo selecionado...", "Export selected organism...")))
                queue.push(core::CmdExportAgent{});
            ImGui::EndDisabled();
            if (ImGui::MenuItem(tr("Importar organismo...", "Import organism...")))
                queue.push(core::CmdImportAgent{});
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr("Ajuda", "Help")))
        {
            if (ImGui::MenuItem(tr("Atalhos", "Shortcuts"))) queue.push(core::CmdOpenHelpWindow{});
            if (ImGui::MenuItem(tr("Sobre", "About"))) queue.push(core::CmdToggleAboutPanel{});
            ImGui::EndMenu();
        }
        menuBarH = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }

    // ----------------------------------------------------------------- toolbar
    const float toolbarH = 48.0F;
    ImGui::SetNextWindowPos(ImVec2(0.0F, menuBarH));
    ImGui::SetNextWindowSize(ImVec2(vpW, toolbarH));
    const ImGuiWindowFlags stripFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
    if (ImGui::Begin("##toolbar", nullptr, stripFlags))
    {
        // Transport: play/pause toggles its icon; reset; single step.
        if (iconButton(info.paused ? icon("play") : icon("pause"), "playpause",
                       info.paused ? tr("Continuar", "Resume") : tr("Pausar", "Pause"),
                       false, info.paused ? "Play" : "Pause"))
        {
            queue.push(core::CmdPauseToggle{});
        }
        ImGui::SameLine();
        if (iconButton(icon("reset"), "reset", tr("Resetar simulacao", "Reset simulation"),
                       false, "Reset"))
        {
            ImGui::OpenPopup("##resetconfirm");
        }
        // Reset overhaul: this is the ONLY full-reset entry point in the toolbar, and it
        // asks first. "Manter labels e genomas" preserves the user's labels and each
        // label's genome template (population respawned from those genomes, brains fresh);
        // "Resetar tudo" returns to the default initial state.
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                                ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("##resetconfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted(tr("Resetar a simulacao. O que deseja fazer?",
                                      "Reset the simulation. What do you want to do?"));
            ImGui::Spacing();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 460.0F);
            ImGui::TextUnformatted(tr(
                "\"Manter labels e genomas\" preserva suas labels e o genoma de cada uma; a "
                "populacao renasce a partir desses genomas (os cerebros recomecam do zero). "
                "\"Resetar tudo\" volta ao estado inicial padrao.",
                "\"Keep labels and genomes\" preserves your labels and each label's genome; the "
                "population is respawned from those genomes (brains start over). "
                "\"Reset everything\" returns to the default initial state."));
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button(tr("Manter labels e genomas", "Keep labels and genomes"),
                              ImVec2(240.0F, 0.0F)))
            {
                queue.push(core::CmdResetKeepLabels{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("Resetar tudo", "Reset everything"), ImVec2(150.0F, 0.0F)))
            {
                queue.push(core::CmdResetSimulation{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("Cancelar", "Cancel"), ImVec2(120.0F, 0.0F)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr("Passo", "Step"))) queue.push(core::CmdStepOnce{});
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Canvas tools as icon buttons. The icon highlights when its tool is active.
        const auto toolIcon = [&](core::CanvasTool t, const char* key,
                                  const char* tipPt, const char* tipEn) {
            if (iconButton(icon(key), key, tr(tipPt, tipEn), state.activeTool == t, toolLabel(t)))
            {
                queue.push(core::CmdSetCanvasTool{t});
            }
            ImGui::SameLine();
        };
        toolIcon(core::CanvasTool::Select, "select", "Selecionar", "Select");
        toolIcon(core::CanvasTool::RectangleSelect, "rect", "Selecao retangular", "Rectangle select");
        toolIcon(core::CanvasTool::LassoSelect, "lasso", "Laco", "Lasso");
        toolIcon(core::CanvasTool::AddFood, "food", "Adicionar comida", "Add food");
        toolIcon(core::CanvasTool::AddAgent, "agent", "Adicionar agente", "Add agent");

        // Obstacle tool: a single icon that opens a horizontal dropdown holding
        // the paint (Pincel) and erase (Limpar) obstacle tools.
        const bool obstacleActive = state.activeTool == core::CanvasTool::PaintObstacle ||
                                    state.activeTool == core::CanvasTool::EraseObstacle;
        if (iconButton(icon("obstacle"), "obstacle", tr("Obstaculo", "Obstacle"),
                       obstacleActive, "Obst"))
        {
            ImGui::OpenPopup("##obstaclepopup");
        }
        if (ImGui::BeginPopup("##obstaclepopup"))
        {
            if (iconButton(icon("brush"), "obs_brush",
                           tr("Pincel (desenhar obstaculo)", "Brush (paint obstacle)"),
                           state.activeTool == core::CanvasTool::PaintObstacle, "Pincel"))
            {
                queue.push(core::CmdSetCanvasTool{core::CanvasTool::PaintObstacle});
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (iconButton(icon("erase"), "obs_erase",
                           tr("Limpar obstaculo", "Erase obstacle"),
                           state.activeTool == core::CanvasTool::EraseObstacle, "Limpar"))
            {
                queue.push(core::CmdSetCanvasTool{core::CanvasTool::EraseObstacle});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        toolIcon(core::CanvasTool::Move, "move", "Mover", "Move");
        toolIcon(core::CanvasTool::Delete, "delete", "Excluir", "Delete");

        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (iconButton(icon("fit"), "fit", tr("Ajustar mundo", "Fit world"), false, "Fit"))
        {
            queue.push(core::CmdFitWorldCamera{});
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0F);
        float ts = static_cast<float>(state.timeScale);
        if (ImGui::SliderFloat("##vel", &ts, 0.1F, 50.0F, "%.1fx", ImGuiSliderFlags_Logarithmic))
        {
            queue.push(core::CmdSetTimeScale{static_cast<double>(ts)});
        }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text(tr("Ag %zu   Com %zu   Obs %zu   %.0f FPS",
                       "Ag %zu   Food %zu   Obs %zu   %.0f FPS"),
                     info.agents, info.foods, info.obstacles, static_cast<double>(info.fps));
    }
    ImGui::End();
    ImGui::PopStyleVar();

    topStripHeight_ = menuBarH + toolbarH;

    // --------------------------------------------------------------- left dock
    if (prefs.dockVisible)
    {
        ImGui::SetNextWindowPos(ImVec2(0.0F, topStripHeight_));
        ImGui::SetNextWindowSize(ImVec2(kDockW, vpH - topStripHeight_));
        const ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        if (ImGui::Begin("##leftdock", nullptr, dockFlags))
        {
            if (ImGui::BeginTabBar("##docktabs"))
            {
                if (ImGui::BeginTabItem(tr("Editor Genetico", "Genetic Editor")))
                {
                    ImGui::BeginChild("##editorscroll", ImVec2(0.0F, -52.0F));
                    // Phase 25.1: Gestalt grouping — params split into labeled
                    // sections (thin separator + small title), mirroring the
                    // Python genetic editor. The editor edits the "bacteria"
                    // species. (Per-group Aplicar + reset-network message: Fase 25.2.)
                    // `id` is a stable, language-independent ImGui table id; the
                    // displayed section title is localized at the call site so a
                    // language flip does not reset table state (Phase 25.2).
                    struct EditorGroup
                    {
                        const char* id;
                        const char* titlePt;
                        const char* titleEn;
                        std::vector<const char*> suffixes;
                    };
                    static const std::vector<EditorGroup> kGroups = {
                        {"grp_body", "Corpo e locomocao", "Body & locomotion",
                            {"body_size", "body_shape", "max_speed", "max_turn",
                             "allow_reverse_locomotion", "movement_mode"}},
                        {"grp_energy", "Energia e reproducao", "Energy & reproduction",
                            {"initial_energy", "death_energy", "split_energy", "v0_cost",
                             "vmax_cost", "energy_cap", "death_by_age_enabled", "death_age",
                             "corpse_to_food", "reproduction_min_age", "reproduction_cooldown"}},
                        {"grp_vision", "Visao", "Vision",
                            // Microfase 32.5/Fase 32.1: nomes com o infixo retina_ (casam
                            // o registry e o editorParameters() corrigido). Sem isso os
                            // checkboxes de "o que enxergar" nao apareciam na secao Visao.
                            {"vision_radius", "retina_count", "retina_fov_degrees", "eye_count",
                             "eye_angle_degrees", "retina_see_food", "retina_see_bacteria",
                             "retina_see_predators", "retina_see_obstacles", "retina_see_all",
                             "retina_see_through_walls", "retina_channel_r",
                             "retina_channel_g", "retina_channel_b", "retina_channel_d",
                             "retina_input_mode"}},
                        {"grp_diet", "Dieta", "Diet",
                            {"diet_food", "diet_agents", "diet_same_label", "food_efficiency",
                             "agent_efficiency"}},
                        {"grp_neural", "Rede neural", "Neural network",
                            {"hidden_layers", "mutation_rate", "mutation_strength"}},
                    };
                    const auto editorNames = UiLeftDock::editorParameters();
                    for (const auto& g : kGroups)
                    {
                        std::vector<std::string> present;
                        for (const char* suf : g.suffixes)
                        {
                            const std::string full = std::string("bacteria_") + suf;
                            if (std::find(editorNames.begin(), editorNames.end(), full) !=
                                editorNames.end())
                            {
                                present.push_back(full);
                            }
                        }
                        if (present.empty()) continue;
                        ImGui::SeparatorText(tr(g.titlePt, g.titleEn));
                        // Reset overhaul: warn ONLY when a brain-architecture change is
                        // actually pending (here: the hidden layers). Applying it rebuilds
                        // the brains and the learning is lost; labels/organisms are kept.
                        if (std::string(g.id) == "grp_neural" &&
                            prefs.pendingValues.count("bacteria_hidden_layers") > 0U)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.78F, 0.30F, 1.0F));
                            ImGui::PushTextWrapPos(0.0F);
                            ImGui::TextUnformatted(tr(
                                "Aviso: aplicar a mudanca de camadas reconstroi os cerebros — o "
                                "aprendizado sera perdido. As labels e os organismos sao mantidos.",
                                "Warning: applying the layer change rebuilds the brains — learning "
                                "will be lost. Labels and organisms are kept."));
                            ImGui::PopTextWrapPos();
                            ImGui::PopStyleColor();
                        }
                        drawParamTable(g.id, registry, prefs, present, queue);
                    }
                    ImGui::EndChild();
                    ImGui::Separator();
                    // Microfase 32.2: live apply targets the label of the first
                    // selected organism (fallback: the default bacteria label).
                    // Show the target so the user always knows who receives it.
                    const simulation::SpeciesRecord* applyTarget = nullptr;
                    {
                        const auto& ag = runner.agents();
                        for (const auto selId : state.selection.ids())
                        {
                            const auto idx = ag.indexOf(selId);
                            if (idx.has_value() && ag.aliveAt(*idx))
                            {
                                applyTarget = runner.species().find(ag.speciesIdAt(*idx));
                                break;
                            }
                        }
                        if (applyTarget == nullptr)
                        {
                            applyTarget = runner.species().findByName("bacteria");
                        }
                    }
                    ImGui::TextDisabled(tr("Label alvo: %s", "Target label: %s"),
                                        applyTarget != nullptr ? applyTarget->label.c_str()
                                                               : "-");
                    if (ImGui::Button(tr("Aplicar a especie", "Apply to species")))
                    {
                        state.applyFeedbackNeural = prefs.pendingValues.count("bacteria_hidden_layers") > 0U;
                        state.applyFeedbackAt = ImGui::GetTime();
                        queue.push(core::CmdApplyGenomeToSpecies{});
                    }
                    ImGui::SetItemTooltip("%s", tr(
                        "Aplica os valores do editor AO VIVO a todos os organismos vivos da "
                        "label alvo e ao genoma-template dela (novos resgates ja nascem assim). "
                        "Nada e deletado e a simulacao NAO reinicia. Cerebros sao preservados.",
                        "Applies the editor values LIVE to every living organism of the target "
                        "label and to its template genome (future rescues inherit it). Nothing "
                        "is deleted and the simulation does NOT restart. Brains are preserved."));
                    ImGui::SameLine();
                    if (ImGui::Button(tr("Aplicar selecionados", "Apply to selected")))
                    {
                        state.applyFeedbackNeural = prefs.pendingValues.count("bacteria_hidden_layers") > 0U;
                        state.applyFeedbackAt = ImGui::GetTime();
                        queue.push(core::CmdApplyGenomeToSelected{});
                    }
                    ImGui::SetItemTooltip("%s", tr(
                        "Aplica os valores do editor AO VIVO apenas aos organismos selecionados, "
                        "mantendo a label, posicao, energia e cerebro de cada um.",
                        "Applies the editor values LIVE to the selected organisms only, keeping "
                        "each one's label, position, energy and brain."));
                    ImGui::SameLine();
                    if (ImGui::Button(tr("Reverter", "Revert"))) queue.push(core::CmdRevertPreferences{});
                    ImGui::SameLine();
                    if (ImGui::Button(tr("Padroes", "Defaults")))
                    {
                        for (const auto& n : UiLeftDock::editorParameters())
                            queue.push(core::CmdRestoreParameterDefault{n});
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem(tr("Substrato", "Substrate")))
                {
                    ImGui::BeginChild("##substratoscroll", ImVec2(0.0F, -52.0F));
                    std::vector<std::string> worldParams;
                    std::vector<std::string> foodParams;
                    for (const auto& n : UiLeftDock::substratoParameters())
                    {
                        const bool isWorld = (n == "substrate_shape" || n == "world_w" ||
                                              n == "world_h" || n == "substrate_radius");
                        (isWorld ? worldParams : foodParams).push_back(n);
                    }
                    ImGui::SeparatorText(tr("Substrato", "Substrate"));
                    drawParamTable("##subworld", registry, prefs, worldParams, queue);
                    ImGui::SeparatorText(tr("Comida", "Food"));
                    drawParamTable("##subfood", registry, prefs, foodParams, queue);
                    ImGui::EndChild();
                    ImGui::Separator();
                    if (ImGui::Button(tr("Aplicar ambiente", "Apply environment"))) queue.push(core::CmdApplyEnvironment{});
                    ImGui::SameLine();
                    if (ImGui::Button(tr("Limpar comida", "Clear food"))) queue.push(core::CmdClearAllFood{});
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Labels"))
                {
                    drawLabelsTab(registry, runner, prefs, queue);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // ----------------------------------------------------- preferences windows
    for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
    {
        if (!prefs.windowOpen[static_cast<std::size_t>(t)]) continue;
        const auto tab = static_cast<config::PrefsTab>(t);
        std::string title = std::string(config::prefsTabLabel(tab)) + "###prefs" + std::to_string(t);
        ImGui::SetNextWindowPos(ImVec2(vpW * 0.35F + static_cast<float>(t) * 26.0F,
                                         90.0F + static_cast<float>(t) * 26.0F),
                                  ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(460.0F, 540.0F), ImGuiCond_FirstUseEver);
        bool open = true;
        if (ImGui::Begin(title.c_str(), &open))
        {
            ImGui::InputTextWithHint("##search", tr("Buscar parametro...", "Search parameter..."), &prefs.searchQuery);
            // Aparencia: presets de tema (teste de tema). Aplica um cenario visual com
            // efeito de profundidade no zoom; por enquanto mostra so o cenario.
            if (tab == config::PrefsTab::Appearance)
            {
                ImGui::SeparatorText(tr("Temas", "Themes"));
                ImGui::PushTextWrapPos(0.0F);
                ImGui::TextUnformatted(tr("Escolha a aparencia da simulacao.",
                                          "Choose the simulation's appearance."));
                ImGui::PopTextWrapPos();
                const std::string activeTheme = config::parameterString(registry, "ui_theme", "orange");
                const ImVec2 thumb(92.0F, 56.0F);
                // Each theme is a gradient swatch (bg + dish hint), green-framed when
                // active. No PNG dependency, so all themes preview consistently.
                const auto swatch = [&](const char* widgetId, const char* label,
                                        const char* themeId, int cmdId, ImU32 cTop, ImU32 cBot,
                                        ImU32 cDish) {
                    const bool sel = activeTheme == themeId;
                    const ImVec2 p0 = ImGui::GetCursorScreenPos();
                    const bool clicked = ImGui::InvisibleButton(widgetId, thumb);
                    const ImVec2 p1(p0.x + thumb.x, p0.y + thumb.y);
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    dl->AddRectFilledMultiColor(p0, p1, cTop, cTop, cBot, cBot);
                    const ImVec2 c{(p0.x + p1.x) * 0.5F, (p0.y + p1.y) * 0.5F};
                    dl->AddCircleFilled(c, 15.0F, cDish, 32);
                    dl->AddCircle(c, 15.0F, IM_COL32(255, 255, 255, 200), 32, 1.5F);
                    dl->AddRect(p0, p1, sel ? IM_COL32(60, 210, 110, 255) : IM_COL32(150, 150, 150, 150),
                                4.0F, 0, sel ? 3.0F : 1.0F);
                    const ImVec2 ts = ImGui::CalcTextSize(label);
                    dl->AddText({c.x - ts.x * 0.5F, p1.y - ts.y - 3.0F}, IM_COL32(255, 255, 255, 235), label);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", label);
                    if (clicked) queue.push(core::CmdSetTheme{cmdId});
                };
                swatch("##th_none", tr("Nenhum", "None"), "none", 0,
                       IM_COL32(44, 44, 52, 255), IM_COL32(22, 22, 28, 255), IM_COL32(64, 64, 74, 255));
                ImGui::SameLine();
                swatch("##th_orange", tr("Laranja", "Orange"), "orange", 1,
                       IM_COL32(246, 144, 73, 255), IM_COL32(237, 74, 87, 255), IM_COL32(251, 183, 140, 255));
                ImGui::SameLine();
                swatch("##th_dblue", tr("Azul escuro", "Dark blue"), "dark_blue", 2,
                       IM_COL32(12, 30, 72, 255), IM_COL32(6, 15, 40, 255), IM_COL32(31, 65, 128, 255));
                ImGui::SameLine();
                swatch("##th_lblue", tr("Azul claro", "Light blue"), "light_blue", 3,
                       IM_COL32(53, 207, 194, 255), IM_COL32(90, 147, 221, 255), IM_COL32(212, 237, 248, 255));
                ImGui::Spacing();
            }
            // Reset overhaul: the neural warning appears ONLY when a brain-architecture
            // change is pending (not just for opening the tab). No "applies live" notice
            // anywhere; physics has no warning at all.
            else if (tab == config::PrefsTab::Neural && hasPendingNeuralArchChange(prefs))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.78F, 0.30F, 1.0F));
                ImGui::PushTextWrapPos(0.0F);
                ImGui::TextUnformatted(tr(
                    "Aviso: aplicar esta mudanca reconstroi os cerebros — o aprendizado sera "
                    "perdido. As labels e os organismos sao mantidos.",
                    "Warning: applying this change rebuilds the brains — learning will be lost. "
                    "Labels and organisms are kept."));
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
            }
            ImGui::Separator();
            ImGui::BeginChild("##prefsscroll", ImVec2(0.0F, -44.0F));
            const auto names = prefsParametersForTabFiltered(registry, prefs, tab);
            if (names.empty())
            {
                ImGui::TextDisabled(tr("Nenhum parametro nesta categoria.", "No parameters in this category."));
            }
            drawParamTable("##prefstbl", registry, prefs, names, queue);
            ImGui::EndChild();
            ImGui::Separator();
            if (ImGui::Button(tr("Aplicar", "Apply")))
            {
                // Feedback: capture whether this apply rebuilds brains BEFORE the
                // pending buffer is cleared, then fire the centered green check.
                state.applyFeedbackNeural = hasPendingNeuralArchChange(prefs);
                state.applyFeedbackAt = ImGui::GetTime();
                queue.push(core::CmdApplyPreferences{});
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("Reverter", "Revert"))) queue.push(core::CmdRevertPreferences{});
            ImGui::SameLine();
            if (ImGui::Button(tr("Restaurar padroes", "Restore defaults")))
            {
                for (const auto& n : names) queue.push(core::CmdRestoreParameterDefault{n});
                queue.push(core::CmdApplyPreferences{});
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("Fechar", "Close"))) queue.push(core::CmdClosePreferencesWindow{t});
        }
        ImGui::End();
        if (!open) queue.push(core::CmdClosePreferencesWindow{t});
    }

    // ------------------------------------------------- selected agent panel
    drawAgentInspector(registry, runner, state, queue);

    // ------------------------------------------------------- metrics window
    drawMetricsWindow(registry, runner, state, queue);

    // ----------------------------------------------- developer window (Fase 30)
    devWindow_.draw(registry, runner, state, queue, info.fps);

    // ----------------------------------------------------------- help / about
    if (prefs.helpWindowOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(420.0F, 460.0F), ImGuiCond_FirstUseEver);
        bool open = true;
        const std::string helpTitle = std::string(tr("Atalhos", "Shortcuts")) + "###help";
        if (ImGui::Begin(helpTitle.c_str(), &open))
        {
            ImGui::TextUnformatted(tr("Atalhos de teclado", "Keyboard shortcuts"));
            ImGui::Separator();
            ImGui::BulletText("%s", tr("Espaco: pausar / retomar", "Space: pause / resume"));
            ImGui::BulletText("%s", tr("Esc: limpar selecao e voltar para Selecao", "Esc: clear selection and return to Select"));
            ImGui::BulletText("%s", tr("Delete: excluir selecionados", "Delete: delete selected"));
            ImGui::BulletText("%s", tr("R: resetar simulacao", "R: reset simulation"));
            ImGui::BulletText("%s", tr("F: ajustar mundo (fit)   T: render simples", "F: fit world   T: simple render"));
            ImGui::BulletText("%s", tr("V: debug de visao        H: esta ajuda", "V: vision debug        H: this help"));
            ImGui::Separator();
            ImGui::TextUnformatted(tr("Ferramentas", "Tools"));
            ImGui::BulletText("%s", tr("S: Selecao   Q: Retangulo   L: Laco", "S: Select   Q: Rectangle   L: Lasso"));
            ImGui::BulletText("%s", tr("G: Comida    A: Agente", "G: Food    A: Agent"));
            ImGui::BulletText("%s", tr("B: Pincel    X: Apagar   M: Mover   D: Excluir", "B: Brush    X: Erase   M: Move   D: Delete"));
            ImGui::Separator();
            ImGui::TextUnformatted(tr("Camera", "Camera"));
            ImGui::BulletText("%s", tr("Setas: mover camera", "Arrows: move camera"));
            ImGui::BulletText("%s", tr("Scroll: zoom no cursor", "Scroll: zoom at cursor"));
            ImGui::BulletText("%s", tr("Botao direito: arrastar (pan)", "Right button: drag (pan)"));
            ImGui::Separator();
            ImGui::TextUnformatted(tr("Selecao", "Selection"));
            ImGui::BulletText("%s", tr("Shift/Ctrl + clique: somar a selecao",
                                       "Shift/Ctrl + click: add to selection"));
        }
        ImGui::End();
        if (!open) queue.push(core::CmdCloseHelpWindow{});
    }

    if (state.showAboutPanel)
    {
        ImGui::SetNextWindowSize(ImVec2(420.0F, 200.0F), ImGuiCond_FirstUseEver);
        bool open = true;
        const std::string aboutTitle = std::string(tr("Sobre", "About")) + "###about";
        if (ImGui::Begin(aboutTitle.c_str(), &open))
        {
            ImGui::TextUnformatted("AgentBioSim C++ / SFML");
            ImGui::Separator();
            ImGui::TextWrapped("%s", tr(
                "Simulacao evolutiva 2D. Fase 25: interface migrada para Dear "
                "ImGui sobre a arquitetura headless (engine + comandos).",
                "2D evolutionary simulation. Phase 25: interface migrated to Dear "
                "ImGui on top of the headless architecture (engine + commands)."));
            ImGui::Spacing();
            ImGui::TextDisabled("Dear ImGui v1.90.9 + ImGui-SFML v2.6");
        }
        ImGui::End();
        if (!open) queue.push(core::CmdToggleAboutPanel{});
    }

    // Feedback de "Aplicado": um visto verde transitório no centro da tela após
    // aplicar (Preferências ou editor de genoma). Limpo e discreto; some sozinho.
    // Quando a aplicação reconstruiu os cérebros, uma nota âmbar acompanha (o aviso
    // "sobre apagar o cérebro" só aparece no momento da aplicação, como pedido).
    {
        const double elapsed = ImGui::GetTime() - state.applyFeedbackAt;
        constexpr double kDuration = 1.6;
        if (state.applyFeedbackAt > 0.0 && elapsed >= 0.0 && elapsed <= kDuration)
        {
            float a = static_cast<float>((1.0 - elapsed / kDuration) / 0.4); // hold then fade
            if (a > 1.0F) a = 1.0F;
            if (a < 0.0F) a = 0.0F;
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            const ImVec2 c{disp.x * 0.5F, disp.y * 0.42F};
            const float R = 48.0F;
            const ImU32 green = ImGui::GetColorU32(ImVec4(0.20F, 0.82F, 0.42F, a));
            const ImU32 fill = ImGui::GetColorU32(ImVec4(0.08F, 0.45F, 0.24F, a * 0.35F));
            dl->AddCircleFilled(c, R, fill, 64);
            dl->AddCircle(c, R, green, 64, 4.0F);
            dl->AddLine(ImVec2(c.x - R * 0.42F, c.y + R * 0.02F),
                        ImVec2(c.x - R * 0.08F, c.y + R * 0.34F), green, 5.0F);
            dl->AddLine(ImVec2(c.x - R * 0.08F, c.y + R * 0.34F),
                        ImVec2(c.x + R * 0.46F, c.y - R * 0.32F), green, 5.0F);
            const char* label = tr("Aplicado", "Applied");
            const ImVec2 ts = ImGui::CalcTextSize(label);
            dl->AddText(ImVec2(c.x - ts.x * 0.5F, c.y + R + 8.0F),
                        ImGui::GetColorU32(ImVec4(0.86F, 0.93F, 0.86F, a)), label);
            if (state.applyFeedbackNeural)
            {
                const char* note = tr("Cerebros reiniciados — aprendizado perdido",
                                      "Brains reset — learning lost");
                const ImVec2 ns = ImGui::CalcTextSize(note);
                dl->AddText(ImVec2(c.x - ns.x * 0.5F, c.y + R + 12.0F + ts.y),
                            ImGui::GetColorU32(ImVec4(1.0F, 0.78F, 0.30F, a)), note);
            }
        }
    }
}
} // namespace agentbiosim::ui
