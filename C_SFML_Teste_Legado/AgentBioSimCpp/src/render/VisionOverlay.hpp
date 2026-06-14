#pragma once

#include "perception/VisionDebug.hpp"

#include <cstddef>

namespace sf { class RenderTarget; }

namespace agentbiosim::render
{
class Camera2D;

// Fase 32.1: pretty, MODE-AWARE drawing of the selected agent's vision.
//
// It is a pure render-side consumer of the engine's read-only
// `perception::VisionDebugData` (filled on-demand for a single target agent), so
// it never touches the simulation. Two looks, chosen by `debug.mode`:
//   - Sector/bin  -> filled angular wedges + distance arcs (polar grid), each
//                    wedge tinted by its bin activation/seen color.
//   - Single/Fullbody (raycast) -> a faint FOV cone + per-ray beams colored by the
//                    seen object, brighter/longer with activation, with hit dots.
//
// Drawn in screen space via `camera.worldToScreen`, batched into a few
// sf::VertexArray draws (no per-ray allocation). Returns the primitive count for
// render stats.
[[nodiscard]] std::size_t drawVisionOverlay(sf::RenderTarget& target,
                                            const Camera2D& camera,
                                            const perception::VisionDebugData& debug);
} // namespace agentbiosim::render
