#include "app/App.hpp"
#include "config/ParameterDefaults.hpp"
#include "neural/Phase9Diagnostics.hpp"
#include "perception/Phase10Diagnostics.hpp"
#include "perception/Phase11Diagnostics.hpp"
#include "simulation/SpatialHash.hpp"
#include "systems/Phase7Diagnostics.hpp"
#include "systems/Phase8Diagnostics.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

int main(const int argc, char* argv[])
{
    try
    {
        bool runSpatialValidation = false;
        bool runSpatialBenchmark = false;
        bool runPhase7Validation = false;
        bool runPhase7Benchmark = false;
        bool runPhase8Validation = false;
        bool runPhase8Benchmark = false;
        bool runPhase9Validation = false;
        bool runPhase9Benchmark = false;
        bool runPhase10Validation = false;
        bool runPhase10Benchmark = false;
        bool runPhase11Validation = false;
        bool runPhase11Benchmark = false;

        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--dump-params")
            {
                const auto registry = agentbiosim::config::createDefaultParameterRegistry();
                registry.dump(std::cout);
                return 0;
            }
            if (argument == "--spatial-selftest")
            {
                runSpatialValidation = true;
            }
            else if (argument == "--spatial-benchmark")
            {
                runSpatialBenchmark = true;
            }
            else if (argument == "--spatial-diagnostics")
            {
                runSpatialValidation = true;
                runSpatialBenchmark = true;
            }
            else if (argument == "--phase7-selftest")
            {
                runPhase7Validation = true;
            }
            else if (argument == "--phase7-benchmark")
            {
                runPhase7Benchmark = true;
            }
            else if (argument == "--phase7-diagnostics")
            {
                runPhase7Validation = true;
                runPhase7Benchmark = true;
            }
            else if (argument == "--phase8-selftest")
            {
                runPhase8Validation = true;
            }
            else if (argument == "--phase8-benchmark")
            {
                runPhase8Benchmark = true;
            }
            else if (argument == "--phase8-diagnostics")
            {
                runPhase8Validation = true;
                runPhase8Benchmark = true;
            }
            else if (argument == "--phase9-selftest")
            {
                runPhase9Validation = true;
            }
            else if (argument == "--phase9-benchmark")
            {
                runPhase9Benchmark = true;
            }
            else if (argument == "--phase9-diagnostics")
            {
                runPhase9Validation = true;
                runPhase9Benchmark = true;
            }
            else if (argument == "--phase10-selftest")
            {
                runPhase10Validation = true;
            }
            else if (argument == "--phase10-benchmark")
            {
                runPhase10Benchmark = true;
            }
            else if (argument == "--phase10-diagnostics")
            {
                runPhase10Validation = true;
                runPhase10Benchmark = true;
            }
            else if (argument == "--phase11-selftest")
            {
                runPhase11Validation = true;
            }
            else if (argument == "--phase11-benchmark")
            {
                runPhase11Benchmark = true;
            }
            else if (argument == "--phase11-diagnostics")
            {
                runPhase11Validation = true;
                runPhase11Benchmark = true;
            }
        }

        if (runSpatialValidation)
        {
            const auto summary = agentbiosim::simulation::runSpatialHashValidation();
            std::cout << "SpatialHash validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 2;
            }
        }

        if (runSpatialBenchmark)
        {
            const auto rows = agentbiosim::simulation::runSpatialHashMicrobenchmark();
            std::cout << "SpatialHash microbenchmark\n";
            std::cout << "entities,queries,cell_size,rebuild_ms,query_ms,avg_query_us,avg_candidates\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.entities << ','
                          << row.queries << ','
                          << row.cellSize << ','
                          << row.rebuildMilliseconds << ','
                          << row.queryMilliseconds << ','
                          << row.averageQueryMicroseconds << ','
                          << row.averageCandidates << '\n';
            }
        }

        if (runSpatialValidation || runSpatialBenchmark)
        {
            return 0;
        }

        if (runPhase7Validation)
        {
            const auto summary = agentbiosim::systems::runPhase7Validation();
            std::cout << "Phase7 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 3;
            }
        }

        if (runPhase7Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase7Microbenchmark();
            std::cout << "Phase7 food/energy/interaction microbenchmark\n";
            std::cout << "agents,foods,use_spatial,step_ms,foods_consumed,deaths\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.agents << ','
                          << row.foods << ','
                          << (row.useSpatial ? "true" : "false") << ','
                          << row.stepMilliseconds << ','
                          << row.foodsConsumed << ','
                          << row.deaths << '\n';
            }
        }

        if (runPhase7Validation || runPhase7Benchmark)
        {
            return 0;
        }

        if (runPhase8Validation)
        {
            const auto summary = agentbiosim::systems::runPhase8Validation();
            std::cout << "Phase8 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 4;
            }
        }

        if (runPhase8Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase8Microbenchmark();
            std::cout << "Phase8 locomotion microbenchmark\n";
            std::cout << "agents,movement_mode,smooth_locomotion,step_ms,agents_processed,max_speed_observed,wall_collisions\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.agents << ','
                          << row.movementMode << ','
                          << (row.smoothLocomotion ? "true" : "false") << ','
                          << row.stepMilliseconds << ','
                          << row.agentsProcessed << ','
                          << row.maxSpeedObserved << ','
                          << row.wallCollisions << '\n';
            }
        }

        if (runPhase8Validation || runPhase8Benchmark)
        {
            return 0;
        }

        if (runPhase9Validation)
        {
            const auto summary = agentbiosim::neural::runPhase9Validation();
            std::cout << "Phase9 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 5;
            }
        }

        if (runPhase9Benchmark)
        {
            const auto rows = agentbiosim::neural::runPhase9Microbenchmark();
            std::cout << "Phase9 MLP forward microbenchmark\n";
            std::cout << "architecture,input_size,output_size,hidden_layers,agents,forwards,total_ms,avg_forward_us\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.architecture << ','
                          << row.inputSize << ','
                          << row.outputSize << ','
                          << row.hiddenLayers << ','
                          << row.agents << ','
                          << row.forwards << ','
                          << row.totalMilliseconds << ','
                          << row.averageForwardMicroseconds << '\n';
            }
        }

        if (runPhase9Validation || runPhase9Benchmark)
        {
            return 0;
        }

        if (runPhase10Validation)
        {
            const auto summary = agentbiosim::perception::runPhase10Validation();
            std::cout << "Phase10 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 6;
            }
        }

        if (runPhase10Benchmark)
        {
            const auto rows = agentbiosim::perception::runPhase10Microbenchmark();
            std::cout << "Phase10 perception microbenchmark\n";
            std::cout << "scenario,agents,retina_count,eye_count,channels,input_size,repeats,total_ms,avg_perception_us,avg_candidates,spatial\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.agents << ','
                          << row.retinaCount << ','
                          << row.eyeCount << ','
                          << row.channelCount << ','
                          << row.inputSize << ','
                          << row.repeats << ','
                          << row.totalMilliseconds << ','
                          << row.averagePerceptionMicroseconds << ','
                          << row.averageCandidatesPerAgent << ','
                          << (row.usedSpatialHash ? "true" : "false") << '\n';
            }
        }

        if (runPhase10Validation || runPhase10Benchmark)
        {
            return 0;
        }

        if (runPhase11Validation)
        {
            const auto summary = agentbiosim::perception::runPhase11Validation();
            std::cout << "Phase11 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 7;
            }
        }

        if (runPhase11Benchmark)
        {
            const auto rows = agentbiosim::perception::runPhase11Microbenchmark();
            std::cout << "Phase11 perception microbenchmark\n";
            std::cout << "scenario,vision_mode,agents,retina_count,eye_count,channels,input_size,repeats,total_ms,avg_perception_us,avg_candidates,avg_hits,spatial,debug\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.visionMode << ','
                          << row.agents << ','
                          << row.retinaCount << ','
                          << row.eyeCount << ','
                          << row.channelCount << ','
                          << row.inputSize << ','
                          << row.repeats << ','
                          << row.totalMilliseconds << ','
                          << row.averagePerceptionMicroseconds << ','
                          << row.averageCandidatesPerAgent << ','
                          << row.averageHitsPerAgent << ','
                          << (row.usedSpatialHash ? "true" : "false") << ','
                          << (row.debugActive ? "true" : "false") << '\n';
            }
        }

        if (runPhase11Validation || runPhase11Benchmark)
        {
            return 0;
        }

        agentbiosim::App app;
        return app.run();
    }
    catch (const std::exception& exc)
    {
        std::cerr << "AgentBioSimCpp failed: " << exc.what() << '\n';
        return 1;
    }
}
