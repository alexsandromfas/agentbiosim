#include "systems/Phase31Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterMetadata.hpp"
#include "config/ParameterRegistry.hpp"
#include "core/Command.hpp"
#include "io/SaveFile.hpp"
#include "render/Camera2D.hpp"
#include "sim/SimulationRunner.hpp"
#include "ui/InputRouter.hpp"
#include "ui/UiPreferencesPanel.hpp"  // prefs model free functions
#include "ui/UiState.hpp"

#include <SFML/Window/Event.hpp>

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
double brainChecksum(const neural::BrainSnapshot& b)
{
    double s = 0.0;
    for (const auto& layer : b.weights) for (const double w : layer) s += w;
    for (const auto& layer : b.biases) for (const double w : layer) s += w;
    for (const auto& layer : b.gates) for (const double w : layer) s += w;
    for (const double w : b.shortcutWeights) s += w;
    for (const double w : b.recurrentWeights) s += w;
    for (const auto& c : b.neatConnections) s += c.weight;
    return s;
}

// Feeds one KeyPressed event through the real InputRouter and returns the
// drained commands. No window is needed — sf::Event is a plain struct.
std::vector<core::Command> pressKey(ui::InputRouter& router,
                                    const sim::SimulationRunner& runner,
                                    ui::UiState& state,
                                    const sf::Keyboard::Key key)
{
    render::Camera2D camera;
    core::CommandQueue queue;
    sf::Event ev{};
    ev.type = sf::Event::KeyPressed;
    ev.key.code = key;
    ev.key.shift = false;
    ev.key.control = false;
    ev.key.alt = false;
    ev.key.system = false;
    router.handleEvent(ev, {800U, 600U}, camera, runner, state, queue);
    return queue.drain();
}

template <typename Cmd>
bool hasCommand(const std::vector<core::Command>& cmds)
{
    for (const auto& c : cmds)
    {
        if (std::holds_alternative<Cmd>(c)) return true;
    }
    return false;
}
} // namespace

Phase31ValidationSummary runPhase31Validation()
{
    Phase31ValidationSummary summary;
    std::ostringstream log;
    auto check = [&](const bool cond, const std::string& name) {
        ++summary.checks;
        if (cond) { log << "  ok: " << name << '\n'; }
        else { summary.passed = false; log << "  FAIL: " << name << '\n'; }
    };

    config::ParameterRegistry reg = config::createDefaultParameterRegistry();
    static_cast<void>(reg.setValue("auto_export_substrate", false));
    sim::SimulationRunner runner(reg);
    runner.initialize();
    runner.step(1.0 / 30.0);  // brains exist after the first step

    // ---------------- A. keyboard shortcuts (real InputRouter, headless) ------
    {
        ui::InputRouter router;
        ui::UiState state;

        check(hasCommand<core::CmdPauseToggle>(pressKey(router, runner, state, sf::Keyboard::Space)),
              "Space -> pausar/retomar");
        const auto esc = pressKey(router, runner, state, sf::Keyboard::Escape);
        check(hasCommand<core::CmdClearSelection>(esc) && hasCommand<core::CmdSetCanvasTool>(esc),
              "Esc -> limpa selecao + volta para Selecao");
        check(hasCommand<core::CmdDeleteSelected>(pressKey(router, runner, state, sf::Keyboard::Delete)),
              "Delete -> excluir selecionados");
        check(hasCommand<core::CmdResetSimulation>(pressKey(router, runner, state, sf::Keyboard::R)),
              "R -> resetar simulacao");
        check(hasCommand<core::CmdFitWorldCamera>(pressKey(router, runner, state, sf::Keyboard::F)),
              "F -> ajustar mundo");
        check(hasCommand<core::CmdToggleSimpleRender>(pressKey(router, runner, state, sf::Keyboard::T)),
              "T -> render simples");
        check(hasCommand<core::CmdToggleVisionDebug>(pressKey(router, runner, state, sf::Keyboard::V)),
              "V -> debug de visao (overlay do selecionado)");
        check(hasCommand<core::CmdToggleHelpPanel>(pressKey(router, runner, state, sf::Keyboard::H)),
              "H -> ajuda");

        const auto toolKey = [&](const sf::Keyboard::Key key, const core::CanvasTool tool,
                                 const char* name) {
            const auto cmds = pressKey(router, runner, state, key);
            bool ok = false;
            for (const auto& c : cmds)
            {
                if (const auto* t = std::get_if<core::CmdSetCanvasTool>(&c))
                {
                    ok = t->tool == tool;
                }
            }
            check(ok, std::string(name) + " -> ferramenta correta");
        };
        toolKey(sf::Keyboard::S, core::CanvasTool::Select, "S (Selecao)");
        toolKey(sf::Keyboard::Q, core::CanvasTool::RectangleSelect, "Q (Retangulo)");
        toolKey(sf::Keyboard::L, core::CanvasTool::LassoSelect, "L (Laco)");
        toolKey(sf::Keyboard::G, core::CanvasTool::AddFood, "G (Comida)");
        toolKey(sf::Keyboard::A, core::CanvasTool::AddAgent, "A (Agente)");
        toolKey(sf::Keyboard::B, core::CanvasTool::PaintObstacle, "B (Pincel)");
        toolKey(sf::Keyboard::X, core::CanvasTool::EraseObstacle, "X (Apagar)");
        toolKey(sf::Keyboard::M, core::CanvasTool::Move, "M (Mover)");
        toolKey(sf::Keyboard::D, core::CanvasTool::Delete, "D (Excluir)");

        check(hasCommand<core::CmdPanCameraScreen>(pressKey(router, runner, state, sf::Keyboard::Up)),
              "Setas -> mover camera");
        check(pressKey(router, runner, state, sf::Keyboard::W).empty(),
              "W nao e mais atalho (camera = setas; letras = ferramentas)");
    }

    // ---------------- B. preferences model resolves for every tab -------------
    {
        for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
        {
            const auto names = ui::prefsParametersForTab(reg, static_cast<config::PrefsTab>(t), "");
            check(!names.empty(), std::string("aba de preferencias com parametros: ") +
                                      config::prefsTabLabel(static_cast<config::PrefsTab>(t)));
        }
    }

    // ---------------- C. window-opening commands accepted ---------------------
    {
        check(runner.applyCommand(core::Command{core::CmdOpenPreferencesWindow{0}}),
              "comando abrir janela de preferencias aceito");
        check(runner.applyCommand(core::Command{core::CmdOpenHelpWindow{}}),
              "comando abrir ajuda aceito");
        check(runner.applyCommand(core::Command{core::CmdToggleAboutPanel{}}),
              "comando sobre aceito");
    }

    // ---------------- D. critical flows end to end ----------------------------
    {
        // Spawn agent + pick it.
        const std::size_t before = runner.agents().size();
        check(runner.applyCommand(core::Command{core::CmdSpawnAgentAt{{500.0, 350.0}, 9.0}}),
              "fluxo: adicionar agente (comando aceito)");
        check(runner.agents().size() == before + 1, "fluxo: agente adicionado");
        const auto picked = runner.pickAgentAt({500.0, 350.0}, 12.0);
        check(picked.isValid(), "fluxo: selecionar agente no ponto");

        // Spawn + clear food.
        check(runner.applyCommand(core::Command{core::CmdSpawnFoodAt{{200.0, 200.0}, 5.0, 25.0}}),
              "fluxo: adicionar comida");
        check(runner.applyCommand(core::Command{core::CmdClearFood{}}), "fluxo: limpar comida");
        check(runner.foods().empty(), "fluxo: comida limpa");

        // Paint + erase obstacle.
        const std::size_t obsBefore = runner.obstacles().size();
        check(runner.applyCommand(core::Command{core::CmdPaintObstacleAt{{300.0, 300.0}, 20.0}}),
              "fluxo: pintar obstaculo");
        check(runner.obstacles().size() == obsBefore + 1, "fluxo: obstaculo pintado");
        check(runner.applyCommand(core::Command{core::CmdEraseObstacleAt{{300.0, 300.0}, 30.0}}),
              "fluxo: apagar obstaculo");
        check(runner.obstacles().size() == obsBefore, "fluxo: obstaculo apagado");

        // Species ops: create from selected, label/color/population, remove.
        std::vector<simulation::EntityId> ids{picked};
        const auto newSpecies = runner.createSpeciesFromSelected("teste_paridade", ids);
        check(newSpecies != simulation::kInvalidSpeciesId, "fluxo: nova especie com selecionados");
        check(runner.setSpeciesLabel(newSpecies, "Paridade"), "fluxo: renomear especie");
        check(runner.setSpeciesColorAndRecolor(newSpecies, {10, 200, 30}), "fluxo: cor da especie");
        check(runner.adjustSpeciesPopulation(newSpecies, 0, +2), "fluxo: ajustar populacao minima");
        check(runner.countAgentsOfSpecies(newSpecies) == 1, "fluxo: contagem da especie");

        // Per-species neural reset replaces the brains (Phase 31 fix).
        const auto bacteriaId = runner.species().idByName("bacteria");
        std::unordered_map<std::uint64_t, double> before2;
        for (const auto& kv : runner.snapshot().brains) before2[kv.first] = brainChecksum(kv.second);
        check(runner.applyCommand(core::Command{core::CmdResetNeuralForSpecies{
                  static_cast<std::uint32_t>(bacteriaId)}}),
              "fluxo: resetar rede da especie (comando aceito)");
        std::size_t changed = 0;
        std::size_t total = 0;
        for (const auto& kv : runner.snapshot().brains)
        {
            const auto it = before2.find(kv.first);
            if (it == before2.end()) continue;
            ++total;
            if (std::abs(it->second - brainChecksum(kv.second)) > 1e-12) ++changed;
        }
        check(total > 0 && changed > 0, "fluxo: reset substituiu cerebros (" +
                                             std::to_string(changed) + "/" + std::to_string(total) + ")");
        runner.step(1.0 / 30.0);
        check(true, "fluxo: simulacao segue apos reset neural");

        // Remove the test species (agents reassigned to bacteria).
        check(runner.removeSpeciesSafe(newSpecies), "fluxo: excluir especie de teste");

        // Organism export -> import roundtrip preserves the mind.
        sim::AgentExport exported;
        check(runner.exportAgent(picked, exported), "fluxo: exportar organismo");
        const std::string orgPath = "phase31_tmp.organism";
        std::string err;
        check(io::saveAgentToFile(orgPath, exported, err), "fluxo: gravar .organism " + err);
        const io::AgentLoadResult loaded = io::loadAgentFromFile(orgPath);
        check(loaded.ok, "fluxo: ler .organism " + loaded.error);
        const std::size_t beforeImport = runner.agents().size();
        const auto importedId = runner.importAgent(loaded.agent, {400.0, 300.0});
        check(importedId.isValid() && runner.agents().size() == beforeImport + 1,
              "fluxo: importar organismo");
        sim::AgentExport reExported;
        check(runner.exportAgent(importedId, reExported) &&
                  std::abs(brainChecksum(reExported.brain) - brainChecksum(exported.brain)) < 1e-9,
              "fluxo: mente preservada no roundtrip do organismo");
        std::remove(orgPath.c_str());

        // Save -> open roundtrip (quick — full coverage is Phase 28's selftest).
        const std::string simPath = "phase31_tmp.agentbiosim";
        io::SaveBundle bundle;
        bundle.snapshot = runner.snapshot();
        check(io::saveToFile(simPath, bundle, err), "fluxo: salvar simulacao " + err);
        const io::LoadResult lr = io::loadFromFile(simPath);
        check(lr.ok, "fluxo: abrir simulacao " + lr.error);
        sim::SimulationRunner other(reg);
        other.initialize();
        other.restore(lr.bundle.snapshot);
        check(other.agents().size() == runner.agents().size(), "fluxo: roundtrip de save");
        std::remove(simPath.c_str());

        // Genome / environment / population apply commands accepted.
        check(runner.applyCommand(core::Command{core::CmdApplyGenomeToSpecies{}}),
              "fluxo: aplicar genoma a especie aceito");
        check(runner.applyCommand(core::Command{core::CmdApplyEnvironment{}}),
              "fluxo: aplicar ambiente aceito");
        check(runner.applyCommand(core::Command{core::CmdApplyPopulation{}}),
              "fluxo: aplicar populacao aceito");
    }

    // ---------------- E. Microfase 31.1: mundo/comida vivos + min/max por label
    {
        config::ParameterRegistry reg2 = config::createDefaultParameterRegistry();
        static_cast<void>(reg2.setValue("auto_export_substrate", false));
        sim::SimulationRunner r(reg2);
        r.initialize();
        for (int i = 0; i < 3; ++i) r.step(1.0 / 30.0);

        // 1. Encolher o substrato AO VIVO: sem reset, organismos empurrados pra dentro.
        const std::size_t agentsBefore = r.agents().size();
        const auto stepsBefore = r.stats().stepsExecuted;
        const auto keepId = r.agents().idAt(0);
        static_cast<void>(reg2.setValue("world_w", 400.0));
        static_cast<void>(reg2.setValue("world_h", 300.0));
        r.applyWorldConfigLive();
        check(r.agents().size() == agentsBefore && r.stats().stepsExecuted == stepsBefore &&
                  r.agents().contains(keepId),
              "31.1: encolher mundo NAO reseta (agentes/passos/ids preservados)");
        bool allInside = true;
        for (std::size_t i = 0; i < r.agents().size(); ++i)
        {
            const auto p = r.agents().positionAt(i);
            if (p.x < 0.0 || p.x > 400.0 || p.y < 0.0 || p.y > 300.0) { allInside = false; break; }
        }
        for (std::size_t i = 0; i < r.foods().size() && allInside; ++i)
        {
            const auto p = r.foods().positionAt(i);
            if (p.x < 0.0 || p.x > 400.0 || p.y < 0.0 || p.y > 300.0) { allInside = false; break; }
        }
        check(allInside, "31.1: agentes e comida empurrados pra dentro do mundo menor");
        r.step(1.0 / 30.0);
        check(true, "31.1: simulacao segue apos reshape");

        // 2. CmdApplyEnvironment tambem reconfigura ao vivo (sem reset).
        static_cast<void>(reg2.setValue("world_w", 600.0));
        const auto steps2 = r.stats().stepsExecuted;
        check(r.applyCommand(core::Command{core::CmdApplyEnvironment{}}) &&
                  r.stats().stepsExecuted == steps2 && r.agents().contains(keepId),
              "31.1: Aplicar ambiente nao reseta");

        // 3. Comida ao vivo: subir o alvo repoe sem reset. A interacao (comer) e
        //    pausada via dev-toggle para a contagem nao ser mascarada pelos ~150
        //    agentes comendo mais rapido que o teto de reposicao por passo.
        const int targetBefore = static_cast<int>(r.foods().size());
        static_cast<void>(reg2.setValue("food_target", targetBefore + 40));
        static_cast<void>(r.applyCommand(core::Command{core::CmdSetDevSystemEnabled{
            static_cast<int>(core::ProfileSection::Interaction), false}}));
        for (int i = 0; i < 3; ++i) r.step(1.0 / 30.0);
        static_cast<void>(r.applyCommand(core::Command{core::CmdSetDevSystemEnabled{-1, true}}));
        check(static_cast<int>(r.foods().size()) > targetBefore && r.agents().contains(keepId),
              "31.1: mudar quantidade de comida aplica ao vivo (sem reset)");

        // 4. Todo organismo pertence a uma label valida.
        bool allLabeled = true;
        for (std::size_t i = 0; i < r.agents().size(); ++i)
        {
            if (!r.species().contains(r.agents().speciesIdAt(i))) { allLabeled = false; break; }
        }
        check(allLabeled, "31.1: todo organismo pertence a uma label existente");

        // 5. Maximo POR LABEL no nascimento: com max = contagem atual e split
        //    facilitado, a label nao cresce.
        const auto bacteriaId = r.species().idByName("bacteria");
        const std::size_t capCount = r.countAgentsOfSpecies(bacteriaId);
        static_cast<void>(r.speciesMutable().setMaxPopulation(
            bacteriaId, static_cast<int>(capCount)));
        static_cast<void>(reg2.setValue("bacteria_split_energy", 1.0));
        for (int i = 0; i < 4; ++i) r.step(1.0 / 30.0);
        check(r.countAgentsOfSpecies(bacteriaId) <= capCount,
              "31.1: maximo da label respeitado no nascimento (" +
                  std::to_string(r.countAgentsOfSpecies(bacteriaId)) + " <= " +
                  std::to_string(capCount) + ")");

        // 6. Resgate de MINIMO por label: abaixo do minimo, repoe ate ele.
        static_cast<void>(r.speciesMutable().setMaxPopulation(bacteriaId, 0));
        static_cast<void>(r.speciesMutable().setMinPopulation(bacteriaId, 30));
        std::vector<simulation::EntityId> toDelete;
        for (std::size_t i = 0; i < r.agents().size(); ++i)
        {
            if (r.agents().speciesIdAt(i) == bacteriaId && toDelete.size() + 5 <
                r.countAgentsOfSpecies(bacteriaId))
            {
                toDelete.push_back(r.agents().idAt(i));
            }
        }
        r.deleteAgents(toDelete);
        check(r.countAgentsOfSpecies(bacteriaId) < 30, "31.1: populacao reduzida p/ teste");
        r.step(1.0 / 30.0);
        check(r.countAgentsOfSpecies(bacteriaId) >= 30,
              "31.1: resgate repoe ate o minimo da label (" +
                  std::to_string(r.countAgentsOfSpecies(bacteriaId)) + " >= 30)");
        // Repostos pertencem a label e estao dentro do mundo.
        bool rescuedOk = true;
        for (std::size_t i = 0; i < r.agents().size(); ++i)
        {
            if (!r.species().contains(r.agents().speciesIdAt(i))) { rescuedOk = false; break; }
            const auto p = r.agents().positionAt(i);
            if (p.x < 0.0 || p.x > 600.0 || p.y < 0.0 || p.y > 300.0) { rescuedOk = false; break; }
        }
        check(rescuedOk, "31.1: resgatados com label valida e dentro do mundo");
    }

    // ------------- F. Microfase 32.1: defaults de min/max por label -----------
    {
        config::ParameterRegistry reg3 = config::createDefaultParameterRegistry();
        static_cast<void>(reg3.setValue("auto_export_substrate", false));
        // Pressao de reproducao: split barato + comida abundante.
        static_cast<void>(reg3.setValue("bacteria_split_energy", 1.0));
        static_cast<void>(reg3.setValue("food_target", 400));
        sim::SimulationRunner r(reg3);
        r.initialize();

        const auto bacteriaId = r.species().idByName("bacteria");
        const auto* rec = r.species().find(bacteriaId);
        check(rec != nullptr && rec->minPopulation == 5 && rec->maxPopulation == 150,
              "32.1: label padrao nasce com min=5 / max=150");

        bool capHeld = true;
        for (int i = 0; i < 120 && capHeld; ++i)
        {
            r.step(1.0 / 30.0);
            if (r.countAgentsOfSpecies(bacteriaId) > 150) capHeld = false;
        }
        check(capHeld, "32.1: maximo padrao (150) segura sob pressao de reproducao (" +
                           std::to_string(r.countAgentsOfSpecies(bacteriaId)) + " <= 150)");

        // Resgate padrao: derrubar para 2 -> volta para >= 5 num passo.
        std::vector<simulation::EntityId> doomed;
        for (std::size_t i = 0; i < r.agents().size(); ++i)
        {
            if (doomed.size() + 2 < r.countAgentsOfSpecies(bacteriaId) &&
                r.agents().speciesIdAt(i) == bacteriaId)
            {
                doomed.push_back(r.agents().idAt(i));
            }
        }
        r.deleteAgents(doomed);
        r.step(1.0 / 30.0);
        check(r.countAgentsOfSpecies(bacteriaId) >= 5,
              "32.1: minimo padrao (5) resgatado (" +
                  std::to_string(r.countAgentsOfSpecies(bacteriaId)) + " >= 5)");

        // Labels criadas pelo usuario tambem nascem com 5/150.
        std::vector<simulation::EntityId> one{r.agents().idAt(0)};
        const auto newId = r.createSpeciesFromSelected("label_321", one);
        const auto* newRec = r.species().find(newId);
        check(newRec != nullptr && newRec->minPopulation == 5 && newRec->maxPopulation == 150,
              "32.1: label criada nasce com min=5 / max=150");
    }

    summary.details = log.str();
    return summary;
}
} // namespace agentbiosim::systems
