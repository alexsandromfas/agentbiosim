#include "systems/Phase34Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterRegistry.hpp"
#include "core/Command.hpp"
#include "io/SaveFile.hpp"
#include "sim/SimulationRunner.hpp"
#include "simulation/GenomeStore.hpp"
#include "simulation/SpeciesStore.hpp"

#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
constexpr double kDt = 1.0 / 30.0;

std::uint32_t u32(const simulation::SpeciesId id) noexcept
{
    return static_cast<std::uint32_t>(id);
}
} // namespace

Phase34ValidationSummary runPhase34Validation()
{
    Phase34ValidationSummary summary;
    std::ostringstream log;
    auto check = [&](const bool cond, const std::string& name) {
        ++summary.checks;
        if (cond) { log << "  ok: " << name << '\n'; }
        else { summary.passed = false; log << "  FAIL: " << name << '\n'; }
    };

    config::ParameterRegistry reg = config::createDefaultParameterRegistry();
    sim::SimulationRunner runner(reg);
    runner.initialize();
    runner.step(kDt);  // brains exist; ~150 bacteria alive

    const auto bacteriaId = runner.species().idByName("bacteria");
    check(bacteriaId != simulation::kInvalidSpeciesId, "bacteria existe");

    // Template genome of a species (its defaultGenomeId record).
    const auto templGenome = [&](const simulation::SpeciesId sid) -> const simulation::GenomeRecord* {
        const auto* sp = runner.species().find(sid);
        return sp != nullptr ? runner.genomes().find(sp->defaultGenomeId) : nullptr;
    };

    // --- A) Granular: editing ONE field changes only that field ------------
    {
        const auto* g0 = templGenome(bacteriaId);
        check(g0 != nullptr, "A: template do bacteria existe");
        const double bodyBefore = g0 != nullptr ? g0->bodySize : -1.0;
        const bool eatAgentsBefore = g0 != nullptr ? g0->diet.eatAgents : false;
        const double mutRateBefore = g0 != nullptr ? g0->mutationRate : -1.0;
        // Edit split_energy via the COMMAND (proves the dispatch wiring).
        runner.applyCommand(core::Command{core::CmdSetSpeciesGenomeField{
            u32(bacteriaId), "split_energy", config::ParameterValue{222.0}}});
        const auto* g1 = templGenome(bacteriaId);
        check(g1 != nullptr && std::abs(g1->splitEnergy - 222.0) < 1e-9,
              "A: split_energy aplicado ao template");
        check(g1 != nullptr && std::abs(g1->bodySize - bodyBefore) < 1e-9,
              "A: body_size permaneceu intacto");
        check(g1 != nullptr && g1->diet.eatAgents == eatAgentsBefore,
              "A: dieta permaneceu intacta");
        check(g1 != nullptr && std::abs(g1->mutationRate - mutRateBefore) < 1e-9,
              "A: taxa de mutacao permaneceu intacta");
    }

    // --- B) Applies to the template AND every living member ----------------
    {
        const auto applied = runner.setSpeciesGenomeField(
            bacteriaId, "split_energy", config::ParameterValue{333.0});
        std::size_t members = 0;
        std::size_t matched = 0;
        const auto& ag = runner.agents();
        for (std::size_t i = 0; i < ag.size(); ++i)
        {
            if (!ag.aliveAt(i) || ag.speciesIdAt(i) != bacteriaId) continue;
            ++members;
            const auto* g = runner.genomes().find(ag.genomeIdAt(i));
            if (g != nullptr && std::abs(g->splitEnergy - 333.0) < 1e-9) ++matched;
        }
        check(members > 0 && matched == members && applied == members,
              "B: aplicado ao template e a todos os " + std::to_string(members) + " membros");
    }

    // --- C) body_size refreshes each member's radius live ------------------
    {
        runner.setSpeciesGenomeField(bacteriaId, "body_size", config::ParameterValue{13.5});
        bool ok = false;
        const auto& ag = runner.agents();
        for (std::size_t i = 0; i < ag.size(); ++i)
        {
            if (!ag.aliveAt(i) || ag.speciesIdAt(i) != bacteriaId) continue;
            ok = std::abs(ag.radiusAt(i) - 13.5) < 1e-9;
            break;
        }
        check(ok, "C: body_size atualizou o raio dos membros");
    }

    // --- D) A diet field keeps the species' dietSnapshot in sync -----------
    {
        runner.setSpeciesGenomeField(bacteriaId, "diet_agents", config::ParameterValue{true});
        const auto* sp = runner.species().find(bacteriaId);
        check(sp != nullptr && sp->dietSnapshot.eatAgents, "D: dietSnapshot sincronizado");
    }

    // --- E) Editing one species does not touch another ---------------------
    {
        const auto sp2 = runner.createSpeciesDefault();
        const auto* before = templGenome(sp2);
        const double sp2Split = before != nullptr ? before->splitEnergy : -1.0;
        runner.setSpeciesGenomeField(bacteriaId, "split_energy", config::ParameterValue{444.0});
        const auto* after = templGenome(sp2);
        check(after != nullptr && std::abs(after->splitEnergy - sp2Split) < 1e-9,
              "E: a outra especie nao foi afetada");
        check(after != nullptr && std::abs(after->splitEnergy - 444.0) > 1e-9,
              "E: especies sao independentes");
    }

    // --- F) "+" with no selection: create species + spawn 5 ----------------
    {
        const auto sp3 = runner.createSpeciesDefault();
        check(runner.countAgentsOfSpecies(sp3) == 5, "F: '+' sem selecao spawna 5 organismos");
    }

    // --- G) "+" from selection: reassign, no spawn -------------------------
    {
        std::vector<simulation::EntityId> sel;
        const auto& ag = runner.agents();
        for (std::size_t i = 0; i < ag.size() && sel.size() < 3; ++i)
        {
            if (ag.aliveAt(i) && ag.speciesIdAt(i) == bacteriaId) sel.push_back(ag.idAt(i));
        }
        const std::size_t bactBefore = runner.countAgentsOfSpecies(bacteriaId);
        const auto sp4 = runner.createSpeciesFromSelected("Selecao", sel);
        check(runner.countAgentsOfSpecies(sp4) == sel.size(),
              "G: os selecionados viram a nova especie (sem spawn)");
        check(runner.countAgentsOfSpecies(bacteriaId) == bactBefore - sel.size(),
              "G: os selecionados saem da especie antiga");
    }

    // --- H) Determinism: same edits over the same seed => same state -------
    {
        const auto runOnce = [](double& outSplit, double& outBody, std::size_t& outCount) {
            config::ParameterRegistry r = config::createDefaultParameterRegistry();
            sim::SimulationRunner rr(r);
            rr.initialize();
            for (int i = 0; i < 5; ++i) rr.step(kDt);
            const auto bid = rr.species().idByName("bacteria");
            rr.setSpeciesGenomeField(bid, "split_energy", config::ParameterValue{255.0});
            rr.setSpeciesGenomeField(bid, "body_size", config::ParameterValue{11.0});
            const auto* sp = rr.species().find(bid);
            const auto* g = sp != nullptr ? rr.genomes().find(sp->defaultGenomeId) : nullptr;
            outSplit = g != nullptr ? g->splitEnergy : -1.0;
            outBody = g != nullptr ? g->bodySize : -1.0;
            outCount = rr.countAgentsOfSpecies(bid);
        };
        double s1 = 0.0;
        double s2 = 0.0;
        double b1 = 0.0;
        double b2 = 0.0;
        std::size_t c1 = 0;
        std::size_t c2 = 0;
        runOnce(s1, b1, c1);
        runOnce(s2, b2, c2);
        check(std::abs(s1 - s2) < 1e-12 && std::abs(b1 - b2) < 1e-12 && c1 == c2,
              "H: determinismo (2 execucoes identicas)");
    }

    // ===================== Fase 34.2: traços globais -> genoma =====================

    // --- I) MovementSystem reads maxSpeed PER GENOME -----------------------
    {
        const auto slow = runner.createSpeciesDefault();
        runner.setSpeciesGenomeField(slow, "max_speed", config::ParameterValue{0.0});
        for (int i = 0; i < 4; ++i) runner.step(kDt);
        const auto& ag = runner.agents();
        double slowMax = 0.0;
        double bacteriaMax = 0.0;
        for (std::size_t i = 0; i < ag.size(); ++i)
        {
            if (!ag.aliveAt(i)) continue;
            const auto v = ag.velocityAt(i);
            const double sp = std::hypot(v.x, v.y);
            if (ag.speciesIdAt(i) == slow) slowMax = std::max(slowMax, sp);
            else if (ag.speciesIdAt(i) == bacteriaId) bacteriaMax = std::max(bacteriaMax, sp);
        }
        check(slowMax < 1.0e-6, "I: max_speed=0 congela a especie (Movement le por-genoma)");
        check(bacteriaMax > 1.0, "I: a especie default (max_speed=300) se move");
    }

    // --- I) DeathSystem reads deathEnergy PER GENOME ------------------------
    {
        const auto doomed = runner.createSpeciesDefault();
        runner.adjustSpeciesPopulation(doomed, 0, -5);  // minPopulation 0 (no floor)
        // Threshold above any real energy -> every member starves; if Death read the
        // global 50 instead, these full-energy agents would NOT die.
        runner.setSpeciesGenomeField(doomed, "death_energy", config::ParameterValue{1.0e9});
        const std::size_t before = runner.countAgentsOfSpecies(doomed);
        for (int i = 0; i < 4; ++i) runner.step(kDt);
        check(before == 5 && runner.countAgentsOfSpecies(doomed) == 0,
              "I: death_energy alto extingue a especie (Death le por-genoma)");
    }

    // --- I) The new traits are writable through the granular bridge --------
    {
        runner.setSpeciesGenomeField(bacteriaId, "metab_v0_cost", config::ParameterValue{3.25});
        runner.setSpeciesGenomeField(bacteriaId, "max_turn", config::ParameterValue{1.5});
        const auto* sp = runner.species().find(bacteriaId);
        const auto* g = sp != nullptr ? runner.genomes().find(sp->defaultGenomeId) : nullptr;
        check(g != nullptr && std::abs(g->moveCostV0 - 3.25) < 1e-9 &&
                  std::abs(g->maxTurn - 1.5) < 1e-9,
              "I: novos traços escrevem no genoma via setSpeciesGenomeField");
    }

    // --- J) Save/load round-trip preserves the new genome fields -----------
    {
        config::ParameterRegistry regS = config::createDefaultParameterRegistry();
        sim::SimulationRunner rs(regS);
        rs.initialize();
        rs.step(kDt);
        const auto bid = rs.species().idByName("bacteria");
        rs.setSpeciesGenomeField(bid, "max_speed", config::ParameterValue{123.0});
        rs.setSpeciesGenomeField(bid, "death_energy", config::ParameterValue{77.0});
        rs.setSpeciesGenomeField(bid, "metab_vmax_cost", config::ParameterValue{12.5});

        io::SaveBundle bundle;
        bundle.snapshot = rs.snapshot();
        const std::string path = "phase34_tmp.agentbiosim";
        std::string err;
        check(io::saveToFile(path, bundle, err), "J: gravar .agentbiosim " + err);
        const io::LoadResult lr = io::loadFromFile(path);
        check(lr.ok, "J: ler .agentbiosim " + lr.error);

        config::ParameterRegistry regR = config::createDefaultParameterRegistry();
        sim::SimulationRunner rr(regR);
        rr.initialize();
        rr.restore(lr.bundle.snapshot);
        const auto bidR = rr.species().idByName("bacteria");
        const auto* spR = rr.species().find(bidR);
        const auto* gR = spR != nullptr ? rr.genomes().find(spR->defaultGenomeId) : nullptr;
        check(gR != nullptr && std::abs(gR->maxSpeed - 123.0) < 1e-9 &&
                  std::abs(gR->deathEnergy - 77.0) < 1e-9 &&
                  std::abs(gR->moveCostVmax - 12.5) < 1e-9,
              "J: save/load preserva maxSpeed/deathEnergy/metab_vmax_cost do genoma");
    }

    // --- K) Determinism with species that carry DIFFERENT traits -----------
    {
        const auto runOnce = [](double& sumX, std::size_t& count) {
            config::ParameterRegistry r = config::createDefaultParameterRegistry();
            sim::SimulationRunner rr(r);
            rr.initialize();
            rr.step(kDt);
            const auto bid = rr.species().idByName("bacteria");
            rr.setSpeciesGenomeField(bid, "max_speed", config::ParameterValue{55.0});
            const auto fast = rr.createSpeciesDefault();
            rr.setSpeciesGenomeField(fast, "max_speed", config::ParameterValue{480.0});
            rr.setSpeciesGenomeField(fast, "metab_v0_cost", config::ParameterValue{2.0});
            for (int i = 0; i < 8; ++i) rr.step(kDt);
            sumX = 0.0;
            count = 0;
            const auto& ag = rr.agents();
            for (std::size_t i = 0; i < ag.size(); ++i)
            {
                if (!ag.aliveAt(i)) continue;
                sumX += ag.positionAt(i).x;
                ++count;
            }
        };
        double x1 = 0.0;
        double x2 = 0.0;
        std::size_t c1 = 0;
        std::size_t c2 = 0;
        runOnce(x1, c1);
        runOnce(x2, c2);
        check(c1 == c2 && std::abs(x1 - x2) < 1e-9,
              "K: determinismo com especies de traços distintos (2 execucoes identicas)");
    }

    summary.details = log.str();
    return summary;
}
} // namespace agentbiosim::systems
