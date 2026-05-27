#pragma once

#include "config/ParameterRegistry.hpp"

namespace agentbiosim::config
{
ParameterRegistry createDefaultParameterRegistry();
void registerDefaultParameters(ParameterRegistry& registry);
} // namespace agentbiosim::config
