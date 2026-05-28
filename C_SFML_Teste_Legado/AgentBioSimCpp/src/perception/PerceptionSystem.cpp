#include "perception/PerceptionSystem.hpp"

#include "config/ParameterHelpers.hpp"
#include "perception/SceneQuery.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace agentbiosim::perception
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = kPi * 2.0;
constexpr double kDegToRad = kPi / 180.0;

double wrapAngle(double angle)
{
    angle = std::fmod(angle + kPi, kTwoPi);
    if (angle < 0.0)
    {
        angle += kTwoPi;
    }
    return angle - kPi;
}

struct EyeSpec
{
    double positionOffset = 0.0;
    double gazeOffset = 0.0;
};

std::array<EyeSpec, 2> computeEyeSpecs(const RetinaConfig& config)
{
    if (config.eyeCount <= 1)
    {
        return {EyeSpec{0.0, 0.0}, EyeSpec{0.0, 0.0}};
    }
    const double gazeHalf = config.eyeAngleDegrees * 0.5 * kDegToRad;
    const double posHalf = config.eyeSeparationDegrees * 0.5 * kDegToRad;
    return {EyeSpec{-posHalf, -gazeHalf}, EyeSpec{posHalf, gazeHalf}};
}

double channelValue(const RetinaChannel channel, const double activation,
                    const double r, const double g, const double b)
{
    switch (channel)
    {
    case RetinaChannel::R:
        return r;
    case RetinaChannel::G:
        return g;
    case RetinaChannel::B:
        return b;
    case RetinaChannel::RD:
        return activation * r;
    case RetinaChannel::GD:
        return activation * g;
    case RetinaChannel::BD:
        return activation * b;
    case RetinaChannel::D:
        return activation;
    }
    return activation;
}

void singleVisionForAgent(const std::size_t agentIndex,
                          const simulation::AgentStore& agents,
                          const std::vector<VisibleCandidate>& candidates,
                          const RetinaConfig& retina,
                          const std::vector<RetinaChannel>& channels,
                          double* output)
{
    const std::size_t stride = channels.size();
    const std::size_t retinaCount = retina.retinaCount;
    const double visionRadius = retina.visionRadius;
    const double halfFovRad = retina.fovDegrees * 0.5 * kDegToRad;

    const double agentAngle = agents.angleAt(agentIndex);
    const simulation::Vec2 agentPos = agents.positionAt(agentIndex);
    const double agentRadius = agents.radiusAt(agentIndex);
    const std::uint64_t agentId = agents.idAt(agentIndex).value;

    const auto eyeSpecs = computeEyeSpecs(retina);
    const std::size_t eyeCount = std::max<std::size_t>(1U, retina.eyeCount);

    thread_local std::vector<double> bestDist;
    thread_local std::vector<std::array<double, 3>> bestColor;
    bestDist.resize(retinaCount);
    bestColor.resize(retinaCount);

    for (std::size_t eyeIdx = 0; eyeIdx < eyeCount; ++eyeIdx)
    {
        const EyeSpec& spec = eyeSpecs[eyeIdx];
        const double posAngle = agentAngle + spec.positionOffset;
        const double eyeX = agentPos.x + std::cos(posAngle) * agentRadius;
        const double eyeY = agentPos.y + std::sin(posAngle) * agentRadius;
        const double gazeAngle = agentAngle + spec.gazeOffset;

        std::fill(bestDist.begin(), bestDist.begin() + static_cast<std::ptrdiff_t>(retinaCount),
                  std::numeric_limits<double>::infinity());
        for (std::size_t i = 0; i < retinaCount; ++i)
        {
            bestColor[i] = {0.0, 0.0, 0.0};
        }

        for (const auto& candidate : candidates)
        {
            if (candidate.entityType == simulation::SpatialEntityType::Agent &&
                candidate.entityId == agentId)
            {
                continue;
            }

            const double dx = candidate.x - eyeX;
            const double dy = candidate.y - eyeY;
            const double centerDist = std::hypot(dx, dy);

            double effDist = centerDist - candidate.radius;
            if (effDist > visionRadius)
            {
                continue;
            }
            if (effDist < 0.0)
            {
                effDist = 0.0;
            }

            const double objAngle = std::atan2(dy, dx);
            const double relAngle = wrapAngle(objAngle - gazeAngle);

            double halfSpan = 0.0;
            if (centerDist > candidate.radius && candidate.radius > 0.0)
            {
                halfSpan = std::asin(std::min(1.0, candidate.radius / centerDist));
            }
            else if (centerDist <= candidate.radius)
            {
                halfSpan = kPi;
            }

            if (std::abs(relAngle) > halfFovRad + halfSpan)
            {
                continue;
            }

            std::size_t rayIdx = 0;
            if (retinaCount > 1)
            {
                const double rel = ((relAngle + halfFovRad) / (2.0 * halfFovRad)) *
                                   static_cast<double>(retinaCount - 1);
                const long rounded = std::lround(rel);
                rayIdx = static_cast<std::size_t>(
                    std::clamp(rounded, 0L, static_cast<long>(retinaCount - 1)));
            }

            if (effDist < bestDist[rayIdx])
            {
                bestDist[rayIdx] = effDist;
                bestColor[rayIdx] = {candidate.colorR, candidate.colorG, candidate.colorB};
            }
        }

        const std::size_t eyeOffset = eyeIdx * retinaCount * stride;
        for (std::size_t ray = 0; ray < retinaCount; ++ray)
        {
            double activation = 0.0;
            double r = 0.0;
            double g = 0.0;
            double b = 0.0;

            if (std::isfinite(bestDist[ray]))
            {
                activation = std::clamp((visionRadius - bestDist[ray]) / visionRadius, 0.0, 1.0);
                r = bestColor[ray][0];
                g = bestColor[ray][1];
                b = bestColor[ray][2];
            }

            const std::size_t base = eyeOffset + ray * stride;
            for (std::size_t ch = 0; ch < stride; ++ch)
            {
                output[base + ch] = channelValue(channels[ch], activation, r, g, b);
            }
        }
    }
}
} // namespace

RetinaConfig retinaConfigFromRegistry(const config::ParameterRegistry& registry,
                                     const std::string& prefix)
{
    RetinaConfig result;
    result.visionMode = normalizeRetinaText(
        config::parameterString(registry, "retina_vision_mode", "single"));
    result.visionRadius = std::max(1.0e-9,
        config::parameterDouble(registry, prefix + "_vision_radius", 120.0));
    result.retinaCount = static_cast<std::size_t>(std::max(1,
        config::parameterInt(registry, prefix + "_retina_count", 18)));
    result.fovDegrees = std::clamp(
        config::parameterDouble(registry, prefix + "_retina_fov_degrees", 180.0), 1.0, 360.0);
    const int eyeRaw = config::parameterInt(registry, prefix + "_eye_count", 1);
    result.eyeCount = eyeRaw >= 2 ? 2U : 1U;
    result.eyeAngleDegrees = config::parameterDouble(registry, prefix + "_eye_angle_degrees", 60.0);
    result.eyeSeparationDegrees = config::parameterDouble(registry, prefix + "_eye_separation_degrees", 45.0);

    result.seeFood = config::parameterBool(registry, prefix + "_retina_see_food", true);
    result.seeAgents = config::parameterBool(registry, prefix + "_retina_see_bacteria", false);
    result.seePredators = config::parameterBool(registry, prefix + "_retina_see_predators", false);
    result.seeObstacles = config::parameterBool(registry, prefix + "_retina_see_obstacles", false);
    result.seeAll = config::parameterBool(registry, prefix + "_retina_see_all", false);
    result.seeThroughWalls = config::parameterBool(registry, prefix + "_retina_see_through_walls", true);

    result.inputMode = normalizeInputMode(
        config::parameterString(registry, prefix + "_retina_input_mode", "distance_only"));
    result.channelR = config::parameterBool(registry, prefix + "_retina_channel_r", false);
    result.channelG = config::parameterBool(registry, prefix + "_retina_channel_g", false);
    result.channelB = config::parameterBool(registry, prefix + "_retina_channel_b", false);
    result.channelD = config::parameterBool(registry, prefix + "_retina_channel_d", true);

    return result;
}

PerceptionConfig PerceptionSystem::fromRegistry(const config::ParameterRegistry& registry,
                                                const std::string& speciesPrefix)
{
    PerceptionConfig config;
    config.retina = retinaConfigFromRegistry(registry, speciesPrefix);
    return config;
}

PerceptionResult PerceptionSystem::computeInputs(const simulation::AgentStore& agents,
                                                  const simulation::FoodStore& foods,
                                                  simulation::SpatialHash* spatial,
                                                  const simulation::World& world,
                                                  const PerceptionConfig& config)
{
    PerceptionResult result;
    const auto& retina = config.retina;
    result.inputSize = retina.inputSize();
    result.agentCount = agents.size();
    result.active = true;

    if (result.inputSize == 0 || result.agentCount == 0)
    {
        lastStats_ = {};
        lastStats_.visionMode = retina.visionMode;
        return result;
    }

    result.flatInputs.assign(result.agentCount * result.inputSize, 0.0);
    const auto channels = retina.activeChannels();

    const double maxSeenRadius = std::max({
        retina.seeFood || retina.seeAll ? 5.0 : 0.0,
        retina.seeAgents || retina.seeAll ? 12.0 : 0.0,
        retina.seePredators || retina.seeAll ? 18.0 : 0.0,
        1.0
    });

    std::size_t totalCandidates = 0;

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents.aliveAt(i))
        {
            continue;
        }

        const simulation::Vec2 pos = agents.positionAt(i);
        const double agentRadius = agents.radiusAt(i);
        const double searchRadius = retina.visionRadius + agentRadius + maxSeenRadius;

        queryVisibleCandidates(
            pos.x, pos.y, searchRadius,
            retina.seeFood, retina.seeAgents, retina.seePredators, retina.seeAll,
            agents.idAt(i).value,
            spatial, agents, foods,
            candidateBuffer_);

        totalCandidates += candidateBuffer_.size();

        double* agentOutput = result.flatInputs.data() + i * result.inputSize;
        singleVisionForAgent(i, agents, candidateBuffer_, retina, channels, agentOutput);
    }

    lastStats_ = {};
    lastStats_.agentsProcessed = agents.size();
    lastStats_.totalCandidatesQueried = totalCandidates;
    lastStats_.averageCandidatesPerAgent = agents.size() > 0
        ? static_cast<double>(totalCandidates) / static_cast<double>(agents.size())
        : 0.0;
    lastStats_.visionMode = retina.visionMode;
    lastStats_.inputSize = result.inputSize;
    lastStats_.channelCount = channels.size();
    lastStats_.retinaCount = retina.retinaCount;
    lastStats_.eyeCount = retina.eyeCount;
    lastStats_.usedSpatialHash = spatial != nullptr && !spatial->empty();

    (void)world;
    return result;
}

const PerceptionStats& PerceptionSystem::lastStats() const noexcept
{
    return lastStats_;
}
} // namespace agentbiosim::perception
