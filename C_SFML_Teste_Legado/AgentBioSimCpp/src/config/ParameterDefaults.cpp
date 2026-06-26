#include "config/ParameterDefaults.hpp"

#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace agentbiosim::config
{
namespace
{
std::vector<std::string> strings(std::initializer_list<const char*> values)
{
    std::vector<std::string> out;
    out.reserve(values.size());
    for (const char* value : values)
    {
        out.emplace_back(value);
    }
    return out;
}

void addBool(ParameterRegistry& registry,
             const char* name,
             const bool defaultValue,
             const char* category,
             const char* description,
             std::initializer_list<const char*> aliases,
             std::initializer_list<const char*> domains)
{
    registry.add({name, ParameterType::Boolean, defaultValue, category, description, strings(aliases), {}, strings(domains)});
}

void addInt(ParameterRegistry& registry,
            const char* name,
            const int defaultValue,
            const char* category,
            const char* description,
            const std::optional<double> minValue,
            const std::optional<double> maxValue,
            std::initializer_list<const char*> aliases,
            std::initializer_list<const char*> domains)
{
    registry.add({name,
                  ParameterType::Integer,
                  defaultValue,
                  category,
                  description,
                  strings(aliases),
                  {minValue, maxValue},
                  strings(domains)});
}

void addDouble(ParameterRegistry& registry,
               const char* name,
               const double defaultValue,
               const char* category,
               const char* description,
               const std::optional<double> minValue,
               const std::optional<double> maxValue,
               std::initializer_list<const char*> aliases,
               std::initializer_list<const char*> domains)
{
    registry.add({name,
                  ParameterType::Floating,
                  defaultValue,
                  category,
                  description,
                  strings(aliases),
                  {minValue, maxValue},
                  strings(domains)});
}

void addString(ParameterRegistry& registry,
               const char* name,
               const char* defaultValue,
               const char* category,
               const char* description,
               std::initializer_list<const char*> aliases,
               std::initializer_list<const char*> domains)
{
    registry.add({name, ParameterType::String, std::string(defaultValue), category, description, strings(aliases), {}, strings(domains)});
}

void addColor(ParameterRegistry& registry,
              const char* name,
              const ColorRgb defaultValue,
              const char* category,
              const char* description,
              std::initializer_list<const char*> aliases,
              std::initializer_list<const char*> domains)
{
    registry.add({name, ParameterType::ColorRgb, defaultValue, category, description, strings(aliases), {}, strings(domains)});
}

struct SpeciesDefaults
{
    const char* prefix;
    const char* label;
    int count;
    double minRadius;
    double maxRadius;
    int minLimit;
    int maxLimit;
    double v0Cost;
    double vmaxCost;
    double energyCap;
    double bodySize;
    int hiddenLayers;
    int layer1;
    int layer2;
    int layer3;
    int layer4;
    int layer5;
    bool seeFood;
    bool seeAgents;
    bool dietFood;
    bool dietAgents;
    ColorRgb color;
};

void registerSpeciesParameters(ParameterRegistry& registry, const SpeciesDefaults& defaults)
{
    const std::string p = defaults.prefix;
    const std::string category = std::string("species.") + p;
    const std::string speciesDomain = std::string("species.") + p;

    addInt(registry, (p + "_count").c_str(), defaults.count, category.c_str(), "Initial population count.", 0.0, std::nullopt, {}, {"runtime", speciesDomain.c_str()});
    addDouble(registry, (p + "_min_r").c_str(), defaults.minRadius, category.c_str(), "Legacy minimum spawn radius.", 0.0, std::nullopt, {}, {"runtime", speciesDomain.c_str()});
    addDouble(registry, (p + "_max_r").c_str(), defaults.maxRadius, category.c_str(), "Legacy maximum spawn radius.", 0.0, std::nullopt, {}, {"runtime", speciesDomain.c_str()});
    addInt(registry, (p + "_min_limit").c_str(), defaults.minLimit, category.c_str(), "Minimum population limit.", 0.0, std::nullopt, {}, {"runtime", speciesDomain.c_str()});
    addInt(registry, (p + "_max_limit").c_str(), defaults.maxLimit, category.c_str(), "Maximum population limit; zero means unlimited for legacy bacteria.", 0.0, std::nullopt, {}, {"runtime", speciesDomain.c_str()});

    addDouble(registry, (p + "_initial_energy").c_str(), 100.0, category.c_str(), "Initial energy for new organisms.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});
    addDouble(registry, (p + "_death_energy").c_str(), 50.0, category.c_str(), "Energy threshold for death.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});
    addDouble(registry, (p + "_split_energy").c_str(), 150.0, category.c_str(), "Energy threshold for reproduction.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});
    addBool(registry, (p + "_age_death_enabled").c_str(), false, category.c_str(), "Enable death by age.", {}, {"runtime", "energy", speciesDomain.c_str()});
    addDouble(registry, (p + "_death_age").c_str(), 3600.0, category.c_str(), "Age death threshold in seconds.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});
    addBool(registry, (p + "_corpse_to_food").c_str(), false, category.c_str(), "Convert dead organism to food.", {}, {"runtime", "food", speciesDomain.c_str()});
    addDouble(registry, (p + "_reproduction_min_age").c_str(), 0.0, category.c_str(), "Minimum age before reproduction.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});
    addDouble(registry, (p + "_reproduction_cooldown").c_str(), 0.0, category.c_str(), "Cooldown between reproductions.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});
    // Fase 34.3: reproduction strategy + litter size (per species genome trait).
    addString(registry, (p + "_reproduction_mode").c_str(), "energy", category.c_str(),
              "Reproduction trigger: 'energy' (reach split energy) or 'age' (by age + cooldown, no energy cost).",
              {}, {"runtime", "energy", speciesDomain.c_str()});
    addInt(registry, (p + "_offspring_count").c_str(), 1, category.c_str(),
           "How many children are created per reproduction event.", 1.0, std::nullopt, {},
           {"runtime", "energy", speciesDomain.c_str()});
    addDouble(registry, (p + "_metab_v0_cost").c_str(), defaults.v0Cost, category.c_str(), "Energy cost per second at zero speed.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});
    addDouble(registry, (p + "_metab_vmax_cost").c_str(), defaults.vmaxCost, category.c_str(), "Energy cost per second at maximum speed.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});
    addDouble(registry, (p + "_energy_cap").c_str(), defaults.energyCap, category.c_str(), "Maximum stored energy.", 0.0, std::nullopt, {}, {"runtime", "energy", speciesDomain.c_str()});

    addDouble(registry, (p + "_body_size").c_str(), defaults.bodySize, category.c_str(), "Fixed body size.", 0.1, std::nullopt, {}, {"runtime", speciesDomain.c_str()});
    addString(registry, (p + "_body_shape").c_str(), "ellipse", category.c_str(), "Body shape: ellipse or circle.", {}, {"runtime", speciesDomain.c_str()});
    addDouble(registry, (p + "_max_speed").c_str(), 300.0, category.c_str(), "Maximum movement speed.", 0.0, std::nullopt, {}, {"runtime", "physics", speciesDomain.c_str()});
    addDouble(registry, (p + "_max_turn").c_str(), 3.141592653589793, category.c_str(), "Maximum turn rate in radians.", 0.0, std::nullopt, {}, {"runtime", "physics", speciesDomain.c_str()});
    addBool(registry, (p + "_allow_reverse_locomotion").c_str(), false, category.c_str(), "Allow reverse movement.", {}, {"runtime", "physics", speciesDomain.c_str()});
    addString(registry, (p + "_movement_mode").c_str(), "forward", category.c_str(), "Locomotion output mode.", {}, {"runtime", "physics", speciesDomain.c_str()});

    addDouble(registry, (p + "_vision_radius").c_str(), 120.0, category.c_str(), "Sensor vision radius.", 0.0, std::nullopt, {}, {"runtime", "vision", speciesDomain.c_str()});
    addString(registry, (p + "_vision_mode").c_str(), "frontal", category.c_str(), "Legacy species vision mode.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addInt(registry, (p + "_retina_count").c_str(), 18, category.c_str(), "Number of retina bins/rays.", 1.0, std::nullopt, {}, {"runtime", "vision", speciesDomain.c_str()});
    addDouble(registry, (p + "_retina_fov_degrees").c_str(), 180.0, category.c_str(), "Retina field of view in degrees.", 1.0, 360.0, {}, {"runtime", "vision", speciesDomain.c_str()});
    addInt(registry, (p + "_eye_count").c_str(), 1, category.c_str(), "Number of eyes.", 1.0, 2.0, {}, {"runtime", "vision", speciesDomain.c_str()});
    addDouble(registry, (p + "_eye_angle_degrees").c_str(), 60.0, category.c_str(), "Angle between eyes.", 0.0, 360.0, {}, {"runtime", "vision", speciesDomain.c_str()});
    addDouble(registry, (p + "_eye_separation_degrees").c_str(), 45.0, category.c_str(), "Eye separation on body perimeter.", 0.0, 360.0, {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_show_vision").c_str(), false, category.c_str(), "Show vision overlay for this species.", {}, {"ui", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_see_food").c_str(), defaults.seeFood, category.c_str(), "Retina can see food.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_see_bacteria").c_str(), defaults.seeAgents, category.c_str(), "Retina can see organisms/legacy bacteria.", {(p + "_retina_see_agents").c_str()}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_see_predators").c_str(), false, category.c_str(), "Retina can see legacy predators.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_see_obstacles").c_str(), false, category.c_str(), "Retina can see obstacles.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_see_all").c_str(), false, category.c_str(), "Retina sees all colored substrate objects.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_see_through_walls").c_str(), true, category.c_str(), "Retina can see through obstacle walls.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addString(registry, (p + "_retina_input_mode").c_str(), "distance_only", category.c_str(), "Legacy retina input mode.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_channel_r").c_str(), false, category.c_str(), "Enable red retina channel.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_channel_g").c_str(), false, category.c_str(), "Enable green retina channel.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_channel_b").c_str(), false, category.c_str(), "Enable blue retina channel.", {}, {"runtime", "vision", speciesDomain.c_str()});
    addBool(registry, (p + "_retina_channel_d").c_str(), true, category.c_str(), "Enable dedicated distance retina channel.", {}, {"runtime", "vision", speciesDomain.c_str()});

    addBool(registry, (p + "_diet_food").c_str(), defaults.dietFood, category.c_str(), "Species can eat food.", {}, {"runtime", "food", speciesDomain.c_str()});
    addBool(registry, (p + "_diet_agents").c_str(), defaults.dietAgents, category.c_str(), "Species can eat other organisms.", {}, {"runtime", "food", speciesDomain.c_str()});
    addBool(registry, (p + "_diet_same_label").c_str(), false, category.c_str(), "Species can eat same label/species.", {(p + "_diet_same_species").c_str()}, {"runtime", "food", speciesDomain.c_str()});
    addDouble(registry, (p + "_diet_food_efficiency").c_str(), 1.0, category.c_str(), "Energy efficiency when eating food.", 0.0, std::nullopt, {}, {"runtime", "food", speciesDomain.c_str()});
    addDouble(registry, (p + "_diet_agent_efficiency").c_str(), 0.7, category.c_str(), "Energy efficiency when eating organisms.", 0.0, std::nullopt, {}, {"runtime", "food", speciesDomain.c_str()});

    addInt(registry, (p + "_hidden_layers").c_str(), defaults.hiddenLayers, category.c_str(), "Number of hidden neural layers.", 0.0, 5.0, {}, {"runtime", "neural", speciesDomain.c_str()});
    addInt(registry, (p + "_neurons_layer_1").c_str(), defaults.layer1, category.c_str(), "Neuron count for hidden layer 1.", 0.0, std::nullopt, {}, {"runtime", "neural", speciesDomain.c_str()});
    addInt(registry, (p + "_neurons_layer_2").c_str(), defaults.layer2, category.c_str(), "Neuron count for hidden layer 2.", 0.0, std::nullopt, {}, {"runtime", "neural", speciesDomain.c_str()});
    addInt(registry, (p + "_neurons_layer_3").c_str(), defaults.layer3, category.c_str(), "Neuron count for hidden layer 3.", 0.0, std::nullopt, {}, {"runtime", "neural", speciesDomain.c_str()});
    addInt(registry, (p + "_neurons_layer_4").c_str(), defaults.layer4, category.c_str(), "Neuron count for hidden layer 4.", 0.0, std::nullopt, {}, {"runtime", "neural", speciesDomain.c_str()});
    addInt(registry, (p + "_neurons_layer_5").c_str(), defaults.layer5, category.c_str(), "Neuron count for hidden layer 5.", 0.0, std::nullopt, {}, {"runtime", "neural", speciesDomain.c_str()});
    addDouble(registry, (p + "_mutation_rate").c_str(), 0.05, category.c_str(), "Base neural mutation rate.", 0.0, 1.0, {}, {"runtime", "neural", speciesDomain.c_str()});
    addDouble(registry, (p + "_mutation_strength").c_str(), 0.08, category.c_str(), "Base neural mutation strength.", 0.0, std::nullopt, {}, {"runtime", "neural", speciesDomain.c_str()});
    addInt(registry, (p + "_structural_jitter").c_str(), 0, category.c_str(), "Legacy structural jitter setting.", 0.0, std::nullopt, {}, {"runtime", "neural", speciesDomain.c_str()});
    addColor(registry, (p + "_color").c_str(), defaults.color, category.c_str(), "Default body color.", {}, {"ui", "appearance", speciesDomain.c_str()});

    (void)defaults.label;
}

void registerNeatParameters(ParameterRegistry& registry,
                            const char* prefix,
                            const double weightInitStd,
                            const double addConnectionRate,
                            const double addNodeRate,
                            const double toggleConnectionRate,
                            const double removeConnectionRate,
                            const double resetWeightRate,
                            const int maxHiddenNodes,
                            const int maxConnections,
                            const bool recurrent)
{
    const std::string p = prefix;
    const std::string category = std::string("neural.") + p;

    addString(registry, (p + "_initial_topology").c_str(), "minimal", category.c_str(), "Initial NEAT topology: minimal or layered.", {}, {"runtime", "neural"});
    addDouble(registry, (p + "_weight_init_std").c_str(), weightInitStd, category.c_str(), "Initial weight standard deviation.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, (p + "_weight_mutation_rate").c_str(), -1.0, category.c_str(), "Override weight mutation rate; -1 uses species base rate.", -1.0, 1.0, {}, {"runtime", "neural"});
    addDouble(registry, (p + "_weight_mutation_strength").c_str(), -1.0, category.c_str(), "Override weight mutation strength; -1 uses species base strength.", -1.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, (p + "_add_connection_rate").c_str(), addConnectionRate, category.c_str(), "Probability of adding a connection during mutation.", 0.0, 1.0, {}, {"runtime", "neural"});
    addDouble(registry, (p + "_add_node_rate").c_str(), addNodeRate, category.c_str(), "Probability of adding a node during mutation.", 0.0, 1.0, {}, {"runtime", "neural"});
    addDouble(registry, (p + "_toggle_connection_rate").c_str(), toggleConnectionRate, category.c_str(), "Probability of toggling a connection.", 0.0, 1.0, {}, {"runtime", "neural"});
    addDouble(registry, (p + "_remove_connection_rate").c_str(), removeConnectionRate, category.c_str(), "Probability of removing a connection.", 0.0, 1.0, {}, {"runtime", "neural"});
    addDouble(registry, (p + "_reset_weight_rate").c_str(), resetWeightRate, category.c_str(), "Probability of resetting a weight.", 0.0, 1.0, {}, {"runtime", "neural"});
    addInt(registry, (p + "_max_hidden_nodes").c_str(), maxHiddenNodes, category.c_str(), "Maximum hidden nodes allowed.", 1.0, std::nullopt, {}, {"runtime", "neural"});
    addInt(registry, (p + "_max_connections").c_str(), maxConnections, category.c_str(), "Maximum connections allowed.", 1.0, std::nullopt, {}, {"runtime", "neural"});

    if (recurrent)
    {
        addDouble(registry, (p + "_recurrent_connection_rate").c_str(), 0.12, category.c_str(), "Probability of adding a recurrent connection.", 0.0, 1.0, {}, {"runtime", "neural"});
        addDouble(registry, (p + "_memory_decay").c_str(), 0.85, category.c_str(), "Recurrent NEAT memory decay.", 0.0, 0.999, {}, {"runtime", "neural"});
        addDouble(registry, (p + "_state_clip").c_str(), 1.0, category.c_str(), "Recurrent NEAT state clamp.", 0.0, std::nullopt, {}, {"runtime", "neural"});
        addBool(registry, (p + "_reset_state_on_copy").c_str(), true, category.c_str(), "Reset recurrent state on offspring copy.", {}, {"runtime", "neural"});
    }
}
} // namespace

ParameterRegistry createDefaultParameterRegistry()
{
    ParameterRegistry registry;
    registerDefaultParameters(registry);
    return registry;
}

void registerDefaultParameters(ParameterRegistry& registry)
{
    // Phase 25.2: UI language. Category "appearance" routes it to the Appearance
    // preferences tab; the value is the locale tag consumed by i18n ("pt-br"/"en").
    // Stored like any other setting so it persists with the rest of the prefs.
    addString(registry, "ui_language", "pt-br", "appearance", "UI display language (pt-br or en).", {}, {"runtime", "ui"});
    // UI scale (acessibilidade): tamanho de fontes/menus/botoes. Default medio (um
    // pouco maior que o original "small", que ficava pequeno em telas menores).
    addString(registry, "ui_scale", "medium", "appearance", "UI text/widget size: small, medium or large.", {}, {"runtime", "ui"});
    // Visual theme (appearance skin): "none" = plain simulation, or a theme id such as
    // "orange". Chosen via the thumbnail picker in the Appearance tab.
    addString(registry, "ui_theme", "orange", "appearance", "Visual theme id: none, orange, dark_blue, light_blue.", {}, {"runtime", "ui"});
    // Phase 27: observability toggles (off by default; near-zero cost when off).
    addBool(registry, "profiler_enabled", false, "performance.observability", "Enable the per-system profiler.", {}, {"runtime", "performance", "ui"});
    addBool(registry, "metrics_enabled", false, "performance.observability", "Enable time-series metrics collection.", {}, {"runtime", "performance", "ui"});
    addInt(registry, "metrics_max_samples", 600, "performance.observability", "Maximum metrics samples kept (ring buffer).", 10.0, 100000.0, {}, {"runtime", "performance"});
    addInt(registry, "metrics_sample_interval", 1, "performance.observability", "Sample metrics every N steps.", 1.0, 1000.0, {}, {"runtime", "performance"});
    addString(registry, "log_level", "off", "performance.observability", "Log verbosity: off, error, warn, info, debug.", {}, {"runtime", "ui"});
    addDouble(registry, "time_scale", 1.0, "simulation.time", "Simulation time multiplier.", 0.1, std::nullopt, {}, {"runtime"});
    addInt(registry, "fps", 60, "render.timing", "Target render frames per second.", 1.0, 240.0, {}, {"runtime", "render"});
    addBool(registry, "paused", false, "simulation.time", "Pause simulation updates.", {}, {"runtime", "ui"});
    // Fase 32.1: timing de fisica vive na aba "Fisica" (categoria root = physics),
    // ao lado da colisao/fluidos — e o lever de performance vs velocidade (menos Hz
    // = menos passos por segundo simulado; dt sempre fixo, determinismo preservado).
    addInt(registry, "physics_steps_per_second", 30, "physics.timing", "Fixed physics steps per simulated second.", 1.0, 1000.0, {}, {"runtime", "physics"});
    addInt(registry, "max_physics_steps_per_frame", 8, "physics.timing", "Maximum fixed steps processed per rendered frame.", 1.0, std::nullopt, {}, {"runtime", "physics"});
    addDouble(registry, "max_physics_backlog_seconds", 0.25, "physics.timing", "Maximum accumulated simulation backlog.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    addBool(registry, "use_spatial", true, "performance.spatial", "Enable spatial hash acceleration.", {}, {"runtime", "performance"});
    // Phase 32: deterministic multithreading of perception + neural forward
    // (disjoint per-agent writes, no RNG -> bit-identical to serial).
    addBool(registry, "use_parallel_systems", true, "performance.parallel", "Multithread perception and neural forward (deterministic).", {}, {"runtime", "performance"});
    // Fase 35: pipeline em 2 threads (simulação numa thread, render/UI na principal).
    // Só afeta a janela interativa; selftests/golden/bench não passam por App.
    addBool(registry, "sim_render_threaded", true, "performance.parallel", "Run simulation on its own thread, overlapped with rendering (window only).", {}, {"runtime", "performance"});
    addString(registry, "substrate_shape", "rectangular", "world.substrate", "Substrate shape: rectangular or circular.", {}, {"runtime", "world"});
    addDouble(registry, "world_w", 1000.0, "world.substrate", "Rectangular substrate width.", 1.0, std::nullopt, {}, {"runtime", "world"});
    addDouble(registry, "world_h", 700.0, "world.substrate", "Rectangular substrate height.", 1.0, std::nullopt, {}, {"runtime", "world"});
    addDouble(registry, "substrate_radius", 400.0, "world.substrate", "Circular substrate radius.", 10.0, std::nullopt, {}, {"runtime", "world"});
    addInt(registry, "random_seed", -1, "simulation.random", "Fixed random seed; -1 disables fixed seeding.", -1.0, std::nullopt, {}, {"runtime", "debug"});
    addString(registry, "agent_template_name", "organismo_1", "genome.template", "Default agent/genome template name.", {"species_template_name"}, {"ui", "species"});
    addInt(registry, "max_deaths_per_step", 5, "simulation.lifecycle", "Maximum deaths processed per step.", 0.0, std::nullopt, {}, {"runtime"});
    // Microfase 32.4: OFF by default. The minimum population is maintained by
    // blocking deaths/predation at the floor + reproduction, never by spawning from
    // nothing. This knob only re-enables the legacy per-step respawn for opt-in use.
    addBool(registry, "population_min_rescue_enabled", false, "simulation.population", "Respawn agents to refill below-minimum populations (legacy; off by default).", {}, {"runtime", "species"});

    addInt(registry, "retina_skip", 0, "vision.global", "Frames skipped between retina updates.", 0.0, std::nullopt, {}, {"runtime", "vision"});
    addString(registry, "retina_vision_mode", "single", "vision.global", "Global retina algorithm: single, fullbody or sector.", {}, {"runtime", "vision"});
    addBool(registry, "render_enabled", true, "render", "Enable rendering.", {}, {"runtime", "render"});
    addBool(registry, "simple_render", false, "render", "Use simplified renderer.", {}, {"runtime", "render"});
    addBool(registry, "show_spatial_hash", false, "render.debug", "Show spatial hash overlay.", {}, {"ui", "debug", "render"});
    addDouble(registry, "render_resolution_scale", 1.0, "render", "Render resolution multiplier.", 1.0, 3.0, {}, {"runtime", "render"});
    addBool(registry, "use_numba_kernels", true, "performance.python_compat", "Legacy Python Numba kernel toggle.", {"use_native_kernels"}, {"performance"});
    addBool(registry, "use_numba_batch_retina", false, "performance.python_compat", "Legacy Python batch retina toggle.", {"use_native_batch_retina"}, {"performance", "vision"});
    addBool(registry, "use_grouped_vision_batches", true, "performance.vision", "Group agents by compatible vision signature.", {}, {"runtime", "performance", "vision"});
    addBool(registry, "use_persistent_perception_arrays", false, "performance.vision", "Use persistent perception buffers.", {}, {"runtime", "performance", "vision"});
    addBool(registry, "retina_high_scale_auto_sector", false, "performance.vision", "Switch to sector vision at high scale.", {}, {"runtime", "performance", "vision"});
    addInt(registry, "retina_high_scale_sector_min_agents", 800, "performance.vision", "Agent threshold for automatic sector vision.", 1.0, std::nullopt, {}, {"runtime", "performance", "vision"});
    addBool(registry, "retina_high_scale_global_sector", false, "performance.vision", "Experimental global sector vision.", {}, {"runtime", "performance", "vision"});
    addBool(registry, "reuse_spatial_grid", true, "performance.spatial", "Reuse spatial grid where possible.", {}, {"runtime", "performance"});
    addBool(registry, "use_numba_locomotion_energy", false, "performance.python_compat", "Legacy Python Numba locomotion/energy toggle.", {"use_native_locomotion_energy"}, {"performance", "physics"});
    addBool(registry, "brain_cache_disable", false, "performance.neural", "Disable grouped brain cache.", {}, {"runtime", "performance", "neural"});
    addInt(registry, "brain_cache_max_entries", 32, "performance.neural", "Maximum grouped brain cache entries.", 0.0, std::nullopt, {}, {"runtime", "performance", "neural"});
    addInt(registry, "brain_cache_max_mb", 512, "performance.neural", "Approximate brain cache memory cap in MB.", 0.0, std::nullopt, {}, {"runtime", "performance", "neural"});
    addBool(registry, "brain_cache_log", false, "performance.neural", "Log brain cache diagnostics.", {}, {"debug", "performance", "neural"});
    // Phase 14: canonical names; legacy Numba/native aliases preserved for compatibility.
    addBool(registry, "use_batch_forward", false, "performance.neural", "Enable batched brain forward path.", {"use_numba_brain_forward", "use_native_brain_forward"}, {"performance", "neural"});
    addInt(registry, "batch_forward_min_size", 256, "performance.neural", "Minimum batch size for batched brain forward.", 1.0, std::nullopt, {"numba_brain_forward_min_batch"}, {"runtime", "performance", "neural"});

    addString(registry, "retina_bins_mode", "nearest", "vision.bins", "Sector-bin aggregation mode.", {}, {"runtime", "vision"});
    addInt(registry, "retina_bins_distance_subdivisions", 5, "vision.bins", "Radial subdivisions used by sector vision.", 1.0, 99.0, {}, {"runtime", "vision"});
    addString(registry, "retina_bins_distance_distribution", "near_detail", "vision.bins", "Radial subdivision distribution.", {}, {"runtime", "vision"});
    addString(registry, "retina_bins_distance_falloff", "linear", "vision.bins", "Distance falloff function.", {}, {"runtime", "vision"});
    addString(registry, "retina_bins_projection", "center", "vision.bins", "Candidate projection mode.", {}, {"runtime", "vision"});
    addInt(registry, "retina_bins_candidate_limit", 128, "vision.bins", "Candidate limit per sector query; zero means unlimited.", 0.0, std::nullopt, {}, {"runtime", "vision"});
    addBool(registry, "retina_bins_obstacles_block_vision", false, "vision.bins", "Obstacles block sector vision.", {}, {"runtime", "vision"});

    addString(registry, "neural_network_type", "mlp", "neural.global", "Selected brain type.", {}, {"runtime", "neural"});
    addDouble(registry, "neural_gate_init", 1.0, "neural.gated_mlp", "Initial gate value.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_gate_min", 0.0, "neural.gated_mlp", "Minimum gate value.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_gate_max", 2.0, "neural.gated_mlp", "Maximum gate value.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_gate_mutation_rate", -1.0, "neural.gated_mlp", "Override gate mutation rate.", -1.0, 1.0, {}, {"runtime", "neural"});
    addDouble(registry, "neural_gate_mutation_strength", -1.0, "neural.gated_mlp", "Override gate mutation strength.", -1.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_shortcut_init_std", 0.05, "neural.shortcut_mlp", "Initial shortcut weight stddev.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_shortcut_scale", 0.25, "neural.shortcut_mlp", "Shortcut output scale.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_shortcut_mutation_rate", -1.0, "neural.shortcut_mlp", "Override shortcut mutation rate.", -1.0, 1.0, {}, {"runtime", "neural"});
    addDouble(registry, "neural_shortcut_mutation_strength", -1.0, "neural.shortcut_mlp", "Override shortcut mutation strength.", -1.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_rnn_recurrent_init_std", 0.08, "neural.simple_rnn", "Initial recurrent weight stddev.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_rnn_recurrent_scale", 0.35, "neural.simple_rnn", "Recurrent output scale.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addDouble(registry, "neural_rnn_memory_decay", 0.6, "neural.simple_rnn", "RNN memory decay.", 0.0, 0.999, {}, {"runtime", "neural"});
    addDouble(registry, "neural_rnn_state_clip", 1.0, "neural.simple_rnn", "RNN state clamp.", 0.0, std::nullopt, {}, {"runtime", "neural"});
    addBool(registry, "neural_rnn_reset_state_on_copy", true, "neural.simple_rnn", "Reset RNN state on offspring copy.", {}, {"runtime", "neural"});
    addDouble(registry, "neural_rnn_mutation_rate", -1.0, "neural.simple_rnn", "Override RNN mutation rate.", -1.0, 1.0, {}, {"runtime", "neural"});
    addDouble(registry, "neural_rnn_mutation_strength", -1.0, "neural.simple_rnn", "Override RNN mutation strength.", -1.0, std::nullopt, {}, {"runtime", "neural"});

    registerNeatParameters(registry, "neural_neat", 0.6, 0.08, 0.03, 0.01, 0.005, 0.02, 64, 512, false);
    registerNeatParameters(registry, "neural_proto_neat", 0.6, 0.04, 0.02, 0.0, 0.0, 0.03, 48, 384, false);
    registerNeatParameters(registry, "neural_recurrent_neat", 0.45, 0.08, 0.025, 0.01, 0.003, 0.02, 64, 640, true);

    addString(registry, "food_mode", "instant", "food", "Food mode: instant or chunk.", {"food_type"}, {"runtime", "food"});
    addString(registry, "food_chunk_mode", "fixed", "food",
              "Chunk replenish behaviour: 'fixed' refills the chunks in place forever; 'roaming' lets a chunk be eaten to nothing and a new one appears elsewhere.",
              {}, {"runtime", "food"});
    addInt(registry, "food_chunk_particles", 60, "food",
           "Roaming mode only: how many particles make up one whole crumb. New crumbs drop with this many particles each (irregular edges).",
           1.0, std::nullopt, {}, {"runtime", "food"});
    addInt(registry, "food_target", 50, "food", "Target food count.", 0.0, std::nullopt, {}, {"runtime", "food"});
    addDouble(registry, "food_min_r", 4.5, "food", "Minimum instant food radius.", 0.0, std::nullopt, {}, {"runtime", "food"});
    addDouble(registry, "food_max_r", 5.0, "food", "Maximum instant food radius.", 0.0, std::nullopt, {}, {"runtime", "food"});
    addDouble(registry, "food_replenish_interval", 0.1, "food", "Food replenishment interval in seconds.", 0.0, std::nullopt, {}, {"runtime", "food"});
    addDouble(registry, "food_bite_seconds", 6.0, "food.chunk", "Seconds required to consume a chunk particle.", 0.05, std::nullopt, {}, {"runtime", "food"});
    addDouble(registry, "food_piece_particle_radius", 5.0, "food.chunk", "Chunk food particle radius.", 0.1, std::nullopt, {}, {"runtime", "food"});
    addDouble(registry, "food_piece_cluster_radius", 36.0, "food.chunk", "Chunk food cluster radius.", 0.1, std::nullopt, {}, {"runtime", "food"});
    addBool(registry, "food_trim_excess_enabled", true, "food", "Trim excess food above target.", {}, {"runtime", "food"});
    addInt(registry, "food_trim_max_per_step", 5, "food", "Maximum food particles trimmed per step.", 0.0, std::nullopt, {}, {"runtime", "food"});

    // Microfase 32.1: population limits now have real defaults — min 5 (rescue
    // floor) and max 150 for the default label (0 previously meant "no limit",
    // so the per-label cap from 31.1 never engaged out of the box).
    registerSpeciesParameters(registry,
                              {"bacteria", "Bacteria", 150, 6.0, 12.0, 5, 150, 0.5, 8.0, 400.0, 9.0, 4, 20, 20, 20, 20, 0, true, false, true, false, {220, 220, 220}});
    registerSpeciesParameters(registry,
                              {"predator", "Predator", 0, 10.0, 18.0, 5, 100, 1.0, 15.0, 600.0, 14.0, 2, 16, 8, 0, 0, 0, true, true, false, true, {80, 120, 220}});
    addBool(registry, "predators_enabled", false, "species.predator", "Enable legacy predators.", {"predator_enabled"}, {"runtime", "species.predator"});

    addDouble(registry, "agents_inertia", 1.0, "physics", "Global agent inertia.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    addBool(registry, "smooth_locomotion_enabled", false, "physics.smooth_locomotion", "Enable smooth locomotion dynamics.", {}, {"runtime", "physics"});
    addBool(registry, "smooth_linear_inertia_enabled", true, "physics.smooth_locomotion", "Enable linear inertia.", {}, {"runtime", "physics"});
    addDouble(registry, "smooth_max_linear_accel", 900.0, "physics.smooth_locomotion", "Maximum linear acceleration.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    addBool(registry, "smooth_linear_drag_enabled", true, "physics.smooth_locomotion", "Enable linear drag.", {}, {"runtime", "physics"});
    addDouble(registry, "smooth_linear_drag", 0.75, "physics.smooth_locomotion", "Linear drag coefficient.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    addBool(registry, "smooth_angular_inertia_enabled", true, "physics.smooth_locomotion", "Enable angular inertia.", {}, {"runtime", "physics"});
    addDouble(registry, "smooth_max_angular_accel", 12.566370614359172, "physics.smooth_locomotion", "Maximum angular acceleration.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    addBool(registry, "smooth_angular_drag_enabled", true, "physics.smooth_locomotion", "Enable angular drag.", {}, {"runtime", "physics"});
    addDouble(registry, "smooth_angular_drag", 1.5, "physics.smooth_locomotion", "Angular drag coefficient.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    // Fase 32.1: interpolacao de render na aba "Fisica" (junto do timing): suaviza
    // o movimento entre passos de fisica, util quando se baixa o physics_steps_per_second.
    addBool(registry, "render_interpolation_enabled", false, "physics.timing", "Interpolate render poses between physics steps.", {}, {"runtime", "render"});
    addBool(registry, "camera_follow_smoothing_enabled", true, "render.camera", "Enable smooth selected-agent camera follow.", {}, {"ui", "render"});
    addDouble(registry, "camera_follow_smoothing", 10.0, "render.camera", "Camera follow smoothing strength.", 0.0, std::nullopt, {}, {"ui", "render"});
    addBool(registry, "agent_collision_enabled", true, "physics.collision", "Enable organism-organism collision.", {}, {"runtime", "physics"});
    addBool(registry, "agent_collision_elasticity_enabled", true, "physics.collision", "Enable elastic collision response.", {}, {"runtime", "physics"});
    addDouble(registry, "agent_collision_restitution", 0.12, "physics.collision", "Collision restitution.", 0.0, 1.0, {}, {"runtime", "physics"});
    addDouble(registry, "agent_collision_velocity_transfer", 0.35, "physics.collision", "Velocity transferred during collision.", 0.0, 1.0, {}, {"runtime", "physics"});
    addDouble(registry, "agent_collision_separation", 0.9, "physics.collision", "Collision separation factor.", 0.0, 1.0, {}, {"runtime", "physics"});
    addDouble(registry, "agent_collision_max_impulse", 900.0, "physics.collision", "Maximum collision impulse.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    addBool(registry, "global_viscosity_enabled", false, "physics.fluid", "Enable global viscosity.", {}, {"runtime", "physics"});
    addDouble(registry, "global_viscosity_drag", 0.2, "physics.fluid", "Global viscosity drag.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    addBool(registry, "movable_chunk_food_enabled", false, "physics.food", "Allow chunk food particles to move.", {}, {"runtime", "physics", "food"});
    addBool(registry, "chunk_food_collision_enabled", true, "physics.food", "Enable food-food collision for chunk particles.", {}, {"runtime", "physics", "food"});
    addBool(registry, "chunk_food_adhesion_enabled", true, "physics.food", "Enable food chunk adhesion.", {}, {"runtime", "physics", "food"});
    addDouble(registry, "chunk_food_adhesion_strength", 0.35, "physics.food", "Food chunk adhesion strength.", 0.0, std::nullopt, {}, {"runtime", "physics", "food"});
    addDouble(registry, "chunk_food_mass_scale", 1.0, "physics.food", "Food particle mass scale.", 0.0, std::nullopt, {}, {"runtime", "physics", "food"});
    addDouble(registry, "chunk_food_drag", 1.6, "physics.food", "Food particle drag.", 0.0, std::nullopt, {}, {"runtime", "physics", "food"});
    addDouble(registry, "chunk_food_push_strength", 0.45, "physics.food", "Agent push strength on food particles.", 0.0, std::nullopt, {}, {"runtime", "physics", "food"});
    addBool(registry, "brownian_motion_enabled", false, "physics.fluid", "Enable light brownian motion.", {}, {"runtime", "physics"});
    addDouble(registry, "brownian_motion_strength", 3.0, "physics.fluid", "Brownian motion strength.", 0.0, std::nullopt, {}, {"runtime", "physics"});
    addBool(registry, "allow_reverse_locomotion", false, "physics.legacy", "Legacy global reverse locomotion toggle.", {}, {"runtime", "physics"});
    addDouble(registry, "reproduction_min_age", 0.0, "simulation.legacy", "Legacy global reproduction minimum age.", 0.0, std::nullopt, {}, {"runtime"});
    addDouble(registry, "reproduction_cooldown", 0.0, "simulation.legacy", "Legacy global reproduction cooldown.", 0.0, std::nullopt, {}, {"runtime"});

    addBool(registry, "show_selected_details", true, "ui.agent_inspector", "Show selected agent details.", {}, {"ui"});
    addBool(registry, "show_multi_selected_vision", false, "ui.vision", "Show vision overlays for multi-selection.", {}, {"ui", "vision"});
    addBool(registry, "show_metrics_chart", false, "ui.metrics", "Show metrics chart.", {}, {"ui"});
    addString(registry, "neural_view_dense_layout", "fixed", "ui.neural_viewer", "Neural viewer dense layout mode.", {}, {"ui", "neural"});
    addBool(registry, "camera_follow_selected_agent", true, "ui.camera", "Follow selected agent with camera.", {}, {"ui", "render"});
    addInt(registry, "metrics_chart_sample_seconds", 5, "ui.metrics", "Metrics chart sampling interval in seconds.", 1.0, std::nullopt, {}, {"ui"});
    addBool(registry, "debug_tracebacks", false, "debug", "Enable verbose debug tracebacks.", {}, {"debug"});
    addDouble(registry, "diagnostic_heartbeat_minutes", 1.0, "debug", "Diagnostic heartbeat interval in minutes.", 0.0, std::nullopt, {}, {"debug"});
    addBool(registry, "save_recovery_on_close", true, "save", "Save recovery snapshot on close.", {}, {"runtime", "save"});
    addBool(registry, "auto_export_substrate", true, "save.autosave", "Autosave enabled; legacy internal name.", {"autosave_enabled"}, {"runtime", "save"});
    addDouble(registry, "auto_export_interval_minutes", 30.0, "save.autosave", "Autosave interval in minutes.", 0.0, std::nullopt, {"autosave_interval_minutes"}, {"runtime", "save"});
    addBool(registry, "export_substrate_include_brain_activations", false, "save.export", "Include brain activations in exports.", {}, {"runtime", "save", "neural"});
    addBool(registry, "export_substrate_pretty_json", false, "save.export", "Write human-readable JSON.", {}, {"runtime", "save"});
    addBool(registry, "debug_reproduction_color", false, "debug", "Debug reproduction with color changes.", {}, {"debug"});
    addColor(registry, "substrate_bg_color", {10, 10, 20}, "appearance", "Legacy substrate background color.", {"substrate_background_color"}, {"ui", "appearance"});
    addBool(registry, "background_gradient_enabled", false, "appearance.background", "Enable background gradient.", {}, {"ui", "appearance"});
    addColor(registry, "background_color_top", {10, 10, 20}, "appearance.background", "Background top color.", {}, {"ui", "appearance"});
    addColor(registry, "background_color_bottom", {10, 10, 20}, "appearance.background", "Background bottom color.", {}, {"ui", "appearance"});
    addBool(registry, "substrate_gradient_enabled", false, "appearance.substrate", "Enable substrate gradient.", {}, {"ui", "appearance"});
    addColor(registry, "substrate_color_top", {10, 10, 20}, "appearance.substrate", "Substrate top color.", {}, {"ui", "appearance"});
    addColor(registry, "substrate_color_bottom", {10, 10, 20}, "appearance.substrate", "Substrate bottom color.", {}, {"ui", "appearance"});
    addBool(registry, "substrate_border_enabled", true, "appearance.substrate", "Show substrate border.", {}, {"ui", "appearance"});
    addColor(registry, "substrate_border_color", {40, 200, 40}, "appearance.substrate", "Substrate border color.", {}, {"ui", "appearance"});
    addColor(registry, "food_color", {220, 30, 30}, "appearance.food", "Default food color.", {}, {"ui", "appearance", "food"});

    addBool(registry, "disable_brain_activations", false, "debug.neural", "Disable expensive brain activation details.", {}, {"debug", "neural"});
    addBool(registry, "mem_diag_enable", false, "debug.memory", "Enable memory diagnostics.", {}, {"debug"});
    addDouble(registry, "mem_diag_interval", 60.0, "debug.memory", "Memory diagnostics interval in seconds.", 0.0, std::nullopt, {}, {"debug"});
    addDouble(registry, "mem_warn_mb", 1024.0, "debug.memory", "Memory warning threshold in MB.", 0.0, std::nullopt, {}, {"debug"});
}
} // namespace agentbiosim::config
