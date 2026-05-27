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

private:
    std::vector<ParameterDefinition> definitions_;
    std::unordered_map<std::string, std::size_t> indexByName_;
};
} // namespace agentbiosim::config
