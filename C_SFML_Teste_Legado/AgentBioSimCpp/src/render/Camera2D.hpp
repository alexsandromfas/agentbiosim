#pragma once

#include <SFML/System/Vector2.hpp>

namespace agentbiosim::render
{
class Camera2D
{
public:
    [[nodiscard]] sf::Vector2f center() const noexcept;
    [[nodiscard]] float zoom() const noexcept;
    [[nodiscard]] float minZoom() const noexcept;
    [[nodiscard]] float maxZoom() const noexcept;

    void setCenter(sf::Vector2f center) noexcept;
    void setZoom(float zoom) noexcept;
    void setZoomLimits(float minZoom, float maxZoom) noexcept;

    [[nodiscard]] sf::Vector2f worldToScreen(sf::Vector2f worldPosition, sf::Vector2u viewportSize) const noexcept;
    [[nodiscard]] sf::Vector2f screenToWorld(sf::Vector2f screenPosition, sf::Vector2u viewportSize) const noexcept;

    void pan(sf::Vector2f screenDelta) noexcept;
    void zoomBy(float factor) noexcept;
    void zoomAt(float factor, sf::Vector2f screenAnchor, sf::Vector2u viewportSize) noexcept;
    void fitWorld(sf::Vector2f minWorld, sf::Vector2f maxWorld, sf::Vector2u viewportSize, float paddingPixels) noexcept;

private:
    sf::Vector2f center_{0.0F, 0.0F};
    float zoom_ = 1.0F;
    float minZoom_ = 0.01F;
    float maxZoom_ = 20.0F;
};
} // namespace agentbiosim::render
