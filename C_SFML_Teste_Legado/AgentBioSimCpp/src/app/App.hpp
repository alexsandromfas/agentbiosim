#pragma once

#include "config/ParameterRegistry.hpp"
#include "perception/VisionDebug.hpp"
#include "render/Camera2D.hpp"
#include "render/Renderer.hpp"
#include "sim/SimulationRunner.hpp"
#include "simulation/FixedTimestep.hpp"
#include "simulation/SpatialHash.hpp"
#include "ui/Command.hpp"
#include "ui/InputRouter.hpp"
#include "ui/UiPanel.hpp"
#include "ui/UiState.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <vector>

namespace agentbiosim
{
// Phase 22: App now acts as the AppController. It owns the SFML window, the
// Camera2D, the Renderer, the SimulationRunner (engine), the InputRouter and
// the UiPanel. It does NOT contain simulation logic anymore — all sim systems
// live inside `runner_`. This resolves Debt 4 by lifting the engine
// orchestration out of App.
class App
{
public:
    App();

    int run();

private:
    void processEvents();
    void handleResize(unsigned int width, unsigned int height);
    void configureFromParameters();
    void configureRenderOptions();
    void fitCameraToWorld();
    void update();
    void render();
    void updateFpsTitle();
    void drainCommandsAndApply();

    config::ParameterRegistry parameters_;
    sim::SimulationRunner runner_;
    simulation::FixedTimestep timestep_;
    render::Camera2D camera_;
    render::Renderer renderer_;
    render::RenderOptions renderOptions_;
    render::RenderStats lastRenderStats_;
    sf::RenderWindow window_;
    sf::Clock frameClock_;
    sf::Clock fpsClock_;
    sf::Vector2i lastMousePosition_{0, 0};
    perception::VisionDebugData visionDebug_{};
    sf::Font font_;
    bool fontLoaded_ = false;

    ui::UiState uiState_{};
    ui::InputRouter inputRouter_{};
    ui::UiPanel uiPanel_{};
    ui::CommandQueue commandQueue_;

    unsigned int frames_ = 0;
    float lastFps_ = 0.0F;
    unsigned long long simulatedSteps_ = 0;
    unsigned int lastStepsThisFrame_ = 0;
};
} // namespace agentbiosim
