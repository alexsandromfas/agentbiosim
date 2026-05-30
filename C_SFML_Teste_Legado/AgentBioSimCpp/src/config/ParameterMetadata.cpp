#include "config/ParameterMetadata.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace agentbiosim::config
{
namespace
{
struct FlagEntry { const char* name; unsigned int flags; };

// Phase 23: parameters whose effects require a world rebuild via reset().
// Changing world size or spawn topology mid-simulation requires reset.
const std::vector<FlagEntry> kRequireResetParams{
    {"world_w",                ApplyFlag::RequiresReset},
    {"world_h",                ApplyFlag::RequiresReset},
    {"substrate_radius",       ApplyFlag::RequiresReset},
    {"substrate_shape",        ApplyFlag::RequiresReset},
    {"random_seed",            ApplyFlag::RequiresReset},
    {"physics_steps_per_second", ApplyFlag::RequiresReset},
};

// Phase 23: parameters that need PerceptionSystem reconfig. The current
// implementation reconfigures perception on next initialize, so functionally
// they also require reset. We mark them explicitly so the UI badge says
// "requer rebuild de percepção".
const std::vector<FlagEntry> kRebuildPerceptionParams{
    {"retina_vision_mode",          ApplyFlag::RebuildPerception | ApplyFlag::RequiresReset},
    {"retina_bins_mode",            ApplyFlag::RebuildPerception | ApplyFlag::RequiresReset},
    {"retina_bins_distance_subdivisions", ApplyFlag::RebuildPerception | ApplyFlag::RequiresReset},
    {"retina_bins_distance_distribution", ApplyFlag::RebuildPerception | ApplyFlag::RequiresReset},
    {"retina_bins_distance_falloff",      ApplyFlag::RebuildPerception | ApplyFlag::RequiresReset},
    {"retina_bins_projection",            ApplyFlag::RebuildPerception | ApplyFlag::RequiresReset},
    {"retina_bins_candidate_limit",       ApplyFlag::RebuildPerception | ApplyFlag::RequiresReset},
    {"retina_bins_obstacles_block_vision",ApplyFlag::RebuildPerception | ApplyFlag::RequiresReset},
};

// Phase 23: parameters that change the brain type / topology. Changing them
// affects newly created brains only — existing brains are NOT recreated. The
// UI badge says "afeta novos agentes".
const std::vector<FlagEntry> kRebuildBrainsParams{
    {"neural_network_type",   ApplyFlag::RebuildBrains | ApplyFlag::RequiresReset},
    {"neural_neat_initial_topology",            ApplyFlag::RebuildBrains | ApplyFlag::RequiresReset},
    {"neural_proto_neat_initial_topology",      ApplyFlag::RebuildBrains | ApplyFlag::RequiresReset},
    {"neural_recurrent_neat_initial_topology",  ApplyFlag::RebuildBrains | ApplyFlag::RequiresReset},
};

// Phase 23: parameters that affect Renderer/UI on the next frame.
const std::vector<FlagEntry> kRefreshRendererParams{
    {"render_enabled",                ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"simple_render",                 ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"render_resolution_scale",       ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"substrate_bg_color",            ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"background_gradient_enabled",   ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"background_color_top",          ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"background_color_bottom",       ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"substrate_gradient_enabled",    ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"substrate_color_top",           ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"substrate_color_bottom",        ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"substrate_border_enabled",      ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
    {"substrate_border_color",        ApplyFlag::Immediate | ApplyFlag::RefreshRenderer},
};

// Phase 23: parameters whose backend lives in Fase 27 (save/load/autosave) or
// later — the UI exposes them but they have no engine effect yet.
const std::vector<FlagEntry> kPendingFuturePhaseParams{
    {"auto_export_substrate",                      ApplyFlag::PendingFuturePhase},
    {"auto_export_interval_minutes",               ApplyFlag::PendingFuturePhase},
    {"export_substrate_include_brain_activations", ApplyFlag::PendingFuturePhase},
    {"export_substrate_pretty_json",               ApplyFlag::PendingFuturePhase},
    {"save_recovery_on_close",                     ApplyFlag::PendingFuturePhase},
    {"debug_tracebacks",                           ApplyFlag::PendingFuturePhase},
    {"diagnostic_heartbeat_minutes",               ApplyFlag::PendingFuturePhase},
    {"brain_cache_disable",                        ApplyFlag::PendingFuturePhase},
    {"brain_cache_max_entries",                    ApplyFlag::PendingFuturePhase},
    {"brain_cache_max_mb",                         ApplyFlag::PendingFuturePhase},
    {"brain_cache_log",                            ApplyFlag::PendingFuturePhase},
    {"use_grouped_vision_batches",                 ApplyFlag::PendingFuturePhase},
    {"use_persistent_perception_arrays",           ApplyFlag::PendingFuturePhase},
    {"show_selected_details",                      ApplyFlag::PendingFuturePhase},
    {"show_metrics_chart",                         ApplyFlag::PendingFuturePhase},
    {"neural_view_dense_layout",                   ApplyFlag::PendingFuturePhase},
    {"camera_follow_selected_agent",               ApplyFlag::PendingFuturePhase},
    {"camera_follow_smoothing_enabled",            ApplyFlag::PendingFuturePhase},
    {"camera_follow_smoothing",                    ApplyFlag::PendingFuturePhase},
};

void applyFlagsFromTable(ParameterRegistry& registry, const std::vector<FlagEntry>& table)
{
    for (const auto& entry : table)
    {
        static_cast<void>(registry.setApplyFlags(entry.name, entry.flags));
    }
}
} // namespace

void applyPhase23ApplyFlags(ParameterRegistry& registry)
{
    applyFlagsFromTable(registry, kRequireResetParams);
    applyFlagsFromTable(registry, kRebuildPerceptionParams);
    applyFlagsFromTable(registry, kRebuildBrainsParams);
    applyFlagsFromTable(registry, kRefreshRendererParams);
    applyFlagsFromTable(registry, kPendingFuturePhaseParams);
}

const char* prefsCategoryDisplay(const std::string& category) noexcept
{
    if (category == "simulation")  return "Simulação";
    if (category == "physics")     return "Física";
    if (category == "vision")      return "Sistema de Visão";
    if (category == "neural")      return "Redes Neurais";
    if (category == "autosave")    return "Autosave";
    if (category == "appearance")  return "Aparência";
    if (category == "performance") return "Performance";
    return category.c_str();
}

const char* prefsTabLabel(const PrefsTab tab) noexcept
{
    switch (tab)
    {
    case PrefsTab::Simulation:  return "Simulação";
    case PrefsTab::Physics:     return "Física";
    case PrefsTab::Vision:      return "Sistema de Visão";
    case PrefsTab::Neural:      return "Redes Neurais";
    case PrefsTab::Autosave:    return "Autosave";
    case PrefsTab::Appearance:  return "Aparência";
    case PrefsTab::Performance: return "Performance";
    case PrefsTab::Count: break;
    }
    return "?";
}

namespace
{
// Phase 23.1: friendly Portuguese label map. ASCII-only on purpose. Internal
// name still shown in dim small text under the row for power users.
struct LabelEntry { const char* name; const char* label; };
constexpr std::array<LabelEntry, 70> kFriendlyLabels{{
    {"time_scale",                   "Velocidade da simulacao"},
    {"paused",                       "Pausa"},
    {"physics_steps_per_second",     "Passos de fisica por segundo"},
    {"max_physics_steps_per_frame",  "Maximo de passos por frame"},
    {"max_physics_backlog_seconds",  "Backlog maximo (segundos)"},
    {"random_seed",                  "Semente aleatoria (-1 = aleatoria)"},
    {"max_deaths_per_step",          "Mortes maximas por passo"},
    {"population_min_rescue_enabled","Resgatar populacao minima"},
    {"render_enabled",               "Renderizar"},
    {"simple_render",                "Renderizacao simples"},

    {"agents_inertia",               "Inercia dos agentes"},
    {"smooth_locomotion_enabled",    "Locomocao suavizada"},
    {"smooth_linear_inertia_enabled","Inercia linear suavizada"},
    {"smooth_max_linear_accel",      "Aceleracao linear maxima"},
    {"smooth_linear_drag_enabled",   "Arrasto linear suavizado"},
    {"smooth_linear_drag",           "Intensidade do arrasto linear"},
    {"smooth_angular_inertia_enabled","Inercia angular suavizada"},
    {"smooth_max_angular_accel",     "Aceleracao angular maxima"},
    {"smooth_angular_drag_enabled",  "Arrasto angular suavizado"},
    {"smooth_angular_drag",          "Intensidade do arrasto angular"},
    {"agent_collision_enabled",      "Colisao entre agentes"},
    {"agent_collision_elasticity_enabled","Elasticidade da colisao"},
    {"agent_collision_restitution",  "Restituicao (elasticidade)"},
    {"agent_collision_velocity_transfer","Transferencia de velocidade"},
    {"agent_collision_separation",   "Separacao apos colisao"},
    {"agent_collision_max_impulse",  "Impulso maximo"},
    {"global_viscosity_enabled",     "Viscosidade global"},
    {"global_viscosity_drag",        "Intensidade da viscosidade"},
    {"movable_chunk_food_enabled",   "Comida em pedacos com inercia"},
    {"chunk_food_collision_enabled", "Colisao entre pedacos de comida"},
    {"chunk_food_adhesion_enabled",  "Adesao entre pedacos"},
    {"chunk_food_adhesion_strength", "Forca de adesao"},
    {"chunk_food_mass_scale",        "Massa relativa dos pedacos"},
    {"chunk_food_drag",              "Arrasto dos pedacos"},
    {"chunk_food_push_strength",     "Empurrao em pedacos"},
    {"brownian_motion_enabled",      "Movimento browniano"},
    {"brownian_motion_strength",     "Intensidade do movimento browniano"},

    {"retina_skip",                  "Pular retinas (1 = nenhum skip)"},
    {"retina_vision_mode",           "Modo de visao da retina"},
    {"retina_bins_mode",             "Modo de bins"},
    {"retina_bins_distance_subdivisions","Subdivisoes de distancia"},
    {"retina_bins_distance_distribution","Distribuicao de distancias"},
    {"retina_bins_distance_falloff", "Decaimento por distancia"},
    {"retina_bins_projection",       "Projecao de bins"},
    {"retina_bins_candidate_limit",  "Limite de candidatos"},
    {"retina_bins_obstacles_block_vision","Obstaculos bloqueiam visao"},
    {"retina_high_scale_auto_sector","Auto-sector em alta escala"},
    {"retina_high_scale_sector_min_agents","Minimo de agentes para auto-sector"},
    {"retina_high_scale_global_sector","Sector global em alta escala"},
    {"show_multi_selected_vision",   "Mostrar visao dos selecionados"},

    {"neural_network_type",          "Tipo de rede neural"},
    {"neural_gate_init",             "Inicializacao do portao (Gated)"},
    {"neural_gate_min",              "Minimo do portao"},
    {"neural_gate_max",              "Maximo do portao"},
    {"neural_shortcut_init_std",     "Inicializacao do atalho (Shortcut)"},
    {"neural_shortcut_scale",        "Escala do atalho"},
    {"neural_rnn_memory_decay",      "Decaimento da memoria (RNN)"},
    {"neural_rnn_state_clip",        "Saturacao do estado (RNN)"},
    {"neural_rnn_reset_state_on_copy","Resetar estado ao copiar"},

    {"auto_export_substrate",        "Autosave ativado"},
    {"auto_export_interval_minutes", "Intervalo de autosave (min)"},
    {"export_substrate_include_brain_activations","Incluir ativacoes neurais"},
    {"export_substrate_pretty_json", "JSON formatado"},
    {"save_recovery_on_close",       "Salvar recuperacao ao fechar"},
    {"debug_tracebacks",             "Tracebacks de debug"},
    {"diagnostic_heartbeat_minutes", "Heartbeat de diagnostico (min)"},

    {"use_spatial",                  "Usar spatial hash"},
    {"reuse_spatial_grid",           "Reutilizar grid espacial"},
    {"use_batch_forward",            "Forward em lote (batch)"},
    {"batch_forward_min_size",       "Tamanho minimo do batch"},
}};
} // namespace

const char* prefsFriendlyLabel(const std::string& name) noexcept
{
    for (const auto& e : kFriendlyLabels)
    {
        if (name == e.name) return e.label;
    }
    return nullptr;
}

bool prefsShouldHideParameter(const std::string& name) noexcept
{
    // Phase 23.1: hide Numba aliases — the canonical C++ name is shown.
    if (name == "use_numba_kernels")              return true;
    if (name == "use_numba_batch_retina")         return true;
    if (name == "use_numba_locomotion_energy")    return true;
    if (name == "use_numba_brain_forward")        return true;
    if (name == "numba_brain_forward_min_batch")  return true;
    if (name == "use_native_brain_forward")       return true;
    if (name == "autosave_enabled")               return true;  // alias of auto_export_substrate
    // Hide knobs that the SFML-native panel cannot edit safely yet (raw strings
    // without domains) and species/food per-prefix params (Fase 24).
    if (name.rfind("herbivore_", 0) == 0)         return true;
    if (name.rfind("carnivore_", 0) == 0)         return true;
    return false;
}

int prefsTabForCategory(const std::string& c) noexcept
{
    // Phase 23: registry categories are dotted keys ("simulation.time",
    // "save.autosave", "performance.neural"). Take the root prefix for tab
    // routing. Categories that are not exposed via the prefs UI return -1.
    const auto dot = c.find('.');
    const std::string root = dot == std::string::npos ? c : c.substr(0, dot);
    if (root == "simulation" || root == "render" || root == "ui") return static_cast<int>(PrefsTab::Simulation);
    if (root == "physics")                  return static_cast<int>(PrefsTab::Physics);
    if (root == "vision")                   return static_cast<int>(PrefsTab::Vision);
    if (root == "neural")                   return static_cast<int>(PrefsTab::Neural);
    if (root == "save")                     return static_cast<int>(PrefsTab::Autosave);
    if (root == "appearance" || root == "world") return static_cast<int>(PrefsTab::Appearance);
    if (root == "performance")              return static_cast<int>(PrefsTab::Performance);
    // species.*, food.* etc. live in Fase 24 / not exposed here.
    return -1;
}
} // namespace agentbiosim::config
