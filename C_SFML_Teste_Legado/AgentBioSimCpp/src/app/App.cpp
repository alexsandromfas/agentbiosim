#include "app/App.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterHelpers.hpp"
#include "config/ParameterMetadata.hpp"
#include "core/Version.hpp"

#include <SFML/Graphics/View.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
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
    if (font_.loadFromFile("C:/Windows/Fonts/segoeui.ttf") ||
        font_.loadFromFile("C:/Windows/Fonts/arial.ttf"))
    {
        fontLoaded_ = true;
        uiPanel_.setFont(&font_);
        preferencesPanel_.setFont(&font_);
        leftDock_.setFont(&font_);
    }
    uiState_.timeScale = config::parameterDouble(parameters_, "time_scale", 1.0);
    std::cout << "AgentBioSimCpp Phase 24.2: painel lateral (Editor / Substrato / Labels) ("
              << runner_.species().size() << " species, " << runner_.genomes().size()
              << " genomes, " << runner_.foods().size() << " foods, "
              << runner_.obstacles().size() << " obstacles).\n";
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
        // Phase 23.1: preferences windows + popups have highest priority.
        // Then UI panel (menu/toolbar/dropdowns), then canvas via InputRouter.
        const sf::Vector2u vp = window_.getSize();
        // Phase 24.2: top strip height = menu bar + toolbar (left dock sits below it).
        const float topStripH = uiPanel_.metrics().menuBarHeight + uiPanel_.metrics().toolbarHeight;
        if (event.type == sf::Event::MouseButtonPressed)
        {
            const int sx = event.mouseButton.x;
            const int sy = event.mouseButton.y;
            if (preferencesPanel_.pointInsideAnyWindow(sx, sy, vp, uiState_.preferences))
            {
                if (event.mouseButton.button == sf::Mouse::Left)
                {
                    static_cast<void>(preferencesPanel_.handleMouseClick(sx, sy, vp,
                                                                            parameters_,
                                                                            uiState_.preferences,
                                                                            commandQueue_));
                }
                continue;
            }
            // Phase 24.2: persistent left dock (Editor/Substrato/Labels).
            if (leftDock_.pointInsideDock(sx, sy, vp, topStripH, uiState_.preferences))
            {
                if (event.mouseButton.button == sf::Mouse::Left)
                {
                    static_cast<void>(leftDock_.handleMouseClick(sx, sy, vp, topStripH,
                                                                    parameters_, runner_,
                                                                    uiState_.preferences,
                                                                    commandQueue_));
                }
                continue;
            }
            if (uiPanel_.pointInsidePanel(sx, sy, uiState_))
            {
                if (event.mouseButton.button == sf::Mouse::Left)
                {
                    static_cast<void>(uiPanel_.handleMouseClick(sx, sy, runner_, uiState_,
                                                                   commandQueue_));
                }
                continue;
            }
        }
        // Phase 23.2: drag prefs window when a drag is in progress; otherwise
        // let the InputRouter receive the move.
        if (event.type == sf::Event::MouseMoved)
        {
            if (uiState_.preferences.draggingTab >= 0 || uiState_.preferences.draggingHelp ||
                uiState_.velocitySliderDragging)
            {
                if (uiState_.velocitySliderDragging)
                {
                    const float x = static_cast<float>(event.mouseMove.x) -
                                        uiState_.velocitySliderTrackX;
                    const float w = uiState_.velocitySliderTrackW;
                    const float rel = std::clamp(x / std::max(1.0F, w), 0.0F, 1.0F);
                    const double minV = 0.1;
                    const double maxV = 50.0;
                    const double v = std::pow(10.0,
                        std::log10(minV) + static_cast<double>(rel) *
                            (std::log10(maxV) - std::log10(minV)));
                    commandQueue_.push(ui::CmdSetTimeScale{v});
                }
                else
                {
                    preferencesPanel_.handleMouseMove(event.mouseMove.x, event.mouseMove.y,
                                                        vp, uiState_.preferences, commandQueue_);
                }
                continue;
            }
        }
        if (event.type == sf::Event::MouseButtonReleased &&
            event.mouseButton.button == sf::Mouse::Left)
        {
            if (uiState_.preferences.draggingTab >= 0 || uiState_.preferences.draggingHelp)
            {
                preferencesPanel_.handleMouseRelease(event.mouseButton.x, event.mouseButton.y,
                                                       vp, uiState_.preferences, commandQueue_);
                continue;
            }
            if (uiState_.velocitySliderDragging)
            {
                uiState_.velocitySliderDragging = false;
                continue;
            }
        }
        // Phase 23.2: inline text editor for numeric parameters.
        if (!uiState_.preferences.editingParam.empty())
        {
            if (event.type == sf::Event::TextEntered)
            {
                const auto u = event.text.unicode;
                if (u == 8U)  // backspace
                {
                    if (!uiState_.preferences.editingBuffer.empty())
                    {
                        uiState_.preferences.editingBuffer.pop_back();
                    }
                }
                else if (u >= 32U && u < 127U)
                {
                    const char ch = static_cast<char>(u);
                    if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-' ||
                        ch == '+' || ch == 'e' || ch == 'E')
                    {
                        uiState_.preferences.editingBuffer.push_back(ch);
                    }
                }
                continue;
            }
            if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Enter ||
                    event.key.code == sf::Keyboard::Return)
                {
                    commandQueue_.push(ui::CmdCommitEditParameter{});
                    continue;
                }
                if (event.key.code == sf::Keyboard::Escape)
                {
                    commandQueue_.push(ui::CmdCancelEditParameter{});
                    continue;
                }
                // Swallow all other keys while editing so the canvas tools
                // do not react to digits/letters.
                continue;
            }
        }
        // Phase 24.2: inline editor for a species/label name in the Labels tab.
        if (uiState_.preferences.editingSpeciesId != 0U)
        {
            if (event.type == sf::Event::TextEntered)
            {
                const auto u = event.text.unicode;
                if (u == 8U)
                {
                    if (!uiState_.preferences.editingSpeciesBuffer.empty())
                        uiState_.preferences.editingSpeciesBuffer.pop_back();
                }
                else if (u >= 32U && u < 127U)
                {
                    uiState_.preferences.editingSpeciesBuffer.push_back(static_cast<char>(u));
                }
                continue;
            }
            if (event.type == sf::Event::KeyPressed)
            {
                if (event.key.code == sf::Keyboard::Enter ||
                    event.key.code == sf::Keyboard::Return)
                {
                    commandQueue_.push(ui::CmdCommitEditSpeciesName{});
                    continue;
                }
                if (event.key.code == sf::Keyboard::Escape)
                {
                    commandQueue_.push(ui::CmdCancelEditSpeciesName{});
                    continue;
                }
                continue;
            }
        }
        // Phase 23.1: scroll wheel — route to a prefs window when one is under
        // the cursor; otherwise let the InputRouter zoom the canvas.
        if (event.type == sf::Event::MouseWheelScrolled)
        {
            const int sx = static_cast<int>(event.mouseWheelScroll.x);
            const int sy = static_cast<int>(event.mouseWheelScroll.y);
            if (preferencesPanel_.handleMouseWheel(sx, sy, vp, event.mouseWheelScroll.delta,
                                                      uiState_.preferences, commandQueue_))
            {
                continue;
            }
            if (leftDock_.handleMouseWheel(sx, sy, vp, topStripH, event.mouseWheelScroll.delta,
                                              uiState_.preferences, commandQueue_))
            {
                continue;
            }
        }
        // Phase 23.1: Esc closes the topmost open prefs surface first.
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
        {
            if (!uiState_.preferences.openPopup.empty())
            {
                commandQueue_.push(ui::CmdClosePrefsPopup{});
                continue;
            }
            if (uiState_.preferences.helpWindowOpen)
            {
                commandQueue_.push(ui::CmdCloseHelpWindow{});
                continue;
            }
            if (uiState_.preferences.substratePlaceholderOpen)
            {
                commandQueue_.push(ui::CmdCloseSubstratePlaceholder{});
                continue;
            }
            for (int t = 0; t < static_cast<int>(config::PrefsTab::Count); ++t)
            {
                if (uiState_.preferences.windowOpen[static_cast<std::size_t>(t)])
                {
                    commandQueue_.push(ui::CmdClosePreferencesWindow{t});
                    break;
                }
            }
            // Fall through to InputRouter so Esc also clears selection.
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
    float dockW = uiState_.preferences.dockVisible ? ui::UiLeftDock::kDockW : 0.0F;
    if (dockW > static_cast<float>(vp.x) * 0.6F) dockW = 0.0F;  // safety on tiny windows
    const sf::Vector2u fitVp{vp.x - static_cast<unsigned int>(dockW), vp.y};
    camera_.fitWorld(toSfml(runner_.world().minBounds()), toSfml(runner_.world().maxBounds()),
                       fitVp, kWorldPaddingPixels);
    if (dockW > 0.0F)
    {
        camera_.pan({dockW * 0.5F, 0.0F});
    }
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
                uiState_.showHelp = !uiState_.showHelp;
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
                uiState_.preferences.pendingValues[c.name] = c.value;
                ++uiState_.preferences.controlInteractions;
            }
            else if constexpr (std::is_same_v<T, ui::CmdApplyPreferences>)
            {
                const unsigned int flags =
                    ui::prefsApplyPending(parameters_, uiState_.preferences);
                if (flags & config::ApplyFlag::RefreshRenderer)
                {
                    configureRenderOptions();
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
            else if constexpr (std::is_same_v<T, ui::CmdApplyGenomeToSpecies> ||
                                 std::is_same_v<T, ui::CmdApplyPopulation> ||
                                 std::is_same_v<T, ui::CmdApplyEnvironment>)
            {
                // Phase 24: bake pending edits into registry + reset so new
                // agents read the new defaults. Existing agents are recreated
                // from the species defaults during spawnInitial().
                const unsigned int flags = ui::prefsApplyPending(parameters_,
                    uiState_.preferences);
                if (flags & config::ApplyFlag::RefreshRenderer) configureRenderOptions();
                configureFromParameters();
                runner_.reset();
                // Phase 24.1 fix: DO NOT auto-fit the camera here. The user's
                // intent on "Aplicar ambiente" is to grow/shrink the substrate
                // while keeping the visual size of organisms / food the same.
                // Auto-fit would rezoom and change the apparent size of every
                // entity. The user can press F manually to fit the world.
            }
            else if constexpr (std::is_same_v<T, ui::CmdApplyGenomeToSelected>)
            {
                // Phase 24: bake pending edits into registry, then delete the
                // selected agents so the runner respawns them with the new
                // defaults on the next rescue tick. Full per-agent genome
                // mutation lives in Fase 25 (painel de agente selecionado).
                static_cast<void>(ui::prefsApplyPending(parameters_,
                    uiState_.preferences));
                runner_.deleteAgents(uiState_.selection.ids());
                uiState_.selection.clear();
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
                // Phase 24.2: per-species neural reset needs GenomeStore/brain
                // lifetime APIs — deferred to Fase 25. The Labels button is
                // rendered DISABLED, so this arm should not be reached, but we
                // keep it as an explicit no-op (never a fake success).
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
    lastStepsThisFrame_ = timestep_.beginFrame(realDeltaSeconds);
    for (unsigned int step = 0; step < lastStepsThisFrame_; ++step)
    {
        runner_.step(timestep_.fixedDeltaSeconds());
        ++simulatedSteps_;
    }
}

void App::render()
{
    if (!renderOptions_.renderEnabled)
    {
        window_.clear();
        window_.display();
        ++frames_;
        return;
    }
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

    lastRenderStats_ = renderer_.render(window_, camera_, runner_.world(),
                                          runner_.agents(), runner_.foods(),
                                          renderOptions_, nullptr, obstaclePtr,
                                          &selInput);
    // Phase 24.2: persistent left dock below the top strip, drawn before the
    // top menu so menu dropdowns overlay it.
    const float topStripH = uiPanel_.metrics().menuBarHeight + uiPanel_.metrics().toolbarHeight;
    leftDock_.draw(window_, topStripH, parameters_, runner_, uiState_.preferences);
    uiPanel_.draw(window_, runner_, uiState_);
    // Phase 23: preferences window sits above panel + canvas.
    preferencesPanel_.draw(window_, parameters_, uiState_.preferences);
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
