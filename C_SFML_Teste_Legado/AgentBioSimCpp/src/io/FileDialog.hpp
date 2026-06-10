#pragma once

#include <string>

// Phase 28: native Save/Open file dialogs for the .agentbiosim format. Windows
// only (the project is Windows/MSVC); returns an empty string when the user
// cancels. windows.h is isolated to the .cpp, like core/AssetPath.
namespace agentbiosim::io
{
[[nodiscard]] std::string saveSimulationDialog(const std::string& suggestedName);
[[nodiscard]] std::string openSimulationDialog();
// Single-organism files (.organism).
[[nodiscard]] std::string saveOrganismDialog(const std::string& suggestedName);
[[nodiscard]] std::string openOrganismDialog();
} // namespace agentbiosim::io
