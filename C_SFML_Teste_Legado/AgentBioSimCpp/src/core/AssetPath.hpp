#pragma once

#include <string>

namespace agentbiosim::core
{
// Phase 25.3: absolute path of the directory that contains the running
// executable, with a trailing separator (e.g. "C:/.../build/Release/"). Used to
// locate bundled assets (icons) regardless of the current working directory —
// CMake copies the Assets/ folder next to the executable on every build.
// Returns an empty string if the location cannot be determined.
[[nodiscard]] std::string executableDir();
} // namespace agentbiosim::core
