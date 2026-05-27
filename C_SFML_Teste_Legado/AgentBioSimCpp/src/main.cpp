#include "app/App.hpp"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        agentbiosim::App app;
        return app.run();
    }
    catch (const std::exception& exc)
    {
        std::cerr << "AgentBioSimCpp failed: " << exc.what() << '\n';
        return 1;
    }
}
