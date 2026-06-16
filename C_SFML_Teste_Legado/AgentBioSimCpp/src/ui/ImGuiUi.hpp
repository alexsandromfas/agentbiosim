#pragma once

#include "config/ParameterRegistry.hpp"
#include "core/Command.hpp"
#include "ui/DevWindow.hpp"
#include "ui/UiState.hpp"

#include <SFML/Graphics/Texture.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

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
    // Phase 25.3: lazily load the toolbar icon textures (once, on the first
    // draw, when the GL context is live) and look them up by key. A missing icon
    // resolves to nullptr so the toolbar falls back to a text label.
    void loadIcons();
    [[nodiscard]] const sf::Texture* icon(const char* key) const;

    // Phase 26: retractable inspector for the selected agent (Genoma + Rede
    // Neural tabs). It reads the runner read-only and writes UiState's trace
    // signal so App can drive the engine trace target. No-op when no agent is
    // selected or the panel is closed.
    void drawAgentInspector(const config::ParameterRegistry& registry,
                            const sim::SimulationRunner& runner,
                            UiState& state,
                            core::CommandQueue& queue);

    // Phase 27: metrics charts + per-system profiler table. Reads the runner's
    // MetricsSystem/Profiler (read-only) and toggles the engine knobs via
    // commands. No-op when the window is closed.
    void drawMetricsWindow(const config::ParameterRegistry& registry,
                           const sim::SimulationRunner& runner,
                           UiState& state,
                           core::CommandQueue& queue);

    // Fase 34.1: the left dock is now ONE tab per species (color-tinted ear,
    // editable name <=10 chars, a trailing "+" tab, retractable). Each tab is the
    // genome editor of THAT species, bound to its genome: per-species fields edit
    // live (orange while diverging, Enter applies just that field via
    // CmdSetSpeciesGenomeField); global fields are shown marked. drawSpeciesEditor
    // renders the body of the active species tab; the neural-reset confirmation
    // modal lives here (it erases the species' learning).
    void drawSpeciesDock(const config::ParameterRegistry& registry,
                         const sim::SimulationRunner& runner,
                         UiState& state,
                         core::CommandQueue& queue);
    void drawSpeciesEditor(const config::ParameterRegistry& registry,
                           const sim::SimulationRunner& runner,
                           UiState& state,
                           core::CommandQueue& queue,
                           std::uint32_t speciesId);
    // Fase 34.1: the Substrate controls moved out of the dock into a window
    // opened from the top menu bar (between Agente and Ajuda).
    void drawSubstrateWindow(const config::ParameterRegistry& registry,
                             const sim::SimulationRunner& runner,
                             UiState& state,
                             core::CommandQueue& queue);

    float topStripHeight_ = 0.0F;
    // Fase 34.1: species whose "Resetar rede" confirmation modal is open (0 = none).
    std::uint32_t pendingNeuralResetSpecies_ = 0;
    std::unordered_map<std::string, sf::Texture> icons_;
    bool iconsLoaded_ = false;

    // Phase 30: in-app performance window (reads the profiler; can launch an
    // isolated Phase 29 benchmark scenario in the background).
    DevWindow devWindow_;
};
} // namespace agentbiosim::ui
