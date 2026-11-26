#pragma once

// The first-run welcome and the library tour (docs/design.md, "First-run welcome and tour").
// No Nexus or ImGui dependency: Ui.cpp draws them, and these rules say when and what.

#include <cstddef>
#include <vector>

namespace Welcome
{
    // settings.json "welcome_shown": the welcome was answered, or wasn't needed (an upgrade).
    constexpr const char* kShownSetting = "welcome_shown";

    // What Ui::Init does at load.
    struct Start
    {
        bool ShowWelcome       = false; // a real first run: ask about the examples, then offer the tour
        bool MarkShown         = false; // save "welcome_shown": true now (no welcome for this library)
        bool MarkExamplesAdded = false; // save "examples_added": true now (as before the welcome)
    };

    // aExamplesAdded / aWelcomeShown: the settings. aNoLibrary: Storage::LoadMacros found no
    // macros.json and no .bak. aTreeEmpty: nothing was loaded. A first run is all four (no
    // library ever, nothing remembered). An upgrade or a library that exists gets no welcome and
    // no examples, and both settings are saved (as "examples_added" was before the welcome). Once
    // the welcome was answered, nothing changes ("Start empty" leaves "examples_added" unset, so
    // the options page's button later adds them without asking "again?").
    Start Decide(bool aExamplesAdded, bool aWelcomeShown, bool aNoLibrary, bool aTreeEmpty);

    // What a tour step points at in the library window (None: a box in the middle of it).
    enum class ETarget
    {
        None,
        Toolbar,  // New folder, New macro and the rest
        Search,   // the search box
        Tree,     // the folders and macros
        Header,   // the selected macro's name with Send now, Preview and "..."
        Lines,    // "Lines (n)" and the line rows
        Sections, // Where it's active, Hotkey and favourite, Note
        Count
    };

    // Steps for the editor (Header, Lines, Sections) need a macro selected; the tour selects
    // one of the examples if there are any.
    bool NeedsMacro(ETarget aTarget);

    struct Step
    {
        ETarget     Target;
        const char* Title; // TX texts: look them up with Localization::Lookup
        const char* Text;
    };

    // The tour, in order. It never points at anything behind the Advanced options box; one step
    // names the box on the options page.
    const std::vector<Step>& Steps();
}
