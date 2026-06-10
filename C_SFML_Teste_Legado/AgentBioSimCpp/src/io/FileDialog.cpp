#include "io/FileDialog.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <cstring>
#endif

namespace agentbiosim::io
{
#ifdef _WIN32
namespace
{
// Double-null-terminated filters (the trailing "\0" plus the implicit literal
// terminator gives the required "\0\0").
const char* const kSimFilter = "AgentBioSim (*.agentbiosim)\0*.agentbiosim\0Todos (*.*)\0*.*\0";
const char* const kOrgFilter = "Organismo (*.organism)\0*.organism\0Todos (*.*)\0*.*\0";

std::string runSave(const std::string& suggested, const char* filter, const char* defExt)
{
    char file[MAX_PATH] = {};
    if (!suggested.empty())
    {
        std::size_t n = suggested.size();
        if (n > sizeof(file) - 1) n = sizeof(file) - 1;
        std::memcpy(file, suggested.data(), n);
        file[n] = '\0';
    }
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetSaveFileNameA(&ofn) != 0 ? std::string(file) : std::string();
}

std::string runOpen(const char* filter, const char* defExt)
{
    char file[MAX_PATH] = {};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&ofn) != 0 ? std::string(file) : std::string();
}
} // namespace

std::string saveSimulationDialog(const std::string& suggestedName)
{
    return runSave(suggestedName, kSimFilter, "agentbiosim");
}
std::string openSimulationDialog() { return runOpen(kSimFilter, "agentbiosim"); }
std::string saveOrganismDialog(const std::string& suggestedName)
{
    return runSave(suggestedName, kOrgFilter, "organism");
}
std::string openOrganismDialog() { return runOpen(kOrgFilter, "organism"); }
#else
std::string saveSimulationDialog(const std::string&) { return {}; }
std::string openSimulationDialog() { return {}; }
std::string saveOrganismDialog(const std::string&) { return {}; }
std::string openOrganismDialog() { return {}; }
#endif
} // namespace agentbiosim::io
