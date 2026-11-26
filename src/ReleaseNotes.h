#pragma once

// "What's new": the notes shown once after an update, from CHANGELOG.md (built into the DLL as
// a resource). docs/design.md, "Update notes". No Nexus or ImGui dependency.

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ReleaseNotes
{
    // settings.json "show_update_notes": show them after an update (on unless saved as false).
    constexpr const char* kShowSetting = "show_update_notes";
    // settings.json "notes_seen": the version (numbers only, "2026.11.21.1412") whose notes were
    // last shown, or that was running when there were none to show.
    constexpr const char* kSeenSetting = "notes_seen";

    bool ShowFromSettings(const nlohmann::json& aSettings);

    // "2026.11.21.1412", with or without ".Dev" (any suffix): false for anything else.
    struct Version
    {
        int Part[4] = {0, 0, 0, 0};
        bool operator<(const Version& o) const;
        bool operator==(const Version& o) const;
    };
    bool ParseVersion(const std::string& aText, Version& aOut);

    // One "## [2026.11.21.1412] ..." section of the changelog ("## [Unreleased]" is skipped).
    struct Section
    {
        std::string Version; // as written between the brackets
        std::string Body;    // the lines under it, up to the next "## "
    };
    std::vector<Section> Parse(const std::string& aChangelog);

    // The sections to show: those newer than aSeen, up to and including aCurrent, newest first.
    // An empty or unreadable aSeen (an update from before this feature) shows aCurrent's only.
    std::vector<Section> Since(const std::vector<Section>& aSections, const std::string& aSeen,
                               const std::string& aCurrent);

    // What Ui::Init does at load. aFirstRun: the welcome is shown instead (and the version is
    // remembered). Show is false when the notes are turned off, nothing's new, or there's no
    // section to show; Remember is true whenever "notes_seen" should be set to aCurrent now
    // (when Show is true it's set once the window is closed).
    struct Start
    {
        bool Show     = false;
        bool Remember = false;
    };
    Start Decide(bool aShowSetting, bool aFirstRun, const std::string& aSeen, const std::string& aCurrent,
                 bool aHasSections);

    // The body as blocks to draw: headings ("### Added"), bullets ("- ...", with their indent
    // level and wrapped continuation lines joined) and plain paragraphs. **bold**, `code` and
    // [links](url) become plain text.
    enum class EKind { Heading, Bullet, Text };
    struct Block
    {
        EKind       Kind   = EKind::Text;
        int         Indent = 0; // bullets: 0 for "- ", 1 for "  - ", ...
        std::string Text;
    };
    std::vector<Block> ToBlocks(const std::string& aBody);
}
