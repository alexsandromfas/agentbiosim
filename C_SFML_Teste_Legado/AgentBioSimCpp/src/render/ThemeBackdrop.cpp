#include "render/ThemeBackdrop.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace agentbiosim::render
{
namespace
{
constexpr float kPi = 3.14159265358979323846F;
// Stars sit on the dish plane (depth 1) and are drawn on top of it, so the white dots
// stay around the substrate and never appear behind it, at any zoom.
constexpr float kStarDepth = 1.0F;

// Fullscreen multi-stop linear gradient running from the screen's top-right corner
// (offset 0) to the bottom-left corner (offset 1), matching the SVG bgGrad. Built as
// a triangle strip of bands perpendicular to that diagonal; the strip is wide enough
// (the screen diagonal on each side) to cover the whole window.
void drawLinearGradient(sf::RenderTarget& target, const std::vector<GradientStop>& stops,
                        const sf::Vector2u vp)
{
    if (stops.size() < 2U) return;
    const float w = static_cast<float>(vp.x);
    const float h = static_cast<float>(vp.y);
    const sf::Vector2f start{w, 0.0F};
    const sf::Vector2f axis{-w, h};
    const float diag = std::sqrt(w * w + h * h);
    sf::Vector2f perp{h, w};
    const float plen = std::sqrt(perp.x * perp.x + perp.y * perp.y);
    perp = {perp.x / plen * diag, perp.y / plen * diag};

    // Extend slightly beyond [0,1] so the fill covers past the two diagonal corners.
    std::vector<GradientStop> ext;
    ext.reserve(stops.size() + 2U);
    ext.push_back({-0.1F, stops.front().color});
    for (const auto& s : stops) ext.push_back(s);
    ext.push_back({1.1F, stops.back().color});

    sf::VertexArray strip(sf::TriangleStrip, ext.size() * 2U);
    std::size_t v = 0;
    for (const auto& s : ext)
    {
        const sf::Vector2f p{start.x + axis.x * s.offset, start.y + axis.y * s.offset};
        strip[v].position = {p.x + perp.x, p.y + perp.y};
        strip[v].color = s.color;
        ++v;
        strip[v].position = {p.x - perp.x, p.y - perp.y};
        strip[v].color = s.color;
        ++v;
    }
    target.draw(strip);
}

// Multi-stop radial fill with screen radius R0. The bright centre is offset by the
// fill's focal (fraction of radius) and the gradient reaches R0*spread, approximating
// the SVG's off-centre cx/cy + r. Drawn as one triangle strip per colour band so all
// stops interpolate smoothly (vector-crisp).
void drawRadialFill(sf::RenderTarget& target, const sf::Vector2f c0, const float R0,
                    const RadialFill& fill)
{
    if (fill.stops.size() < 2U || R0 <= 0.0F) return;
    const sf::Vector2f c{c0.x + fill.focal.x * R0, c0.y + fill.focal.y * R0};
    const float R = R0 * (fill.spread > 0.0F ? fill.spread : 1.0F);
    constexpr int kSeg = 72;
    for (std::size_t b = 0; b + 1U < fill.stops.size(); ++b)
    {
        const float r0 = fill.stops[b].offset * R;
        const float r1 = fill.stops[b + 1U].offset * R;
        const sf::Color c0 = fill.stops[b].color;
        const sf::Color c1 = fill.stops[b + 1U].color;
        sf::VertexArray band(sf::TriangleStrip, static_cast<std::size_t>(kSeg + 1) * 2U);
        std::size_t v = 0;
        for (int k = 0; k <= kSeg; ++k)
        {
            const float a = 2.0F * kPi * static_cast<float>(k) / static_cast<float>(kSeg);
            const float ca = std::cos(a);
            const float sa = std::sin(a);
            band[v].position = {c.x + ca * r0, c.y + sa * r0};
            band[v].color = c0;
            ++v;
            band[v].position = {c.x + ca * r1, c.y + sa * r1};
            band[v].color = c1;
            ++v;
        }
        target.draw(band);
    }
}

void drawDisc(sf::RenderTarget& target, const sf::Vector2f c, const float r, const sf::Color color)
{
    if (r <= 0.05F) return;
    sf::CircleShape disc(r, 24);
    disc.setOrigin(r, r);
    disc.setPosition(c);
    disc.setFillColor(color);
    target.draw(disc);
}

void drawRing(sf::RenderTarget& target, const sf::Vector2f c, const float r, const float width,
              const sf::Color color)
{
    if (r <= 0.5F) return;
    sf::CircleShape ring(r, 96);
    ring.setOrigin(r, r);
    ring.setPosition(c);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(color);
    ring.setOutlineThickness(std::max(0.5F, width));
    target.draw(ring);
}

// A reusable subtle film-grain texture (random specks). Drawn additively at low alpha
// so it textures the image without graying the vibrant palette (an approximation of
// the SVG's feTurbulence/overlay grain).
const sf::Texture& grainTexture()
{
    static sf::Texture tex;
    static bool built = false;
    if (!built)
    {
        constexpr unsigned int kS = 256;
        sf::Image img;
        img.create(kS, kS, sf::Color(0, 0, 0, 0));
        std::uint32_t seed = 0x9E3779B9U;
        for (unsigned int y = 0; y < kS; ++y)
        {
            for (unsigned int x = 0; x < kS; ++x)
            {
                seed ^= seed << 13;
                seed ^= seed >> 17;
                seed ^= seed << 5;
                const std::uint32_t r = seed % 1000U;
                std::uint8_t a = 0;
                if (r < 70U) a = static_cast<std::uint8_t>(40 + (seed % 60U)); // sparse specks
                const std::uint8_t g = static_cast<std::uint8_t>(180 + (seed % 75U));
                img.setPixel(x, y, sf::Color(g, g, g, a));
            }
        }
        tex.loadFromImage(img);
        tex.setRepeated(true);
        built = true;
    }
    return tex;
}
} // namespace

namespace
{
// Sets the target view to match the framebuffer (the default view stays pinned to the
// window's creation size, which throws the scene off on a maximized/resized window).
void useFramebufferView(sf::RenderTarget& target)
{
    const sf::Vector2u vp = target.getSize();
    if (vp.x == 0U || vp.y == 0U) return;
    target.setView(sf::View(sf::FloatRect(0.0F, 0.0F,
                                            static_cast<float>(vp.x), static_cast<float>(vp.y))));
}

// Builds the normalised-theme -> screen mapping for a frame. Theme positions are in
// substrate units (dish centre = origin, radius = 1); they map to world coords
// (substrateCenter + p*substrateRadius) and then to the screen with per-depth parallax
// against the home framing. Depth 1 == the camera exactly (so the dish and the
// organisms, drawn by the renderer with the same camera, stay aligned).
struct Mapper
{
    sf::Vector2f vc;
    sf::Vector2f camCenter;
    float camZoom;
    sf::Vector2f homeCenter;
    float homeZoom;
    sf::Vector2f substrateCenter;
    float substrateRadius;

    [[nodiscard]] float layerZoom(const float d) const { return homeZoom + (camZoom - homeZoom) * d; }
    [[nodiscard]] sf::Vector2f screen(const sf::Vector2f norm, const float d) const
    {
        const sf::Vector2f world{substrateCenter.x + norm.x * substrateRadius,
                                 substrateCenter.y + norm.y * substrateRadius};
        const float zl = layerZoom(d);
        const sf::Vector2f cl{homeCenter.x + (camCenter.x - homeCenter.x) * d,
                              homeCenter.y + (camCenter.y - homeCenter.y) * d};
        return sf::Vector2f{(world.x - cl.x) * zl + vc.x, (world.y - cl.y) * zl + vc.y};
    }
    // Screen radius of a normalised radius at depth d (in world units * layer zoom).
    [[nodiscard]] float radius(const float normR, const float d) const
    {
        return normR * substrateRadius * layerZoom(d);
    }
};

Mapper makeMapper(const sf::RenderTarget& target, const Camera2D& camera, const ThemeFrame& frame)
{
    const sf::Vector2u vp = target.getSize();
    Mapper m;
    m.vc = {static_cast<float>(vp.x) * 0.5F, static_cast<float>(vp.y) * 0.5F};
    m.camCenter = camera.center();
    m.camZoom = camera.zoom();
    m.homeCenter = frame.homeCenter;
    m.homeZoom = frame.homeZoom;
    m.substrateCenter = frame.substrateCenter;
    m.substrateRadius = frame.substrateRadius;
    return m;
}
} // namespace

void drawThemeBehind(sf::RenderTarget& target, const Camera2D& camera, const Theme& theme,
                     const float timeSeconds, const ThemeFrame& frame)
{
    useFramebufferView(target);
    const sf::Vector2u vp = target.getSize();
    if (vp.x == 0U || vp.y == 0U) return;
    const Mapper m = makeMapper(target, camera, frame);

    // 1. Background gradient (fixed, fills the screen).
    drawLinearGradient(target, theme.background, vp);

    // 2. Soft orbs (gentle autonomous drift, in normalised units, so it feels alive).
    for (std::size_t i = 0; i < theme.orbs.size(); ++i)
    {
        const auto& orb = theme.orbs[i];
        if (orb.fill < 0 || orb.fill >= static_cast<int>(theme.fills.size())) continue;
        const float phase = static_cast<float>(i) * 1.7F;
        const sf::Vector2f drift{std::sin(timeSeconds * 0.18F + phase) * 0.017F,
                                 std::cos(timeSeconds * 0.15F + phase) * 0.012F};
        const sf::Vector2f c = m.screen({orb.pos.x + drift.x, orb.pos.y + drift.y}, orb.depth);
        drawRadialFill(target, c, m.radius(orb.radius, orb.depth), theme.fills[orb.fill]);
    }

    // 3. Dish fill (the substrate interior; the organisms draw on top of it).
    if (theme.dish.fill >= 0 && theme.dish.fill < static_cast<int>(theme.fills.size()))
    {
        const sf::Vector2f c = m.screen(theme.dish.center, theme.dish.depth);
        drawRadialFill(target, c, m.radius(theme.dish.radius, theme.dish.depth),
                       theme.fills[theme.dish.fill]);
    }
}

void drawThemeFront(sf::RenderTarget& target, const Camera2D& camera, const Theme& theme,
                    const ThemeFrame& frame)
{
    useFramebufferView(target);
    const sf::Vector2u vp = target.getSize();
    if (vp.x == 0U || vp.y == 0U) return;
    const Mapper m = makeMapper(target, camera, frame);

    // 4. Dish rim strokes (the world boundary), on top of the organisms.
    {
        const sf::Vector2f c = m.screen(theme.dish.center, theme.dish.depth);
        drawRing(target, c, m.radius(theme.dish.radius, theme.dish.depth),
                 m.radius(theme.dish.rim1Width, theme.dish.depth), theme.dish.rim1Color);
        drawRing(target, c, m.radius(theme.dish.rim2Radius, theme.dish.depth),
                 m.radius(theme.dish.rim2Width, theme.dish.depth), theme.dish.rim2Color);
    }

    // 5. Sparkle stars around the dish: positioned on the dish plane (follow it), FIXED
    // pixel size regardless of zoom — like distant stars.
    for (const auto& star : theme.stars)
    {
        drawDisc(target, m.screen(star.pos, kStarDepth), star.radius, star.color);
    }

    // 6. Grain overlay (subtle, additive).
    if (theme.grainOpacity > 0.0F)
    {
        const sf::Texture& tex = grainTexture();
        sf::Sprite grain(tex);
        grain.setTextureRect(sf::IntRect(0, 0, static_cast<int>(vp.x), static_cast<int>(vp.y)));
        grain.setColor(sf::Color(255, 255, 255,
                                 static_cast<std::uint8_t>(theme.grainOpacity * 255.0F)));
        target.draw(grain, sf::RenderStates(sf::BlendAdd));
    }
}
} // namespace agentbiosim::render
