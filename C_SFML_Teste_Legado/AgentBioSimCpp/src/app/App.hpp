#pragma once

#include "config/ParameterRegistry.hpp"
#include "perception/VisionDebug.hpp"
#include "render/Camera2D.hpp"
#include "render/Renderer.hpp"
#include "sim/SimulationRunner.hpp"
#include "simulation/FixedTimestep.hpp"
#include "simulation/SpatialHash.hpp"
#include "core/Command.hpp"
#include "ui/ImGuiUi.hpp"
#include "ui/InputRouter.hpp"
#include "ui/UiState.hpp"

#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <future>
#include <string>
#include <vector>

namespace agentbiosim
{
// Phase 22 (refactored in 22.1 hotfix): App acts as the AppController. It owns
// the SFML window, the Camera2D, the Renderer, the SimulationRunner (engine),
// the InputRouter and the UiPanel.
class App
{
public:
    App();
    ~App();

    int run();

private:
    void processEvents();
    void handleResize(unsigned int width, unsigned int height);
    void configureFromParameters();
    void configureRenderOptions();
    void fitCameraToWorld();
    void update();
    void render();
    void updateFpsTitle();
    void drainCommandsAndApply();

    // Phase 28: save/load. `saveSimulation` prompts only when no path is given;
    // `loadSimulation` applies parameters + engine snapshot + camera from a file.
    void saveSimulation(bool forcePrompt);
    void loadSimulation();
    // Phase 28: periodic autosave. Captures the snapshot on the main thread (a
    // self-contained copy) and writes the file on a background thread so the
    // simulation never freezes. Controlled by the registry (enabled + interval).
    void maybeAutosave(double realDeltaSeconds);
    // Phase 28: single-organism export/import (.organism file).
    void exportSelectedAgent();
    void importAgentFromFile();
    // Microfase 32.2: target label for "Aplicar a especie" — the label of the
    // first selected living organism, or the default bacteria label when the
    // selection is empty.
    [[nodiscard]] simulation::SpeciesId speciesOfSelectionOrDefault() const;

    config::ParameterRegistry parameters_;
    sim::SimulationRunner runner_;
    simulation::FixedTimestep timestep_;
    render::Camera2D camera_;
    render::Renderer renderer_;
    render::RenderOptions renderOptions_;
    render::RenderStats lastRenderStats_;
    sf::RenderWindow window_;
    sf::Clock frameClock_;
    sf::Clock fpsClock_;
    sf::Vector2i lastMousePosition_{0, 0};
    perception::VisionDebugData visionDebug_{};

    // Phase 25: ImGui delta clock (ImGui::SFML::Update needs frame dt).
    sf::Clock uiDeltaClock_;

    ui::UiState uiState_{};
    ui::InputRouter inputRouter_{};
    ui::ImGuiUi imguiUi_{};
    core::CommandQueue commandQueue_;

    unsigned int frames_ = 0;
    float lastFps_ = 0.0F;
    unsigned long long simulatedSteps_ = 0;
    unsigned int lastStepsThisFrame_ = 0;

    // Phase 28: last saved/loaded file path ("Salvar" reuses it; empty = prompt).
    std::string currentSavePath_;

    // Phase 28: autosave wall-clock timer + the in-flight background write.
    double autosaveTimerSeconds_ = 0.0;
    std::future<void> autosaveFuture_;
};
} // namespace agentbiosim
