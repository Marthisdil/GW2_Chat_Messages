#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace MacroTree { class Tree; }

// On-disk locations for everything this addon stores.
//   <GW2>\addons\Chat Messages\macros.json        the macro library
//   <GW2>\addons\Chat Messages\macros.json.bak    the version before the last save
// The folder comes from Nexus' Paths_GetAddonDirectory(). Nexus does not create it, so we do.
namespace Storage
{
    std::filesystem::path AddonDir(); // created on first call; empty if Nexus is unavailable

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
