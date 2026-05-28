#pragma once

#include "config/Parameter.hpp"
#include "config/ParameterRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <variant>

namespace agentbiosim::config
{
inline double parameterDouble(const ParameterRegistry& registry, const std::string& name, const double fallback)
{
    const ParameterDefinition* definition = registry.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<double>(&definition->defaultValue))
    {
        return *value;
    }
    if (const auto* value = std::get_if<int>(&definition->defaultValue))
    {
        return static_cast<double>(*value);
    }
    return fallback;
}

inline int parameterInt(const ParameterRegistry& registry, const std::string& name, const int fallback)
{
    const ParameterDefinition* definition = registry.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<int>(&definition->defaultValue))
    {
        return *value;
    }
    if (const auto* value = std::get_if<double>(&definition->defaultValue))
    {
        return static_cast<int>(*value);
    }
    return fallback;
}

inline bool parameterBool(const ParameterRegistry& registry, const std::string& name, const bool fallback)
{
    const ParameterDefinition* definition = registry.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<bool>(&definition->defaultValue))
    {
        return *value;
    }
    return fallback;
}

inline std::string parameterString(const ParameterRegistry& registry, const std::string& name,
                                   const std::string& fallback)
{
    const ParameterDefinition* definition = registry.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<std::string>(&definition->defaultValue))
    {
        return *value;
    }
    return fallback;
}

inline ColorRgb parameterColor(const ParameterRegistry& registry, const std::string& name,
                               const ColorRgb fallback)
{
    const ParameterDefinition* definition = registry.find(name);
    if (definition == nullptr)
    {
        return fallback;
    }
    if (const auto* value = std::get_if<ColorRgb>(&definition->defaultValue))
    {
        return *value;
    }
    return fallback;
}

inline std::string normalizedParameterText(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](const unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        const char lower = static_cast<char>(std::tolower(ch));
        return lower == '-' || lower == ' ' ? '_' : lower;
    });
    return value;
}
} // namespace agentbiosim::config
