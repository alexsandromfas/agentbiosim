#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace agentbiosim::systems
{
// Microfase 23.2 hotfix selftests. Cover the second-round preferences UI
// fixes: enum combo (the registry's `domains` field is tags not enum values
// — must use prefsEnumValuesFor), neural window filter by active type,
// friendly labels for every neural family knob, draggable windows command,
// inline text-edit commands, Restore-Defaults-and-Apply.
struct Phase23_2ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase23_2ValidationSummary runPhase23_2Validation();
} // namespace agentbiosim::systems
