#include "sim/SimulationRunner.hpp"

#include "config/ParameterHelpers.hpp"
#include "neural/BrainFactory.hpp"
#include "simulation/SpeciesBootstrap.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace agentbiosim::sim
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

simulation::Vec2 randomPointInsideWorld(const simulation::World& world,
                                          const double radius,
                                          std::mt19937& rng)
{
    if (world.shape() == simulation::WorldShape::Circular)
    {
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        const double angle = unit(rng) * 2.0 * kPi;
        const double r = std::sqrt(unit(rng)) * std::max(0.0, world.radius() - radius);
        const auto c = world.center();
        return {c.x + std::cos(angle) * r, c.y + std::sin(angle) * r};
    }
    std::uniform_real_distribution<double> ux(radius, std::max(radius, world.width() - radius));
    std::uniform_real_distribution<double> uy(radius, std::max(radius, world.height() - radius));
    return {ux(rng), uy(rng)};
}

bool pointInPolygon(const simulation::Vec2 p, const std::vector<simulation::Vec2>& poly)
{
    bool inside = false;
    const std::size_t n = poly.size();
    if (n < 3U) return false;
    for (std::size_t i = 0, j = n - 1U; i < n; j = i++)
    {
        const auto& a = poly[i];
        const auto& b = poly[j];
        if (((a.y > p.y) != (b.y > p.y)) &&
            (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y + 1.0e-12) + a.x))
        {
            inside = !inside;
        }
    }
    return inside;
}
} // namespace

SimulationRunner::SimulationRunner(const config::ParameterRegistry& parameters)
    : parameters_(parameters)
{
}

void SimulationRunner::initialize()
{
    using config::parameterBool;
    using config::parameterDouble;
    using config::parameterInt;
    using config::parameterString;

    const std::string shape = parameterString(parameters_, "substrate_shape", "rectangular");
    simulation::WorldConfig wcfg;
    wcfg.width = parameterDouble(parameters_, "world_w", 1000.0);
    wcfg.height = parameterDouble(parameters_, "world_h", 700.0);
    wcfg.radius = parameterDouble(parameters_, "substrate_radius", 400.0);
    wcfg.center = {wcfg.width * 0.5, wcfg.height * 0.5};
    wcfg.shape = shape == "circular" ? simulation::WorldShape::Circular
                                      : simulation::WorldShape::Rectangular;
    world_.configure(wcfg);

    const int seedParam = parameterInt(parameters_, "random_seed", -1);
    seed_ = seedParam >= 0 ? static_cast<std::uint64_t>(seedParam) : 1337ULL;
    showSpatialHash_ = parameterBool(parameters_, "show_spatial_hash", false);
    simpleRender_ = parameterBool(parameters_, "simple_render", false);
    timeScale_ = parameterDouble(parameters_, "time_scale", 1.0);

    foodSystem_.reseed(seed_);
    spawnInitial();
    rebuildSpatial();
}

void SimulationRunner::spawnInitial()
{
    using config::parameterBool;
    using config::parameterColor;
    using config::parameterDouble;
    using config::parameterInt;

    agents_.clear();
    foods_.clear();
    genomes_.clear();
    species_.clear();

    std::mt19937 rng(static_cast<std::uint32_t>(seed_));

    constexpr std::size_t kBootstrapInputSize = 4;
    constexpr std::size_t kBootstrapOutputSize = 2;
    const auto def = simulation::bootstrapDefaultSpecies(
        species_, genomes_, parameters_, kBootstrapInputSize, kBootstrapOutputSize);

    const auto* bacteria = species_.find(def.bacteria.speciesId);
    if (bacteria == nullptr) return;

    const auto* obstaclePtr = obstacles_.empty() ? nullptr : &obstacles_;
    auto sampleFree = [&](const double r) {
        constexpr int kMax = 32;
        for (int t = 0; t < kMax; ++t)
        {
            const auto candidate = world_.clampPosition(
                randomPointInsideWorld(world_, r, rng), r);
            if (obstaclePtr == nullptr || !obstaclePtr->overlapsCircle(candidate, r))
            {
                return candidate;
            }
        }
        return world_.clampPosition(world_.center(), r);
    };

    const int bacteriaCount = bacteria->initialCount;
    const double bacteriaRadius =
        std::max(0.1, parameterDouble(parameters_, "bacteria_body_size", 9.0));
    const double bacteriaEnergy =
        std::max(0.0, parameterDouble(parameters_, "bacteria_initial_energy", 100.0));
    std::uniform_real_distribution<double> angleDist(0.0, 2.0 * kPi);
    for (int i = 0; i < bacteriaCount; ++i)
    {
        simulation::AgentSpawn s;
        s.position = sampleFree(bacteriaRadius);
        s.angle = angleDist(rng);
        s.radius = bacteriaRadius;
        s.energy = bacteriaEnergy;
        s.color = bacteria->color;
        s.speciesId = bacteria->id;
        s.genomeId = bacteria->defaultGenomeId;
        s.typeCode = bacteria->typeCode;
        s.bodyShape = bacteria->bodyShape;
        static_cast<void>(agents_.createAgent(s));
    }

    const auto* predator = species_.find(def.predator.speciesId);
    if (predator != nullptr && predator->enabled && predator->initialCount > 0)
    {
        const double pr = std::max(0.1, parameterDouble(parameters_, "predator_body_size", 14.0));
        const double pe = std::max(0.0, parameterDouble(parameters_, "predator_initial_energy", 100.0));
        for (int i = 0; i < predator->initialCount; ++i)
        {
            simulation::AgentSpawn s;
            s.position = sampleFree(pr);
            s.angle = angleDist(rng);
            s.radius = pr;
            s.energy = pe;
            s.color = predator->color;
            s.speciesId = predator->id;
            s.genomeId = predator->defaultGenomeId;
            s.typeCode = predator->typeCode;
            s.bodyShape = predator->bodyShape;
            static_cast<void>(agents_.createAgent(s));
        }
    }

    // Phase 19: initial food spawn via FoodSystem.
    const systems::FoodSystemConfig foodCfg = systems::FoodSystem::fromRegistry(parameters_);
    if (foodCfg.mode == simulation::FoodKind::Instant)
    {
        for (int i = 0; i < foodCfg.target && static_cast<int>(foods_.size()) < foodCfg.target; ++i)
        {
            static_cast<void>(foodSystem_.spawnInstant(foods_, world_, foodCfg, obstaclePtr));
        }
    }
    else
    {
        while (static_cast<int>(foods_.size()) < foodCfg.target)
        {
            const std::size_t before = foods_.size();
            static_cast<void>(foodSystem_.spawnCluster(foods_, world_, foodCfg, obstaclePtr));
            if (foods_.size() == before) break;
        }
    }
}

void SimulationRunner::rebuildSpatial()
{
    const double foodMaxRadius = config::parameterDouble(parameters_, "food_max_r", 5.0);
    const double bacteriaMaxRadius = config::parameterDouble(parameters_, "bacteria_max_r", 12.0);
    const double predatorMaxRadius = config::parameterDouble(parameters_, "predator_max_r", 18.0);
    const double cellSize = std::max({foodMaxRadius, bacteriaMaxRadius, predatorMaxRadius, 1.0}) * 2.0;
    spatialHash_.configure(simulation::spatialConfigForWorld(world_, cellSize));
    spatialHash_.rebuild(agents_, foods_);
}

void SimulationRunner::runOneStep(const double dt)
{
    const auto perceptionConfig = perception::PerceptionSystem::fromRegistry(parameters_, "bacteria");
    const simulation::ObstacleStore* obstaclePtr = obstacles_.empty() ? nullptr : &obstacles_;
    perception::PerceptionDebugRequest debugRequest;
    const auto perceptionResult = perceptionSystem_.computeInputs(
        agents_, foods_, &spatialHash_, world_, perceptionConfig, debugRequest, obstaclePtr);

    const systems::MovementConfig movementConfig = systems::MovementSystem::fromRegistry(parameters_);
    const systems::NeuralSystemConfig neuralConfig = systems::NeuralSystem::fromRegistry(
        parameters_, movementConfig, perceptionResult.inputSize);
    const std::vector<systems::MovementControl> neuralControls =
        neuralSystem_.produceMovementControls(agents_, world_, neuralConfig, &perceptionResult);
    static_cast<void>(movementSystem_.apply(agents_, world_, dt, movementConfig, &neuralControls,
                                              obstaclePtr));

    systems::CollisionConfig collisionConfig = systems::CollisionSystem::fromRegistry(parameters_);
    collisionConfig.dt = dt;
    static_cast<void>(collisionSystem_.apply(agents_, foods_, world_, &spatialHash_, obstaclePtr,
                                               collisionConfig));

    const systems::EnergyConfig energyConfig = systems::EnergySystem::fromRegistry(parameters_);
    static_cast<void>(energySystem_.apply(agents_, dt, energyConfig));

    rebuildSpatial();

    systems::DietInteractionConfig dietCfg =
        systems::InteractionSystem::dietConfigFromRegistry(parameters_);
    dietCfg.dt = dt;
    const auto interStats = interactionSystem_.applyWithDiet(agents_, foods_, genomes_, &spatialHash_, dietCfg);
    stats_.foodEaten += interStats.foodsConsumed + interStats.chunkParticlesDepleted;

    const systems::FoodSystemConfig foodCfg = systems::FoodSystem::fromRegistry(parameters_);
    static_cast<void>(foodSystem_.replenishToTarget(foods_, world_, foodCfg, obstaclePtr));
    static_cast<void>(foodSystem_.trimExcess(foods_, foodCfg));

    const systems::ReproductionConfig reproductionConfig =
        systems::ReproductionSystem::fromRegistry(parameters_, "bacteria", neuralConfig.brainConfig);
    const auto reproStats = reproductionSystem_.apply(
        agents_, genomes_, neuralSystem_, world_,
        neuralConfig.brainConfig, reproductionConfig, dt);
    stats_.births += reproStats.birthsThisStep;

    const systems::DeathConfig deathConfig = systems::DeathSystem::fromRegistry(parameters_);
    const auto deathStats = deathSystem_.apply(agents_, deathConfig);
    stats_.deaths += deathStats.deaths;

    rebuildSpatial();
}

void SimulationRunner::step(const double dt)
{
    if (paused_ && !stepOnce_)
    {
        return;
    }
    const double scaled = std::max(0.0, dt) * std::max(0.0, timeScale_);
    runOneStep(scaled);
    ++stats_.stepsExecuted;
    if (stepOnce_) stepOnce_ = false;
}

void SimulationRunner::reset()
{
    // Phase 24.1 fix: re-read the world config from the registry so that
    // operations like Aplicar ambiente (which writes substrate_shape /
    // world_w / world_h / substrate_radius to the registry and then calls
    // reset()) actually rebuild the world geometry. Before this fix reset()
    // only respawned agents into the OLD world.
    using config::parameterDouble;
    using config::parameterString;
    const std::string shape = parameterString(parameters_, "substrate_shape", "rectangular");
    simulation::WorldConfig wcfg;
    wcfg.width = parameterDouble(parameters_, "world_w", 1000.0);
    wcfg.height = parameterDouble(parameters_, "world_h", 700.0);
    wcfg.radius = parameterDouble(parameters_, "substrate_radius", 400.0);
    wcfg.center = {wcfg.width * 0.5, wcfg.height * 0.5};
    wcfg.shape = shape == "circular" ? simulation::WorldShape::Circular
                                      : simulation::WorldShape::Rectangular;
    world_.configure(wcfg);
    // spawnInitial() will respawn agents/foods. Obstacles are kept across
    // a plain reset() (R key) to match the Phase 22.1 behavior; the Phase 24
    // "Limpar comida" button + an obstacle Clear command remain the way to
    // wipe them explicitly.
    stats_ = {};
    paused_ = false;
    stepOnce_ = false;
    spawnInitial();
    rebuildSpatial();
}

simulation::EntityId SimulationRunner::pickAgentAt(const simulation::Vec2 worldPoint,
                                                    const double pickRadius) const
{
    double bestD2 = pickRadius * pickRadius;
    simulation::EntityId bestId{0};
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (!agents_.aliveAt(i)) continue;
        const auto p = agents_.positionAt(i);
        const double dx = p.x - worldPoint.x;
        const double dy = p.y - worldPoint.y;
        const double d2 = dx * dx + dy * dy;
        // Include the agent's body radius in the pick distance.
        const double r = agents_.radiusAt(i);
        const double reach = (pickRadius + r) * (pickRadius + r);
        if (d2 <= reach && d2 < bestD2 + r * r)
        {
            bestD2 = d2;
            bestId = agents_.idAt(i);
        }
    }
    return bestId;
}

std::size_t SimulationRunner::agentsInRect(const simulation::Vec2 a, const simulation::Vec2 b,
                                             std::vector<simulation::EntityId>& out) const
{
    const double minX = std::min(a.x, b.x);
    const double maxX = std::max(a.x, b.x);
    const double minY = std::min(a.y, b.y);
    const double maxY = std::max(a.y, b.y);
    out.clear();
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (!agents_.aliveAt(i)) continue;
        const auto p = agents_.positionAt(i);
        if (p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY)
        {
            out.push_back(agents_.idAt(i));
        }
    }
    return out.size();
}

std::size_t SimulationRunner::agentsInLasso(const std::vector<simulation::Vec2>& polygon,
                                              std::vector<simulation::EntityId>& out) const
{
    out.clear();
    if (polygon.size() < 3U) return 0U;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (!agents_.aliveAt(i)) continue;
        if (pointInPolygon(agents_.positionAt(i), polygon))
        {
            out.push_back(agents_.idAt(i));
        }
    }
    return out.size();
}

simulation::EntityId SimulationRunner::spawnAgentDefaultAt(const simulation::Vec2 worldPos)
{
    using config::parameterDouble;
    const auto* species = species_.empty()
        ? nullptr
        : species_.find(species_.idByName("bacteria"));
    if (species == nullptr) return {0};
    const double radius = std::max(0.1, parameterDouble(parameters_, "bacteria_body_size", 9.0));
    const double energy = std::max(0.0, parameterDouble(parameters_, "bacteria_initial_energy", 100.0));
    auto pos = world_.clampPosition(worldPos, radius);
    const auto* obs = obstacles_.empty() ? nullptr : &obstacles_;
    if (obs != nullptr && obs->overlapsCircle(pos, radius)) return {0};
    simulation::AgentSpawn s;
    s.position = pos;
    s.radius = radius;
    s.energy = energy;
    s.color = species->color;
    s.speciesId = species->id;
    s.genomeId = species->defaultGenomeId;
    s.typeCode = species->typeCode;
    s.bodyShape = species->bodyShape;
    return agents_.createAgent(s);
}

simulation::EntityId SimulationRunner::spawnFoodAt(const simulation::Vec2 worldPos,
                                                     const double radius,
                                                     const double energy)
{
    auto pos = world_.clampPosition(worldPos, radius);
    const auto* obs = obstacles_.empty() ? nullptr : &obstacles_;
    if (obs != nullptr && obs->overlapsCircle(pos, radius)) return {0};
    simulation::FoodSpawn s;
    s.position = pos;
    s.radius = radius;
    s.energy = energy;
    s.initialEnergy = energy;
    s.kind = simulation::FoodKind::Instant;
    return foods_.createFood(s);
}

void SimulationRunner::deleteAgents(const std::vector<simulation::EntityId>& ids)
{
    for (const auto id : ids)
    {
        static_cast<void>(agents_.removeAgent(id));
    }
}

bool SimulationRunner::applyCommand(const ui::Command& cmd)
{
    return std::visit([&](auto&& c) -> bool {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, ui::CmdPauseToggle>)        { togglePaused(); return true; }
        else if constexpr (std::is_same_v<T, ui::CmdSetPaused>)     { setPaused(c.paused); return true; }
        else if constexpr (std::is_same_v<T, ui::CmdResetSimulation>) { reset(); return true; }
        else if constexpr (std::is_same_v<T, ui::CmdStepOnce>)      { requestStepOnce(); return true; }
        else if constexpr (std::is_same_v<T, ui::CmdSetTimeScale>)  { timeScale_ = std::max(0.0, c.timeScale); return true; }
        else if constexpr (std::is_same_v<T, ui::CmdFitWorldCamera>) { return true; /* handled by AppController */ }
        else if constexpr (std::is_same_v<T, ui::CmdSetCameraCenter>) { return true; /* handled by AppController */ }
        else if constexpr (std::is_same_v<T, ui::CmdPanCameraScreen>) { return true; /* handled by AppController */ }
        else if constexpr (std::is_same_v<T, ui::CmdZoomCameraAt>)   { return true; /* handled by AppController */ }
        else if constexpr (std::is_same_v<T, ui::CmdSetCanvasTool>)  { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, ui::CmdSelectAtWorldPoint>) { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, ui::CmdSelectRect>)     { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, ui::CmdSelectLasso>)    { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, ui::CmdClearSelection>) { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, ui::CmdDeleteSelected>) { return true; /* handled by UI with help of runner */ }
        else if constexpr (std::is_same_v<T, ui::CmdMoveSelectedBy>) { return true; /* handled by UI with help of runner */ }
        else if constexpr (std::is_same_v<T, ui::CmdSpawnFoodAt>)
        {
            static_cast<void>(spawnFoodAt(c.world, c.radius, c.energy)); return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdSpawnAgentAt>)
        {
            static_cast<void>(spawnAgentDefaultAt(c.world)); return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdPaintObstacleAt>)
        {
            static_cast<void>(obstacles_.paint(c.world, c.brushRadius)); return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdEraseObstacleAt>)
        {
            static_cast<void>(obstacles_.eraseAt(c.world, c.eraseRadius)); return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdPaintObstacleStroke>)
        {
            // Phase 22.1: interpolate stamps along the segment so brush draws a
            // continuous trail. Spacing = brushRadius * kBrushSpacingFactor.
            const double spacing = std::max(1.0, c.brushRadius * 0.6);
            const double dx = c.worldTo.x - c.worldFrom.x;
            const double dy = c.worldTo.y - c.worldFrom.y;
            const double dist = std::hypot(dx, dy);
            const std::size_t stamps =
                dist <= spacing ? 1U
                                : static_cast<std::size_t>(std::ceil(dist / spacing));
            for (std::size_t i = 1; i <= stamps; ++i)
            {
                const double t = static_cast<double>(i) / static_cast<double>(stamps);
                const simulation::Vec2 p{c.worldFrom.x + dx * t, c.worldFrom.y + dy * t};
                static_cast<void>(obstacles_.paint(p, c.brushRadius));
            }
            return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdEraseObstacleStroke>)
        {
            const double spacing = std::max(1.0, c.eraseRadius * 0.6);
            const double dx = c.worldTo.x - c.worldFrom.x;
            const double dy = c.worldTo.y - c.worldFrom.y;
            const double dist = std::hypot(dx, dy);
            const std::size_t stamps =
                dist <= spacing ? 1U
                                : static_cast<std::size_t>(std::ceil(dist / spacing));
            for (std::size_t i = 1; i <= stamps; ++i)
            {
                const double t = static_cast<double>(i) / static_cast<double>(stamps);
                const simulation::Vec2 p{c.worldFrom.x + dx * t, c.worldFrom.y + dy * t};
                static_cast<void>(obstacles_.eraseAt(p, c.eraseRadius));
            }
            return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdClearObstacles>) { obstacles_.clear(); return true; }
        else if constexpr (std::is_same_v<T, ui::CmdClearFood>) {
            static_cast<void>(foodSystem_.clearAll(foods_)); return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdToggleSpatialHashOverlay>) {
            showSpatialHash_ = !showSpatialHash_; return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdToggleSelectionOverlay>) { return true; /* UI flag */ }
        else if constexpr (std::is_same_v<T, ui::CmdToggleToolOverlay>) { return true; /* UI flag */ }
        else if constexpr (std::is_same_v<T, ui::CmdToggleHelpPanel>) { return true; /* UI flag */ }
        else if constexpr (std::is_same_v<T, ui::CmdToggleSimpleRender>) {
            simpleRender_ = !simpleRender_; return true;
        }
        else if constexpr (std::is_same_v<T, ui::CmdToggleVisionDebug>) { return true; /* UI flag */ }
        // Phase 22.1 hotfix: new commands. NewSimulation is just an alias for
        // reset; the rest are UI-only (panel toggles, camera reset, quit) and
        // are handled by AppController in drainCommandsAndApply().
        else if constexpr (std::is_same_v<T, ui::CmdNewSimulation>) { reset(); return true; }
        else if constexpr (std::is_same_v<T, ui::CmdQuitApp>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdTogglePreferencesPanel>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdToggleAboutPanel>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdToggleGenomePanel>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdResetCamera>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCloseAllMenus>) { return true; }
        // Phase 23: preferences commands are UI-only from the runner POV.
        else if constexpr (std::is_same_v<T, ui::CmdOpenPreferences>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdClosePreferences>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdSetPreferencesTab>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdSetPreferencesSearch>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdSetParameterValue>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdApplyPreferences>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdRevertPreferences>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdRestoreDefaultsPreferences>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdRestoreParameterDefault>) { return true; }
        // Phase 23.1: multi-window prefs + popups + velocity widget.
        else if constexpr (std::is_same_v<T, ui::CmdOpenPreferencesWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdClosePreferencesWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdScrollPreferencesWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdOpenHelpWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCloseHelpWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdOpenSubstratePlaceholder>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCloseSubstratePlaceholder>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdOpenPrefsPopup>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdClosePrefsPopup>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdAdjustTimeScale>) { return true; }
        // Phase 23.2: draggable windows + text editor + restore-and-apply.
        else if constexpr (std::is_same_v<T, ui::CmdMovePreferencesWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdMoveHelpWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdBeginEditParameter>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCancelEditParameter>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCommitEditParameter>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdRestoreDefaultsAndApply>) { return true; }
        // Phase 24: operational windows + apply commands. Most are UI-only;
        // CmdClearFood maps to foodSystem clearAll.
        else if constexpr (std::is_same_v<T, ui::CmdOpenEditorGenetico>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCloseEditorGenetico>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdOpenEspecies>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCloseEspecies>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdOpenPopulacao>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdClosePopulacao>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdOpenSubstrato>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCloseSubstrato>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdScrollOperationalWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdMoveOperationalWindow>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdApplyGenomeToSelected>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdApplyGenomeToSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdResetNeuralForSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdSelectAllOfSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdAssignSelectedToSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdCreateSpeciesFromSelected>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdApplyPopulation>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdApplyEnvironment>) { return true; }
        else if constexpr (std::is_same_v<T, ui::CmdClearAllFood>)
        {
            static_cast<void>(foodSystem_.clearAll(foods_)); return true;
        }
        else { return false; }
    }, cmd);
}
} // namespace agentbiosim::sim
