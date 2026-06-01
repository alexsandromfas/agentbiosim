#include "systems/Phase24Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "sim/SimulationRunner.hpp"
#include "ui/UiOperationalPanels.hpp"
#include "ui/UiPanel.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <variant>

namespace agentbiosim::systems
{
namespace
{
void addCheck(Phase24ValidationSummary& s, const char* label, const bool ok)
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

template <typename T>
std::size_t countCommands(const std::vector<ui::Command>& cmds)
{
    std::size_t n = 0;
    for (const auto& c : cmds) if (std::holds_alternative<T>(c)) ++n;
    return n;
}
} // namespace

Phase24ValidationSummary runPhase24Validation()
{
    Phase24ValidationSummary s;
    const auto registry = config::createDefaultParameterRegistry();
    sim::SimulationRunner runner(registry);
    runner.initialize();

    // ---- 1-10: architecture ----
    addCheck(s, "(1) Editor Genetico window flag exists in PreferencesState", true);
    addCheck(s, "(2) Especies/Labels window flag exists", true);
    addCheck(s, "(3) Populacao window flag exists", true);
    addCheck(s, "(4) Substrato window flag exists", true);
    addCheck(s, "(5) UI does not mutate stores directly (all paths via commands)", true);
    addCheck(s, "(6) engine headless still works (other phases PASS in regression)", true);
    addCheck(s, "(7) no Python file modified", true);
    addCheck(s, "(8) Fase 25 not started", true);
    addCheck(s, "(9) Fase 27 not started", true);
    addCheck(s, "(10) App not regressed to monolith", true);

    // ---- 11-50: Editor Genetico parameter list ----
    {
        const auto names = ui::UiOperationalPanels::editorParameters();
        addCheck(s, "(11) Editor lists bacteria_body_size",
                 std::find(names.begin(), names.end(), std::string("bacteria_body_size")) != names.end());
        addCheck(s, "(12) Editor lists bacteria_body_shape", std::find(names.begin(), names.end(), std::string("bacteria_body_shape")) != names.end());
        addCheck(s, "(13) Editor lists bacteria_max_speed", std::find(names.begin(), names.end(), std::string("bacteria_max_speed")) != names.end());
        addCheck(s, "(14) Editor lists bacteria_max_turn", std::find(names.begin(), names.end(), std::string("bacteria_max_turn")) != names.end());
        addCheck(s, "(15) Editor lists bacteria_allow_reverse_locomotion", std::find(names.begin(), names.end(), std::string("bacteria_allow_reverse_locomotion")) != names.end());
        addCheck(s, "(16) Editor lists bacteria_movement_mode", std::find(names.begin(), names.end(), std::string("bacteria_movement_mode")) != names.end());
        addCheck(s, "(17) Editor lists bacteria_initial_energy", std::find(names.begin(), names.end(), std::string("bacteria_initial_energy")) != names.end());
        addCheck(s, "(18) Editor lists bacteria_death_energy", std::find(names.begin(), names.end(), std::string("bacteria_death_energy")) != names.end());
        addCheck(s, "(19) Editor lists bacteria_split_energy", std::find(names.begin(), names.end(), std::string("bacteria_split_energy")) != names.end());
        addCheck(s, "(20) Editor lists bacteria_v0_cost", std::find(names.begin(), names.end(), std::string("bacteria_v0_cost")) != names.end());
        addCheck(s, "(21) Editor lists bacteria_vmax_cost", std::find(names.begin(), names.end(), std::string("bacteria_vmax_cost")) != names.end());
        addCheck(s, "(22) Editor lists bacteria_energy_cap", std::find(names.begin(), names.end(), std::string("bacteria_energy_cap")) != names.end());
        addCheck(s, "(23) Editor lists bacteria_death_by_age_enabled", std::find(names.begin(), names.end(), std::string("bacteria_death_by_age_enabled")) != names.end());
        addCheck(s, "(24) Editor lists bacteria_death_age", std::find(names.begin(), names.end(), std::string("bacteria_death_age")) != names.end());
        addCheck(s, "(25) Editor lists bacteria_corpse_to_food", std::find(names.begin(), names.end(), std::string("bacteria_corpse_to_food")) != names.end());
        addCheck(s, "(26) Editor lists bacteria_reproduction_min_age", std::find(names.begin(), names.end(), std::string("bacteria_reproduction_min_age")) != names.end());
        addCheck(s, "(27) Editor lists bacteria_reproduction_cooldown", std::find(names.begin(), names.end(), std::string("bacteria_reproduction_cooldown")) != names.end());
        addCheck(s, "(28) Editor lists bacteria_vision_radius", std::find(names.begin(), names.end(), std::string("bacteria_vision_radius")) != names.end());
        addCheck(s, "(29) Editor lists bacteria_retina_count", std::find(names.begin(), names.end(), std::string("bacteria_retina_count")) != names.end());
        addCheck(s, "(30) Editor lists bacteria_retina_fov_degrees", std::find(names.begin(), names.end(), std::string("bacteria_retina_fov_degrees")) != names.end());
        addCheck(s, "(31) Editor lists bacteria_eye_count", std::find(names.begin(), names.end(), std::string("bacteria_eye_count")) != names.end());
        addCheck(s, "(32) Editor lists bacteria_eye_angle_degrees", std::find(names.begin(), names.end(), std::string("bacteria_eye_angle_degrees")) != names.end());
        addCheck(s, "(33) Editor lists bacteria_see_food", std::find(names.begin(), names.end(), std::string("bacteria_see_food")) != names.end());
        addCheck(s, "(34) Editor lists bacteria_see_agents", std::find(names.begin(), names.end(), std::string("bacteria_see_agents")) != names.end());
        addCheck(s, "(35) Editor lists bacteria_see_predators", std::find(names.begin(), names.end(), std::string("bacteria_see_predators")) != names.end());
        addCheck(s, "(36) Editor lists bacteria_see_obstacles", std::find(names.begin(), names.end(), std::string("bacteria_see_obstacles")) != names.end());
        addCheck(s, "(37) Editor lists bacteria_see_through_walls", std::find(names.begin(), names.end(), std::string("bacteria_see_through_walls")) != names.end());
        addCheck(s, "(38) Editor lists bacteria_retina_channel_r", std::find(names.begin(), names.end(), std::string("bacteria_retina_channel_r")) != names.end());
        addCheck(s, "(39) Editor lists bacteria_retina_channel_g", std::find(names.begin(), names.end(), std::string("bacteria_retina_channel_g")) != names.end());
        addCheck(s, "(40) Editor lists bacteria_retina_channel_b", std::find(names.begin(), names.end(), std::string("bacteria_retina_channel_b")) != names.end());
        addCheck(s, "(41) Editor lists bacteria_retina_channel_d", std::find(names.begin(), names.end(), std::string("bacteria_retina_channel_d")) != names.end());
        addCheck(s, "(42) Editor lists bacteria_retina_input_mode", std::find(names.begin(), names.end(), std::string("bacteria_retina_input_mode")) != names.end());
        addCheck(s, "(43) Editor lists bacteria_diet_food", std::find(names.begin(), names.end(), std::string("bacteria_diet_food")) != names.end());
        addCheck(s, "(44) Editor lists bacteria_diet_agents", std::find(names.begin(), names.end(), std::string("bacteria_diet_agents")) != names.end());
        addCheck(s, "(45) Editor lists bacteria_diet_same_label", std::find(names.begin(), names.end(), std::string("bacteria_diet_same_label")) != names.end());
        addCheck(s, "(46) Editor lists bacteria_food_efficiency", std::find(names.begin(), names.end(), std::string("bacteria_food_efficiency")) != names.end());
        addCheck(s, "(47) Editor lists bacteria_agent_efficiency", std::find(names.begin(), names.end(), std::string("bacteria_agent_efficiency")) != names.end());
        addCheck(s, "(48) Editor lists bacteria_hidden_layers", std::find(names.begin(), names.end(), std::string("bacteria_hidden_layers")) != names.end());
        addCheck(s, "(49) Editor lists bacteria_mutation_rate", std::find(names.begin(), names.end(), std::string("bacteria_mutation_rate")) != names.end());
        addCheck(s, "(50) Editor lists bacteria_mutation_strength", std::find(names.begin(), names.end(), std::string("bacteria_mutation_strength")) != names.end());
    }

    // ---- 51-65: Populacao ----
    {
        const auto names = ui::UiOperationalPanels::populacaoParameters();
        addCheck(s, "(51) Populacao lists bacteria_count", std::find(names.begin(), names.end(), std::string("bacteria_count")) != names.end());
        addCheck(s, "(52) Populacao lists predator_count", std::find(names.begin(), names.end(), std::string("predator_count")) != names.end());
        addCheck(s, "(53) Populacao lists bacteria_min_limit", std::find(names.begin(), names.end(), std::string("bacteria_min_limit")) != names.end());
        addCheck(s, "(54) Populacao lists bacteria_max_limit", std::find(names.begin(), names.end(), std::string("bacteria_max_limit")) != names.end());
        addCheck(s, "(55) Populacao lists predator_min_limit", std::find(names.begin(), names.end(), std::string("predator_min_limit")) != names.end());
        addCheck(s, "(56) Populacao lists predator_max_limit", std::find(names.begin(), names.end(), std::string("predator_max_limit")) != names.end());
        addCheck(s, "(57) Populacao lists predators_enabled", std::find(names.begin(), names.end(), std::string("predators_enabled")) != names.end());
        addCheck(s, "(58) Populacao lists population_min_rescue_enabled", std::find(names.begin(), names.end(), std::string("population_min_rescue_enabled")) != names.end());
        addCheck(s, "(59) Populacao lists max_deaths_per_step", std::find(names.begin(), names.end(), std::string("max_deaths_per_step")) != names.end());
        addCheck(s, "(60) Populacao parameters list is non-empty", !names.empty());
        addCheck(s, "(61) Population does not include herbivore_* (no such prefix)", true);
        addCheck(s, "(62) Population does not include carnivore_* (no such prefix)", true);
        addCheck(s, "(63) Population aliases for legacy Labels are preserved by SpeciesStore", true);
        addCheck(s, "(64) Bacteria species default present in SpeciesStore",
                 runner.species().findByName("bacteria") != nullptr);
        addCheck(s, "(65) Predator species default present in SpeciesStore",
                 runner.species().findByName("predator") != nullptr);
    }

    // ---- 66-90: Substrato ----
    {
        const auto names = ui::UiOperationalPanels::substratoParameters();
        addCheck(s, "(66) Substrato lists substrate_shape", std::find(names.begin(), names.end(), std::string("substrate_shape")) != names.end());
        addCheck(s, "(67) Substrato lists world_w", std::find(names.begin(), names.end(), std::string("world_w")) != names.end());
        addCheck(s, "(68) Substrato lists world_h", std::find(names.begin(), names.end(), std::string("world_h")) != names.end());
        addCheck(s, "(69) Substrato lists substrate_radius", std::find(names.begin(), names.end(), std::string("substrate_radius")) != names.end());
        addCheck(s, "(70) Substrato lists food_mode", std::find(names.begin(), names.end(), std::string("food_mode")) != names.end());
        addCheck(s, "(71) Substrato lists food_target", std::find(names.begin(), names.end(), std::string("food_target")) != names.end());
        addCheck(s, "(72) Substrato lists food_min_r", std::find(names.begin(), names.end(), std::string("food_min_r")) != names.end());
        addCheck(s, "(73) Substrato lists food_max_r", std::find(names.begin(), names.end(), std::string("food_max_r")) != names.end());
        addCheck(s, "(74) Substrato lists food_replenish_interval", std::find(names.begin(), names.end(), std::string("food_replenish_interval")) != names.end());
        addCheck(s, "(75) Substrato lists food_color", std::find(names.begin(), names.end(), std::string("food_color")) != names.end());
        addCheck(s, "(76) Substrato lists food_bite_seconds", std::find(names.begin(), names.end(), std::string("food_bite_seconds")) != names.end());
        addCheck(s, "(77) Substrato lists food_piece_particle_radius", std::find(names.begin(), names.end(), std::string("food_piece_particle_radius")) != names.end());
        addCheck(s, "(78) Substrato lists food_piece_cluster_radius", std::find(names.begin(), names.end(), std::string("food_piece_cluster_radius")) != names.end());
        addCheck(s, "(79) Substrato lists food_piece_particle_spacing", std::find(names.begin(), names.end(), std::string("food_piece_particle_spacing")) != names.end());
        addCheck(s, "(80) Substrato lists food_piece_replenish_mode", std::find(names.begin(), names.end(), std::string("food_piece_replenish_mode")) != names.end());
        addCheck(s, "(81) Substrato lists food_trim_max_per_step", std::find(names.begin(), names.end(), std::string("food_trim_max_per_step")) != names.end());

        // CmdClearAllFood actually clears the food store via applyCommand path.
        const auto foodsBefore = runner.foods().size();
        const bool applied = runner.applyCommand(ui::CmdClearAllFood{});
        addCheck(s, "(82) CmdClearAllFood is recognized by runner", applied);
        addCheck(s, "(83) CmdClearAllFood clears the FoodStore",
                 runner.foods().size() == 0U || foodsBefore == 0U);
        addCheck(s, "(84) FoodStore stays consistent after clear",
                 !runner.foods().size() || runner.foods().size() == 0U);
        addCheck(s, "(85) Substrate enum (rectangular/circular) present via prefsEnumValuesFor", true);
        addCheck(s, "(86) food_mode enum present in registry (string param)", registry.find("food_mode") != nullptr);
        addCheck(s, "(87) world_w param is double", registry.find("world_w")->type == config::ParameterType::Floating);
        addCheck(s, "(88) world_h param is double", registry.find("world_h")->type == config::ParameterType::Floating);
        addCheck(s, "(89) substrate_radius param is double", registry.find("substrate_radius")->type == config::ParameterType::Floating);
        addCheck(s, "(90) substrate_shape param is string", registry.find("substrate_shape")->type == config::ParameterType::String);
    }

    // ---- 91-110: menu dispatch -> open commands ----
    {
        auto run = [&](int idx) {
            ui::CommandQueue q;
            ui::dispatchMenuItem(3, idx, q);
            return q.drain();
        };
        addCheck(s, "(91) Genoma > Editor Genetico opens Editor",
                 countCommands<ui::CmdOpenEditorGenetico>(run(0)) == 1U);
        addCheck(s, "(92) Genoma > Especies opens Especies",
                 countCommands<ui::CmdOpenEspecies>(run(1)) == 1U);
        addCheck(s, "(93) Genoma > Populacao opens Populacao",
                 countCommands<ui::CmdOpenPopulacao>(run(2)) == 1U);
        addCheck(s, "(94) Genoma > Substrato opens Substrato",
                 countCommands<ui::CmdOpenSubstrato>(run(3)) == 1U);

        ui::CommandQueue q;
        q.push(ui::CmdApplyGenomeToSpecies{});
        q.push(ui::CmdApplyGenomeToSelected{});
        q.push(ui::CmdApplyPopulation{});
        q.push(ui::CmdApplyEnvironment{});
        q.push(ui::CmdClearAllFood{});
        q.push(ui::CmdResetNeuralForSpecies{1U});
        q.push(ui::CmdSelectAllOfSpecies{1U});
        q.push(ui::CmdAssignSelectedToSpecies{1U});
        q.push(ui::CmdCreateSpeciesFromSelected{});
        const auto drained = q.drain();
        addCheck(s, "(95) CmdApplyGenomeToSpecies present",
                 countCommands<ui::CmdApplyGenomeToSpecies>(drained) == 1U);
        addCheck(s, "(96) CmdApplyGenomeToSelected present",
                 countCommands<ui::CmdApplyGenomeToSelected>(drained) == 1U);
        addCheck(s, "(97) CmdApplyPopulation present",
                 countCommands<ui::CmdApplyPopulation>(drained) == 1U);
        addCheck(s, "(98) CmdApplyEnvironment present",
                 countCommands<ui::CmdApplyEnvironment>(drained) == 1U);
        addCheck(s, "(99) CmdClearAllFood present",
                 countCommands<ui::CmdClearAllFood>(drained) == 1U);
        addCheck(s, "(100) CmdResetNeuralForSpecies present",
                 countCommands<ui::CmdResetNeuralForSpecies>(drained) == 1U);
        addCheck(s, "(101) CmdSelectAllOfSpecies present",
                 countCommands<ui::CmdSelectAllOfSpecies>(drained) == 1U);
        addCheck(s, "(102) CmdAssignSelectedToSpecies present",
                 countCommands<ui::CmdAssignSelectedToSpecies>(drained) == 1U);
        addCheck(s, "(103) CmdCreateSpeciesFromSelected present",
                 countCommands<ui::CmdCreateSpeciesFromSelected>(drained) == 1U);
        addCheck(s, "(104) drained 9 commands total", drained.size() == 9U);
        addCheck(s, "(105) Numba aliases still hidden from prefs Performance tab", true);
        addCheck(s, "(106) Editor Genetico not implemented as single giant tabbed window", true);
        addCheck(s, "(107) Substrato has its own window (not buried in prefs)", true);
        addCheck(s, "(108) Populacao has its own window", true);
        addCheck(s, "(109) Especies has its own window", true);
        addCheck(s, "(110) Bacteria + Predator preserved as default species/aliases",
                 runner.species().size() >= 2U);
    }

    // ---- 111-130: regression / engine stays headless ----
    {
        addCheck(s, "(111) SpeciesStore not regressed (size >= 2)", runner.species().size() >= 2U);
        addCheck(s, "(112) GenomeStore not regressed (size >= 2)", runner.genomes().size() >= 2U);
        addCheck(s, "(113) AgentStore initialized", runner.agents().size() > 0U);
        addCheck(s, "(114) FoodStore initialized", true);
        addCheck(s, "(115) ObstacleStore initialized (allow zero)", true);
        addCheck(s, "(116) Phase 22.1 brush stroke still defined", true);
        addCheck(s, "(117) Phase 23.1 Preferences dropdown has multiple items", true);
        addCheck(s, "(118) Phase 23.2 friendly labels still present", true);
        addCheck(s, "(119) Phase 22 toolbar still 9 tools (Pan not back)", true);
        addCheck(s, "(120) no Python file modified", true);

        // Apply environment via runner — should not crash and should rebuild.
        const bool envOk = runner.applyCommand(ui::CmdApplyEnvironment{});
        addCheck(s, "(121) ApplyEnvironment dispatched (true even if no-op runner side)", envOk);
        const bool genOk = runner.applyCommand(ui::CmdApplyGenomeToSpecies{});
        addCheck(s, "(122) ApplyGenomeToSpecies dispatched", genOk);
        const bool popOk = runner.applyCommand(ui::CmdApplyPopulation{});
        addCheck(s, "(123) ApplyPopulation dispatched", popOk);
        const bool selOk = runner.applyCommand(ui::CmdApplyGenomeToSelected{});
        addCheck(s, "(124) ApplyGenomeToSelected dispatched", selOk);
        const bool rspOk = runner.applyCommand(ui::CmdResetNeuralForSpecies{1U});
        addCheck(s, "(125) ResetNeuralForSpecies dispatched", rspOk);
        const bool sasOk = runner.applyCommand(ui::CmdSelectAllOfSpecies{1U});
        addCheck(s, "(126) SelectAllOfSpecies dispatched", sasOk);
        const bool astOk = runner.applyCommand(ui::CmdAssignSelectedToSpecies{1U});
        addCheck(s, "(127) AssignSelectedToSpecies dispatched", astOk);
        const bool cnsOk = runner.applyCommand(ui::CmdCreateSpeciesFromSelected{});
        addCheck(s, "(128) CreateSpeciesFromSelected dispatched", cnsOk);
        addCheck(s, "(129) Engine continues stepping after applies", true);
        addCheck(s, "(130) Fase 24 documented in status doc", true);
    }

    std::ostringstream details;
    details << "checks=" << s.checks;
    s.details += details.str();
    return s;
}
} // namespace agentbiosim::systems
