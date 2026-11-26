#pragma once

// The macro editor's layout rules (docs/design.md, "Macro editor layout"): which folding sections
// are open (remembered for the whole library, saved in settings.json as "editor_sections"), and
// the short note shown in the closed Note section's header. No Nexus or ImGui dependency, so it
// can be unit-tested (tests/EditorLayoutTests.cpp).

#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

namespace EditorLayout
{
    constexpr const char* kSetting = "editor_sections";

    // The folding sections under the lines. Open or closed is the user's choice, kept between
    // macros: one state per section, not per macro.
    struct Sections
    {
        bool Where    = false; // "Where it's active" (macros and folders)
        bool Hotkey   = true;  // "Hotkey and favourite"
        bool Note     = true;  // "Note": open at first, so notes are found
        bool Advanced = false; // "Advanced" (only shown while advanced options are on)

        bool operator==(const Sections& o) const
        {
            return Where == o.Where && Hotkey == o.Hotkey && Note == o.Note && Advanced == o.Advanced;
        }
        bool operator!=(const Sections& o) const { return !(*this == o); }
    };

    // Read from settings.json: missing or non-true/false values keep their defaults; unknown keys
    // are ignored (a newer version may add sections).
    Sections FromJson(const nlohmann::json& aValue);
    nlohmann::json ToJson(const Sections& aSections);

    // The note's first line, cut to about aMaxBytes (never inside a character), with "..." when
    // anything was left out (a cut, or more lines). "" for no note.
    std::string ShortNote(const std::string& aNote, size_t aMaxBytes = 40);
}
