#include "render/Renderer.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>

namespace agentbiosim::render
{
namespace
{
sf::Vector2f toSfml(const simulation::Vec2 value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y)};
}

sf::Color toSfmlColor(const simulation::ColorRgb color, const sf::Uint8 alpha = 255)
{
    return {color.r, color.g, color.b, alpha};
}

sf::Color mixColor(const sf::Color top, const sf::Color bottom)
{
    return {
        static_cast<sf::Uint8>((static_cast<unsigned int>(top.r) + static_cast<unsigned int>(bottom.r)) / 2U),
        static_cast<sf::Uint8>((static_cast<unsigned int>(top.g) + static_cast<unsigned int>(bottom.g)) / 2U),
        static_cast<sf::Uint8>((static_cast<unsigned int>(top.b) + static_cast<unsigned int>(bottom.b)) / 2U),
        static_cast<sf::Uint8>((static_cast<unsigned int>(top.a) + static_cast<unsigned int>(bottom.a)) / 2U),
    };
}
} // namespace

RenderStats Renderer::render(sf::RenderTarget& target,
                             const Camera2D& camera,
                             const simulation::World& world,
                             const simulation::AgentStore& agents,
                             const simulation::FoodStore& foods,
                             const RenderOptions& options,
                             const perception::VisionDebugData* visionDebug) const
{
    RenderStats stats;
    if (!options.renderEnabled)
    {
        stats.skipped = true;
        return stats;
    }

    drawBackground(target, options);
    drawWorldBoundary(target, camera, world, options);
    stats.foodsDrawn = drawFoods(target, camera, foods, options);
    stats.agentsDrawn = drawAgents(target, camera, agents, options);
    if (visionDebug != nullptr && visionDebug->active && !visionDebug->rays.empty())
    {
        stats.visionRaysDrawn = drawVisionDebug(target, camera, *visionDebug);
    }
    return stats;
}

std::size_t Renderer::drawVisionDebug(sf::RenderTarget& target,
                                      const Camera2D& camera,
                                      const perception::VisionDebugData& debug) const
{
    const sf::Vector2u viewport = target.getSize();
    sf::VertexArray lines(sf::Lines);
    lines.resize(debug.rays.size() * 2U);
    std::size_t idx = 0;
    for (const auto& ray : debug.rays)
    {
        const sf::Vector2f startW{static_cast<float>(ray.startX), static_cast<float>(ray.startY)};
        const sf::Vector2f endW{static_cast<float>(ray.hitX), static_cast<float>(ray.hitY)};
        const sf::Vector2f startS = camera.worldToScreen(startW, viewport);
        const sf::Vector2f endS = camera.worldToScreen(endW, viewport);

        sf::Color color;
        if (ray.hit)
        {
            const sf::Uint8 r = static_cast<sf::Uint8>(std::clamp(ray.hitColorR * 255.0, 0.0, 255.0));
            const sf::Uint8 g = static_cast<sf::Uint8>(std::clamp(ray.hitColorG * 255.0, 0.0, 255.0));
            const sf::Uint8 b = static_cast<sf::Uint8>(std::clamp(ray.hitColorB * 255.0, 0.0, 255.0));
            const sf::Uint8 a = static_cast<sf::Uint8>(std::clamp(ray.activation * 255.0 + 64.0, 64.0, 255.0));
            color = sf::Color(r, g, b, a);
        }
        else
        {
            color = sf::Color(80, 80, 100, 80);
        }

        lines[idx].position = startS;
        lines[idx].color = color;
        ++idx;
        lines[idx].position = endS;
        lines[idx].color = color;
        ++idx;
    }
    target.draw(lines);
    return debug.rays.size();
}

void Renderer::drawBackground(sf::RenderTarget& target, const RenderOptions& options) const
{
    if (!options.backgroundGradientEnabled)
    {
        target.clear(options.backgroundColor);
        return;
    }

    target.clear(options.backgroundColorTop);
    const sf::Vector2u size = target.getSize();
    sf::VertexArray gradient(sf::Quads, 4);
    gradient[0].position = {0.0F, 0.0F};
    gradient[1].position = {static_cast<float>(size.x), 0.0F};
    gradient[2].position = {static_cast<float>(size.x), static_cast<float>(size.y)};
    gradient[3].position = {0.0F, static_cast<float>(size.y)};
    gradient[0].color = options.backgroundColorTop;
    gradient[1].color = options.backgroundColorTop;
    gradient[2].color = options.backgroundColorBottom;
    gradient[3].color = options.backgroundColorBottom;
    target.draw(gradient);
}

void Renderer::drawWorldBoundary(sf::RenderTarget& target,
                                 const Camera2D& camera,
                                 const simulation::World& world,
                                 const RenderOptions& options) const
{
    const sf::Vector2u viewport = target.getSize();
    const sf::Color substrateFill = options.substrateGradientEnabled
        ? mixColor(options.substrateColorTop, options.substrateColorBottom)
        : options.substrateColorTop;

    if (world.shape() == simulation::WorldShape::Circular)
    {
        const sf::Vector2f center = camera.worldToScreen(toSfml(world.center()), viewport);
        const float radius = static_cast<float>(world.radius()) * camera.zoom();

        sf::CircleShape circle(radius, 160);
        circle.setOrigin(radius, radius);
        circle.setPosition(center);
        circle.setFillColor(substrateFill);
        if (options.substrateBorderEnabled)
        {
            circle.setOutlineColor(options.substrateBorderColor);
            circle.setOutlineThickness(2.0F);
        }
        target.draw(circle);
        return;
    }

    const sf::Vector2f topLeft = camera.worldToScreen(toSfml(world.minBounds()), viewport);
    const sf::Vector2f bottomRight = camera.worldToScreen(toSfml(world.maxBounds()), viewport);
    const sf::Vector2f position{std::min(topLeft.x, bottomRight.x), std::min(topLeft.y, bottomRight.y)};
    const sf::Vector2f size{std::abs(bottomRight.x - topLeft.x), std::abs(bottomRight.y - topLeft.y)};

    sf::RectangleShape rectangle(size);
    rectangle.setPosition(position);
    rectangle.setFillColor(substrateFill);
    if (options.substrateBorderEnabled)
    {
        rectangle.setOutlineColor(options.substrateBorderColor);
        rectangle.setOutlineThickness(2.0F);
    }
    target.draw(rectangle);
}

std::size_t Renderer::drawFoods(sf::RenderTarget& target,
                                const Camera2D& camera,
                                const simulation::FoodStore& foods,
                                const RenderOptions& options) const
{
    const sf::Vector2u viewport = target.getSize();
    std::size_t drawn = 0;

    for (std::size_t i = 0; i < foods.size(); ++i)
    {
        if (!foods.aliveAt(i))
        {
            continue;
        }

        const sf::Vector2f position = camera.worldToScreen(toSfml(foods.positionAt(i)), viewport);
        const float radius = std::max(1.0F, static_cast<float>(foods.radiusAt(i)) * camera.zoom());

        sf::CircleShape food(radius, 24);
        food.setOrigin(radius, radius);
        food.setPosition(position);
        food.setFillColor(toSfmlColor(foods.colorAt(i)));
        if (foods.kindAt(i) == simulation::FoodKind::Chunk && radius >= 3.0F)
        {
            food.setOutlineThickness(std::max(1.0F, camera.zoom()));
            food.setOutlineColor(options.chunkFoodOutlineColor);
        }
        target.draw(food);
        ++drawn;
    }

    return drawn;
}

std::size_t Renderer::drawAgents(sf::RenderTarget& target,
                                 const Camera2D& camera,
                                 const simulation::AgentStore& agents,
                                 const RenderOptions& options) const
{
    const sf::Vector2u viewport = target.getSize();
    std::size_t drawn = 0;

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents.aliveAt(i))
        {
            continue;
        }

        const simulation::Vec2 worldPosition = agents.positionAt(i);
        const sf::Vector2f position = camera.worldToScreen(toSfml(worldPosition), viewport);
        const float radius = std::max(1.0F, static_cast<float>(agents.radiusAt(i)) * camera.zoom());

        sf::CircleShape agent(radius, 32);
        agent.setOrigin(radius, radius);
        agent.setPosition(position);
        agent.setFillColor(toSfmlColor(agents.colorAt(i)));
        target.draw(agent);

        const double angle = agents.angleAt(i);
        const simulation::Vec2 headWorld{
            worldPosition.x + std::cos(angle) * agents.radiusAt(i),
            worldPosition.y + std::sin(angle) * agents.radiusAt(i),
        };
        const sf::Vector2f headPosition = camera.worldToScreen(toSfml(headWorld), viewport);
        const float headRadius = std::max(1.0F, radius * 0.25F);
        sf::CircleShape head(headRadius, 12);
        head.setOrigin(headRadius, headRadius);
        head.setPosition(headPosition);
        head.setFillColor(options.agentHeadColor);
        target.draw(head);
        ++drawn;
    }

    return drawn;
}
} // namespace agentbiosim::render
