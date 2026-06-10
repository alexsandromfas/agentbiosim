#include "systems/Phase30Diagnostics.hpp"

#include "bench/Benchmark.hpp"
#include "config/ParameterDefaults.hpp"
#include "config/ParameterRegistry.hpp"
#include "core/Profiler.hpp"
#include "sim/SimulationRunner.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

namespace agentbiosim::systems
{
namespace
{
// Builds a runner mirroring a benchmark scenario (same knobs the Phase 29
// runner path sets), with the profiler FORCED — the same data path the
// Developer Window uses.
config::ParameterRegistry registryForScenario(const bench::BenchmarkScenario& sc)
{
    config::ParameterRegistry reg = config::createDefaultParameterRegistry();
    static_cast<void>(reg.setValue("random_seed", sc.seed));
    static_cast<void>(reg.setValue("bacteria_count", sc.agents));
    static_cast<void>(reg.setValue("food_target", sc.foods));
    static_cast<void>(reg.setValue("neural_network_type", std::string(sc.neuralType)));
    static_cast<void>(reg.setValue("retina_vision_mode", std::string(sc.visionMode)));
    static_cast<void>(reg.setValue("predators_enabled", sc.predatorsEnabled));
    static_cast<void>(reg.setValue("auto_export_substrate", false));
    static_cast<void>(reg.setValue("metrics_enabled", false));
    return reg;
}

std::string positionsChecksum(const sim::SimulationRunner& runner)
{
    double sx = 0.0;
    double sy = 0.0;
    double se = 0.0;
    for (std::size_t i = 0; i < runner.agents().size(); ++i)
    {
        const auto p = runner.agents().positionAt(i);
        sx += p.x;
        sy += p.y;
        se += runner.agents().energyAt(i);
    }
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%zu|%.9f|%.9f|%.9f", runner.agents().size(), sx, sy, se);
    return buf;
}
} // namespace

Phase30ValidationSummary runPhase30Validation()
{
    Phase30ValidationSummary summary;
    std::ostringstream log;
    auto check = [&](const bool cond, const std::string& name) {
        ++summary.checks;
        if (cond) { log << "  ok: " << name << '\n'; }
        else { summary.passed = false; log << "  FAIL: " << name << '\n'; }
    };

    bench::BenchmarkScenario sc;
    sc.name = "phase30";
    sc.agents = 150;
    sc.foods = 100;
    sc.steps = 60;
    sc.warmupSteps = 10;
    sc.repeats = 1;
    sc.seed = 30303;

    // ---- 1+2. Window data source vs Phase 29 benchmark, and sum ~100%. ----
    const bench::BenchmarkResult benchResult = bench::runScenario(sc);
    std::string benchTop;
    double benchTopPct = 0.0;
    double benchSum = 0.0;
    for (const auto& sec : benchResult.sections)
    {
        benchSum += sec.percentOfStep;
        if (sec.percentOfStep > benchTopPct && sec.name != "overhead")
        {
            benchTopPct = sec.percentOfStep;
            benchTop = sec.name;
        }
    }
    check(benchSum >= 90.0 && benchSum <= 115.0, "benchmark sections sum ~100%");

    // Same scenario through the Developer Window's data path: a runner with the
    // profiler forced (no profiler_enabled param), stepping the same workload.
    config::ParameterRegistry reg = registryForScenario(sc);
    sim::SimulationRunner runner(reg);
    runner.initialize();
    runner.setProfilerForced(true);
    for (int i = 0; i < sc.warmupSteps; ++i) runner.step(1.0 / 30.0);
    runner.profilerMutable().reset();
    for (int i = 0; i < sc.steps; ++i) runner.step(1.0 / 30.0);

    const core::Profiler& p = runner.profiler();
    check(p.stepCount() == static_cast<std::uint64_t>(sc.steps),
          "profiler forced on headless (no registry param) records steps");

    std::string liveTop;
    double liveTopPct = 0.0;
    double liveSum = 0.0;
    for (int i = 0; i <= static_cast<int>(core::ProfileSection::SpatialHash); ++i)
    {
        const auto s = static_cast<core::ProfileSection>(i);
        const double pct = p.percentOfStep(s);
        liveSum += pct;
        if (pct > liveTopPct)
        {
            liveTopPct = pct;
            liveTop = core::profileSectionName(s);
        }
    }
    const double simStepNs = static_cast<double>(p.accumulatedNs(core::ProfileSection::SimStep));
    const double overheadPct = simStepNs > 0.0
        ? static_cast<double>(p.overheadAccumNs()) / simStepNs * 100.0 : 0.0;
    liveSum += overheadPct;
    check(liveSum >= 90.0 && liveSum <= 115.0, "live sections + overhead sum ~100%");
    check(liveTop == benchTop, "top-cost system matches the Phase 29 benchmark (" +
                                   liveTop + " == " + benchTop + ")");
    check(std::abs(liveTopPct - benchTopPct) <= 15.0,
          "top-system share within 15pp of the benchmark");

    // ---- 3. Toggle determinism + a disabled system genuinely skips work. ----
    {
        config::ParameterRegistry regA = registryForScenario(sc);
        sim::SimulationRunner a(regA);
        a.initialize();
        for (int i = 0; i < 8; ++i) a.step(1.0 / 30.0);

        config::ParameterRegistry regB = registryForScenario(sc);
        sim::SimulationRunner b(regB);
        b.initialize();
        // Toggle several systems off and back ON before any step: flags are only
        // read at step time, so the run must be identical.
        static_cast<void>(b.applyCommand(core::Command{core::CmdSetDevSystemEnabled{
            static_cast<int>(core::ProfileSection::Neural), false}}));
        static_cast<void>(b.applyCommand(core::Command{core::CmdSetDevSystemEnabled{
            static_cast<int>(core::ProfileSection::Collision), false}}));
        static_cast<void>(b.applyCommand(core::Command{core::CmdSetDevSystemEnabled{-1, true}}));
        for (int i = 0; i < 8; ++i) b.step(1.0 / 30.0);
        check(positionsChecksum(a) == positionsChecksum(b),
              "toggle off + restore before step keeps the run identical");
    }
    {
        config::ParameterRegistry regC = registryForScenario(sc);
        sim::SimulationRunner c(regC);
        c.initialize();
        // Disable food replenishment + interaction, wipe the food: with the Food
        // system off the store must stay empty; re-enabling replenishes again.
        static_cast<void>(c.applyCommand(core::Command{core::CmdClearFood{}}));
        static_cast<void>(c.applyCommand(core::Command{core::CmdSetDevSystemEnabled{
            static_cast<int>(core::ProfileSection::Food), false}}));
        for (int i = 0; i < 3; ++i) c.step(1.0 / 30.0);
        check(c.foods().empty(), "disabled Food system genuinely skips replenishment");
        static_cast<void>(c.applyCommand(core::Command{core::CmdSetDevSystemEnabled{-1, true}}));
        c.step(1.0 / 30.0);
        check(!c.foods().empty(), "re-enabled Food system replenishes again");
        // Several systems off at once must not crash (stale spatial hash etc.).
        for (int s = 0; s <= static_cast<int>(core::ProfileSection::SpatialHash); ++s)
        {
            static_cast<void>(c.applyCommand(core::Command{core::CmdSetDevSystemEnabled{s, false}}));
        }
        for (int i = 0; i < 3; ++i) c.step(1.0 / 30.0);
        check(true, "all systems off steps without crash");
    }

    // ---- 4. Embedded benchmark path: reproducible + isolated state. ----
    {
        const bench::BenchmarkResult r1 = bench::runScenario(sc);
        const bench::BenchmarkResult r2 = bench::runScenario(sc);
        check(r1.finalAgents == r2.finalAgents && r1.finalFoods == r2.finalFoods,
              "embedded scenario reproducible (same seed -> same final counts)");
    }

    // ---- 5. Dev-window counters are readable headless. ----
    {
        const auto counts = runner.brainTypeCounts();
        std::size_t total = 0;
        for (const std::size_t n : counts) total += n;
        check(total == runner.agents().size(), "brain-type counts cover every agent");
        check(runner.approxBrainBytes() > 0, "brain memory estimate positive");
        check(runner.spatialHash().stats().totalCells > 0, "spatial hash stats readable");
    }

    summary.details = log.str();
    return summary;
}

std::vector<Phase30DiagnosticsRow> runPhase30Diagnostics()
{
    using clock = std::chrono::high_resolution_clock;
    std::vector<Phase30DiagnosticsRow> rows;
    for (const int agents : {100, 300, 600, 1000})
    {
        for (const bool forced : {false, true})
        {
            bench::BenchmarkScenario sc;
            sc.agents = agents;
            sc.foods = agents / 2;
            sc.seed = 777;
            config::ParameterRegistry reg = registryForScenario(sc);
            sim::SimulationRunner runner(reg);
            runner.initialize();
            runner.setProfilerForced(forced);
            for (int i = 0; i < 5; ++i) runner.step(1.0 / 30.0);

            constexpr int iterations = 30;
            const auto start = clock::now();
            for (int i = 0; i < iterations; ++i) runner.step(1.0 / 30.0);
            const auto end = clock::now();

            Phase30DiagnosticsRow row;
            row.agents = runner.agents().size();
            row.profilerForced = forced;
            row.stepMilliseconds =
                std::chrono::duration<double, std::milli>(end - start).count() / iterations;
            rows.push_back(row);
        }
    }
    return rows;
}
} // namespace agentbiosim::systems
