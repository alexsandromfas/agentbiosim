#pragma once

// Phase 29: build metadata embedded for benchmark reports. The git commit is
// injected by CMake at configure time (AGENTBIOSIM_GIT_COMMIT); falls back to
// "unknown" when git is unavailable. Build mode comes from NDEBUG.
namespace agentbiosim::core
{
#ifndef AGENTBIOSIM_GIT_COMMIT
#define AGENTBIOSIM_GIT_COMMIT "unknown"
#endif

inline constexpr const char* kGitCommit = AGENTBIOSIM_GIT_COMMIT;

#ifdef NDEBUG
inline constexpr const char* kBuildMode = "Release";
#else
inline constexpr const char* kBuildMode = "Debug";
#endif
} // namespace agentbiosim::core
