#include "app/App.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/Parameter.hpp"
#include "core/Version.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
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
} // namespace

App::App()
    : parameters_(config::createDefaultParameterRegistry()),
      window_(sf::VideoMode(kWindowWidth, kWindowHeight), "AgentBioSimCpp")
{
    window_.setFramerateLimit(kFrameLimit);
    configureFromParameters();
    fitCameraToWorld();

    std::cout << "AgentBioSimCpp Phase 3: world/camera/fixed timestep initialized.\n";
    std::cout << "Controls: mouse wheel zoom, right/middle drag pan, F fit world, Space pause.\n";
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

void App::update()
{
    const double realDeltaSeconds = frameClock_.restart().asSeconds();
    lastStepsThisFrame_ = timestep_.beginFrame(realDeltaSeconds);
    simulatedSteps_ += lastStepsThisFrame_;
}

void App::render()
{
    window_.clear(sf::Color(11, 14, 18));
    renderWorldBoundary();
    window_.display();
    ++frames_;
}

void App::renderWorldBoundary()
{
    const sf::Vector2u viewport = window_.getSize();
    const sf::Color fillColor(18, 24, 30);
    const sf::Color outlineColor(70, 210, 120);

    if (world_.shape() == simulation::WorldShape::Circular)
    {
        const sf::Vector2f center = camera_.worldToScreen(toSfml(world_.center()), viewport);
        const float radius = static_cast<float>(world_.radius()) * camera_.zoom();

        sf::CircleShape circle(radius, 160);
        circle.setOrigin(radius, radius);
        circle.setPosition(center);
        circle.setFillColor(fillColor);
        circle.setOutlineColor(outlineColor);
        circle.setOutlineThickness(2.0F);
        window_.draw(circle);
        return;
    }

    const sf::Vector2f topLeft = camera_.worldToScreen(toSfml(world_.minBounds()), viewport);
    const sf::Vector2f bottomRight = camera_.worldToScreen(toSfml(world_.maxBounds()), viewport);
    const sf::Vector2f position{std::min(topLeft.x, bottomRight.x), std::min(topLeft.y, bottomRight.y)};
    const sf::Vector2f size{std::abs(bottomRight.x - topLeft.x), std::abs(bottomRight.y - topLeft.y)};

    sf::RectangleShape rectangle(size);
    rectangle.setPosition(position);
    rectangle.setFillColor(fillColor);
    rectangle.setOutlineColor(outlineColor);
    rectangle.setOutlineThickness(2.0F);
    window_.draw(rectangle);
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
