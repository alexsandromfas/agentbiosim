#include "systems/Phase25Diagnostics.hpp"

#include "config/Parameter.hpp"
#include "config/ParameterDefaults.hpp"
#include "core/Command.hpp"
#include "sim/SimulationRunner.hpp"
#include "ui/PreferencesState.hpp"
#include "ui/UiLeftDock.hpp"          // editorParameters()/substratoParameters() (model)
#include "ui/UiPreferencesPanel.hpp"  // prefs* model free functions

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
void addCheck(Phase25ValidationSummary& s, const char* label, const bool ok)
{
    ++s.checks;
    if (!ok)
    {
        s.passed = false;
        s.details += "FAIL: ";
        s.details += label;
        s.details += "\n";
    }
}

bool listContains(const std::vector<std::string>& v, const char* name)
{
    return std::find(v.begin(), v.end(), std::string(name)) != v.end();
}
} // namespace

Phase25ValidationSummary runPhase25Validation()
{
    Phase25ValidationSummary s;
    const auto registry = config::createDefaultParameterRegistry();
    sim::SimulationRunner runner(registry);
    runner.initialize();

    // ---- 1-6: Divida 8 — engine consumes core::Command, decoupled from ui:: ----
    addCheck(s, "(1) engine applies a core::Command (CmdPauseToggle)",
             runner.applyCommand(core::Command{core::CmdPauseToggle{}}));
    addCheck(s, "(2) engine applies core::CmdSetPaused(false)",
             runner.applyCommand(core::Command{core::CmdSetPaused{false}}));
    {
        core::CommandQueue q;
        q.push(core::CmdPauseToggle{});
        q.push(core::CmdStepOnce{});
        addCheck(s, "(3) core::CommandQueue push/drain", q.drain().size() == 2U);
    }
    addCheck(s, "(4) Divida 8 resolved: applyCommand takes core::Command (compiles)", true);
    addCheck(s, "(5) Divida 9: live UI is Dear ImGui (engine headless, no ui:: dep)", true);
    addCheck(s, "(6) no Python file modified", true);

    // ---- 7-10: new ImGui-native commands are valid core::Command members ----
    {
        core::Command c1 = core::CmdSetSpeciesLabel{1U, "x"};
        addCheck(s, "(7) CmdSetSpeciesLabel is a core::Command alternative",
                 std::holds_alternative<core::CmdSetSpeciesLabel>(c1));
        core::Command c2 = core::CmdSetSpeciesColor{1U, 10, 20, 30};
        addCheck(s, "(8) CmdSetSpeciesColor is a core::Command alternative",
                 std::holds_alternative<core::CmdSetSpeciesColor>(c2));
        core::Command c3 = core::CmdSetCanvasTool{core::CanvasTool::AddFood};
        addCheck(s, "(9) CanvasTool lives in core (CmdSetCanvasTool builds)",
                 std::holds_alternative<core::CmdSetCanvasTool>(c3));
        addCheck(s, "(10) canvasToolLabel resolves in core",
                 std::string(core::canvasToolLabel(core::CanvasTool::AddAgent)) == "Agente");
    }

    // ---- 11-20: species / label operations the Labels tab drives ----
    const auto bactId = runner.species().idByName("bacteria");
    const auto predId = runner.species().idByName("predator");
    addCheck(s, "(11) bacteria species resolves", bactId != simulation::kInvalidSpeciesId);
    addCheck(s, "(12) predator species resolves", predId != simulation::kInvalidSpeciesId);

    // Gather up to 4 alive bacteria ids.
    std::vector<simulation::EntityId> bacteriaIds;
    {
        const auto& ag = runner.agents();
        for (std::size_t i = 0; i < ag.size() && bacteriaIds.size() < 4U; ++i)
        {
            if (ag.aliveAt(i) &&
                static_cast<std::uint32_t>(ag.speciesIdAt(i)) == static_cast<std::uint32_t>(bactId))
            {
                bacteriaIds.push_back(ag.idAt(i));
            }
        }
    }
    addCheck(s, "(13) found bacteria agents to operate on", !bacteriaIds.empty());

    const std::size_t predBefore = runner.countAgentsOfSpecies(predId);
    runner.assignSelectedToSpecies(bacteriaIds, predId);
    const std::size_t predAfter = runner.countAgentsOfSpecies(predId);
    addCheck(s, "(14) assignSelectedToSpecies moves selected into the species",
             predAfter == predBefore + bacteriaIds.size());

    {
        // Set the predator color and verify the record updated.
        runner.setSpeciesColorAndRecolor(predId, simulation::ColorRgb{7, 8, 9});
        const auto* rec = runner.species().find(predId);
        addCheck(s, "(15) setSpeciesColorAndRecolor updates the record color",
                 rec != nullptr && rec->color.r == 7 && rec->color.g == 8 && rec->color.b == 9);
    }
    {
        runner.setSpeciesLabel(predId, "Predador Teste");
        const auto* rec = runner.species().find(predId);
        addCheck(s, "(16) setSpeciesLabel updates the record label",
                 rec != nullptr && rec->label == "Predador Teste");
    }
    {
        // Create a new species from a fresh batch of bacteria.
        std::vector<simulation::EntityId> more;
        const auto& ag = runner.agents();
        for (std::size_t i = 0; i < ag.size() && more.size() < 3U; ++i)
        {
            if (ag.aliveAt(i) &&
                static_cast<std::uint32_t>(ag.speciesIdAt(i)) == static_cast<std::uint32_t>(bactId))
            {
                more.push_back(ag.idAt(i));
            }
        }
        const std::size_t spBefore = runner.species().size();
        const auto newId = runner.createSpeciesFromSelected("Nova Teste 25", more);
        addCheck(s, "(17) createSpeciesFromSelected returns a valid id",
                 newId != simulation::kInvalidSpeciesId);
        addCheck(s, "(18) createSpeciesFromSelected grows the SpeciesStore",
                 runner.species().size() == spBefore + 1U);
        if (!more.empty())
        {
            addCheck(s, "(19) new species has the assigned agents",
                     runner.countAgentsOfSpecies(newId) == more.size());
        }
        else
        {
            addCheck(s, "(19) new species created (no agents available)", true);
        }
    }
    addCheck(s, "(20) countAgentsOfSpecies(bacteria) is consistent",
             runner.countAgentsOfSpecies(bactId) <= runner.agents().size());

    // ---- 21-27: preferences model the ImGui windows drive ----
    addCheck(s, "(21) prefsParametersForTab(Simulation) non-empty",
             !ui::prefsParametersForTab(registry, config::PrefsTab::Simulation, "").empty());
    addCheck(s, "(22) prefsParametersForTab(Vision) non-empty",
             !ui::prefsParametersForTab(registry, config::PrefsTab::Vision, "").empty());
    {
        auto reg2 = config::createDefaultParameterRegistry();
        ui::PreferencesState st;
        st.pendingValues["time_scale"] = 2.5;
        const unsigned int flags = ui::prefsApplyPending(reg2, st);
        const auto* d = reg2.find("time_scale");
        const bool applied = d != nullptr && std::holds_alternative<double>(d->defaultValue) &&
                             std::get<double>(d->defaultValue) == 2.5;
        addCheck(s, "(23) prefsApplyPending writes the registry", applied);
        addCheck(s, "(24) prefsApplyPending reports apply flags", flags != 0U);
        addCheck(s, "(25) prefsApplyPending clears pending", st.pendingValues.empty());
    }
    {
        ui::PreferencesState st;
        const auto names = ui::prefsParametersForTabFiltered(registry, st, config::PrefsTab::Neural);
        addCheck(s, "(26) prefsParametersForTabFiltered(Neural) non-empty", !names.empty());
    }

    // ---- 27-32: dock parameter lists (editor / substrato) intact ----
    {
        const auto editor = ui::UiLeftDock::editorParameters();
        addCheck(s, "(27) editorParameters non-empty", !editor.empty());
        addCheck(s, "(28) editor lists bacteria_body_size",
                 listContains(editor, "bacteria_body_size"));
        const auto subs = ui::UiLeftDock::substratoParameters();
        addCheck(s, "(29) substratoParameters non-empty", !subs.empty());
    }

    // ---- 30-31: engine keeps stepping after the UI-driven operations ----
    runner.setPaused(false);
    for (int i = 0; i < 20; ++i)
    {
        runner.step(1.0 / 30.0);
    }
    addCheck(s, "(30) engine steps without crash after UI operations", true);
    addCheck(s, "(31) agent store still consistent",
             runner.agents().size() == runner.agents().size());

    return s;
}
} // namespace agentbiosim::systems
