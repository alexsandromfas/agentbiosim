#include "app/App.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterHelpers.hpp"
#include "config/ParameterMetadata.hpp"
#include "core/AssetPath.hpp"
#include "core/Logger.hpp"
#include "core/Profiler.hpp"
#include "core/Version.hpp"
#include "i18n/Locale.hpp"
#include "io/FileDialog.hpp"
#include "io/SaveFile.hpp"
#include "ui/ImGuiTheme.hpp"
#include "ui/UiPreferencesPanel.hpp"  // prefs* model free functions (prefsApplyPending, etc.)

#include <imgui.h>
#include <imgui-SFML.h>

#include <SFML/Graphics/View.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <sstream>
#include <variant>

namespace agentbiosim
{
namespace
{
constexpr unsigned int kWindowWidth = 1280;
constexpr unsigned int kWindowHeight = 720;
constexpr unsigned int kFrameLimit = 120;
constexpr float kWorldPaddingPixels = 48.0F;

sf::ContextSettings makeContextSettings()
{
    // Phase 22.1: enable antialiasing so vetorial agent bodies and overlays
    // look smooth instead of rasterized. 8x is the SFML 2.6 sane default; if
    // the GPU/driver caps the value SFML clamps automatically.
    sf::ContextSettings s;
    s.antialiasingLevel = 8;
    return s;
}

sf::Vector2f toSfml(const simulation::Vec2 value)
{
    return {static_cast<float>(value.x), static_cast<float>(value.y)};
}

std::uint8_t colorChannel(const int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

sf::Color toRenderColor(const config::ColorRgb color)
{
    return {colorChannel(color.r), colorChannel(color.g), colorChannel(color.b)};
}
} // namespace

App::App()
    : parameters_(config::createDefaultParameterRegistry()),
      runner_(parameters_),
      window_(sf::VideoMode(kWindowWidth, kWindowHeight), "AgentBioSimCpp",
                sf::Style::Default, makeContextSettings())
{
    // Phase 23: stamp applyFlags onto parameters that need reset / rebuild /
    // renderer refresh / are pending future phases. Done here rather than in
    // ParameterDefaults so the 200-entry defaults file does not need touching.
    config::applyPhase23ApplyFlags(parameters_);
    window_.setFramerateLimit(kFrameLimit);
    // Phase 22.1: snap the SFML view to the actual window pixels so that
    // anything we draw in screen-space (the entire Renderer + UI) sits 1:1
    // with the framebuffer. Without this, the view stays at the construction
    // size and the framebuffer stretches it on Maximize / Resize, breaking
    // every screen-to-world mapping in the input path.
    window_.setView(sf::View(sf::FloatRect(0.0F, 0.0F,
                                              static_cast<float>(kWindowWidth),
                                              static_cast<float>(kWindowHeight))));
    runner_.initialize();
    configureFromParameters();
    configureRenderOptions();
    fitCameraToWorld();
    // Phase 27: crash hook + session log (level driven by the log_level param,
    // set in configureFromParameters). Off by default -> the file just records
    // session start/end and crashes.
    core::installCrashHandler();
    core::Logger::instance().open(core::executableDir());
    core::Logger::instance().log(core::LogLevel::Info, "APP_START", std::string(kVersionString));
    // Phase 25: initialize Dear ImGui (ImGui-SFML backend). Load Segoe UI at
    // 18px for a modern look with Latin-1 glyphs (covers PT-BR accents); fall
    // back to the built-in font if the file is unavailable. Then apply the
    // project's dark theme so every panel shares one visual language.
    if (!ImGui::SFML::Init(window_))
    {
        std::cerr << "AgentBioSimCpp: falha ao inicializar ImGui-SFML.\n";
    }
    {
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;  // do not write imgui.ini next to the executable
        io.Fonts->Clear();
        if (io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", 18.0F) == nullptr)
        {
            io.Fonts->AddFontDefault();
        }
        static_cast<void>(ImGui::SFML::UpdateFontTexture());
    }
    ui::applyImGuiTheme();
    uiState_.timeScale = config::parameterDouble(parameters_, "time_scale", 1.0);
    std::cout << "AgentBioSimCpp Phase 25: UI Dear ImGui (Editor / Substrato / Labels) ("
              << runner_.species().size() << " species, " << runner_.genomes().size()
              << " genomes, " << runner_.foods().size() << " foods, "
              << runner_.obstacles().size() << " obstacles).\n";
}

App::~App()
{
    // Phase 27: flush + close the session log.
    core::Logger::instance().log(core::LogLevel::Info, "APP_STOP");
    core::Logger::instance().close();
    // Phase 25: tear down the ImGui-SFML context.
    ImGui::SFML::Shutdown();
}

int App::run()
{
    while (window_.isOpen())
    {
        processEvents();
        update();
        render();
        updateFpsTitle();
        if (uiState_.quitRequested) window_.close();
    }
    return 0;
}

void App::processEvents()
{
    sf::Event event{};
    while (window_.pollEvent(event))
    {
        // Phase 25: feed every event to Dear ImGui first so its widgets react.
        ImGui::SFML::ProcessEvent(window_, event);

        if (event.type == sf::Event::Closed)
        {
            window_.close();
            continue;
        }
        if (event.type == sf::Event::Resized)
        {
            handleResize(event.size.width, event.size.height);
            continue;
        }
        // Phase 25 (Divida 9): ImGui owns ALL widget interaction. When the
        // pointer is over an ImGui window, or a text field has keyboard focus,
        // ImGui captures the event and the world canvas must not react.
        // Otherwise the event belongs to the canvas and goes to the InputRouter.
        // This single capture test replaces the hand-rolled hit-testing of the
        // old SFML panels (UiPanel / UiPreferencesPanel / UiLeftDock) and the
        // manual text-edit / scroll / drag handling, while preserving the
        // Microfase 22.1 screen->world conversion in the InputRouter.
        const ImGuiIO& io = ImGui::GetIO();
        const bool mouseEvent =
            event.type == sf::Event::MouseButtonPressed ||
            event.type == sf::Event::MouseButtonReleased ||
            event.type == sf::Event::MouseMoved ||
            event.type == sf::Event::MouseWheelScrolled;
        const bool keyboardEvent =
            event.type == sf::Event::KeyPressed ||
            event.type == sf::Event::KeyReleased ||
            event.type == sf::Event::TextEntered;
        if ((mouseEvent && io.WantCaptureMouse) || (keyboardEvent && io.WantCaptureKeyboard))
        {
            continue;
        }
        inputRouter_.handleEvent(event, window_.getSize(), camera_, runner_, uiState_,
                                   commandQueue_);
    }
    inputRouter_.update(window_.getSize(), camera_, uiState_, commandQueue_);
}

void App::handleResize(const unsigned int width, const unsigned int height)
{
    if (width == 0U || height == 0U) return;
    // Phase 22.1 hotfix: critical fix for the "clique sai do lugar quando
    // maximiza" bug. SFML keeps the View pinned to its construction size on
    // window resize, so the framebuffer stretches to the new window pixels
    // but every draw call still uses the old coordinate space. Setting a new
    // View matched to the new pixel size makes screen-to-world conversions
    // line up again. fitCameraToWorld() then re-zooms so the world fits the
    // larger viewport.
    window_.setView(sf::View(sf::FloatRect(0.0F, 0.0F,
                                              static_cast<float>(width),
                                              static_cast<float>(height))));
    fitCameraToWorld();
}

void App::configureFromParameters()
{
    // Phase 25.2: mirror the persisted ui_language setting into the i18n module
    // (the runtime source of truth the UI reads). Runs on startup and again on
    // every Immediate-flag apply, so changing the language takes effect live.
    i18n::setLanguage(i18n::languageFromTag(
        config::parameterString(parameters_, "ui_language", "pt-br")));

    // Phase 27: log verbosity follows the registry (also applied live on Apply).
    core::Logger::instance().setLevel(
        core::logLevelFromString(config::parameterString(parameters_, "log_level", "off")));

    simulation::FixedTimestepConfig timestepConfig;
    timestepConfig.physicsStepsPerSecond = config::parameterDouble(parameters_,
        "physics_steps_per_second", 30.0);
    timestepConfig.maxStepsPerFrame = static_cast<unsigned int>(std::max(1,
        config::parameterInt(parameters_, "max_physics_steps_per_frame", 8)));
    timestepConfig.maxBacklogSeconds = config::parameterDouble(parameters_,
        "max_physics_backlog_seconds", 0.25);
    timestepConfig.timeScale = config::parameterDouble(parameters_, "time_scale", 1.0);
    timestepConfig.paused = config::parameterBool(parameters_, "paused", false);
    timestep_.configure(timestepConfig);
    if (timestepConfig.paused) runner_.setPaused(true);
}

void App::configureRenderOptions()
{
    renderOptions_.renderEnabled = config::parameterBool(parameters_, "render_enabled", true);
    renderOptions_.simpleRender = config::parameterBool(parameters_, "simple_render", false);
    renderOptions_.renderResolutionScale = static_cast<float>(
        config::parameterDouble(parameters_, "render_resolution_scale", 1.0));
    renderOptions_.backgroundColor =
        toRenderColor(config::parameterColor(parameters_, "substrate_bg_color", {10, 10, 20}));
    renderOptions_.backgroundGradientEnabled =
        config::parameterBool(parameters_, "background_gradient_enabled", false);
    renderOptions_.backgroundColorTop =
        toRenderColor(config::parameterColor(parameters_, "background_color_top", {10, 10, 20}));
    renderOptions_.backgroundColorBottom =
        toRenderColor(config::parameterColor(parameters_, "background_color_bottom", {10, 10, 20}));
    renderOptions_.substrateGradientEnabled =
        config::parameterBool(parameters_, "substrate_gradient_enabled", false);
    renderOptions_.substrateColorTop =
        toRenderColor(config::parameterColor(parameters_, "substrate_color_top", {10, 10, 20}));
    renderOptions_.substrateColorBottom =
        toRenderColor(config::parameterColor(parameters_, "substrate_color_bottom", {10, 10, 20}));
    renderOptions_.substrateBorderEnabled =
        config::parameterBool(parameters_, "substrate_border_enabled", true);
    renderOptions_.substrateBorderColor =
        toRenderColor(config::parameterColor(parameters_, "substrate_border_color", {40, 200, 40}));
}

void App::fitCameraToWorld()
{
    // Phase 24.2: when the left dock is visible it covers the left kDockW pixels
    // of the full-window canvas. We frame the world into the VISIBLE region
    // (right of the dock) by fitting against a reduced width and then panning
    // the content right by dockW/2. This only edits camera center_/zoom_, which
    // screenToWorld/worldToScreen consume symmetrically, so the Microfase 22.1
    // coordinate conversion stays exact.
    const sf::Vector2u vp = window_.getSize();
    float dockW = uiState_.preferences.dockVisible ? ui::ImGuiUi::kDockW : 0.0F;
    if (dockW > static_cast<float>(vp.x) * 0.6F) dockW = 0.0F;  // safety on tiny windows
    const sf::Vector2u fitVp{vp.x - static_cast<unsigned int>(dockW), vp.y};
    camera_.fitWorld(toSfml(runner_.world().minBounds()), toSfml(runner_.world().maxBounds()),
                       fitVp, kWorldPaddingPixels);
    if (dockW > 0.0F)
    {
        camera_.pan({dockW * 0.5F, 0.0F});
    }
}

void App::saveSimulation(const bool forcePrompt)
{
    std::string path = currentSavePath_;
    if (forcePrompt || path.empty())
    {
        const std::string suggested = path.empty() ? std::string("simulacao.agentbiosim") : path;
        path = io::saveSimulationDialog(suggested);
        if (path.empty()) return;  // user cancelled the dialog
    }

    io::SaveBundle bundle;
    bundle.snapshot = runner_.snapshot();
    for (const auto& def : parameters_.definitions())
    {
        bundle.params.emplace_back(def.name, def.defaultValue);
    }
    const sf::Vector2f cc = camera_.center();
    bundle.camera.valid = true;
    bundle.camera.centerX = cc.x;
    bundle.camera.centerY = cc.y;
    bundle.camera.zoom = camera_.zoom();

    std::string error;
    if (io::saveToFile(path, bundle, error))
    {
        currentSavePath_ = path;
        std::cout << "Simulacao salva: " << path << '\n';
    }
    else
    {
        std::cerr << "Falha ao salvar: " << error << '\n';
    }
}

void App::loadSimulation()
{
    const std::string path = io::openSimulationDialog();
    if (path.empty()) return;  // user cancelled the dialog

    const io::LoadResult result = io::loadFromFile(path);
    if (!result.ok)
    {
        std::cerr << "Falha ao abrir: " << result.error << '\n';
        return;
    }

    // Apply the saved parameters first (so the engine reads the same config),
    // then replace the engine state, then re-derive runtime/render config.
    for (const auto& p : result.bundle.params)
    {
        static_cast<void>(parameters_.setValue(p.first, p.second));
    }
    runner_.restore(result.bundle.snapshot);
    configureFromParameters();
    configureRenderOptions();

    if (result.bundle.camera.valid)
    {
        camera_.setCenter({static_cast<float>(result.bundle.camera.centerX),
                            static_cast<float>(result.bundle.camera.centerY)});
        camera_.setZoom(static_cast<float>(result.bundle.camera.zoom));
    }
    else
    {
        fitCameraToWorld();
    }

    // The previous selection / viewer target no longer apply.
    uiState_.selection.clear();
    runner_.clearNeuralViewerTarget();
    runner_.clearVisionDebugTarget();
    currentSavePath_ = path;
    std::cout << "Simulacao carregada: " << path << '\n';
}

void App::exportSelectedAgent()
{
    simulation::EntityId selected{};
    for (const auto id : uiState_.selection.ids())
    {
        if (runner_.agents().contains(id)) { selected = id; break; }
    }
    if (!selected.isValid())
    {
        std::cerr << "Nenhum organismo selecionado para exportar.\n";
        return;
    }
    sim::AgentExport data;
    if (!runner_.exportAgent(selected, data))
    {
        std::cerr << "Falha ao capturar o organismo selecionado.\n";
        return;
    }
    const std::string path = io::saveOrganismDialog("organismo.organism");
    if (path.empty()) return;  // cancelled

    std::string error;
    if (io::saveAgentToFile(path, data, error))
    {
        std::cout << "Organismo exportado: " << path << '\n';
    }
    else
    {
        std::cerr << "Falha ao exportar: " << error << '\n';
    }
}

void App::importAgentFromFile()
{
    const std::string path = io::openOrganismDialog();
    if (path.empty()) return;  // cancelled

    const io::AgentLoadResult result = io::loadAgentFromFile(path);
    if (!result.ok)
    {
        std::cerr << "Falha ao importar: " << result.error << '\n';
        return;
    }
    const sf::Vector2f center = camera_.center();
    const simulation::EntityId id =
        runner_.importAgent(result.agent, {static_cast<double>(center.x),
                                            static_cast<double>(center.y)});
    if (id.isValid())
    {
        uiState_.selection.clear();
        static_cast<void>(uiState_.selection.add(id));
        std::cout << "Organismo importado: " << path << '\n';
    }
}

simulation::SpeciesId App::speciesOfSelectionOrDefault() const
{
    const auto& agents = runner_.agents();
    for (const auto id : uiState_.selection.ids())
    {
        const auto idx = agents.indexOf(id);
        if (idx.has_value() && agents.aliveAt(*idx))
        {
            return agents.speciesIdAt(*idx);
        }
    }
    const auto* bacteria = runner_.species().findByName("bacteria");
    return bacteria != nullptr ? bacteria->id : simulation::kInvalidSpeciesId;
}

void App::maybeAutosave(const double realDeltaSeconds)
{
    const bool enabled = config::parameterBool(parameters_, "auto_export_substrate", true);
    if (!enabled)
    {
        autosaveTimerSeconds_ = 0.0;
        return;
    }
    const double intervalMinutes =
        config::parameterDouble(parameters_, "auto_export_interval_minutes", 30.0);
    if (intervalMinutes <= 0.0) return;

    autosaveTimerSeconds_ += std::max(0.0, realDeltaSeconds);
    if (autosaveTimerSeconds_ < intervalMinutes * 60.0) return;
    autosaveTimerSeconds_ = 0.0;

    // Skip this tick if the previous autosave is still being written (never block
    // the UI thread waiting on disk).
    if (autosaveFuture_.valid() &&
        autosaveFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        return;
    }

    // Capture a self-contained copy on the main thread (fast — memory only), then
    // serialize + write on a background thread (the slow part) so the simulation
    // does not freeze.
    auto bundle = std::make_shared<io::SaveBundle>();
    bundle->snapshot = runner_.snapshot();
    for (const auto& def : parameters_.definitions())
    {
        bundle->params.emplace_back(def.name, def.defaultValue);
    }
    const sf::Vector2f cc = camera_.center();
    bundle->camera.valid = true;
    bundle->camera.centerX = cc.x;
    bundle->camera.centerY = cc.y;
    bundle->camera.zoom = camera_.zoom();

    const std::string path = core::executableDir() + "autosave.agentbiosim";
    autosaveFuture_ = std::async(std::launch::async, [bundle, path]() {
        std::string error;
        static_cast<void>(io::saveToFile(path, *bundle, error));
    });
}

void App::drainCommandsAndApply()
{
    auto commands = commandQueue_.drain();
    uiState_.commandsProcessed += commands.size();
    for (const auto& cmd : commands)
    {
        std::visit([&](auto&& c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, ui::CmdFitWorldCamera>)
            {
                fitCameraToWorld();
            }
            else if constexpr (std::is_same_v<T, ui::CmdResetCamera>)
            {
                // Phase 22.1: distinct from Fit — explicitly recentre on the
                // world centre at the default zoom that fits everything.
                fitCameraToWorld();
            }
            else if constexpr (std::is_same_v<T, ui::CmdSaveSimulation>)
            {
                saveSimulation(/*forcePrompt=*/false);
            }
            else if constexpr (std::is_same_v<T, ui::CmdSaveSimulationAs>)
            {
                saveSimulation(/*forcePrompt=*/true);
            }
            else if constexpr (std::is_same_v<T, ui::CmdLoadSimulation>)
            {
                loadSimulation();
            }
            else if constexpr (std::is_same_v<T, ui::CmdExportAgent>)
            {
                exportSelectedAgent();
            }
            else if constexpr (std::is_same_v<T, ui::CmdImportAgent>)
            {
                importAgentFromFile();
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetCameraCenter>)
            {
                camera_.setCenter({static_cast<float>(c.worldCenter.x),
                                    static_cast<float>(c.worldCenter.y)});
            }
            else if constexpr (std::is_same_v<T, ui::CmdPanCameraScreen>)
            {
                camera_.pan({static_cast<float>(c.dx), static_cast<float>(c.dy)});
            }
            else if constexpr (std::is_same_v<T, ui::CmdZoomCameraAt>)
            {
                camera_.zoomAt(static_cast<float>(c.factor),
                                 {static_cast<float>(c.screenX), static_cast<float>(c.screenY)},
                                 window_.getSize());
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetCanvasTool>)
            {
                uiState_.activeTool = c.tool;
            }
            else if constexpr (std::is_same_v<T, ui::CmdSelectAtWorldPoint>)
            {
                const auto id = runner_.pickAgentAt(c.world, c.pickRadius);
                if (!c.additive) uiState_.selection.clear();
                if (id.isValid()) uiState_.selection.add(id);
            }
            else if constexpr (std::is_same_v<T, ui::CmdSelectRect>)
            {
                std::vector<simulation::EntityId> hits;
                static_cast<void>(runner_.agentsInRect(c.worldA, c.worldB, hits));
                if (!c.additive) uiState_.selection.clear();
                for (const auto id : hits) uiState_.selection.add(id);
            }
            else if constexpr (std::is_same_v<T, ui::CmdSelectLasso>)
            {
                std::vector<simulation::EntityId> hits;
                static_cast<void>(runner_.agentsInLasso(c.worldPolygon, hits));
                if (!c.additive) uiState_.selection.clear();
                for (const auto id : hits) uiState_.selection.add(id);
            }
            else if constexpr (std::is_same_v<T, ui::CmdClearSelection>)
            {
                uiState_.selection.clear();
            }
            else if constexpr (std::is_same_v<T, ui::CmdDeleteSelected>)
            {
                runner_.deleteAgents(uiState_.selection.ids());
                uiState_.selection.clear();
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleSelectionOverlay>)
            {
                uiState_.showSelectionOverlay = !uiState_.showSelectionOverlay;
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleToolOverlay>)
            {
                uiState_.showToolOverlay = !uiState_.showToolOverlay;
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleHelpPanel>)
            {
                // Phase 31 fix: the H shortcut must drive the LIVE ImGui help
                // window (prefs.helpWindowOpen), not the dead SFML-era showHelp.
                uiState_.preferences.helpWindowOpen = !uiState_.preferences.helpWindowOpen;
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleVisionDebug>)
            {
                // Phase 31 fix: V / menu "Debug de visao" toggles the selected-
                // agent vision overlay (same backend as the inspector checkbox).
                uiState_.selectedVisionOverlay = !uiState_.selectedVisionOverlay;
            }
            else if constexpr (std::is_same_v<T, ui::CmdTogglePreferencesPanel>)
            {
                uiState_.showPreferencesPlaceholder = !uiState_.showPreferencesPlaceholder;
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleAboutPanel>)
            {
                uiState_.showAboutPanel = !uiState_.showAboutPanel;
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleGenomePanel>)
            {
                uiState_.showGenomePlaceholder = !uiState_.showGenomePlaceholder;
            }
            else if constexpr (std::is_same_v<T, ui::CmdQuitApp>)
            {
                uiState_.quitRequested = true;
            }
            else if constexpr (std::is_same_v<T, ui::CmdCloseAllMenus>)
            {
                uiState_.openMenuIndex = -1;
            }
            // Phase 23: preferences window dispatch (Phase 23.1 redirects
            // the legacy CmdOpenPreferences to the Simulation window so old
            // tests / external callers still work).
            else if constexpr (std::is_same_v<T, ui::CmdOpenPreferences>)
            {
                uiState_.preferences.windowOpen[
                    static_cast<std::size_t>(config::PrefsTab::Simulation)] = true;
                uiState_.openMenuIndex = -1;
            }
            else if constexpr (std::is_same_v<T, ui::CmdClosePreferences>)
            {
                for (auto& b : uiState_.preferences.windowOpen) b = false;
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetPreferencesTab>)
            {
                uiState_.preferences.activeTab = c.tab;
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetPreferencesSearch>)
            {
                uiState_.preferences.searchQuery = c.query;
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetParameterValue>)
            {
                // Live UI/engine knobs apply immediately and in isolation (no
                // pending edit, no reset): the language combo (Phase 25.2) and
                // the observability toggles (Phase 27). configureFromParameters
                // syncs i18n + log level; the runner reads the profiler/metrics
                // flags from the registry each step.
                const bool immediate =
                    c.name == "ui_language" || c.name == "log_level" ||
                    c.name == "profiler_enabled" || c.name == "metrics_enabled" ||
                    c.name == "metrics_max_samples" || c.name == "metrics_sample_interval";
                if (immediate)
                {
                    if (parameters_.setValue(c.name, c.value))
                    {
                        configureFromParameters();
                    }
                    uiState_.preferences.pendingValues.erase(c.name);
                }
                else
                {
                    uiState_.preferences.pendingValues[c.name] = c.value;
                    ++uiState_.preferences.controlInteractions;
                }
            }
            else if constexpr (std::is_same_v<T, ui::CmdApplyPreferences>)
            {
                const unsigned int flags =
                    ui::prefsApplyPending(parameters_, uiState_.preferences);
                if (flags & config::ApplyFlag::RefreshRenderer)
                {
                    configureRenderOptions();
                }
                // Microfase 31.1: world geometry reshapes LIVE — no reset, no
                // camera refit; organisms are pushed back inside the bounds.
                if (flags & config::ApplyFlag::ReshapeWorld)
                {
                    runner_.applyWorldConfigLive();
                }
                if (flags & config::ApplyFlag::RequiresReset)
                {
                    runner_.reset();
                    fitCameraToWorld();
                }
                if (flags & config::ApplyFlag::Immediate)
                {
                    // Re-read timing / pause from registry without resetting.
                    configureFromParameters();
                }
            }
            else if constexpr (std::is_same_v<T, ui::CmdRevertPreferences>)
            {
                uiState_.preferences.pendingValues.clear();
                ++uiState_.preferences.revertedCount;
            }
            else if constexpr (std::is_same_v<T, ui::CmdRestoreDefaultsPreferences>)
            {
                // Restore defaults for parameters of the *active tab* only.
                const auto names = ui::prefsParametersForTab(parameters_,
                    static_cast<config::PrefsTab>(uiState_.preferences.activeTab), "");
                for (const auto& n : names)
                {
                    const auto* d = parameters_.find(n);
                    if (d != nullptr)
                    {
                        uiState_.preferences.pendingValues[n] = d->originalDefault;
                    }
                }
                ++uiState_.preferences.restoredDefaultsCount;
            }
            else if constexpr (std::is_same_v<T, ui::CmdRestoreParameterDefault>)
            {
                const auto* d = parameters_.find(c.name);
                if (d != nullptr)
                {
                    uiState_.preferences.pendingValues[c.name] = d->originalDefault;
                }
            }
            // Phase 23.1: new commands.
            else if constexpr (std::is_same_v<T, ui::CmdOpenPreferencesWindow>)
            {
                if (c.tab >= 0 && c.tab < static_cast<int>(config::PrefsTab::Count))
                {
                    uiState_.preferences.windowOpen[static_cast<std::size_t>(c.tab)] = true;
                }
                uiState_.openMenuIndex = -1;
            }
            else if constexpr (std::is_same_v<T, ui::CmdClosePreferencesWindow>)
            {
                if (c.tab >= 0 && c.tab < static_cast<int>(config::PrefsTab::Count))
                {
                    uiState_.preferences.windowOpen[static_cast<std::size_t>(c.tab)] = false;
                }
            }
            else if constexpr (std::is_same_v<T, ui::CmdScrollPreferencesWindow>)
            {
                if (c.tab >= 0 && c.tab < static_cast<int>(config::PrefsTab::Count))
                {
                    auto& s = uiState_.preferences.windowScroll[
                        static_cast<std::size_t>(c.tab)];
                    s = std::max(0, s + c.delta);
                }
            }
            else if constexpr (std::is_same_v<T, ui::CmdOpenHelpWindow>)
            {
                uiState_.preferences.helpWindowOpen = true;
                uiState_.openMenuIndex = -1;
            }
            else if constexpr (std::is_same_v<T, ui::CmdCloseHelpWindow>)
            {
                uiState_.preferences.helpWindowOpen = false;
            }
            else if constexpr (std::is_same_v<T, ui::CmdOpenSubstratePlaceholder>)
            {
                uiState_.preferences.substratePlaceholderOpen = true;
                uiState_.openMenuIndex = -1;
            }
            else if constexpr (std::is_same_v<T, ui::CmdCloseSubstratePlaceholder>)
            {
                uiState_.preferences.substratePlaceholderOpen = false;
            }
            else if constexpr (std::is_same_v<T, ui::CmdOpenPrefsPopup>)
            {
                uiState_.preferences.openPopup = c.popup;
            }
            else if constexpr (std::is_same_v<T, ui::CmdClosePrefsPopup>)
            {
                uiState_.preferences.openPopup.clear();
            }
            else if constexpr (std::is_same_v<T, ui::CmdAdjustTimeScale>)
            {
                const double cur = config::parameterDouble(parameters_, "time_scale", 1.0);
                const double next = std::clamp(cur * c.factor, 0.1, 50.0);
                static_cast<void>(parameters_.setValue("time_scale", next));
                uiState_.timeScale = next;
                configureFromParameters();
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetTimeScale>)
            {
                const double v = std::clamp(c.timeScale, 0.1, 50.0);
                static_cast<void>(parameters_.setValue("time_scale", v));
                uiState_.timeScale = v;
                configureFromParameters();
            }
            // Phase 23.2: drag, text edit, restore defaults + apply.
            else if constexpr (std::is_same_v<T, ui::CmdMovePreferencesWindow>)
            {
                if (c.tab >= 0 && c.tab < static_cast<int>(config::PrefsTab::Count))
                {
                    const auto idx = static_cast<std::size_t>(c.tab);
                    uiState_.preferences.windowX[idx] = c.x;
                    uiState_.preferences.windowY[idx] = c.y;
                }
            }
            else if constexpr (std::is_same_v<T, ui::CmdMoveHelpWindow>)
            {
                uiState_.preferences.helpWindowX = c.x;
                uiState_.preferences.helpWindowY = c.y;
            }
            else if constexpr (std::is_same_v<T, ui::CmdBeginEditParameter>)
            {
                uiState_.preferences.editingParam = c.name;
                uiState_.preferences.editingBuffer = c.initialBuffer;
            }
            else if constexpr (std::is_same_v<T, ui::CmdCancelEditParameter>)
            {
                uiState_.preferences.editingParam.clear();
                uiState_.preferences.editingBuffer.clear();
            }
            else if constexpr (std::is_same_v<T, ui::CmdCommitEditParameter>)
            {
                if (!uiState_.preferences.editingParam.empty())
                {
                    const auto* def = parameters_.find(uiState_.preferences.editingParam);
                    if (def != nullptr)
                    {
                        try
                        {
                            if (def->type == config::ParameterType::Integer)
                            {
                                const int v = std::stoi(uiState_.preferences.editingBuffer);
                                uiState_.preferences.pendingValues[
                                    uiState_.preferences.editingParam] = v;
                            }
                            else if (def->type == config::ParameterType::Floating)
                            {
                                const double v = std::stod(uiState_.preferences.editingBuffer);
                                uiState_.preferences.pendingValues[
                                    uiState_.preferences.editingParam] = v;
                            }
                        }
                        catch (...)
                        {
                            // Invalid input — ignore, keep current value.
                        }
                    }
                }
                uiState_.preferences.editingParam.clear();
                uiState_.preferences.editingBuffer.clear();
            }
            else if constexpr (std::is_same_v<T, ui::CmdRestoreDefaultsAndApply>)
            {
                // Find which window is on top — use the highest-indexed open
                // tab (matches the front-most-first hit-test order in the
                // panel). The user clicked the "Restaurar padroes" button on
                // that window, so restore its parameters only.
                int activeTab = -1;
                for (int t = static_cast<int>(config::PrefsTab::Count) - 1;
                     t >= 0; --t)
                {
                    if (uiState_.preferences.windowOpen[
                        static_cast<std::size_t>(t)])
                    {
                        activeTab = t;
                        break;
                    }
                }
                if (activeTab >= 0)
                {
                    const auto names = ui::prefsParametersForTabFiltered(
                        parameters_, uiState_.preferences,
                        static_cast<config::PrefsTab>(activeTab));
                    for (const auto& n : names)
                    {
                        const auto* d = parameters_.find(n);
                        if (d != nullptr)
                        {
                            uiState_.preferences.pendingValues[n] =
                                d->originalDefault;
                        }
                    }
                    // Apply immediately so the user sees the values change.
                    const unsigned int flags = ui::prefsApplyPending(
                        parameters_, uiState_.preferences);
                    if (flags & config::ApplyFlag::RefreshRenderer)
                    {
                        configureRenderOptions();
                    }
                    if (flags & config::ApplyFlag::ReshapeWorld)
                    {
                        runner_.applyWorldConfigLive();  // Microfase 31.1: live, no reset
                    }
                    if (flags & config::ApplyFlag::RequiresReset)
                    {
                        runner_.reset();
                        fitCameraToWorld();
                    }
                    if (flags & config::ApplyFlag::Immediate)
                    {
                        configureFromParameters();
                    }
                    ++uiState_.preferences.restoredDefaultsCount;
                }
            }
            // Phase 24: operational window open/close + scroll/move + apply.
            else if constexpr (std::is_same_v<T, ui::CmdOpenEditorGenetico>)
            {
                uiState_.preferences.editorOpen = true;
                uiState_.openMenuIndex = -1;
            }
            else if constexpr (std::is_same_v<T, ui::CmdCloseEditorGenetico>)
            {
                uiState_.preferences.editorOpen = false;
            }
            else if constexpr (std::is_same_v<T, ui::CmdOpenEspecies>)
            {
                uiState_.preferences.especiesOpen = true;
                uiState_.openMenuIndex = -1;
            }
            else if constexpr (std::is_same_v<T, ui::CmdCloseEspecies>)
            {
                uiState_.preferences.especiesOpen = false;
            }
            else if constexpr (std::is_same_v<T, ui::CmdOpenPopulacao>)
            {
                uiState_.preferences.populacaoOpen = true;
                uiState_.openMenuIndex = -1;
            }
            else if constexpr (std::is_same_v<T, ui::CmdClosePopulacao>)
            {
                uiState_.preferences.populacaoOpen = false;
            }
            else if constexpr (std::is_same_v<T, ui::CmdOpenSubstrato>)
            {
                uiState_.preferences.substratoOpen = true;
                uiState_.openMenuIndex = -1;
            }
            else if constexpr (std::is_same_v<T, ui::CmdCloseSubstrato>)
            {
                uiState_.preferences.substratoOpen = false;
            }
            else if constexpr (std::is_same_v<T, ui::CmdScrollOperationalWindow>)
            {
                auto bump = [&](int& v) { v = std::max(0, v + c.delta); };
                if (c.which == 0) bump(uiState_.preferences.editorScroll);
                else if (c.which == 1) bump(uiState_.preferences.especiesScroll);
                else if (c.which == 2) bump(uiState_.preferences.populacaoScroll);
                else if (c.which == 3) bump(uiState_.preferences.substratoScroll);
            }
            else if constexpr (std::is_same_v<T, ui::CmdMoveOperationalWindow>)
            {
                if (c.which == 0) { uiState_.preferences.editorX = c.x;    uiState_.preferences.editorY = c.y; }
                else if (c.which == 1) { uiState_.preferences.especiesX = c.x;  uiState_.preferences.especiesY = c.y; }
                else if (c.which == 2) { uiState_.preferences.populacaoX = c.x; uiState_.preferences.populacaoY = c.y; }
                else if (c.which == 3) { uiState_.preferences.substratoX = c.x; uiState_.preferences.substratoY = c.y; }
            }
            else if constexpr (std::is_same_v<T, ui::CmdApplyGenomeToSpecies>)
            {
                // Microfase 32.2: LIVE apply — the Phase 24 placeholder did a
                // full reset here, which respawned only the registry species and
                // silently wiped every user-created label's members. Now the
                // editor template is applied in place to the TARGET label: the
                // label of the first selected organism, or the default bacteria
                // label when nothing is selected. Nothing is deleted; the
                // simulation does not restart.
                const unsigned int flags = ui::prefsApplyPending(parameters_,
                    uiState_.preferences);
                if (flags & config::ApplyFlag::RefreshRenderer) configureRenderOptions();
                configureFromParameters();
                static_cast<void>(runner_.applyEditorGenomeToSpecies(
                    speciesOfSelectionOrDefault()));
            }
            else if constexpr (std::is_same_v<T, ui::CmdApplyPopulation> ||
                                 std::is_same_v<T, ui::CmdApplyEnvironment>)
            {
                // Microfase 31.1: substrate/food/population changes do NOT
                // restart the simulation. Bake the pending edits; the runner's
                // CmdApplyEnvironment arm reshapes the world live (agents/food
                // pushed back inside) and food knobs are read every step anyway.
                const unsigned int flags = ui::prefsApplyPending(parameters_,
                    uiState_.preferences);
                if (flags & config::ApplyFlag::RefreshRenderer) configureRenderOptions();
                configureFromParameters();
            }
            else if constexpr (std::is_same_v<T, ui::CmdApplyGenomeToSelected>)
            {
                // Microfase 32.2: LIVE apply to the selected organisms only —
                // the Phase 24 placeholder DELETED them (hoping the rescue would
                // respawn defaults), which destroyed freshly created labels.
                // Each organism keeps its label, position, energy and brain;
                // only its genome scalars take the editor values. Selection is
                // preserved so the user can keep iterating.
                const unsigned int flags = ui::prefsApplyPending(parameters_,
                    uiState_.preferences);
                if (flags & config::ApplyFlag::RefreshRenderer) configureRenderOptions();
                configureFromParameters();
                static_cast<void>(runner_.applyEditorGenomeToAgents(
                    uiState_.selection.ids()));
            }
            else if constexpr (std::is_same_v<T, ui::CmdClearAllFood>)
            {
                static_cast<void>(runner_.applyCommand(ui::CmdClearFood{}));
            }
            else if constexpr (std::is_same_v<T, ui::CmdSelectAllOfSpecies>)
            {
                // Walk AgentStore and add ids whose speciesIdAt matches.
                uiState_.selection.clear();
                const auto& ag = runner_.agents();
                for (std::size_t i = 0; i < ag.size(); ++i)
                {
                    if (!ag.aliveAt(i)) continue;
                    if (static_cast<std::uint32_t>(ag.speciesIdAt(i)) == c.speciesId)
                    {
                        uiState_.selection.add(ag.idAt(i));
                    }
                }
            }
            else if constexpr (std::is_same_v<T, ui::CmdAssignSelectedToSpecies>)
            {
                // Phase 24.2: real implementation — move selected agents into
                // the species and recolor them (mirrors Python
                // engine.assign_label_to_agents).
                runner_.assignSelectedToSpecies(uiState_.selection.ids(), c.speciesId);
            }
            else if constexpr (std::is_same_v<T, ui::CmdCreateSpeciesFromSelected>)
            {
                const auto id = runner_.createSpeciesFromSelected("Nova label",
                                                                    uiState_.selection.ids());
                static_cast<void>(id);
            }
            else if constexpr (std::is_same_v<T, ui::CmdRemoveSelectedFromSpecies>)
            {
                runner_.removeSelectedFromSpecies(uiState_.selection.ids());
            }
            else if constexpr (std::is_same_v<T, ui::CmdCycleSpeciesColor>)
            {
                static_cast<void>(runner_.cycleSpeciesColor(c.speciesId));
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetSpeciesShowGraph>)
            {
                static_cast<void>(runner_.setSpeciesShowGraph(c.speciesId, c.show));
            }
            else if constexpr (std::is_same_v<T, ui::CmdAdjustSpeciesPop>)
            {
                static_cast<void>(runner_.adjustSpeciesPopulation(c.speciesId, c.field, c.delta));
            }
            else if constexpr (std::is_same_v<T, ui::CmdRemoveSpecies>)
            {
                static_cast<void>(runner_.removeSpeciesSafe(c.speciesId));
            }
            else if constexpr (std::is_same_v<T, ui::CmdResetNeuralForSpecies>)
            {
                // Phase 31: handled by the engine (SimulationRunner::applyCommand
                // calls resetNeuralForSpecies). Nothing to do at the App layer.
                static_cast<void>(c);
            }
            // Phase 24.2: left dock state.
            else if constexpr (std::is_same_v<T, ui::CmdSetDockTab>)
            {
                uiState_.preferences.dockActiveTab = c.tab;
                uiState_.preferences.dockScroll = 0;
            }
            else if constexpr (std::is_same_v<T, ui::CmdScrollDock>)
            {
                uiState_.preferences.dockScroll =
                    std::max(0, uiState_.preferences.dockScroll + c.delta);
            }
            else if constexpr (std::is_same_v<T, ui::CmdToggleLeftDock>)
            {
                uiState_.preferences.dockVisible = !uiState_.preferences.dockVisible;
            }
            else if constexpr (std::is_same_v<T, ui::CmdBeginEditSpeciesName>)
            {
                uiState_.preferences.editingSpeciesId = c.speciesId;
                uiState_.preferences.editingSpeciesBuffer = c.initialBuffer;
            }
            else if constexpr (std::is_same_v<T, ui::CmdCommitEditSpeciesName>)
            {
                if (uiState_.preferences.editingSpeciesId != 0U &&
                    !uiState_.preferences.editingSpeciesBuffer.empty())
                {
                    static_cast<void>(runner_.setSpeciesLabel(
                        uiState_.preferences.editingSpeciesId,
                        uiState_.preferences.editingSpeciesBuffer));
                }
                uiState_.preferences.editingSpeciesId = 0U;
                uiState_.preferences.editingSpeciesBuffer.clear();
            }
            else if constexpr (std::is_same_v<T, ui::CmdCancelEditSpeciesName>)
            {
                uiState_.preferences.editingSpeciesId = 0U;
                uiState_.preferences.editingSpeciesBuffer.clear();
            }
            // Phase 25: ImGui-native direct species label/color edits.
            else if constexpr (std::is_same_v<T, ui::CmdSetSpeciesLabel>)
            {
                if (!c.label.empty())
                {
                    static_cast<void>(runner_.setSpeciesLabel(
                        static_cast<simulation::SpeciesId>(c.speciesId), c.label));
                }
            }
            else if constexpr (std::is_same_v<T, ui::CmdSetSpeciesColor>)
            {
                static_cast<void>(runner_.setSpeciesColorAndRecolor(
                    static_cast<simulation::SpeciesId>(c.speciesId),
                    simulation::ColorRgb{static_cast<std::uint8_t>(c.r),
                                           static_cast<std::uint8_t>(c.g),
                                           static_cast<std::uint8_t>(c.b)}));
            }
            else
            {
                static_cast<void>(c);
            }
        }, cmd);
        static_cast<void>(runner_.applyCommand(cmd));
    }
}

void App::update()
{
    const double realDeltaSeconds = frameClock_.restart().asSeconds();
    drainCommandsAndApply();

    // Phase 30: while the developer window is open, keep the profiler on without
    // touching the user's profiler_enabled preference. Zero cost when closed.
    runner_.setProfilerForced(uiState_.showDevWindow);

    // Phase 26: drive the engine's neural-trace and vision-debug targets from the
    // UI state the inspector set last frame. When the Rede Neural tab is hidden
    // (or no agent selected) the target is cleared, so the engine captures no
    // trace and the cost is zero.
    if (uiState_.neuralTraceActive && runner_.agents().contains(uiState_.neuralTraceAgent))
    {
        runner_.setNeuralViewerTarget(uiState_.neuralTraceAgent);
    }
    else
    {
        runner_.clearNeuralViewerTarget();
    }
    // Fase 32.1: o visualizador de visao aparece AUTOMATICAMENTE quando ha
    // EXATAMENTE 1 organismo selecionado (e a preferencia mestre esta ligada). Com 0
    // ou >1 selecionados o alvo e limpo — a percepcao nem preenche o debug, custo
    // zero, e nao pesa desenhar varios overlays.
    {
        simulation::EntityId visionId{};
        std::size_t selectedAlive = 0;
        for (const auto id : uiState_.selection.ids())
        {
            if (runner_.agents().contains(id))
            {
                ++selectedAlive;
                visionId = id;
            }
        }
        if (uiState_.selectedVisionOverlay && selectedAlive == 1 && visionId.isValid())
        {
            runner_.setVisionDebugTarget(visionId);
        }
        else
        {
            runner_.clearVisionDebugTarget();
        }
    }

    lastStepsThisFrame_ = timestep_.beginFrame(realDeltaSeconds);
    for (unsigned int step = 0; step < lastStepsThisFrame_; ++step)
    {
        runner_.step(timestep_.fixedDeltaSeconds());
        ++simulatedSteps_;
    }

    // Phase 28: periodic autosave (wall-clock; writes on a background thread).
    maybeAutosave(realDeltaSeconds);

    // Phase 27: heartbeat (no-op unless logging is enabled). Interval from the
    // existing diagnostic_heartbeat_minutes knob.
    const double heartbeatSeconds =
        std::max(1.0, config::parameterDouble(parameters_, "diagnostic_heartbeat_minutes", 5.0) * 60.0);
    core::Logger::instance().heartbeat(
        heartbeatSeconds, "steps=" + std::to_string(simulatedSteps_) +
                              " agents=" + std::to_string(runner_.agents().size()));
}

void App::render()
{
    // Phase 25: build the Dear ImGui frame every render pass (between
    // ImGui::SFML::Update and ImGui::SFML::Render), regardless of whether the
    // world is drawn, so the UI stays responsive. ImGuiUi reads engine/UI state
    // and emits commands; it never mutates stores.
    ImGui::SFML::Update(window_, uiDeltaClock_.restart());
    ui::ImGuiFrameInfo info;
    info.fps = lastFps_;
    info.steps = simulatedSteps_;
    info.agents = runner_.agents().size();
    info.foods = runner_.foods().size();
    info.obstacles = runner_.obstacles().size();
    info.paused = runner_.paused();
    info.simpleRender = renderOptions_.simpleRender;
    {
        // Phase 27: profile the ImGui frame build under the "ui" section.
        core::ScopedTimer uiTimer(runner_.profilerMutable(), core::ProfileSection::Ui);
        imguiUi_.draw(parameters_, runner_, uiState_, commandQueue_, info);
    }

    if (renderOptions_.renderEnabled)
    {
        const auto* obstaclePtr = runner_.obstacles().empty() ? nullptr : &runner_.obstacles();

        // Phase 22.1: wire selection overlays into the renderer. Marquee/lasso
        // are taken from uiState_; selection halos read agent positions through
        // SimulationRunner via the entity ids the selection stores.
        render::SelectionRenderInput selInput;
        if (uiState_.showSelectionOverlay)
        {
            selInput.selectedIds = &uiState_.selection.ids();
        }
        selInput.marqueeActive = uiState_.marquee.active;
        selInput.marqueeStartWorld = uiState_.marquee.startWorld;
        selInput.marqueeEndWorld = uiState_.marquee.endWorld;
        selInput.lassoActive = uiState_.lasso.active;
        selInput.lassoPoints = &uiState_.lasso.points;
        if (uiState_.lastMouseValid &&
            (uiState_.activeTool == ui::CanvasTool::PaintObstacle ||
             uiState_.activeTool == ui::CanvasTool::EraseObstacle))
        {
            selInput.brushCursorActive = true;
            selInput.brushCursorWorld = uiState_.lastMouseWorld;
            selInput.brushCursorRadius = uiState_.brushRadius *
                (uiState_.activeTool == ui::CanvasTool::EraseObstacle ? 1.5 : 1.0);
            selInput.brushIsEraser = uiState_.activeTool == ui::CanvasTool::EraseObstacle;
        }

        // Phase 26: selected-agent vision overlay (rays come from the engine's
        // debug data, only filled when a vision target is set).
        const auto& visionData = runner_.visionDebug();
        const auto* visionDebugPtr =
            (uiState_.selectedVisionOverlay && visionData.active) ? &visionData : nullptr;

        core::ScopedTimer renderTimer(runner_.profilerMutable(), core::ProfileSection::Render);
        lastRenderStats_ = renderer_.render(window_, camera_, runner_.world(),
                                              runner_.agents(), runner_.foods(),
                                              renderOptions_, visionDebugPtr, obstaclePtr,
                                              &selInput);
    }
    else
    {
        window_.clear();
    }

    // Phase 25: Dear ImGui draws on top of the world canvas.
    ImGui::SFML::Render(window_);
    window_.display();
    ++frames_;
}

void App::updateFpsTitle()
{
    const float elapsed = fpsClock_.getElapsedTime().asSeconds();
    if (elapsed >= 0.5F)
    {
        lastFps_ = static_cast<float>(frames_) / elapsed;
        frames_ = 0;
        fpsClock_.restart();
        std::ostringstream title;
        title << "AgentBioSimCpp Phase 22.1 - " << static_cast<int>(lastFps_) << " FPS"
                << "  steps=" << simulatedSteps_
                << "  tool=" << ui::canvasToolLabel(uiState_.activeTool)
                << "  sel=" << uiState_.selection.size();
        window_.setTitle(title.str());
    }
}
} // namespace agentbiosim
