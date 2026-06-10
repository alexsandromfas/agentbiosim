#include "ui/DevWindow.hpp"

#include "core/Profiler.hpp"
#include "i18n/Locale.hpp"
#include "sim/SimulationRunner.hpp"
#include "simulation/SpeciesStore.hpp"
#include "neural/BrainType.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace agentbiosim::ui
{
using i18n::tr;

namespace
{
// Friendly bilingual label per profiler section (plus the synthetic rows the
// table appends: render/ui per frame and the unprofiled glue/overhead).
const char* sectionLabel(const core::ProfileSection s)
{
    switch (s)
    {
    case core::ProfileSection::Perception:   return tr("Percepcao (visao)", "Perception (vision)");
    case core::ProfileSection::Neural:       return tr("Rede neural", "Neural network");
    case core::ProfileSection::Movement:     return tr("Locomocao", "Movement");
    case core::ProfileSection::Collision:    return tr("Colisao / fisica", "Collision / physics");
    case core::ProfileSection::Energy:       return tr("Energia", "Energy");
    case core::ProfileSection::Interaction:  return tr("Interacao (comer)", "Interaction (eating)");
    case core::ProfileSection::Food:         return tr("Comida (reposicao)", "Food (replenish)");
    case core::ProfileSection::Reproduction: return tr("Reproducao", "Reproduction");
    case core::ProfileSection::Death:        return tr("Morte", "Death");
    case core::ProfileSection::SpatialHash:  return tr("Spatial hash", "Spatial hash");
    case core::ProfileSection::Render:       return tr("Render (frame)", "Render (frame)");
    case core::ProfileSection::Ui:           return tr("UI (frame)", "UI (frame)");
    case core::ProfileSection::SimStep:      return tr("Passo total", "Total step");
    case core::ProfileSection::Count:        break;
    }
    return "?";
}

// Heat color for a cost share: green -> amber -> red as the share grows.
ImVec4 heatColor(const double pct)
{
    if (pct >= 30.0) return ImVec4(0.92F, 0.41F, 0.49F, 1.0F);  // red
    if (pct >= 15.0) return ImVec4(0.95F, 0.71F, 0.34F, 1.0F);  // amber
    return ImVec4(0.37F, 0.83F, 0.59F, 1.0F);                   // green
}

void pushHistory(std::vector<float>& v, const float value, const int cap)
{
    v.push_back(value);
    if (static_cast<int>(v.size()) > cap)
    {
        v.erase(v.begin(), v.begin() + (static_cast<int>(v.size()) - cap));
    }
}

std::string bytesHuman(const std::size_t bytes)
{
    char buf[32];
    if (bytes >= 1024ULL * 1024ULL)
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    return buf;
}

const char* const kBenchNeuralOptions[] = {"mlp", "gated_mlp", "shortcut_mlp", "modulated_mlp",
                                           "simple_rnn", "neat", "proto_neat", "recurrent_neat"};
const char* const kBenchVisionOptions[] = {"single", "raycast", "sector"};
} // namespace

void DevWindow::drawCostTable(const sim::SimulationRunner& runner)
{
    const core::Profiler& p = runner.profiler();
    ImGui::SeparatorText(tr("Custo por sistema", "Cost per system"));
    if (p.stepCount() == 0)
    {
        ImGui::TextDisabled("%s", tr("Coletando (aguarde alguns passos; a janela liga o profiler sozinha)...",
                                     "Collecting (wait a few steps; the window enables the profiler itself)..."));
        return;
    }

    ImGui::Checkbox(tr("Ordenar por custo", "Sort by cost"), &sortByCost_);

    struct Row
    {
        const char* name;
        double avgUs;
        double pct;
        bool perFrame;
    };
    std::vector<Row> rows;
    for (int i = 0; i <= static_cast<int>(core::ProfileSection::SpatialHash); ++i)
    {
        const auto s = static_cast<core::ProfileSection>(i);
        rows.push_back({sectionLabel(s), p.averageUs(s), p.percentOfStep(s), false});
    }
    // Unprofiled glue between the sim sections.
    const double steps = static_cast<double>(p.stepCount());
    const double overheadUs = static_cast<double>(p.overheadAccumNs()) / 1000.0 / steps;
    const double simStepNs = static_cast<double>(p.accumulatedNs(core::ProfileSection::SimStep));
    const double overheadPct = simStepNs > 0.0
        ? static_cast<double>(p.overheadAccumNs()) / simStepNs * 100.0 : 0.0;
    rows.push_back({tr("Outros (glue)", "Other (glue)"), overheadUs, overheadPct, false});
    // Render/Ui are per FRAME (App-side scopes), outside the step: informative.
    for (const auto s : {core::ProfileSection::Render, core::ProfileSection::Ui})
    {
        if (p.stat(s).calls > 0)
        {
            rows.push_back({sectionLabel(s), p.averageUs(s), 0.0, true});
        }
    }

    if (sortByCost_)
    {
        std::stable_sort(rows.begin(), rows.end(),
                         [](const Row& a, const Row& b) { return a.avgUs > b.avgUs; });
    }

    if (ImGui::BeginTable("##devcost", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(tr("Sistema", "System"), ImGuiTableColumnFlags_WidthStretch, 0.34F);
        ImGui::TableSetupColumn("us", ImGuiTableColumnFlags_WidthStretch, 0.16F);
        ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthStretch, 0.12F);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.38F);
        ImGui::TableHeadersRow();
        for (const Row& r : rows)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(r.name);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.1f", r.avgUs);
            ImGui::TableSetColumnIndex(2);
            if (r.perFrame) ImGui::TextDisabled("%s", tr("frame", "frame"));
            else ImGui::Text("%.1f", r.pct);
            ImGui::TableSetColumnIndex(3);
            if (!r.perFrame)
            {
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, heatColor(r.pct));
                ImGui::ProgressBar(static_cast<float>(r.pct) / 100.0F, ImVec2(-1.0F, 12.0F), "");
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }
    ImGui::TextDisabled("%s %.1f us  |  %s",
                        tr("Passo total:", "Total step:"),
                        p.averageUs(core::ProfileSection::SimStep),
                        tr("% relativo ao passo; render/UI sao por frame.",
                           "% is relative to the step; render/UI are per frame."));
}

void DevWindow::drawHistory(const sim::SimulationRunner& runner, const float fps)
{
    const core::Profiler& p = runner.profiler();
    ImGui::SeparatorText(tr("Historico", "History"));
    if (p.stepCount() > 0)
    {
        pushHistory(histTotal_, static_cast<float>(p.lastUs(core::ProfileSection::SimStep)), kHistorySize);
        pushHistory(histPerception_, static_cast<float>(p.lastUs(core::ProfileSection::Perception)), kHistorySize);
        pushHistory(histNeural_, static_cast<float>(p.lastUs(core::ProfileSection::Neural)), kHistorySize);
        pushHistory(histCollision_, static_cast<float>(p.lastUs(core::ProfileSection::Collision)), kHistorySize);
    }
    pushHistory(histFps_, fps, kHistorySize);

    ImGui::Text("%s %.0f   |   %s %.1f us (%s %.1f us)",
                "FPS:", static_cast<double>(fps),
                tr("passo:", "step:"), p.lastUs(core::ProfileSection::SimStep),
                tr("media", "avg"), p.averageUs(core::ProfileSection::SimStep));

    const auto plot = [](const char* label, const std::vector<float>& data) {
        if (data.empty()) return;
        char overlay[32];
        std::snprintf(overlay, sizeof(overlay), "%.0f", static_cast<double>(data.back()));
        ImGui::TextUnformatted(label);
        ImGui::PlotLines((std::string("##h") + label).c_str(), data.data(),
                         static_cast<int>(data.size()), 0, overlay, 0.0F, FLT_MAX,
                         ImVec2(-1.0F, 46.0F));
    };
    plot(tr("Passo total (us)", "Total step (us)"), histTotal_);
    plot(tr("Percepcao (us)", "Perception (us)"), histPerception_);
    plot(tr("Rede neural (us)", "Neural (us)"), histNeural_);
    plot(tr("Colisao (us)", "Collision (us)"), histCollision_);
}

void DevWindow::drawCounters(const sim::SimulationRunner& runner)
{
    ImGui::SeparatorText(tr("Mundo", "World"));
    ImGui::Text("%s %zu   |   %s %zu   |   %s %zu",
                tr("Agentes:", "Agents:"), runner.agents().size(),
                tr("Comida:", "Food:"), runner.foods().size(),
                tr("Obstaculos:", "Obstacles:"), runner.obstacles().size());
    for (const auto& rec : runner.species().records())
    {
        if (!rec.enabled) continue;
        ImGui::BulletText("%s: %zu", rec.label.empty() ? rec.name.c_str() : rec.label.c_str(),
                          runner.countAgentsOfSpecies(rec.id));
    }

    const auto counts = runner.brainTypeCounts();
    std::string brains;
    for (int t = 0; t < 8; ++t)
    {
        if (counts[static_cast<std::size_t>(t)] == 0) continue;
        if (!brains.empty()) brains += "   ";
        brains += std::string(neural::brainTypeLabel(static_cast<neural::BrainType>(t))) + ": " +
                  std::to_string(counts[static_cast<std::size_t>(t)]);
    }
    if (!brains.empty())
    {
        ImGui::Text("%s %s", tr("Cerebros:", "Brains:"), brains.c_str());
    }

    const auto stats = runner.spatialHash().stats();
    ImGui::Text("%s %zu/%zu %s (max %zu, %s %.1f)",
                tr("Spatial hash:", "Spatial hash:"), stats.occupiedCells, stats.totalCells,
                tr("celulas ocupadas", "occupied cells"), stats.maxBucketSize,
                tr("media/celula", "avg/cell"), stats.averageEntriesPerOccupiedCell);

    // Rough memory estimate: SoA bytes per entity (documented constants) + brain
    // parameters. It is an order-of-magnitude indicator, not an exact figure.
    const std::size_t agentBytes = runner.agents().size() * 160;
    const std::size_t foodBytes = runner.foods().size() * 112;
    const std::size_t obstacleBytes = runner.obstacles().size() * 64;
    const std::size_t brainBytes = runner.approxBrainBytes();
    ImGui::Text("%s ~%s (%s %s)",
                tr("Memoria (stores):", "Memory (stores):"),
                bytesHuman(agentBytes + foodBytes + obstacleBytes + brainBytes).c_str(),
                tr("cerebros", "brains"), bytesHuman(brainBytes).c_str());
}

void DevWindow::drawToggles(const sim::SimulationRunner& runner, core::CommandQueue& queue)
{
    ImGui::SeparatorText(tr("Isolamento de custo (diagnostico)", "Cost isolation (diagnostic)"));
    ImGui::TextDisabled("%s", tr("Desligar um sistema mostra o delta de custo, mas torna a simulacao "
                                 "incorreta enquanto desligado. Tudo e restaurado ao fechar a janela.",
                                 "Turning a system off shows the cost delta but makes the simulation "
                                 "incorrect while off. Everything is restored when the window closes."));
    if (runner.anyDevToggleOff())
    {
        ImGui::TextColored(ImVec4(0.95F, 0.71F, 0.34F, 1.0F), "%s",
                           tr("Atencao: ha sistemas desligados.", "Warning: some systems are off."));
    }
    int column = 0;
    for (int i = 0; i <= static_cast<int>(core::ProfileSection::SpatialHash); ++i)
    {
        bool on = runner.devSystemEnabled(i);
        ImGui::PushID(i);
        if (ImGui::Checkbox(sectionLabel(static_cast<core::ProfileSection>(i)), &on))
        {
            queue.push(core::CmdSetDevSystemEnabled{i, on});
        }
        ImGui::PopID();
        if (++column % 2 == 1) ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5F + 8.0F);
    }
    if (column % 2 == 1) ImGui::NewLine();
    if (ImGui::Button(tr("Restaurar todos os sistemas", "Restore all systems")))
    {
        queue.push(core::CmdSetDevSystemEnabled{-1, true});
    }
}

void DevWindow::drawEmbeddedBenchmark()
{
    ImGui::SeparatorText(tr("Cenario de benchmark (isolado)", "Benchmark scenario (isolated)"));
    ImGui::TextDisabled("%s", tr("Roda em segundo plano com estado proprio — nao afeta a simulacao atual.",
                                 "Runs in the background with its own state — does not affect the current simulation."));

    ImGui::SetNextItemWidth(90.0F);
    ImGui::InputInt(tr("Agentes", "Agents"), &benchAgents_, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0F);
    ImGui::InputInt(tr("Comida", "Food"), &benchFoods_, 0, 0);
    ImGui::SetNextItemWidth(90.0F);
    ImGui::InputInt(tr("Passos", "Steps"), &benchSteps_, 0, 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0F);
    ImGui::InputInt("Seed", &benchSeed_, 0, 0);
    ImGui::SetNextItemWidth(140.0F);
    ImGui::Combo(tr("Rede", "Brain"), &benchNeuralIdx_, kBenchNeuralOptions,
                 IM_ARRAYSIZE(kBenchNeuralOptions));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0F);
    ImGui::Combo(tr("Visao", "Vision"), &benchVisionIdx_, kBenchVisionOptions,
                 IM_ARRAYSIZE(kBenchVisionOptions));

    if (benchRunning_)
    {
        if (benchFuture_.valid() &&
            benchFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            benchResult_ = benchFuture_.get();
            benchRunning_ = false;
            benchHasResult_ = true;
        }
        else
        {
            ImGui::TextColored(ImVec4(0.37F, 0.69F, 0.95F, 1.0F), "%s",
                               tr("Rodando cenario...", "Running scenario..."));
        }
    }
    else if (ImGui::Button(tr("Rodar cenario", "Run scenario")))
    {
        bench::BenchmarkScenario sc;
        sc.name = "devwindow";
        sc.agents = std::max(1, benchAgents_);
        sc.foods = std::max(0, benchFoods_);
        sc.steps = std::clamp(benchSteps_, 10, 5000);
        sc.warmupSteps = 10;
        sc.repeats = 1;
        sc.seed = benchSeed_;
        sc.neuralType = kBenchNeuralOptions[benchNeuralIdx_];
        sc.visionMode = kBenchVisionOptions[benchVisionIdx_];
        benchFuture_ = std::async(std::launch::async,
                                  [sc]() { return bench::runScenario(sc); });
        benchRunning_ = true;
        benchHasResult_ = false;
    }

    if (benchHasResult_)
    {
        const bench::BenchmarkResult& r = benchResult_;
        ImGui::Text("%s %.1f us (%s %.1f / max %.1f)  |  %.0f %s",
                    tr("Passo medio:", "Mean step:"), r.meanStepUs,
                    tr("min", "min"), r.minStepUs, r.maxStepUs,
                    r.stepsPerSecond, tr("passos/s", "steps/s"));
        if (ImGui::BeginTable("##devbench", 3,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(tr("Sistema", "System"));
            ImGui::TableSetupColumn("us");
            ImGui::TableSetupColumn("%");
            ImGui::TableHeadersRow();
            for (const auto& sec : r.sections)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(sec.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.1f", sec.avgUsPerStep);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.1f", sec.percentOfStep);
            }
            ImGui::EndTable();
        }
    }
}

void DevWindow::draw(const config::ParameterRegistry& registry,
                     const sim::SimulationRunner& runner,
                     UiState& state,
                     core::CommandQueue& queue,
                     const float fps)
{
    static_cast<void>(registry);

    if (!state.showDevWindow)
    {
        if (wasOpen_)
        {
            // Closing: restore every dev toggle and drop the history buffers.
            queue.push(core::CmdSetDevSystemEnabled{-1, true});
            histTotal_.clear();
            histPerception_.clear();
            histNeural_.clear();
            histCollision_.clear();
            histFps_.clear();
            wasOpen_ = false;
        }
        return;
    }
    wasOpen_ = true;

    ImGui::SetNextWindowSize(ImVec2(520.0F, 640.0F), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(120.0F, 96.0F), ImGuiCond_FirstUseEver);
    bool open = true;
    const std::string title = std::string(tr("Janela do Desenvolvedor", "Developer Window")) + "###devwindow";
    if (ImGui::Begin(title.c_str(), &open))
    {
        drawCostTable(runner);
        ImGui::Spacing();
        drawHistory(runner, fps);
        ImGui::Spacing();
        drawCounters(runner);
        ImGui::Spacing();
        drawToggles(runner, queue);
        ImGui::Spacing();
        drawEmbeddedBenchmark();
    }
    ImGui::End();
    if (!open) state.showDevWindow = false;
}
} // namespace agentbiosim::ui
