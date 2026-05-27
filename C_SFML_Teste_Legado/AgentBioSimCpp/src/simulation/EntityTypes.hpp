#pragma once

#include "simulation/World.hpp"

#include <cstdint>

namespace agentbiosim::simulation
{
using SpeciesId = std::uint32_t;

enum class AgentTypeCode : std::uint16_t
{
    Organism = 0,
    LegacyBacteria = 1,
    LegacyPredator = 2
};

enum class FoodKind : std::uint16_t
{
    Instant = 0,
    Chunk = 1
};

struct ColorRgb
{
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
};

struct AgentSpawn
{
    Vec2 position{};
    Vec2 velocity{};
    double angle = 0.0;
    double radius = 9.0;
    double energy = 100.0;
    double age = 0.0;
    ColorRgb color{220, 220, 220};
    SpeciesId speciesId = 0;
    AgentTypeCode typeCode = AgentTypeCode::LegacyBacteria;
};

struct FoodSpawn
{
    Vec2 position{};
    double radius = 5.0;
    double energy = 25.0;
    double initialEnergy = 25.0;
    ColorRgb color{220, 30, 30};
    FoodKind kind = FoodKind::Instant;
};
} // namespace agentbiosim::simulation
