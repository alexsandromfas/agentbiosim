#include "render/Camera2D.hpp"

#include <algorithm>

namespace agentbiosim::render
{
sf::Vector2f Camera2D::center() const noexcept
{
    return center_;
}

float Camera2D::zoom() const noexcept
{
    return zoom_;
}

float Camera2D::minZoom() const noexcept
{
    return minZoom_;
}

float Camera2D::maxZoom() const noexcept
{
    return maxZoom_;
}

void Camera2D::setCenter(const sf::Vector2f center) noexcept
{
    center_ = center;
}

void Camera2D::setZoom(const float zoom) noexcept
{
    zoom_ = std::max(minZoom_, std::min(zoom, maxZoom_));
}

void Camera2D::setZoomLimits(const float minZoom, const float maxZoom) noexcept
{
    minZoom_ = std::max(0.0001F, minZoom);
    maxZoom_ = std::max(minZoom_, maxZoom);
    setZoom(zoom_);
}

sf::Vector2f Camera2D::worldToScreen(const sf::Vector2f worldPosition, const sf::Vector2u viewportSize) const noexcept
{
    const sf::Vector2f viewportCenter{static_cast<float>(viewportSize.x) * 0.5F, static_cast<float>(viewportSize.y) * 0.5F};
    return {(worldPosition.x - center_.x) * zoom_ + viewportCenter.x,
            (worldPosition.y - center_.y) * zoom_ + viewportCenter.y};
}

sf::Vector2f Camera2D::screenToWorld(const sf::Vector2f screenPosition, const sf::Vector2u viewportSize) const noexcept
{
    const sf::Vector2f viewportCenter{static_cast<float>(viewportSize.x) * 0.5F, static_cast<float>(viewportSize.y) * 0.5F};
    return {((screenPosition.x - viewportCenter.x) / zoom_) + center_.x,
            ((screenPosition.y - viewportCenter.y) / zoom_) + center_.y};
}

void Camera2D::pan(const sf::Vector2f screenDelta) noexcept
{
    center_.x -= screenDelta.x / zoom_;
    center_.y -= screenDelta.y / zoom_;
}

void Camera2D::zoomBy(const float factor) noexcept
{
    if (factor > 0.0F)
    {
        setZoom(zoom_ * factor);
    }
}

void Camera2D::zoomAt(const float factor, const sf::Vector2f screenAnchor, const sf::Vector2u viewportSize) noexcept
{
    const sf::Vector2f before = screenToWorld(screenAnchor, viewportSize);
    zoomBy(factor);
    const sf::Vector2f after = screenToWorld(screenAnchor, viewportSize);
    center_.x += before.x - after.x;
    center_.y += before.y - after.y;
}

void Camera2D::fitWorld(const sf::Vector2f minWorld,
                        const sf::Vector2f maxWorld,
                        const sf::Vector2u viewportSize,
                        const float paddingPixels) noexcept
{
    const float width = std::max(1.0F, maxWorld.x - minWorld.x);
    const float height = std::max(1.0F, maxWorld.y - minWorld.y);
    const float availableWidth = std::max(1.0F, static_cast<float>(viewportSize.x) - (paddingPixels * 2.0F));
    const float availableHeight = std::max(1.0F, static_cast<float>(viewportSize.y) - (paddingPixels * 2.0F));

    center_ = {(minWorld.x + maxWorld.x) * 0.5F, (minWorld.y + maxWorld.y) * 0.5F};
    setZoom(std::min(availableWidth / width, availableHeight / height));
}
} // namespace agentbiosim::render
