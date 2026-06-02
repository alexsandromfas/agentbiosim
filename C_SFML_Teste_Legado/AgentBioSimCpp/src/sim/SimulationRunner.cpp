#include "sim/SimulationRunner.hpp"

#include "config/ParameterHelpers.hpp"
#include "neural/BrainFactory.hpp"
#include "simulation/SpeciesBootstrap.hpp"

#include <algorithm>
#include <array>
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

// Phase 24.2: Labels-tab species operations. -----------------------------------

namespace
{
// Palette mirrors the Python label palette in engine.create_agent_label.
constexpr std::array<simulation::ColorRgb, 6> kLabelPalette{{
    {240, 94, 94}, {90, 170, 255}, {135, 220, 130},
    {245, 195, 75}, {180, 130, 255}, {255, 140, 85}
}};
} // namespace

void SimulationRunner::assignSelectedToSpecies(const std::vector<simulation::EntityId>& ids,
                                                 const simulation::SpeciesId speciesId)
{
    const auto* rec = species_.find(speciesId);
    if (rec == nullptr) return;
    const simulation::ColorRgb color = rec->color;
    const simulation::GenomeId genome = rec->defaultGenomeId;
    for (const auto id : ids)
    {
        const auto idx = agents_.indexOf(id);
        if (!idx.has_value()) continue;
        agents_.setSpeciesIdAt(*idx, speciesId);
        agents_.setColorAt(*idx, color);
        if (genome != simulation::kInvalidGenomeId)
        {
            agents_.setGenomeIdAt(*idx, genome);
        }
    }
}

void SimulationRunner::removeSelectedFromSpecies(const std::vector<simulation::EntityId>& ids)
{
    // Python reassigns orphaned agents to the default label. Here the default
    // is the bacteria species; if it is missing we leave the agent as-is.
    const auto* bacteria = species_.findByName("bacteria");
    if (bacteria == nullptr) return;
    assignSelectedToSpecies(ids, bacteria->id);
}

simulation::SpeciesId SimulationRunner::createSpeciesFromSelected(
    const std::string& label, const std::vector<simulation::EntityId>& ids)
{
    // The new species shares the bacteria template (prefix/genome/type/body),
    // matching Python where all labels share one genome template and only differ
    // by color + population limits. The color cycles through the label palette.
    const auto* base = species_.findByName("bacteria");
    simulation::SpeciesRecord rec;
    if (base != nullptr) rec = *base;
    rec.id = simulation::kInvalidSpeciesId;  // registerSpecies assigns a fresh id
    rec.name = label;
    rec.label = label;
    rec.legacyAliases.clear();
    rec.initialCount = 0;
    rec.color = kLabelPalette[species_.size() % kLabelPalette.size()];
    const simulation::SpeciesId id = species_.registerSpecies(rec);
    assignSelectedToSpecies(ids, id);
    return id;
}

bool SimulationRunner::setSpeciesColorAndRecolor(const simulation::SpeciesId speciesId,
                                                   const simulation::ColorRgb color)
{
    if (!species_.setColor(speciesId, color)) return false;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (agents_.aliveAt(i) && agents_.speciesIdAt(i) == speciesId)
        {
            agents_.setColorAt(i, color);
        }
    }
    return true;
}

bool SimulationRunner::cycleSpeciesColor(const simulation::SpeciesId speciesId)
{
    const auto* rec = species_.find(speciesId);
    if (rec == nullptr) return false;
    // Find current palette index and advance to the next color.
    std::size_t next = 0;
    for (std::size_t i = 0; i < kLabelPalette.size(); ++i)
    {
        if (kLabelPalette[i].r == rec->color.r && kLabelPalette[i].g == rec->color.g &&
            kLabelPalette[i].b == rec->color.b)
        {
            next = (i + 1) % kLabelPalette.size();
            break;
        }
    }
    return setSpeciesColorAndRecolor(speciesId, kLabelPalette[next]);
}

bool SimulationRunner::adjustSpeciesPopulation(const simulation::SpeciesId speciesId,
                                                 const int field, const int delta)
{
    const auto* rec = species_.find(speciesId);
    if (rec == nullptr) return false;
    if (field == 0) return species_.setMinPopulation(speciesId, rec->minPopulation + delta);
    if (field == 1) return species_.setMaxPopulation(speciesId, rec->maxPopulation + delta);
    if (field == 2) return species_.setInitialCount(speciesId, rec->initialCount + delta);
    return false;
}

bool SimulationRunner::setSpeciesShowGraph(const simulation::SpeciesId speciesId, const bool show)
{
    return species_.setShowGraph(speciesId, show);
}

bool SimulationRunner::setSpeciesLabel(const simulation::SpeciesId speciesId,
                                         const std::string& label)
{
    return species_.setLabel(speciesId, label);
}

bool SimulationRunner::removeSpeciesSafe(const simulation::SpeciesId speciesId)
{
    const auto* bacteria = species_.findByName("bacteria");
    // Never delete the default bacteria species.
    if (bacteria != nullptr && bacteria->id == speciesId) return false;
    // Reassign that species' live agents to bacteria, then soft-disable it.
    if (bacteria != nullptr)
    {
        std::vector<simulation::EntityId> orphans;
        for (std::size_t i = 0; i < agents_.size(); ++i)
        {
            if (agents_.aliveAt(i) && agents_.speciesIdAt(i) == speciesId)
            {
                orphans.push_back(agents_.idAt(i));
            }
        }
        assignSelectedToSpecies(orphans, bacteria->id);
    }
    return species_.setEnabled(speciesId, false);
}

std::size_t SimulationRunner::countAgentsOfSpecies(const simulation::SpeciesId speciesId) const
{
    std::size_t n = 0;
    for (std::size_t i = 0; i < agents_.size(); ++i)
    {
        if (agents_.aliveAt(i) && agents_.speciesIdAt(i) == speciesId) ++n;
    }
    return n;
}

bool SimulationRunner::applyCommand(const core::Command& cmd)
{
    return std::visit([&](auto&& c) -> bool {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, core::CmdPauseToggle>)        { togglePaused(); return true; }
        else if constexpr (std::is_same_v<T, core::CmdSetPaused>)     { setPaused(c.paused); return true; }
        else if constexpr (std::is_same_v<T, core::CmdResetSimulation>) { reset(); return true; }
        else if constexpr (std::is_same_v<T, core::CmdStepOnce>)      { requestStepOnce(); return true; }
        else if constexpr (std::is_same_v<T, core::CmdSetTimeScale>)  { timeScale_ = std::max(0.0, c.timeScale); return true; }
        else if constexpr (std::is_same_v<T, core::CmdFitWorldCamera>) { return true; /* handled by AppController */ }
        else if constexpr (std::is_same_v<T, core::CmdSetCameraCenter>) { return true; /* handled by AppController */ }
        else if constexpr (std::is_same_v<T, core::CmdPanCameraScreen>) { return true; /* handled by AppController */ }
        else if constexpr (std::is_same_v<T, core::CmdZoomCameraAt>)   { return true; /* handled by AppController */ }
        else if constexpr (std::is_same_v<T, core::CmdSetCanvasTool>)  { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, core::CmdSelectAtWorldPoint>) { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, core::CmdSelectRect>)     { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, core::CmdSelectLasso>)    { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, core::CmdClearSelection>) { return true; /* handled by UI */ }
        else if constexpr (std::is_same_v<T, core::CmdDeleteSelected>) { return true; /* handled by UI with help of runner */ }
        else if constexpr (std::is_same_v<T, core::CmdMoveSelectedBy>) { return true; /* handled by UI with help of runner */ }
        else if constexpr (std::is_same_v<T, core::CmdSpawnFoodAt>)
        {
            static_cast<void>(spawnFoodAt(c.world, c.radius, c.energy)); return true;
        }
        else if constexpr (std::is_same_v<T, core::CmdSpawnAgentAt>)
        {
            static_cast<void>(spawnAgentDefaultAt(c.world)); return true;
        }
        else if constexpr (std::is_same_v<T, core::CmdPaintObstacleAt>)
        {
            static_cast<void>(obstacles_.paint(c.world, c.brushRadius)); return true;
        }
        else if constexpr (std::is_same_v<T, core::CmdEraseObstacleAt>)
        {
            static_cast<void>(obstacles_.eraseAt(c.world, c.eraseRadius)); return true;
        }
        else if constexpr (std::is_same_v<T, core::CmdPaintObstacleStroke>)
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
        else if constexpr (std::is_same_v<T, core::CmdEraseObstacleStroke>)
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
        else if constexpr (std::is_same_v<T, core::CmdClearObstacles>) { obstacles_.clear(); return true; }
        else if constexpr (std::is_same_v<T, core::CmdClearFood>) {
            static_cast<void>(foodSystem_.clearAll(foods_)); return true;
        }
        else if constexpr (std::is_same_v<T, core::CmdToggleSpatialHashOverlay>) {
            showSpatialHash_ = !showSpatialHash_; return true;
        }
        else if constexpr (std::is_same_v<T, core::CmdToggleSelectionOverlay>) { return true; /* UI flag */ }
        else if constexpr (std::is_same_v<T, core::CmdToggleToolOverlay>) { return true; /* UI flag */ }
        else if constexpr (std::is_same_v<T, core::CmdToggleHelpPanel>) { return true; /* UI flag */ }
        else if constexpr (std::is_same_v<T, core::CmdToggleSimpleRender>) {
            simpleRender_ = !simpleRender_; return true;
        }
        else if constexpr (std::is_same_v<T, core::CmdToggleVisionDebug>) { return true; /* UI flag */ }
        // Phase 22.1 hotfix: new commands. NewSimulation is just an alias for
        // reset; the rest are UI-only (panel toggles, camera reset, quit) and
        // are handled by AppController in drainCommandsAndApply().
        else if constexpr (std::is_same_v<T, core::CmdNewSimulation>) { reset(); return true; }
        else if constexpr (std::is_same_v<T, core::CmdQuitApp>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdTogglePreferencesPanel>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdToggleAboutPanel>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdToggleGenomePanel>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdResetCamera>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCloseAllMenus>) { return true; }
        // Phase 23: preferences commands are UI-only from the runner POV.
        else if constexpr (std::is_same_v<T, core::CmdOpenPreferences>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdClosePreferences>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdSetPreferencesTab>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdSetPreferencesSearch>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdSetParameterValue>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdApplyPreferences>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdRevertPreferences>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdRestoreDefaultsPreferences>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdRestoreParameterDefault>) { return true; }
        // Phase 23.1: multi-window prefs + popups + velocity widget.
        else if constexpr (std::is_same_v<T, core::CmdOpenPreferencesWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdClosePreferencesWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdScrollPreferencesWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdOpenHelpWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCloseHelpWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdOpenSubstratePlaceholder>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCloseSubstratePlaceholder>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdOpenPrefsPopup>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdClosePrefsPopup>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdAdjustTimeScale>) { return true; }
        // Phase 23.2: draggable windows + text editor + restore-and-apply.
        else if constexpr (std::is_same_v<T, core::CmdMovePreferencesWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdMoveHelpWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdBeginEditParameter>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCancelEditParameter>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCommitEditParameter>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdRestoreDefaultsAndApply>) { return true; }
        // Phase 24: operational windows + apply commands. Most are UI-only;
        // CmdClearFood maps to foodSystem clearAll.
        else if constexpr (std::is_same_v<T, core::CmdOpenEditorGenetico>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCloseEditorGenetico>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdOpenEspecies>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCloseEspecies>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdOpenPopulacao>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdClosePopulacao>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdOpenSubstrato>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCloseSubstrato>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdScrollOperationalWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdMoveOperationalWindow>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdApplyGenomeToSelected>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdApplyGenomeToSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdResetNeuralForSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdSelectAllOfSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdAssignSelectedToSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCreateSpeciesFromSelected>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdApplyPopulation>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdApplyEnvironment>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdClearAllFood>)
        {
            static_cast<void>(foodSystem_.clearAll(foods_)); return true;
        }
        // Phase 24.2: left dock + per-label commands are coordinated by the
        // AppController (which owns the selection + dedicated runner methods),
        // so the runner treats them as UI-only here.
        else if constexpr (std::is_same_v<T, core::CmdSetDockTab>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdScrollDock>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdToggleLeftDock>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdRemoveSelectedFromSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCycleSpeciesColor>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdSetSpeciesShowGraph>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdAdjustSpeciesPop>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdRemoveSpecies>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdBeginEditSpeciesName>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCommitEditSpeciesName>) { return true; }
        else if constexpr (std::is_same_v<T, core::CmdCancelEditSpeciesName>) { return true; }
        else { return false; }
    }, cmd);
}
} // namespace agentbiosim::sim
