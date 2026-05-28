#pragma once

#include "config/ParameterRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace agentbiosim::perception
{
enum class RetinaInputMode : std::uint8_t
{
    DistanceOnly,
    ColorDistance,
    ColorPlusDistance,
    ColorOnly
};

enum class RetinaChannel : std::uint8_t
{
    R = 0,
    G = 1,
    B = 2,
    RD = 3,
    GD = 4,
    BD = 5,
    D = 6
};

inline std::string normalizeRetinaText(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        const char lower = static_cast<char>(std::tolower(ch));
        return lower == '-' || lower == ' ' ? '_' : lower;
    });
    return value;
}

inline RetinaInputMode normalizeInputMode(const std::string& raw)
{
    const std::string value = normalizeRetinaText(raw);
    if (value == "color_distance" || value == "weighted" || value == "color_weighted" ||
        value == "cor_distancia")
    {
        return RetinaInputMode::ColorDistance;
    }
    if (value == "color_plus_distance" || value == "separate" || value == "separadas" ||
        value == "cor_distancia_separadas")
    {
        return RetinaInputMode::ColorPlusDistance;
    }
    if (value == "color_only" || value == "cor_apenas")
    {
        return RetinaInputMode::ColorOnly;
    }
    return RetinaInputMode::DistanceOnly;
}

inline const char* inputModeName(const RetinaInputMode mode)
{
    switch (mode)
    {
    case RetinaInputMode::DistanceOnly:
        return "distance_only";
    case RetinaInputMode::ColorDistance:
        return "color_distance";
    case RetinaInputMode::ColorPlusDistance:
        return "color_plus_distance";
    case RetinaInputMode::ColorOnly:
        return "color_only";
    }
    return "distance_only";
}

inline const char* channelName(const RetinaChannel channel)
{
    switch (channel)
    {
    case RetinaChannel::R:
        return "R";
    case RetinaChannel::G:
        return "G";
    case RetinaChannel::B:
        return "B";
    case RetinaChannel::RD:
        return "RD";
    case RetinaChannel::GD:
        return "GD";
    case RetinaChannel::BD:
        return "BD";
    case RetinaChannel::D:
        return "D";
    }
    return "D";
}

struct RetinaConfig
{
    std::string visionMode = "single";
    double visionRadius = 120.0;
    std::size_t retinaCount = 18;
    double fovDegrees = 180.0;
    std::size_t eyeCount = 1;
    double eyeAngleDegrees = 60.0;
    double eyeSeparationDegrees = 45.0;

    bool seeFood = true;
    bool seeAgents = false;
    bool seePredators = false;
    bool seeObstacles = false;
    bool seeAll = false;
    bool seeThroughWalls = true;

    RetinaInputMode inputMode = RetinaInputMode::DistanceOnly;
    bool channelR = false;
    bool channelG = false;
    bool channelB = false;
    bool channelD = true;

    [[nodiscard]] std::vector<RetinaChannel> activeChannels() const
    {
        if (inputMode == RetinaInputMode::DistanceOnly)
        {
            return {RetinaChannel::D};
        }

        std::vector<RetinaChannel> channels;
        if (inputMode == RetinaInputMode::ColorDistance)
        {
            if (channelR)
            {
                channels.push_back(RetinaChannel::RD);
            }
            if (channelG)
            {
                channels.push_back(RetinaChannel::GD);
            }
            if (channelB)
            {
                channels.push_back(RetinaChannel::BD);
            }
            return channels.empty() ? std::vector<RetinaChannel>{RetinaChannel::D} : channels;
        }

        if (inputMode == RetinaInputMode::ColorPlusDistance)
        {
            if (channelR)
            {
                channels.push_back(RetinaChannel::R);
            }
            if (channelG)
            {
                channels.push_back(RetinaChannel::G);
            }
            if (channelB)
            {
                channels.push_back(RetinaChannel::B);
            }
            if (channels.empty())
            {
                return {RetinaChannel::D};
            }
            channels.push_back(RetinaChannel::D);
            return channels;
        }

        if (inputMode == RetinaInputMode::ColorOnly)
        {
            if (channelR)
            {
                channels.push_back(RetinaChannel::R);
            }
            if (channelG)
            {
                channels.push_back(RetinaChannel::G);
            }
            if (channelB)
            {
                channels.push_back(RetinaChannel::B);
            }
            return channels.empty() ? std::vector<RetinaChannel>{RetinaChannel::D} : channels;
        }

        return {RetinaChannel::D};
    }

    [[nodiscard]] std::size_t channelCount() const
    {
        return activeChannels().size();
    }

    [[nodiscard]] std::size_t inputSize() const
    {
        return std::max<std::size_t>(1U, retinaCount) * std::max<std::size_t>(1U, eyeCount) * channelCount();
    }

    [[nodiscard]] std::string channelDescription() const
    {
        const auto channels = activeChannels();
        std::string desc;
        for (std::size_t i = 0; i < channels.size(); ++i)
        {
            if (i > 0)
            {
                desc += '/';
            }
            desc += channelName(channels[i]);
        }
        return desc;
    }
};

[[nodiscard]] RetinaConfig retinaConfigFromRegistry(const config::ParameterRegistry& registry,
                                                    const std::string& prefix);
} // namespace agentbiosim::perception
