#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>
#include <vector>

// Theme backdrop (teste de tema): a faithful, fully-procedural reproduction of the
// theme SVGs (Assets/themes/*.svg) drawn with native SFML primitives, so it stays
// vector-crisp at any zoom (the SVG is just gradients + circles). The Theme struct is
// pure data (no SFML rendering, no engine) describing each layer in the SVG's own
// coordinate space (the viewBox). render::ThemeBackdrop turns it into draw calls with
// per-layer parallax. See render/ThemeBackdrop.* for the rendering.
namespace agentbiosim::render
{
// A single colour stop of a gradient. offset in [0,1].
struct GradientStop
{
    float offset = 0.0F;
    sf::Color color{};
};

// A multi-stop radial fill (0 = centre, 1 = rim), used by the orbs and the dish.
// `focal` offsets the bright centre as a fraction of the radius (SVG cx/cy - 0.5),
// giving the lit-from-one-side look; `spread` scales how far the gradient reaches
// past the nominal radius so the soft edge isn't clipped.
struct RadialFill
{
    std::vector<GradientStop> stops;
    sf::Vector2f focal{0.0F, 0.0F};
    float spread = 1.0F;
};

// A soft glowing orb (one of the colour blobs around the dish).
struct ThemeOrb
{
    sf::Vector2f pos{};
    float radius = 0.0F;
    int fill = 0;       // index into Theme::fills
    float depth = 0.0F; // parallax depth (0 = far/fixed, 1 = on the dish plane)
};

// A decorative dashed "orbit" ring with two highlight dots, optionally rotated.
struct ThemeRing
{
    sf::Vector2f center{};
    float radius = 0.0F;
    float rotationDeg = 0.0F;
    sf::Vector2f dotA{};
    float dotARadius = 0.0F;
    sf::Vector2f dotB{};
    float dotBRadius = 0.0F;
    float depth = 0.0F;
};

// A dashed free stroke (e.g. the little quadratic curve), pre-sampled into points.
struct ThemeStroke
{
    std::vector<sf::Vector2f> points;
    float depth = 0.0F;
};

// A tiny sparkle.
struct ThemeStar
{
    sf::Vector2f pos{};
    float radius = 0.0F;
    sf::Color color{};
};

// The substrate "dish": a radial fill plus two rim strokes.
struct ThemeDish
{
    sf::Vector2f center{};
    float radius = 0.0F;
    int fill = 0;
    sf::Color rim1Color{};
    float rim1Width = 0.0F;
    float rim2Radius = 0.0F;
    sf::Color rim2Color{};
    float rim2Width = 0.0F;
    float depth = 1.0F;
};

struct Theme
{
    bool active = false;

    // IMPORTANT: all positions/radii in a Theme are NORMALISED to the substrate:
    // the dish centre is the origin (0,0) and the dish radius is 1. At render time
    // they are mapped to world units as substrateCenter + pos * substrateRadius, so the
    // whole composition scales with the substrate — growing the substrate scales the
    // decorations together (no overlap), and the camera reframes automatically.
    sf::Vector2f frameHalfExtent{2.089F, 1.175F}; // SVG canvas half-size, in dish radii

    // Background: multi-stop linear gradient running screen top-right -> bottom-left
    // (drawn full-screen, fixed).
    std::vector<GradientStop> background;

    std::vector<RadialFill> fills; // palette referenced by orbs/dish
    std::vector<ThemeOrb> orbs;
    std::vector<ThemeRing> rings;
    std::vector<ThemeStroke> strokes;
    ThemeDish dish;
    std::vector<ThemeStar> stars;

    sf::Color ringStroke{255, 255, 255, 128};
    float ringDashSpacing = 0.027F; // normalised spacing between dash dots
    float ringDotRadius = 0.003F;   // normalised dot radius

    float grainOpacity = 0.16F;
};

// The themes, translated from Assets/themes/*.svg into the substrate-normalised space
// described above. All share the orange geometry/parallax; only the colours differ.
[[nodiscard]] Theme orangeTheme();
[[nodiscard]] Theme darkBlueTheme();
[[nodiscard]] Theme lightBlueTheme();
// Dispatch by string id ("orange", "dark_blue", "light_blue"); defaults to orange.
[[nodiscard]] Theme themeById(const std::string& id);
} // namespace agentbiosim::render
