#include "simulation/FixedTimestep.hpp"

#include <algorithm>
#include <cmath>

namespace agentbiosim::simulation
{
namespace
{
double sanitizeStepsPerSecond(const double value)
{
    return std::max(1.0, value);
}
} // namespace

FixedTimestep::FixedTimestep(FixedTimestepConfig config)
{
    configure(config);
}

void FixedTimestep::configure(FixedTimestepConfig config)
{
    config.physicsStepsPerSecond = sanitizeStepsPerSecond(config.physicsStepsPerSecond);
    config.maxStepsPerFrame = std::max(1U, config.maxStepsPerFrame);
    config.timeScale = std::max(0.0, config.timeScale);

    config_ = config;
    fixedDeltaSeconds_ = 1.0 / config_.physicsStepsPerSecond;
    config_.maxBacklogSeconds = std::max(fixedDeltaSeconds_, config.maxBacklogSeconds);
    accumulatorSeconds_ = std::min(accumulatorSeconds_, config_.maxBacklogSeconds);
}

void FixedTimestep::setPaused(const bool paused) noexcept
{
    config_.paused = paused;
}

void FixedTimestep::setTimeScale(const double timeScale) noexcept
{
    config_.timeScale = std::max(0.0, timeScale);
}

void FixedTimestep::reset() noexcept
{
    accumulatorSeconds_ = 0.0;
    lastDroppedSeconds_ = 0.0;
}

const FixedTimestepConfig& FixedTimestep::config() const noexcept
{
    return config_;
}

double FixedTimestep::fixedDeltaSeconds() const noexcept
{
    return fixedDeltaSeconds_;
}

double FixedTimestep::accumulatorSeconds() const noexcept
{
    return accumulatorSeconds_;
}

double FixedTimestep::interpolationAlpha() const noexcept
{
    if (fixedDeltaSeconds_ <= 0.0)
    {
        return 0.0;
    }
    return std::max(0.0, std::min(accumulatorSeconds_ / fixedDeltaSeconds_, 1.0));
}

bool FixedTimestep::paused() const noexcept
{
    return config_.paused;
}

double FixedTimestep::timeScale() const noexcept
{
    return config_.timeScale;
}

double FixedTimestep::lastDroppedSeconds() const noexcept
{
    return lastDroppedSeconds_;
}

unsigned int FixedTimestep::beginFrame(const double realDeltaSeconds) noexcept
{
    lastDroppedSeconds_ = 0.0;
    if (config_.paused || realDeltaSeconds <= 0.0 || config_.timeScale <= 0.0)
    {
        return 0;
    }

    accumulatorSeconds_ += realDeltaSeconds * config_.timeScale;
    if (accumulatorSeconds_ > config_.maxBacklogSeconds)
    {
        lastDroppedSeconds_ += accumulatorSeconds_ - config_.maxBacklogSeconds;
        accumulatorSeconds_ = config_.maxBacklogSeconds;
    }

    const auto availableSteps = static_cast<unsigned int>(
        std::floor((accumulatorSeconds_ + fixedDeltaSeconds_ * 1e-9) / fixedDeltaSeconds_));
    const unsigned int stepsThisFrame = std::min(availableSteps, config_.maxStepsPerFrame);
    const double simulatedSeconds = static_cast<double>(stepsThisFrame) * fixedDeltaSeconds_;

    if (stepsThisFrame < availableSteps)
    {
        lastDroppedSeconds_ += std::max(0.0, accumulatorSeconds_ - simulatedSeconds);
        accumulatorSeconds_ = 0.0;
    }
    else
    {
        accumulatorSeconds_ = std::max(0.0, accumulatorSeconds_ - simulatedSeconds);
        if (accumulatorSeconds_ < fixedDeltaSeconds_ * 1e-6)
        {
            accumulatorSeconds_ = 0.0;
        }
    }

    return stepsThisFrame;
}
} // namespace agentbiosim::simulation
