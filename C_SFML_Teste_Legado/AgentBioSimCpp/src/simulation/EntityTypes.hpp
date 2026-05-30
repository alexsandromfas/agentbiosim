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

enum class BodyShapeCode : std::uint16_t
{
    Ellipse = 0,
    Circle = 1
};

struct ColorRgb
{
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
};

using GenomeId = std::uint64_t;
inline constexpr GenomeId kInvalidGenomeId = 0;

struct AgentSpawn
{
    Vec2 position{};
    Vec2 velocity{};
    double angle = 0.0;
    double radius = 9.0;
    double energy = 100.0;
    double age = 0.0;
    double angularVelocity = 0.0;
    double reproductionCooldown = 0.0;
    ColorRgb color{220, 220, 220};
    SpeciesId speciesId = 0;
    GenomeId genomeId = kInvalidGenomeId;
    AgentTypeCode typeCode = AgentTypeCode::LegacyBacteria;
    BodyShapeCode bodyShape = BodyShapeCode::Ellipse;
};

struct FoodSpawn
{
    Vec2 position{};
    double radius = 5.0;
    double energy = 25.0;
    double initialEnergy = 25.0;
    ColorRgb color{220, 30, 30};
    FoodKind kind = FoodKind::Instant;
    // Phase 19: cluster grouping for chunk food. 0 means "no cluster" (instant food).
    std::uint32_t clusterId = 0;
};
} // namespace agentbiosim::simulation
