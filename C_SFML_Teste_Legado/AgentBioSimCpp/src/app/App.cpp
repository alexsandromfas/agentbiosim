#include "app/App.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/Parameter.hpp"
#include "core/Version.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <variant>

namespace agentbiosim
{
namespace
{
constexpr unsigned int kWindowWidth = 1280;
constexpr unsigned int kWindowHeight = 720;
constexpr unsigned int kFrameLimit = 120;
constexpr float kWorldPaddingPixels = 48.0F;

double parameterDouble(const config::ParameterRegistry& parameters, const std::string& name, const double fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<double>(&definition->defaultValue))
    {
        return *value;
    }
    if (const auto* value = std::get_if<int>(&definition->defaultValue))
    {
        return static_cast<double>(*value);
    }
    return fallback;
}

int parameterInt(const config::ParameterRegistry& parameters, const std::string& name, const int fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<int>(&definition->defaultValue))
    {
        return *value;
    }
    if (const auto* value = std::get_if<double>(&definition->defaultValue))
    {
        return static_cast<int>(*value);
    }
    return fallback;
}

bool parameterBool(const config::ParameterRegistry& parameters, const std::string& name, const bool fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<bool>(&definition->defaultValue))
    {
        return *value;
    }
    return fallback;
}

std::string parameterString(const config::ParameterRegistry& parameters, const std::string& name, const std::string& fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<std::string>(&definition->defaultValue))
    {
        return *value;
    }
    return fallback;
}

sf::Vector2f toSfml(const simulation::Vec2 value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y)};
}

config::ColorRgb parameterColor(const config::ParameterRegistry& parameters,
                                const std::string& name,
                                const config::ColorRgb fallback)
{
    const config::ParameterDefinition* definition = parameters.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<config::ColorRgb>(&definition->defaultValue))
    {
        return *value;
    }
    return fallback;
}

std::uint8_t colorChannel(const int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

simulation::ColorRgb toEntityColor(const config::ColorRgb color)
{
    return {colorChannel(color.r), colorChannel(color.g), colorChannel(color.b)};
}

sf::Color toRenderColor(const config::ColorRgb color)
{
    return {colorChannel(color.r), colorChannel(color.g), colorChannel(color.b)};
}

simulation::Vec2 randomPointInsideWorld(const simulation::World& world,
                                        const double entityRadius,
                                        std::mt19937& rng)
{
    if (world.shape() == simulation::WorldShape::Circular)
    {
        std::uniform_real_distribution<double> unit(0.0, 1.0);
        const double angle = unit(rng) * 2.0 * 3.14159265358979323846;
        const double radius = std::sqrt(unit(rng)) * std::max(0.0, world.radius() - entityRadius);
        const simulation::Vec2 center = world.center();
        return {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
    }

    const double minX = entityRadius;
    const double maxX = std::max(entityRadius, world.width() - entityRadius);
    const double minY = entityRadius;
    const double maxY = std::max(entityRadius, world.height() - entityRadius);
    std::uniform_real_distribution<double> xDistribution(minX, maxX);
    std::uniform_real_distribution<double> yDistribution(minY, maxY);
    return {xDistribution(rng), yDistribution(rng)};
}
} // namespace

App::App()
    : parameters_(config::createDefaultParameterRegistry()),
      window_(sf::VideoMode(kWindowWidth, kWindowHeight), "AgentBioSimCpp")
{
    window_.setFramerateLimit(kFrameLimit);
    configureFromParameters();
    configureRenderOptions();
    fitCameraToWorld();
    spawnDemoEntities();

    std::cout << "AgentBioSimCpp Phase 4: basic entity stores initialized.\n";
    std::cout << "Controls: mouse wheel zoom, right/middle drag pan, F fit world, Space pause.\n";
    std::cout << "Spawned static visual smoke test: " << agents_.size() << " agents, "
              << foods_.size() << " foods.\n";
}

int App::run()
{
    while (window_.isOpen())
    {
        processEvents();
        update();
        render();
        updateFpsTitle();
    }

    return 0;
}

void App::processEvents()
{
    sf::Event event{};
    while (window_.pollEvent(event))
    {
        if (event.type == sf::Event::Closed)
        {
            window_.close();
        }
        else if (event.type == sf::Event::Resized)
        {
            handleResize(event.size.width, event.size.height);
        }
        else if (event.type == sf::Event::MouseWheelScrolled)
        {
            const float factor = event.mouseWheelScroll.delta > 0.0F ? 1.1F : 1.0F / 1.1F;
            camera_.zoomAt(factor,
                           {static_cast<float>(event.mouseWheelScroll.x), static_cast<float>(event.mouseWheelScroll.y)},
                           window_.getSize());
        }
        else if (event.type == sf::Event::MouseButtonPressed)
        {
            if (event.mouseButton.button == sf::Mouse::Right || event.mouseButton.button == sf::Mouse::Middle)
            {
                isPanning_ = true;
                lastMousePosition_ = {event.mouseButton.x, event.mouseButton.y};
            }
        }
        else if (event.type == sf::Event::MouseButtonReleased)
        {
            if (event.mouseButton.button == sf::Mouse::Right || event.mouseButton.button == sf::Mouse::Middle)
            {
                isPanning_ = false;
            }
        }
        else if (event.type == sf::Event::MouseMoved)
        {
            if (isPanning_)
            {
                const sf::Vector2i current{event.mouseMove.x, event.mouseMove.y};
                const sf::Vector2i delta = current - lastMousePosition_;
                camera_.pan({static_cast<float>(delta.x), static_cast<float>(delta.y)});
                lastMousePosition_ = current;
            }
        }
        else if (event.type == sf::Event::KeyPressed)
        {
            if (event.key.code == sf::Keyboard::F)
            {
                fitCameraToWorld();
            }
            else if (event.key.code == sf::Keyboard::Space)
            {
                timestep_.setPaused(!timestep_.paused());
            }
        }
    }
}

void App::handleResize(const unsigned int width, const unsigned int height)
{
    if (width == 0U || height == 0U)
    {
        return;
    }
    fitCameraToWorld();
}

void App::configureFromParameters()
{
    const double width = parameterDouble(parameters_, "world_w", 1000.0);
    const double height = parameterDouble(parameters_, "world_h", 700.0);
    const double radius = parameterDouble(parameters_, "substrate_radius", 400.0);
    const std::string shapeName = parameterString(parameters_, "substrate_shape", "rectangular");

    simulation::WorldConfig worldConfig;
    worldConfig.width = width;
    worldConfig.height = height;
    worldConfig.radius = radius;
    worldConfig.center = {width * 0.5, height * 0.5};
    worldConfig.shape = shapeName == "circular" ? simulation::WorldShape::Circular : simulation::WorldShape::Rectangular;
    world_.configure(worldConfig);

    simulation::FixedTimestepConfig timestepConfig;
    timestepConfig.physicsStepsPerSecond = parameterDouble(parameters_, "physics_steps_per_second", 30.0);
    timestepConfig.maxStepsPerFrame = static_cast<unsigned int>(std::max(1, parameterInt(parameters_, "max_physics_steps_per_frame", 8)));
    timestepConfig.maxBacklogSeconds = parameterDouble(parameters_, "max_physics_backlog_seconds", 0.25);
    timestepConfig.timeScale = parameterDouble(parameters_, "time_scale", 1.0);
    timestepConfig.paused = parameterBool(parameters_, "paused", false);
    timestep_.configure(timestepConfig);
}

void App::fitCameraToWorld()
{
    camera_.fitWorld(toSfml(world_.minBounds()), toSfml(world_.maxBounds()), window_.getSize(), kWorldPaddingPixels);
}

void App::configureRenderOptions()
{
    renderOptions_.renderEnabled = parameterBool(parameters_, "render_enabled", true);
    renderOptions_.simpleRender = parameterBool(parameters_, "simple_render", false);
    renderOptions_.renderResolutionScale = static_cast<float>(parameterDouble(parameters_, "render_resolution_scale", 1.0));

    renderOptions_.backgroundColor =
        toRenderColor(parameterColor(parameters_, "substrate_bg_color", {10, 10, 20}));
    renderOptions_.backgroundGradientEnabled = parameterBool(parameters_, "background_gradient_enabled", false);
    renderOptions_.backgroundColorTop =
        toRenderColor(parameterColor(parameters_, "background_color_top", {10, 10, 20}));
    renderOptions_.backgroundColorBottom =
        toRenderColor(parameterColor(parameters_, "background_color_bottom", {10, 10, 20}));

    renderOptions_.substrateGradientEnabled = parameterBool(parameters_, "substrate_gradient_enabled", false);
    renderOptions_.substrateColorTop =
        toRenderColor(parameterColor(parameters_, "substrate_color_top", {10, 10, 20}));
    renderOptions_.substrateColorBottom =
        toRenderColor(parameterColor(parameters_, "substrate_color_bottom", {10, 10, 20}));
    renderOptions_.substrateBorderEnabled = parameterBool(parameters_, "substrate_border_enabled", true);
    renderOptions_.substrateBorderColor =
        toRenderColor(parameterColor(parameters_, "substrate_border_color", {40, 200, 40}));
}

void App::spawnDemoEntities()
{
    agents_.clear();
    foods_.clear();

    const int seedParameter = parameterInt(parameters_, "random_seed", -1);
    const std::uint32_t seed = seedParameter >= 0 ? static_cast<std::uint32_t>(seedParameter) : 1337U;
    std::mt19937 rng(seed);

    const int agentCount = std::max(0, parameterInt(parameters_, "bacteria_count", 150));
    const double agentRadius = std::max(0.1, parameterDouble(parameters_, "bacteria_body_size", 9.0));
    const double initialEnergy = std::max(0.0, parameterDouble(parameters_, "bacteria_initial_energy", 100.0));
    const simulation::ColorRgb agentColor = toEntityColor(parameterColor(parameters_, "bacteria_color", {220, 220, 220}));

    std::uniform_real_distribution<double> angleDistribution(0.0, 2.0 * 3.14159265358979323846);
    for (int i = 0; i < agentCount; ++i)
    {
        simulation::AgentSpawn spawn;
        spawn.position = world_.clampPosition(randomPointInsideWorld(world_, agentRadius, rng), agentRadius);
        spawn.angle = angleDistribution(rng);
        spawn.radius = agentRadius;
        spawn.energy = initialEnergy;
        spawn.color = agentColor;
        spawn.speciesId = 0;
        spawn.typeCode = simulation::AgentTypeCode::LegacyBacteria;
        [[maybe_unused]] const simulation::EntityId createdAgent = agents_.createAgent(spawn);
    }

    const int foodCount = std::max(0, parameterInt(parameters_, "food_target", 50));
    const double foodMinRadius = std::max(0.1, parameterDouble(parameters_, "food_min_r", 4.5));
    const double foodMaxRadius = std::max(foodMinRadius, parameterDouble(parameters_, "food_max_r", 5.0));
    const simulation::ColorRgb foodColor = toEntityColor(parameterColor(parameters_, "food_color", {220, 30, 30}));
    const std::string foodMode = parameterString(parameters_, "food_mode", "instant");
    const simulation::FoodKind foodKind = foodMode == "chunk" ? simulation::FoodKind::Chunk : simulation::FoodKind::Instant;
    std::uniform_real_distribution<double> foodRadiusDistribution(foodMinRadius, foodMaxRadius);

    for (int i = 0; i < foodCount; ++i)
    {
        const double radius = foodRadiusDistribution(rng);
        const double energy = std::max(1.0e-9, radius * radius);

        simulation::FoodSpawn spawn;
        spawn.position = world_.clampPosition(randomPointInsideWorld(world_, radius, rng), radius);
        spawn.radius = radius;
        spawn.energy = energy;
        spawn.initialEnergy = energy;
        spawn.color = foodColor;
        spawn.kind = foodKind;
        [[maybe_unused]] const simulation::EntityId createdFood = foods_.createFood(spawn);
    }
}

void App::update()
{
    const double realDeltaSeconds = frameClock_.restart().asSeconds();
    lastStepsThisFrame_ = timestep_.beginFrame(realDeltaSeconds);
    simulatedSteps_ += lastStepsThisFrame_;
}

void App::render()
{
    lastRenderStats_ = renderer_.render(window_, camera_, world_, agents_, foods_, renderOptions_);
    window_.display();
    ++frames_;
}

void App::updateFpsTitle()
{
    const float elapsed = fpsClock_.getElapsedTime().asSeconds();
    if (elapsed < 0.5F)
    {
        return;
    }

    const float fps = static_cast<float>(frames_) / elapsed;
    lastFps_ = fps;
    const char* shapeName = world_.shape() == simulation::WorldShape::Circular ? "circular" : "rectangular";

    std::ostringstream title;
    title << "AgentBioSimCpp " << kVersionString << " | FPS " << static_cast<int>(lastFps_ + 0.5F)
          << " | agents " << lastRenderStats_.agentsDrawn << "/" << agents_.size()
          << " | food " << lastRenderStats_.foodsDrawn << "/" << foods_.size()
          << " | world " << shapeName << " | zoom " << camera_.zoom()
          << " | dt " << timestep_.fixedDeltaSeconds()
          << " | steps " << simulatedSteps_;
    if (timestep_.paused())
    {
        title << " | paused";
    }
    window_.setTitle(title.str());

    frames_ = 0;
    fpsClock_.restart();
}
} // namespace agentbiosim
