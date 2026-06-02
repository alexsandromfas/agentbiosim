#pragma once

#include <cstddef>
#include <string>

namespace agentbiosim::systems
{
// Phase 25 selftest: the UI was migrated to Dear ImGui, but the engine and the
// command/model backend must be unchanged and decoupled. This headless test
// validates (without any ImGui/SFML window):
//   * Divida 8: SimulationRunner::applyCommand consumes core::Command (the
//     engine no longer depends on ui::); a core::Command applied to the runner
//     produces the expected effect;
//   * the new ImGui-native commands (CmdSetSpeciesLabel / CmdSetSpeciesColor)
//     are valid members of core::Command;
//   * the species/label operations the Labels tab drives still work
//     (assign / create / set label / set color / count);
//   * the preferences model the ImGui windows drive still works
//     (prefsParametersForTab / prefsApplyPending);
//   * the dock parameter lists (editor / substrato) are intact;
//   * the engine keeps stepping after these operations.
struct Phase25ValidationSummary
{
    bool passed = true;
    std::size_t checks = 0;
    std::string details;
};

[[nodiscard]] Phase25ValidationSummary runPhase25Validation();
} // namespace agentbiosim::systems
