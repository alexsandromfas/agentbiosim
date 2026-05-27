#include "app/App.hpp"
#include "config/ParameterDefaults.hpp"

#include <exception>
#include <iostream>
#include <string>

int main(const int argc, char* argv[])
{
    try
    {
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--dump-params")
            {
                const auto registry = agentbiosim::config::createDefaultParameterRegistry();
                registry.dump(std::cout);
                return 0;
            }
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
