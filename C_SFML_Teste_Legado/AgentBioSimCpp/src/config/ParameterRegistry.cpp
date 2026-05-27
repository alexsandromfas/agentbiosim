#include "config/ParameterRegistry.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace agentbiosim::config
{
namespace
{
std::string doubleToString(const double value)
{
    std::ostringstream stream;
    stream << std::setprecision(10) << value;
    return stream.str();
}
} // namespace

std::string parameterTypeName(const ParameterType type)
{
    switch (type)
    {
    case ParameterType::Boolean:
        return "bool";
    case ParameterType::Integer:
        return "int";
    case ParameterType::Floating:
        return "float";
    case ParameterType::String:
        return "string";
    case ParameterType::ColorRgb:
        return "color_rgb";
    }

    return "unknown";
}

std::string parameterValueToString(const ParameterValue& value)
{
    return std::visit(
        [](const auto& typedValue) -> std::string
        {
            using ValueType = std::decay_t<decltype(typedValue)>;
            if constexpr (std::is_same_v<ValueType, bool>)
            {
                return typedValue ? "true" : "false";
            }
            else if constexpr (std::is_same_v<ValueType, int>)
            {
                return std::to_string(typedValue);
            }
            else if constexpr (std::is_same_v<ValueType, double>)
            {
                return doubleToString(typedValue);
            }
            else if constexpr (std::is_same_v<ValueType, std::string>)
            {
                return typedValue;
            }
            else if constexpr (std::is_same_v<ValueType, ColorRgb>)
            {
                return "(" + std::to_string(typedValue.r) + "," + std::to_string(typedValue.g) + "," +
                       std::to_string(typedValue.b) + ")";
            }
        },
        value);
}

std::string numericRangeToString(const NumericRange& range)
{
    if (!range.min && !range.max)
    {
        return "-";
    }

    const std::string minValue = range.min ? doubleToString(*range.min) : "-inf";
    const std::string maxValue = range.max ? doubleToString(*range.max) : "+inf";
    return "[" + minValue + ", " + maxValue + "]";
}

void ParameterRegistry::add(ParameterDefinition definition)
{
    if (definition.name.empty())
    {
        throw std::invalid_argument("Parameter name cannot be empty");
    }

    const auto registerName = [this](const std::string& name, const std::size_t index)
    {
        if (name.empty())
        {
            return;
        }
        const auto [_, inserted] = indexByName_.emplace(name, index);
        if (!inserted)
        {
            throw std::invalid_argument("Duplicate parameter name or alias: " + name);
        }
    };

    const std::size_t index = definitions_.size();
    registerName(definition.name, index);
    for (const auto& alias : definition.aliases)
    {
        registerName(alias, index);
    }
    definitions_.push_back(std::move(definition));
}

const ParameterDefinition* ParameterRegistry::find(const std::string& nameOrAlias) const
{
    const auto it = indexByName_.find(nameOrAlias);
    if (it == indexByName_.end())
    {
        return nullptr;
    }
    return &definitions_[it->second];
}

const std::vector<ParameterDefinition>& ParameterRegistry::definitions() const noexcept
{
    return definitions_;
}

std::size_t ParameterRegistry::size() const noexcept
{
    return definitions_.size();
}

void ParameterRegistry::dump(std::ostream& output) const
{
    output << "AgentBioSimCpp ParameterRegistry dump\n";
    output << "registered_parameters=" << definitions_.size() << "\n\n";

    for (const auto& definition : definitions_)
    {
        output << definition.name << "\n";
        output << "  type: " << parameterTypeName(definition.type) << "\n";
        output << "  default: " << parameterValueToString(definition.defaultValue) << "\n";
        output << "  category: " << definition.category << "\n";
        output << "  range: " << numericRangeToString(definition.range) << "\n";

        output << "  domains:";
        for (const auto& domain : definition.domains)
        {
            output << ' ' << domain;
        }
        output << "\n";

        if (!definition.aliases.empty())
        {
            output << "  aliases:";
            for (const auto& alias : definition.aliases)
            {
                output << ' ' << alias;
            }
            output << "\n";
        }

        output << "  description: " << definition.description << "\n\n";
    }
}
} // namespace agentbiosim::config
