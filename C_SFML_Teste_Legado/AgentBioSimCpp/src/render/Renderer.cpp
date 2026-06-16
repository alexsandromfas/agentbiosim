#include "render/Renderer.hpp"

#include "render/VisionOverlay.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/ConvexShape.hpp>
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

// Phase 22.1: brighten/darken helpers for vetorial body coloring.
sf::Color darken(const sf::Color c, const float k)
{
    const float m = std::clamp(k, 0.0F, 1.0F);
    return {static_cast<sf::Uint8>(static_cast<float>(c.r) * (1.0F - m)),
            static_cast<sf::Uint8>(static_cast<float>(c.g) * (1.0F - m)),
            static_cast<sf::Uint8>(static_cast<float>(c.b) * (1.0F - m)),
            c.a};
}

sf::Color lighten(const sf::Color c, const float k)
{
    const float m = std::clamp(k, 0.0F, 1.0F);
    const auto blend = [m](const sf::Uint8 v) {
        const float f = static_cast<float>(v) + (255.0F - static_cast<float>(v)) * m;
        return static_cast<sf::Uint8>(std::clamp(f, 0.0F, 255.0F));
    };
    return {blend(c.r), blend(c.g), blend(c.b), c.a};
}
} // namespace

RenderStats Renderer::render(sf::RenderTarget& target,
                             const Camera2D& camera,
                             const simulation::World& world,
                             const simulation::AgentStore& agents,
                             const simulation::FoodStore& foods,
                             const RenderOptions& options,
                             const perception::VisionDebugData* visionDebug,
                             const simulation::ObstacleStore* obstacles,
                             const SelectionRenderInput* selection,
                             const RenderInterpolation* interpolation) const
{
    RenderStats stats;
    if (!options.renderEnabled)
    {
        stats.skipped = true;
        return stats;
    }

    // Theme skin: the active theme already painted the background and the substrate
    // (dish) behind us, so skip them here and just draw the entities on the dish.
    if (!options.themeSkinActive)
    {
        drawBackground(target, options);
        drawWorldBoundary(target, camera, world, options);
    }
    if (options.showSpatialHashOverlay)
    {
        drawSpatialGrid(target, camera, world, options);
    }
    if (obstacles != nullptr && !obstacles->empty())
    {
        stats.obstaclesDrawn = drawObstacles(target, camera, *obstacles, options);
    }
    stats.foodsDrawn = drawFoods(target, camera, foods, options);
    stats.agentsDrawn = drawAgents(target, camera, agents, options, interpolation);
    if (visionDebug != nullptr && visionDebug->active && !visionDebug->rays.empty())
    {
        // Fase 32.1: mode-aware, prettier overlay (sector wedges vs raycast beams).
        stats.visionRaysDrawn = drawVisionOverlay(target, camera, *visionDebug);
    }
    // Phase 22.1: selection overlays go above agents but below UI panel.
    if (selection != nullptr)
    {
        stats.selectionHalosDrawn = drawSelectionHalos(target, camera, agents, *selection);
        if (selection->marqueeActive)
        {
            stats.marqueeRectsDrawn = drawMarquee(target, camera, *selection);
        }
        if (selection->lassoActive)
        {
            stats.lassoSegmentsDrawn = drawLasso(target, camera, *selection);
        }
        if (selection->brushCursorActive)
        {
            drawBrushCursor(target, camera, *selection);
        }
    }
    return stats;
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
    const bool grad = options.substrateGradientEnabled;
    const sf::Color top = options.substrateColorTop;
    const sf::Color bot = options.substrateColorBottom;
    // Real vertical gradient (was a flat 50/50 MIX before — white->black showed up
    // as solid gray instead of an actual top-to-bottom fade).
    const auto lerpC = [](const sf::Color a, const sf::Color b, float t) {
        t = std::clamp(t, 0.0F, 1.0F);
        const auto ch = [t](const sf::Uint8 ca, const sf::Uint8 cb) {
            return static_cast<sf::Uint8>(static_cast<float>(ca) +
                   (static_cast<float>(cb) - static_cast<float>(ca)) * t);
        };
        return sf::Color(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a));
    };

    if (world.shape() == simulation::WorldShape::Circular)
    {
        const sf::Vector2f center = camera.worldToScreen(toSfml(world.center()), viewport);
        const float radius = static_cast<float>(world.radius()) * camera.zoom();
        constexpr int kSeg = 192;
        if (grad)
        {
            // Vertical gradient clipped to the disc (triangle fan; rim vertices
            // tinted by their vertical position so the fade matches the rectangle).
            sf::VertexArray fan(sf::TriangleFan, static_cast<std::size_t>(kSeg) + 2U);
            fan[0].position = center;
            fan[0].color = lerpC(top, bot, 0.5F);
            for (int i = 0; i <= kSeg; ++i)
            {
                const float a = static_cast<float>(i) / static_cast<float>(kSeg) * 6.28318530718F;
                const sf::Vector2f p{center.x + std::cos(a) * radius, center.y + std::sin(a) * radius};
                fan[static_cast<std::size_t>(i) + 1U].position = p;
                fan[static_cast<std::size_t>(i) + 1U].color =
                    lerpC(top, bot, (p.y - (center.y - radius)) / (2.0F * radius));
            }
            target.draw(fan);
        }
        else
        {
            sf::CircleShape circle(radius, kSeg);
            circle.setOrigin(radius, radius);
            circle.setPosition(center);
            circle.setFillColor(top);
            target.draw(circle);
        }
        if (options.substrateBorderEnabled)
        {
            sf::CircleShape ring(radius, kSeg);
            ring.setOrigin(radius, radius);
            ring.setPosition(center);
            ring.setFillColor(sf::Color::Transparent);
            ring.setOutlineColor(options.substrateBorderColor);
            ring.setOutlineThickness(2.0F);
            target.draw(ring);
        }
        return;
    }

    const sf::Vector2f topLeft = camera.worldToScreen(toSfml(world.minBounds()), viewport);
    const sf::Vector2f bottomRight = camera.worldToScreen(toSfml(world.maxBounds()), viewport);
    const sf::Vector2f position{std::min(topLeft.x, bottomRight.x), std::min(topLeft.y, bottomRight.y)};
    const sf::Vector2f size{std::abs(bottomRight.x - topLeft.x), std::abs(bottomRight.y - topLeft.y)};

    if (grad)
    {
        sf::VertexArray quad(sf::Quads, 4);
        quad[0].position = {position.x, position.y};
        quad[1].position = {position.x + size.x, position.y};
        quad[2].position = {position.x + size.x, position.y + size.y};
        quad[3].position = {position.x, position.y + size.y};
        quad[0].color = top;
        quad[1].color = top;
        quad[2].color = bot;
        quad[3].color = bot;
        target.draw(quad);
    }
    else
    {
        sf::RectangleShape rectangle(size);
        rectangle.setPosition(position);
        rectangle.setFillColor(top);
        target.draw(rectangle);
    }
    if (options.substrateBorderEnabled)
    {
        sf::RectangleShape ring(size);
        ring.setPosition(position);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineColor(options.substrateBorderColor);
        ring.setOutlineThickness(2.0F);
        target.draw(ring);
    }
}

void Renderer::drawSpatialGrid(sf::RenderTarget& target,
                               const Camera2D& camera,
                               const simulation::World& world,
                               const RenderOptions& options) const
{
    const double cell = options.spatialHashCellSize;
    if (cell <= 0.0) return;
    const sf::Vector2u viewport = target.getSize();
    const simulation::Vec2 lo = world.minBounds();
    const simulation::Vec2 hi = world.maxBounds();
    const sf::Color line(90, 110, 150, 60);

    sf::VertexArray grid(sf::Lines);
    // Vertical lines (constant x), spanning the world height.
    for (double x = lo.x; x <= hi.x + 1.0e-6; x += cell)
    {
        grid.append({camera.worldToScreen({static_cast<float>(x), static_cast<float>(lo.y)}, viewport), line});
        grid.append({camera.worldToScreen({static_cast<float>(x), static_cast<float>(hi.y)}, viewport), line});
    }
    // Horizontal lines (constant y), spanning the world width.
    for (double y = lo.y; y <= hi.y + 1.0e-6; y += cell)
    {
        grid.append({camera.worldToScreen({static_cast<float>(lo.x), static_cast<float>(y)}, viewport), line});
        grid.append({camera.worldToScreen({static_cast<float>(hi.x), static_cast<float>(y)}, viewport), line});
    }
    target.draw(grid);
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
        const float radius = std::max(1.5F, static_cast<float>(foods.radiusAt(i)) * camera.zoom());

        // Phase 22.1: higher segment count + soft inner highlight for a more
        // vetorial appearance. 32 segments is cheap (vertex-bound).
        const sf::Color base = toSfmlColor(foods.colorAt(i));
        sf::CircleShape food(radius, 32);
        food.setOrigin(radius, radius);
        food.setPosition(position);
        food.setFillColor(base);
        if (foods.kindAt(i) == simulation::FoodKind::Chunk && radius >= 3.0F)
        {
            food.setOutlineThickness(std::max(1.0F, camera.zoom() * 0.6F));
            food.setOutlineColor(options.chunkFoodOutlineColor);
        }
        target.draw(food);
        // (Brilho interno removido a pedido: a comida e um disco liso.)
        ++drawn;
    }

    return drawn;
}

std::size_t Renderer::drawObstacles(sf::RenderTarget& target,
                                      const Camera2D& camera,
                                      const simulation::ObstacleStore& obstacles,
                                      const RenderOptions& options) const
{
    static_cast<void>(options);
    const sf::Vector2u viewport = target.getSize();
    std::size_t drawn = 0;
    for (std::size_t i = 0; i < obstacles.size(); ++i)
    {
        const sf::Vector2f pos = camera.worldToScreen(toSfml(obstacles.positionAt(i)), viewport);
        const float radius = std::max(1.0F, static_cast<float>(obstacles.radiusAt(i)) * camera.zoom());
        const sf::Color base = toSfmlColor(obstacles.colorAt(i));
        sf::CircleShape disc(radius, 32);
        disc.setOrigin(radius, radius);
        disc.setPosition(pos);
        disc.setFillColor(base);
        disc.setOutlineThickness(std::max(0.7F, camera.zoom() * 0.4F));
        disc.setOutlineColor(darken(base, 0.45F));
        target.draw(disc);
        ++drawn;
    }
    return drawn;
}

std::size_t Renderer::drawAgents(sf::RenderTarget& target,
                                 const Camera2D& camera,
                                 const simulation::AgentStore& agents,
                                 const RenderOptions& options,
                                 const RenderInterpolation* interpolation) const
{
    static_cast<void>(options);
    const sf::Vector2u viewport = target.getSize();
    std::size_t drawn = 0;

    // Render interpolation (visual only): draw at lerp(prev, live, alpha).
    const bool interp = interpolation != nullptr && interpolation->enabled &&
                        interpolation->prevPositions != nullptr;
    const double alpha = interp ? std::clamp(static_cast<double>(interpolation->alpha), 0.0, 1.0) : 0.0;

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents.aliveAt(i))
        {
            continue;
        }

        simulation::Vec2 worldPosition = agents.positionAt(i);
        if (interp)
        {
            const auto it = interpolation->prevPositions->find(agents.idAt(i).value);
            if (it != interpolation->prevPositions->end())
            {
                worldPosition.x = it->second.x + (worldPosition.x - it->second.x) * alpha;
                worldPosition.y = it->second.y + (worldPosition.y - it->second.y) * alpha;
            }
        }
        const sf::Vector2f position = camera.worldToScreen(toSfml(worldPosition), viewport);
        const float radius = std::max(2.0F, static_cast<float>(agents.radiusAt(i)) * camera.zoom());
        const sf::Color base = toSfmlColor(agents.colorAt(i));
        const double angle = agents.angleAt(i);

        // Phase 22.1: vetorial body — 40-segment disc with a subtle dark
        // outline and a small inner lightness gradient.
        // Phase 25.1 fix: honor the per-agent body shape. Ellipse organisms are
        // drawn elongated along their heading (scaled + rotated disc) instead of
        // a perfect circle, so "Forma do corpo: Elipse" is actually visible.
        sf::CircleShape body(radius, 40);
        body.setOrigin(radius, radius);
        body.setPosition(position);
        body.setFillColor(base);
        body.setOutlineThickness(std::max(0.8F, camera.zoom() * 0.35F));
        body.setOutlineColor(darken(base, 0.55F));
        if (agents.bodyShapeAt(i) == simulation::BodyShapeCode::Ellipse)
        {
            body.setRotation(static_cast<float>(angle * 180.0 / 3.14159265358979));
            body.setScale(1.35F, 0.72F);  // long axis along heading
        }
        target.draw(body);

        // Inner lightness gradient (two soft concentric discs).
        if (radius >= 3.5F)
        {
            const float ir = radius * 0.55F;
            sf::CircleShape inner(ir, 24);
            inner.setOrigin(ir, ir);
            inner.setPosition(position.x - radius * 0.12F, position.y - radius * 0.14F);
            inner.setFillColor(sf::Color(lighten(base, 0.22F).r,
                                          lighten(base, 0.22F).g,
                                          lighten(base, 0.22F).b, 130));
            target.draw(inner);
        }

        // Phase 22.1: oriented triangle "head" instead of a circle so the
        // viewer can read agent direction at a glance.
        const float headLen = std::max(2.0F, radius * 1.05F);
        const float headWidth = std::max(1.4F, radius * 0.55F);
        const float c = static_cast<float>(std::cos(angle));
        const float s = static_cast<float>(std::sin(angle));
        sf::ConvexShape head(3);
        head.setPoint(0, {position.x + c * headLen,
                            position.y + s * headLen});
        head.setPoint(1, {position.x + c * radius * 0.30F - s * headWidth,
                            position.y + s * radius * 0.30F + c * headWidth});
        head.setPoint(2, {position.x + c * radius * 0.30F + s * headWidth,
                            position.y + s * radius * 0.30F - c * headWidth});
        head.setFillColor(darken(base, 0.65F));
        head.setOutlineThickness(0.6F);
        head.setOutlineColor(sf::Color(10, 14, 18, 220));
        target.draw(head);

        ++drawn;
    }

    return drawn;
}

std::size_t Renderer::drawSelectionHalos(sf::RenderTarget& target,
                                            const Camera2D& camera,
                                            const simulation::AgentStore& agents,
                                            const SelectionRenderInput& sel) const
{
    if (sel.selectedIds == nullptr || sel.selectedIds->empty()) return 0;
    const sf::Vector2u viewport = target.getSize();
    std::size_t drawn = 0;
    for (const auto id : *sel.selectedIds)
    {
        const auto maybeIdx = agents.indexOf(id);
        if (!maybeIdx.has_value()) continue;
        const std::size_t idx = *maybeIdx;
        if (!agents.aliveAt(idx)) continue;

        const sf::Vector2f position = camera.worldToScreen(toSfml(agents.positionAt(idx)), viewport);
        const float radius = std::max(2.0F, static_cast<float>(agents.radiusAt(idx)) * camera.zoom());
        // Outer halo: a thin bright ring around the agent.
        const float haloR = radius + std::max(2.0F, radius * 0.35F);
        sf::CircleShape halo(haloR, 48);
        halo.setOrigin(haloR, haloR);
        halo.setPosition(position);
        halo.setFillColor(sf::Color(80, 200, 255, 60));
        halo.setOutlineThickness(2.0F);
        halo.setOutlineColor(sf::Color(160, 230, 255, 230));
        target.draw(halo);
        ++drawn;
    }
    return drawn;
}

std::size_t Renderer::drawMarquee(sf::RenderTarget& target,
                                     const Camera2D& camera,
                                     const SelectionRenderInput& sel) const
{
    const sf::Vector2u viewport = target.getSize();
    const sf::Vector2f a = camera.worldToScreen(toSfml(sel.marqueeStartWorld), viewport);
    const sf::Vector2f b = camera.worldToScreen(toSfml(sel.marqueeEndWorld), viewport);
    const sf::Vector2f topLeft{std::min(a.x, b.x), std::min(a.y, b.y)};
    const sf::Vector2f size{std::abs(b.x - a.x), std::abs(b.y - a.y)};
    sf::RectangleShape rect(size);
    rect.setPosition(topLeft);
    rect.setFillColor(sf::Color(90, 170, 250, 35));
    rect.setOutlineColor(sf::Color(150, 210, 255, 230));
    rect.setOutlineThickness(1.5F);
    target.draw(rect);
    return 1U;
}

std::size_t Renderer::drawLasso(sf::RenderTarget& target,
                                   const Camera2D& camera,
                                   const SelectionRenderInput& sel) const
{
    if (sel.lassoPoints == nullptr || sel.lassoPoints->size() < 2U) return 0;
    const sf::Vector2u viewport = target.getSize();
    sf::VertexArray strip(sf::LineStrip, sel.lassoPoints->size() + 1U);
    for (std::size_t i = 0; i < sel.lassoPoints->size(); ++i)
    {
        const sf::Vector2f s = camera.worldToScreen(toSfml((*sel.lassoPoints)[i]), viewport);
        strip[i].position = s;
        strip[i].color = sf::Color(150, 210, 255, 230);
    }
    // close the loop visually
    const sf::Vector2f close = camera.worldToScreen(toSfml(sel.lassoPoints->front()), viewport);
    strip[sel.lassoPoints->size()].position = close;
    strip[sel.lassoPoints->size()].color = sf::Color(150, 210, 255, 230);
    target.draw(strip);
    return sel.lassoPoints->size();
}

void Renderer::drawBrushCursor(sf::RenderTarget& target,
                                  const Camera2D& camera,
                                  const SelectionRenderInput& sel) const
{
    const sf::Vector2u viewport = target.getSize();
    const sf::Vector2f c = camera.worldToScreen(toSfml(sel.brushCursorWorld), viewport);
    const float r = std::max(2.0F, static_cast<float>(sel.brushCursorRadius) * camera.zoom());
    sf::CircleShape ring(r, 48);
    ring.setOrigin(r, r);
    ring.setPosition(c);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(1.4F);
    ring.setOutlineColor(sel.brushIsEraser
                            ? sf::Color(255, 140, 110, 200)
                            : sf::Color(120, 200, 255, 200));
    target.draw(ring);
}
} // namespace agentbiosim::render
