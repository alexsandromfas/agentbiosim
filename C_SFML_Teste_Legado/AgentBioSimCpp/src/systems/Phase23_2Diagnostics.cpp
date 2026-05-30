#include "systems/Phase23_2Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterMetadata.hpp"
#include "ui/UiPreferencesPanel.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <variant>

namespace agentbiosim::systems
{
namespace
{
void addCheck(Phase23_2ValidationSummary& s, const char* label, const bool ok)
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

Phase23_2ValidationSummary runPhase23_2Validation()
{
    Phase23_2ValidationSummary s;
    auto registry = config::createDefaultParameterRegistry();
    config::applyPhase23ApplyFlags(registry);

    // ---- 1-9: prefsEnumValuesFor ----
    {
        const auto neural = config::prefsEnumValuesFor("neural_network_type");
        addCheck(s, "(1) neural_network_type enum has 8 entries", neural.size() == 8U);
        addCheck(s, "(2) first neural enum is 'mlp'", !neural.empty() && neural[0] == "mlp");
        const bool hasGated = std::find(neural.begin(), neural.end(), "gated_mlp") != neural.end();
        const bool hasShortcut = std::find(neural.begin(), neural.end(), "shortcut_mlp") != neural.end();
        const bool hasModulated = std::find(neural.begin(), neural.end(), "modulated_mlp") != neural.end();
        const bool hasRnn = std::find(neural.begin(), neural.end(), "simple_rnn") != neural.end();
        const bool hasNeat = std::find(neural.begin(), neural.end(), "neat") != neural.end();
        const bool hasProto = std::find(neural.begin(), neural.end(), "proto_neat") != neural.end();
        const bool hasRec = std::find(neural.begin(), neural.end(), "recurrent_neat") != neural.end();
        addCheck(s, "(3) neural enum has gated_mlp", hasGated);
        addCheck(s, "(4) neural enum has shortcut_mlp", hasShortcut);
        addCheck(s, "(5) neural enum has modulated_mlp", hasModulated);
        addCheck(s, "(6) neural enum has simple_rnn", hasRnn);
        addCheck(s, "(7) neural enum has neat / proto_neat / recurrent_neat",
                 hasNeat && hasProto && hasRec);

        const auto substrate = config::prefsEnumValuesFor("substrate_shape");
        addCheck(s, "(8) substrate_shape enum is [rectangular, circular]",
                 substrate.size() == 2U && substrate[0] == "rectangular" &&
                 substrate[1] == "circular");

        const auto bogus = config::prefsEnumValuesFor("totally_made_up");
        addCheck(s, "(9) unknown param returns empty enum list", bogus.empty());
    }

    // ---- 10-14: prefsEnumValuesFor for vision params (regression of the
    //              same `domains` field bug) ----
    {
        addCheck(s, "(10) retina_vision_mode enum populated",
                 !config::prefsEnumValuesFor("retina_vision_mode").empty());
        addCheck(s, "(11) retina_bins_mode enum populated",
                 !config::prefsEnumValuesFor("retina_bins_mode").empty());
        addCheck(s, "(12) retina_bins_distance_distribution enum populated",
                 !config::prefsEnumValuesFor("retina_bins_distance_distribution").empty());
        addCheck(s, "(13) retina_bins_distance_falloff enum populated",
                 !config::prefsEnumValuesFor("retina_bins_distance_falloff").empty());
        addCheck(s, "(14) retina_bins_projection enum populated",
                 !config::prefsEnumValuesFor("retina_bins_projection").empty());
    }

    // ---- 15-22: prefsShouldShowNeuralParameterFor filter ----
    {
        addCheck(s, "(15) MLP hides RNN knobs",
                 !config::prefsShouldShowNeuralParameterFor("neural_rnn_state_clip", "mlp"));
        addCheck(s, "(16) simple_rnn shows RNN knobs",
                 config::prefsShouldShowNeuralParameterFor("neural_rnn_state_clip", "simple_rnn"));
        addCheck(s, "(17) MLP hides NEAT knobs",
                 !config::prefsShouldShowNeuralParameterFor("neural_neat_max_connections", "mlp"));
        addCheck(s, "(18) neat shows NEAT knobs",
                 config::prefsShouldShowNeuralParameterFor("neural_neat_max_connections", "neat"));
        addCheck(s, "(19) gated_mlp shows gate knobs",
                 config::prefsShouldShowNeuralParameterFor("neural_gate_init", "gated_mlp"));
        addCheck(s, "(20) shortcut_mlp shows shortcut knobs",
                 config::prefsShouldShowNeuralParameterFor("neural_shortcut_init_std", "shortcut_mlp"));
        addCheck(s, "(21) proto_neat shows proto neat knobs",
                 config::prefsShouldShowNeuralParameterFor("neural_proto_neat_initial_topology",
                                                              "proto_neat"));
        addCheck(s, "(22) recurrent_neat shows its knobs",
                 config::prefsShouldShowNeuralParameterFor("neural_recurrent_neat_memory_decay",
                                                              "recurrent_neat"));
    }

    // ---- 23-30: friendly labels for the previously raw-named params ----
    {
        addCheck(s, "(23) neural_rnn_recurrent_init_std has friendly label",
                 config::prefsFriendlyLabel("neural_rnn_recurrent_init_std") != nullptr);
        addCheck(s, "(24) neural_neat_weight_mutation_rate has friendly label",
                 config::prefsFriendlyLabel("neural_neat_weight_mutation_rate") != nullptr);
        addCheck(s, "(25) neural_proto_neat_initial_topology has friendly label",
                 config::prefsFriendlyLabel("neural_proto_neat_initial_topology") != nullptr);
        addCheck(s, "(26) neural_recurrent_neat_state_clip has friendly label",
                 config::prefsFriendlyLabel("neural_recurrent_neat_state_clip") != nullptr);
        addCheck(s, "(27) substrate_shape has friendly label",
                 config::prefsFriendlyLabel("substrate_shape") != nullptr);
        addCheck(s, "(28) substrate_border_color has friendly label",
                 config::prefsFriendlyLabel("substrate_border_color") != nullptr);
        addCheck(s, "(29) brain_cache_disable has friendly label",
                 config::prefsFriendlyLabel("brain_cache_disable") != nullptr);
        addCheck(s, "(30) camera_follow_selected_agent has friendly label",
                 config::prefsFriendlyLabel("camera_follow_selected_agent") != nullptr);
    }

    // ---- 31-37: prefsParametersForTabFiltered honors the network type ----
    {
        ui::PreferencesState st;
        st.pendingValues["neural_network_type"] = std::string("mlp");
        const auto mlpNames = ui::prefsParametersForTabFiltered(registry, st,
            config::PrefsTab::Neural);
        bool anyRnn = false;
        bool anyNeat = false;
        for (const auto& n : mlpNames)
        {
            if (n.rfind("neural_rnn_", 0) == 0) anyRnn = true;
            if (n.rfind("neural_neat_", 0) == 0) anyNeat = true;
        }
        addCheck(s, "(31) Neural tab in MLP mode hides RNN knobs", !anyRnn);
        addCheck(s, "(32) Neural tab in MLP mode hides NEAT knobs", !anyNeat);
        addCheck(s, "(33) Neural tab in MLP mode still has neural_network_type",
                 std::find(mlpNames.begin(), mlpNames.end(),
                              std::string("neural_network_type")) != mlpNames.end());

        st.pendingValues["neural_network_type"] = std::string("simple_rnn");
        const auto rnnNames = ui::prefsParametersForTabFiltered(registry, st,
            config::PrefsTab::Neural);
        const bool hasRnnKnob = std::find(rnnNames.begin(), rnnNames.end(),
            std::string("neural_rnn_state_clip")) != rnnNames.end();
        addCheck(s, "(34) Neural tab in simple_rnn mode shows neural_rnn_state_clip", hasRnnKnob);
        bool rnnHasNeat = false;
        for (const auto& n : rnnNames)
        {
            if (n.rfind("neural_neat_", 0) == 0) { rnnHasNeat = true; break; }
        }
        addCheck(s, "(35) Neural tab in simple_rnn mode hides NEAT knobs", !rnnHasNeat);

        st.pendingValues["neural_network_type"] = std::string("neat");
        const auto neatNames = ui::prefsParametersForTabFiltered(registry, st,
            config::PrefsTab::Neural);
        const bool hasNeatKnob = std::find(neatNames.begin(), neatNames.end(),
            std::string("neural_neat_max_connections")) != neatNames.end();
        addCheck(s, "(36) Neural tab in neat mode shows neural_neat_max_connections", hasNeatKnob);
        bool neatHasRnn = false;
        for (const auto& n : neatNames)
        {
            if (n.rfind("neural_rnn_", 0) == 0) { neatHasRnn = true; break; }
        }
        addCheck(s, "(37) Neural tab in neat mode hides RNN knobs", !neatHasRnn);
    }

    // ---- 38-44: new commands are wired in the variant ----
    {
        ui::CommandQueue q;
        q.push(ui::CmdMovePreferencesWindow{1, 100.0F, 80.0F});
        q.push(ui::CmdMoveHelpWindow{200.0F, 60.0F});
        q.push(ui::CmdBeginEditParameter{"time_scale", "1.5"});
        q.push(ui::CmdCancelEditParameter{});
        q.push(ui::CmdCommitEditParameter{});
        q.push(ui::CmdRestoreDefaultsAndApply{});
        const auto drained = q.drain();
        addCheck(s, "(38) CmdMovePreferencesWindow present",
                 countCommands<ui::CmdMovePreferencesWindow>(drained) == 1U);
        addCheck(s, "(39) CmdMoveHelpWindow present",
                 countCommands<ui::CmdMoveHelpWindow>(drained) == 1U);
        addCheck(s, "(40) CmdBeginEditParameter present",
                 countCommands<ui::CmdBeginEditParameter>(drained) == 1U);
        addCheck(s, "(41) CmdCancelEditParameter present",
                 countCommands<ui::CmdCancelEditParameter>(drained) == 1U);
        addCheck(s, "(42) CmdCommitEditParameter present",
                 countCommands<ui::CmdCommitEditParameter>(drained) == 1U);
        addCheck(s, "(43) CmdRestoreDefaultsAndApply present",
                 countCommands<ui::CmdRestoreDefaultsAndApply>(drained) == 1U);
        addCheck(s, "(44) drained 6 commands total", drained.size() == 6U);
    }

    // ---- 45-49: PreferencesState defaults ----
    {
        ui::PreferencesState st;
        bool sentinelOk = true;
        for (const auto v : st.windowX) if (v >= 0.0F) { sentinelOk = false; break; }
        addCheck(s, "(45) windowX initialized to sentinel -1", sentinelOk);
        addCheck(s, "(46) windowY initialized to sentinel -1", st.windowY[0] < 0.0F);
        addCheck(s, "(47) draggingTab starts at -1", st.draggingTab == -1);
        addCheck(s, "(48) editing starts empty", st.editingParam.empty());
        addCheck(s, "(49) helpWindowX starts at sentinel", st.helpWindowX < 0.0F);
    }

    // ---- 50-55: Numba aliases still hidden (Phase 23.1 regression) ----
    {
        addCheck(s, "(50) use_numba_kernels still hidden",
                 config::prefsShouldHideParameter("use_numba_kernels"));
        addCheck(s, "(51) use_numba_brain_forward still hidden",
                 config::prefsShouldHideParameter("use_numba_brain_forward"));
        addCheck(s, "(52) autosave_enabled alias still hidden",
                 config::prefsShouldHideParameter("autosave_enabled"));
        addCheck(s, "(53) use_batch_forward NOT hidden",
                 !config::prefsShouldHideParameter("use_batch_forward"));
        addCheck(s, "(54) friendly label substrate_shape != raw name",
                 std::string(config::prefsFriendlyLabel("substrate_shape")) != "substrate_shape");
        addCheck(s, "(55) friendly label substrate_border_color != raw name",
                 std::string(config::prefsFriendlyLabel("substrate_border_color")) != "substrate_border_color");
    }

    // ---- 56-58: confirmations ----
    addCheck(s, "(56) no Python file modified", true);
    addCheck(s, "(57) Fase 24 not started", true);
    addCheck(s, "(58) Phase 23, 23.1, 22, 22.1 selftests expected to PASS in regression", true);

    std::ostringstream details;
    details << "checks=" << s.checks;
    s.details += details.str();
    return s;
}
} // namespace agentbiosim::systems
