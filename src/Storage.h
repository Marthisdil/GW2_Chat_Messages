#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace MacroTree { class Tree; }

// On-disk locations for everything this addon stores.
//   <GW2>\addons\Chat Messages\macros.json        the macro library
//   <GW2>\addons\Chat Messages\macros.json.bak    the version before the last save
//   <GW2>\addons\Chat Messages\exports\           files written by Export
//   <GW2>\addons\Chat Messages\imports\           files listed in the Import window
// The folder comes from Nexus' Paths_GetAddonDirectory(). Nexus does not create it, so we do.
namespace Storage
{
    std::filesystem::path AddonDir(); // created on first call; empty if Nexus is unavailable
    std::filesystem::path ExportsDir(); // <addon>\exports, created on first call (empty if unavailable)
    std::filesystem::path ImportsDir(); // <addon>\imports, the same

    // A path in aDir for aFileName that doesn't exist yet: "name.json", else "name (2).json"...
    std::filesystem::path FreeFile(const std::filesystem::path& aDir, const std::string& aFileName);
    // Writes UTF-8 text via a temporary file and rename. Returns false (logged) on failure.
    bool WriteText(const std::filesystem::path& aFile, const std::string& aText);
    // Opens a folder in Windows Explorer (only ever on a click).
    void OpenFolder(const std::filesystem::path& aDir);

    // Writes via a temporary file and rename so a crash can't leave a half-written file.
    // With aKeepBackup, the previous file is first copied to "<file>.bak".
    bool WriteJson(const std::filesystem::path& aFile, const nlohmann::json& aJson, bool aKeepBackup);

    // Loads the library into aTree. A missing file means an empty library. Never loses data: a file
    // that can't be read is renamed to "macros.json.unreadable-<time>" and the backup is tried
    // instead. aMessages gets what the user should be told (empty when all went well).
    void LoadMacros(MacroTree::Tree& aTree, std::vector<std::string>& aMessages);

    // Returns false if the file couldn't be written (the reason is logged).
    bool SaveMacros(const MacroTree::Tree& aTree);
}
