#include "ui/ImGuiUi.hpp"

#include "config/Parameter.hpp"
#include "config/ParameterMetadata.hpp"
#include "sim/SimulationRunner.hpp"
#include "simulation/SpeciesStore.hpp"
#include "ui/ImGuiTheme.hpp"
#include "ui/UiLeftDock.hpp"          // editorParameters()/substratoParameters() (model)
#include "ui/UiPreferencesPanel.hpp"  // prefs* model free functions

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace agentbiosim::ui
{
namespace
{
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
    if (ImGui::IsItemHovered() && !def->description.empty())
    {
        ImGui::SetTooltip("%s", def->description.c_str());
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

bool toolButton(const char* label, bool active)
{
    int pushed = 0;
    if (active)
    {
        const ThemeAccent a = themeAccent();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(a.r, a.g, a.b, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(a.r, a.g, a.b, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(a.r, a.g, a.b, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05F, 0.06F, 0.08F, 1.0F));
        pushed = 4;
    }
    const bool clicked = ImGui::Button(label);
    if (pushed) ImGui::PopStyleColor(pushed);
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

        ImGui::TextDisabled("%zu individuos", runner.countAgentsOfSpecies(rec.id));

        bool sg = rec.showGraph;
        if (ImGui::Checkbox("Grafico", &sg))
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
        popField("Min", 0, rec.minPopulation);
        ImGui::SameLine();
        popField("Max", 1, rec.maxPopulation);
        ImGui::SameLine();
        popField("Ini", 2, rec.initialCount);

        if (ImGui::Button("Selecionar")) queue.push(core::CmdSelectAllOfSpecies{sid});
        ImGui::SameLine();
        if (ImGui::Button("Atribuir selecao")) queue.push(core::CmdAssignSelectedToSpecies{sid});
        ImGui::SameLine();
        if (ImGui::Button("Remover selecao")) queue.push(core::CmdRemoveSelectedFromSpecies{sid});

        ImGui::BeginDisabled(true);
        ImGui::Button("Resetar rede (Fase 26)");
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Excluir")) queue.push(core::CmdRemoveSpecies{sid});

        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Separator();
    {
        const auto eff = prefsEffectiveValue(reg, prefs, "population_min_rescue_enabled");
        bool resc = asBool(eff);
        if (ImGui::Checkbox("Resgate de populacao minima", &resc))
        {
            queue.push(core::CmdSetParameterValue{"population_min_rescue_enabled", resc});
        }
    }
    if (ImGui::Button("+ Nova label com os selecionados"))
    {
        queue.push(core::CmdCreateSpeciesFromSelected{});
    }
}
} // namespace

void ImGuiUi::draw(const config::ParameterRegistry& registry,
                   const sim::SimulationRunner& runner,
                   UiState& state,
                   core::CommandQueue& queue,
                   const ImGuiFrameInfo& info)
{
    PreferencesState& prefs = state.preferences;
    const ImGuiIO& io = ImGui::GetIO();
    const float vpW = io.DisplaySize.x;
    const float vpH = io.DisplaySize.y;

    // ---------------------------------------------------------------- menu bar
    float menuBarH = 0.0F;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Arquivo"))
        {
            if (ImGui::MenuItem("Novo")) queue.push(core::CmdNewSimulation{});
            ImGui::Separator();
            ImGui::MenuItem("Abrir Simulacao", nullptr, false, false);
            ImGui::MenuItem("Salvar Simulacao", nullptr, false, false);
            ImGui::MenuItem("Salvar Como...", nullptr, false, false);
            ImGui::Separator();
            ImGui::MenuItem("Exportar substrato (Fase 28)", nullptr, false, false);
            ImGui::MenuItem("Importar substrato (Fase 28)", nullptr, false, false);
            ImGui::Separator();
            if (ImGui::MenuItem("Sair")) queue.push(core::CmdQuitApp{});
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Exibir"))
        {
            if (ImGui::MenuItem("Painel lateral", nullptr, prefs.dockVisible))
                queue.push(core::CmdToggleLeftDock{});
            ImGui::Separator();
            if (ImGui::MenuItem("Ajustar mundo (Fit)")) queue.push(core::CmdFitWorldCamera{});
            if (ImGui::MenuItem("Resetar camera")) queue.push(core::CmdResetCamera{});
            ImGui::Separator();
            if (ImGui::MenuItem("Render simples", nullptr, info.simpleRender))
                queue.push(core::CmdToggleSimpleRender{});
            if (ImGui::MenuItem("Overlay spatial hash")) queue.push(core::CmdToggleSpatialHashOverlay{});
            if (ImGui::MenuItem("Debug de visao")) queue.push(core::CmdToggleVisionDebug{});
            if (ImGui::MenuItem("Overlay de selecao", nullptr, state.showSelectionOverlay))
                queue.push(core::CmdToggleSelectionOverlay{});
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Preferencias"))
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
        if (ImGui::BeginMenu("Agente"))
        {
            ImGui::MenuItem("Inspetor do agente (Fase 26)", nullptr, false, false);
            ImGui::MenuItem("Visualizador neural (Fase 26)", nullptr, false, false);
            ImGui::Separator();
            ImGui::MenuItem("Exportar agente (Fase 28)", nullptr, false, false);
            ImGui::MenuItem("Carregar agente (Fase 28)", nullptr, false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Ajuda"))
        {
            if (ImGui::MenuItem("Atalhos")) queue.push(core::CmdOpenHelpWindow{});
            if (ImGui::MenuItem("Sobre")) queue.push(core::CmdToggleAboutPanel{});
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
        if (ImGui::Button(info.paused ? "Play" : "Pause")) queue.push(core::CmdPauseToggle{});
        ImGui::SameLine();
        if (ImGui::Button("Reset")) queue.push(core::CmdResetSimulation{});
        ImGui::SameLine();
        if (ImGui::Button("Passo")) queue.push(core::CmdStepOnce{});
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        const core::CanvasTool tools[] = {
            core::CanvasTool::Select, core::CanvasTool::RectangleSelect,
            core::CanvasTool::LassoSelect, core::CanvasTool::AddFood,
            core::CanvasTool::AddAgent, core::CanvasTool::PaintObstacle,
            core::CanvasTool::EraseObstacle, core::CanvasTool::Move, core::CanvasTool::Delete};
        for (const core::CanvasTool t : tools)
        {
            if (toolButton(core::canvasToolLabel(t), state.activeTool == t))
            {
                queue.push(core::CmdSetCanvasTool{t});
            }
            ImGui::SameLine();
        }
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (ImGui::Button("Fit")) queue.push(core::CmdFitWorldCamera{});
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
        ImGui::Text("Ag %zu   Com %zu   Obs %zu   %.0f FPS",
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
                if (ImGui::BeginTabItem("Editor Genetico"))
                {
                    ImGui::BeginChild("##editorscroll", ImVec2(0.0F, -52.0F));
                    // Phase 25.1: Gestalt grouping — params split into labeled
                    // sections (thin separator + small title), mirroring the
                    // Python genetic editor. The editor edits the "bacteria"
                    // species. (Per-group Aplicar + reset-network message: Fase 25.2.)
                    struct EditorGroup { const char* title; std::vector<const char*> suffixes; };
                    static const std::vector<EditorGroup> kGroups = {
                        {"Corpo e locomocao",
                            {"body_size", "body_shape", "max_speed", "max_turn",
                             "allow_reverse_locomotion", "movement_mode"}},
                        {"Energia e reproducao",
                            {"initial_energy", "death_energy", "split_energy", "v0_cost",
                             "vmax_cost", "energy_cap", "death_by_age_enabled", "death_age",
                             "corpse_to_food", "reproduction_min_age", "reproduction_cooldown"}},
                        {"Visao",
                            {"vision_radius", "retina_count", "retina_fov_degrees", "eye_count",
                             "eye_angle_degrees", "see_food", "see_agents", "see_predators",
                             "see_obstacles", "see_through_walls", "retina_channel_r",
                             "retina_channel_g", "retina_channel_b", "retina_channel_d",
                             "retina_input_mode"}},
                        {"Dieta",
                            {"diet_food", "diet_agents", "diet_same_label", "food_efficiency",
                             "agent_efficiency"}},
                        {"Rede neural",
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
                        ImGui::SeparatorText(g.title);
                        drawParamTable(g.title, registry, prefs, present, queue);
                    }
                    ImGui::EndChild();
                    ImGui::Separator();
                    if (ImGui::Button("Aplicar a especie")) queue.push(core::CmdApplyGenomeToSpecies{});
                    ImGui::SameLine();
                    if (ImGui::Button("Aplicar selecionados")) queue.push(core::CmdApplyGenomeToSelected{});
                    ImGui::SameLine();
                    if (ImGui::Button("Reverter")) queue.push(core::CmdRevertPreferences{});
                    ImGui::SameLine();
                    if (ImGui::Button("Padroes"))
                    {
                        for (const auto& n : UiLeftDock::editorParameters())
                            queue.push(core::CmdRestoreParameterDefault{n});
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Substrato"))
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
                    ImGui::SeparatorText("Substrato");
                    drawParamTable("##subworld", registry, prefs, worldParams, queue);
                    ImGui::SeparatorText("Comida");
                    drawParamTable("##subfood", registry, prefs, foodParams, queue);
                    ImGui::EndChild();
                    ImGui::Separator();
                    if (ImGui::Button("Aplicar ambiente")) queue.push(core::CmdApplyEnvironment{});
                    ImGui::SameLine();
                    if (ImGui::Button("Limpar comida")) queue.push(core::CmdClearAllFood{});
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
            ImGui::InputTextWithHint("##search", "Buscar parametro...", &prefs.searchQuery);
            ImGui::Separator();
            ImGui::BeginChild("##prefsscroll", ImVec2(0.0F, -44.0F));
            const auto names = prefsParametersForTabFiltered(registry, prefs, tab);
            if (names.empty())
            {
                ImGui::TextDisabled("Nenhum parametro nesta categoria.");
            }
            drawParamTable("##prefstbl", registry, prefs, names, queue);
            ImGui::EndChild();
            ImGui::Separator();
            if (ImGui::Button("Aplicar")) queue.push(core::CmdApplyPreferences{});
            ImGui::SameLine();
            if (ImGui::Button("Reverter")) queue.push(core::CmdRevertPreferences{});
            ImGui::SameLine();
            if (ImGui::Button("Restaurar padroes"))
            {
                for (const auto& n : names) queue.push(core::CmdRestoreParameterDefault{n});
                queue.push(core::CmdApplyPreferences{});
            }
            ImGui::SameLine();
            if (ImGui::Button("Fechar")) queue.push(core::CmdClosePreferencesWindow{t});
        }
        ImGui::End();
        if (!open) queue.push(core::CmdClosePreferencesWindow{t});
    }

    // ----------------------------------------------------------- help / about
    if (prefs.helpWindowOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(420.0F, 460.0F), ImGuiCond_FirstUseEver);
        bool open = true;
        if (ImGui::Begin("Atalhos###help", &open))
        {
            ImGui::TextUnformatted("Atalhos de teclado");
            ImGui::Separator();
            ImGui::BulletText("Espaco: pausar / retomar");
            ImGui::BulletText("Esc: limpar selecao e voltar para Selecao");
            ImGui::BulletText("Delete: excluir selecionados");
            ImGui::BulletText("R: resetar simulacao");
            ImGui::BulletText("F: ajustar mundo (fit)   T: render simples");
            ImGui::BulletText("V: debug de visao        H: esta ajuda");
            ImGui::Separator();
            ImGui::TextUnformatted("Ferramentas");
            ImGui::BulletText("S: Selecao   Q: Retangulo   L: Laco");
            ImGui::BulletText("G: Comida    A: Agente");
            ImGui::BulletText("B: Pincel    X: Apagar   M: Mover   D: Excluir");
            ImGui::Separator();
            ImGui::TextUnformatted("Camera");
            ImGui::BulletText("WASD / setas: mover camera");
            ImGui::BulletText("Scroll: zoom no cursor");
            ImGui::BulletText("Botao direito: arrastar (pan)");
        }
        ImGui::End();
        if (!open) queue.push(core::CmdCloseHelpWindow{});
    }

    if (state.showAboutPanel)
    {
        ImGui::SetNextWindowSize(ImVec2(420.0F, 200.0F), ImGuiCond_FirstUseEver);
        bool open = true;
        if (ImGui::Begin("Sobre###about", &open))
        {
            ImGui::TextUnformatted("AgentBioSim C++ / SFML");
            ImGui::Separator();
            ImGui::TextWrapped("Simulacao evolutiva 2D. Fase 25: interface migrada para Dear "
                                "ImGui sobre a arquitetura headless (engine + comandos).");
            ImGui::Spacing();
            ImGui::TextDisabled("Dear ImGui v1.90.9 + ImGui-SFML v2.6");
        }
        ImGui::End();
        if (!open) queue.push(core::CmdToggleAboutPanel{});
    }
}
} // namespace agentbiosim::ui
