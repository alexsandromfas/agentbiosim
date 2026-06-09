#include "config/ParameterMetadata.hpp"

#include "i18n/Locale.hpp"

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
    if (category == "simulation")  return i18n::tr("Simulação", "Simulation");
    if (category == "physics")     return i18n::tr("Física", "Physics");
    if (category == "vision")      return i18n::tr("Sistema de Visão", "Vision System");
    if (category == "neural")      return i18n::tr("Redes Neurais", "Neural Networks");
    if (category == "autosave")    return i18n::tr("Autosave", "Autosave");
    if (category == "appearance")  return i18n::tr("Aparência", "Appearance");
    if (category == "performance") return i18n::tr("Performance", "Performance");
    return category.c_str();
}

const char* prefsTabLabel(const PrefsTab tab) noexcept
{
    switch (tab)
    {
    case PrefsTab::Simulation:  return i18n::tr("Simulação", "Simulation");
    case PrefsTab::Physics:     return i18n::tr("Física", "Physics");
    case PrefsTab::Vision:      return i18n::tr("Sistema de Visão", "Vision System");
    case PrefsTab::Neural:      return i18n::tr("Redes Neurais", "Neural Networks");
    case PrefsTab::Autosave:    return i18n::tr("Autosave", "Autosave");
    case PrefsTab::Appearance:  return i18n::tr("Aparência", "Appearance");
    case PrefsTab::Performance: return i18n::tr("Performance", "Performance");
    case PrefsTab::Count: break;
    }
    return "?";
}

namespace
{
// Phase 23.1: friendly label map. ASCII-only on purpose. Phase 25.2: each entry
// now carries both languages; `prefsFriendlyLabel` resolves the active one.
struct LabelEntry { const char* name; const char* ptbr; const char* en; };

const char* pick(const LabelEntry& e) noexcept { return i18n::tr(e.ptbr, e.en); }

// Phase 23.2: friendly labels list grown to cover all neural family knobs.
const std::vector<LabelEntry> kFriendlyLabels{
    {"ui_language",                  "Idioma", "Language"},
    {"profiler_enabled",             "Profiler por sistema", "Per-system profiler"},
    {"metrics_enabled",              "Metricas (series temporais)", "Metrics (time series)"},
    {"metrics_max_samples",          "Maximo de amostras", "Max samples"},
    {"metrics_sample_interval",      "Intervalo de amostragem (passos)", "Sample interval (steps)"},
    {"log_level",                    "Nivel de log", "Log level"},
    {"time_scale",                   "Velocidade da simulacao", "Simulation speed"},
    {"paused",                       "Pausa", "Paused"},
    {"physics_steps_per_second",     "Passos de fisica por segundo", "Physics steps per second"},
    {"max_physics_steps_per_frame",  "Maximo de passos por frame", "Max steps per frame"},
    {"max_physics_backlog_seconds",  "Backlog maximo (segundos)", "Max backlog (seconds)"},
    {"random_seed",                  "Semente aleatoria (-1 = aleatoria)", "Random seed (-1 = random)"},
    {"max_deaths_per_step",          "Mortes maximas por passo", "Max deaths per step"},
    {"population_min_rescue_enabled","Resgatar populacao minima", "Rescue minimum population"},
    {"render_enabled",               "Renderizar", "Render"},
    {"simple_render",                "Renderizacao simples", "Simple rendering"},

    {"agents_inertia",               "Inercia dos agentes", "Agent inertia"},
    {"smooth_locomotion_enabled",    "Locomocao suavizada", "Smoothed locomotion"},
    {"smooth_linear_inertia_enabled","Inercia linear suavizada", "Smoothed linear inertia"},
    {"smooth_max_linear_accel",      "Aceleracao linear maxima", "Max linear acceleration"},
    {"smooth_linear_drag_enabled",   "Arrasto linear suavizado", "Smoothed linear drag"},
    {"smooth_linear_drag",           "Intensidade do arrasto linear", "Linear drag strength"},
    {"smooth_angular_inertia_enabled","Inercia angular suavizada", "Smoothed angular inertia"},
    {"smooth_max_angular_accel",     "Aceleracao angular maxima", "Max angular acceleration"},
    {"smooth_angular_drag_enabled",  "Arrasto angular suavizado", "Smoothed angular drag"},
    {"smooth_angular_drag",          "Intensidade do arrasto angular", "Angular drag strength"},
    {"agent_collision_enabled",      "Colisao entre agentes", "Agent-agent collision"},
    {"agent_collision_elasticity_enabled","Elasticidade da colisao", "Collision elasticity"},
    {"agent_collision_restitution",  "Restituicao (elasticidade)", "Restitution (bounciness)"},
    {"agent_collision_velocity_transfer","Transferencia de velocidade", "Velocity transfer"},
    {"agent_collision_separation",   "Separacao apos colisao", "Post-collision separation"},
    {"agent_collision_max_impulse",  "Impulso maximo", "Max impulse"},
    {"global_viscosity_enabled",     "Viscosidade global", "Global viscosity"},
    {"global_viscosity_drag",        "Intensidade da viscosidade", "Viscosity strength"},
    {"movable_chunk_food_enabled",   "Comida em pedacos com inercia", "Movable chunk food"},
    {"chunk_food_collision_enabled", "Colisao entre pedacos de comida", "Chunk food collision"},
    {"chunk_food_adhesion_enabled",  "Adesao entre pedacos", "Chunk adhesion"},
    {"chunk_food_adhesion_strength", "Forca de adesao", "Adhesion strength"},
    {"chunk_food_mass_scale",        "Massa relativa dos pedacos", "Relative chunk mass"},
    {"chunk_food_drag",              "Arrasto dos pedacos", "Chunk drag"},
    {"chunk_food_push_strength",     "Empurrao em pedacos", "Chunk push strength"},
    {"brownian_motion_enabled",      "Movimento browniano", "Brownian motion"},
    {"brownian_motion_strength",     "Intensidade do movimento browniano", "Brownian motion strength"},

    {"retina_skip",                  "Pular retinas (1 = nenhum skip)", "Retina skip (1 = no skip)"},
    {"retina_vision_mode",           "Modo de visao da retina", "Retina vision mode"},
    {"retina_bins_mode",             "Modo de bins", "Bins mode"},
    {"retina_bins_distance_subdivisions","Subdivisoes de distancia", "Distance subdivisions"},
    {"retina_bins_distance_distribution","Distribuicao de distancias", "Distance distribution"},
    {"retina_bins_distance_falloff", "Decaimento por distancia", "Distance falloff"},
    {"retina_bins_projection",       "Projecao de bins", "Bins projection"},
    {"retina_bins_candidate_limit",  "Limite de candidatos", "Candidate limit"},
    {"retina_bins_obstacles_block_vision","Obstaculos bloqueiam visao", "Obstacles block vision"},
    {"retina_high_scale_auto_sector","Auto-sector em alta escala", "Auto-sector at high scale"},
    {"retina_high_scale_sector_min_agents","Minimo de agentes para auto-sector", "Min agents for auto-sector"},
    {"retina_high_scale_global_sector","Sector global em alta escala", "Global sector at high scale"},
    {"show_multi_selected_vision",   "Mostrar visao dos selecionados", "Show vision of selected"},

    {"neural_network_type",          "Tipo de rede neural", "Neural network type"},
    {"neural_gate_init",             "Inicializacao do portao (Gated)", "Gate init (Gated)"},
    {"neural_gate_min",              "Minimo do portao", "Gate minimum"},
    {"neural_gate_max",              "Maximo do portao", "Gate maximum"},
    {"neural_shortcut_init_std",     "Inicializacao do atalho (Shortcut)", "Shortcut init std"},
    {"neural_shortcut_scale",        "Escala do atalho", "Shortcut scale"},
    {"neural_rnn_memory_decay",      "Decaimento da memoria (RNN)", "Memory decay (RNN)"},
    {"neural_rnn_state_clip",        "Saturacao do estado (RNN)", "State clip (RNN)"},
    {"neural_rnn_reset_state_on_copy","Resetar estado ao copiar", "Reset state on copy"},
    {"neural_rnn_recurrent_init_std","Desvio padrao inicial recorrente (RNN)", "Recurrent init std (RNN)"},
    {"neural_rnn_recurrent_scale",   "Escala da conexao recorrente (RNN)", "Recurrent connection scale (RNN)"},
    {"neural_rnn_mutation_rate",     "Taxa de mutacao (RNN)", "Mutation rate (RNN)"},
    {"neural_rnn_mutation_strength", "Forca da mutacao (RNN)", "Mutation strength (RNN)"},
    {"neural_gate_mutation_rate",    "Taxa de mutacao do portao", "Gate mutation rate"},
    {"neural_gate_mutation_strength","Forca da mutacao do portao", "Gate mutation strength"},
    {"neural_shortcut_mutation_rate","Taxa de mutacao do atalho", "Shortcut mutation rate"},
    {"neural_shortcut_mutation_strength","Forca da mutacao do atalho", "Shortcut mutation strength"},

    {"neural_neat_initial_topology",            "Topologia inicial (NEAT)", "Initial topology (NEAT)"},
    {"neural_neat_weight_init_std",             "Desvio padrao inicial dos pesos (NEAT)", "Weight init std (NEAT)"},
    {"neural_neat_weight_mutation_rate",        "Taxa de mutacao de pesos (NEAT)", "Weight mutation rate (NEAT)"},
    {"neural_neat_weight_mutation_strength",    "Forca da mutacao de pesos (NEAT)", "Weight mutation strength (NEAT)"},
    {"neural_neat_add_connection_rate",         "Taxa de adicionar conexao (NEAT)", "Add-connection rate (NEAT)"},
    {"neural_neat_add_node_rate",               "Taxa de adicionar no (NEAT)", "Add-node rate (NEAT)"},
    {"neural_neat_toggle_connection_rate",      "Taxa de alternar conexao (NEAT)", "Toggle-connection rate (NEAT)"},
    {"neural_neat_remove_connection_rate",      "Taxa de remover conexao (NEAT)", "Remove-connection rate (NEAT)"},
    {"neural_neat_reset_weight_rate",           "Taxa de zerar peso (NEAT)", "Reset-weight rate (NEAT)"},
    {"neural_neat_max_hidden_nodes",            "Maximo de neuronios ocultos (NEAT)", "Max hidden nodes (NEAT)"},
    {"neural_neat_max_connections",             "Maximo de conexoes (NEAT)", "Max connections (NEAT)"},

    {"neural_proto_neat_initial_topology",          "Topologia inicial (NEAT simplificada)", "Initial topology (proto-NEAT)"},
    {"neural_proto_neat_weight_init_std",           "Desvio padrao inicial dos pesos (proto NEAT)", "Weight init std (proto-NEAT)"},
    {"neural_proto_neat_weight_mutation_rate",      "Taxa de mutacao de pesos (proto NEAT)", "Weight mutation rate (proto-NEAT)"},
    {"neural_proto_neat_weight_mutation_strength",  "Forca da mutacao de pesos (proto NEAT)", "Weight mutation strength (proto-NEAT)"},
    {"neural_proto_neat_add_connection_rate",       "Taxa de adicionar conexao (proto NEAT)", "Add-connection rate (proto-NEAT)"},
    {"neural_proto_neat_add_node_rate",             "Taxa de adicionar no (proto NEAT)", "Add-node rate (proto-NEAT)"},
    {"neural_proto_neat_toggle_connection_rate",    "Taxa de alternar conexao (proto NEAT)", "Toggle-connection rate (proto-NEAT)"},
    {"neural_proto_neat_remove_connection_rate",    "Taxa de remover conexao (proto NEAT)", "Remove-connection rate (proto-NEAT)"},
    {"neural_proto_neat_reset_weight_rate",         "Taxa de zerar peso (proto NEAT)", "Reset-weight rate (proto-NEAT)"},
    {"neural_proto_neat_max_hidden_nodes",          "Maximo de neuronios ocultos (proto NEAT)", "Max hidden nodes (proto-NEAT)"},
    {"neural_proto_neat_max_connections",           "Maximo de conexoes (proto NEAT)", "Max connections (proto-NEAT)"},

    {"neural_recurrent_neat_initial_topology",          "Topologia inicial (NEAT recorrente)", "Initial topology (recurrent NEAT)"},
    {"neural_recurrent_neat_weight_init_std",           "Desvio padrao dos pesos (NEAT recorrente)", "Weight init std (recurrent NEAT)"},
    {"neural_recurrent_neat_weight_mutation_rate",      "Taxa de mutacao de pesos (NEAT recorrente)", "Weight mutation rate (recurrent NEAT)"},
    {"neural_recurrent_neat_weight_mutation_strength",  "Forca da mutacao de pesos (NEAT recorrente)", "Weight mutation strength (recurrent NEAT)"},
    {"neural_recurrent_neat_add_connection_rate",       "Taxa de adicionar conexao (NEAT recorrente)", "Add-connection rate (recurrent NEAT)"},
    {"neural_recurrent_neat_add_node_rate",             "Taxa de adicionar no (NEAT recorrente)", "Add-node rate (recurrent NEAT)"},
    {"neural_recurrent_neat_toggle_connection_rate",    "Taxa de alternar conexao (NEAT recorrente)", "Toggle-connection rate (recurrent NEAT)"},
    {"neural_recurrent_neat_remove_connection_rate",    "Taxa de remover conexao (NEAT recorrente)", "Remove-connection rate (recurrent NEAT)"},
    {"neural_recurrent_neat_reset_weight_rate",         "Taxa de zerar peso (NEAT recorrente)", "Reset-weight rate (recurrent NEAT)"},
    {"neural_recurrent_neat_max_hidden_nodes",          "Maximo de neuronios ocultos (NEAT recorrente)", "Max hidden nodes (recurrent NEAT)"},
    {"neural_recurrent_neat_max_connections",           "Maximo de conexoes (NEAT recorrente)", "Max connections (recurrent NEAT)"},
    {"neural_recurrent_neat_recurrent_connection_rate", "Taxa de conexao recorrente (NEAT recorrente)", "Recurrent-connection rate (recurrent NEAT)"},
    {"neural_recurrent_neat_memory_decay",              "Decaimento da memoria (NEAT recorrente)", "Memory decay (recurrent NEAT)"},
    {"neural_recurrent_neat_state_clip",                "Saturacao do estado (NEAT recorrente)", "State clip (recurrent NEAT)"},
    {"neural_recurrent_neat_reset_state_on_copy",       "Resetar estado ao copiar (NEAT recorrente)", "Reset state on copy (recurrent NEAT)"},

    {"auto_export_substrate",        "Autosave ativado", "Autosave enabled"},
    {"auto_export_interval_minutes", "Intervalo de autosave (min)", "Autosave interval (min)"},
    {"export_substrate_include_brain_activations","Incluir ativacoes neurais", "Include neural activations"},
    {"export_substrate_pretty_json", "JSON formatado", "Pretty JSON"},
    {"save_recovery_on_close",       "Salvar recuperacao ao fechar", "Save recovery on close"},
    {"debug_tracebacks",             "Tracebacks de debug", "Debug tracebacks"},
    {"diagnostic_heartbeat_minutes", "Heartbeat de diagnostico (min)", "Diagnostic heartbeat (min)"},

    {"use_spatial",                  "Usar spatial hash", "Use spatial hash"},
    {"reuse_spatial_grid",           "Reutilizar grid espacial", "Reuse spatial grid"},
    {"use_batch_forward",            "Forward em lote (batch)", "Batch forward"},
    {"batch_forward_min_size",       "Tamanho minimo do batch", "Min batch size"},

    // Simulation / time / world
    {"substrate_shape",              "Formato do substrato", "Substrate shape"},
    {"world_w",                      "Largura do mundo", "World width"},
    {"world_h",                      "Altura do mundo", "World height"},
    {"substrate_radius",             "Raio do substrato (circular)", "Substrate radius (circular)"},
    {"render_resolution_scale",      "Escala de resolucao de renderizacao", "Render resolution scale"},
    {"show_spatial_hash",            "Mostrar grade espacial", "Show spatial grid"},
    {"show_selected_details",        "Mostrar detalhes do agente selecionado", "Show selected agent details"},
    {"show_metrics_chart",           "Mostrar grafico de metricas", "Show metrics chart"},
    {"neural_view_dense_layout",     "Layout denso da visualizacao neural", "Dense neural view layout"},
    {"camera_follow_selected_agent", "Camera segue o agente selecionado", "Camera follows selected agent"},
    {"camera_follow_smoothing_enabled","Suavizacao do seguimento da camera", "Camera follow smoothing"},
    {"camera_follow_smoothing",      "Intensidade da suavizacao da camera", "Camera follow smoothing strength"},

    // Background and substrate colors
    {"substrate_bg_color",                "Cor de fundo", "Background color"},
    {"background_gradient_enabled",       "Usar degrade no fundo", "Use background gradient"},
    {"background_color_top",              "Cor do fundo (topo)", "Background color (top)"},
    {"background_color_bottom",           "Cor do fundo (base)", "Background color (bottom)"},
    {"substrate_gradient_enabled",        "Usar degrade no substrato", "Use substrate gradient"},
    {"substrate_color_top",               "Cor do substrato (topo)", "Substrate color (top)"},
    {"substrate_color_bottom",            "Cor do substrato (base)", "Substrate color (bottom)"},
    {"substrate_border_enabled",          "Mostrar borda do substrato", "Show substrate border"},
    {"substrate_border_color",            "Cor da borda do substrato", "Substrate border color"},

    // Brain cache (Fase 27)
    {"brain_cache_disable",          "Desativar cache de cerebros (Fase 27)", "Disable brain cache (Phase 27)"},
    {"brain_cache_max_entries",      "Maximo de entradas do cache (Fase 27)", "Max cache entries (Phase 27)"},
    {"brain_cache_max_mb",           "Tamanho maximo do cache em MB (Fase 27)", "Max cache size in MB (Phase 27)"},
    {"brain_cache_log",              "Registrar atividade do cache (Fase 27)", "Log cache activity (Phase 27)"},

    // Performance / future
    {"use_grouped_vision_batches",       "Visao em lotes agrupados (Fase 30)", "Grouped vision batches (Phase 30)"},
    {"use_persistent_perception_arrays", "Buffers de percepcao persistentes (Fase 30)", "Persistent perception buffers (Phase 30)"},

    // Phase 24.1: populacao knobs (absolute names for the Populacao panel).
    {"bacteria_count",      "Quantidade inicial de bacterias", "Initial bacteria count"},
    {"predator_count",      "Quantidade inicial de predadores", "Initial predator count"},
    {"bacteria_min_limit",  "Limite minimo de bacterias", "Minimum bacteria limit"},
    {"bacteria_max_limit",  "Limite maximo de bacterias (0 = sem limite)", "Maximum bacteria limit (0 = no limit)"},
    {"predator_min_limit",  "Limite minimo de predadores", "Minimum predator limit"},
    {"predator_max_limit",  "Limite maximo de predadores (0 = sem limite)", "Maximum predator limit (0 = no limit)"},
    {"predators_enabled",   "Habilitar predadores", "Enable predators"},

    // Phase 25.1: substrato + comida (apareciam com nome interno em ingles).
    {"food_mode",                    "Modo da comida", "Food mode"},
    {"food_target",                  "Quantidade alvo de comida", "Target food amount"},
    {"food_min_r",                   "Raio minimo de spawn da comida", "Food spawn min radius"},
    {"food_max_r",                   "Raio maximo de spawn da comida", "Food spawn max radius"},
    {"food_replenish_interval",      "Intervalo de reposicao (s)", "Replenish interval (s)"},
    {"food_color",                   "Cor da comida", "Food color"},
    {"food_bite_seconds",            "Tempo para consumir uma particula (s)", "Time to consume a particle (s)"},
    {"food_piece_particle_radius",   "Raio da particula (pedaco)", "Particle radius (chunk)"},
    {"food_piece_cluster_radius",    "Raio do cluster (pedaco)", "Cluster radius (chunk)"},
    {"food_piece_particle_spacing",  "Espacamento entre particulas", "Particle spacing"},
    {"food_piece_replenish_mode",    "Modo de reposicao (pedacos)", "Replenish mode (chunks)"},
    {"food_trim_max_per_step",       "Maximo de particulas removidas por passo", "Max particles trimmed per step"},
};
} // namespace

const char* prefsFriendlyLabel(const std::string& name) noexcept
{
    for (const auto& e : kFriendlyLabels)
    {
        if (name == e.name) return pick(e);
    }
    return nullptr;
}

namespace
{
// Phase 24.1 fix: labels by SUFFIX for species-prefixed knobs. After stripping
// "bacteria_" / "predator_" the same label applies. Phase 25.2: bilingual.
const std::vector<LabelEntry> kSpeciesSuffixLabels{
    {"body_size",                "Tamanho do corpo", "Body size"},
    {"body_shape",               "Forma do corpo", "Body shape"},
    {"max_speed",                "Velocidade maxima", "Max speed"},
    {"max_turn",                 "Giro maximo (rad/s)", "Max turn (rad/s)"},
    {"allow_reverse_locomotion", "Permitir marcha re", "Allow reverse locomotion"},
    {"movement_mode",            "Modo de movimento", "Movement mode"},
    {"initial_energy",           "Energia inicial", "Initial energy"},
    {"death_energy",             "Energia minima de sobrevivencia", "Minimum survival energy"},
    {"split_energy",             "Energia para reproducao", "Energy to reproduce"},
    {"v0_cost",                  "Custo parado", "Idle cost"},
    {"vmax_cost",                "Custo em velocidade maxima", "Cost at max speed"},
    {"energy_cap",               "Energia maxima (cap)", "Maximum energy (cap)"},
    {"death_by_age_enabled",     "Morrer por idade", "Death by age"},
    {"death_age",                "Idade de morte (s)", "Death age (s)"},
    {"corpse_to_food",           "Cadaver vira comida", "Corpse becomes food"},
    {"reproduction_min_age",     "Idade minima para reproduzir (s)", "Min age to reproduce (s)"},
    {"reproduction_cooldown",    "Cooldown de reproducao (s)", "Reproduction cooldown (s)"},
    {"vision_radius",            "Raio de visao", "Vision radius"},
    {"retina_count",             "Quantidade de retinas", "Retina count"},
    {"retina_fov_degrees",       "Campo de visao (graus)", "Field of view (degrees)"},
    {"eye_count",                "Quantidade de olhos", "Eye count"},
    {"eye_angle_degrees",        "Angulo entre olhos (graus)", "Angle between eyes (degrees)"},
    {"see_food",                 "Enxerga comida", "Sees food"},
    {"see_agents",               "Enxerga organismos", "Sees organisms"},
    {"see_predators",            "Enxerga predadores", "Sees predators"},
    {"see_obstacles",            "Enxerga obstaculos", "Sees obstacles"},
    {"see_through_walls",        "Enxerga atraves de paredes", "Sees through walls"},
    {"retina_channel_r",         "Canal R da retina", "Retina R channel"},
    {"retina_channel_g",         "Canal G da retina", "Retina G channel"},
    {"retina_channel_b",         "Canal B da retina", "Retina B channel"},
    {"retina_channel_d",         "Canal de distancia", "Distance channel"},
    {"retina_input_mode",        "Modo de entrada da retina", "Retina input mode"},
    {"diet_food",                "Come comida", "Eats food"},
    {"diet_agents",              "Come organismos", "Eats organisms"},
    {"diet_same_label",          "Pode comer mesma especie", "Can eat same species"},
    {"food_efficiency",          "Eficiencia ao comer comida", "Food-eating efficiency"},
    {"agent_efficiency",         "Eficiencia ao comer agente", "Agent-eating efficiency"},
    {"hidden_layers",            "Camadas ocultas (rede)", "Hidden layers (network)"},
    {"mutation_rate",            "Taxa de mutacao", "Mutation rate"},
    {"mutation_strength",        "Intensidade da mutacao", "Mutation strength"},
    {"count",                    "Quantidade inicial", "Initial count"},
    {"min_r",                    "Raio minimo de spawn", "Spawn min radius"},
    {"max_r",                    "Raio maximo de spawn", "Spawn max radius"},
    {"min_limit",                "Limite minimo de populacao", "Minimum population limit"},
    {"max_limit",                "Limite maximo de populacao (0 = sem limite)", "Maximum population limit (0 = no limit)"},
    {"color",                    "Cor", "Color"},
};
} // namespace

const char* prefsFriendlyLabelBySuffix(const std::string& suffix) noexcept
{
    for (const auto& e : kSpeciesSuffixLabels)
    {
        if (suffix == e.name) return pick(e);
    }
    return nullptr;
}

// Phase 23.2: dedicated enum values for string parameters that the prefs UI
// renders as combos. The registry's `domains` field stores tags, not enum
// values, so we cannot derive the combo content from it.
namespace
{
bool nameEndsWith(const std::string& name, const char* suffix) noexcept
{
    const std::string s = suffix;
    return name.size() >= s.size() &&
           name.compare(name.size() - s.size(), s.size(), s) == 0;
}
} // namespace

std::vector<std::string> prefsEnumValuesFor(const std::string& name)
{
    // Phase 25.2: UI language selector.
    if (name == "ui_language") return {"pt-br", "en"};
    // Phase 27: log verbosity selector.
    if (name == "log_level") return {"off", "error", "warn", "info", "debug"};

    // Phase 25.1: species-prefixed enums (bacteria_/predator_/<new species>_).
    // Matched by suffix so the Editor Genetico renders them as dropdowns.
    if (name == "body_shape" || nameEndsWith(name, "_body_shape"))
        return {"ellipse", "circle"};
    if (name == "movement_mode" || nameEndsWith(name, "_movement_mode"))
        return {"forward", "omni"};
    if (name == "retina_input_mode" || nameEndsWith(name, "_retina_input_mode"))
        return {"distance_only", "color_distance", "color_plus_distance", "color_only"};

    // Phase 25.1: food/substrate enums.
    if (name == "food_mode")          return {"instant", "chunk"};
    if (name == "food_piece_replenish_mode")
        return {"spawn_cluster", "grow_existing", "grow_particles"};

    if (name == "substrate_shape")    return {"rectangular", "circular"};
    if (name == "neural_network_type") return {
        "mlp",
        "gated_mlp",
        "shortcut_mlp",
        "modulated_mlp",
        "simple_rnn",
        "neat",
        "proto_neat",
        "recurrent_neat"
    };
    if (name == "retina_vision_mode")  return {"frontal", "omni", "raycast", "raycast_omni"};
    if (name == "retina_bins_mode")    return {"single", "sector", "global", "auto_sector"};
    if (name == "retina_bins_distance_distribution") return {"linear", "log", "quadratic"};
    if (name == "retina_bins_distance_falloff")      return {"none", "linear", "exponential"};
    if (name == "retina_bins_projection")            return {"flat", "fisheye"};
    if (name == "neural_neat_initial_topology")            return {"empty", "minimal", "layered"};
    if (name == "neural_proto_neat_initial_topology")      return {"empty", "minimal", "layered"};
    if (name == "neural_recurrent_neat_initial_topology")  return {"empty", "minimal", "layered"};
    return {};
}

std::string prefsEnumDisplayLabel(const std::string& name, const std::string& v)
{
    const auto is = [&v](const char* x) { return v == x; };
    namespace tr = i18n;

    // Phase 25.2: language names are endonyms — shown the same regardless of the
    // currently-active language so a user who flipped to a language they cannot
    // read can still find their way back.
    if (name == "ui_language")
    {
        if (is("pt-br")) return "Portugues (BR)";
        if (is("en"))    return "English";
    }
    if (name == "log_level")
    {
        if (is("off"))   return tr::tr("Desligado", "Off");
        if (is("error")) return tr::tr("Erro", "Error");
        if (is("warn"))  return tr::tr("Aviso", "Warning");
        if (is("info"))  return tr::tr("Info", "Info");
        if (is("debug")) return tr::tr("Depuracao", "Debug");
    }
    if (name == "substrate_shape")
    {
        if (is("rectangular")) return tr::tr("Retangular", "Rectangular");
        if (is("circular"))    return tr::tr("Circular", "Circular");
    }
    if (name == "food_mode")
    {
        if (is("instant")) return tr::tr("Instantanea", "Instant");
        if (is("chunk"))   return tr::tr("Em pedacos", "Chunks");
    }
    if (name == "food_piece_replenish_mode")
    {
        if (is("spawn_cluster"))  return tr::tr("Novo cluster", "New cluster");
        if (is("grow_existing"))  return tr::tr("Crescer existentes", "Grow existing");
        if (is("grow_particles")) return tr::tr("Crescer particulas", "Grow particles");
    }
    if (name == "body_shape" || nameEndsWith(name, "_body_shape"))
    {
        if (is("ellipse")) return tr::tr("Elipse", "Ellipse");
        if (is("circle"))  return tr::tr("Circulo", "Circle");
    }
    if (name == "movement_mode" || nameEndsWith(name, "_movement_mode"))
    {
        if (is("forward")) return tr::tr("Para frente", "Forward");
        if (is("omni"))    return tr::tr("Omnidirecional", "Omnidirectional");
    }
    if (name == "retina_input_mode" || nameEndsWith(name, "_retina_input_mode"))
    {
        if (is("distance_only"))      return tr::tr("Apenas distancia", "Distance only");
        if (is("color_distance"))     return tr::tr("Cor + distancia (combinadas)", "Color + distance (combined)");
        if (is("color_plus_distance"))return tr::tr("Cor e distancia (separadas)", "Color and distance (separate)");
        if (is("color_only"))         return tr::tr("Apenas cor", "Color only");
    }
    if (name == "retina_vision_mode")
    {
        if (is("frontal"))      return tr::tr("Frontal", "Frontal");
        if (is("omni"))         return tr::tr("Omni", "Omni");
        if (is("raycast"))      return tr::tr("Raycast", "Raycast");
        if (is("raycast_omni")) return tr::tr("Raycast omni", "Raycast omni");
    }
    if (name == "retina_bins_mode")
    {
        if (is("single"))      return tr::tr("Unico", "Single");
        if (is("sector"))      return tr::tr("Setor", "Sector");
        if (is("global"))      return tr::tr("Global", "Global");
        if (is("auto_sector")) return tr::tr("Auto-setor", "Auto-sector");
    }
    if (name == "retina_bins_distance_distribution")
    {
        if (is("linear"))    return tr::tr("Linear", "Linear");
        if (is("log"))       return tr::tr("Logaritmica", "Logarithmic");
        if (is("quadratic")) return tr::tr("Quadratica", "Quadratic");
    }
    if (name == "retina_bins_distance_falloff")
    {
        if (is("none"))        return tr::tr("Nenhum", "None");
        if (is("linear"))      return tr::tr("Linear", "Linear");
        if (is("exponential")) return tr::tr("Exponencial", "Exponential");
    }
    if (name == "retina_bins_projection")
    {
        if (is("flat"))    return tr::tr("Plana", "Flat");
        if (is("fisheye")) return tr::tr("Olho de peixe", "Fisheye");
    }
    if (nameEndsWith(name, "initial_topology"))
    {
        if (is("empty"))   return tr::tr("Vazia", "Empty");
        if (is("minimal")) return tr::tr("Minima", "Minimal");
        if (is("layered")) return tr::tr("Em camadas", "Layered");
    }
    if (name == "neural_network_type")
    {
        if (is("mlp"))            return tr::tr("MLP", "MLP");
        if (is("gated_mlp"))      return tr::tr("MLP com portao (gated)", "Gated MLP");
        if (is("shortcut_mlp"))   return tr::tr("MLP com atalho (shortcut)", "Shortcut MLP");
        if (is("modulated_mlp"))  return tr::tr("MLP modulada", "Modulated MLP");
        if (is("simple_rnn"))     return tr::tr("RNN simples", "Simple RNN");
        if (is("neat"))           return tr::tr("NEAT", "NEAT");
        if (is("proto_neat"))     return tr::tr("NEAT simplificada", "Proto-NEAT");
        if (is("recurrent_neat")) return tr::tr("NEAT recorrente", "Recurrent NEAT");
    }
    return v;
}

// Phase 25.2: curated, localized parameter help. Two tables — exact-name for
// global parameters and by-suffix for per-species genome parameters (shared by
// bacteria_*, predator_* and any user species). The text explains what the
// parameter does and the practical effect of changing it, which the old terse
// one-liners in ParameterDefaults did not. ASCII-only, same rationale as labels.
namespace
{
struct HelpEntry { const char* name; const char* ptbr; const char* en; };

const std::vector<HelpEntry> kParameterHelp{
    {"ui_language",
     "Idioma de toda a interface. A troca e aplicada na hora, sem reiniciar.",
     "Language of the entire interface. The change applies instantly, without restarting."},
    {"time_scale",
     "Multiplicador da velocidade da simulacao. Acima de 1 acelera o tempo simulado; abaixo de 1 desacelera, util para observar comportamentos rapidos em detalhe.",
     "Simulation speed multiplier. Above 1 fast-forwards simulated time; below 1 slows it down, useful for examining fast behaviors in detail."},
    {"paused",
     "Congela a simulacao. Os agentes ficam parados, mas a interface e a camera continuam respondendo.",
     "Freezes the simulation. Agents stop, but the interface and camera stay responsive."},
    {"physics_steps_per_second",
     "Quantos passos de fisica sao calculados por segundo simulado. Valores maiores deixam colisoes e movimento mais precisos, porem custam mais CPU.",
     "How many physics steps are computed per simulated second. Higher values make collisions and movement more accurate but cost more CPU."},
    {"max_physics_steps_per_frame",
     "Teto de passos de fisica por quadro. Limita o quanto a simulacao tenta 'recuperar' quando o computador nao acompanha, evitando travamentos em cadeia.",
     "Cap on physics steps per rendered frame. Limits how much the simulation tries to 'catch up' when the machine falls behind, preventing stutter spirals."},
    {"random_seed",
     "Semente do gerador aleatorio. Um valor fixo torna a simulacao reproduzivel; -1 sorteia uma semente nova a cada reset.",
     "Seed for the random generator. A fixed value makes the run reproducible; -1 draws a fresh seed on every reset."},
    {"population_min_rescue_enabled",
     "Quando uma especie chega perto da extincao, repoe individuos automaticamente ate o limite minimo, evitando que a simulacao esvazie.",
     "When a species nears extinction, automatically respawns individuals up to the minimum limit so the simulation does not empty out."},
    {"max_deaths_per_step",
     "Limite de mortes processadas por passo. Suaviza picos de mortalidade em massa, distribuindo as mortes por varios passos.",
     "Limit on deaths processed per step. Smooths mass-mortality spikes by spreading deaths across several steps."},
    {"render_enabled",
     "Liga ou desliga o desenho do mundo. Desligar acelera muito execucoes longas em que so interessa o resultado final.",
     "Turns world drawing on or off. Turning it off greatly speeds up long runs where only the final result matters."},
    {"simple_render",
     "Desenho simplificado dos agentes (sem detalhes). Aumenta o FPS quando ha muitos organismos na tela.",
     "Simplified agent drawing (no detail). Raises FPS when many organisms are on screen."},
    {"substrate_shape",
     "Formato do mundo: retangular ou circular. Muda como os agentes esbarram nas bordas e onde a comida pode aparecer.",
     "Shape of the world: rectangular or circular. Changes how agents hit the borders and where food can appear."},
    {"world_w",
     "Largura do mundo retangular, em unidades de simulacao. Mundos maiores diluem a populacao e a comida.",
     "Width of the rectangular world, in simulation units. Larger worlds spread out population and food."},
    {"world_h",
     "Altura do mundo retangular, em unidades de simulacao. Mundos maiores diluem a populacao e a comida.",
     "Height of the rectangular world, in simulation units. Larger worlds spread out population and food."},
    {"substrate_radius",
     "Raio do mundo circular. So tem efeito quando o formato do substrato e circular.",
     "Radius of the circular world. Only has effect when the substrate shape is circular."},
    {"food_mode",
     "Como a comida se comporta: 'instantanea' some inteira ao ser tocada; 'em pedacos' precisa ser consumida aos poucos, como um aglomerado de particulas.",
     "How food behaves: 'instant' disappears whole when touched; 'chunks' must be eaten gradually, like a cluster of particles."},
    {"food_target",
     "Quantidade de comida que o mundo tenta manter. A reposicao trabalha para chegar nesse alvo.",
     "Amount of food the world tries to maintain. Replenishment works toward this target."},
    {"food_replenish_interval",
     "Intervalo, em segundos, entre reposicoes de comida. Intervalos curtos mantem o mundo mais farto.",
     "Interval, in seconds, between food replenishments. Short intervals keep the world better fed."},
    {"food_min_r",
     "Distancia minima do centro em que a comida pode nascer. Use junto com o raio maximo para criar aneis ou zonas vazias.",
     "Minimum distance from the center where food can spawn. Combine with the max radius to create rings or empty zones."},
    {"food_max_r",
     "Distancia maxima do centro em que a comida pode nascer. Valores menores concentram a comida no meio do mundo.",
     "Maximum distance from the center where food can spawn. Smaller values concentrate food in the middle of the world."},
    {"food_color",
     "Cor com que as particulas de comida sao desenhadas.",
     "Color used to draw food particles."},
    {"neural_network_type",
     "Arquitetura do cerebro dos agentes. Cada tipo tem capacidade de memoria e custo de calculo diferentes; trocar afeta apenas agentes criados depois.",
     "Architecture of the agents' brain. Each type has different memory capacity and compute cost; changing it affects only agents created afterwards."},
    {"retina_vision_mode",
     "Como a retina enxerga o entorno: frontal (so a frente), omni (em volta) ou por raios (raycast). Afeta o que o cerebro recebe como entrada e o custo da percepcao.",
     "How the retina sees the surroundings: frontal (front only), omni (all around) or raycast. Affects what the brain receives as input and the cost of perception."},
    {"agents_inertia",
     "Da inercia ao movimento: os agentes aceleram e freiam gradualmente em vez de mudar de velocidade instantaneamente. Deixa a locomocao mais organica.",
     "Gives movement inertia: agents accelerate and brake gradually instead of changing speed instantly. Makes locomotion more organic."},
    {"agent_collision_enabled",
     "Faz os agentes colidirem fisicamente uns com os outros em vez de se atravessarem.",
     "Makes agents physically collide with one another instead of passing through."},
    {"brownian_motion_enabled",
     "Adiciona um tremor aleatorio ao movimento, imitando agitacao termica. Util para evitar que organismos parados fiquem perfeitamente imoveis.",
     "Adds a random jitter to movement, mimicking thermal agitation. Useful to keep idle organisms from sitting perfectly still."},
    {"auto_export_substrate",
     "Salva o substrato automaticamente em intervalos regulares. (Backend completo previsto para a Fase 27.)",
     "Automatically saves the substrate at regular intervals. (Full backend planned for Phase 27.)"},
    {"predators_enabled",
     "Habilita a especie predadora. Desligar deixa apenas as bacterias, util para estudar crescimento sem predacao.",
     "Enables the predator species. Disabling leaves only bacteria, useful to study growth without predation."},
};

// Per-species genome parameters, matched by suffix (after stripping the
// species prefix). Shared by bacteria_*, predator_* and user species.
const std::vector<HelpEntry> kSpeciesHelp{
    {"body_size",
     "Tamanho do corpo. Corpos maiores ocupam mais espaco e colidem com area maior, mas costumam gastar mais energia para se mover.",
     "Body size. Larger bodies take up more space and collide over a bigger area, but usually spend more energy to move."},
    {"body_shape",
     "Forma do corpo (elipse ou circulo). Afeta a aparencia e a area de colisao do organismo.",
     "Body shape (ellipse or circle). Affects the organism's appearance and collision area."},
    {"max_speed",
     "Velocidade maxima de deslocamento. Mais velocidade ajuda a cacar ou fugir, mas o custo de energia cresce com a velocidade.",
     "Maximum travel speed. More speed helps hunt or flee, but energy cost rises with speed."},
    {"max_turn",
     "Velocidade maxima de giro, em radianos por segundo. Valores altos deixam o organismo mais agil para mudar de direcao.",
     "Maximum turn rate, in radians per second. High values make the organism more agile at changing direction."},
    {"allow_reverse_locomotion",
     "Permite andar para tras. Desligado, o organismo so se move para a frente e precisa girar para mudar de rumo.",
     "Allows moving backwards. When off, the organism only moves forward and must turn to change direction."},
    {"movement_mode",
     "Modo de locomocao: 'para frente' segue a direcao do corpo; 'omnidirecional' permite deslizar em qualquer direcao.",
     "Locomotion mode: 'forward' follows the body's heading; 'omnidirectional' lets it slide in any direction."},
    {"initial_energy",
     "Energia com que o organismo nasce. Mais energia inicial aumenta a chance de sobreviver ate encontrar comida.",
     "Energy the organism is born with. More starting energy increases the odds of surviving until it finds food."},
    {"death_energy",
     "Nivel de energia abaixo do qual o organismo morre de fome. Valores maiores tornam a vida mais arriscada.",
     "Energy level below which the organism starves to death. Higher values make life riskier."},
    {"split_energy",
     "Energia necessaria para se reproduzir. Valores menores aceleram a reproducao e o crescimento populacional.",
     "Energy required to reproduce. Lower values speed up reproduction and population growth."},
    {"v0_cost",
     "Gasto de energia por segundo estando parado (metabolismo basal). Valores altos pressionam o organismo a comer com frequencia.",
     "Energy spent per second while idle (basal metabolism). High values pressure the organism to eat often."},
    {"vmax_cost",
     "Gasto extra de energia ao se mover em velocidade maxima. Define o quanto correr e caro em relacao a ficar parado.",
     "Extra energy spent when moving at top speed. Sets how expensive sprinting is compared to standing still."},
    {"energy_cap",
     "Energia maxima que o organismo pode acumular. Limita quanto da para 'estocar' antes de reproduzir.",
     "Maximum energy the organism can accumulate. Limits how much it can 'stockpile' before reproducing."},
    {"death_by_age_enabled",
     "Liga a morte por velhice. Desligado, os organismos so morrem por fome ou predacao.",
     "Enables death by old age. When off, organisms only die from starvation or predation."},
    {"death_age",
     "Idade, em segundos, em que o organismo morre de velhice (quando a morte por idade esta ligada).",
     "Age, in seconds, at which the organism dies of old age (when death-by-age is enabled)."},
    {"corpse_to_food",
     "Ao morrer, o organismo vira comida no lugar onde caiu, reciclando energia de volta para o ecossistema.",
     "On death, the organism turns into food where it fell, recycling energy back into the ecosystem."},
    {"reproduction_min_age",
     "Idade minima, em segundos, antes que o organismo possa se reproduzir. Evita reproducao imediata apos o nascimento.",
     "Minimum age, in seconds, before the organism can reproduce. Prevents reproducing immediately after birth."},
    {"reproduction_cooldown",
     "Tempo de espera, em segundos, entre duas reproducoes do mesmo organismo. Controla o ritmo de natalidade.",
     "Wait time, in seconds, between two reproductions of the same organism. Controls the birth rate."},
    {"vision_radius",
     "Ate que distancia o organismo enxerga. Raios maiores detectam comida e ameacas mais longe, mas custam mais para processar.",
     "How far the organism can see. Larger radii detect food and threats from farther away, but cost more to process."},
    {"retina_count",
     "Numero de retinas (faixas sensoriais) ao redor do organismo. Mais retinas dao uma visao mais detalhada do entorno.",
     "Number of retinas (sensory bands) around the organism. More retinas give a more detailed view of the surroundings."},
    {"retina_fov_degrees",
     "Abertura do campo de visao, em graus. Angulos largos veem mais em volta; estreitos focam a frente.",
     "Field-of-view angle, in degrees. Wide angles see more around; narrow ones focus ahead."},
    {"eye_count",
     "Quantidade de olhos. Mais olhos ampliam a cobertura visual ao redor do corpo.",
     "Number of eyes. More eyes widen the visual coverage around the body."},
    {"eye_angle_degrees",
     "Angulo de separacao entre os olhos, em graus. Define o quanto a visao se espalha lateralmente.",
     "Separation angle between eyes, in degrees. Sets how far vision spreads sideways."},
    {"see_food",
     "Permite que o organismo enxergue comida. Desligar cega-o para recursos.",
     "Lets the organism see food. Turning it off blinds it to resources."},
    {"see_agents",
     "Permite enxergar outros organismos. Base para comportamentos de cardume, fuga e caca.",
     "Lets it see other organisms. The basis for schooling, fleeing and hunting behaviors."},
    {"see_predators",
     "Permite enxergar predadores especificamente. Ajuda a desenvolver fuga.",
     "Lets it see predators specifically. Helps evolve fleeing behavior."},
    {"see_obstacles",
     "Permite enxergar obstaculos do mundo, util para aprender a desviar.",
     "Lets it see world obstacles, useful for learning to avoid them."},
    {"see_through_walls",
     "Se ligado, a visao atravessa paredes e obstaculos; se desligado, eles bloqueiam a linha de visao.",
     "When on, vision passes through walls and obstacles; when off, they block the line of sight."},
    {"retina_input_mode",
     "O que cada retina entrega ao cerebro: so distancia, so cor, ou combinacoes das duas. Muda a riqueza e o tamanho da entrada sensorial.",
     "What each retina feeds the brain: distance only, color only, or combinations. Changes the richness and size of the sensory input."},
    {"diet_food",
     "Permite que o organismo se alimente de comida do ambiente.",
     "Lets the organism feed on environmental food."},
    {"diet_agents",
     "Permite que o organismo coma outros organismos (predacao/canibalismo).",
     "Lets the organism eat other organisms (predation/cannibalism)."},
    {"diet_same_label",
     "Permite comer individuos da propria especie. Desligado, ele poupa os seus.",
     "Allows eating individuals of its own species. When off, it spares its own kind."},
    {"food_efficiency",
     "Fracao da energia da comida que e de fato aproveitada ao comer. Eficiencia baixa exige comer mais.",
     "Fraction of a food's energy actually absorbed when eating. Low efficiency means it must eat more."},
    {"agent_efficiency",
     "Fracao da energia aproveitada ao comer outro organismo. Define o quanto a predacao compensa.",
     "Fraction of energy absorbed when eating another organism. Sets how rewarding predation is."},
    {"hidden_layers",
     "Tamanho das camadas ocultas do cerebro (ex.: '16,8'). Redes maiores aprendem comportamentos mais complexos, porem mutam mais devagar.",
     "Sizes of the brain's hidden layers (e.g. '16,8'). Bigger networks can learn more complex behaviors but mutate more slowly."},
    {"mutation_rate",
     "Probabilidade de cada peso da rede sofrer mutacao na reproducao. Valores altos diversificam rapido, mas podem destruir adaptacoes ja conquistadas.",
     "Probability that each network weight mutates at reproduction. High values diversify quickly but can wreck already-won adaptations."},
    {"mutation_strength",
     "Intensidade de cada mutacao quando ela ocorre. Controla o tamanho do 'salto' genetico entre pais e filhos.",
     "Magnitude of each mutation when it occurs. Controls the size of the genetic 'jump' between parent and offspring."},
    {"count",
     "Quantidade de individuos criados ao iniciar ou resetar a simulacao.",
     "Number of individuals created when starting or resetting the simulation."},
    {"min_limit",
     "Populacao minima da especie. Com o resgate ligado, individuos sao repostos para nao cair abaixo disso.",
     "Minimum population for the species. With rescue on, individuals are respawned so it does not drop below this."},
    {"max_limit",
     "Populacao maxima da especie (0 = sem limite). Impede explosoes populacionais que travam a simulacao.",
     "Maximum population for the species (0 = no limit). Prevents population explosions that bog down the simulation."},
    {"color",
     "Cor com que os individuos da especie sao desenhados.",
     "Color used to draw the species' individuals."},
};

bool stripSpeciesPrefix(const std::string& name, std::string& suffixOut)
{
    static const char* kPrefixes[] = {"bacteria_", "predator_"};
    for (const char* p : kPrefixes)
    {
        const std::string pre = p;
        if (name.size() > pre.size() && name.compare(0, pre.size(), pre) == 0)
        {
            suffixOut = name.substr(pre.size());
            return true;
        }
    }
    return false;
}
} // namespace

const char* prefsParameterHelp(const std::string& name) noexcept
{
    for (const auto& e : kParameterHelp)
    {
        if (name == e.name) return i18n::tr(e.ptbr, e.en);
    }
    std::string suffix;
    if (stripSpeciesPrefix(name, suffix))
    {
        for (const auto& e : kSpeciesHelp)
        {
            if (suffix == e.name) return i18n::tr(e.ptbr, e.en);
        }
    }
    return nullptr;
}

int prefsDecimalsFor(const std::string& name) noexcept
{
    // Integer-like doubles: whole numbers make sense, decimals do not.
    if (name == "world_w" || name == "world_h" || name == "substrate_radius") return 0;
    if (name == "food_piece_particle_radius" || name == "food_piece_cluster_radius" ||
        name == "food_piece_particle_spacing") return 0;
    if (nameEndsWith(name, "_body_size") || nameEndsWith(name, "_vision_radius") ||
        nameEndsWith(name, "_max_speed") || nameEndsWith(name, "_retina_fov_degrees") ||
        nameEndsWith(name, "_eye_angle_degrees")) return 0;

    // Fine-grained fractions: small values that need precision.
    const auto has = [&name](const char* token) {
        return name.find(token) != std::string::npos;
    };
    if (has("rate") || has("_std") || has("decay") || has("clip") || has("scale") ||
        has("strength") || has("restitution") || has("drag") || has("efficiency") ||
        has("elasticity") || has("init")) return 3;

    return 2;
}

// Phase 23.2: per-architecture parameter prefixes used by the neural window
// filter. A parameter belongs to a specific architecture if its name starts
// with one of these prefixes; otherwise it is a common knob.
namespace
{
bool isNeuralPrefix(const std::string& n, const char* prefix)
{
    return n.rfind(prefix, 0) == 0;
}
}

bool prefsShouldShowNeuralParameterFor(
    const std::string& name, const std::string& currentNetworkType) noexcept
{
    // Always show the type selector itself.
    if (name == "neural_network_type") return true;

    // Detect architecture prefix.
    if (isNeuralPrefix(name, "neural_gate_"))           return currentNetworkType == "gated_mlp" ||
                                                                   currentNetworkType == "modulated_mlp";
    if (isNeuralPrefix(name, "neural_shortcut_"))       return currentNetworkType == "shortcut_mlp";
    if (isNeuralPrefix(name, "neural_rnn_"))            return currentNetworkType == "simple_rnn";
    if (isNeuralPrefix(name, "neural_proto_neat_"))     return currentNetworkType == "proto_neat";
    if (isNeuralPrefix(name, "neural_recurrent_neat_")) return currentNetworkType == "recurrent_neat";
    if (isNeuralPrefix(name, "neural_neat_"))           return currentNetworkType == "neat";

    // Common neural.* knobs always show.
    return true;
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
