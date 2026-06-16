#pragma once

#include "render/Camera2D.hpp"
#include "render/Theme.hpp"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/System/Vector2.hpp>

// Renders a Theme as a SKIN over the live simulation, entirely with native SFML
// primitives (vector-crisp at any zoom). The theme geometry is normalised to the
// substrate; here it is mapped to world units as substrateCenter + pos*substrateRadius,
// so the composition scales with the substrate (growing it scales the decorations
// together — no overlap — and the camera reframes).
//
// Drawing is split so the simulation entities sit between the layers:
//   drawThemeBehind(...)  -> background + decorations + dish fill
//   <renderer draws organisms/food inside the dish>
//   drawThemeFront(...)   -> dish rim + stars + grain (on top)
//
// homeCenter (world) maps to the screen centre at homeZoom — the "home" framing where
// every layer aligns like the SVG. As the camera zooms/pans away from home, layers
// respond by their depth (the dish is depth 1, i.e. exactly the camera). Everything
// uses the same camera convention as the renderer (viewport centre = window centre), so
// the decorations, the dish and the organisms stay aligned.
namespace agentbiosim::render
{
struct ThemeFrame
{
    sf::Vector2f substrateCenter; // world
    float substrateRadius = 1.0F; // world
    sf::Vector2f homeCenter;      // world (parallax anchor / home framing centre)
    float homeZoom = 1.0F;
};

void drawThemeBehind(sf::RenderTarget& target, const Camera2D& camera, const Theme& theme,
                     float timeSeconds, const ThemeFrame& frame);
void drawThemeFront(sf::RenderTarget& target, const Camera2D& camera, const Theme& theme,
                    const ThemeFrame& frame);
} // namespace agentbiosim::render
