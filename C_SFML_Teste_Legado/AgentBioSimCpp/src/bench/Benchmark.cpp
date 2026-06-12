#include "bench/Benchmark.hpp"

#include "config/ParameterDefaults.hpp"
#include "config/ParameterRegistry.hpp"
#include "core/BuildInfo.hpp"
#include "core/Profiler.hpp"
#include "core/Version.hpp"
#include "io/Json.hpp"
#include "sim/SimulationRunner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace agentbiosim::bench
{
namespace
{
std::string nowStamp(const bool fileSafe)
{
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    tmv = *std::localtime(&t);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), fileSafe ? "%Y%m%d_%H%M%S" : "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

std::string num(const double v, const int decimals = 3)
{
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}
} // namespace

BenchmarkMeta currentMeta()
{
    BenchmarkMeta meta;
    meta.commit = core::kGitCommit;
    meta.buildMode = core::kBuildMode;
    meta.version = kVersionString;
    meta.dateTime = nowStamp(false);
    return meta;
}

std::vector<BenchmarkScenario> defaultScenarios()
{
    std::vector<BenchmarkScenario> out;
    // Phase 32: scale ladder extended to 2000/5000 (fewer steps/repeats at the
    // top so the suite stays runnable; per-step stats are what matter).
    const std::tuple<int, int, int, int> scales[] = {
        {100, 100, 300, 3}, {300, 150, 300, 3}, {600, 300, 300, 3},
        {1000, 500, 300, 3}, {2000, 1000, 150, 2}, {5000, 2500, 60, 2}};
    for (const auto& [agents, foods, steps, repeats] : scales)
    {
        BenchmarkScenario s;
        s.name = "scale_" + std::to_string(agents);
        s.agents = agents;
        s.foods = foods;
        s.steps = steps;
        s.repeats = repeats;
        out.push_back(s);
    }
    for (const char* vm : {"single", "raycast", "sector"})
    {
        BenchmarkScenario s;
        s.name = std::string("vision_") + vm;
        s.agents = 600;
        s.foods = 300;
        s.visionMode = vm;
        out.push_back(s);
    }
    for (const char* nt : {"mlp", "simple_rnn", "neat"})
    {
        BenchmarkScenario s;
        s.name = std::string("neural_") + nt;
        s.agents = 600;
        s.foods = 300;
        s.neuralType = nt;
        out.push_back(s);
    }
    return out;
}

BenchmarkResult runScenario(const BenchmarkScenario& sc)
{
    using clock = std::chrono::high_resolution_clock;
    BenchmarkResult result;
    result.scenario = sc;
    const int repeats = std::max(1, sc.repeats);
    const int steps = std::max(1, sc.steps);

    std::vector<double> repeatStepUs;
    repeatStepUs.reserve(static_cast<std::size_t>(repeats));

    for (int rep = 0; rep < repeats; ++rep)
    {
        config::ParameterRegistry reg = config::createDefaultParameterRegistry();
        static_cast<void>(reg.setValue("random_seed", sc.seed));
        static_cast<void>(reg.setValue("bacteria_count", sc.agents));
        static_cast<void>(reg.setValue("food_target", sc.foods));
        static_cast<void>(reg.setValue("neural_network_type", std::string(sc.neuralType)));
        static_cast<void>(reg.setValue("retina_vision_mode", std::string(sc.visionMode)));
        static_cast<void>(reg.setValue("predators_enabled", sc.predatorsEnabled));
        static_cast<void>(reg.setValue("auto_export_substrate", false));
        // The runner enables the profiler from this knob each step; metrics stay
        // off (their cost is not part of the benchmark).
        static_cast<void>(reg.setValue("profiler_enabled", true));
        static_cast<void>(reg.setValue("metrics_enabled", false));

        sim::SimulationRunner runner(reg);
        runner.initialize();
        for (int i = 0; i < std::max(0, sc.warmupSteps); ++i) runner.step(1.0 / 30.0);

        // Discard warm-up samples; measured steps below accumulate the profile.
        runner.profilerMutable().reset();
        const auto start = clock::now();
        for (int i = 0; i < steps; ++i) runner.step(1.0 / 30.0);
        const auto end = clock::now();

        const double totalUs = std::chrono::duration<double, std::micro>(end - start).count();
        repeatStepUs.push_back(totalUs / static_cast<double>(steps));

        if (rep == repeats - 1)
        {
            const core::Profiler& prof = runner.profiler();
            for (int s = 0; s <= static_cast<int>(core::ProfileSection::SpatialHash); ++s)
            {
                const auto section = static_cast<core::ProfileSection>(s);
                result.sections.push_back(
                    {core::profileSectionName(section), prof.averageUs(section), prof.percentOfStep(section)});
            }
            const double simStepNs =
                static_cast<double>(prof.accumulatedNs(core::ProfileSection::SimStep));
            const double overheadNs = static_cast<double>(prof.overheadAccumNs());
            const double overheadPct = simStepNs > 0.0 ? overheadNs / simStepNs * 100.0 : 0.0;
            const double overheadUs = prof.stepCount() > 0
                ? (overheadNs / 1000.0) / static_cast<double>(prof.stepCount())
                : 0.0;
            result.sections.push_back({"overhead", overheadUs, overheadPct});
            result.finalAgents = runner.agents().size();
            result.finalFoods = runner.foods().size();
            result.stepsMeasured = prof.stepCount();
        }
    }

    double sum = 0.0;
    double mn = repeatStepUs.front();
    double mx = repeatStepUs.front();
    for (const double v : repeatStepUs) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    const double mean = sum / static_cast<double>(repeatStepUs.size());
    double variance = 0.0;
    for (const double v : repeatStepUs) variance += (v - mean) * (v - mean);
    variance /= static_cast<double>(repeatStepUs.size());

    result.meanStepUs = mean;
    result.minStepUs = mn;
    result.maxStepUs = mx;
    result.stdStepUs = std::sqrt(variance);
    result.stepsPerSecond = mean > 0.0 ? 1.0e6 / mean : 0.0;
    return result;
}

std::vector<BenchmarkResult> runSuite(const std::vector<BenchmarkScenario>& scenarios)
{
    std::vector<BenchmarkResult> results;
    results.reserve(scenarios.size());
    for (const auto& s : scenarios) results.push_back(runScenario(s));
    return results;
}

std::string toCsv(const std::vector<BenchmarkResult>& results, const BenchmarkMeta& meta)
{
    std::ostringstream out;
    out << "# commit=" << meta.commit << " build=" << meta.buildMode << " version=" << meta.version
        << " date=" << meta.dateTime << "\n";
    out << "scenario,agents,foods,neural,vision,seed,repeats,steps,mean_step_us,min_us,max_us,"
           "std_us,steps_per_sec,final_agents,final_foods";
    for (int s = 0; s <= static_cast<int>(core::ProfileSection::SpatialHash); ++s)
    {
        out << ',' << core::profileSectionName(static_cast<core::ProfileSection>(s)) << "_pct";
    }
    out << ",overhead_pct\n";
    for (const auto& r : results)
    {
        out << r.scenario.name << ',' << r.scenario.agents << ',' << r.scenario.foods << ','
            << r.scenario.neuralType << ',' << r.scenario.visionMode << ',' << r.scenario.seed << ','
            << r.scenario.repeats << ',' << r.scenario.steps << ',' << num(r.meanStepUs) << ','
            << num(r.minStepUs) << ',' << num(r.maxStepUs) << ',' << num(r.stdStepUs) << ','
            << num(r.stepsPerSecond, 1) << ',' << r.finalAgents << ',' << r.finalFoods;
        for (const auto& sec : r.sections) out << ',' << num(sec.percentOfStep, 2);
        out << '\n';
    }
    return out.str();
}

std::string toJson(const std::vector<BenchmarkResult>& results, const BenchmarkMeta& meta)
{
    io::Json root = io::Json::makeObject();
    io::Json m = io::Json::makeObject();
    m.set("commit", io::Json(meta.commit));
    m.set("build", io::Json(meta.buildMode));
    m.set("version", io::Json(meta.version));
    m.set("date", io::Json(meta.dateTime));
    root.set("meta", std::move(m));

    io::Json arr = io::Json::makeArray();
    for (const auto& r : results)
    {
        io::Json o = io::Json::makeObject();
        o.set("scenario", io::Json(r.scenario.name));
        o.set("agents", io::Json(r.scenario.agents));
        o.set("foods", io::Json(r.scenario.foods));
        o.set("neural", io::Json(r.scenario.neuralType));
        o.set("vision", io::Json(r.scenario.visionMode));
        o.set("seed", io::Json(r.scenario.seed));
        o.set("repeats", io::Json(r.scenario.repeats));
        o.set("steps", io::Json(r.scenario.steps));
        o.set("mean_step_us", io::Json(r.meanStepUs));
        o.set("min_us", io::Json(r.minStepUs));
        o.set("max_us", io::Json(r.maxStepUs));
        o.set("std_us", io::Json(r.stdStepUs));
        o.set("steps_per_sec", io::Json(r.stepsPerSecond));
        o.set("final_agents", io::Json(static_cast<std::uint64_t>(r.finalAgents)));
        o.set("final_foods", io::Json(static_cast<std::uint64_t>(r.finalFoods)));
        io::Json secs = io::Json::makeArray();
        for (const auto& sec : r.sections)
        {
            io::Json so = io::Json::makeObject();
            so.set("name", io::Json(sec.name));
            so.set("avg_us_per_step", io::Json(sec.avgUsPerStep));
            so.set("pct_of_step", io::Json(sec.percentOfStep));
            secs.push(std::move(so));
        }
        o.set("sections", std::move(secs));
        arr.push(std::move(o));
    }
    root.set("results", std::move(arr));
    return root.dump(true);
}

std::string toMarkdown(const std::vector<BenchmarkResult>& results, const BenchmarkMeta& meta)
{
    std::ostringstream out;
    out << "# Benchmark AgentBioSim (headless)\n\n";
    out << "- Commit: `" << meta.commit << "`  Build: **" << meta.buildMode << "**  Versao: "
        << meta.version << "\n";
    out << "- Data: " << meta.dateTime << "\n";
    out << "- Modo: headless (sem janela / render / ImGui); profiler por sistema (Fase 27).\n\n";

    out << "## Resultados\n\n";
    out << "| cenario | agentes | neural | visao | us/passo (media) | min | max | desvio | passos/s |\n";
    out << "|---|---:|---|---|---:|---:|---:|---:|---:|\n";
    for (const auto& r : results)
    {
        out << "| " << r.scenario.name << " | " << r.scenario.agents << " | " << r.scenario.neuralType
            << " | " << r.scenario.visionMode << " | " << num(r.meanStepUs) << " | " << num(r.minStepUs)
            << " | " << num(r.maxStepUs) << " | " << num(r.stdStepUs) << " | "
            << num(r.stepsPerSecond, 1) << " |\n";
    }

    out << "\n## Percentual por sistema (do profiler)\n\n";
    for (const auto& r : results)
    {
        out << "### " << r.scenario.name << "\n\n";
        out << "| sistema | us/passo | % do passo |\n|---|---:|---:|\n";
        for (const auto& sec : r.sections)
        {
            out << "| " << sec.name << " | " << num(sec.avgUsPerStep) << " | "
                << num(sec.percentOfStep, 2) << " |\n";
        }
        out << '\n';
    }
    return out.str();
}

std::string writeReports(const std::vector<BenchmarkResult>& results, const BenchmarkMeta& meta)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories("benchmarks/results", ec);
    fs::create_directories("benchmarks/reports", ec);

    const std::string stamp = nowStamp(true);
    const std::string base = "agentbiosim_" + stamp + "_" + meta.buildMode;

    const auto write = [](const std::string& path, const std::string& text) -> bool {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        return static_cast<bool>(f);
    };

    bool ok = write("benchmarks/results/" + base + ".csv", toCsv(results, meta));
    ok = write("benchmarks/results/" + base + ".json", toJson(results, meta)) && ok;
    ok = write("benchmarks/reports/" + base + ".md", toMarkdown(results, meta)) && ok;
    return ok ? base : std::string();
}

Phase29ValidationSummary runPhase29Validation()
{
    Phase29ValidationSummary summary;
    std::ostringstream log;
    auto check = [&](const bool cond, const std::string& name) {
        ++summary.checks;
        if (cond) { log << "  ok: " << name << '\n'; }
        else { summary.passed = false; log << "  FAIL: " << name << '\n'; }
    };

    // A small, fast scenario for validation.
    BenchmarkScenario sc;
    sc.name = "selftest";
    sc.agents = 120;
    sc.foods = 80;
    sc.steps = 40;
    sc.warmupSteps = 5;
    sc.repeats = 2;
    sc.seed = 4242;

    const BenchmarkResult result = runScenario(sc);
    check(result.meanStepUs > 0.0, "measured a positive step time");
    check(!result.sections.empty(), "profiler produced per-system sections");
    check(result.stepsMeasured == static_cast<std::uint64_t>(sc.steps), "step count matches");

    double pctSum = 0.0;
    for (const auto& sec : result.sections) pctSum += sec.percentOfStep;
    check(pctSum >= 90.0 && pctSum <= 115.0,
          "per-system percentages sum to ~100% (" + num(pctSum, 1) + ")");

    // Reproducibility: same seed -> same simulation outcome (entity counts).
    BenchmarkScenario repro = sc;
    repro.repeats = 1;
    const BenchmarkResult a = runScenario(repro);
    const BenchmarkResult b = runScenario(repro);
    check(a.finalAgents == b.finalAgents && a.finalFoods == b.finalFoods,
          "reproducible: same seed -> same final counts");

    // Reports are well-formed.
    const BenchmarkMeta meta = currentMeta();
    const std::vector<BenchmarkResult> one{result};
    const std::string csv = toCsv(one, meta);
    const std::string json = toJson(one, meta);
    const std::string md = toMarkdown(one, meta);
    check(csv.find("scenario,agents") != std::string::npos, "CSV has a header row");
    check(md.find('|') != std::string::npos, "Markdown has a table");
    io::Json parsed;
    std::string parseErr;
    const bool jsonOk = io::Json::parse(json, parsed, parseErr) && parsed.isObject() &&
                        parsed.find("results") != nullptr;
    check(jsonOk, "JSON report is well-formed");

    summary.details = log.str();
    return summary;
}
} // namespace agentbiosim::bench
