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
        lastMouseScreen_ = {ev.mouseMove.x, ev.mouseMove.y};
        uiState.lastMouseWorld = screenToWorld(camera, viewportSize,
                                                 ev.mouseMove.x, ev.mouseMove.y);
        uiState.lastMouseValid = true;
        if (marqueeActive_)
        {
            uiState.marquee.endWorld = uiState.lastMouseWorld;
        }
        if (lassoActive_ && uiState.activeTool == CanvasTool::LassoSelect)
        {
            // Add point if it moved enough.
            if (uiState.lasso.points.empty() ||
                std::hypot(uiState.lasso.points.back().x - uiState.lastMouseWorld.x,
                            uiState.lasso.points.back().y - uiState.lastMouseWorld.y) > 1.0)
            {
                uiState.lasso.points.push_back(uiState.lastMouseWorld);
            }
        }
        break;
    }
    case sf::Event::MouseButtonPressed:
    {
        ++uiState.mouseEvents;
        const int sx = ev.mouseButton.x;
        const int sy = ev.mouseButton.y;
        const auto worldPos = screenToWorld(camera, viewportSize, sx, sy);
        if (ev.mouseButton.button == sf::Mouse::Right ||
            ev.mouseButton.button == sf::Mouse::Middle ||
            uiState.activeTool == CanvasTool::Pan)
        {
            panActive_ = true;
            lastMouseScreen_ = {sx, sy};
            break;
        }
        if (ev.mouseButton.button == sf::Mouse::Left)
        {
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
                queue.push(CmdPaintObstacleAt{worldPos, uiState.brushRadius});
                break;
            case CanvasTool::EraseObstacle:
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
        case sf::Keyboard::Escape:  queue.push(CmdClearSelection{});
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
        case sf::Keyboard::W:       queue.push(CmdPanCameraScreen{0.0, -40.0}); break;
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
    if (panActive_)
    {
        const auto current = sf::Mouse::getPosition();
        const sf::Vector2i delta = current - lastMouseScreen_;
        if (delta.x != 0 || delta.y != 0)
        {
            queue.push(CmdPanCameraScreen{static_cast<double>(delta.x),
                                            static_cast<double>(delta.y)});
            lastMouseScreen_ = current;
        }
    }
    static_cast<void>(viewportSize);
    static_cast<void>(camera);
    static_cast<void>(uiState);
}
} // namespace agentbiosim::ui
