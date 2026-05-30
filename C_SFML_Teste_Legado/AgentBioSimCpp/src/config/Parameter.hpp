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

// Phase 23: ColorRgb must be equality-comparable so std::variant<...,ColorRgb>
// can use operator==.
inline bool operator==(const ColorRgb& a, const ColorRgb& b) noexcept
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}
inline bool operator!=(const ColorRgb& a, const ColorRgb& b) noexcept { return !(a == b); }

using ParameterValue = std::variant<bool, int, double, std::string, ColorRgb>;

struct NumericRange
{
    std::optional<double> min;
    std::optional<double> max;
};

// Phase 23 hotfix: ParameterApplyFlags describe what must happen when a
// parameter is modified at runtime. Multiple flags may be OR-ed together; the
// AppController interprets them when draining preferences commands.
namespace ApplyFlag
{
constexpr unsigned int Immediate          = 0x0001U; // applied next frame
constexpr unsigned int RequiresReset      = 0x0002U; // SimulationRunner::reset
constexpr unsigned int RebuildPerception  = 0x0004U; // PerceptionSystem reconfig
constexpr unsigned int RebuildBrains      = 0x0008U; // new brains for new agents
constexpr unsigned int RefreshRenderer    = 0x0010U; // App::configureRenderOptions
constexpr unsigned int PendingFuturePhase = 0x0020U; // backend not yet implemented
}

struct ParameterDefinition
{
    std::string name;
    ParameterType type = ParameterType::String;
    ParameterValue defaultValue;       // current value (mutated via setValue)
    std::string category;
    std::string description;
    std::vector<std::string> aliases;
    NumericRange range;
    std::vector<std::string> domains;
    // Phase 23: fields appended at the end so existing aggregate
    // initializers in ParameterDefaults.cpp keep compiling without changes.
    ParameterValue originalDefault;    // populated by ParameterRegistry::add
    unsigned int applyFlags = ApplyFlag::Immediate;
};

std::string parameterTypeName(ParameterType type);
std::string parameterValueToString(const ParameterValue& value);
std::string numericRangeToString(const NumericRange& range);
} // namespace agentbiosim::config
