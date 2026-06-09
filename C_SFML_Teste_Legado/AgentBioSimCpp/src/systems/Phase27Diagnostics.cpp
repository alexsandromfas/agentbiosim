#include "systems/Phase27Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterRegistry.hpp"
#include "core/Logger.hpp"
#include "core/Profiler.hpp"
#include "sim/SimulationRunner.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
config::ParameterRegistry makeRegistry(const int seed, const bool profiler, const bool metrics,
                                       const int bacteriaCount)
{
    config::ParameterRegistry reg = config::createDefaultParameterRegistry();
    static_cast<void>(reg.setValue("random_seed", seed));
    static_cast<void>(reg.setValue("profiler_enabled", profiler));
    static_cast<void>(reg.setValue("metrics_enabled", metrics));
    if (bacteriaCount > 0)
    {
        static_cast<void>(reg.setValue("bacteria_count", bacteriaCount));
    }
    return reg;
}
} // namespace

Phase27ValidationSummary runPhase27Validation()
{
    Phase27ValidationSummary summary;
    std::string log;
    auto check = [&](const bool condition, const std::string& name) {
        ++summary.checks;
        if (condition) { log += "  ok: " + name + "\n"; }
        else { summary.passed = false; log += "  FAIL: " + name + "\n"; }
    };

    constexpr int kSteps = 25;

    // ---- Metrics: produced, latest matches live state, per-species present. ----
    {
        config::ParameterRegistry reg = makeRegistry(12345, false, true, 0);
        sim::SimulationRunner runner(reg);
        runner.initialize();
        for (int i = 0; i < kSteps; ++i) runner.step(1.0 / 30.0);

        const auto& m = runner.metrics();
        check(m.sampleCount() == static_cast<std::size_t>(kSteps), "metrics: one sample per step");
        check(m.latest() != nullptr, "metrics: latest sample exists");
        if (m.latest() != nullptr)
        {
            check(static_cast<std::size_t>(m.latest()->population) == runner.agents().size(),
                  "metrics: latest population == live population");
        }
        check(!m.speciesMetrics().empty(), "metrics: per-species series present");
        check(m.smartFactor() >= 0.0, "metrics: group smart factor computed");
    }

    // ---- Metrics determinism: same seed => identical population series. ----
    {
        config::ParameterRegistry regA = makeRegistry(777, false, true, 0);
        config::ParameterRegistry regB = makeRegistry(777, false, true, 0);
        sim::SimulationRunner a(regA);
        sim::SimulationRunner b(regB);
        a.initialize();
        b.initialize();
        for (int i = 0; i < kSteps; ++i) { a.step(1.0 / 30.0); b.step(1.0 / 30.0); }
        const auto seriesA = a.metrics().series(MetricField::Population);
        const auto seriesB = b.metrics().series(MetricField::Population);
        bool identical = seriesA.size() == seriesB.size() && !seriesA.empty();
        for (std::size_t i = 0; i < seriesA.size() && identical; ++i)
        {
            identical = seriesA[i] == seriesB[i];
        }
        check(identical, "metrics: population series deterministic for fixed seed");
    }

    // ---- Profiler ON: per-system recording + nesting invariant. ----
    {
        config::ParameterRegistry reg = makeRegistry(12345, true, false, 0);
        sim::SimulationRunner runner(reg);
        runner.initialize();
        for (int i = 0; i < kSteps; ++i) runner.step(1.0 / 30.0);

        const core::Profiler& p = runner.profiler();
        check(p.stepCount() == static_cast<std::uint64_t>(kSteps), "profiler: SimStep per step");
        check(p.stat(core::ProfileSection::Perception).calls == static_cast<std::uint64_t>(kSteps),
              "profiler: perception once per step");
        check(p.stat(core::ProfileSection::SpatialHash).calls ==
                  static_cast<std::uint64_t>(kSteps) * 2U,
              "profiler: spatialhash twice per step");
        check(p.accumulatedNs(core::ProfileSection::SimStep) > 0, "profiler: SimStep accumulated > 0");
        // Nesting invariant: the inner sim sections sum to <= the whole step
        // (a tiny slack covers timer overhead inside SimStep).
        const double inner = static_cast<double>(p.simSectionsAccumNs());
        const double step = static_cast<double>(p.accumulatedNs(core::ProfileSection::SimStep));
        check(inner <= step * 1.05, "profiler: sum(sections) <= SimStep (overhead margin)");
        check(inner > 0.0, "profiler: sections cover real time");
        check(p.overheadAccumNs() <= p.accumulatedNs(core::ProfileSection::SimStep),
              "profiler: overhead well-defined");
    }

    // ---- Profiler OFF: nothing recorded (zero cost). ----
    {
        config::ParameterRegistry reg = makeRegistry(12345, false, false, 0);
        sim::SimulationRunner runner(reg);
        runner.initialize();
        for (int i = 0; i < kSteps; ++i) runner.step(1.0 / 30.0);
        const core::Profiler& p = runner.profiler();
        check(p.stepCount() == 0, "profiler off: no SimStep recorded");
        check(p.accumulatedNs(core::ProfileSection::Neural) == 0, "profiler off: no section recorded");
    }

    // ---- Logger level gating round-trips. ----
    {
        check(core::logLevelFromString("off") == core::LogLevel::Off, "log: off parses");
        check(core::logLevelFromString("info") == core::LogLevel::Info, "log: info parses");
        check(core::logLevelFromString("debug") == core::LogLevel::Debug, "log: debug parses");
        core::Logger::instance().setLevel(core::LogLevel::Off);
        core::Logger::instance().log(core::LogLevel::Info, "SHOULD_NOT_WRITE");  // must not crash
        check(core::Logger::instance().level() == core::LogLevel::Off, "log: level off honored");
    }

    summary.details = log;
    return summary;
}

std::vector<Phase27DiagnosticsRow> runPhase27Diagnostics()
{
    std::vector<Phase27DiagnosticsRow> rows;
    const std::vector<std::size_t> counts = {100, 300, 600, 1000};
    for (const std::size_t count : counts)
    {
        for (const bool profiler : {false, true})
        {
            for (const bool metrics : {false, true})
            {
                config::ParameterRegistry reg =
                    makeRegistry(20260602, profiler, metrics, static_cast<int>(count));
                static_cast<void>(reg.setValue("predators_enabled", false));
                sim::SimulationRunner runner(reg);
                runner.initialize();
                runner.step(1.0 / 30.0);  // warmup

                constexpr int iterations = 30;
                const auto start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < iterations; ++i) runner.step(1.0 / 30.0);
                const auto end = std::chrono::high_resolution_clock::now();

                Phase27DiagnosticsRow row;
                row.agents = runner.agents().size();
                row.profilerOn = profiler;
                row.metricsOn = metrics;
                row.stepMilliseconds =
                    std::chrono::duration<double, std::milli>(end - start).count() / iterations;
                rows.push_back(row);
            }
        }
    }
    return rows;
}
} // namespace agentbiosim::systems
