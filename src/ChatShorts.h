#pragma once

// Macros from Blish HUD's Chat Shorts module (agaertner's bhm-chat-macros), for the Import
// window's "From Blish HUD" (docs/design.md, "Import / export"). Chat Shorts keeps them in a
// LiteDB file, macros.db (read by src/LiteDb.*): a flat list of macros in "chat_macros", each
// pointing at its lines in "chat_lines". This turns those documents into macros.
// No Nexus or ImGui dependency (tested by tests/ChatShortsTests.cpp).

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Hotkey.h"
#include "MacroTree.h"

namespace ChatShorts
{
    constexpr const char* kFileName = "macros.db";

    // Where Blish HUD keeps it unless its settings folder was moved:
    // <Documents>\Guild Wars 2\addons\blishhud\chat_shorts\macros.db.
    std::filesystem::path DefaultFile(const std::filesystem::path& aDocuments);
    // A file the Import window lists: "*.db", but not LiteDB's "*-log.db".
    bool IsDbFileName(const std::filesystem::path& aPath);
    // LiteDB's side file next to it ("macros-log.db"): while it has anything in it, the newest
    // changes may not be in macros.db yet (Blish HUD still running, or it didn't close cleanly).
    std::filesystem::path LogFileOf(const std::filesystem::path& aDbFile);

    // --- The fields (exposed for the tests) ---
    // A line's "channel": Chat Shorts' name ("Say", "Guild3"; any case) or its number (0-15),
    // the same 16 channels in the same order as ours. False if it's neither.
    bool    ChannelOf(const nlohmann::json& aValue, MacroTree::EChannel& aOut);
    // A macro's "game_mode": flags as text ("PvE, WvW") or a number (PvE 1, WvW 2, PvP 4).
    // None (0) means every mode in Chat Shorts, so it becomes all of them.
    uint8_t GameModesOf(const nlohmann::json& aValue);
    // A macro's "key_binding": JSON text, {"PrimaryKey": <key>, "ModifierKeys": <flags>}; keys
    // are Windows key codes, modifiers Ctrl 1, Alt 2, Shift 4. "{}" or anything else: no hotkey.
    Hotkey::Combo HotkeyOf(const nlohmann::json& aValue);
    // {blish} becomes {nexus} (the addon's version). aMissing gets the Chat Shorts placeholders
    // this addon doesn't have ("wp", "poi", "area"), once each; they're left as typed.
    std::string FixPlaceholders(const std::string& aText, std::vector<std::string>& aMissing);

    // Adds every macro in aCollections (LiteDb::Result::Collections) to aTree's top level, sorted
    // A-Z by title. Lines no macro points at (Chat Shorts leaves them behind when a macro is
    // deleted) are ignored. Voice commands and linked text files aren't taken (a warning counts
    // them). Problems go into aWarnings. Returns the number of macros added.
    size_t Build(const std::map<std::string, std::vector<nlohmann::json>>& aCollections, MacroTree::Tree& aTree,
                 std::vector<std::string>& aWarnings);
}
