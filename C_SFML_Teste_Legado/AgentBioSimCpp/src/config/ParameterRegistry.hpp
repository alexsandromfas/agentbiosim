#pragma once

#include "config/Parameter.hpp"

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

namespace agentbiosim::config
{
class ParameterRegistry
{
public:
    void add(ParameterDefinition definition);

    [[nodiscard]] const ParameterDefinition* find(const std::string& nameOrAlias) const;
    [[nodiscard]] const std::vector<ParameterDefinition>& definitions() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    void dump(std::ostream& output) const;

    // Phase 23: runtime mutation. The registry stores the current value in
    // `defaultValue` (semantically: current value = last applied value; original
    // default is preserved in the `originalDefault` field of ParameterDefinition).
    // `setValue` clamps numeric values into `range`, accepts enum strings only
    // when they belong to `domains`, and returns true if the value was applied.
    bool setValue(const std::string& nameOrAlias, const ParameterValue& value);
    bool restoreDefault(const std::string& nameOrAlias);
    bool setApplyFlags(const std::string& nameOrAlias, unsigned int flags);

    // Phase 23: case-insensitive substring search across name, aliases,
    // description and category. Returns matching names. Empty query returns all.
    [[nodiscard]] std::vector<std::string> search(const std::string& query) const;

    // Phase 23: list of distinct category labels in registration order.
    [[nodiscard]] std::vector<std::string> categories() const;

private:
    std::vector<ParameterDefinition> definitions_;
    std::unordered_map<std::string, std::size_t> indexByName_;
};
} // namespace agentbiosim::config
