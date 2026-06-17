#include "ui/InputRouter.hpp"

#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/Keyboard.hpp>

#include <cmath>

namespace agentbiosim::ui
{
namespace
{
simulation::Vec2 screenToWorld(const render::Camera2D& camera, const sf::Vector2u viewport,
                                const int sx, const int sy)
{
    const auto v = camera.screenToWorld({static_cast<float>(sx), static_cast<float>(sy)},
                                          viewport);
    return {static_cast<double>(v.x), static_cast<double>(v.y)};
}

} // namespace

void InputRouter::handleEvent(const sf::Event& ev,
                                const sf::Vector2u viewportSize,
                                const render::Camera2D& camera,
                                const sim::SimulationRunner& runner,
                                UiState& uiState,
                                CommandQueue& queue)
{
    static_cast<void>(runner);

    switch (ev.type)
    {
    case sf::Event::MouseMoved:
    {
        ++uiState.mouseEvents;
        const int sx = ev.mouseMove.x;
        const int sy = ev.mouseMove.y;
        const auto worldPos = screenToWorld(camera, viewportSize, sx, sy);
        uiState.lastMouseWorld = worldPos;
        uiState.lastMouseValid = true;

        // Phase 22.1: event-driven pan. Uses event coords (window-relative),
        // never sf::Mouse::getPosition which returns desktop coords and breaks
        // on maximize.
        if (panActive_)
        {
            const sf::Vector2i current{sx, sy};
            const sf::Vector2i delta = current - lastMouseScreen_;
            if (delta.x != 0 || delta.y != 0)
            {
                queue.push(CmdPanCameraScreen{static_cast<double>(delta.x),
                                                static_cast<double>(delta.y)});
                lastMouseScreen_ = current;
            }
        }

        // Marquee drag preview.
        if (marqueeActive_)
        {
            uiState.marquee.endWorld = worldPos;
        }

        // Lasso drag accumulator.
        if (lassoActive_ && uiState.activeTool == CanvasTool::LassoSelect)
        {
            if (uiState.lasso.points.empty() ||
                std::hypot(uiState.lasso.points.back().x - worldPos.x,
                            uiState.lasso.points.back().y - worldPos.y) > 1.0)
            {
                uiState.lasso.points.push_back(worldPos);
            }
        }

        // Phase 22.1: paint/eraser stroke. Emit interpolated stamps from the
        // last stamped position to the current mouse position so dragging the
        // brush draws a continuous trail rather than a single stamp.
        if (paintActive_ && uiState.activeTool == CanvasTool::PaintObstacle)
        {
            const auto to8 = [](float c) { return static_cast<int>(c * 255.0F + 0.5F); };
            queue.push(CmdPaintObstacleStroke{lastStampWorld_, worldPos, uiState.brushRadius,
                                                to8(uiState.obstacleColor[0]),
                                                to8(uiState.obstacleColor[1]),
                                                to8(uiState.obstacleColor[2])});
            lastStampWorld_ = worldPos;
        }
        if (eraserActive_ && uiState.activeTool == CanvasTool::EraseObstacle)
        {
            queue.push(CmdEraseObstacleStroke{lastStampWorld_, worldPos,
                                                uiState.brushRadius * 1.5});
            lastStampWorld_ = worldPos;
        }
        break;
    }
    case sf::Event::MouseButtonPressed:
    {
        ++uiState.mouseEvents;
        const int sx = ev.mouseButton.x;
        const int sy = ev.mouseButton.y;
        const auto worldPos = screenToWorld(camera, viewportSize, sx, sy);

        // Right/middle mouse button always pans (Phase 22.1: Pan canvas tool
        // was removed from the toolbar, so pan is purely right-mouse-driven).
        if (ev.mouseButton.button == sf::Mouse::Right ||
            ev.mouseButton.button == sf::Mouse::Middle)
        {
            panActive_ = true;
            lastMouseScreen_ = {sx, sy};
            break;
        }
        if (ev.mouseButton.button == sf::Mouse::Left)
        {
            // Any left-click on the canvas closes open menus.
            queue.push(CmdCloseAllMenus{});

            switch (uiState.activeTool)
            {
            case CanvasTool::Select:
                queue.push(CmdSelectAtWorldPoint{worldPos, shift_ || ctrl_, 12.0});
                break;
            case CanvasTool::RectangleSelect:
                marqueeActive_ = true;
                marqueeStartWorld_ = worldPos;
                uiState.marquee.startWorld = worldPos;
                uiState.marquee.endWorld = worldPos;
                uiState.marquee.active = true;
                break;
            case CanvasTool::LassoSelect:
                lassoActive_ = true;
                uiState.lasso.points.clear();
                uiState.lasso.points.push_back(worldPos);
                uiState.lasso.active = true;
                break;
            case CanvasTool::AddFood:
                queue.push(CmdSpawnFoodAt{worldPos, 5.0, 25.0});
                break;
            case CanvasTool::AddAgent:
                queue.push(CmdSpawnAgentAt{worldPos, 9.0});
                break;
            case CanvasTool::PaintObstacle:
            {
                paintActive_ = true;
                lastStampWorld_ = worldPos;
                const auto to8 = [](float c) { return static_cast<int>(c * 255.0F + 0.5F); };
                queue.push(CmdPaintObstacleAt{worldPos, uiState.brushRadius,
                                                to8(uiState.obstacleColor[0]),
                                                to8(uiState.obstacleColor[1]),
                                                to8(uiState.obstacleColor[2])});
                break;
            }
            case CanvasTool::EraseObstacle:
                eraserActive_ = true;
                lastStampWorld_ = worldPos;
                queue.push(CmdEraseObstacleAt{worldPos, uiState.brushRadius * 1.5});
                break;
            case CanvasTool::Delete:
                queue.push(CmdDeleteSelected{});
                break;
            case CanvasTool::Move:
            case CanvasTool::None:
            case CanvasTool::Pan:
                break;
            }
        }
        break;
    }
    case sf::Event::MouseButtonReleased:
    {
        ++uiState.mouseEvents;
        if (ev.mouseButton.button == sf::Mouse::Right ||
            ev.mouseButton.button == sf::Mouse::Middle)
        {
            panActive_ = false;
            break;
        }
        if (ev.mouseButton.button == sf::Mouse::Left)
        {
            if (marqueeActive_)
            {
                marqueeActive_ = false;
                uiState.marquee.active = false;
                queue.push(CmdSelectRect{uiState.marquee.startWorld, uiState.marquee.endWorld,
                                          shift_ || ctrl_});
            }
            else if (lassoActive_)
            {
                lassoActive_ = false;
                uiState.lasso.active = false;
                if (uiState.lasso.points.size() >= 3U)
                {
                    queue.push(CmdSelectLasso{uiState.lasso.points, shift_ || ctrl_});
                }
                uiState.lasso.points.clear();
            }
            paintActive_ = false;
            eraserActive_ = false;
        }
        break;
    }
    case sf::Event::MouseWheelScrolled:
    {
        ++uiState.mouseEvents;
        const double factor = ev.mouseWheelScroll.delta > 0.0F ? 1.1 : 1.0 / 1.1;
        queue.push(CmdZoomCameraAt{factor,
                                     static_cast<double>(ev.mouseWheelScroll.x),
                                     static_cast<double>(ev.mouseWheelScroll.y)});
        break;
    }
    case sf::Event::KeyPressed:
    {
        ++uiState.keyEvents;
        const auto key = ev.key.code;
        shift_ = ev.key.shift;
        ctrl_ = ev.key.control;
        switch (key)
        {
        case sf::Keyboard::Space:   queue.push(CmdPauseToggle{}); break;
        case sf::Keyboard::Escape:  queue.push(CmdCloseAllMenus{});
                                     queue.push(CmdClearSelection{});
                                     queue.push(CmdSetCanvasTool{CanvasTool::Select}); break;
        case sf::Keyboard::Delete:  queue.push(CmdDeleteSelected{}); break;
        case sf::Keyboard::R:       queue.push(CmdResetSimulation{}); break;
        case sf::Keyboard::F:       queue.push(CmdFitWorldCamera{}); break;
        case sf::Keyboard::T:       queue.push(CmdToggleSimpleRender{}); break;
        case sf::Keyboard::V:       queue.push(CmdToggleVisionDebug{}); break;
        case sf::Keyboard::H:       queue.push(CmdToggleHelpPanel{}); break;
        case sf::Keyboard::S:       queue.push(CmdSetCanvasTool{CanvasTool::Select}); break;
        case sf::Keyboard::G:       queue.push(CmdSetCanvasTool{CanvasTool::AddFood}); break;
        case sf::Keyboard::A:       queue.push(CmdSetCanvasTool{CanvasTool::AddAgent}); break;
        case sf::Keyboard::M:       queue.push(CmdSetCanvasTool{CanvasTool::Move}); break;
        case sf::Keyboard::D:       queue.push(CmdSetCanvasTool{CanvasTool::Delete}); break;
        case sf::Keyboard::B:       queue.push(CmdSetCanvasTool{CanvasTool::PaintObstacle}); break;
        case sf::Keyboard::X:       queue.push(CmdSetCanvasTool{CanvasTool::EraseObstacle}); break;
        case sf::Keyboard::L:       queue.push(CmdSetCanvasTool{CanvasTool::LassoSelect}); break;
        case sf::Keyboard::Q:       queue.push(CmdSetCanvasTool{CanvasTool::RectangleSelect}); break;
        // Phase 31: camera pan is arrows-only. W used to pan while A/S/D were
        // tool shortcuts, which made the help text ("WASD") a lie — letters are
        // tools, arrows are camera.
        case sf::Keyboard::Up:      queue.push(CmdPanCameraScreen{0.0, -40.0}); break;
        case sf::Keyboard::Down:    queue.push(CmdPanCameraScreen{0.0, 40.0}); break;
        case sf::Keyboard::Left:    queue.push(CmdPanCameraScreen{-40.0, 0.0}); break;
        case sf::Keyboard::Right:   queue.push(CmdPanCameraScreen{40.0, 0.0}); break;
        default: break;
        }
        break;
    }
    case sf::Event::KeyReleased:
    {
        ++uiState.keyEvents;
        shift_ = ev.key.shift;
        ctrl_ = ev.key.control;
        break;
    }
    default:
        break;
    }
}

void InputRouter::update(const sf::Vector2u viewportSize,
                          const render::Camera2D& camera,
                          UiState& uiState,
                          CommandQueue& queue)
{
    // Phase 22.1: pan is now driven by MouseMoved events (see handleEvent).
    // The polled-Mouse::getPosition path was removed because it returned
    // desktop coords and broke after maximize/resize. update() remains here
    // because the router contract is unchanged from Phase 22.
    static_cast<void>(viewportSize);
    static_cast<void>(camera);
    static_cast<void>(uiState);
    static_cast<void>(queue);
}
} // namespace agentbiosim::ui
