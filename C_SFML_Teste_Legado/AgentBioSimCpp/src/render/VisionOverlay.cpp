#include "render/VisionOverlay.hpp"

#include "render/Camera2D.hpp"

#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>

namespace agentbiosim::render
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

sf::Uint8 toByte(const double v)
{
    return static_cast<sf::Uint8>(std::clamp(v, 0.0, 255.0));
}

// Color seen by a ray/bin: the hit object's color when it hit, else a cool
// neutral tint. Alpha grows with activation so empty directions stay faint.
sf::Color seenColor(const perception::VisionRayDebug& ray, const double minAlpha,
                    const double maxAlpha)
{
    const double act = std::clamp(ray.activation, 0.0, 1.0);
    const double alpha = minAlpha + (maxAlpha - minAlpha) * act;
    if (ray.hit)
    {
        return {toByte(ray.hitColorR * 255.0), toByte(ray.hitColorG * 255.0),
                toByte(ray.hitColorB * 255.0), toByte(alpha)};
    }
    return {120, 150, 200, toByte(std::min(alpha, 70.0))};
}

sf::Vector2f w2s(const Camera2D& camera, const double x, const double y,
                 const sf::Vector2u vp)
{
    return camera.worldToScreen({static_cast<float>(x), static_cast<float>(y)}, vp);
}

// ---- Sector / bin look: filled angular wedges + a faint polar grid ----------
std::size_t drawSector(sf::RenderTarget& target, const Camera2D& camera,
                       const perception::VisionDebugData& debug, const sf::Vector2u vp)
{
    const std::size_t binCount = std::max<std::size_t>(1, debug.retinaCount);
    const double fovRad = debug.fovDegrees * kPi / 180.0;
    const double binWidth = fovRad / static_cast<double>(binCount);
    const double halfBin = binWidth * 0.5;
    const double radius = std::max(1.0, debug.visionRadius);

    sf::VertexArray wedges(sf::Triangles);
    sf::VertexArray grid(sf::Lines);

    for (const auto& ray : debug.rays)
    {
        const double cAng = std::atan2(ray.dirY, ray.dirX);
        const double act = std::clamp(ray.activation, 0.0, 1.0);
        // Active bins reach farther so the "where is something" reads at a glance.
        const double reach = radius * (0.30 + 0.70 * act);
        const sf::Color fill = seenColor(ray, 28.0, 200.0);
        const sf::Vector2f apex = w2s(camera, ray.startX, ray.startY, vp);

        constexpr int kSeg = 5;
        const double a0 = cAng - halfBin;
        const double a1 = cAng + halfBin;
        for (int s = 0; s < kSeg; ++s)
        {
            const double t0 = a0 + (a1 - a0) * (static_cast<double>(s) / kSeg);
            const double t1 = a0 + (a1 - a0) * (static_cast<double>(s + 1) / kSeg);
            const sf::Vector2f p0 = w2s(camera, ray.startX + std::cos(t0) * reach,
                                        ray.startY + std::sin(t0) * reach, vp);
            const sf::Vector2f p1 = w2s(camera, ray.startX + std::cos(t1) * reach,
                                        ray.startY + std::sin(t1) * reach, vp);
            wedges.append({apex, fill});
            wedges.append({p0, fill});
            wedges.append({p1, fill});
        }

        // Thin bin boundary at full radius (structure of the polar grid).
        const sf::Color edge(90, 120, 170, 70);
        grid.append({apex, edge});
        grid.append({w2s(camera, ray.startX + std::cos(a1) * radius,
                         ray.startY + std::sin(a1) * radius, vp), edge});
    }

    target.draw(grid);
    target.draw(wedges);
    return debug.rays.size();
}

// ---- Raycast look: faint FOV cone + colored beams + hit dots -----------------
void appendDot(sf::VertexArray& tris, const sf::Vector2f c, const float r, const sf::Color col)
{
    // Small diamond (4 triangles share the center) — cheap round-ish marker.
    const sf::Vector2f n{c.x, c.y - r};
    const sf::Vector2f e{c.x + r, c.y};
    const sf::Vector2f s{c.x, c.y + r};
    const sf::Vector2f w{c.x - r, c.y};
    tris.append({c, col}); tris.append({n, col}); tris.append({e, col});
    tris.append({c, col}); tris.append({e, col}); tris.append({s, col});
    tris.append({c, col}); tris.append({s, col}); tris.append({w, col});
    tris.append({c, col}); tris.append({w, col}); tris.append({n, col});
}

std::size_t drawRaycast(sf::RenderTarget& target, const Camera2D& camera,
                        const perception::VisionDebugData& debug, const sf::Vector2u vp)
{
    sf::VertexArray cone(sf::Triangles);
    sf::VertexArray beams(sf::Lines);
    sf::VertexArray dots(sf::Triangles);

    const float dotR = std::max(2.0F, 4.0F * camera.zoom());

    for (const auto& ray : debug.rays)
    {
        const sf::Vector2f start = w2s(camera, ray.startX, ray.startY, vp);
        const double endX = ray.hit ? ray.hitX : ray.startX + ray.dirX * ray.maxLength;
        const double endY = ray.hit ? ray.hitY : ray.startY + ray.dirY * ray.maxLength;
        const sf::Vector2f end = w2s(camera, endX, endY, vp);

        // Faint cone fill from the eye to the ray's far point (low alpha so the
        // overlap builds a soft vision field).
        const sf::Color coneCol(90, 140, 210, 22);
        cone.append({start, coneCol});
        cone.append({end, coneCol});
        cone.append({w2s(camera, ray.startX, ray.startY, vp), coneCol});

        const sf::Color beam = seenColor(ray, 60.0, 235.0);
        beams.append({start, beam});
        beams.append({end, beam});

        if (ray.hit)
        {
            appendDot(dots, end, dotR, beam);
        }
    }

    target.draw(cone);
    target.draw(beams);
    target.draw(dots);
    return debug.rays.size();
}
} // namespace

std::size_t drawVisionOverlay(sf::RenderTarget& target, const Camera2D& camera,
                              const perception::VisionDebugData& debug)
{
    if (!debug.active || debug.rays.empty())
    {
        return 0;
    }
    const sf::Vector2u vp = target.getSize();
    if (debug.mode == perception::VisionMode::Sector)
    {
        return drawSector(target, camera, debug, vp);
    }
    return drawRaycast(target, camera, debug, vp);
}
} // namespace agentbiosim::render
