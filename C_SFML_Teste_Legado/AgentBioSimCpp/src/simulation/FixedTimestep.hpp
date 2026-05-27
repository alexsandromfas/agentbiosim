#pragma once

namespace agentbiosim::simulation
{
struct FixedTimestepConfig
{
    double physicsStepsPerSecond = 30.0;
    unsigned int maxStepsPerFrame = 8;
    double maxBacklogSeconds = 0.25;
    double timeScale = 1.0;
    bool paused = false;
};

class FixedTimestep
{
public:
    FixedTimestep() = default;
    explicit FixedTimestep(FixedTimestepConfig config);

    void configure(FixedTimestepConfig config);
    void setPaused(bool paused) noexcept;
    void setTimeScale(double timeScale) noexcept;
    void reset() noexcept;

    [[nodiscard]] const FixedTimestepConfig& config() const noexcept;
    [[nodiscard]] double fixedDeltaSeconds() const noexcept;
    [[nodiscard]] double accumulatorSeconds() const noexcept;
    [[nodiscard]] double interpolationAlpha() const noexcept;
    [[nodiscard]] bool paused() const noexcept;
    [[nodiscard]] double timeScale() const noexcept;
    [[nodiscard]] double lastDroppedSeconds() const noexcept;

    unsigned int beginFrame(double realDeltaSeconds) noexcept;

private:
    FixedTimestepConfig config_{};
    double fixedDeltaSeconds_ = 1.0 / 30.0;
    double accumulatorSeconds_ = 0.0;
    double lastDroppedSeconds_ = 0.0;
};
} // namespace agentbiosim::simulation
