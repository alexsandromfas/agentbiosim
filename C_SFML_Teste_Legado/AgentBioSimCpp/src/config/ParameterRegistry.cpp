#include "config/ParameterRegistry.hpp"

#include <algorithm>
#include <cctype>
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
    // Phase 23: capture immutable factory default for restoreDefault().
    definition.originalDefault = definition.defaultValue;
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

// Phase 23: mutate the stored value with validation. Numeric clamping uses
// the registered range. Enum parameters (Type::String with non-empty domains)
// only accept values that are listed. ColorRgb is clamped per channel into
// [0,255]. Bool is taken as-is.
bool ParameterRegistry::setValue(const std::string& nameOrAlias, const ParameterValue& value)
{
    const auto it = indexByName_.find(nameOrAlias);
    if (it == indexByName_.end()) return false;
    ParameterDefinition& def = definitions_[it->second];

    switch (def.type)
    {
    case ParameterType::Boolean:
    {
        const auto* b = std::get_if<bool>(&value);
        if (b == nullptr) return false;
        def.defaultValue = *b;
        return true;
    }
    case ParameterType::Integer:
    {
        int v;
        if (const auto* iv = std::get_if<int>(&value)) v = *iv;
        else if (const auto* dv = std::get_if<double>(&value)) v = static_cast<int>(*dv);
        else return false;
        if (def.range.min.has_value()) v = std::max(v, static_cast<int>(*def.range.min));
        if (def.range.max.has_value()) v = std::min(v, static_cast<int>(*def.range.max));
        def.defaultValue = v;
        return true;
    }
    case ParameterType::Floating:
    {
        double v;
        if (const auto* dv = std::get_if<double>(&value)) v = *dv;
        else if (const auto* iv = std::get_if<int>(&value)) v = static_cast<double>(*iv);
        else return false;
        if (def.range.min.has_value()) v = std::max(v, *def.range.min);
        if (def.range.max.has_value()) v = std::min(v, *def.range.max);
        def.defaultValue = v;
        return true;
    }
    case ParameterType::String:
    {
        const auto* s = std::get_if<std::string>(&value);
        if (s == nullptr) return false;
        // Phase 25.1 fix: the `domains` field stores filter TAGS (e.g.
        // {"runtime","world"}), NOT enum values. Validating a string against it
        // wrongly rejected valid enum values such as substrate_shape="circular"
        // (the combo change was silently dropped via setValue() returning false,
        // so "Aplicar ambiente" reverted the substrate to rectangular). The same
        // silently broke every string-enum apply (movement mode, body shape,
        // food mode, vision mode, ...). The real enum option lists live in
        // prefsEnumValuesFor() in the UI layer, which already restricts the combo
        // to valid values, and the engine maps any unknown string to a safe
        // default. So accept any string here.
        def.defaultValue = *s;
        return true;
    }
    case ParameterType::ColorRgb:
    {
        const auto* c = std::get_if<ColorRgb>(&value);
        if (c == nullptr) return false;
        ColorRgb out{std::clamp(c->r, 0, 255),
                     std::clamp(c->g, 0, 255),
                     std::clamp(c->b, 0, 255)};
        def.defaultValue = out;
        return true;
    }
    }
    return false;
}

bool ParameterRegistry::restoreDefault(const std::string& nameOrAlias)
{
    const auto it = indexByName_.find(nameOrAlias);
    if (it == indexByName_.end()) return false;
    ParameterDefinition& def = definitions_[it->second];
    def.defaultValue = def.originalDefault;
    return true;
}

bool ParameterRegistry::setApplyFlags(const std::string& nameOrAlias, const unsigned int flags)
{
    const auto it = indexByName_.find(nameOrAlias);
    if (it == indexByName_.end()) return false;
    definitions_[it->second].applyFlags = flags;
    return true;
}

std::vector<std::string> ParameterRegistry::search(const std::string& query) const
{
    std::string lq;
    lq.reserve(query.size());
    for (char ch : query) lq.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));

    auto lower = [](std::string s) {
        for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return s;
    };

    std::vector<std::string> out;
    out.reserve(definitions_.size());
    for (const auto& def : definitions_)
    {
        if (lq.empty())
        {
            out.push_back(def.name);
            continue;
        }
        bool match = lower(def.name).find(lq) != std::string::npos;
        if (!match) match = lower(def.category).find(lq) != std::string::npos;
        if (!match) match = lower(def.description).find(lq) != std::string::npos;
        if (!match)
        {
            for (const auto& a : def.aliases)
            {
                if (lower(a).find(lq) != std::string::npos) { match = true; break; }
            }
        }
        if (match) out.push_back(def.name);
    }
    return out;
}

std::vector<std::string> ParameterRegistry::categories() const
{
    std::vector<std::string> out;
    for (const auto& def : definitions_)
    {
        if (std::find(out.begin(), out.end(), def.category) == out.end())
        {
            out.push_back(def.category);
        }
    }
    return out;
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
