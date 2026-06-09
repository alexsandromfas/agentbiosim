#pragma once

#include <string>

namespace agentbiosim::core
{
// Phase 27: low-overhead session logger. Levels gate work up front, so when the
// level is Off (or below the message level) `log()` returns before formatting —
// steady-state cost is a single comparison. One newline-delimited file per
// session under <baseDir>/logs/runtime/.
enum class LogLevel : int
{
    Off = 0,
    Error,
    Warn,
    Info,
    Debug
};

[[nodiscard]] LogLevel logLevelFromString(const std::string& text) noexcept;
[[nodiscard]] const char* logLevelName(LogLevel level) noexcept;

class Logger
{
public:
    [[nodiscard]] static Logger& instance();

    void setLevel(LogLevel level) noexcept { level_ = level; }
    [[nodiscard]] LogLevel level() const noexcept { return level_; }

    // Open the session file (idempotent). Returns false if it could not be
    // created; logging then silently no-ops.
    bool open(const std::string& baseDir);
    void close();

    void log(LogLevel level, const std::string& event, const std::string& detail = {});

    // Writes an INFO "heartbeat" line at most once per `intervalSeconds` of wall
    // time. Cheap to call every frame.
    void heartbeat(double intervalSeconds, const std::string& detail = {});

    [[nodiscard]] const std::string& path() const noexcept { return path_; }

private:
    Logger() = default;

    LogLevel level_ = LogLevel::Off;
    std::string path_;
    double lastHeartbeatSeconds_ = -1.0e18;
};

// Install a process-wide crash handler that appends a line to the active session
// log on an unhandled fault, then lets the default handler run. Does NOT attempt
// real recovery/save (that is Fase 28). No-op outside Windows.
void installCrashHandler();
} // namespace agentbiosim::core
