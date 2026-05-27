#pragma once

#include "config/ParameterRegistry.hpp"
#include "render/Camera2D.hpp"
#include "simulation/FixedTimestep.hpp"
#include "simulation/World.hpp"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>

namespace agentbiosim
{
class App
{
public:
    App();

    int run();

private:
    void processEvents();
    void handleResize(unsigned int width, unsigned int height);
    void configureFromParameters();
    void fitCameraToWorld();
    void update();
    void render();
    void renderWorldBoundary();
    void updateFpsTitle();

    config::ParameterRegistry parameters_;
    simulation::World world_;
    simulation::FixedTimestep timestep_;
    render::Camera2D camera_;
    sf::RenderWindow window_;
    sf::Clock frameClock_;
    sf::Clock fpsClock_;
    sf::Vector2i lastMousePosition_{0, 0};
    bool isPanning_ = false;
    unsigned int frames_ = 0;
    unsigned long long simulatedSteps_ = 0;
    unsigned int lastStepsThisFrame_ = 0;
    float lastFps_ = 0.0F;
};
} // namespace agentbiosim
