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

void writeChannels(double* output, const std::vector<RetinaChannel>& channels,
                   const double activation, const double r, const double g, const double b)
{
    const std::size_t stride = channels.size();
    for (std::size_t ch = 0; ch < stride; ++ch)
    {
        output[ch] = channelValue(channels[ch], activation, r, g, b);
    }
}

struct EyePose
{
    double eyeX;
    double eyeY;
    double gazeAngle;
    double cosGaze;
    double sinGaze;
};

EyePose computeEyePose(const simulation::Vec2 agentPos, const double agentAngle,
                       const double agentRadius, const EyeSpec spec)
{
    const double posAngle = agentAngle + spec.positionOffset;
    EyePose pose;
    pose.eyeX = agentPos.x + std::cos(posAngle) * agentRadius;
    pose.eyeY = agentPos.y + std::sin(posAngle) * agentRadius;
    pose.gazeAngle = agentAngle + spec.gazeOffset;
    pose.cosGaze = std::cos(pose.gazeAngle);
    pose.sinGaze = std::sin(pose.gazeAngle);
    return pose;
}

void singleVisionForAgent(const std::size_t agentIndex,
                          const simulation::AgentStore& agents,
                          const std::vector<VisibleCandidate>& candidates,
                          const RetinaConfig& retina,
                          const std::vector<RetinaChannel>& channels,
                          double* output,
                          std::size_t& hitCounter,
                          VisionDebugData* debugOut)
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
        const EyePose pose = computeEyePose(agentPos, agentAngle, agentRadius, eyeSpecs[eyeIdx]);

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

            const double dx = candidate.x - pose.eyeX;
            const double dy = candidate.y - pose.eyeY;
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
            const double relAngle = wrapAngle(objAngle - pose.gazeAngle);

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
            bool hit = false;

            if (std::isfinite(bestDist[ray]))
            {
                activation = std::clamp((visionRadius - bestDist[ray]) / visionRadius, 0.0, 1.0);
                r = bestColor[ray][0];
                g = bestColor[ray][1];
                b = bestColor[ray][2];
                hit = true;
                ++hitCounter;
            }

            const std::size_t base = eyeOffset + ray * stride;
            writeChannels(output + base, channels, activation, r, g, b);

            if (debugOut != nullptr)
            {
                VisionRayDebug rd;
                rd.eyeIndex = eyeIdx;
                rd.rayIndex = ray;
                rd.startX = pose.eyeX;
                rd.startY = pose.eyeY;
                rd.maxLength = visionRadius;
                rd.hit = hit;
                rd.hitDistance = hit ? bestDist[ray] : -1.0;
                rd.activation = activation;
                rd.hitColorR = r;
                rd.hitColorG = g;
                rd.hitColorB = b;
                const double rayLocalAngle = retinaCount > 1
                    ? (-halfFovRad) + (static_cast<double>(ray) / static_cast<double>(retinaCount - 1)) * 2.0 * halfFovRad
                    : 0.0;
                const double rayWorldAngle = pose.gazeAngle + rayLocalAngle;
                rd.dirX = std::cos(rayWorldAngle);
                rd.dirY = std::sin(rayWorldAngle);
                const double displayLength = hit ? bestDist[ray] : visionRadius;
                rd.hitX = pose.eyeX + rd.dirX * displayLength;
                rd.hitY = pose.eyeY + rd.dirY * displayLength;
                debugOut->rays.push_back(rd);
            }
        }
    }
}

void fullbodyVisionForAgent(const std::size_t agentIndex,
                            const simulation::AgentStore& agents,
                            const std::vector<VisibleCandidate>& candidates,
                            const RetinaConfig& retina,
                            const std::vector<RetinaChannel>& channels,
                            const std::vector<double>& relCos,
                            const std::vector<double>& relSin,
                            double* output,
                            std::size_t& hitCounter,
                            VisionDebugData* debugOut)
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
    thread_local std::vector<double> rayDirX;
    thread_local std::vector<double> rayDirY;
    bestDist.resize(retinaCount);
    bestColor.resize(retinaCount);
    rayDirX.resize(retinaCount);
    rayDirY.resize(retinaCount);

    for (std::size_t eyeIdx = 0; eyeIdx < eyeCount; ++eyeIdx)
    {
        const EyePose pose = computeEyePose(agentPos, agentAngle, agentRadius, eyeSpecs[eyeIdx]);

        // Rotate cached relative ray directions to world space using one 2D rotation matrix.
        for (std::size_t r = 0; r < retinaCount; ++r)
        {
            rayDirX[r] = pose.cosGaze * relCos[r] - pose.sinGaze * relSin[r];
            rayDirY[r] = pose.sinGaze * relCos[r] + pose.cosGaze * relSin[r];
        }

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

            const double ox = pose.eyeX - candidate.x;
            const double oy = pose.eyeY - candidate.y;
            const double centerDistSq = ox * ox + oy * oy;
            const double rr = candidate.radius * candidate.radius;
            const double minPossibleDist = std::sqrt(centerDistSq) - candidate.radius;
            if (minPossibleDist > visionRadius)
            {
                continue;
            }

            // Angular pre-filter: compute relative angle and half-span to skip rays.
            const double dx = candidate.x - pose.eyeX;
            const double dy = candidate.y - pose.eyeY;
            const double centerDist = std::hypot(dx, dy);
            const double objAngle = std::atan2(dy, dx);
            const double relAngle = wrapAngle(objAngle - pose.gazeAngle);

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

            // Determine which rays could possibly intersect this candidate's angular wedge.
            long rayStart = 0;
            long rayEnd = static_cast<long>(retinaCount) - 1;
            if (retinaCount > 1)
            {
                const double leftAngle = std::max(-halfFovRad, relAngle - halfSpan);
                const double rightAngle = std::min(halfFovRad, relAngle + halfSpan);
                const double scale = static_cast<double>(retinaCount - 1) / (2.0 * halfFovRad);
                const double leftIdx = (leftAngle + halfFovRad) * scale;
                const double rightIdx = (rightAngle + halfFovRad) * scale;
                rayStart = std::max(0L, static_cast<long>(std::floor(leftIdx)));
                rayEnd = std::min(static_cast<long>(retinaCount - 1),
                                  static_cast<long>(std::ceil(rightIdx)));
            }

            // Ray-circle intersection for each candidate ray.
            const double c = centerDistSq - rr;
            for (long r = rayStart; r <= rayEnd; ++r)
            {
                const std::size_t rayIdx = static_cast<std::size_t>(r);
                const double dxr = rayDirX[rayIdx];
                const double dyr = rayDirY[rayIdx];
                const double bVal = dxr * ox + dyr * oy;
                const double disc = bVal * bVal - c;
                if (disc < 0.0)
                {
                    continue;
                }
                const double sqrtDisc = std::sqrt(disc);
                const double t1 = -bVal - sqrtDisc;
                const double t2 = -bVal + sqrtDisc;
                double t = std::numeric_limits<double>::infinity();
                if (t1 >= 0.0)
                {
                    t = t1;
                }
                else if (t2 >= 0.0)
                {
                    t = 0.0;
                }
                if (t > visionRadius)
                {
                    continue;
                }
                if (t < bestDist[rayIdx])
                {
                    bestDist[rayIdx] = t;
                    bestColor[rayIdx] = {candidate.colorR, candidate.colorG, candidate.colorB};
                }
            }
        }

        const std::size_t eyeOffset = eyeIdx * retinaCount * stride;
        for (std::size_t ray = 0; ray < retinaCount; ++ray)
        {
            double activation = 0.0;
            double r = 0.0;
            double g = 0.0;
            double b = 0.0;
            bool hit = false;

            if (std::isfinite(bestDist[ray]))
            {
                activation = std::clamp((visionRadius - bestDist[ray]) / visionRadius, 0.0, 1.0);
                r = bestColor[ray][0];
                g = bestColor[ray][1];
                b = bestColor[ray][2];
                hit = true;
                ++hitCounter;
            }

            const std::size_t base = eyeOffset + ray * stride;
            writeChannels(output + base, channels, activation, r, g, b);

            if (debugOut != nullptr)
            {
                VisionRayDebug rd;
                rd.eyeIndex = eyeIdx;
                rd.rayIndex = ray;
                rd.startX = pose.eyeX;
                rd.startY = pose.eyeY;
                rd.dirX = rayDirX[ray];
                rd.dirY = rayDirY[ray];
                rd.maxLength = visionRadius;
                rd.hit = hit;
                rd.hitDistance = hit ? bestDist[ray] : -1.0;
                rd.activation = activation;
                rd.hitColorR = r;
                rd.hitColorG = g;
                rd.hitColorB = b;
                const double displayLength = hit ? bestDist[ray] : visionRadius;
                rd.hitX = pose.eyeX + rd.dirX * displayLength;
                rd.hitY = pose.eyeY + rd.dirY * displayLength;
                debugOut->rays.push_back(rd);
            }
        }
    }
}
double distanceBinActivation(const double distance, const double visionRadius,
                             const SectorBinsConfig& opts)
{
    if (visionRadius <= 1.0e-12)
    {
        return 0.0;
    }
    const double norm = std::clamp(distance / visionRadius, 0.0, 1.0);
    if (opts.falloff == BinsFalloff::None)
    {
        return 1.0;
    }
    const int subdivisions = std::max(1, opts.subdivisions);
    double representative;
    int band;
    if (subdivisions <= 1)
    {
        representative = norm;
        band = 0;
    }
    else if (opts.distribution == BinsDistribution::NearDetail)
    {
        const double sqrtNorm = std::sqrt(norm);
        band = std::clamp(static_cast<int>(std::floor(sqrtNorm * subdivisions)), 0, subdivisions - 1);
        const double left = (static_cast<double>(band) / subdivisions);
        const double right = (static_cast<double>(band + 1) / subdivisions);
        representative = (left * left + right * right) * 0.5;
    }
    else
    {
        band = std::clamp(static_cast<int>(std::floor(norm * subdivisions)), 0, subdivisions - 1);
        representative = (static_cast<double>(band) + 0.5) / subdivisions;
    }
    if (opts.falloff == BinsFalloff::Step)
    {
        return std::clamp(static_cast<double>(subdivisions - band) / subdivisions, 0.0, 1.0);
    }
    double value = std::clamp(1.0 - representative, 0.0, 1.0);
    if (opts.falloff == BinsFalloff::Quadratic)
    {
        value *= value;
    }
    return value;
}

template <typename Visitor>
void forEachSectorIndex(const double relAngle, const double objRadius, const double centerDist,
                       const double halfFov, const std::size_t retinaCount,
                       const BinsProjection projection, Visitor visit)
{
    if (retinaCount <= 1)
    {
        if (std::abs(relAngle) <= halfFov)
        {
            visit(static_cast<std::size_t>(0));
        }
        return;
    }
    const double scale = static_cast<double>(retinaCount) / (2.0 * halfFov);

    double span = 0.0;
    if ((projection == BinsProjection::CenterEdges || projection == BinsProjection::ApparentSize) &&
        centerDist > 1.0e-9 && objRadius > 0.0)
    {
        const double ratio = std::clamp(objRadius / centerDist, 0.0, 1.0);
        span = std::asin(ratio);
    }

    auto emit = [&](const double angle) {
        if (std::abs(angle) > halfFov)
        {
            return;
        }
        const double rel = (angle + halfFov) * scale;
        const long idx = static_cast<long>(std::floor(rel));
        const std::size_t clamped = static_cast<std::size_t>(
            std::clamp(idx, 0L, static_cast<long>(retinaCount) - 1L));
        visit(clamped);
    };

    if (projection == BinsProjection::CenterEdges)
    {
        std::size_t emitted[3];
        std::size_t emittedCount = 0;
        auto pushUnique = [&](const std::size_t idx) {
            for (std::size_t i = 0; i < emittedCount; ++i)
            {
                if (emitted[i] == idx)
                {
                    return;
                }
            }
            if (emittedCount < 3)
            {
                emitted[emittedCount++] = idx;
                visit(idx);
            }
        };
        auto emitUnique = [&](const double angle) {
            if (std::abs(angle) > halfFov)
            {
                return;
            }
            const double rel = (angle + halfFov) * scale;
            const long idx = static_cast<long>(std::floor(rel));
            const std::size_t clamped = static_cast<std::size_t>(
                std::clamp(idx, 0L, static_cast<long>(retinaCount) - 1L));
            pushUnique(clamped);
        };
        emitUnique(relAngle);
        if (span > 1.0e-9)
        {
            emitUnique(relAngle - span);
            emitUnique(relAngle + span);
        }
        return;
    }

    if (projection == BinsProjection::ApparentSize && span > 1.0e-9)
    {
        double left = std::max(-halfFov, relAngle - span);
        double right = std::min(halfFov, relAngle + span);
        if (left > right)
        {
            return;
        }
        long start = static_cast<long>(std::floor((left + halfFov) * scale));
        long end = static_cast<long>(std::floor((right + halfFov) * scale));
        start = std::clamp(start, 0L, static_cast<long>(retinaCount) - 1L);
        end = std::clamp(end, 0L, static_cast<long>(retinaCount) - 1L);
        if (end < start)
        {
            std::swap(start, end);
        }
        for (long i = start; i <= end; ++i)
        {
            visit(static_cast<std::size_t>(i));
        }
        return;
    }

    emit(relAngle);
}

void sectorBinsVisionForAgent(const std::size_t agentIndex,
                              const simulation::AgentStore& agents,
                              const std::vector<VisibleCandidate>& candidates,
                              const RetinaConfig& retina,
                              const std::vector<RetinaChannel>& channels,
                              double* output,
                              std::size_t& hitCounter,
                              std::size_t& candidatesAfterLimit,
                              VisionDebugData* debugOut)
{
    const SectorBinsConfig& opts = retina.sectorBins;
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

    // Apply candidate limit (per-agent): nearest N by squared distance.
    thread_local std::vector<VisibleCandidate> limited;
    const std::vector<VisibleCandidate>* effectiveCandidates = &candidates;
    if (opts.candidateLimit > 0 &&
        candidates.size() > static_cast<std::size_t>(opts.candidateLimit))
    {
        limited.assign(candidates.begin(), candidates.end());
        const double ax = agentPos.x;
        const double ay = agentPos.y;
        const std::size_t keep = static_cast<std::size_t>(opts.candidateLimit);
        std::partial_sort(limited.begin(), limited.begin() + static_cast<std::ptrdiff_t>(keep),
                          limited.end(),
                          [ax, ay](const VisibleCandidate& a, const VisibleCandidate& b) {
                              const double da = (a.x - ax) * (a.x - ax) + (a.y - ay) * (a.y - ay);
                              const double db = (b.x - ax) * (b.x - ax) + (b.y - ay) * (b.y - ay);
                              return da < db;
                          });
        limited.resize(keep);
        effectiveCandidates = &limited;
    }
    candidatesAfterLimit += effectiveCandidates->size();

    thread_local std::vector<double> bestDist;
    thread_local std::vector<double> bestScore;
    thread_local std::vector<std::array<double, 3>> bestColor;
    thread_local std::vector<double> sumValues;
    thread_local std::vector<double> weightedValues;
    thread_local std::vector<double> weights;
    bestDist.resize(retinaCount);
    bestScore.resize(retinaCount);
    bestColor.resize(retinaCount);
    sumValues.resize(retinaCount * stride);
    weightedValues.resize(retinaCount * stride);
    weights.resize(retinaCount);

    for (std::size_t eyeIdx = 0; eyeIdx < eyeCount; ++eyeIdx)
    {
        const EyePose pose = computeEyePose(agentPos, agentAngle, agentRadius, eyeSpecs[eyeIdx]);

        std::fill(bestDist.begin(), bestDist.begin() + static_cast<std::ptrdiff_t>(retinaCount),
                  std::numeric_limits<double>::infinity());
        std::fill(bestScore.begin(), bestScore.begin() + static_cast<std::ptrdiff_t>(retinaCount),
                  -std::numeric_limits<double>::infinity());
        for (std::size_t i = 0; i < retinaCount; ++i)
        {
            bestColor[i] = {0.0, 0.0, 0.0};
        }
        std::fill(sumValues.begin(), sumValues.begin() + static_cast<std::ptrdiff_t>(retinaCount * stride), 0.0);
        std::fill(weightedValues.begin(), weightedValues.begin() + static_cast<std::ptrdiff_t>(retinaCount * stride), 0.0);
        std::fill(weights.begin(), weights.begin() + static_cast<std::ptrdiff_t>(retinaCount), 0.0);

        for (const auto& candidate : *effectiveCandidates)
        {
            if (candidate.entityType == simulation::SpatialEntityType::Agent &&
                candidate.entityId == agentId)
            {
                continue;
            }

            const double dx = candidate.x - pose.eyeX;
            const double dy = candidate.y - pose.eyeY;
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
            const double relAngle = wrapAngle(objAngle - pose.gazeAngle);

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

            const double activation = distanceBinActivation(effDist, visionRadius, opts);
            const double cR = candidate.colorR;
            const double cG = candidate.colorG;
            const double cB = candidate.colorB;
            const double colorMax = std::max({cR, cG, cB});

            forEachSectorIndex(
                relAngle, candidate.radius, centerDist, halfFovRad, retinaCount, opts.projection,
                [&](const std::size_t rayIdx) {
                    const std::size_t base = rayIdx * stride;
                    if (opts.mode == BinsMode::SumSaturating)
                    {
                        for (std::size_t ch = 0; ch < stride; ++ch)
                        {
                            const double val = channelValue(channels[ch], activation, cR, cG, cB);
                            sumValues[base + ch] = std::min(1.0, sumValues[base + ch] + val);
                        }
                        return;
                    }
                    if (opts.mode == BinsMode::WeightedAverage)
                    {
                        const double weight = std::max(1.0e-9, activation);
                        for (std::size_t ch = 0; ch < stride; ++ch)
                        {
                            const double val = channelValue(channels[ch], activation, cR, cG, cB);
                            weightedValues[base + ch] += val * weight;
                        }
                        weights[rayIdx] += weight;
                        return;
                    }
                    // Nearest or Strongest.
                    const double score = (opts.mode == BinsMode::Strongest)
                        ? activation * colorMax
                        : -effDist;
                    const bool better = (opts.mode == BinsMode::Strongest)
                        ? score > bestScore[rayIdx]
                        : effDist < bestDist[rayIdx];
                    if (better)
                    {
                        bestScore[rayIdx] = score;
                        bestDist[rayIdx] = effDist;
                        bestColor[rayIdx] = {cR, cG, cB};
                    }
                });
        }

        const std::size_t eyeOffset = eyeIdx * retinaCount * stride;
        for (std::size_t ray = 0; ray < retinaCount; ++ray)
        {
            const std::size_t base = eyeOffset + ray * stride;
            double activation = 0.0;
            double r = 0.0;
            double g = 0.0;
            double b = 0.0;
            bool hit = false;

            if (opts.mode == BinsMode::SumSaturating)
            {
                for (std::size_t ch = 0; ch < stride; ++ch)
                {
                    output[base + ch] = sumValues[ray * stride + ch];
                    if (output[base + ch] > 0.0)
                    {
                        hit = true;
                    }
                }
                if (hit)
                {
                    ++hitCounter;
                }
            }
            else if (opts.mode == BinsMode::WeightedAverage)
            {
                if (weights[ray] > 0.0)
                {
                    hit = true;
                    ++hitCounter;
                    const double w = weights[ray];
                    for (std::size_t ch = 0; ch < stride; ++ch)
                    {
                        output[base + ch] = weightedValues[ray * stride + ch] / w;
                    }
                }
                else
                {
                    for (std::size_t ch = 0; ch < stride; ++ch)
                    {
                        output[base + ch] = 0.0;
                    }
                }
            }
            else
            {
                if (std::isfinite(bestDist[ray]))
                {
                    activation = distanceBinActivation(bestDist[ray], visionRadius, opts);
                    r = bestColor[ray][0];
                    g = bestColor[ray][1];
                    b = bestColor[ray][2];
                    hit = true;
                    ++hitCounter;
                }
                writeChannels(output + base, channels, activation, r, g, b);
            }

            if (debugOut != nullptr)
            {
                VisionRayDebug rd;
                rd.eyeIndex = eyeIdx;
                rd.rayIndex = ray;
                rd.startX = pose.eyeX;
                rd.startY = pose.eyeY;
                rd.maxLength = visionRadius;
                rd.hit = hit;
                rd.hitDistance = (opts.mode == BinsMode::Nearest || opts.mode == BinsMode::Strongest)
                    ? (std::isfinite(bestDist[ray]) ? bestDist[ray] : -1.0)
                    : -1.0;
                if (opts.mode == BinsMode::SumSaturating)
                {
                    double maxCh = 0.0;
                    for (std::size_t ch = 0; ch < stride; ++ch)
                    {
                        maxCh = std::max(maxCh, output[base + ch]);
                    }
                    rd.activation = maxCh;
                }
                else if (opts.mode == BinsMode::WeightedAverage)
                {
                    rd.activation = weights[ray] > 0.0 ? 1.0 : 0.0;
                }
                else
                {
                    rd.activation = activation;
                }
                rd.hitColorR = r;
                rd.hitColorG = g;
                rd.hitColorB = b;
                const double rayLocalAngle = retinaCount > 1
                    ? (-halfFovRad) +
                          (static_cast<double>(ray) / static_cast<double>(retinaCount - 1)) *
                              2.0 * halfFovRad
                    : 0.0;
                const double rayWorldAngle = pose.gazeAngle + rayLocalAngle;
                rd.dirX = std::cos(rayWorldAngle);
                rd.dirY = std::sin(rayWorldAngle);
                const double displayLength = hit
                    ? (rd.hitDistance >= 0.0 ? rd.hitDistance
                                              : (1.0 - rd.activation) * visionRadius)
                    : visionRadius;
                rd.hitX = pose.eyeX + rd.dirX * displayLength;
                rd.hitY = pose.eyeY + rd.dirY * displayLength;
                debugOut->rays.push_back(rd);
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

    // Sector/bins global parameters.
    result.sectorBins.mode = normalizeBinsMode(
        config::parameterString(registry, "retina_bins_mode", "nearest"));
    result.sectorBins.subdivisions = std::clamp(
        config::parameterInt(registry, "retina_bins_distance_subdivisions", 5), 1, 99);
    result.sectorBins.distribution = normalizeBinsDistribution(
        config::parameterString(registry, "retina_bins_distance_distribution", "near_detail"));
    result.sectorBins.falloff = normalizeBinsFalloff(
        config::parameterString(registry, "retina_bins_distance_falloff", "linear"));
    result.sectorBins.projection = normalizeBinsProjection(
        config::parameterString(registry, "retina_bins_projection", "center"));
    result.sectorBins.candidateLimit = std::max(0,
        config::parameterInt(registry, "retina_bins_candidate_limit", 128));
    result.sectorBins.obstaclesBlockVision = config::parameterBool(
        registry, "retina_bins_obstacles_block_vision", false);

    // High-scale auto sector.
    result.highScaleAutoSector = config::parameterBool(
        registry, "retina_high_scale_auto_sector", false);
    result.highScaleSectorMinAgents = std::max(1,
        config::parameterInt(registry, "retina_high_scale_sector_min_agents", 800));
    result.highScaleGlobalSector = config::parameterBool(
        registry, "retina_high_scale_global_sector", false);

    return result;
}

PerceptionConfig PerceptionSystem::fromRegistry(const config::ParameterRegistry& registry,
                                                const std::string& speciesPrefix)
{
    PerceptionConfig config;
    config.retina = retinaConfigFromRegistry(registry, speciesPrefix);
    return config;
}

void PerceptionSystem::ensureRayCache(const std::size_t retinaCount, const double halfFovRad)
{
    if (rayCache_.retinaCount == retinaCount && rayCache_.halfFovRad == halfFovRad)
    {
        return;
    }
    rayCache_.retinaCount = retinaCount;
    rayCache_.halfFovRad = halfFovRad;
    rayCache_.relCos.resize(retinaCount);
    rayCache_.relSin.resize(retinaCount);
    if (retinaCount == 1)
    {
        rayCache_.relCos[0] = 1.0;
        rayCache_.relSin[0] = 0.0;
        return;
    }
    for (std::size_t r = 0; r < retinaCount; ++r)
    {
        const double rel = (-halfFovRad) +
            (static_cast<double>(r) / static_cast<double>(retinaCount - 1)) * 2.0 * halfFovRad;
        rayCache_.relCos[r] = std::cos(rel);
        rayCache_.relSin[r] = std::sin(rel);
    }
}

PerceptionResult PerceptionSystem::computeInputs(const simulation::AgentStore& agents,
                                                  const simulation::FoodStore& foods,
                                                  simulation::SpatialHash* spatial,
                                                  const simulation::World& world,
                                                  const PerceptionConfig& config,
                                                  const PerceptionDebugRequest& debugRequest,
                                                  const simulation::ObstacleStore* obstacles)
{
    PerceptionResult result;
    const auto& retina = config.retina;
    result.inputSize = retina.inputSize();
    result.agentCount = agents.size();
    result.active = true;

    const VisionMode requestedMode = normalizeVisionMode(retina.visionMode);
    VisionMode activeMode = requestedMode;
    bool fallback = false;
    bool autoSectorActive = false;
    std::string fallbackReason;
    if (!isVisionModeImplemented(requestedMode))
    {
        activeMode = VisionMode::Single;
        fallback = true;
        fallbackReason = std::string(visionModeName(requestedMode)) +
                         " not implemented yet; falling back to single";
    }
    // High-scale auto sector: override requested mode when threshold met.
    if (retina.highScaleAutoSector &&
        agents.size() >= static_cast<std::size_t>(retina.highScaleSectorMinAgents) &&
        activeMode != VisionMode::Sector)
    {
        activeMode = VisionMode::Sector;
        autoSectorActive = true;
    }

    if (debugRequest.out != nullptr)
    {
        debugRequest.out->clear();
    }

    if (result.inputSize == 0 || result.agentCount == 0)
    {
        lastStats_ = {};
        lastStats_.visionMode = visionModeName(activeMode);
        lastStats_.visionModeEnum = activeMode;
        lastStats_.requestedModeEnum = requestedMode;
        lastStats_.fallbackMode = fallback;
        lastStats_.fallbackReason = fallbackReason;
        lastStats_.autoSectorActive = autoSectorActive;
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

    const double halfFovRad = retina.fovDegrees * 0.5 * kDegToRad;
    if (activeMode == VisionMode::Fullbody)
    {
        ensureRayCache(retina.retinaCount, halfFovRad);
    }

    std::size_t totalCandidates = 0;
    std::size_t totalCandidatesAfterLimit = 0;
    std::size_t totalHits = 0;

    const std::uint64_t debugTargetId = debugRequest.out != nullptr ? debugRequest.agentId : 0;

    // Phase 20: occlusion is active when obstacles are present and either
    // `seeThroughWalls=false` (global gate) or, in sector mode, the dedicated
    // `obstaclesBlockVision` flag is set. The candidate buffer is filtered
    // before being handed to the vision strategy.
    const bool obstaclesActive = obstacles != nullptr && !obstacles->empty();
    const bool occlusionEnabledForRays = obstaclesActive &&
        (!retina.seeThroughWalls ||
         (activeMode == VisionMode::Sector && retina.sectorBins.obstaclesBlockVision));
    std::size_t obstacleCandidateCount = 0;
    std::size_t occlusionChecksCount = 0;
    std::size_t occludedCandidatesCount = 0;

    for (std::size_t i = 0; i < agents.size(); ++i)
    {
        if (!agents.aliveAt(i))
        {
            continue;
        }

        const simulation::Vec2 pos = agents.positionAt(i);
        const double agentRadius = agents.radiusAt(i);
        const double searchRadius = retina.visionRadius + agentRadius + maxSeenRadius;
        const std::uint64_t agentId = agents.idAt(i).value;

        queryVisibleCandidates(
            pos.x, pos.y, searchRadius,
            retina.seeFood, retina.seeAgents, retina.seePredators, retina.seeAll,
            agentId,
            spatial, agents, foods,
            candidateBuffer_);

        // Phase 20: append obstacles as candidates when the retina sees them.
        // Even when seeObstacles=false, obstacles still occlude vision (filtered
        // below). seeAll bypasses retina filters but also adds obstacles.
        if (obstaclesActive && (retina.seeObstacles || retina.seeAll))
        {
            const std::size_t beforeObstacles = candidateBuffer_.size();
            appendObstacleCandidates(pos.x, pos.y, searchRadius, *obstacles, candidateBuffer_);
            obstacleCandidateCount += candidateBuffer_.size() - beforeObstacles;
        }

        // Phase 20: pre-filter occluded candidates so the vision strategies see
        // a clean buffer. This is cheaper than testing inside each strategy and
        // keeps the strategies unchanged. Obstacle candidates are kept (an
        // obstacle never occludes itself).
        if (occlusionEnabledForRays)
        {
            const std::size_t before = candidateBuffer_.size();
            std::size_t writeIdx = 0;
            for (std::size_t r = 0; r < candidateBuffer_.size(); ++r)
            {
                const auto& cand = candidateBuffer_[r];
                if (cand.entityType == simulation::SpatialEntityType::Obstacle)
                {
                    candidateBuffer_[writeIdx++] = cand;
                    continue;
                }
                ++occlusionChecksCount;
                if (isOccludedByObstacles(pos.x, pos.y, cand.x, cand.y, *obstacles))
                {
                    ++occludedCandidatesCount;
                    continue;
                }
                candidateBuffer_[writeIdx++] = cand;
            }
            candidateBuffer_.resize(writeIdx);
            static_cast<void>(before);
        }

        totalCandidates += candidateBuffer_.size();

        const bool fillDebug = (debugTargetId != 0 && agentId == debugTargetId);
        VisionDebugData* debugSink = fillDebug ? debugRequest.out : nullptr;
        if (fillDebug)
        {
            debugSink->agentId = agentId;
            debugSink->active = true;
            debugSink->mode = activeMode;
            debugSink->retinaCount = retina.retinaCount;
            debugSink->eyeCount = retina.eyeCount;
            debugSink->visionRadius = retina.visionRadius;
            debugSink->fovDegrees = retina.fovDegrees;
            debugSink->rays.reserve(retina.retinaCount * std::max<std::size_t>(1U, retina.eyeCount));
        }

        double* agentOutput = result.flatInputs.data() + i * result.inputSize;
        if (activeMode == VisionMode::Fullbody)
        {
            fullbodyVisionForAgent(i, agents, candidateBuffer_, retina, channels,
                                    rayCache_.relCos, rayCache_.relSin,
                                    agentOutput, totalHits, debugSink);
        }
        else if (activeMode == VisionMode::Sector)
        {
            sectorBinsVisionForAgent(i, agents, candidateBuffer_, retina, channels,
                                      agentOutput, totalHits, totalCandidatesAfterLimit, debugSink);
        }
        else
        {
            singleVisionForAgent(i, agents, candidateBuffer_, retina, channels,
                                  agentOutput, totalHits, debugSink);
        }
    }

    lastStats_ = {};
    lastStats_.agentsProcessed = agents.size();
    lastStats_.totalCandidatesQueried = totalCandidates;
    lastStats_.totalCandidatesAfterLimit = activeMode == VisionMode::Sector
        ? totalCandidatesAfterLimit : totalCandidates;
    lastStats_.averageCandidatesPerAgent = agents.size() > 0
        ? static_cast<double>(totalCandidates) / static_cast<double>(agents.size())
        : 0.0;
    lastStats_.averageCandidatesAfterLimit = agents.size() > 0
        ? static_cast<double>(lastStats_.totalCandidatesAfterLimit) / static_cast<double>(agents.size())
        : 0.0;
    lastStats_.visionMode = visionModeName(activeMode);
    lastStats_.visionModeEnum = activeMode;
    lastStats_.requestedModeEnum = requestedMode;
    lastStats_.inputSize = result.inputSize;
    lastStats_.channelCount = channels.size();
    lastStats_.retinaCount = retina.retinaCount;
    lastStats_.eyeCount = retina.eyeCount;
    lastStats_.totalRayHits = totalHits;
    lastStats_.usedSpatialHash = spatial != nullptr && !spatial->empty();
    lastStats_.fallbackMode = fallback;
    lastStats_.fallbackReason = fallbackReason;
    lastStats_.autoSectorActive = autoSectorActive;
    lastStats_.obstacleCandidates = obstacleCandidateCount;
    lastStats_.occlusionChecks = occlusionChecksCount;
    lastStats_.occludedCandidates = occludedCandidatesCount;

    (void)world;
    return result;
}

const PerceptionStats& PerceptionSystem::lastStats() const noexcept
{
    return lastStats_;
}
} // namespace agentbiosim::perception
