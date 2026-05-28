#pragma once

#include <cstdint>
#include <string>

namespace agentbiosim::perception
{
enum class VisionMode : std::uint8_t
{
    Single = 0,
    Fullbody = 1,
    Sector = 2
};

inline VisionMode normalizeVisionMode(const std::string& raw)
{
    std::string value;
    value.reserve(raw.size());
    for (const char ch : raw)
    {
        const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        value.push_back(lower == '-' || lower == ' ' ? '_' : lower);
    }
    if (value == "fullbody" || value == "raycast" || value == "full_body" ||
        value == "fullbody_raycast" || value == "ray_cast" || value == "geometric")
    {
        return VisionMode::Fullbody;
    }
    if (value == "sector" || value == "bins" || value == "angular_bins" ||
        value == "setorial" || value == "sectorial" || value == "fast_sector")
    {
        return VisionMode::Sector;
    }
    return VisionMode::Single;
}

inline const char* visionModeName(const VisionMode mode)
{
    switch (mode)
    {
    case VisionMode::Single:
        return "single";
    case VisionMode::Fullbody:
        return "fullbody";
    case VisionMode::Sector:
        return "sector";
    }
    return "single";
}

inline bool isVisionModeImplementedInPhase11(const VisionMode mode)
{
    return mode == VisionMode::Single || mode == VisionMode::Fullbody;
}
} // namespace agentbiosim::perception
