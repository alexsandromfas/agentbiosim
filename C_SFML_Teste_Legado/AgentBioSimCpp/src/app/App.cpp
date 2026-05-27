#include "app/App.hpp"

#include "core/Version.hpp"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <sstream>

namespace agentbiosim
{
namespace
{
constexpr unsigned int kWindowWidth = 1280;
constexpr unsigned int kWindowHeight = 720;
constexpr unsigned int kFrameLimit = 120;
} // namespace

App::App()
    : window_(sf::VideoMode(kWindowWidth, kWindowHeight), "AgentBioSimCpp")
{
    window_.setFramerateLimit(kFrameLimit);
}

int App::run()
{
    while (window_.isOpen())
    {
        processEvents();
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
    }
}

void App::render()
{
    window_.clear(sf::Color(11, 14, 18));
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
    std::ostringstream title;
    title << "AgentBioSimCpp " << kVersionString << " | FPS " << static_cast<int>(fps + 0.5F);
    window_.setTitle(title.str());

    frames_ = 0;
    fpsClock_.restart();
    frameClock_.restart();
}
} // namespace agentbiosim
