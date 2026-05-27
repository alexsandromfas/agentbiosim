#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>

namespace agentbiosim
{
class App
{
public:
    App();

    int run();

private:
    void processEvents();
    void render();
    void updateFpsTitle();

    sf::RenderWindow window_;
    sf::Clock frameClock_;
    sf::Clock fpsClock_;
    unsigned int frames_ = 0;
};
} // namespace agentbiosim
