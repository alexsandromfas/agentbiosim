#pragma once

#include "perception/VisionDebug.hpp"
#include "render/Camera2D.hpp"
#include "render/RenderOptions.hpp"
#include "simulation/AgentStore.hpp"
#include "simulation/EntityId.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/ObstacleStore.hpp"
#include "simulation/World.hpp"

#include <SFML/Graphics/RenderTarget.hpp>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace agentbiosim::render
{
struct RenderStats
{
    bool skipped = false;
    std::size_t agentsDrawn = 0;
    std::size_t foodsDrawn = 0;
    std::size_t obstaclesDrawn = 0;
    std::size_t visionRaysDrawn = 0;
    // Phase 22.1: overlay counters so diagnostics can attribute render cost.
    std::size_t selectionHalosDrawn = 0;
    std::size_t marqueeRectsDrawn = 0;
    std::size_t lassoSegmentsDrawn = 0;
};

// Phase 22.1: small POD describing selection overlays the renderer should
// draw. Kept ABI-free of UI types so headless callers can construct it from
// any source.
struct SelectionRenderInput
{
    const std::vector<simulation::EntityId>* selectedIds = nullptr;
    // Marquee/lasso preview (active while dragging).
    bool marqueeActive = false;
    simulation::Vec2 marqueeStartWorld{};
    simulation::Vec2 marqueeEndWorld{};
    bool lassoActive = false;
    const std::vector<simulation::Vec2>* lassoPoints = nullptr;
    // Optional: draw a brush ring at this world position (cursor preview).
    bool brushCursorActive = false;
    simulation::Vec2 brushCursorWorld{};
    double brushCursorRadius = 0.0;
    bool brushIsEraser = false;
};

// Render interpolation (render_interpolation_enabled): when on, agents are drawn
// at lerp(previous-step position, current position, alpha) so movement is smooth
// even at low physics rates. `prevPositions` maps agent id -> position right before
// the most recent physics step; ids absent from it (newborns) draw at the live
// position. Purely visual: the simulation state is never read or written here.
struct RenderInterpolation
{
    bool enabled = false;
    float alpha = 0.0F;
    const std::unordered_map<std::uint64_t, simulation::Vec2>* prevPositions = nullptr;
};

class Renderer
{
public:
    [[nodiscard]] RenderStats render(sf::RenderTarget& target,
                                     const Camera2D& camera,
                                     const simulation::World& world,
                                     const simulation::AgentStore& agents,
                                     const simulation::FoodStore& foods,
                                     const RenderOptions& options,
                                     const perception::VisionDebugData* visionDebug = nullptr,
                                     const simulation::ObstacleStore* obstacles = nullptr,
                                     const SelectionRenderInput* selection = nullptr,
                                     const RenderInterpolation* interpolation = nullptr) const;

private:
    void drawBackground(sf::RenderTarget& target, const RenderOptions& options) const;
    void drawWorldBoundary(sf::RenderTarget& target,
                           const Camera2D& camera,
                           const simulation::World& world,
                           const RenderOptions& options) const;
    void drawSpatialGrid(sf::RenderTarget& target,
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
                           const RenderOptions& options,
                           const RenderInterpolation* interpolation) const;
    std::size_t drawObstacles(sf::RenderTarget& target,
                               const Camera2D& camera,
                               const simulation::ObstacleStore& obstacles,
                               const RenderOptions& options) const;
    // Fase 32.1: o desenho da visao do agente selecionado vive em
    // render/VisionOverlay (mode-aware: cunhas no setor, feixes no raycast).
    // Phase 22.1: separate pass for selection halos so they sit above the
    // agents but under UI panels and overlays.
    std::size_t drawSelectionHalos(sf::RenderTarget& target,
                                     const Camera2D& camera,
                                     const simulation::AgentStore& agents,
                                     const SelectionRenderInput& sel) const;
    std::size_t drawMarquee(sf::RenderTarget& target,
                              const Camera2D& camera,
                              const SelectionRenderInput& sel) const;
    std::size_t drawLasso(sf::RenderTarget& target,
                            const Camera2D& camera,
                            const SelectionRenderInput& sel) const;
    void drawBrushCursor(sf::RenderTarget& target,
                          const Camera2D& camera,
                          const SelectionRenderInput& sel) const;
};
} // namespace agentbiosim::render
