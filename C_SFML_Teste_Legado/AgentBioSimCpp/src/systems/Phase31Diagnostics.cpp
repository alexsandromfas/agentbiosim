#include "systems/Phase31Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterHelpers.hpp"
#include "config/ParameterMetadata.hpp"
#include "config/ParameterRegistry.hpp"
#include "core/Command.hpp"
#include "core/Profiler.hpp"
#include "io/SaveFile.hpp"
#include "perception/PerceptionSystem.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"
#include "systems/CollisionSystem.hpp"
#include "systems/DeathSystem.hpp"
#include "systems/FoodSystem.hpp"
#include "systems/InteractionSystem.hpp"
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
#include <unordered_set>
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

        // 6. Microfase 32.4: o piso BLOQUEIA a morte (em vez de respawnar do nada).
        //    O comportamento antigo (repor ate o minimo criando organismos do nada)
        //    foi removido — o usuario reportou "quando morre um esta surgindo outro do
        //    nada". Prova robusta (independe de reproducao): com death_energy enorme,
        //    todo agente vira candidato a morte; com o minimo ACIMA da populacao atual,
        //    o contador de mortes nao sobe (o piso bloqueia toda morte).
        const auto popNow = r.countAgentsOfSpecies(bacteriaId);
        static_cast<void>(r.speciesMutable().setMaxPopulation(bacteriaId, 0));
        static_cast<void>(r.speciesMutable().setMinPopulation(
            bacteriaId, static_cast<int>(popNow) + 50));
        static_cast<void>(reg2.setValue("bacteria_death_energy", 1.0e9));  // todos famintos
        const auto deathsBefore = r.stats().deaths;
        r.step(1.0 / 30.0);
        check(r.stats().deaths == deathsBefore,
              "32.4: piso bloqueia morte (min > pop, death_energy enorme -> 0 mortes, sem respawn)");
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
        // (O antigo "resgate de minimo" foi removido na 32.4 — sem respawn do nada.
        //  O piso por bloqueio de morte e o no-respawn sao cobertos no bloco I.)

        // Labels criadas pelo usuario tambem nascem com 5/150.
        std::vector<simulation::EntityId> one{r.agents().idAt(0)};
        const auto newId = r.createSpeciesFromSelected("label_321", one);
        const auto* newRec = r.species().find(newId);
        check(newRec != nullptr && newRec->minPopulation == 5 && newRec->maxPopulation == 150,
              "32.1: label criada nasce com min=5 / max=150");
    }

    // ------------- G. Microfase 32.2: editor de genoma aplica AO VIVO ---------
    // Reproduz o fluxo reportado pelo usuario: criar label a partir de
    // selecionados, mudar a dieta no editor e aplicar — pre-32.2 isso resetava
    // o mundo (Aplicar a especie) ou DELETAVA os selecionados (Aplicar
    // selecionados), apagando a label recem-criada.
    {
        config::ParameterRegistry reg4 = config::createDefaultParameterRegistry();
        static_cast<void>(reg4.setValue("auto_export_substrate", false));
        sim::SimulationRunner r(reg4);
        r.initialize();
        r.step(1.0 / 30.0);  // materializa os cerebros (syncBrains)

        const auto bacteriaId = r.species().idByName("bacteria");
        const auto bacteriaGenome = r.species().find(bacteriaId)->defaultGenomeId;

        // G1: a label criada tem genoma-template PROPRIO (pre-32.2 compartilhava
        // o registro da bacteria, entao editar uma editava a outra).
        std::vector<simulation::EntityId> sel;
        for (std::size_t i = 0; i < r.agents().size() && sel.size() < 10U; ++i)
        {
            sel.push_back(r.agents().idAt(i));
        }
        const auto labelId = r.createSpeciesFromSelected("label_322", sel);
        const auto* labelRec = r.species().find(labelId);
        check(labelRec != nullptr &&
                  labelRec->defaultGenomeId != simulation::kInvalidGenomeId &&
                  labelRec->defaultGenomeId != bacteriaGenome,
              "32.2: label criada tem genoma-template proprio");

        // G2: atribuir um organismo a uma label preserva o genoma pessoal dele.
        const auto movedId = r.agents().idAt(20);
        const auto movedIdx0 = r.agents().indexOf(movedId);
        const auto genomeBefore = r.agents().genomeIdAt(*movedIdx0);
        r.assignSelectedToSpecies({movedId}, labelId);
        const auto movedIdx = r.agents().indexOf(movedId);
        check(movedIdx.has_value() &&
                  r.agents().speciesIdAt(*movedIdx) == labelId &&
                  r.agents().genomeIdAt(*movedIdx) == genomeBefore,
              "32.2: atribuir a label nao apaga o genoma do organismo");

        // G3: "Aplicar a especie" AO VIVO — dieta passa a comer organismos;
        // ninguem e deletado, a simulacao nao reseta, o template da bacteria
        // (registro compartilhado pelos iniciais) fica intacto.
        const auto stepsBefore = r.stats().stepsExecuted;
        const std::size_t agentsBefore = r.agents().size();
        const std::size_t labelBefore = r.countAgentsOfSpecies(labelId);
        sim::AgentExport mindBefore;
        check(r.exportAgent(movedId, mindBefore), "32.2: exportar mente antes do aplicar");
        static_cast<void>(reg4.setValue("bacteria_diet_agents", true));
        const std::size_t applied = r.applyEditorGenomeToSpecies(labelId);
        check(applied == labelBefore && r.agents().size() == agentsBefore &&
                  r.countAgentsOfSpecies(labelId) == labelBefore,
              "32.2: aplicar a especie nao deleta ninguem (" +
                  std::to_string(applied) + " de " + std::to_string(labelBefore) + ")");
        check(r.stats().stepsExecuted == stepsBefore,
              "32.2: aplicar a especie nao reseta a simulacao");
        bool membersCarnivorous = true;
        for (std::size_t i = 0; i < r.agents().size(); ++i)
        {
            if (!r.agents().aliveAt(i) || r.agents().speciesIdAt(i) != labelId) continue;
            const auto* g = r.genomes().find(r.agents().genomeIdAt(i));
            if (g == nullptr || !g->diet.eatAgents) membersCarnivorous = false;
        }
        check(membersCarnivorous, "32.2: todos os membros da label receberam a dieta nova");
        const auto* bacteriaTemplate = r.genomes().find(bacteriaGenome);
        check(bacteriaTemplate != nullptr && !bacteriaTemplate->diet.eatAgents,
              "32.2: template da bacteria NAO foi afetado pelo aplicar na label");
        check(r.species().find(labelId)->dietSnapshot.eatAgents,
              "32.2: dietSnapshot da label atualizado");

        // G4: o cerebro (pesos evoluidos) sobrevive ao aplicar.
        sim::AgentExport mindAfter;
        check(r.exportAgent(movedId, mindAfter) &&
                  std::abs(brainChecksum(mindAfter.brain) - brainChecksum(mindBefore.brain)) < 1e-9,
              "32.2: cerebro preservado no aplicar (checksum identico)");

        // G5: a reproducao honra o genoma POR LABEL — split barato aplicado so
        // na label faz so a label reproduzir (bacteria continua split=150).
        static_cast<void>(reg4.setValue("bacteria_split_energy", 1.0));
        static_cast<void>(r.applyEditorGenomeToSpecies(labelId));
        const std::size_t labelPre = r.countAgentsOfSpecies(labelId);
        const std::size_t bacteriaPre = r.countAgentsOfSpecies(bacteriaId);
        for (int s = 0; s <= static_cast<int>(core::ProfileSection::SpatialHash); ++s)
        {
            const auto sec = static_cast<core::ProfileSection>(s);
            const bool keep = sec == core::ProfileSection::Reproduction ||
                              sec == core::ProfileSection::SpatialHash;
            r.setDevSystemEnabled(s, keep);
        }
        r.step(1.0 / 30.0);
        const std::size_t labelPost = r.countAgentsOfSpecies(labelId);
        const std::size_t bacteriaPost = r.countAgentsOfSpecies(bacteriaId);
        for (int s = 0; s <= static_cast<int>(core::ProfileSection::SpatialHash); ++s)
        {
            r.setDevSystemEnabled(s, true);
        }
        check(labelPost > labelPre,
              "32.2: label com split barato reproduz (" + std::to_string(labelPre) +
                  " -> " + std::to_string(labelPost) + ")");
        check(bacteriaPost == bacteriaPre,
              "32.2: bacteria (split=150) nao reproduz junto (" +
                  std::to_string(bacteriaPre) + " -> " + std::to_string(bacteriaPost) + ")");

        // G6: "Aplicar selecionados" muda SO os selecionados, sem tocar o
        // template nem os demais (copy-on-write do registro compartilhado).
        static_cast<void>(reg4.setValue("bacteria_split_energy", 150.0));
        static_cast<void>(reg4.setValue("bacteria_diet_agents", false));
        static_cast<void>(reg4.setValue("bacteria_body_size", 12.0));
        std::vector<simulation::EntityId> trio;
        for (std::size_t i = 0; i < r.agents().size() && trio.size() < 3U; ++i)
        {
            if (r.agents().aliveAt(i) && r.agents().speciesIdAt(i) == bacteriaId)
            {
                trio.push_back(r.agents().idAt(i));
            }
        }
        const std::size_t agentsBeforeSel = r.agents().size();
        const std::size_t appliedSel = r.applyEditorGenomeToAgents(trio);
        check(appliedSel == trio.size() && r.agents().size() == agentsBeforeSel,
              "32.2: aplicar selecionados nao deleta ninguem");
        bool trioOk = true;
        for (const auto id : trio)
        {
            const auto idx = r.agents().indexOf(id);
            const auto* g = idx.has_value()
                ? r.genomes().find(r.agents().genomeIdAt(*idx)) : nullptr;
            if (!idx.has_value() || g == nullptr || g->bodySize != 12.0 ||
                r.agents().radiusAt(*idx) != 12.0 ||
                r.agents().genomeIdAt(*idx) == bacteriaGenome ||
                r.agents().speciesIdAt(*idx) != bacteriaId)
            {
                trioOk = false;
            }
        }
        check(trioOk, "32.2: selecionados ganharam genoma proprio com corpo novo (label mantida)");
        const auto* templateAfterSel = r.genomes().find(bacteriaGenome);
        check(templateAfterSel != nullptr && templateAfterSel->bodySize == 9.0,
              "32.2: template da bacteria intacto apos aplicar selecionados");
    }

    // ------------- H. Microfase 32.3: predacao dirigida pela dieta ------------
    // Bug reportado: mudar a dieta de uma label para "comer organismos" nao
    // fazia ela predar. Causa: a predacao tinha um gate global vindo do flag
    // legado predators_enabled (default false), que so deveria controlar o
    // SPAWN da especie predadora legada — nao a predacao por dieta.
    {
        // H1 (unidade): com predators_enabled=false, o config do registry ainda
        // deixa a predacao DISPONIVEL (a dieta de cada agente decide).
        config::ParameterRegistry regP = config::createDefaultParameterRegistry();
        check(!config::parameterBool(regP, "predators_enabled", true),
              "32.3: predators_enabled e false por padrao");
        const DietInteractionConfig dcfg0 =
            InteractionSystem::dietConfigFromRegistry(regP);
        check(dcfg0.predationEnabled,
              "32.3: predacao disponivel apesar de predators_enabled=false");

        // H2 (ponta-a-ponta): um predador (genoma com eatAgents) encostado numa
        // presa de OUTRA label preda num passo, pelo caminho real do registry.
        simulation::GenomeStore genomes;
        simulation::GenomeRecord predatorGenome;
        predatorGenome.diet.eatFood = false;
        predatorGenome.diet.eatAgents = true;       // dieta = comer organismos
        predatorGenome.diet.eatSameSpecies = false;
        predatorGenome.diet.agentEfficiency = 0.7;
        predatorGenome.energyCap = 400.0;
        const auto predGid = genomes.createGenome(predatorGenome);
        simulation::GenomeRecord preyGenome;
        preyGenome.diet.eatFood = true;
        preyGenome.diet.eatAgents = false;
        const auto preyGid = genomes.createGenome(preyGenome);

        simulation::AgentStore agents;
        simulation::AgentSpawn pred;
        pred.position = {100.0, 100.0};
        pred.radius = 10.0;
        pred.energy = 100.0;
        pred.speciesId = 1;                          // label do predador
        pred.genomeId = predGid.id;
        const auto predId = agents.createAgent(pred);
        simulation::AgentSpawn prey;
        prey.position = {108.0, 100.0};              // encostado (dist 8 < r+r 20)
        prey.radius = 10.0;
        prey.energy = 60.0;
        prey.speciesId = 2;                          // outra label
        prey.genomeId = preyGid.id;
        const auto preyId = agents.createAgent(prey);

        simulation::FoodStore foods;
        DietInteractionConfig dcfg = InteractionSystem::dietConfigFromRegistry(regP);
        dcfg.useSpatial = false;                     // caminho O(n^2), sem hash
        InteractionSystem sys;
        const double predEnergyBefore = agents.energyAt(*agents.indexOf(predId));
        const auto pstats = sys.applyWithDiet(agents, foods, genomes, nullptr, dcfg);
        check(pstats.predationEvents == 1, "32.3: predador comeu 1 presa num passo");
        check(!agents.contains(preyId), "32.3: presa removida apos predacao");
        const auto predIdxAfter = agents.indexOf(predId);
        check(predIdxAfter.has_value() &&
                  agents.energyAt(*predIdxAfter) > predEnergyBefore,
              "32.3: predador ganhou energia da presa");

        // H3: mesma label NAO se preda (eatSameSpecies=false) — guarda contra
        // o predador comer os proprios.
        simulation::AgentStore sameLabel;
        simulation::AgentSpawn a1 = pred; a1.position = {200.0, 200.0};
        simulation::AgentSpawn a2 = pred; a2.position = {208.0, 200.0};
        const auto sid1 = sameLabel.createAgent(a1);
        static_cast<void>(sameLabel.createAgent(a2));
        const auto sameStats = sys.applyWithDiet(sameLabel, foods, genomes, nullptr, dcfg);
        check(sameStats.predationEvents == 0 && sameLabel.contains(sid1),
              "32.3: membros da mesma label nao se predam");
    }

    // ------------- I. Microfase 32.4: piso por bloqueio de morte + predacao sem atraso ----
    {
        // I0: SEM respawn do nada (runner dedicado). Reproducao desligada no template
        // (split_energy alto ANTES do initialize, para o genoma baker o valor), minimo
        // bem acima da populacao e rescue off (default 32.4): a populacao reduzida
        // permanece CONGELADA por varios passos — nada surge do nada para "completar" o
        // minimo (era exatamente o bug "quando morre um surge outro do nada").
        {
            config::ParameterRegistry regNR = config::createDefaultParameterRegistry();
            static_cast<void>(regNR.setValue("auto_export_substrate", false));
            static_cast<void>(regNR.setValue("bacteria_split_energy", 1.0e9));  // repro off no template
            sim::SimulationRunner rNR(regNR);
            rNR.initialize();
            const auto nrId = rNR.species().idByName("bacteria");
            static_cast<void>(rNR.speciesMutable().setMinPopulation(nrId, 30));
            static_cast<void>(rNR.speciesMutable().setMaxPopulation(nrId, 0));
            std::vector<simulation::EntityId> nrDoom;
            for (std::size_t i = 0; i < rNR.agents().size(); ++i)
                if (nrDoom.size() + 2 < rNR.countAgentsOfSpecies(nrId) &&
                    rNR.agents().speciesIdAt(i) == nrId)
                    nrDoom.push_back(rNR.agents().idAt(i));
            rNR.deleteAgents(nrDoom);
            const auto nrBefore = rNR.countAgentsOfSpecies(nrId);
            for (int i = 0; i < 5; ++i) rNR.step(1.0 / 30.0);
            check(nrBefore == 2 && rNR.countAgentsOfSpecies(nrId) == 2,
                  "32.4 I0: sem respawn do nada (2 permanece 2 por 5 passos; rescue off + repro off)");
        }

        // I1: DeathSystem bloqueia mortes no piso. 5 famintos, min=3 -> so 2 morrem
        // (ate o piso) e 3 sobrevivem; sem store o piso nao se aplica (5 morrem).
        simulation::SpeciesStore spD;
        simulation::SpeciesRecord recD;
        recD.name = "floor";
        recD.minPopulation = 3;
        recD.enabled = true;
        const auto floorSp = spD.registerSpecies(recD);
        auto makeStarving = [&](simulation::AgentStore& store) {
            for (int i = 0; i < 5; ++i)
            {
                simulation::AgentSpawn s;
                s.position = {static_cast<double>(i) * 5.0, 0.0};
                s.radius = 5.0;
                s.energy = 10.0;            // <= deathEnergy (50) => candidato a morte
                s.speciesId = floorSp;
                static_cast<void>(store.createAgent(s));
            }
        };
        systems::DeathConfig dcI;
        dcI.deathEnergy = 50.0;
        dcI.maxDeathsPerStep = 5;
        systems::DeathSystem deathSys;

        simulation::AgentStore agFloor;
        makeStarving(agFloor);
        const auto dsFloor = deathSys.apply(agFloor, dcI, &spD);
        check(agFloor.size() == 3 && dsFloor.deaths == 2 && dsFloor.blockedByMinPopulation > 0,
              "32.4 I1: morte bloqueada no piso (5 famintos, min=3 -> 2 morrem, 3 vivem)");

        simulation::AgentStore agNoStore;
        makeStarving(agNoStore);
        const auto dsNoStore = deathSys.apply(agNoStore, dcI, nullptr);
        check(dsNoStore.deaths == 5,
              "32.4 I1: sem store o piso nao se aplica (5 morrem ate max_deaths)");

        // Genomas + especies compartilhados pelos testes de predacao (I2/I3/I4).
        simulation::GenomeStore genI;
        simulation::GenomeRecord predG;
        predG.diet.eatFood = false;
        predG.diet.eatAgents = true;
        predG.diet.eatSameSpecies = false;
        predG.diet.agentEfficiency = 0.7;
        predG.energyCap = 400.0;
        const auto predGid = genI.createGenome(predG);
        simulation::GenomeRecord preyG;
        preyG.diet.eatFood = true;
        preyG.diet.eatAgents = false;
        const auto preyGid = genI.createGenome(preyG);
        simulation::FoodStore noFood;
        systems::InteractionSystem sysI;

        simulation::SpeciesStore spP;
        simulation::SpeciesRecord recPred; recPred.name = "pred"; recPred.minPopulation = 0; recPred.enabled = true;
        const auto predSp = spP.registerSpecies(recPred);
        simulation::SpeciesRecord recPrey; recPrey.name = "prey"; recPrey.minPopulation = 1; recPrey.enabled = true;
        const auto preySp = spP.registerSpecies(recPrey);

        auto makePredator = [&](simulation::AgentStore& s, const simulation::Vec2 at) {
            simulation::AgentSpawn a; a.position = at; a.radius = 10.0; a.energy = 100.0;
            a.speciesId = predSp; a.genomeId = predGid.id; return s.createAgent(a);
        };
        auto makePrey = [&](simulation::AgentStore& s, const simulation::Vec2 at) {
            simulation::AgentSpawn a; a.position = at; a.radius = 10.0; a.energy = 60.0;
            a.speciesId = preySp; a.genomeId = preyGid.id; return s.createAgent(a);
        };
        systems::DietInteractionConfig diI;
        diI.useSpatial = false;  // caminho O(n^2), sem hash

        // I2: presa NO piso (count=1 == min=1) e protegida; sem store seria comida.
        simulation::AgentStore agAtFloor;
        static_cast<void>(makePredator(agAtFloor, {100.0, 100.0}));
        const auto preyAtFloor = makePrey(agAtFloor, {108.0, 100.0});  // encostada (dist 8 < 20)
        const auto sFloorProt = sysI.applyWithDiet(agAtFloor, noFood, genI, nullptr, diI, &spP);
        check(sFloorProt.predationEvents == 0 && agAtFloor.contains(preyAtFloor),
              "32.4 I2: presa no piso minimo nao e predada");
        simulation::AgentStore agNoProt;
        static_cast<void>(makePredator(agNoProt, {100.0, 100.0}));
        const auto preyNoProt = makePrey(agNoProt, {108.0, 100.0});
        const auto sNoProt = sysI.applyWithDiet(agNoProt, noFood, genI, nullptr, diI, nullptr);
        check(sNoProt.predationEvents == 1 && !agNoProt.contains(preyNoProt),
              "32.4 I2: sem piso a mesma presa e comida (controle)");

        // I3: predacao PARA exatamente no piso. 2 predadores + 2 presas (min=1):
        // com store so 1 e comida (para em min=1); sem store as 2 morrem.
        auto buildCluster = [&](simulation::AgentStore& s) {
            static_cast<void>(makePredator(s, {100.0, 100.0}));
            static_cast<void>(makePredator(s, {100.0, 110.0}));
            static_cast<void>(makePrey(s, {110.0, 100.0}));
            static_cast<void>(makePrey(s, {110.0, 110.0}));
        };
        simulation::AgentStore agStop; buildCluster(agStop);
        const auto sStop = sysI.applyWithDiet(agStop, noFood, genI, nullptr, diI, &spP);
        check(sStop.predationEvents == 1,
              "32.4 I3: predacao para no piso (2 presas, min=1 -> 1 comida)");
        simulation::AgentStore agStopNo; buildCluster(agStopNo);
        const auto sStopNo = sysI.applyWithDiet(agStopNo, noFood, genI, nullptr, diI, nullptr);
        check(sStopNo.predationEvents == 2,
              "32.4 I3: sem piso ambas as presas sao comidas (controle)");

        // I4: regressao do atraso. Predador e presa encostados (dist 12 < 20). Ordem
        // NOVA (Interaction antes de Collision) mata no mesmo passo; ordem ANTIGA
        // (Collision primeiro, separation=0.9) separa os corpos (dist 26.4 > 20) e a
        // predacao erra o toque -> a presa sobrevive aquele passo.
        simulation::World worldI;
        simulation::WorldConfig wcI;
        wcI.width = 400.0; wcI.height = 400.0; wcI.radius = 200.0;
        wcI.center = {200.0, 200.0}; wcI.shape = simulation::WorldShape::Rectangular;
        worldI.configure(wcI);

        simulation::AgentStore agNew;
        static_cast<void>(makePredator(agNew, {100.0, 100.0}));
        const auto preyNew = makePrey(agNew, {112.0, 100.0});
        const auto sNew = sysI.applyWithDiet(agNew, noFood, genI, nullptr, diI, nullptr);
        check(sNew.predationEvents == 1 && !agNew.contains(preyNew),
              "32.4 I4: ordem nova (predacao antes da colisao) mata a presa no mesmo passo");

        simulation::AgentStore agOld;
        static_cast<void>(makePredator(agOld, {100.0, 100.0}));
        const auto preyOld = makePrey(agOld, {112.0, 100.0});
        systems::CollisionConfig ccI;
        ccI.agentCollisionEnabled = true;
        ccI.separation = 0.9;
        ccI.useSpatial = false;
        ccI.dt = 1.0 / 30.0;
        systems::CollisionSystem collI;
        static_cast<void>(collI.apply(agOld, noFood, worldI, nullptr, nullptr, ccI));  // separa
        const auto sOld = sysI.applyWithDiet(agOld, noFood, genI, nullptr, diI, nullptr);
        check(sOld.predationEvents == 0 && agOld.contains(preyOld),
              "32.4 I4: ordem antiga (colisao primeiro) separa e a presa sobrevive ao passo");
    }

    // ------------- J. Microfase 32.5: visao POR LABEL filtra a percepcao --------
    // A geometria da retina e global (define o tamanho da entrada da rede), mas O
    // QUE cada agente enxerga vem do genoma. Mesma cena fisica, genomas diferentes
    // => percepcoes diferentes. "Ver tudo" e o caso simples: nao filtra tipo.
    {
        simulation::World wj;
        simulation::WorldConfig wcj;
        wcj.width = 400.0; wcj.height = 400.0; wcj.radius = 200.0;
        wcj.center = {200.0, 200.0}; wcj.shape = simulation::WorldShape::Rectangular;
        wj.configure(wcj);

        // Comida diretamente a frente do agente (olha para +x, angle=0).
        simulation::FoodStore foodsj;
        simulation::FoodSpawn fj; fj.position = {250.0, 200.0}; fj.radius = 6.0; fj.energy = 50.0;
        static_cast<void>(foodsj.createFood(fj));

        simulation::GenomeStore genj;
        simulation::GenomeRecord seer;    seer.vision.seeFood = true;  seer.vision.seeAll = false;
        const auto seerId = genj.createGenome(seer);
        simulation::GenomeRecord blind;   blind.vision.seeFood = false; blind.vision.seeAll = false;
        const auto blindId = genj.createGenome(blind);
        simulation::GenomeRecord allSeer; allSeer.vision.seeFood = false; allSeer.vision.seeAll = true;
        const auto allId = genj.createGenome(allSeer);

        perception::PerceptionConfig pcj;
        pcj.retina.visionMode = "single";
        pcj.retina.visionRadius = 120.0;
        pcj.retina.retinaCount = 8;
        pcj.retina.fovDegrees = 200.0;
        pcj.retina.inputMode = perception::RetinaInputMode::DistanceOnly;
        pcj.parallelEnabled = false;
        perception::PerceptionSystem psj;

        auto perceiveMax = [&](const simulation::GenomeId gid) {
            simulation::AgentStore ag;
            simulation::AgentSpawn a; a.position = {200.0, 200.0}; a.angle = 0.0;
            a.radius = 9.0; a.energy = 100.0; a.genomeId = gid;
            static_cast<void>(ag.createAgent(a));
            const auto r = psj.computeInputs(ag, foodsj, nullptr, wj, pcj, {}, nullptr, &genj);
            double m = 0.0;
            for (const double x : r.flatInputs) m = std::max(m, x);
            return m;
        };
        check(perceiveMax(seerId.id) > 1.0e-9,
              "32.5 J: genoma que ve comida percebe a comida (input != 0)");
        check(perceiveMax(blindId.id) <= 1.0e-9,
              "32.5 J: genoma que NAO ve comida nao percebe nada (input 0)");
        check(perceiveMax(allId.id) > 1.0e-9,
              "32.5 J: genoma 'ver tudo' percebe a comida sem filtro de tipo");

        // Bootstrap copia a visao do registry para o genoma da label (per-label).
        config::ParameterRegistry regj = config::createDefaultParameterRegistry();
        static_cast<void>(regj.setValue("auto_export_substrate", false));
        sim::SimulationRunner rj(regj);
        rj.initialize();
        const auto bId = rj.species().idByName("bacteria");
        const auto* brec = rj.species().find(bId);
        const auto* bgen = brec != nullptr ? rj.genomes().find(brec->defaultGenomeId) : nullptr;
        check(bgen != nullptr && bgen->vision.seeFood && !bgen->vision.seeAll,
              "32.5 J: genoma da label padrao carrega a visao do registry (seeFood=true)");
    }

    // ------------- K. Fase 32.1: visualizador da visao do agente selecionado ----
    // O visualizador desenha a partir do VisionDebugData (read-only) preenchido pela
    // percepcao SO para o agente-alvo. Aqui validamos o motor desse dado: alvo unico
    // preenche; sem alvo (0 ou >1 selecionados) NAO preenche (custo zero); o modo
    // (single/raycast vs sector/bin) chega no debug para o overlay escolher o desenho.
    {
        config::ParameterRegistry regK = config::createDefaultParameterRegistry();
        static_cast<void>(regK.setValue("auto_export_substrate", false));
        static_cast<void>(regK.setValue("bacteria_count", 40));
        static_cast<void>(regK.setValue("food_target", 60));
        static_cast<void>(regK.setValue("retina_vision_mode", std::string("single")));
        sim::SimulationRunner rk(regK);
        rk.initialize();

        const auto targetId = rk.agents().idAt(0);
        rk.setVisionDebugTarget(targetId);
        rk.step(1.0 / 30.0);
        const auto& vd = rk.visionDebug();
        check(vd.active && vd.agentId == targetId.value && !vd.rays.empty(),
              "32.1 K: alvo unico preenche o VisionDebugData (raios do selecionado)");
        check(vd.mode == perception::VisionMode::Single,
              "32.1 K: modo single (raycast) refletido no debug");

        rk.clearVisionDebugTarget();
        rk.step(1.0 / 30.0);
        check(!rk.visionDebug().active,
              "32.1 K: sem alvo (0 ou >1 selecionados) o debug fica inativo (custo zero)");

        config::ParameterRegistry regKs = config::createDefaultParameterRegistry();
        static_cast<void>(regKs.setValue("auto_export_substrate", false));
        static_cast<void>(regKs.setValue("bacteria_count", 40));
        static_cast<void>(regKs.setValue("food_target", 60));
        static_cast<void>(regKs.setValue("retina_vision_mode", std::string("sector")));
        sim::SimulationRunner rks(regKs);
        rks.initialize();
        rks.setVisionDebugTarget(rks.agents().idAt(0));
        rks.step(1.0 / 30.0);
        check(rks.visionDebug().active &&
                  rks.visionDebug().mode == perception::VisionMode::Sector &&
                  !rks.visionDebug().rays.empty(),
              "32.1 K: modo setor (bin) preenche o debug -> overlay desenha cunhas");
    }

    // ------------- L. Reproducao do bug relatado: piso de morte ponta-a-ponta -----
    // Replica o fluxo do usuario (label CRIADA) sob fome total, varios passos, e
    // confere que a populacao NUNCA cai abaixo do min da label. Se cair, o piso esta
    // furado para labels criadas (que o I0/I1 nao cobriam — usavam a bacteria default).
    {
        // L1: bacteria default sob fome total -> estabiliza no min (5), nunca abaixo.
        config::ParameterRegistry regL = config::createDefaultParameterRegistry();
        static_cast<void>(regL.setValue("auto_export_substrate", false));
        static_cast<void>(regL.setValue("bacteria_count", 30));
        static_cast<void>(regL.setValue("food_target", 0));            // fome total
        static_cast<void>(regL.setValue("bacteria_split_energy", 1.0e12)); // sem reproducao
        sim::SimulationRunner rl(regL);
        rl.initialize();
        const auto bId = rl.species().idByName("bacteria");
        std::size_t minSeenDefault = static_cast<std::size_t>(-1);
        for (int s = 0; s < 900; ++s)
        {
            rl.step(1.0 / 30.0);
            minSeenDefault = std::min(minSeenDefault, rl.countAgentsOfSpecies(bId));
        }
        check(minSeenDefault >= 5,
              "L1: bacteria default sob fome nunca cai abaixo do min=5 (min visto=" +
                  std::to_string(minSeenDefault) + ")");

        // L2: label CRIADA pelo usuario sob fome total -> idem (min=5).
        config::ParameterRegistry regL2 = config::createDefaultParameterRegistry();
        static_cast<void>(regL2.setValue("auto_export_substrate", false));
        static_cast<void>(regL2.setValue("bacteria_count", 30));
        static_cast<void>(regL2.setValue("food_target", 0));
        static_cast<void>(regL2.setValue("bacteria_split_energy", 1.0e12));
        sim::SimulationRunner rl2(regL2);
        rl2.initialize();
        std::vector<simulation::EntityId> sel;
        for (std::size_t i = 0; i < 12 && i < rl2.agents().size(); ++i)
        {
            sel.push_back(rl2.agents().idAt(i));
        }
        const auto created = rl2.createSpeciesFromSelected("presa_l2", sel);
        const auto* crec = rl2.species().find(created);
        const int createdFloor = crec != nullptr ? crec->minPopulation : -1;
        std::size_t minSeenCreated = static_cast<std::size_t>(-1);
        for (int s = 0; s < 900; ++s)
        {
            rl2.step(1.0 / 30.0);
            minSeenCreated = std::min(minSeenCreated, rl2.countAgentsOfSpecies(created));
        }
        check(createdFloor == 5 && minSeenCreated >= 5,
              "L2: label CRIADA sob fome nunca cai abaixo do min=5 (floor=" +
                  std::to_string(createdFloor) + ", min visto=" +
                  std::to_string(minSeenCreated) + ")");

        // L3: PREDACAO real (predador legado cacando bacteria, sem comida) nao pode
        // derrubar a presa abaixo do min dela.
        config::ParameterRegistry regL3 = config::createDefaultParameterRegistry();
        static_cast<void>(regL3.setValue("auto_export_substrate", false));
        static_cast<void>(regL3.setValue("bacteria_count", 20));
        static_cast<void>(regL3.setValue("predators_enabled", true));
        static_cast<void>(regL3.setValue("predator_count", 40));
        static_cast<void>(regL3.setValue("food_target", 0));               // forca a caca
        static_cast<void>(regL3.setValue("bacteria_split_energy", 1.0e12)); // presa nao repoe
        sim::SimulationRunner rl3(regL3);
        rl3.initialize();
        const auto preyId = rl3.species().idByName("bacteria");
        const auto predId = rl3.species().idByName("predator");
        const std::size_t preds0 = rl3.countAgentsOfSpecies(predId);
        std::size_t minPrey = static_cast<std::size_t>(-1);
        for (int s = 0; s < 900; ++s)
        {
            rl3.step(1.0 / 30.0);
            minPrey = std::min(minPrey, rl3.countAgentsOfSpecies(preyId));
        }
        check(preds0 > 0 && minPrey >= 5,
              "L3: predacao nao derruba a presa abaixo do min=5 (predadores=" +
                  std::to_string(preds0) + ", presa min vista=" + std::to_string(minPrey) + ")");
    }

    // ------------- M. Determinismo independente do time_scale --------------------
    // O passo do engine usa dt FIXO; a velocidade e PLAYBACK (o FixedTimestep do App
    // roda MAIS passos por segundo real), nao escala o dt. Logo, dois runners com
    // time_scale 1x e 10x, MESMA seed, rodando o MESMO numero de passos com o MESMO
    // dt fixo, produzem estado IDENTICO. (Antes da correcao o runner multiplicava o
    // dt por time_scale -> 10x dava dt gigante por passo e a populacao desabava.)
    {
        auto digest = [](const sim::SimulationRunner& r) {
            double acc = static_cast<double>(r.agents().size()) * 1000.0;
            for (std::size_t i = 0; i < r.agents().size(); ++i)
            {
                const auto p = r.agents().positionAt(i);
                acc += p.x * 1.1 + p.y * 1.7 + r.agents().energyAt(i) * 0.3;
            }
            return acc;
        };
        config::ParameterRegistry regA = config::createDefaultParameterRegistry();
        static_cast<void>(regA.setValue("auto_export_substrate", false));
        static_cast<void>(regA.setValue("random_seed", 777));
        static_cast<void>(regA.setValue("time_scale", 1.0));
        config::ParameterRegistry regB = config::createDefaultParameterRegistry();
        static_cast<void>(regB.setValue("auto_export_substrate", false));
        static_cast<void>(regB.setValue("random_seed", 777));
        static_cast<void>(regB.setValue("time_scale", 10.0));
        sim::SimulationRunner runA(regA);
        sim::SimulationRunner runB(regB);
        runA.initialize();
        runB.initialize();
        for (int s = 0; s < 300; ++s)
        {
            runA.step(1.0 / 30.0);
            runB.step(1.0 / 30.0);
        }
        check(runA.agents().size() == runB.agents().size() &&
                  std::abs(digest(runA) - digest(runB)) < 1e-9,
              "32.x M: time_scale nao afeta o resultado do passo (1x vs 10x identicos com dt fixo)");
    }

    // ------------- N. Food fix: a reposicao atinge o alvo (antes travava em ~5/passo) --
    // O cap por trim_max_per_step prendia a comida muito abaixo de alvos grandes
    // (~700 vs 20000), o que tambem limitava a populacao. Agora a reposicao enche
    // ate o food_target a cada passo.
    {
        config::ParameterRegistry regN = config::createDefaultParameterRegistry();
        static_cast<void>(regN.setValue("auto_export_substrate", false));
        static_cast<void>(regN.setValue("bacteria_count", 50));
        static_cast<void>(regN.setValue("food_target", 1500));
        static_cast<void>(regN.setValue("food_mode", std::string("instant")));
        sim::SimulationRunner rn(regN);
        rn.initialize();
        for (int i = 0; i < 5; ++i) rn.step(1.0 / 30.0);
        check(static_cast<int>(rn.foods().size()) >= 1490,
              "N: reposicao de comida atinge o alvo alto (1500) em poucos passos (" +
                  std::to_string(rn.foods().size()) + ")");
    }

    // ------------- O. Comida em pedacos ITINERANTE (food_chunk_mode=roaming) ----------
    // Modo novo: um chunk pode ser comido ate o fim e desaparecer; um chunk NOVO surge
    // em OUTRO lugar (id diferente), em vez de repor no mesmo site (modo 'fixo'). Teste
    // unitario do FoodSystem (sem organismos -> sem ruido). Contrasta roaming x fixed.
    {
        const simulation::World world{simulation::WorldConfig{
            simulation::WorldShape::Rectangular, 800.0, 600.0, 400.0, {400.0, 300.0}}};
        systems::FoodSystemConfig cfg;
        cfg.mode = simulation::FoodKind::Chunk;
        cfg.target = 40;
        cfg.particleRadius = 5.0;
        cfg.clusterRadius = 20.0;
        cfg.trimMaxPerStep = 100;

        const auto clusterIdSet = [](const simulation::FoodStore& f) {
            std::unordered_set<std::uint32_t> ids;
            for (std::size_t i = 0; i < f.size(); ++i) ids.insert(f.clusterIdAt(i));
            return ids;
        };
        const auto removeCluster = [](simulation::FoodStore& f, std::uint32_t victim) {
            std::vector<simulation::EntityId> doomed;
            for (std::size_t i = 0; i < f.size(); ++i)
                if (f.clusterIdAt(i) == victim) doomed.push_back(f.idAt(i));
            for (const auto id : doomed) static_cast<void>(f.removeFood(id));
        };

        // Roaming: a fully-eaten chunk does NOT come back with the same id; a new one
        // (new id) appears elsewhere, and the chunk count is restored.
        systems::FoodSystem fsR;
        cfg.chunkRoaming = true;
        simulation::FoodStore foodsR;
        static_cast<void>(fsR.replenishToTarget(foodsR, world, cfg));
        const auto idsR0 = clusterIdSet(foodsR);
        check(idsR0.size() >= 2, "O: roaming criou multiplos chunks (" +
                                     std::to_string(idsR0.size()) + ")");
        const std::uint32_t victimR = *idsR0.begin();
        removeCluster(foodsR, victimR);
        static_cast<void>(fsR.replenishToTarget(foodsR, world, cfg));
        const auto idsR1 = clusterIdSet(foodsR);
        check(idsR1.count(victimR) == 0,
              "O: chunk esgotado NAO reaparece com o mesmo id (itinerante)");
        check(idsR1.size() >= idsR0.size(),
              "O: contagem de chunks restaurada por um chunk NOVO em outro lugar");

        // Fixed (legacy): refills the SAME sites in place -> the eaten site id returns.
        systems::FoodSystem fsF;
        cfg.chunkRoaming = false;
        simulation::FoodStore foodsF;
        static_cast<void>(fsF.replenishToTarget(foodsF, world, cfg));
        const auto idsF0 = clusterIdSet(foodsF);
        const std::uint32_t victimF = *idsF0.begin();
        removeCluster(foodsF, victimF);
        static_cast<void>(fsF.replenishToTarget(foodsF, world, cfg));
        check(clusterIdSet(foodsF).count(victimF) == 1,
              "O: modo fixo repoe o MESMO site no lugar (contraste com itinerante)");
    }

    summary.details = log.str();
    return summary;
}
} // namespace agentbiosim::systems
