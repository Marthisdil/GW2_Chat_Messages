#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace MacroTree { class Tree; }

// On-disk locations for everything this addon stores.
//   <GW2>\addons\GW2_Chat_Messages\macros.json        the macro library
//   <GW2>\addons\GW2_Chat_Messages\macros.json.bak    the version before the last save
//   <GW2>\addons\GW2_Chat_Messages\settings.json      addon settings (the text size)
//   <GW2>\addons\GW2_Chat_Messages\exports\           files written by Export
//   <GW2>\addons\GW2_Chat_Messages\imports\           files listed in the Import window
//   <GW2>\addons\GW2_Chat_Messages\backups\           copies of the library (docs/design.md, "Backups")
//   <GW2>\addons\GW2_Chat_Messages\localization\      language files you add (docs/design.md, "Languages")
// The folder comes from Nexus' Paths_GetAddonDirectory(). Nexus does not create it, so we do.
namespace Storage
{
    // AddonLoad, before anything reads or writes a file: looks up the addon folder once (and
    // creates it) and starts the writer thread. AddonUnload, last: Shutdown writes everything
    // still waiting, then stops the thread (docs/design.md, "Saving off the render thread").
    void Init();
    void Shutdown();
    // Runs aJob on the writer thread, after everything posted before it. A job with a key
    // replaces that key's job while it's still waiting (a burst of saves writes once). Jobs may
    // only use the file functions below, never the tree, ImGui or another module's state.
    void Post(std::function<void()> aJob, const std::string& aKey = std::string());
    void Flush(); // waits until every job posted so far has run (render thread only, never in a job)

    std::filesystem::path AddonDir(); // looked up once (Init); empty if Nexus is unavailable
    std::filesystem::path ExportsDir(); // <addon>\exports, created on first call (empty if unavailable)
    std::filesystem::path ImportsDir(); // <addon>\imports, the same
    std::filesystem::path LocalizationDir(); // <addon>\localization (language files), the same
    std::filesystem::path BackupsDir(); // <addon>\backups, the same
    std::filesystem::path PerfDir();    // <addon>\perf (the frame recorder's files), the same

    // A path in aDir for aFileName that doesn't exist yet: "name.json", else "name (2).json"...
    std::filesystem::path FreeFile(const std::filesystem::path& aDir, const std::string& aFileName);
    // Writes UTF-8 text via a temporary file and rename. Returns false (logged) on failure.
    bool WriteText(const std::filesystem::path& aFile, const std::string& aText);
    // Adds aText to the end of aFile (made if it's missing). aStart: the file is emptied first.
    // For the frame recorder's file, from a writer job. False if it couldn't be written.
    bool AppendText(const std::filesystem::path& aFile, const std::string& aText, bool aStart);
    // Opens a folder in Windows Explorer (only ever on a click).
    void OpenFolder(const std::filesystem::path& aDir);
    // Opens one of the addon's built-in web addresses (Globals.h) in the browser, only ever on a
    // click. Anything that isn't an https:// address is refused. Never pass an address from a
    // macro, a file or the network.
    void OpenWebPage(const char* aUrl);

    // Writes via a temporary file and rename so a crash can't leave a half-written file.
    // With aKeepBackup, the previous file is first copied to "<file>.bak".
    bool WriteJson(const std::filesystem::path& aFile, const nlohmann::json& aJson, bool aKeepBackup);

    // Loads the library into aTree. A missing file means an empty library. Never loses data: a file
    // that can't be read is renamed to "macros.json.unreadable-<time>" and the backup is tried
    // instead. aMessages gets what the user should be told (empty when all went well).
    // Returns true when there was no macros.json and no macros.json.bak at all (a first run, or
    // the user removed both).
    bool LoadMacros(MacroTree::Tree& aTree, std::vector<std::string>& aMessages);

    // Saves the library on the writer thread: the tree becomes JSON here (call it on the render
    // thread, which owns the tree), the file is written there. Returns the save's number, or 0
    // for a tree from a newer version (Tree::FromNewerVersion): that file is never written over.
    // A tree read from an older format first keeps the file as "macros.json.v<N>.bak" (once).
    // LastMacrosSave says how the newest finished save went (a save replaced by a newer one
    // while it waited never finishes: compare with the number you got last).
    uint64_t SaveMacrosLater(const MacroTree::Tree& aTree);
    struct SaveResult
    {
        uint64_t Seq = 0;    // 0: none finished yet
        bool     Ok  = true; // false: the file couldn't be written (the reason is logged)
    };
    SaveResult LastMacrosSave();
    // SaveMacrosLater, then waits for it (at unload). False if it couldn't be written.
    bool SaveMacros(const MacroTree::Tree& aTree);

    // The library as it's written to macros.json (and to a backup). JsonText turns the tree's
    // JSON (MacroTree::Tree::ToJson) into that text, so a job can do it off the render thread.
    std::string LibraryText(const MacroTree::Tree& aTree);
    std::string JsonText(const nlohmann::json& aJson);
    // macros.json as it is on disk (at most kMaxBackupBytes); false if it can't be read. Backups
    // of a read-only library copy this, not LibraryText.
    bool ReadLibraryText(std::string& aText);

    // Backups (Backups.h). aName is always a file name that Backups::ParseFileName accepts;
    // anything else is refused, so these never touch another file.
    constexpr size_t kMaxBackupBytes = 32 * 1024 * 1024;
    std::vector<std::string> BackupNames(); // the names of the files in the backups folder
    bool WriteBackup(const std::string& aName, const std::string& aText); // false if it exists already
    bool DeleteBackup(const std::string& aName);
    // The file's text (at most kMaxBackupBytes). False if it can't be read.
    bool ReadBackupText(const std::string& aName, std::string& aText);
    // Reads a backup into aTree; aTree is only changed on success. aError says why not.
    bool ReadBackup(const std::string& aName, MacroTree::Tree& aTree, std::string& aError);

    // settings.json as an object; an empty object if it's missing or can't be read (logged).
    // It has "format" (kSettingsFormat). Settings never need a new number: a changed setting
    // gets a new name instead (docs/design.md, "Saved files").
    // The file is read once and kept in memory; SaveSetting changes it there (at once: the next
    // LoadSettings sees it) and the writer thread writes the file. Any thread.
    constexpr int kSettingsFormat = 1;
    nlohmann::json LoadSettings();
    // True once the change is kept in memory (false only for a non-object): the file is written
    // by the writer thread, which logs a failed write.
    bool SaveSetting(const char* aKey, const nlohmann::json& aValue); // changes one key; written soon
    bool SaveSettings(const nlohmann::json& aSettings);
}
