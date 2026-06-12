#include "systems/Phase32Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterRegistry.hpp"
#include "sim/SimulationRunner.hpp"
#include "systems/MetricsSystem.hpp"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
struct ChecksumScenario
{
    const char* name;
    int agents;
    int steps;
    const char* neuralType;
    const char* visionMode;
};

// Scenarios chosen to exercise every hot path the optimizations touch:
// dense + recurrent + NEAT brains, single + sector vision, reproduction,
// eating, rescue, deaths.
const ChecksumScenario kScenarios[] = {
    {"mlp_single", 300, 120, "mlp", "single"},
    {"mlp_sector", 300, 120, "mlp", "sector"},
    {"rnn_single", 200, 100, "simple_rnn", "single"},
    {"neat_single", 150, 80, "neat", "single"},
};

config::ParameterRegistry registryFor(const ChecksumScenario& sc)
{
    config::ParameterRegistry reg = config::createDefaultParameterRegistry();
    static_cast<void>(reg.setValue("random_seed", 9090));
    static_cast<void>(reg.setValue("bacteria_count", sc.agents));
    static_cast<void>(reg.setValue("neural_network_type", std::string(sc.neuralType)));
    static_cast<void>(reg.setValue("retina_vision_mode", std::string(sc.visionMode)));
    static_cast<void>(reg.setValue("auto_export_substrate", false));
    static_cast<void>(reg.setValue("metrics_enabled", true));
    // Microfase 32.2: the scenarios spawn MORE agents than the new default
    // per-label cap (32.1: max_limit=150), which would block every birth and
    // silently drop reproduction/mutation coverage from the golden digest.
    // Uncap so the digest keeps exercising births — this also keeps the digest
    // byte-identical to the historical Phase 32 golden artifact.
    static_cast<void>(reg.setValue("bacteria_max_limit", 0));
    return reg;
}

// High-precision digest of the full dynamic state. %.17g round-trips doubles
// exactly, so any behavioral drift shows up as a different string.
std::string digestRunner(const sim::SimulationRunner& runner)
{
    double px = 0.0;
    double py = 0.0;
    double pe = 0.0;
    double pa = 0.0;
    for (std::size_t i = 0; i < runner.agents().size(); ++i)
    {
        const auto p = runner.agents().positionAt(i);
        px += p.x;
        py += p.y;
        pe += runner.agents().energyAt(i);
        pa += runner.agents().ageAt(i);
    }
    double fx = 0.0;
    double fe = 0.0;
    for (std::size_t i = 0; i < runner.foods().size(); ++i)
    {
        const auto p = runner.foods().positionAt(i);
        fx += p.x + p.y;
        fe += runner.foods().energyAt(i);
    }
    double brains = 0.0;
    for (const auto& kv : runner.snapshot().brains)
    {
        const auto& b = kv.second;
        for (const auto& layer : b.weights) for (const double w : layer) brains += w;
        for (const double w : b.recurrentState) brains += w;
        for (const auto& c : b.neatConnections) brains += c.weight;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf), "a=%zu f=%zu g=%zu px=%.17g py=%.17g pe=%.17g pa=%.17g fx=%.17g fe=%.17g bw=%.17g",
                  runner.agents().size(), runner.foods().size(), runner.genomes().size(),
                  px, py, pe, pa, fx, fe, brains);
    return buf;
}

std::string runScenarioDigest(const ChecksumScenario& sc)
{
    config::ParameterRegistry reg = registryFor(sc);
    sim::SimulationRunner runner(reg);
    runner.initialize();
    for (int i = 0; i < sc.steps; ++i)
    {
        runner.step(1.0 / 30.0);
    }
    return digestRunner(runner);
}

std::vector<float> metricsSeries(const ChecksumScenario& sc)
{
    config::ParameterRegistry reg = registryFor(sc);
    sim::SimulationRunner runner(reg);
    runner.initialize();
    for (int i = 0; i < sc.steps; ++i)
    {
        runner.step(1.0 / 30.0);
    }
    std::vector<float> all = runner.metrics().series(MetricField::Population);
    const std::vector<float> energy = runner.metrics().series(MetricField::MeanEnergy);
    all.insert(all.end(), energy.begin(), energy.end());
    return all;
}
} // namespace

std::string runPhase32StateChecksum()
{
    std::ostringstream out;
    for (const auto& sc : kScenarios)
    {
        out << sc.name << ": " << runScenarioDigest(sc) << '\n';
    }
    return out.str();
}

Phase32ValidationSummary runPhase32Validation()
{
    Phase32ValidationSummary summary;
    std::ostringstream log;
    auto check = [&](const bool cond, const std::string& name) {
        ++summary.checks;
        if (cond) { log << "  ok: " << name << '\n'; }
        else { summary.passed = false; log << "  FAIL: " << name << '\n'; }
    };

    // 1. Determinism: same seed -> bit-identical state, for every scenario
    //    (covers the multithreaded paths: thread count must not affect results).
    for (const auto& sc : kScenarios)
    {
        const std::string a = runScenarioDigest(sc);
        const std::string b = runScenarioDigest(sc);
        check(!a.empty() && a == b,
              std::string("determinismo bit-identico (2 execucoes) [") + sc.name + "]");
    }

    // 2. Metrics series unchanged between two same-seed runs.
    {
        const auto a = metricsSeries(kScenarios[0]);
        const auto b = metricsSeries(kScenarios[0]);
        check(!a.empty() && a == b, "series de metricas identicas entre execucoes");
    }

    summary.details = log.str();
    return summary;
}
} // namespace agentbiosim::systems
