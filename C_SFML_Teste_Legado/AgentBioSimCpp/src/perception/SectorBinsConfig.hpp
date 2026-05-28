#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace agentbiosim::perception
{
enum class BinsMode : std::uint8_t
{
    Nearest = 0,
    Strongest = 1,
    SumSaturating = 2,
    WeightedAverage = 3
};

enum class BinsDistribution : std::uint8_t
{
    Linear = 0,
    NearDetail = 1
};

enum class BinsFalloff : std::uint8_t
{
    Linear = 0,
    Quadratic = 1,
    Step = 2,
    None = 3
};

enum class BinsProjection : std::uint8_t
{
    Center = 0,
    CenterEdges = 1,
    ApparentSize = 2
};

inline std::string normalizeBinsText(std::string value)
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

inline BinsMode normalizeBinsMode(const std::string& raw)
{
    const std::string value = normalizeBinsText(raw);
    if (value == "strongest" || value == "max" || value == "mais_forte")
    {
        return BinsMode::Strongest;
    }
    if (value == "sum" || value == "sum_saturating" || value == "soma" || value == "soma_saturada")
    {
        return BinsMode::SumSaturating;
    }
    if (value == "weighted_average" || value == "weighted" || value == "media" ||
        value == "media_ponderada")
    {
        return BinsMode::WeightedAverage;
    }
    return BinsMode::Nearest;
}

inline BinsDistribution normalizeBinsDistribution(const std::string& raw)
{
    const std::string value = normalizeBinsText(raw);
    if (value == "linear")
    {
        return BinsDistribution::Linear;
    }
    return BinsDistribution::NearDetail;
}

inline BinsFalloff normalizeBinsFalloff(const std::string& raw)
{
    const std::string value = normalizeBinsText(raw);
    if (value == "quadratic")
    {
        return BinsFalloff::Quadratic;
    }
    if (value == "step")
    {
        return BinsFalloff::Step;
    }
    if (value == "none")
    {
        return BinsFalloff::None;
    }
    return BinsFalloff::Linear;
}

inline BinsProjection normalizeBinsProjection(const std::string& raw)
{
    const std::string value = normalizeBinsText(raw);
    if (value == "center_edges" || value == "edges" || value == "centro_bordas")
    {
        return BinsProjection::CenterEdges;
    }
    if (value == "apparent_size" || value == "apparent" || value == "tamanho_aparente")
    {
        return BinsProjection::ApparentSize;
    }
    return BinsProjection::Center;
}

inline const char* binsModeName(const BinsMode mode)
{
    switch (mode)
    {
    case BinsMode::Nearest:
        return "nearest";
    case BinsMode::Strongest:
        return "strongest";
    case BinsMode::SumSaturating:
        return "sum_saturating";
    case BinsMode::WeightedAverage:
        return "weighted_average";
    }
    return "nearest";
}

inline const char* binsDistributionName(const BinsDistribution dist)
{
    return dist == BinsDistribution::Linear ? "linear" : "near_detail";
}

inline const char* binsFalloffName(const BinsFalloff falloff)
{
    switch (falloff)
    {
    case BinsFalloff::Linear:
        return "linear";
    case BinsFalloff::Quadratic:
        return "quadratic";
    case BinsFalloff::Step:
        return "step";
    case BinsFalloff::None:
        return "none";
    }
    return "linear";
}

inline const char* binsProjectionName(const BinsProjection projection)
{
    switch (projection)
    {
    case BinsProjection::Center:
        return "center";
    case BinsProjection::CenterEdges:
        return "center_edges";
    case BinsProjection::ApparentSize:
        return "apparent_size";
    }
    return "center";
}

struct SectorBinsConfig
{
    BinsMode mode = BinsMode::Nearest;
    int subdivisions = 5;
    BinsDistribution distribution = BinsDistribution::NearDetail;
    BinsFalloff falloff = BinsFalloff::Linear;
    BinsProjection projection = BinsProjection::Center;
    int candidateLimit = 128;
    bool obstaclesBlockVision = false;
};
} // namespace agentbiosim::perception
