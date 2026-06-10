#pragma once

#include "config/Parameter.hpp"
#include "sim/SimulationSnapshot.hpp"

#include <string>
#include <utility>
#include <vector>

// Phase 28: read/write the `.agentbiosim` save file. The file is JSON (via the
// project's small io::Json) holding the full engine snapshot plus the parameter
// values, the camera and metadata — everything needed to reopen a simulation
// exactly as it was saved. Forward-looking format only (no legacy Python compat).
namespace agentbiosim::io
{
constexpr int kSaveSchemaVersion = 1;

struct CameraState
{
    bool valid = false;
    double centerX = 0.0;
    double centerY = 0.0;
    double zoom = 1.0;
};

// One persisted parameter (name + current value).
using SavedParam = std::pair<std::string, config::ParameterValue>;

struct SaveBundle
{
    sim::SimulationSnapshot snapshot;
    std::vector<SavedParam> params;
    CameraState camera;
};

struct LoadResult
{
    bool ok = false;
    std::string error;
    int schemaVersion = 0;
    SaveBundle bundle;
};

// Serialize a bundle to a `.agentbiosim` file. Returns false (with the OS error
// folded into the message) when the file cannot be written.
[[nodiscard]] bool saveToFile(const std::string& path, const SaveBundle& bundle,
                              std::string& error);

// Parse a `.agentbiosim` file. `ok` is false with an `error` on any failure.
[[nodiscard]] LoadResult loadFromFile(const std::string& path);

// Phase 28: single-organism (.organism) export/import.
struct AgentLoadResult
{
    bool ok = false;
    std::string error;
    sim::AgentExport agent;
};

[[nodiscard]] bool saveAgentToFile(const std::string& path, const sim::AgentExport& agent,
                                   std::string& error);
[[nodiscard]] AgentLoadResult loadAgentFromFile(const std::string& path);
} // namespace agentbiosim::io
