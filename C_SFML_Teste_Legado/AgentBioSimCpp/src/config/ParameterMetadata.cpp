#include "config/ParameterMetadata.hpp"

#include <algorithm>
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
