#include "render/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace agentbiosim::render
{
namespace
{
// rgba helper (alpha as 0..255).
sf::Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    return sf::Color(r, g, b, a);
}

// Parallax depths (0 = far/almost fixed, 1 = on the dish plane). The orbs sit ALMOST
// on the dish plane so they move with the substrate (subtle parallax, never sliding
// far behind it); individual orbs vary slightly around kDepthOrbs. Stars are locked to
// the dish plane in the backdrop. The dish is the focal plane.
constexpr float kDepthOrbs = 0.88F;
constexpr float kDepthDish = 1.0F;
} // namespace

// A theme's colour palette. The GEOMETRY (orb positions, dish, stars, parallax) is
// shared by every theme via buildTheme(); only these colours change between themes, so
// a new theme = one palette (exactly the request: copy the orange theme, swap colours).
struct ThemePalette
{
    std::vector<GradientStop> background; // top-right -> bottom-left
    RadialFill dishFill;                  // [0]
    RadialFill orbFill[4];                // [1..4], referenced by the 8 orbs
    sf::Color rimOuter;                   // bold outer rim (just outside the world)
    sf::Color rimInner;                   // inner rim = world boundary
};

Theme buildTheme(const ThemePalette& p)
{
    Theme t;
    // Geometry NORMALISED to the substrate (dish centre = origin, radius = 1). Converted
    // from the SVG (dish (858,480), r=406): normalised = (svg - 858,480) / 406. Shared
    // by all themes so the composition scales with the substrate.
    t.frameHalfExtent = {2.089F, 1.175F}; // = (848,477)/406, the SVG canvas half-size
    t.background = p.background;

    // Fill palette: 0=dish, 1..4 = orb fills (the 8 orbs reference these). The orb edge
    // is the SVG circle clipping the r="60%" gradient at ~0.2 alpha (visible soft edge),
    // baked into each fill's last stop.
    t.fills.push_back(p.dishFill);
    t.fills.push_back(p.orbFill[0]);
    t.fills.push_back(p.orbFill[1]);
    t.fills.push_back(p.orbFill[2]);
    t.fills.push_back(p.orbFill[3]);

    // Orbs (normalised cx, cy, r, fill index). Depths vary slightly for subtle parallax.
    t.orbs = {
        {{1.7414F, -1.0468F}, 0.6773F, 1, kDepthOrbs + 0.04F},
        {{1.3842F, -0.7512F}, 0.3695F, 1, kDepthOrbs - 0.05F},
        {{1.9877F, -0.3325F}, 0.4926F, 2, kDepthOrbs},
        {{-2.1010F, -0.0123F}, 0.5296F, 3, kDepthOrbs + 0.05F},
        {{-1.7438F, 0.8251F}, 0.4064F, 3, kDepthOrbs - 0.04F},
        {{1.7906F, 0.5542F}, 0.6773F, 2, kDepthOrbs + 0.03F},
        {{-2.0764F, -1.1700F}, 0.6158F, 4, kDepthOrbs - 0.03F},
        {{-1.6207F, -0.8621F}, 0.3202F, 1, kDepthOrbs + 0.06F},
    };

    // Dish (substrate). World boundary (radius 1) = the INNER rim, where organisms stop;
    // the fill + bold outer rim sit slightly outside (1.025), forming a border band.
    t.dish.center = {0.0F, 0.0F};
    t.dish.radius = 1.025F;
    t.dish.fill = 0;
    t.dish.rim1Color = p.rimOuter;
    t.dish.rim1Width = 0.0074F;
    t.dish.rim2Radius = 1.0F;
    t.dish.rim2Color = p.rimInner;
    t.dish.rim2Width = 0.0039F;
    t.dish.depth = kDepthDish;

    // Sparkles AROUND the dish (rejection-sampled in the canvas frame, outside the dish).
    // Fixed pixel size on screen; positions normalised. Same seed -> same layout per theme.
    {
        std::uint32_t s = 0x1234567U;
        const auto rnd = [&s]() {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return static_cast<float>(s % 100000U) / 100000.0F;
        };
        constexpr int kStars = 34;
        const float hx = t.frameHalfExtent.x;
        const float hy = t.frameHalfExtent.y;
        int placed = 0;
        for (int guard = 0; placed < kStars && guard < 4000; ++guard)
        {
            const float x = (rnd() * 2.0F - 1.0F) * hx;
            const float y = (rnd() * 2.0F - 1.0F) * hy;
            if (x * x + y * y < 1.06F * 1.06F) continue;
            const float sizePx = 1.6F + rnd() * 2.6F;
            const auto a = static_cast<std::uint8_t>(120.0F + rnd() * 135.0F);
            t.stars.push_back({{x, y}, sizePx, rgba(255, 255, 255, a)});
            ++placed;
        }
    }

    t.grainOpacity = 0.16F;
    t.active = true;
    return t;
}

// Each orb/dish fill maps the SVG stops (0% -> 0, 55% -> 0.66, near-edge -> 1.0).
ThemePalette orangePalette()
{
    ThemePalette p;
    p.background = {{0.00F, rgba(246, 144, 73)}, {0.38F, rgba(243, 106, 62)},
                    {0.72F, rgba(240, 83, 63)}, {1.00F, rgba(237, 74, 87)}};
    p.dishFill = {{{0.00F, rgba(251, 183, 140, 158)}, {0.65F, rgba(248, 160, 106, 128)},
                   {1.00F, rgba(244, 144, 92, 107)}}};
    p.orbFill[0] = {{{0.00F, rgba(251, 175, 198, 153)}, {0.66F, rgba(249, 142, 132, 107)},
                     {1.00F, rgba(248, 149, 78, 51)}}};
    p.orbFill[1] = {{{0.00F, rgba(255, 198, 126, 140)}, {0.66F, rgba(251, 162, 88, 92)},
                     {1.00F, rgba(245, 136, 64, 41)}}};
    p.orbFill[2] = {{{0.00F, rgba(247, 154, 192, 173)}, {0.66F, rgba(244, 131, 154, 112)},
                     {1.00F, rgba(242, 107, 114, 46)}}};
    p.orbFill[3] = {{{0.00F, rgba(252, 184, 156, 140)}, {0.66F, rgba(248, 145, 110, 92)},
                     {1.00F, rgba(243, 122, 82, 38)}}};
    p.rimOuter = rgba(255, 255, 255, 230);
    p.rimInner = rgba(255, 255, 255, 150);
    return p;
}

ThemePalette darkBluePalette()
{
    ThemePalette p;
    p.background = {{0.00F, rgba(12, 30, 72)}, {0.50F, rgba(10, 24, 56)}, {1.00F, rgba(6, 15, 40)}};
    p.dishFill = {{{0.00F, rgba(31, 65, 128, 102)}, {0.65F, rgba(22, 50, 97, 82)},
                   {1.00F, rgba(16, 42, 82, 71)}}};
    p.orbFill[0] = {{{0.00F, rgba(46, 94, 176, 140)}, {0.66F, rgba(30, 66, 136, 92)},
                     {1.00F, rgba(20, 47, 98, 36)}}};   // orbBlue
    p.orbFill[1] = {{{0.00F, rgba(58, 79, 176, 128)}, {0.66F, rgba(40, 58, 134, 82)},
                     {1.00F, rgba(27, 42, 96, 31)}}};   // orbIndigo
    p.orbFill[2] = {{{0.00F, rgba(61, 114, 200, 184)}, {0.66F, rgba(36, 76, 146, 112)},
                     {1.00F, rgba(22, 49, 95, 41)}}};   // orbDeep
    p.orbFill[3] = {{{0.00F, rgba(46, 94, 176, 140)}, {0.66F, rgba(30, 66, 136, 92)},
                     {1.00F, rgba(20, 47, 98, 36)}}};   // (orbBlue reused)
    p.rimOuter = rgba(126, 190, 255, 242); // #7EBEFF
    p.rimInner = rgba(77, 168, 255, 128);  // #4DA8FF
    return p;
}

ThemePalette lightBluePalette()
{
    ThemePalette p;
    p.background = {{0.00F, rgba(53, 207, 194)}, {0.34F, rgba(69, 195, 210)},
                    {0.70F, rgba(90, 147, 221)}, {1.00F, rgba(108, 132, 216)}};
    p.dishFill = {{{0.00F, rgba(212, 237, 248, 133)}, {0.65F, rgba(187, 221, 240, 112)},
                   {1.00F, rgba(168, 208, 234, 97)}}};
    p.orbFill[0] = {{{0.00F, rgba(187, 164, 234, 158)}, {0.66F, rgba(156, 134, 222, 107)},
                     {1.00F, rgba(132, 116, 212, 46)}}};  // orbPurple
    p.orbFill[1] = {{{0.00F, rgba(167, 203, 242, 133)}, {0.66F, rgba(134, 168, 230, 87)},
                     {1.00F, rgba(110, 143, 224, 36)}}};  // orbBlue
    p.orbFill[2] = {{{0.00F, rgba(222, 198, 238, 153)}, {0.66F, rgba(203, 168, 230, 97)},
                     {1.00F, rgba(183, 144, 220, 36)}}};  // orbLavender
    p.orbFill[3] = {{{0.00F, rgba(156, 240, 224, 199)}, {0.66F, rgba(84, 210, 198, 128)},
                     {1.00F, rgba(60, 196, 190, 51)}}};   // orbTeal
    p.rimOuter = rgba(255, 255, 255, 230);
    p.rimInner = rgba(255, 255, 255, 128);
    return p;
}

Theme orangeTheme() { return buildTheme(orangePalette()); }
Theme darkBlueTheme() { return buildTheme(darkBluePalette()); }
Theme lightBlueTheme() { return buildTheme(lightBluePalette()); }

Theme themeById(const std::string& id)
{
    if (id == "dark_blue") return darkBlueTheme();
    if (id == "light_blue") return lightBlueTheme();
    return orangeTheme();
}
} // namespace agentbiosim::render
