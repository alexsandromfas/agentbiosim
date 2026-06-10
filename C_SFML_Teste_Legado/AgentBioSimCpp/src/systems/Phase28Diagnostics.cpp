#include "systems/Phase28Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterRegistry.hpp"
#include "io/SaveFile.hpp"
#include "neural/BrainSerializer.hpp"
#include "sim/SimulationRunner.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
double brainChecksum(const neural::BrainSnapshot& b)
{
    double s = 0.0;
    for (const auto& layer : b.weights) for (const double w : layer) s += w;
    for (const auto& layer : b.biases) for (const double w : layer) s += w;
    for (const auto& layer : b.gates) for (const double w : layer) s += w;
    for (const double w : b.shortcutWeights) s += w;
    for (const double w : b.shortcutBias) s += w;
    for (const double w : b.recurrentWeights) s += w;
    for (const double w : b.recurrentState) s += w;
    for (const auto& c : b.neatConnections)
        s += c.weight + (c.enabled ? 0.5 : 0.0) + static_cast<double>(c.src) * 1e-3 +
             static_cast<double>(c.dst) * 2e-3;
    for (const auto& kv : b.neatState) s += kv.second;
    s += static_cast<double>(b.neatNextNodeId) + static_cast<double>(b.neatNextInnovation);
    return s;
}

bool close(const double a, const double b) { return std::abs(a - b) < 1e-9; }
} // namespace

Phase28ValidationSummary runPhase28Validation()
{
    Phase28ValidationSummary summary;
    std::ostringstream log;
    auto check = [&](const bool cond, const std::string& name) {
        ++summary.checks;
        if (cond) { log << "  ok: " << name << '\n'; }
        else { summary.passed = false; log << "  FAIL: " << name << '\n'; }
    };

    const std::string tmpPath = "phase28_selftest_tmp.agentbiosim";
    const std::vector<std::string> types = {
        "mlp", "gated_mlp", "shortcut_mlp", "modulated_mlp",
        "simple_rnn", "neat", "proto_neat", "recurrent_neat"};

    for (const auto& type : types)
    {
        config::ParameterRegistry reg = config::createDefaultParameterRegistry();
        static_cast<void>(reg.setValue("neural_network_type", type));

        sim::SimulationRunner r1(reg);
        r1.initialize();
        for (int i = 0; i < 10; ++i) r1.step(1.0 / 30.0);

        const sim::SimulationSnapshot before = r1.snapshot();
        check(before.agents.size() > 0, "has agents to save [" + type + "]");
        check(before.brains.size() == before.agents.size(), "one brain per agent [" + type + "]");

        io::SaveBundle bundle;
        bundle.snapshot = before;
        bundle.params.emplace_back("neural_network_type", config::ParameterValue{type});
        bundle.params.emplace_back("time_scale", config::ParameterValue{2.5});
        bundle.camera.valid = true;
        bundle.camera.zoom = 1.75;

        std::string err;
        const bool saved = io::saveToFile(tmpPath, bundle, err);
        check(saved, "save ok [" + type + "] " + err);

        const io::LoadResult loaded = io::loadFromFile(tmpPath);
        check(loaded.ok, "load ok [" + type + "] " + loaded.error);
        if (!loaded.ok) continue;

        // Restore into a fresh runner and re-capture to compare.
        sim::SimulationRunner r2(reg);
        r2.initialize();
        r2.restore(loaded.bundle.snapshot);
        const sim::SimulationSnapshot after = r2.snapshot();

        check(after.agents.size() == before.agents.size(), "agent count preserved [" + type + "]");
        check(after.foods.size() == before.foods.size(), "food count preserved [" + type + "]");
        check(after.species.size() == before.species.size(), "species preserved [" + type + "]");
        check(after.genomes.size() == before.genomes.size(), "genomes preserved [" + type + "]");
        check(after.brains.size() == before.brains.size(), "brain count preserved [" + type + "]");
        check(after.stepsExecuted == before.stepsExecuted, "step counter preserved [" + type + "]");
        check(close(after.world.width, before.world.width) &&
                  static_cast<int>(after.world.shape) == static_cast<int>(before.world.shape),
              "world preserved [" + type + "]");

        // Agent state (positions/energy) by id.
        std::unordered_map<std::uint64_t, std::size_t> idxAfter;
        for (std::size_t i = 0; i < after.agentIds.size(); ++i)
            idxAfter[after.agentIds[i].value] = i;
        bool agentsMatch = true;
        for (std::size_t i = 0; i < before.agentIds.size(); ++i)
        {
            const auto it = idxAfter.find(before.agentIds[i].value);
            if (it == idxAfter.end()) { agentsMatch = false; break; }
            const auto& a = before.agents[i];
            const auto& b = after.agents[it->second];
            if (!close(a.position.x, b.position.x) || !close(a.position.y, b.position.y) ||
                !close(a.energy, b.energy) || !close(a.age, b.age) || a.genomeId != b.genomeId)
            {
                agentsMatch = false;
                break;
            }
        }
        check(agentsMatch, "agent positions/energy/genome preserved [" + type + "]");

        // Brains: the "minds" — compare a checksum of every brain by agent id.
        std::unordered_map<std::uint64_t, double> sumBefore;
        for (const auto& kv : before.brains) sumBefore[kv.first] = brainChecksum(kv.second);
        bool brainsMatch = true;
        for (const auto& kv : after.brains)
        {
            const auto it = sumBefore.find(kv.first);
            if (it == sumBefore.end() || !close(it->second, brainChecksum(kv.second)))
            {
                brainsMatch = false;
                break;
            }
        }
        check(brainsMatch, "brains (minds) preserved [" + type + "]");

        // A loaded simulation keeps stepping without crashing.
        for (int i = 0; i < 5; ++i) r2.step(1.0 / 30.0);
        check(true, "loaded sim steps without crash [" + type + "]");

        // Parameters round-trip through the file.
        bool foundParam = false;
        for (const auto& p : loaded.bundle.params)
        {
            if (p.first == "neural_network_type") foundParam = true;
        }
        check(foundParam, "params round-trip [" + type + "]");
        check(loaded.bundle.camera.valid && close(loaded.bundle.camera.zoom, 1.75),
              "camera round-trip [" + type + "]");
    }

    std::remove(tmpPath.c_str());
    summary.details = log.str();
    return summary;
}

std::vector<Phase28DiagnosticsRow> runPhase28Diagnostics()
{
    std::vector<Phase28DiagnosticsRow> rows;
    const std::string tmpPath = "phase28_diag_tmp.agentbiosim";
    const std::vector<std::string> types = {"mlp", "simple_rnn", "neat"};
    const std::vector<std::size_t> counts = {100, 600, 1000};

    for (const auto& type : types)
    {
        for (const std::size_t count : counts)
        {
            config::ParameterRegistry reg = config::createDefaultParameterRegistry();
            static_cast<void>(reg.setValue("neural_network_type", type));
            static_cast<void>(reg.setValue("bacteria_count", static_cast<int>(count)));
            static_cast<void>(reg.setValue("predators_enabled", false));
            sim::SimulationRunner runner(reg);
            runner.initialize();
            for (int i = 0; i < 5; ++i) runner.step(1.0 / 30.0);

            io::SaveBundle bundle;
            bundle.snapshot = runner.snapshot();

            std::string err;
            const auto t0 = std::chrono::high_resolution_clock::now();
            static_cast<void>(io::saveToFile(tmpPath, bundle, err));
            const auto t1 = std::chrono::high_resolution_clock::now();
            const io::LoadResult loaded = io::loadFromFile(tmpPath);
            const auto t2 = std::chrono::high_resolution_clock::now();
            static_cast<void>(loaded);

            std::size_t bytes = 0;
            {
                std::ifstream f(tmpPath, std::ios::binary | std::ios::ate);
                if (f) bytes = static_cast<std::size_t>(f.tellg());
            }

            Phase28DiagnosticsRow row;
            row.brainType = type;
            row.agents = runner.agents().size();
            row.saveMilliseconds = std::chrono::duration<double, std::milli>(t1 - t0).count();
            row.loadMilliseconds = std::chrono::duration<double, std::milli>(t2 - t1).count();
            row.fileBytes = bytes;
            rows.push_back(row);
        }
    }
    std::remove(tmpPath.c_str());
    return rows;
}
} // namespace agentbiosim::systems
