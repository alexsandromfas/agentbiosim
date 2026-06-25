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

// ---- Sector / bins look: a polar GRID (angular bins x distance rings). The faint
// grid always shows the bin structure; the cell (angle x distance band) where an
// object is detected LIGHTS UP with the object color, transparency growing with the
// activation. This mirrors the perception (retinaCount angular bins, subdivisions
// radial bands, near_detail = sqrt spacing). Cheap: a couple of VertexArrays, one
// draw each, and it only runs for the single selected agent.
std::size_t drawSector(sf::RenderTarget& target, const Camera2D& camera,
                       const perception::VisionDebugData& debug, const sf::Vector2u vp)
{
    const double fovRad = debug.fovDegrees * kPi / 180.0;
    const std::size_t binCount = std::max<std::size_t>(1, debug.retinaCount);
    const double binWidth = fovRad / static_cast<double>(binCount);
    const double halfBin = binWidth * 0.5;
    const double radius = std::max(1.0, debug.visionRadius);
    const std::size_t subs = std::max<std::size_t>(1, debug.distanceSubdivisions);
    const bool nearDetail = debug.nearDetail;

    // Radius of the ring boundary at normalized fraction `f` (matches the perception's
    // distance distribution: near_detail packs more bands close to the eye via sqrt).
    const auto ringR = [&](const double f) {
        return nearDetail ? radius * f * f : radius * f;
    };
    // Which distance band a normalized distance falls into (same math as the engine).
    const auto bandOf = [&](double norm) {
        norm = std::clamp(norm, 0.0, 1.0);
        const int b = nearDetail
            ? static_cast<int>(std::floor(std::sqrt(norm) * static_cast<double>(subs)))
            : static_cast<int>(std::floor(norm * static_cast<double>(subs)));
        return std::clamp(b, 0, static_cast<int>(subs) - 1);
    };

    sf::VertexArray cells(sf::Triangles);  // only the lit cells get filled
    sf::VertexArray grid(sf::Lines);       // faint polar grid (always)
    constexpr int kSeg = 4;

    for (const auto& ray : debug.rays)
    {
        const double cAng = std::atan2(ray.dirY, ray.dirX);
        const double a0 = cAng - halfBin;
        const double a1 = cAng + halfBin;
        const double act = std::clamp(ray.activation, 0.0, 1.0);

        // The lit band: the nearest object's distance band (or the inner band for the
        // aggregate modes that do not track a single distance).
        int litBand = -1;
        if (ray.hit)
        {
            litBand = ray.hitDistance >= 0.0 ? bandOf(ray.hitDistance / radius) : 0;
        }
        // Lit color = the detected object's color (bright floor so dark objects still
        // read); alpha grows with activation so nearer/stronger glows more.
        sf::Color lit(130, 200, 250, 0);
        if (ray.hit)
        {
            const double sum = ray.hitColorR + ray.hitColorG + ray.hitColorB;
            if (sum > 0.12)
            {
                lit.r = toByte(ray.hitColorR * 255.0);
                lit.g = toByte(ray.hitColorG * 255.0);
                lit.b = toByte(ray.hitColorB * 255.0);
            }
            lit.a = toByte(70.0 + 170.0 * act);
        }

        for (std::size_t b = 0; b < subs; ++b)
        {
            const double r0 = ringR(static_cast<double>(b) / static_cast<double>(subs));
            const double r1 = ringR(static_cast<double>(b + 1) / static_cast<double>(subs));
            // Concentric ring boundary arc (faint) — shows the longitudinal segmentation.
            const sf::Color arcCol(120, 150, 200, 48);
            for (int s = 0; s < kSeg; ++s)
            {
                const double t0 = a0 + (a1 - a0) * (static_cast<double>(s) / kSeg);
                const double t1 = a0 + (a1 - a0) * (static_cast<double>(s + 1) / kSeg);
                grid.append({w2s(camera, ray.startX + std::cos(t0) * r1, ray.startY + std::sin(t0) * r1, vp), arcCol});
                grid.append({w2s(camera, ray.startX + std::cos(t1) * r1, ray.startY + std::sin(t1) * r1, vp), arcCol});
                if (static_cast<int>(b) == litBand)
                {
                    const sf::Vector2f i0 = w2s(camera, ray.startX + std::cos(t0) * r0, ray.startY + std::sin(t0) * r0, vp);
                    const sf::Vector2f i1 = w2s(camera, ray.startX + std::cos(t1) * r0, ray.startY + std::sin(t1) * r0, vp);
                    const sf::Vector2f o0 = w2s(camera, ray.startX + std::cos(t0) * r1, ray.startY + std::sin(t0) * r1, vp);
                    const sf::Vector2f o1 = w2s(camera, ray.startX + std::cos(t1) * r1, ray.startY + std::sin(t1) * r1, vp);
                    cells.append({i0, lit}); cells.append({o0, lit}); cells.append({o1, lit});
                    cells.append({i0, lit}); cells.append({o1, lit}); cells.append({i1, lit});
                }
            }
        }

        // Radial bin edges (faint) — the angular segmentation structure.
        const sf::Color edge(120, 150, 200, 55);
        const sf::Vector2f eye = w2s(camera, ray.startX, ray.startY, vp);
        grid.append({eye, edge});
        grid.append({w2s(camera, ray.startX + std::cos(a0) * radius, ray.startY + std::sin(a0) * radius, vp), edge});
        grid.append({eye, edge});
        grid.append({w2s(camera, ray.startX + std::cos(a1) * radius, ray.startY + std::sin(a1) * radius, vp), edge});
    }

    target.draw(cells);
    target.draw(grid);
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
