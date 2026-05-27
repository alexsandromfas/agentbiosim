#include "app/App.hpp"
#include "config/ParameterDefaults.hpp"
#include "simulation/SpatialHash.hpp"

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

        agentbiosim::App app;
        return app.run();
    }
    catch (const std::exception& exc)
    {
        std::cerr << "AgentBioSimCpp failed: " << exc.what() << '\n';
        return 1;
    }
}
