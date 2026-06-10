#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Phase 29: formal headless benchmark runner. Runs parametrized scenarios with
// repetitions, reusing the Phase 27 per-system profiler, and emits reproducible
// CSV/JSON/Markdown reports with commit/build metadata. No window, no ImGui, no
// render — pure simulation timing. This is the data source for the Fase 30
// developer window and the Fase 33 performance proof.
namespace agentbiosim::bench
{
struct BenchmarkScenario
{
    std::string name;
    int agents = 300;
    int foods = 150;
    int steps = 300;       // measured steps per repeat
    int warmupSteps = 30;  // unmeasured warm-up
    int repeats = 3;
    int seed = 1234;
    std::string neuralType = "mlp";     // neural_network_type
    std::string visionMode = "single";  // retina_vision_mode
    bool predatorsEnabled = false;
};

struct SectionTiming
{
    std::string name;        // perception / neural / ... / overhead
    double avgUsPerStep = 0.0;
    double percentOfStep = 0.0;
};

struct BenchmarkResult
{
    BenchmarkScenario scenario;
    // Wall-clock per-step time, aggregated across repeats (each repeat = its
    // mean step time).
    double meanStepUs = 0.0;
    double minStepUs = 0.0;
    double maxStepUs = 0.0;
    double stdStepUs = 0.0;
    double stepsPerSecond = 0.0;  // 1e6 / meanStepUs
    // Per-system breakdown from the profiler (last repeat), plus overhead.
    std::vector<SectionTiming> sections;
    std::size_t finalAgents = 0;
    std::size_t finalFoods = 0;
    std::uint64_t stepsMeasured = 0;
};

struct BenchmarkMeta
{
    std::string commit;
    std::string buildMode;
    std::string version;
    std::string dateTime;
};

[[nodiscard]] BenchmarkMeta currentMeta();
[[nodiscard]] std::vector<BenchmarkScenario> defaultScenarios();
[[nodiscard]] BenchmarkResult runScenario(const BenchmarkScenario& scenario);
[[nodiscard]] std::vector<BenchmarkResult> runSuite(const std::vector<BenchmarkScenario>& scenarios);

// Report serializers (well-formed CSV / JSON / Markdown).
[[nodiscard]] std::string toCsv(const std::vector<BenchmarkResult>& results, const BenchmarkMeta& meta);
[[nodiscard]] std::string toJson(const std::vector<BenchmarkResult>& results, const BenchmarkMeta& meta);
[[nodiscard]] std::string toMarkdown(const std::vector<BenchmarkResult>& results, const BenchmarkMeta& meta);
// Writes timestamped reports under `benchmarks/results/` and `benchmarks/reports/`.
// Returns the common basename used (empty on failure).
[[nodiscard]] std::string writeReports(const std::vector<BenchmarkResult>& results,
                                       const BenchmarkMeta& meta);

// Phase 29 selftest: reproducibility (same seed -> same sim), per-system
// percentages sum ~100%, reports are well-formed.
struct Phase29ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};
[[nodiscard]] Phase29ValidationSummary runPhase29Validation();
} // namespace agentbiosim::bench
