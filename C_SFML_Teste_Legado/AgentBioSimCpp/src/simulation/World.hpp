#pragma once

namespace agentbiosim::simulation
{
struct Vec2
{
    double x = 0.0;
    double y = 0.0;
};

enum class WorldShape
{
    Rectangular,
    Circular
};

struct WorldConfig
{
    WorldShape shape = WorldShape::Rectangular;
    double width = 1000.0;
    double height = 700.0;
    double radius = 400.0;
    Vec2 center{500.0, 350.0};
};

class World
{
public:
    World() = default;
    explicit World(WorldConfig config);

    void configure(WorldConfig config);

    [[nodiscard]] const WorldConfig& config() const noexcept;
    [[nodiscard]] WorldShape shape() const noexcept;
    [[nodiscard]] double width() const noexcept;
    [[nodiscard]] double height() const noexcept;
    [[nodiscard]] double radius() const noexcept;
    [[nodiscard]] Vec2 center() const noexcept;

    [[nodiscard]] bool isInside(Vec2 position, double radius = 0.0) const noexcept;
    [[nodiscard]] Vec2 clampPosition(Vec2 position, double radius = 0.0) const noexcept;
    [[nodiscard]] Vec2 wrapPosition(Vec2 position) const noexcept;
    [[nodiscard]] double distanceToWall(Vec2 position) const noexcept;
    [[nodiscard]] Vec2 minBounds() const noexcept;
    [[nodiscard]] Vec2 maxBounds() const noexcept;

private:
    WorldConfig config_{};
};
} // namespace agentbiosim::simulation
