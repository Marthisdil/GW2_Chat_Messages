#pragma once

// Link files: a macro kept in step with a .txt file in the raw format (one chat line per row),
// both ways (docs/design.md, "Link files"). Edit the file in Notepad and the macro follows; edit
// the macro in the addon and the file follows. No Nexus or ImGui dependency (tested by
// tests/LinkSyncTests.cpp); the UI polls and does the file reads and writes.

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "MacroTree.h"

namespace LinkSync
{
    using MacroTree::Line;

    constexpr size_t kMaxFileBytes = 256 * 1024;

    // Checks a link path typed by the user. Must end in ".txt". A path without a drive or
    // leading slash is relative to <addon>\links\. Returns "" if fine, else why not.
    std::string CheckPath(const std::string& aPath);
    // Where a stored link path points: absolute paths as they are, others in aAddonDir\links.
    std::filesystem::path Resolve(const std::filesystem::path& aAddonDir, const std::string& aPath);
    // Whether two resolved paths are the same file name (case-insensitive, like Windows).
    bool SamePath(const std::filesystem::path& aA, const std::filesystem::path& aB);

    // The text written to a link file for these lines (Windows line ends, a final newline).
    std::string FileText(const std::vector<Line>& aLines);

    // What was last agreed between the macro and its file. Kept in memory only.
    struct State
    {
        bool        Known = false; // false until the first sync (at load or when linked)
        int64_t     Mtime = 0;     // the file's last-write time when last read or written
        std::string Text;          // the macro's lines as raw text, as last agreed
    };

    struct File
    {
        bool    Exists = false;
        int64_t Mtime  = 0;
        // Reads the file. False with aError if it can't be read (or is too big).
        std::function<bool(std::string& aText, std::string& aError)> Read;
    };

    struct Outcome
    {
        bool              Missing = false;  // the file isn't there: nothing synced
        std::string       Error;            // the file couldn't be read: nothing synced
        bool              LoadMacro = false; // set the macro's lines to Lines
        std::vector<Line> Lines;
        bool              WriteFile = false; // write FileText to the file, then set State::Mtime
        std::string       Text;
        std::string       Warning;          // e.g. more lines than a macro can hold
    };

    // One sync step. A file changed since the last sync wins over the macro (it's the newer
    // edit); otherwise a macro changed in the addon is written to the file. Formatting
    // differences alone (blank rows, line ends, "/say" vs "/s") never cause a write.
    Outcome Sync(State& aState, const std::vector<Line>& aMacroLines, const File& aFile);
}
