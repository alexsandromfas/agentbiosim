#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace agentbiosim::config
{
enum class ParameterType
{
    Boolean,
    Integer,
    Floating,
    String,
    ColorRgb
};

struct ColorRgb
{
    int r = 0;
    int g = 0;
    int b = 0;
};

using ParameterValue = std::variant<bool, int, double, std::string, ColorRgb>;

struct NumericRange
{
    std::optional<double> min;
    std::optional<double> max;
};

struct ParameterDefinition
{
    std::string name;
    ParameterType type = ParameterType::String;
    ParameterValue defaultValue;
    std::string category;
    std::string description;
    std::vector<std::string> aliases;
    NumericRange range;
    std::vector<std::string> domains;
};

std::string parameterTypeName(ParameterType type);
std::string parameterValueToString(const ParameterValue& value);
std::string numericRangeToString(const NumericRange& range);
} // namespace agentbiosim::config
