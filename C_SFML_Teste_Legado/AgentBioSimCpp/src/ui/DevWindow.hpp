#pragma once

#include "bench/Benchmark.hpp"
#include "config/ParameterRegistry.hpp"
#include "core/Command.hpp"
#include "ui/UiState.hpp"

#include <future>
#include <vector>

namespace agentbiosim::sim
{
class SimulationRunner;
}

namespace agentbiosim::ui
{
// Phase 30: the Developer Window — in-app performance observability. It READS
// the Phase 27 per-system profiler (the runner owns it) and can launch a Phase
// 29 benchmark scenario on a background thread with fully isolated state (its
// own registry + runner), so the live simulation is never disturbed.
//
// Cost when hidden: draw() returns immediately; no history is collected and the
// profiler is not forced (App only forces it while `state.showDevWindow`).
// The cost-isolation toggles are restored automatically when the window closes.
class DevWindow
{
public:
    void draw(const config::ParameterRegistry& registry,
              const sim::SimulationRunner& runner,
              UiState& state,
              core::CommandQueue& queue,
              float fps);

private:
    void drawCostTable(const sim::SimulationRunner& runner);
    void drawHistory(const sim::SimulationRunner& runner, float fps);
    void drawCounters(const sim::SimulationRunner& runner);
    void drawToggles(const sim::SimulationRunner& runner, core::CommandQueue& queue);
    void drawEmbeddedBenchmark();

    // Sliding-window history (collected only while the window is open).
    static constexpr int kHistorySize = 240;
    std::vector<float> histTotal_;       // us/step (SimStep, instantaneous)
    std::vector<float> histPerception_;
    std::vector<float> histNeural_;
    std::vector<float> histCollision_;
    std::vector<float> histFps_;

    // Embedded benchmark (background thread; isolated state).
    int benchAgents_ = 300;
    int benchFoods_ = 150;
    int benchSteps_ = 120;
    int benchSeed_ = 1234;
    int benchNeuralIdx_ = 0;
    int benchVisionIdx_ = 0;
    std::future<bench::BenchmarkResult> benchFuture_;
    bool benchRunning_ = false;
    bool benchHasResult_ = false;
    bench::BenchmarkResult benchResult_{};

    bool sortByCost_ = true;
    bool wasOpen_ = false;  // detect close -> restore toggles + drop history
};
} // namespace agentbiosim::ui
