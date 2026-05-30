#include "app/App.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterHelpers.hpp"
#include "core/Version.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <variant>

namespace agentbiosim
{
namespace
{
constexpr unsigned int kWindowWidth = 1280;
constexpr unsigned int kWindowHeight = 720;
constexpr unsigned int kFrameLimit = 120;
constexpr float kWorldPaddingPixels = 48.0F;

sf::Vector2f toSfml(const simulation::Vec2 value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y)};
}

std::uint8_t colorChannel(const int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

sf::Color toRenderColor(const config::ColorRgb color)
{
    return {colorChannel(color.r), colorChannel(color.g), colorChannel(color.b)};
}
} // namespace

App::App()
    : parameters_(config::createDefaultParameterRegistry()),
      runner_(parameters_),
      window_(sf::VideoMode(kWindowWidth, kWindowHeight), "AgentBioSimCpp")
{
    window_.setFramerateLimit(kFrameLimit);
    runner_.initialize();
    configureFromParameters();
    configureRenderOptions();
    fitCameraToWorld();
    // Try to load a font for UI text. Failure is non-fatal — UI still draws rects.
    if (font_.loadFromFile("C:/Windows/Fonts/segoeui.ttf") ||
        font_.loadFromFile("C:/Windows/Fonts/arial.ttf"))
    {
        fontLoaded_ = true;
        uiPanel_.setFont(&font_);
    }
    std::cout << "AgentBioSimCpp Phase 22: UI base + menus + canvas tools ready ("
              << runner_.species().size() << " species, " << runner_.genomes().size()
              << " genomes, " << runner_.foods().size() << " foods, "
              << runner_.obstacles().size() << " obstacles).\n";
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
            continue;
        }
        if (event.type == sf::Event::Resized)
        {
            handleResize(event.size.width, event.size.height);
            continue;
        }
        // Phase 22: UI consumes panel clicks first; otherwise routes to InputRouter.
        if (event.type == sf::Event::MouseButtonPressed)
        {
            const int sx = event.mouseButton.x;
            const int sy = event.mouseButton.y;
            if (uiPanel_.pointInsidePanel(sx, sy))
            {
                if (event.mouseButton.button == sf::Mouse::Left)
                {
                    static_cast<void>(uiPanel_.handleMouseClick(sx, sy, runner_, uiState_,
                                                                   commandQueue_));
                }
                continue;
            }
        }
        inputRouter_.handleEvent(event, window_.getSize(), camera_, runner_, uiState_,
                                   commandQueue_);
    }
    inputRouter_.update(window_.getSize(), camera_, uiState_, commandQueue_);
}

void App::handleResize(const unsigned int width, const unsigned int height)
{
    if (width == 0U || height == 0U) return;
    fitCameraToWorld();
}

void App::configureFromParameters()
{
    simulation::FixedTimestepConfig timestepConfig;
    timestepConfig.physicsStepsPerSecond = config::parameterDouble(parameters_,
        "physics_steps_per_second", 30.0);
    timestepConfig.maxStepsPerFrame = static_cast<unsigned int>(std::max(1,
        config::parameterInt(parameters_, "max_physics_steps_per_frame", 8)));
    timestepConfig.maxBacklogSeconds = config::parameterDouble(parameters_,
        "max_physics_backlog_seconds", 0.25);
    timestepConfig.timeScale = config::parameterDouble(parameters_, "time_scale", 1.0);
    timestepConfig.paused = config::parameterBool(parameters_, "paused", false);
    timestep_.configure(timestepConfig);
    if (timestepConfig.paused) runner_.setPaused(true);
}

void App::configureRenderOptions()
{
    renderOptions_.renderEnabled = config::parameterBool(parameters_, "render_enabled", true);
    renderOptions_.simpleRender = config::parameterBool(parameters_, "simple_render", false);
    renderOptions_.renderResolutionScale = static_cast<float>(
        config::parameterDouble(parameters_, "render_resolution_scale", 1.0));
    renderOptions_.backgroundColor =
        toRenderColor(config::parameterColor(parameters_, "substrate_bg_color", {10, 10, 20}));
    renderOptions_.backgroundGradientEnabled =
        config::parameterBool(parameters_, "background_gradient_enabled", false);
    renderOptions_.backgroundColorTop =
        toRenderColor(config::parameterColor(parameters_, "background_color_top", {10, 10, 20}));
    renderOptions_.backgroundColorBottom =
        toRenderColor(config::parameterColor(parameters_, "background_color_bottom", {10, 10, 20}));
    renderOptions_.substrateGradientEnabled =
        config::parameterBool(parameters_, "substrate_gradient_enabled", false);
    renderOptions_.substrateColorTop =
        toRenderColor(config::parameterColor(parameters_, "substrate_color_top", {10, 10, 20}));
    renderOptions_.substrateColorBottom =
        toRenderColor(config::parameterColor(parameters_, "substrate_color_bottom", {10, 10, 20}));
    renderOptions_.substrateBorderEnabled =
        config::parameterBool(parameters_, "substrate_border_enabled", true);
    renderOptions_.substrateBorderColor =
        toRenderColor(config::parameterColor(parameters_, "substrate_border_color", {40, 200, 40}));
}

void App::fitCameraToWorld()
{
    camera_.fitWorld(toSfml(runner_.world().minBounds()), toSfml(runner_.world().maxBounds()),
                       window_.getSize(), kWorldPaddingPixels);
}

void App::drainCommandsAndApply()
{
    auto commands = commandQueue_.drain();
    uiState_.commandsProcessed += commands.size();
    for (const auto& cmd : commands)
    {
        // Handle UI-side commands here. SimulationRunner consumes engine-only ones.
        std::visit([&](auto&& c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, ui::CmdFitWorldCamera>)
            {
                fitCameraToWorld();
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetCameraCenter>)
            {
                camera_.setCenter({static_cast<float>(c.worldCenter.x),
                                    static_cast<float>(c.worldCenter.y)});
            }
            else if constexpr (std::is_same_v<T, ui::CmdPanCameraScreen>)
            {
                camera_.pan({static_cast<float>(c.dx), static_cast<float>(c.dy)});
            }
            else if constexpr (std::is_same_v<T, ui::CmdZoomCameraAt>)
            {
                camera_.zoomAt(static_cast<float>(c.factor),
                                 {static_cast<float>(c.screenX), static_cast<float>(c.screenY)},
                                 window_.getSize());
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetCanvasTool>)
            {
                uiState_.activeTool = c.tool;
            }
            else if constexpr (std::is_same_v<T, ui::CmdSelectAtWorldPoint>)
            {
                const auto id = runner_.pickAgentAt(c.world, c.pickRadius);
                if (!c.additive) uiState_.selection.clear();
                if (id.isValid()) uiState_.selection.add(id);
            }
            else if constexpr (std::is_same_v<T, ui::CmdSelectRect>)
            {
                std::vector<simulation::EntityId> hits;
                static_cast<void>(runner_.agentsInRect(c.worldA, c.worldB, hits));
                if (!c.additive) uiState_.selection.clear();
                for (const auto id : hits) uiState_.selection.add(id);
            }
            else if constexpr (std::is_same_v<T, ui::CmdSelectLasso>)
            {
                std::vector<simulation::EntityId> hits;
                static_cast<void>(runner_.agentsInLasso(c.worldPolygon, hits));
                if (!c.additive) uiState_.selection.clear();
                for (const auto id : hits) uiState_.selection.add(id);
            }
            else if constexpr (std::is_same_v<T, ui::CmdClearSelection>)
            {
                uiState_.selection.clear();
            }
            else if constexpr (std::is_same_v<T, ui::CmdDeleteSelected>)
            {
                runner_.deleteAgents(uiState_.selection.ids());
                uiState_.selection.clear();
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleSelectionOverlay>)
            {
                uiState_.showSelectionOverlay = !uiState_.showSelectionOverlay;
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleToolOverlay>)
            {
                uiState_.showToolOverlay = !uiState_.showToolOverlay;
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleHelpPanel>)
            {
                uiState_.showHelp = !uiState_.showHelp;
            }
            else
            {
                static_cast<void>(c);
            }
        }, cmd);
        static_cast<void>(runner_.applyCommand(cmd));
    }
}

void App::update()
{
    const double realDeltaSeconds = frameClock_.restart().asSeconds();
    drainCommandsAndApply();
    lastStepsThisFrame_ = timestep_.beginFrame(realDeltaSeconds);
    for (unsigned int step = 0; step < lastStepsThisFrame_; ++step)
    {
        runner_.step(timestep_.fixedDeltaSeconds());
        ++simulatedSteps_;
    }
}

void App::render()
{
    if (!renderOptions_.renderEnabled)
    {
        window_.clear();
        window_.display();
        ++frames_;
        return;
    }
    const auto* obstaclePtr = runner_.obstacles().empty() ? nullptr : &runner_.obstacles();
    lastRenderStats_ = renderer_.render(window_, camera_, runner_.world(),
                                          runner_.agents(), runner_.foods(),
                                          renderOptions_, nullptr, obstaclePtr);
    uiPanel_.draw(window_, runner_, uiState_);
    window_.display();
    ++frames_;
}

void App::updateFpsTitle()
{
    const float elapsed = fpsClock_.getElapsedTime().asSeconds();
    if (elapsed >= 0.5F)
    {
        lastFps_ = static_cast<float>(frames_) / elapsed;
        frames_ = 0;
        fpsClock_.restart();
        std::ostringstream title;
        title << "AgentBioSimCpp Phase 22 - " << static_cast<int>(lastFps_) << " FPS"
                << "  steps=" << simulatedSteps_
                << "  tool=" << ui::canvasToolLabel(uiState_.activeTool)
                << "  sel=" << uiState_.selection.size();
        window_.setTitle(title.str());
    }
}
} // namespace agentbiosim
