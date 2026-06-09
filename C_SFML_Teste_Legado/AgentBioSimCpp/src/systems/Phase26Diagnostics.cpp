#include "systems/Phase26Diagnostics.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterRegistry.hpp"
#include "neural/BrainFactory.hpp"
#include "neural/BrainVariant.hpp"
#include "neural/NeuralView.hpp"
#include "sim/SimulationRunner.hpp"

#include <chrono>
#include <cmath>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
namespace
{
neural::BrainVariant makeBrain(const std::string& typeText, const std::size_t inputSize,
                               const std::size_t outputSize, const std::uint64_t seed)
{
    config::ParameterRegistry reg = config::createDefaultParameterRegistry();
    static_cast<void>(reg.setValue("neural_network_type", typeText));
    const neural::BrainConfig cfg =
        neural::BrainFactory::configFromRegistry(reg, "bacteria", inputSize, outputSize);
    std::mt19937_64 rng(seed);
    neural::BrainCreationResult created = neural::BrainFactory::createBrain(cfg, rng);
    return std::move(created.brain);
}
} // namespace

Phase26ValidationSummary runPhase26Validation()
{
    Phase26ValidationSummary summary;
    std::ostringstream log;
    auto check = [&](const bool condition, const std::string& name) {
        ++summary.checks;
        if (condition)
        {
            log << "  ok: " << name << '\n';
        }
        else
        {
            summary.passed = false;
            log << "  FAIL: " << name << '\n';
        }
    };

    // ---- Per-type: trace matches forward, and the view builds correctly. ----
    const std::size_t inputSize = 6;
    const std::size_t outputSize = 2;
    std::vector<double> input(inputSize, 0.0);
    for (std::size_t i = 0; i < inputSize; ++i)
    {
        input[i] = 0.13 * static_cast<double>(i + 1);
    }

    const std::vector<std::string> types = {
        "mlp", "gated_mlp", "shortcut_mlp", "modulated_mlp",
        "simple_rnn", "neat", "proto_neat", "recurrent_neat"};

    for (const auto& type : types)
    {
        neural::BrainVariant brain = makeBrain(type, inputSize, outputSize, 424242ULL);
        neural::ActivationTrace trace;
        const std::vector<double> output = neural::forwardOf(brain, input, &trace);
        check(output.size() == outputSize, "output size [" + type + "]");

        const bool neat = type.find("neat") != std::string::npos;
        if (!neat)
        {
            check(!trace.layers.empty(), "trace has layers [" + type + "]");
            if (!trace.layers.empty())
            {
                const std::vector<double>& last = trace.layers.back().values;
                bool match = last.size() == output.size();
                for (std::size_t i = 0; i < output.size() && match; ++i)
                {
                    match = std::abs(last[i] - output[i]) < 1.0e-9;
                }
                check(match, "trace last layer == forward output [" + type + "]");
            }
        }
        else
        {
            check(!trace.neatNodes.empty(), "trace has NEAT nodes [" + type + "]");
        }

        const neural::NeuralView view = neural::buildNeuralView(brain, trace, &input);
        check(view.valid, "view valid [" + type + "]");
        check(!view.nodes.empty(), "view has nodes [" + type + "]");
        check(view.columnCount > 0, "view has columns [" + type + "]");
        if (!neat)
        {
            check(!view.edges.empty(), "view has edges [" + type + "]");
            // Column 0 should carry the supplied input activations.
            bool inputColumnOk = false;
            for (const auto& node : view.nodes)
            {
                if (node.column == 0 && node.kind == 0 && node.row == 0)
                {
                    inputColumnOk = std::abs(node.activation - input[0]) < 1.0e-9;
                    break;
                }
            }
            check(inputColumnOk, "view input column = input [" + type + "]");
        }
    }

    // ---- Trace-on-demand: hidden viewer costs nothing (count stays 0). ----
    {
        config::ParameterRegistry reg = config::createDefaultParameterRegistry();
        sim::SimulationRunner runner(reg);
        runner.initialize();
        for (int i = 0; i < 5; ++i)
        {
            runner.step(1.0 / 30.0);
        }
        check(runner.neuralTraceCount() == 0, "hidden viewer: trace count == 0");
        check(!runner.hasSelectedNeuralView(), "hidden viewer: no view");
    }

    // ---- Selecting an agent exposes a valid view + its genome. ----
    {
        config::ParameterRegistry reg = config::createDefaultParameterRegistry();
        sim::SimulationRunner runner(reg);
        runner.initialize();
        check(runner.agents().size() > 0, "runner spawned agents");
        if (runner.agents().size() > 0)
        {
            const auto id = runner.agents().idAt(0);
            runner.setNeuralViewerTarget(id);
            runner.step(1.0 / 30.0);
            check(runner.neuralTraceCount() > 0, "target: trace captured");
            check(runner.hasSelectedNeuralView(), "target: view available");
            check(runner.selectedNeuralView().valid, "target: view valid");
            check(!runner.selectedNeuralView().nodes.empty(), "target: view has nodes");
            check(!runner.selectedNeuralTrace().layers.empty() ||
                      !runner.selectedNeuralTrace().neatNodes.empty(),
                  "target: trace populated");

            const auto idx = runner.agents().indexOf(id);
            check(idx.has_value(), "selected agent has index");
            if (idx.has_value())
            {
                const auto genomeId = runner.agents().genomeIdAt(*idx);
                check(runner.genomes().find(genomeId) != nullptr, "selected agent genome record");
            }
        }
    }

    // ---- Removing the targeted agent clears safely (no crash, view stale). ----
    {
        config::ParameterRegistry reg = config::createDefaultParameterRegistry();
        sim::SimulationRunner runner(reg);
        runner.initialize();
        if (runner.agents().size() > 0)
        {
            const auto id = runner.agents().idAt(0);
            runner.setNeuralViewerTarget(id);
            runner.step(1.0 / 30.0);
            runner.deleteAgents({id});
            runner.step(1.0 / 30.0);  // must not crash
            check(!runner.agents().contains(id), "removed agent is gone");
            check(!runner.hasSelectedNeuralView(), "removed target: view stale/cleared");
        }
    }

    // ---- Switching neural type does not break the viewer path end-to-end. ----
    for (const std::string& type : {std::string("simple_rnn"), std::string("neat"),
                                    std::string("recurrent_neat")})
    {
        config::ParameterRegistry reg = config::createDefaultParameterRegistry();
        static_cast<void>(reg.setValue("neural_network_type", type));
        sim::SimulationRunner runner(reg);
        runner.initialize();
        if (runner.agents().size() > 0)
        {
            runner.setNeuralViewerTarget(runner.agents().idAt(0));
            runner.step(1.0 / 30.0);
            check(runner.hasSelectedNeuralView(), "runner view available [" + type + "]");
            check(runner.selectedNeuralView().valid, "runner view valid [" + type + "]");
        }
    }

    summary.details = log.str();
    return summary;
}

std::vector<Phase26DiagnosticsRow> runPhase26Diagnostics()
{
    std::vector<Phase26DiagnosticsRow> rows;
    const std::vector<std::string> types = {"mlp", "simple_rnn", "neat"};
    const std::vector<std::size_t> counts = {100, 300, 600, 1000};

    for (const auto& type : types)
    {
        for (const std::size_t count : counts)
        {
            for (const bool viewerOn : {false, true})
            {
                config::ParameterRegistry reg = config::createDefaultParameterRegistry();
                static_cast<void>(reg.setValue("neural_network_type", type));
                static_cast<void>(reg.setValue("bacteria_count", static_cast<int>(count)));
                static_cast<void>(reg.setValue("predators_enabled", false));
                sim::SimulationRunner runner(reg);
                runner.initialize();
                if (viewerOn && runner.agents().size() > 0)
                {
                    runner.setNeuralViewerTarget(runner.agents().idAt(0));
                }
                runner.step(1.0 / 30.0);  // warmup (creates brains)

                constexpr int iterations = 30;
                const auto start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < iterations; ++i)
                {
                    runner.step(1.0 / 30.0);
                }
                const auto end = std::chrono::high_resolution_clock::now();

                Phase26DiagnosticsRow row;
                row.brainType = type;
                row.agents = runner.agents().size();
                row.viewerOn = viewerOn;
                row.stepMilliseconds =
                    std::chrono::duration<double, std::milli>(end - start).count() / iterations;
                row.traceCount = runner.neuralTraceCount();
                rows.push_back(row);
            }
        }
    }
    return rows;
}
} // namespace agentbiosim::systems
