#pragma once

#include "config/ParameterRegistry.hpp"
#include "core/Command.hpp"
#include "ui/UiState.hpp"

#include <cstddef>

namespace agentbiosim::sim
{
class SimulationRunner;
}

namespace agentbiosim::ui
{
// Phase 25: per-frame snapshot the toolbar/menus display. Filled by App from
// engine/render state so ImGuiUi stays a pure view that reads and emits commands.
struct ImGuiFrameInfo
{
    float fps = 0.0F;
    unsigned long long steps = 0;
    std::size_t agents = 0;
    std::size_t foods = 0;
    std::size_t obstacles = 0;
    bool paused = false;
    bool simpleRender = false;
};

// Phase 25: the entire application UI, rebuilt in Dear ImGui — top menu bar,
// toolbar of canvas tools, the persistent left dock (Editor Genetico / Substrato
// / Labels) and the preferences windows. It replaces the hand-rolled SFML UI
// (UiPanel / UiPreferencesPanel / UiLeftDock).
//
// ImGuiUi is a VIEW: it reads the ParameterRegistry, the SimulationRunner
// (const) and UiState, and emits core::Command into the queue. It never mutates
// stores directly. It must be called once per frame between ImGui::SFML::Update
// and ImGui::SFML::Render.
class ImGuiUi
{
public:
    // Width of the left dock in pixels. The world canvas keeps rendering full
    // window behind it; App frames the world to the right of this dock.
    static constexpr float kDockW = 460.0F;

    void draw(const config::ParameterRegistry& registry,
              const sim::SimulationRunner& runner,
              UiState& state,
              core::CommandQueue& queue,
              const ImGuiFrameInfo& info);

    // Height (px) of menu bar + toolbar, measured during the last draw(). App
    // uses it to lay out the canvas/camera under the top strip.
    [[nodiscard]] float topStripHeight() const noexcept { return topStripHeight_; }

private:
    float topStripHeight_ = 0.0F;
};
} // namespace agentbiosim::ui
