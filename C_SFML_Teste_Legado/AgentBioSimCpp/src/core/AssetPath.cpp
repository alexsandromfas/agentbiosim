#include "core/AssetPath.hpp"

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
std::string executableDir()
{
#ifdef _WIN32
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length == 0 || length >= sizeof(buffer))
    {
        return {};
    }
    const std::string path(buffer, length);
    const auto slash = path.find_last_of("\\/");
    if (slash == std::string::npos)
    {
        return {};
    }
    return path.substr(0, slash + 1);
#else
    return {};
#endif
}
} // namespace agentbiosim::core
