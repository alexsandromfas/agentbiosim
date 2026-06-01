#include "app/App.hpp"
#include "config/ParameterDefaults.hpp"
#include "neural/Phase9Diagnostics.hpp"
#include "perception/Phase10Diagnostics.hpp"
#include "perception/Phase11Diagnostics.hpp"
#include "perception/Phase12Diagnostics.hpp"
#include "neural/Phase14Diagnostics.hpp"
#include "neural/Phase15Diagnostics.hpp"
#include "neural/Phase16Diagnostics.hpp"
#include "simulation/Phase17Diagnostics.hpp"
#include "systems/Phase18Diagnostics.hpp"
#include "systems/Phase19Diagnostics.hpp"
#include "systems/Phase20Diagnostics.hpp"
#include "systems/Phase21Diagnostics.hpp"
#include "systems/Phase22Diagnostics.hpp"
#include "systems/Phase22_1Diagnostics.hpp"
#include "systems/Phase23Diagnostics.hpp"
#include "systems/Phase23_1Diagnostics.hpp"
#include "systems/Phase23_2Diagnostics.hpp"
#include "systems/Phase24Diagnostics.hpp"
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
        bool runPhase16Validation = false;
        bool runPhase16Benchmark = false;
        bool runPhase17Validation = false;
        bool runPhase17Benchmark = false;
        bool runPhase18Validation = false;
        bool runPhase18Benchmark = false;
        bool runPhase19Validation = false;
        bool runPhase19Benchmark = false;
        bool runPhase20Validation = false;
        bool runPhase20Benchmark = false;
        bool runPhase21Validation = false;
        bool runPhase21Benchmark = false;
        bool runPhase22Validation = false;
        bool runPhase22Benchmark = false;
        bool runPhase22_1Validation = false;
        bool runPhase22_1Benchmark = false;
        bool runPhase23Validation = false;
        bool runPhase23Benchmark = false;
        bool runPhase23_1Validation = false;
        bool runPhase23_1Benchmark = false;
        bool runPhase23_2Validation = false;
        bool runPhase24Validation = false;

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
            else if (argument == "--phase16-selftest")
            {
                runPhase16Validation = true;
            }
            else if (argument == "--phase16-benchmark")
            {
                runPhase16Benchmark = true;
            }
            else if (argument == "--phase16-diagnostics")
            {
                runPhase16Validation = true;
                runPhase16Benchmark = true;
            }
            else if (argument == "--phase17-selftest")
            {
                runPhase17Validation = true;
            }
            else if (argument == "--phase17-benchmark")
            {
                runPhase17Benchmark = true;
            }
            else if (argument == "--phase17-diagnostics")
            {
                runPhase17Validation = true;
                runPhase17Benchmark = true;
            }
            else if (argument == "--phase18-selftest")
            {
                runPhase18Validation = true;
            }
            else if (argument == "--phase18-benchmark")
            {
                runPhase18Benchmark = true;
            }
            else if (argument == "--phase18-diagnostics")
            {
                runPhase18Validation = true;
                runPhase18Benchmark = true;
            }
            else if (argument == "--phase19-selftest")
            {
                runPhase19Validation = true;
            }
            else if (argument == "--phase19-benchmark")
            {
                runPhase19Benchmark = true;
            }
            else if (argument == "--phase19-diagnostics")
            {
                runPhase19Validation = true;
                runPhase19Benchmark = true;
            }
            else if (argument == "--phase20-selftest")
            {
                runPhase20Validation = true;
            }
            else if (argument == "--phase20-benchmark")
            {
                runPhase20Benchmark = true;
            }
            else if (argument == "--phase20-diagnostics")
            {
                runPhase20Validation = true;
                runPhase20Benchmark = true;
            }
            else if (argument == "--phase21-selftest")
            {
                runPhase21Validation = true;
            }
            else if (argument == "--phase21-benchmark")
            {
                runPhase21Benchmark = true;
            }
            else if (argument == "--phase21-diagnostics")
            {
                runPhase21Validation = true;
                runPhase21Benchmark = true;
            }
            else if (argument == "--phase22-selftest")
            {
                runPhase22Validation = true;
            }
            else if (argument == "--phase22-benchmark")
            {
                runPhase22Benchmark = true;
            }
            else if (argument == "--phase22-diagnostics")
            {
                runPhase22Validation = true;
                runPhase22Benchmark = true;
            }
            else if (argument == "--phase22-hotfix-selftest")
            {
                runPhase22_1Validation = true;
            }
            else if (argument == "--phase22-hotfix-benchmark")
            {
                runPhase22_1Benchmark = true;
            }
            else if (argument == "--phase22-hotfix-diagnostics")
            {
                runPhase22_1Validation = true;
                runPhase22_1Benchmark = true;
            }
            else if (argument == "--phase23-selftest")
            {
                runPhase23Validation = true;
            }
            else if (argument == "--phase23-benchmark")
            {
                runPhase23Benchmark = true;
            }
            else if (argument == "--phase23-diagnostics")
            {
                runPhase23Validation = true;
                runPhase23Benchmark = true;
            }
            else if (argument == "--phase23-hotfix-selftest")
            {
                runPhase23_1Validation = true;
            }
            else if (argument == "--phase23-hotfix-benchmark")
            {
                runPhase23_1Benchmark = true;
            }
            else if (argument == "--phase23-hotfix-diagnostics")
            {
                runPhase23_1Validation = true;
                runPhase23_1Benchmark = true;
            }
            else if (argument == "--phase23-hotfix2-selftest")
            {
                runPhase23_2Validation = true;
            }
            else if (argument == "--phase24-selftest")
            {
                runPhase24Validation = true;
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

        if (runPhase16Validation)
        {
            const auto summary = agentbiosim::neural::runPhase16Validation();
            std::cout << "Phase16 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 12;
            }
        }

        if (runPhase16Benchmark)
        {
            const auto rows = agentbiosim::neural::runPhase16Microbenchmark();
            std::cout << "Phase16 NEAT family microbenchmark\n";
            std::cout << "scenario,brain_type,topology,input_size,output_size,hidden,connections,enabled,recurrent,repeats,total_ms,avg_forward_us,avg_clone_us,avg_mutation_us,avg_reset_us,memory_decay,state_clip,reset_on_copy\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.brainType << ','
                          << row.topology << ','
                          << row.inputSize << ','
                          << row.outputSize << ','
                          << row.hiddenNodes << ','
                          << row.connections << ','
                          << row.enabledConnections << ','
                          << row.recurrentConnections << ','
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

        if (runPhase16Validation || runPhase16Benchmark)
        {
            return 0;
        }

        if (runPhase17Validation)
        {
            const auto summary = agentbiosim::simulation::runPhase17Validation();
            std::cout << "Phase17 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 13;
            }
        }

        if (runPhase17Benchmark)
        {
            const auto rows = agentbiosim::simulation::runPhase17Microbenchmark();
            std::cout << "Phase17 species/labels/genomes microbenchmark\n";
            std::cout << "scenario,species,agents,brain_type,repeats,total_ms,avg_op_us,avg_per_agent_us,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.speciesCount << ','
                          << row.agents << ','
                          << row.brainType << ','
                          << row.repeats << ','
                          << row.totalMilliseconds << ','
                          << row.averageOperationMicroseconds << ','
                          << row.averagePerAgentMicroseconds << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase17Validation || runPhase17Benchmark)
        {
            return 0;
        }

        if (runPhase18Validation)
        {
            const auto summary = agentbiosim::systems::runPhase18Validation();
            std::cout << "Phase18 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 14;
            }
        }

        if (runPhase18Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase18Microbenchmark();
            std::cout << "Phase18 predation/diet microbenchmark\n";
            std::cout << "scenario,agents,predators,foods,steps,total_ms,avg_step_us,avg_per_agent_us,foods_consumed,predation_events,corpses_to_food,energy_food,energy_pred,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.agents << ','
                          << row.predators << ','
                          << row.foods << ','
                          << row.steps << ','
                          << row.totalMilliseconds << ','
                          << row.averageStepMicroseconds << ','
                          << row.averagePerAgentMicroseconds << ','
                          << row.foodsConsumed << ','
                          << row.predationEvents << ','
                          << row.corpsesToFoodSpawned << ','
                          << row.energyGainedByFood << ','
                          << row.energyGainedByPredation << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase18Validation || runPhase18Benchmark)
        {
            return 0;
        }

        if (runPhase19Validation)
        {
            const auto summary = agentbiosim::systems::runPhase19Validation();
            std::cout << "Phase19 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 15;
            }
        }

        if (runPhase19Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase19Microbenchmark();
            std::cout << "Phase19 chunk food microbenchmark\n";
            std::cout << "scenario,food_mode,replenish,agents,foods,predators,steps,total_ms,avg_step_us,avg_per_agent_us,foods_consumed,chunk_bites,chunk_depleted,clusters_created,clusters_grown,particles_grown,trimmed,predation_events,energy_food,bite_seconds,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.foodMode << ','
                          << row.replenishMode << ','
                          << row.agents << ','
                          << row.foods << ','
                          << row.predators << ','
                          << row.steps << ','
                          << row.totalMilliseconds << ','
                          << row.averageStepMicroseconds << ','
                          << row.averagePerAgentMicroseconds << ','
                          << row.foodsConsumed << ','
                          << row.chunkBitesApplied << ','
                          << row.chunkParticlesDepleted << ','
                          << row.clustersCreated << ','
                          << row.clustersGrown << ','
                          << row.particlesGrown << ','
                          << row.trimmed << ','
                          << row.predationEvents << ','
                          << row.energyGainedByFood << ','
                          << row.biteSeconds << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase19Validation || runPhase19Benchmark)
        {
            return 0;
        }

        if (runPhase20Validation)
        {
            const auto summary = agentbiosim::systems::runPhase20Validation();
            std::cout << "Phase20 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 16;
            }
        }

        if (runPhase20Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase20Microbenchmark();
            std::cout << "Phase20 obstacles/occlusion microbenchmark\n";
            std::cout << "scenario,agents,foods,obstacles,steps,vision,retina,eyes,channels,see_obs,walls_through,total_ms,avg_step_us,avg_per_agent_us,obstacle_blocks,occlusion_checks,occluded,obstacle_candidates,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.agents << ','
                          << row.foods << ','
                          << row.obstacles << ','
                          << row.steps << ','
                          << row.visionMode << ','
                          << row.retinaCount << ','
                          << row.eyeCount << ','
                          << row.channels << ','
                          << (row.seeObstacles ? "1" : "0") << ','
                          << (row.seeThroughWalls ? "1" : "0") << ','
                          << row.totalMilliseconds << ','
                          << row.averageStepMicroseconds << ','
                          << row.averagePerAgentMicroseconds << ','
                          << row.obstacleBlocks << ','
                          << row.occlusionChecks << ','
                          << row.occludedCandidates << ','
                          << row.obstacleCandidates << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase20Validation || runPhase20Benchmark)
        {
            return 0;
        }

        if (runPhase21Validation)
        {
            const auto summary = agentbiosim::systems::runPhase21Validation();
            std::cout << "Phase21 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 17;
            }
        }

        if (runPhase21Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase21Microbenchmark();
            std::cout << "Phase21 collisions/optional physics microbenchmark\n";
            std::cout << "scenario,agents,foods,obstacles,steps,coll,elast,visc,brown,chunk_mov,chunk_ff,chunk_adh,total_ms,avg_step_us,avg_per_agent_us,pairs_aa,coll_aa,pairs_af,pushes,pairs_ff,coll_ff,adhesions,brownian,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.agents << ','
                          << row.foods << ','
                          << row.obstacles << ','
                          << row.steps << ','
                          << (row.collisionEnabled ? "1" : "0") << ','
                          << (row.elasticityEnabled ? "1" : "0") << ','
                          << (row.viscosityEnabled ? "1" : "0") << ','
                          << (row.brownianEnabled ? "1" : "0") << ','
                          << (row.movableChunkEnabled ? "1" : "0") << ','
                          << (row.chunkCollisionEnabled ? "1" : "0") << ','
                          << (row.chunkAdhesionEnabled ? "1" : "0") << ','
                          << row.totalMilliseconds << ','
                          << row.averageStepMicroseconds << ','
                          << row.averagePerAgentMicroseconds << ','
                          << row.agentPairsTested << ','
                          << row.agentCollisionsResolved << ','
                          << row.agentFoodPairsTested << ','
                          << row.foodPushes << ','
                          << row.foodPairsTested << ','
                          << row.foodCollisionsResolved << ','
                          << row.adhesionsApplied << ','
                          << row.brownianApplied << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase21Validation || runPhase21Benchmark)
        {
            return 0;
        }

        if (runPhase22Validation)
        {
            const auto summary = agentbiosim::systems::runPhase22Validation();
            std::cout << "Phase22 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 18;
            }
        }

        if (runPhase22Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase22Microbenchmark();
            std::cout << "Phase22 UI base microbenchmark\n";
            std::cout << "scenario,agents,foods,obstacles,steps,total_ms,avg_step_us,commands,selection,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.agents << ','
                          << row.foods << ','
                          << row.obstacles << ','
                          << row.steps << ','
                          << row.totalMilliseconds << ','
                          << row.averageStepMicroseconds << ','
                          << row.commandsApplied << ','
                          << row.selectionSize << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase22Validation || runPhase22Benchmark)
        {
            return 0;
        }

        if (runPhase22_1Validation)
        {
            const auto summary = agentbiosim::systems::runPhase22_1Validation();
            std::cout << "Phase22.1 hotfix validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 19;
            }
        }

        if (runPhase22_1Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase22_1Microbenchmark();
            std::cout << "Phase22.1 hotfix microbenchmark\n";
            std::cout << "scenario,agents,foods,obstacles,steps,total_ms,avg_step_us,commands,selection,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.agents << ','
                          << row.foods << ','
                          << row.obstacles << ','
                          << row.steps << ','
                          << row.totalMilliseconds << ','
                          << row.averageStepMicroseconds << ','
                          << row.commandsApplied << ','
                          << row.selectionSize << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase22_1Validation || runPhase22_1Benchmark)
        {
            return 0;
        }

        if (runPhase23Validation)
        {
            const auto summary = agentbiosim::systems::runPhase23Validation();
            std::cout << "Phase23 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 20;
            }
        }

        if (runPhase23Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase23Microbenchmark();
            std::cout << "Phase23 preferences microbenchmark\n";
            std::cout << "scenario,parameters,pending,applied,total_ms,avg_op_us,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.parameters << ','
                          << row.pending << ','
                          << row.applied << ','
                          << row.totalMilliseconds << ','
                          << row.averageOpMicroseconds << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase23Validation || runPhase23Benchmark)
        {
            return 0;
        }

        if (runPhase23_1Validation)
        {
            const auto summary = agentbiosim::systems::runPhase23_1Validation();
            std::cout << "Phase23.1 hotfix validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            if (!summary.passed)
            {
                return 21;
            }
        }

        if (runPhase23_1Benchmark)
        {
            const auto rows = agentbiosim::systems::runPhase23_1Microbenchmark();
            std::cout << "Phase23.1 hotfix microbenchmark\n";
            std::cout << "scenario,windows_open,popups,parameters_visible,total_ms,avg_op_us,notes\n";
            std::cout << std::fixed << std::setprecision(4);
            for (const auto& row : rows)
            {
                std::cout << row.scenario << ','
                          << row.windowsOpen << ','
                          << row.popups << ','
                          << row.parametersVisible << ','
                          << row.totalMilliseconds << ','
                          << row.averageOpMicroseconds << ','
                          << row.notes << '\n';
            }
        }

        if (runPhase23_1Validation || runPhase23_1Benchmark)
        {
            return 0;
        }

        if (runPhase23_2Validation)
        {
            const auto summary = agentbiosim::systems::runPhase23_2Validation();
            std::cout << "Phase23.2 hotfix validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            return summary.passed ? 0 : 22;
        }

        if (runPhase24Validation)
        {
            const auto summary = agentbiosim::systems::runPhase24Validation();
            std::cout << "Phase24 validation: " << (summary.passed ? "PASS" : "FAIL")
                      << " (" << summary.checks << " checks)\n"
                      << summary.details << '\n';
            return summary.passed ? 0 : 24;
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
