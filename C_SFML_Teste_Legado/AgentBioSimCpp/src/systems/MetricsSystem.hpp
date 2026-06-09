#pragma once

#include "simulation/AgentStore.hpp"
#include "simulation/FoodStore.hpp"
#include "simulation/SpeciesStore.hpp"
#include "simulation/World.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace agentbiosim::systems
{
// Phase 27: lightweight time-series metrics. The runner feeds one
// MetricsStepInput per step; the system samples (every `sampleInterval` steps)
// global and per-species series into bounded ring buffers that the ImGui charts
// read. Headless — no ImGui dependency. Off by default; record() returns
// immediately when disabled.
struct MetricsStepInput
{
    std::uint64_t step = 0;
    double dt = 1.0 / 30.0;
    std::size_t births = 0;
    std::size_t deaths = 0;
    std::size_t foodsConsumed = 0;
    std::size_t predationEvents = 0;
    double foodEnergyConsumed = 0.0;       // energy agents gained from food this step
    double predationEnergyConsumed = 0.0;  // energy gained from predation this step
    double referenceVisionRadius = 120.0;  // used to size the "opportunity" area
};

struct MetricsSample
{
    std::uint64_t step = 0;
    float population = 0.0F;
    float foodCount = 0.0F;
    float meanEnergy = 0.0F;
    float births = 0.0F;
    float deaths = 0.0F;
    float foodEaten = 0.0F;
    float predation = 0.0F;
    float smartFactor = 0.0F;  // group/global intelligence (conceptual parity)
};

// One series field selector for series().
enum class MetricField : int
{
    Population = 0,
    FoodCount,
    MeanEnergy,
    Births,
    Deaths,
    FoodEaten,
    Predation,
    SmartFactor
};

struct SpeciesMetric
{
    simulation::SpeciesId id = 0;
    std::deque<float> population;
    std::deque<float> meanEnergy;
};

class MetricsSystem
{
public:
    void configure(std::size_t maxSamples, std::size_t sampleInterval) noexcept;
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }

    // Called every step by the runner; only samples every `sampleInterval` steps.
    void record(const MetricsStepInput& input,
                const simulation::AgentStore& agents,
                const simulation::FoodStore& foods,
                const simulation::SpeciesStore& species,
                const simulation::World& world);

    void reset();

    [[nodiscard]] const std::deque<MetricsSample>& samples() const noexcept { return samples_; }
    [[nodiscard]] std::size_t sampleCount() const noexcept { return samples_.size(); }
    [[nodiscard]] const MetricsSample* latest() const noexcept
    {
        return samples_.empty() ? nullptr : &samples_.back();
    }
    // Copy one global series into a contiguous vector for ImGui::PlotLines.
    [[nodiscard]] std::vector<float> series(MetricField field) const;
    [[nodiscard]] const std::vector<SpeciesMetric>& speciesMetrics() const noexcept
    {
        return speciesMetrics_;
    }
    [[nodiscard]] double smartFactor() const noexcept { return smartFactor_; }

private:
    [[nodiscard]] SpeciesMetric& speciesSlot(simulation::SpeciesId id);

    bool enabled_ = false;
    std::size_t maxSamples_ = 600;
    std::size_t sampleInterval_ = 1;
    std::deque<MetricsSample> samples_;
    std::vector<SpeciesMetric> speciesMetrics_;
    double smartFactor_ = 0.0;  // smoothed group intelligence
};
} // namespace agentbiosim::systems
