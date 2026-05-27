#pragma once

#include "render/Camera2D.hpp"
#include "render/RenderOptions.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/World.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

#include <cstddef>

namespace agentbiosim::render
{
struct RenderStats
{
    bool skipped = false;
    std::size_t agentsDrawn = 0;
    std::size_t foodsDrawn = 0;
};

class Renderer
{
public:
    [[nodiscard]] RenderStats render(sf::RenderTarget& target,
                                     const Camera2D& camera,
                                     const simulation::World& world,
                                     const simulation::AgentStore& agents,
                                     const simulation::FoodStore& foods,
                                     const RenderOptions& options) const;

private:
    void drawBackground(sf::RenderTarget& target, const RenderOptions& options) const;
    void drawWorldBoundary(sf::RenderTarget& target,
                           const Camera2D& camera,
                           const simulation::World& world,
                           const RenderOptions& options) const;
    std::size_t drawFoods(sf::RenderTarget& target,
                          const Camera2D& camera,
                          const simulation::FoodStore& foods,
                          const RenderOptions& options) const;
    std::size_t drawAgents(sf::RenderTarget& target,
                           const Camera2D& camera,
                           const simulation::AgentStore& agents,
                           const RenderOptions& options) const;
};
} // namespace agentbiosim::render
