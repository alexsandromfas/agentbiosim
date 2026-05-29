#include "app/App.hpp"
#include "config/ParameterDefaults.hpp"
#include "neural/Phase9Diagnostics.hpp"
#include "perception/Phase10Diagnostics.hpp"
#include "perception/Phase11Diagnostics.hpp"
#include "perception/Phase12Diagnostics.hpp"
#include "neural/Phase14Diagnostics.hpp"
#include "neural/Phase15Diagnostics.hpp"
#include "systems/Phase13Diagnostics.hpp"
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
        bool runPhase12Validation = false;
        bool runPhase12Benchmark = false;
        bool runPhase13Validation = false;
        bool runPhase13Benchmark = false;
        bool runPhase14Validation = false;
        bool runPhase14Benchmark = false;
        bool runPhase15Validation = false;
        bool runPhase15Benchmark = false;

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
            else if (argument == "--phase12-selftest")
            {
                runPhase12Validation = true;
            }
            else if (argument == "--phase12-benchmark")
            {
                runPhase12Benchmark = true;
            }
            else if (argument == "--phase12-diagnostics")
            {
                runPhase12Validation = true;
                runPhase12Benchmark = true;
            }
            else if (argument == "--phase13-selftest")
            {
                runPhase13Validation = true;
            }
            else if (argument == "--phase13-benchmark")
            {
                runPhase13Benchmark = true;
            }
            else if (argument == "--phase13-diagnostics")
            {
                runPhase13Validation = true;
                runPhase13Benchmark = true;
            }
            else if (argument == "--phase14-selftest")
            {
                runPhase14Validation = true;
            }
            else if (argument == "--phase14-benchmark")
            {
                runPhase14Benchmark = true;
            }
            else if (argument == "--phase14-diagnostics")
            {
                runPhase14Validation = true;
                runPhase14Benchmark = true;
            }
            else if (argument == "--phase15-selftest")
            {
                runPhase15Validation = true;
            }
            else if (argument == "--phase15-benchmark")
            {
                runPhase15Benchmark = true;
            }
            else if (argument == "--phase15-diagnostics")
            {
                runPhase15Validation = true;
                runPhase15Benchmark = true;
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

        if (runPhase12Validation)
        {
            const auto summary = agentbiosim::perception::runPhase12Validation();
            std::cout << "Phase12 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 8;
            }
        }

        if (runPhase12Benchmark)
        {
            const auto rows = agentbiosim::perception::runPhase12Microbenchmark();
            std::cout << "Phase12 perception microbenchmark\n";
            std::cout << "scenario,vision_mode,bins_mode,distribution,falloff,projection,agents,retina_count,eye_count,channels,input_size,subdivisions,cand_limit,repeats,total_ms,avg_perception_us,avg_candidates,avg_cand_after_limit,avg_hits,spatial,debug,auto_sector\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.visionMode << ','
                          << row.binsMode << ','
                          << row.binsDistribution << ','
                          << row.binsFalloff << ','
                          << row.binsProjection << ','
                          << row.agents << ','
                          << row.retinaCount << ','
                          << row.eyeCount << ','
                          << row.channelCount << ','
                          << row.inputSize << ','
                          << row.subdivisions << ','
                          << row.candidateLimit << ','
                          << row.repeats << ','
                          << row.totalMilliseconds << ','
                          << row.averagePerceptionMicroseconds << ','
                          << row.averageCandidatesPerAgent << ','
                          << row.averageCandidatesAfterLimit << ','
                          << row.averageHitsPerAgent << ','
                          << (row.usedSpatialHash ? "true" : "false") << ','
                          << (row.debugActive ? "true" : "false") << ','
                          << (row.autoSectorActive ? "true" : "false") << '\n';
            }
        }

        if (runPhase12Validation || runPhase12Benchmark)
        {
            return 0;
        }

        if (runPhase13Validation)
        {
            const auto summary = agentbiosim::systems::runPhase13Validation();
            std::cout << "Phase13 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 9;
            }
        }

        if (runPhase13Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase13Microbenchmark();
            std::cout << "Phase13 reproduction microbenchmark\n";
            std::cout << "scenario,initial_agents,final_agents,total_births,steps,repeats,total_ms,avg_step_us,us_per_birth,mutation,hidden\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.initialAgents << ','
                          << row.finalAgents << ','
                          << row.totalBirths << ','
                          << row.steps << ','
                          << row.repeats << ','
                          << row.totalMilliseconds << ','
                          << row.averageStepMicroseconds << ','
                          << row.microsecondsPerBirth << ','
                          << (row.mutationEnabled ? "on" : "off") << ','
                          << row.hiddenLayers << '\n';
            }
        }

        if (runPhase13Validation || runPhase13Benchmark)
        {
            return 0;
        }

        if (runPhase14Validation)
        {
            const auto summary = agentbiosim::neural::runPhase14Validation();
            std::cout << "Phase14 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 10;
            }
        }

        if (runPhase14Benchmark)
        {
            const auto rows = agentbiosim::neural::runPhase14Microbenchmark();
            std::cout << "Phase14 dense advanced brain microbenchmark\n";
            std::cout << "scenario,brain_type,hidden,input_size,output_size,agents,repeats,total_ms,avg_forward_us,avg_clone_us,avg_mutation_us\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.brainType << ','
                          << row.hiddenLayers << ','
                          << row.inputSize << ','
                          << row.outputSize << ','
                          << row.agents << ','
                          << row.repeats << ','
                          << row.totalMilliseconds << ','
                          << row.averageForwardMicroseconds << ','
                          << row.averageCloneMicroseconds << ','
                          << row.averageMutationMicroseconds << '\n';
            }
        }

        if (runPhase14Validation || runPhase14Benchmark)
        {
            return 0;
        }

        if (runPhase15Validation)
        {
            const auto summary = agentbiosim::neural::runPhase15Validation();
            std::cout << "Phase15 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 11;
            }
        }

        if (runPhase15Benchmark)
        {
            const auto rows = agentbiosim::neural::runPhase15Microbenchmark();
            std::cout << "Phase15 RNN microbenchmark\n";
            std::cout << "scenario,brain_type,hidden,input_size,output_size,state_size,repeats,total_ms,avg_forward_us,avg_clone_us,avg_mutation_us,avg_reset_us,memory_decay,state_clip,reset_on_copy\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.brainType << ','
                          << row.hiddenLayers << ','
                          << row.inputSize << ','
                          << row.outputSize << ','
                          << row.stateSize << ','
                          << row.repeats << ','
                          << row.totalMilliseconds << ','
                          << row.averageForwardMicroseconds << ','
                          << row.averageCloneMicroseconds << ','
                          << row.averageMutationMicroseconds << ','
                          << row.averageResetMicroseconds << ','
                          << row.memoryDecay << ','
                          << row.stateClip << ','
                          << (row.resetStateOnCopy ? "true" : "false") << '\n';
            }
        }

        if (runPhase15Validation || runPhase15Benchmark)
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
