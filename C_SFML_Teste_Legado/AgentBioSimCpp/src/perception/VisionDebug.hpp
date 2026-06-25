#pragma once

#include "perception/VisionStrategy.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace agentbiosim::perception
{
struct VisionRayDebug
{
    double startX = 0.0;
    double startY = 0.0;
    double dirX = 0.0;
    double dirY = 0.0;
    double maxLength = 0.0;
    double hitDistance = -1.0;
    bool hit = false;
    double hitX = 0.0;
    double hitY = 0.0;
    double activation = 0.0;
    double hitColorR = 0.0;
    double hitColorG = 0.0;
    double hitColorB = 0.0;
    std::size_t eyeIndex = 0;
    std::size_t rayIndex = 0;
};

struct VisionDebugData
{
    std::uint64_t agentId = 0;
    bool active = false;
    VisionMode mode = VisionMode::Single;
    std::size_t retinaCount = 0;
    std::size_t eyeCount = 0;
    double visionRadius = 0.0;
    double fovDegrees = 0.0;
    // Sector/bins: how the distance axis is segmented, so the overlay can draw the
    // longitudinal (radial) bin grid that matches the perception. 1 = no radial split.
    std::size_t distanceSubdivisions = 1;
    bool nearDetail = false;  // true = sqrt distribution (finer bins up close)
    std::vector<VisionRayDebug> rays;

    void clear() noexcept
    {
        agentId = 0;
        active = false;
        rays.clear();
    }

    [[nodiscard]] std::size_t hitCount() const noexcept
    {
        std::size_t count = 0;
        for (const auto& ray : rays)
        {
            if (ray.hit)
            {
                ++count;
            }
        }
        return count;
    }
};
} // namespace agentbiosim::perception
