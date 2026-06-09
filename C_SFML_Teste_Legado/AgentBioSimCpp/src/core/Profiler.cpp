#include "core/Profiler.hpp"

#include <chrono>
#include <cstdio>

namespace agentbiosim::core
{
namespace
{
[[nodiscard]] std::uint64_t nowNanoseconds() noexcept
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
} // namespace

const char* profileSectionName(const ProfileSection section) noexcept
{
    switch (section)
    {
    case ProfileSection::Perception:   return "perception";
    case ProfileSection::Neural:       return "neural";
    case ProfileSection::Movement:     return "movement";
    case ProfileSection::Collision:    return "collision";
    case ProfileSection::Energy:       return "energy";
    case ProfileSection::Interaction:  return "interaction";
    case ProfileSection::Food:         return "food";
    case ProfileSection::Reproduction: return "reproduction";
    case ProfileSection::Death:        return "death";
    case ProfileSection::SpatialHash:  return "spatialhash";
    case ProfileSection::Render:       return "render";
    case ProfileSection::Ui:           return "ui";
    case ProfileSection::SimStep:      return "simstep";
    case ProfileSection::Count:        break;
    }
    return "?";
}

void Profiler::addSample(const ProfileSection section, const std::uint64_t nanoseconds) noexcept
{
    Stat& stat = stats_[static_cast<std::size_t>(section)];
    stat.accumNs += nanoseconds;
    ++stat.calls;
    stat.lastNs = nanoseconds;
    if (nanoseconds > stat.maxNs)
    {
        stat.maxNs = nanoseconds;
    }
}

double Profiler::averageUs(const ProfileSection section) const noexcept
{
    const Stat& stat = stats_[static_cast<std::size_t>(section)];
    if (stat.calls == 0)
    {
        return 0.0;
    }
    return static_cast<double>(stat.accumNs) / static_cast<double>(stat.calls) / 1000.0;
}

double Profiler::lastUs(const ProfileSection section) const noexcept
{
    return static_cast<double>(stats_[static_cast<std::size_t>(section)].lastNs) / 1000.0;
}

std::uint64_t Profiler::simSectionsAccumNs() const noexcept
{
    std::uint64_t sum = 0;
    for (int i = static_cast<int>(ProfileSection::Perception);
         i <= static_cast<int>(ProfileSection::SpatialHash); ++i)
    {
        sum += stats_[static_cast<std::size_t>(i)].accumNs;
    }
    return sum;
}

std::uint64_t Profiler::overheadAccumNs() const noexcept
{
    const std::uint64_t step = accumulatedNs(ProfileSection::SimStep);
    const std::uint64_t inner = simSectionsAccumNs();
    return step > inner ? step - inner : 0;
}

double Profiler::percentOfStep(const ProfileSection section) const noexcept
{
    const std::uint64_t step = accumulatedNs(ProfileSection::SimStep);
    if (step == 0)
    {
        return 0.0;
    }
    return static_cast<double>(accumulatedNs(section)) / static_cast<double>(step) * 100.0;
}

void Profiler::reset() noexcept
{
    for (auto& stat : stats_)
    {
        stat = Stat{};
    }
}

std::string Profiler::report() const
{
    std::string out = "=== PROFILER (per-system) ===\n";
    char line[160];
    std::snprintf(line, sizeof(line), "steps=%llu  simstep avg=%.2f us\n",
                  static_cast<unsigned long long>(stepCount()), averageUs(ProfileSection::SimStep));
    out += line;
    std::snprintf(line, sizeof(line), "%-14s %12s %10s %8s %10s\n",
                  "section", "accum_us", "avg_us", "%step", "calls");
    out += line;
    for (int i = 0; i < static_cast<int>(ProfileSection::Count); ++i)
    {
        const auto section = static_cast<ProfileSection>(i);
        const Stat& stat = stats_[static_cast<std::size_t>(i)];
        std::snprintf(line, sizeof(line), "%-14s %12.1f %10.3f %8.2f %10llu\n",
                      profileSectionName(section), accumulatedUs(section), averageUs(section),
                      percentOfStep(section), static_cast<unsigned long long>(stat.calls));
        out += line;
    }
    std::snprintf(line, sizeof(line), "%-14s %12.1f %8s %8.2f\n", "overhead",
                  static_cast<double>(overheadAccumNs()) / 1000.0, "",
                  accumulatedNs(ProfileSection::SimStep) > 0
                      ? static_cast<double>(overheadAccumNs()) /
                            static_cast<double>(accumulatedNs(ProfileSection::SimStep)) * 100.0
                      : 0.0);
    out += line;
    return out;
}

ScopedTimer::ScopedTimer(Profiler& profiler, const ProfileSection section) noexcept
    : profiler_(profiler.enabled() ? &profiler : nullptr), section_(section), startNs_(0)
{
    if (profiler_ != nullptr)
    {
        startNs_ = nowNanoseconds();
    }
}

ScopedTimer::~ScopedTimer()
{
    if (profiler_ != nullptr)
    {
        profiler_->addSample(section_, nowNanoseconds() - startNs_);
    }
}
} // namespace agentbiosim::core
