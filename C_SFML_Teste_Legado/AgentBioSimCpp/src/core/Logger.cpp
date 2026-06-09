#include "core/Logger.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace agentbiosim::core
{
namespace
{
// The active session file. A raw FILE* (not ofstream) so the crash handler can
// append from an SEH context with minimal machinery.
std::FILE* g_logFile = nullptr;

double nowSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string timestamp()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

#ifdef _WIN32
LONG WINAPI crashFilter(EXCEPTION_POINTERS* info)
{
    if (g_logFile != nullptr)
    {
        const unsigned long code =
            info != nullptr && info->ExceptionRecord != nullptr
                ? static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode)
                : 0UL;
        std::fprintf(g_logFile, "%s\tERROR\tUNHANDLED_EXCEPTION\tcode=0x%08lX\n",
                     timestamp().c_str(), code);
        std::fflush(g_logFile);
    }
    return EXCEPTION_CONTINUE_SEARCH;  // let the default handler crash normally
}
#endif
} // namespace

LogLevel logLevelFromString(const std::string& text) noexcept
{
    if (text == "off" || text == "none") return LogLevel::Off;
    if (text == "error") return LogLevel::Error;
    if (text == "warn" || text == "warning") return LogLevel::Warn;
    if (text == "info") return LogLevel::Info;
    if (text == "debug") return LogLevel::Debug;
    return LogLevel::Off;
}

const char* logLevelName(const LogLevel level) noexcept
{
    switch (level)
    {
    case LogLevel::Off:   return "off";
    case LogLevel::Error: return "error";
    case LogLevel::Warn:  return "warn";
    case LogLevel::Info:  return "info";
    case LogLevel::Debug: return "debug";
    }
    return "off";
}

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

bool Logger::open(const std::string& baseDir)
{
    if (g_logFile != nullptr)
    {
        return true;  // already open
    }
    try
    {
        const std::filesystem::path dir = std::filesystem::path(baseDir) / "logs" / "runtime";
        std::filesystem::create_directories(dir);
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);
        const std::filesystem::path file = dir / (std::string("agentbiosim_") + stamp + ".log");
        path_ = file.string();
#ifdef _WIN32
        if (fopen_s(&g_logFile, path_.c_str(), "a") != 0)
        {
            g_logFile = nullptr;
        }
#else
        g_logFile = std::fopen(path_.c_str(), "a");
#endif
    }
    catch (...)
    {
        g_logFile = nullptr;
    }
    if (g_logFile != nullptr)
    {
        log(LogLevel::Info, "SESSION_STARTED", path_);
        return true;
    }
    return false;
}

void Logger::close()
{
    if (g_logFile != nullptr)
    {
        log(LogLevel::Info, "SESSION_ENDED");
        std::fclose(g_logFile);
        g_logFile = nullptr;
    }
}

void Logger::log(const LogLevel level, const std::string& event, const std::string& detail)
{
    // Up-front gate: nothing happens when disabled or below the active level.
    if (level == LogLevel::Off || level_ == LogLevel::Off || level > level_)
    {
        return;
    }
    if (g_logFile == nullptr)
    {
        return;
    }
    std::fprintf(g_logFile, "%s\t%s\t%s\t%s\n", timestamp().c_str(), logLevelName(level),
                 event.c_str(), detail.c_str());
    std::fflush(g_logFile);
}

void Logger::heartbeat(const double intervalSeconds, const std::string& detail)
{
    if (level_ == LogLevel::Off || g_logFile == nullptr)
    {
        return;
    }
    const double now = nowSeconds();
    if (now - lastHeartbeatSeconds_ < intervalSeconds)
    {
        return;
    }
    lastHeartbeatSeconds_ = now;
    log(LogLevel::Info, "HEARTBEAT", detail);
}

void installCrashHandler()
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(crashFilter);
#endif
}
} // namespace agentbiosim::core
