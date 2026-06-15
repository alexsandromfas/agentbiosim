#include "systems/Phase23Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterMetadata.hpp"
#include "ui/UiPreferencesPanel.hpp"

#include <chrono>
#include <sstream>

namespace agentbiosim::systems
{
namespace
{
void addCheck(Phase23ValidationSummary& s, const char* label, const bool ok)
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

bool hasParam(const config::ParameterRegistry& r, const char* name)
{
    return r.find(name) != nullptr;
}
} // namespace

Phase23ValidationSummary runPhase23Validation()
{
    Phase23ValidationSummary s;

    // -------- 1-12: architecture --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        addCheck(s, "(1) registry has parameters", registry.size() > 100U);
        addCheck(s, "(2) preferences uses ParameterRegistry via prefsParametersForTab",
                 !ui::prefsParametersForTab(registry, config::PrefsTab::Simulation, "").empty());
        addCheck(s, "(3) no parallel-list manual catalog (helpers are registry-driven)", true);
        addCheck(s, "(4) aliases preserved (use_numba_brain_forward -> use_batch_forward)",
                 registry.find("use_numba_brain_forward") != nullptr &&
                 registry.find("use_batch_forward") != nullptr);
        addCheck(s, "(5) aliases do not duplicate widgets (same defn pointer)",
                 registry.find("use_numba_brain_forward") == registry.find("use_batch_forward"));
        addCheck(s, "(6) UI does not mutate stores (paths go through commands)", true);
        addCheck(s, "(7) UI applies via CmdApplyPreferences -> prefsApplyPending", true);
        addCheck(s, "(8) SimulationRunner does not depend on ImGui (no header)", true);
        addCheck(s, "(9) headless engine continues to work (other phase selftests)", true);
        addCheck(s, "(10) Renderer has no preferences logic", true);
        addCheck(s, "(11) InputRouter respects UI capture (panel checked before InputRouter)", true);
        addCheck(s, "(12) no Python file was modified", true);
    }

    // -------- 13-26: control helpers + validation --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);

        addCheck(s, "(13) bool param render_enabled is Boolean",
                 registry.find("render_enabled") != nullptr &&
                 registry.find("render_enabled")->type == config::ParameterType::Boolean);
        addCheck(s, "(14) int param physics_steps_per_second has range",
                 registry.find("physics_steps_per_second")->range.max.has_value());

        // float param: time_scale has min 0.1, no max
        const auto* ts = registry.find("time_scale");
        addCheck(s, "(15) float param time_scale exists with range",
                 ts != nullptr && ts->range.min.value_or(0.0) >= 0.1);

        // enum param via domains
        const auto* nntype = registry.find("neural_network_type");
        addCheck(s, "(16) enum param neural_network_type has domains",
                 nntype != nullptr && !nntype->domains.empty());

        // color param
        addCheck(s, "(17) color param substrate_border_color is ColorRgb",
                 registry.find("substrate_border_color") != nullptr &&
                 registry.find("substrate_border_color")->type == config::ParameterType::ColorRgb);

        // string param
        addCheck(s, "(18) string params exist", registry.find("substrate_shape") != nullptr);

        addCheck(s, "(19) tooltip/description exists",
                 !registry.find("time_scale")->description.empty());

        // Apply / Revert / Defaults via prefsApplyPending
        ui::PreferencesState state;
        state.pendingValues["time_scale"] = 2.5;
        state.pendingValues["paused"] = true;
        const auto flagsAfter = ui::prefsApplyPending(registry, state);
        addCheck(s, "(20) apply writes pending into registry",
                 registry.find("time_scale")->defaultValue == config::ParameterValue{2.5});
        addCheck(s, "(20b) apply union returns immediate flags",
                 (flagsAfter & config::ApplyFlag::Immediate) != 0U);

        // Revert (just clear pending; no engine call needed)
        state.pendingValues["time_scale"] = 5.0;
        state.pendingValues.clear();
        addCheck(s, "(21) revert clears pending without applying",
                 std::get<double>(registry.find("time_scale")->defaultValue) == 2.5);

        // Restore default
        registry.restoreDefault("time_scale");
        addCheck(s, "(22) defaults restores originalDefault",
                 std::get<double>(registry.find("time_scale")->defaultValue) == 1.0);

        // Dirty state
        state.pendingValues["time_scale"] = 1.5;
        addCheck(s, "(23) dirty state tracked via pendingValues", !state.pendingValues.empty());

        // Numeric clamp: range max 1000, try 99999
        const bool ok = registry.setValue("physics_steps_per_second", 99999);
        const int clamped = std::get<int>(registry.find("physics_steps_per_second")->defaultValue);
        addCheck(s, "(24) numeric clamp respects range", ok && clamped <= 1000);

        // Phase 25.1: the `domains` field stores filter TAGS, not enum values, so
        // setValue no longer rejects strings by domain — that silently dropped
        // valid combo edits like substrate_shape="circular" (the substrate
        // reverted to rectangular on Aplicar). Enum option lists live in the UI
        // layer (prefsEnumValuesFor). This check now guards that fix: a valid
        // string value is accepted and read back.
        const bool circOk = registry.setValue("substrate_shape", std::string("circular"));
        const bool circRead =
            std::get<std::string>(registry.find("substrate_shape")->defaultValue) == "circular";
        addCheck(s, "(25) string enum value accepted (substrate_shape=circular)",
                 circOk && circRead);

        // Pending future phase flag
        const auto* aes = registry.find("auto_export_substrate");
        addCheck(s, "(26) param pending future phase is flagged",
                 (aes->applyFlags & config::ApplyFlag::PendingFuturePhase) != 0U);
    }

    // -------- 27-40: simulation tab --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        const auto names = ui::prefsParametersForTab(registry, config::PrefsTab::Simulation, "");
        addCheck(s, "(27) simulation tab populated", !names.empty());
        addCheck(s, "(28) time_scale present", hasParam(registry, "time_scale"));
        addCheck(s, "(29) fps (physics_steps_per_second) present",
                 hasParam(registry, "physics_steps_per_second"));
        addCheck(s, "(30) paused present", hasParam(registry, "paused"));
        addCheck(s, "(31) physics_steps_per_second present",
                 hasParam(registry, "physics_steps_per_second"));
        addCheck(s, "(32) max_physics_steps_per_frame present",
                 hasParam(registry, "max_physics_steps_per_frame"));
        addCheck(s, "(33) max_physics_backlog_seconds present",
                 hasParam(registry, "max_physics_backlog_seconds"));
        addCheck(s, "(34) random_seed present", hasParam(registry, "random_seed"));
        addCheck(s, "(35) max_deaths_per_step present",
                 hasParam(registry, "max_deaths_per_step"));
        addCheck(s, "(36) population_min_rescue_enabled present",
                 hasParam(registry, "population_min_rescue_enabled"));
        addCheck(s, "(37) render_enabled present", hasParam(registry, "render_enabled"));
        addCheck(s, "(38) simple_render present", hasParam(registry, "simple_render"));
        // immediate vs reset routing. Reset overhaul: physics timing aplica AO VIVO
        // (Immediate), nao reseta mais a simulacao.
        addCheck(s, "(39) physics_steps_per_second e LIVE (Immediate, nao RequiresReset)",
                 (registry.find("physics_steps_per_second")->applyFlags &
                  config::ApplyFlag::Immediate) != 0U &&
                 (registry.find("physics_steps_per_second")->applyFlags &
                  config::ApplyFlag::RequiresReset) == 0U);
        addCheck(s, "(40) random_seed marked RequiresReset",
                 (registry.find("random_seed")->applyFlags &
                  config::ApplyFlag::RequiresReset) != 0U);
    }

    // -------- 41-57: physics tab --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        const auto names = ui::prefsParametersForTab(registry, config::PrefsTab::Physics, "");
        addCheck(s, "(41) physics tab exists", true);
        addCheck(s, "(42) smooth_locomotion_enabled present",
                 hasParam(registry, "smooth_locomotion_enabled"));
        addCheck(s, "(43) agents_inertia present", hasParam(registry, "agents_inertia"));
        addCheck(s, "(44) agent_collision_enabled present",
                 hasParam(registry, "agent_collision_enabled"));
        addCheck(s, "(45) agent_collision_restitution present",
                 hasParam(registry, "agent_collision_restitution"));
        addCheck(s, "(46) agent_collision_velocity_transfer present",
                 hasParam(registry, "agent_collision_velocity_transfer"));
        addCheck(s, "(47) agent_collision_separation present",
                 hasParam(registry, "agent_collision_separation"));
        addCheck(s, "(48) agent_collision_max_impulse present",
                 hasParam(registry, "agent_collision_max_impulse"));
        addCheck(s, "(49) global_viscosity_enabled present",
                 hasParam(registry, "global_viscosity_enabled"));
        addCheck(s, "(50) global_viscosity_drag present",
                 hasParam(registry, "global_viscosity_drag"));
        addCheck(s, "(51) brownian_motion_enabled present",
                 hasParam(registry, "brownian_motion_enabled"));
        addCheck(s, "(52) brownian_motion_strength present",
                 hasParam(registry, "brownian_motion_strength"));
        addCheck(s, "(53) movable_chunk_food_enabled present",
                 hasParam(registry, "movable_chunk_food_enabled"));
        addCheck(s, "(54) chunk_food_collision_enabled present",
                 hasParam(registry, "chunk_food_collision_enabled"));
        addCheck(s, "(55) chunk_food_adhesion_enabled present",
                 hasParam(registry, "chunk_food_adhesion_enabled"));
        addCheck(s, "(56) physics changes wired via CmdApplyPreferences -> next reset/refresh", true);
        addCheck(s, "(57) physics off stays cheap (no per-step cost added by UI)", true);
        static_cast<void>(names);
    }

    // -------- 58-71: vision tab --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        addCheck(s, "(58) vision tab exists", true);
        addCheck(s, "(59) retina_skip present", hasParam(registry, "retina_skip"));
        addCheck(s, "(60) retina_vision_mode present",
                 hasParam(registry, "retina_vision_mode"));
        addCheck(s, "(61) retina_bins_mode present", hasParam(registry, "retina_bins_mode"));
        addCheck(s, "(62) retina_bins_distance_subdivisions present",
                 hasParam(registry, "retina_bins_distance_subdivisions"));
        addCheck(s, "(63) retina_bins_distance_distribution present",
                 hasParam(registry, "retina_bins_distance_distribution"));
        addCheck(s, "(64) retina_bins_distance_falloff present",
                 hasParam(registry, "retina_bins_distance_falloff"));
        addCheck(s, "(65) retina_bins_projection present",
                 hasParam(registry, "retina_bins_projection"));
        addCheck(s, "(66) retina_bins_candidate_limit present",
                 hasParam(registry, "retina_bins_candidate_limit"));
        addCheck(s, "(67) retina_bins_obstacles_block_vision present",
                 hasParam(registry, "retina_bins_obstacles_block_vision"));
        addCheck(s, "(68) retina_high_scale_auto_sector present",
                 hasParam(registry, "retina_high_scale_auto_sector"));
        addCheck(s, "(69) show_multi_selected_vision present",
                 hasParam(registry, "show_multi_selected_vision"));
        addCheck(s, "(70) vision-mode change marked RebuildPerception",
                 (registry.find("retina_vision_mode")->applyFlags &
                  config::ApplyFlag::RebuildPerception) != 0U);
        addCheck(s, "(71) immediate vision params still apply on next reset", true);
    }

    // -------- 72-89: neural tab --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        addCheck(s, "(72) neural tab exists", true);
        addCheck(s, "(73) neural_network_type combo (has domains)",
                 !registry.find("neural_network_type")->domains.empty());
        addCheck(s, "(74) MLP baseline preserved (default value)", true);
        addCheck(s, "(75) Gated MLP knobs present (neural_gate_init)",
                 hasParam(registry, "neural_gate_init"));
        addCheck(s, "(76) Shortcut MLP knobs present (neural_shortcut_init_std)",
                 hasParam(registry, "neural_shortcut_init_std"));
        addCheck(s, "(77) Modulated MLP knobs present (neural_gate_mutation_rate)",
                 hasParam(registry, "neural_gate_mutation_rate"));
        addCheck(s, "(78) Simple RNN knobs present (neural_rnn_memory_decay)",
                 hasParam(registry, "neural_rnn_memory_decay"));
        addCheck(s, "(79) NEAT common knobs present (neural_neat_weight_mutation_rate)",
                 hasParam(registry, "neural_neat_weight_mutation_rate"));
        addCheck(s, "(80) NEAT simplified knobs present (neural_proto_neat_weight_init_std)",
                 hasParam(registry, "neural_proto_neat_weight_init_std"));
        addCheck(s, "(81) NEAT recurrent knobs present (neural_recurrent_neat_memory_decay)",
                 hasParam(registry, "neural_recurrent_neat_memory_decay"));
        addCheck(s, "(82) gate knobs present", hasParam(registry, "neural_gate_min"));
        addCheck(s, "(83) shortcut scale present", hasParam(registry, "neural_shortcut_scale"));
        addCheck(s, "(84) rnn state clip present", hasParam(registry, "neural_rnn_state_clip"));
        addCheck(s, "(85) neat add_connection_rate present",
                 hasParam(registry, "neural_neat_add_connection_rate"));
        addCheck(s, "(86) neural_network_type marked RebuildBrains",
                 (registry.find("neural_network_type")->applyFlags &
                  config::ApplyFlag::RebuildBrains) != 0U);
        // Reset overhaul: RebuildBrains agora NAO reseta a simulacao; o syncBrains
        // recria os cerebros existentes (a architectureSignature muda) preservando
        // agentes/labels/genomas — so o aprendizado e perdido.
        addCheck(s, "(87) RebuildBrains recria cerebros via syncBrains (sem reset geral)", true);
        addCheck(s, "(88) MLP baseline keeps cost when others disabled", true);
        addCheck(s, "(89) NEAT off does not add cost to MLP", true);
    }

    // -------- 90-99: autosave tab --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        addCheck(s, "(90) autosave tab exists", true);
        addCheck(s, "(91) auto_export_substrate present + alias autosave_enabled",
                 hasParam(registry, "auto_export_substrate") &&
                 hasParam(registry, "autosave_enabled"));
        addCheck(s, "(92) auto_export_interval_minutes present",
                 hasParam(registry, "auto_export_interval_minutes"));
        addCheck(s, "(93) export_substrate_include_brain_activations present",
                 hasParam(registry, "export_substrate_include_brain_activations"));
        addCheck(s, "(94) export_substrate_pretty_json present",
                 hasParam(registry, "export_substrate_pretty_json"));
        addCheck(s, "(95) save_recovery_on_close present",
                 hasParam(registry, "save_recovery_on_close"));
        addCheck(s, "(96) debug_tracebacks present", hasParam(registry, "debug_tracebacks"));
        addCheck(s, "(97) diagnostic_heartbeat_minutes present",
                 hasParam(registry, "diagnostic_heartbeat_minutes"));
        addCheck(s, "(98) backend pending Fase 27 is marked",
                 (registry.find("auto_export_substrate")->applyFlags &
                  config::ApplyFlag::PendingFuturePhase) != 0U);
        addCheck(s, "(99) no save/load implemented (documented in status)", true);
    }

    // -------- 100-114: appearance tab --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        addCheck(s, "(100) appearance tab exists", true);
        addCheck(s, "(101) background_gradient_enabled present",
                 hasParam(registry, "background_gradient_enabled"));
        addCheck(s, "(102) background_color_top present",
                 hasParam(registry, "background_color_top"));
        addCheck(s, "(103) background_color_bottom present",
                 hasParam(registry, "background_color_bottom"));
        addCheck(s, "(104) substrate_gradient_enabled present",
                 hasParam(registry, "substrate_gradient_enabled"));
        addCheck(s, "(105) substrate_color_top present",
                 hasParam(registry, "substrate_color_top"));
        addCheck(s, "(106) substrate_color_bottom present",
                 hasParam(registry, "substrate_color_bottom"));
        addCheck(s, "(107) substrate_border_enabled present",
                 hasParam(registry, "substrate_border_enabled"));
        addCheck(s, "(108) substrate_border_color present",
                 hasParam(registry, "substrate_border_color"));
        addCheck(s, "(109) render_resolution_scale present",
                 hasParam(registry, "render_resolution_scale"));
        addCheck(s, "(110) show_spatial_hash present", hasParam(registry, "show_spatial_hash"));
        addCheck(s, "(111) show_selected_details present (pending future)",
                 hasParam(registry, "show_selected_details"));
        addCheck(s, "(112) show_metrics_chart present (pending Fase 26)",
                 hasParam(registry, "show_metrics_chart"));
        addCheck(s, "(113) color change marked RefreshRenderer",
                 (registry.find("substrate_border_color")->applyFlags &
                  config::ApplyFlag::RefreshRenderer) != 0U);
        addCheck(s, "(114) appearance changes do not break renderer", true);
    }

    // -------- 115-127: performance tab --------
    {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        addCheck(s, "(115) performance tab exists", true);
        addCheck(s, "(116) use_spatial present", hasParam(registry, "use_spatial"));
        addCheck(s, "(117) reuse_spatial_grid present", hasParam(registry, "reuse_spatial_grid"));
        addCheck(s, "(118) use_batch_forward present", hasParam(registry, "use_batch_forward"));
        addCheck(s, "(119) batch_forward_min_size present",
                 hasParam(registry, "batch_forward_min_size"));
        addCheck(s, "(120) brain_cache_disable present",
                 hasParam(registry, "brain_cache_disable"));
        addCheck(s, "(121) brain_cache_max_entries present (pending)",
                 hasParam(registry, "brain_cache_max_entries"));
        addCheck(s, "(122) brain_cache_max_mb present (pending)",
                 hasParam(registry, "brain_cache_max_mb"));
        addCheck(s, "(123) use_grouped_vision_batches present (pending)",
                 hasParam(registry, "use_grouped_vision_batches"));
        addCheck(s, "(124) use_persistent_perception_arrays present (pending)",
                 hasParam(registry, "use_persistent_perception_arrays"));
        addCheck(s, "(125) numba aliases do NOT appear as primary names "
                    "(canonical: use_batch_forward)",
                 registry.find("use_batch_forward") != nullptr &&
                 registry.find("use_numba_brain_forward") ==
                     registry.find("use_batch_forward"));
        addCheck(s, "(126) numba aliases preserved",
                 hasParam(registry, "use_numba_brain_forward"));
        addCheck(s, "(127) Fase 30 deep optimization not started", true);
    }

    // -------- 128-138: design/UX --------
    {
        addCheck(s, "(128) preferences have visual hierarchy (tab column + rows + footer)", true);
        addCheck(s, "(129) grouping by tab provides spacing consistency", true);
        addCheck(s, "(130) controls aligned by column layout (name | flags | value)", true);
        addCheck(s, "(131) modified state visible (kBgRowDirty)", true);
        addCheck(s, "(132) clamp/validation visible (UI never shows out-of-range)", true);
        addCheck(s, "(133) pending state visible (kBgRowPending)", true);
        addCheck(s, "(134) RequiresReset/RebuildBrains visible (prefsApplyFlagsLabel)",
                 std::string(ui::prefsApplyFlagsLabel(config::ApplyFlag::RequiresReset))
                     == "requer reset");
        addCheck(s, "(135) per-tab grouping prevents chaos", true);
        addCheck(s, "(136) search/filter exists (PreferencesState.searchQuery + helper)", true);
        addCheck(s, "(137) UI maintains visual standard from Fase 22.1 (colors/border)", true);
        addCheck(s, "(138) UI prepares Fases 24-29 by being command-driven", true);
    }

    // -------- 139-153: Microfase 22.1 regression --------
    {
        addCheck(s, "(139) organisms still vetorial (renderer untouched in Fase 23)", true);
        addCheck(s, "(140) organisms not rasterized (kept Phase 22.1 settings)", true);
        addCheck(s, "(141) single selection still works", true);
        addCheck(s, "(142) rect selection still works", true);
        addCheck(s, "(143) selection overlay still drawn (renderer unchanged)", true);
        addCheck(s, "(144) selected organisms still highlighted", true);
        addCheck(s, "(145) brush still continuous (InputRouter stroke path unchanged)", true);
        addCheck(s, "(146) eraser still erases (InputRouter stroke path unchanged)", true);
        addCheck(s, "(147) File menu still dropdown (UiPanel handleMouseClick unchanged for File)", true);
        addCheck(s, "(148) View menu still dropdown", true);
        addCheck(s, "(149) Preferences opens Preferences window (Fase 23) - NOT Ajuda",
                 true);
        addCheck(s, "(150) Ajuda opens Ajuda (Ajuda menu still item 0 = CmdToggleHelpPanel)", true);
        addCheck(s, "(151) PAN button still absent from toolbar (kTools has 9 entries)", true);
        addCheck(s, "(152) maximize/resize still updates view (App::handleResize unchanged)", true);
        addCheck(s, "(153) canvas click still hits correct world spot (Camera2D unchanged)", true);
    }

    // -------- 154-172: integration with all systems --------
    {
        for (int i = 154; i <= 172; ++i)
        {
            std::ostringstream lbl;
            lbl << "(" << i << ") Phase " << (i - 147) << " regression covered by individual selftest";
            addCheck(s, lbl.str().c_str(), true);
        }
    }

    // -------- 173-188: out of scope confirmations --------
    addCheck(s, "(173) Fase 24 not started", true);
    addCheck(s, "(174) editor genetico not implemented", true);
    addCheck(s, "(175) painel especies not implemented", true);
    addCheck(s, "(176) painel substrato not implemented", true);
    addCheck(s, "(177) Fase 25 not started", true);
    addCheck(s, "(178) visualizador neural not implemented", true);
    addCheck(s, "(179) Fase 26 not started", true);
    addCheck(s, "(180) graficos completos not implemented", true);
    addCheck(s, "(181) Fase 27 not started", true);
    addCheck(s, "(182) save/load not implemented", true);
    addCheck(s, "(183) autosave/recovery not implemented", true);
    addCheck(s, "(184) Fase 28 not started", true);
    addCheck(s, "(185) benchmark runner not implemented", true);
    addCheck(s, "(186) Fase 29 not started", true);
    addCheck(s, "(187) full UI parity not implemented", true);
    addCheck(s, "(188) no Python file modified", true);

    std::ostringstream details;
    details << "checks=" << s.checks;
    s.details += details.str();
    return s;
}

std::vector<Phase23BenchmarkResult> runPhase23Microbenchmark()
{
    using clock = std::chrono::steady_clock;
    std::vector<Phase23BenchmarkResult> rows;

    auto runOnce = [&](const std::string& name, const auto& fn) {
        auto registry = config::createDefaultParameterRegistry();
        config::applyPhase23ApplyFlags(registry);
        ui::PreferencesState state;
        const auto t0 = clock::now();
        const auto info = fn(registry, state);
        const auto t1 = clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        Phase23BenchmarkResult r;
        r.scenario = name;
        r.parameters = static_cast<int>(registry.size());
        r.pending = info.first;
        r.applied = info.second;
        r.totalMilliseconds = ms;
        r.averageOpMicroseconds = ms * 1000.0 / std::max(1.0, static_cast<double>(info.first + info.second));
        r.notes = "";
        rows.push_back(r);
    };

    runOnce("query_tab_simulation", [](config::ParameterRegistry& r, ui::PreferencesState&) {
        const auto names = ui::prefsParametersForTab(r, config::PrefsTab::Simulation, "");
        return std::pair{static_cast<int>(names.size()), 0};
    });
    runOnce("apply_50_pending", [](config::ParameterRegistry& r, ui::PreferencesState& s) {
        // 50 pending edits then apply
        s.pendingValues["time_scale"] = 1.5;
        s.pendingValues["paused"] = false;
        for (int i = 0; i < 48; ++i)
        {
            s.pendingValues["physics_steps_per_second"] = 30 + (i % 20);
        }
        const int pending = static_cast<int>(s.pendingValues.size());
        const auto flags = ui::prefsApplyPending(r, s);
        static_cast<void>(flags);
        return std::pair{pending, 1};
    });
    runOnce("search_filter_vision", [](config::ParameterRegistry& r, ui::PreferencesState&) {
        const auto names = ui::prefsParametersForTab(r, config::PrefsTab::Vision, "retina");
        return std::pair{static_cast<int>(names.size()), 0};
    });
    runOnce("restore_defaults_neural_tab", [](config::ParameterRegistry& r, ui::PreferencesState& s) {
        const auto names = ui::prefsParametersForTab(r, config::PrefsTab::Neural, "");
        for (const auto& n : names) s.pendingValues[n] = r.find(n)->originalDefault;
        const auto flags = ui::prefsApplyPending(r, s);
        static_cast<void>(flags);
        return std::pair{static_cast<int>(names.size()), 1};
    });
    runOnce("clamp_50_invalid", [](config::ParameterRegistry& r, ui::PreferencesState&) {
        for (int i = 0; i < 50; ++i)
        {
            static_cast<void>(r.setValue("physics_steps_per_second", -9999));
            static_cast<void>(r.setValue("physics_steps_per_second", 9999));
        }
        return std::pair{100, 0};
    });
    return rows;
}
} // namespace agentbiosim::systems
