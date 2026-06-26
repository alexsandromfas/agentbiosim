#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace agentbiosim::core
{
// Phase 27: scoped per-system profiler. This is the foundation the Fase 29
// benchmark runner and the Fase 30 developer window consume.
//
// Each engine system gets a section. `SimStep` wraps the whole simulation step
// so that "overhead" can be derived as SimStep minus the sum of the inner
// sim sections. `Render` and `Ui` are per-frame (measured in App), outside the
// step, so they do not affect the SimStep/overhead relationship.
//
// The profiler is OFF by default and a ScopedTimer is a single branch + nothing
// when disabled, so steady-state cost is effectively zero.
enum class ProfileSection : int
{
    Perception = 0,
    Neural,
    Movement,
    Collision,
    Energy,
    Interaction,
    Food,
    Reproduction,
    Death,
    SpatialHash,
    Render,
    Ui,
    SimStep,  // whole simulation step (wraps the sim sections above)
    Count
};

inline constexpr int kProfileSectionCount = static_cast<int>(ProfileSection::Count);

// Stable lowercase identifier for a section (used in reports/CSV and the UI).
[[nodiscard]] const char* profileSectionName(ProfileSection section) noexcept;

class Profiler
{
public:
    struct Stat
    {
        std::uint64_t accumNs = 0;  // total nanoseconds across all calls
        std::uint64_t calls = 0;    // number of scopes recorded
        std::uint64_t lastNs = 0;   // duration of the most recent scope (instantaneous)
        std::uint64_t maxNs = 0;    // worst single scope
    };

    // Fase 35: enabled_ é atômico porque, no modo SIM/RENDER em 2 threads, o
    // worker chama setEnabled() dentro de step() (Fase B) enquanto a main lê
    // enabled() ao construir o ScopedTimer de Render (mesma Fase B). É o único
    // ponto de concorrência no profiler — os slots `stats_` são por-seção
    // disjuntos (worker: sim; main: Render/Ui).
    void setEnabled(bool enabled) noexcept { enabled_.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }

    // Record one scope sample. Called by ScopedTimer on scope exit.
    void addSample(ProfileSection section, std::uint64_t nanoseconds) noexcept;

    [[nodiscard]] const Stat& stat(ProfileSection section) const noexcept
    {
        return stats_[static_cast<std::size_t>(section)];
    }
    [[nodiscard]] std::uint64_t accumulatedNs(ProfileSection section) const noexcept
    {
        return stats_[static_cast<std::size_t>(section)].accumNs;
    }
    [[nodiscard]] double accumulatedUs(ProfileSection section) const noexcept
    {
        return static_cast<double>(accumulatedNs(section)) / 1000.0;
    }
    // Average microseconds per call (us/step for sim sections, us/frame for render/ui).
    [[nodiscard]] double averageUs(ProfileSection section) const noexcept;
    [[nodiscard]] double lastUs(ProfileSection section) const noexcept;

    [[nodiscard]] std::uint64_t stepCount() const noexcept
    {
        return stats_[static_cast<std::size_t>(ProfileSection::SimStep)].calls;
    }

    // Sum of the per-system sim sections (Perception..SpatialHash). Excludes
    // SimStep itself, Render and Ui.
    [[nodiscard]] std::uint64_t simSectionsAccumNs() const noexcept;
    // SimStep minus the sum of the sim sections, floored at 0 (unprofiled glue).
    [[nodiscard]] std::uint64_t overheadAccumNs() const noexcept;
    // Section share of the whole step (accumulated). Returns 0 when no step ran.
    [[nodiscard]] double percentOfStep(ProfileSection section) const noexcept;

    void reset() noexcept;
    [[nodiscard]] std::string report() const;

private:
    std::atomic<bool> enabled_{false};
    std::array<Stat, static_cast<std::size_t>(ProfileSection::Count)> stats_{};
};

// RAII scope timer. Does nothing (and reads no clock) when the profiler is
// disabled, so leaving timers in the hot path is cheap.
class ScopedTimer
{
public:
    ScopedTimer(Profiler& profiler, ProfileSection section) noexcept;
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    Profiler* profiler_;  // nullptr when disabled at construction
    ProfileSection section_;
    std::uint64_t startNs_;
};
} // namespace agentbiosim::core
