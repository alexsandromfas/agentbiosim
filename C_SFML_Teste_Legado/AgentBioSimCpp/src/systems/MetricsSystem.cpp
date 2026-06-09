#include "systems/MetricsSystem.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace agentbiosim::systems
{
namespace
{
constexpr double kEps = 1.0e-9;
constexpr double kSmoothAlpha = 0.08;  // group smart-factor smoothing (mirrors Python)

double worldArea(const simulation::World& world)
{
    if (world.shape() == simulation::WorldShape::Circular)
    {
        const double r = std::max(0.0, world.radius());
        return std::max(kEps, 3.14159265358979323846 * r * r);
    }
    return std::max(kEps, world.width() * world.height());
}
} // namespace

void MetricsSystem::configure(const std::size_t maxSamples, const std::size_t sampleInterval) noexcept
{
    maxSamples_ = std::max<std::size_t>(1, maxSamples);
    sampleInterval_ = std::max<std::size_t>(1, sampleInterval);
}

SpeciesMetric& MetricsSystem::speciesSlot(const simulation::SpeciesId id)
{
    for (auto& metric : speciesMetrics_)
    {
        if (metric.id == id) return metric;
    }
    SpeciesMetric slot;
    slot.id = id;
    speciesMetrics_.push_back(std::move(slot));
    return speciesMetrics_.back();
}

void MetricsSystem::record(const MetricsStepInput& input,
                           const simulation::AgentStore& agents,
                           const simulation::FoodStore& foods,
                           const simulation::SpeciesStore& species,
                           const simulation::World& world)
{
    if (!enabled_) return;
    if (sampleInterval_ > 1 && (input.step % sampleInterval_) != 0) return;

    const std::size_t population = agents.size();

    // Global mean energy + per-species accumulation in one pass.
    double energySum = 0.0;
    std::unordered_map<simulation::SpeciesId, std::pair<std::size_t, double>> bySpecies;
    bySpecies.reserve(species.records().size());
    for (std::size_t i = 0; i < population; ++i)
    {
        const double e = agents.energyAt(i);
        energySum += e;
        auto& acc = bySpecies[agents.speciesIdAt(i)];
        acc.first += 1;
        acc.second += e;
    }
    const float meanEnergy =
        population > 0 ? static_cast<float>(energySum / static_cast<double>(population)) : 0.0F;

    // Total food energy -> global resource density.
    double foodEnergy = 0.0;
    for (std::size_t i = 0; i < foods.size(); ++i)
    {
        foodEnergy += foods.energyAt(i);
    }
    const double density = foodEnergy / worldArea(world);

    // Group "smart factor": how well the population converts the available food
    // opportunity into intake. Conceptual parity with sim/intelligence.py
    // (opportunity-conversion, saturating, smoothed) at the group level.
    const double intakeRate = input.foodEnergyConsumed / std::max(kEps, input.dt);
    const double visionArea = 3.14159265358979323846 * input.referenceVisionRadius *
                              input.referenceVisionRadius;
    const double opportunity = density * static_cast<double>(population) * visionArea;
    const double ratio = intakeRate / std::max(1.0, opportunity);
    const double rawSmart = 100.0 * (1.0 - std::exp(-ratio));
    smartFactor_ = smartFactor_ <= kEps ? rawSmart
                                        : smartFactor_ + kSmoothAlpha * (rawSmart - smartFactor_);

    MetricsSample sample;
    sample.step = input.step;
    sample.population = static_cast<float>(population);
    sample.foodCount = static_cast<float>(foods.size());
    sample.meanEnergy = meanEnergy;
    sample.births = static_cast<float>(input.births);
    sample.deaths = static_cast<float>(input.deaths);
    sample.foodEaten = static_cast<float>(input.foodsConsumed);
    sample.predation = static_cast<float>(input.predationEvents);
    sample.smartFactor = static_cast<float>(smartFactor_);
    samples_.push_back(sample);
    while (samples_.size() > maxSamples_)
    {
        samples_.pop_front();
    }

    // Per-species series for every enabled species (0 when none alive this step).
    for (const auto& record : species.records())
    {
        if (!record.enabled) continue;
        SpeciesMetric& metric = speciesSlot(record.id);
        const auto it = bySpecies.find(record.id);
        const std::size_t count = it != bySpecies.end() ? it->second.first : 0;
        const double energy = it != bySpecies.end() ? it->second.second : 0.0;
        metric.population.push_back(static_cast<float>(count));
        metric.meanEnergy.push_back(count > 0 ? static_cast<float>(energy / static_cast<double>(count))
                                              : 0.0F);
        while (metric.population.size() > maxSamples_) metric.population.pop_front();
        while (metric.meanEnergy.size() > maxSamples_) metric.meanEnergy.pop_front();
    }
}

void MetricsSystem::reset()
{
    samples_.clear();
    speciesMetrics_.clear();
    smartFactor_ = 0.0;
}

std::vector<float> MetricsSystem::series(const MetricField field) const
{
    std::vector<float> out;
    out.reserve(samples_.size());
    for (const auto& s : samples_)
    {
        switch (field)
        {
        case MetricField::Population:  out.push_back(s.population); break;
        case MetricField::FoodCount:   out.push_back(s.foodCount); break;
        case MetricField::MeanEnergy:  out.push_back(s.meanEnergy); break;
        case MetricField::Births:      out.push_back(s.births); break;
        case MetricField::Deaths:      out.push_back(s.deaths); break;
        case MetricField::FoodEaten:   out.push_back(s.foodEaten); break;
        case MetricField::Predation:   out.push_back(s.predation); break;
        case MetricField::SmartFactor: out.push_back(s.smartFactor); break;
        }
    }
    return out;
}
} // namespace agentbiosim::systems
