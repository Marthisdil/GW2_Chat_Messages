#pragma once
// The starter library (docs/design.md, "Starter library"): an "Examples" folder with a few nested
// folders of macros showing lines and channels, a whisper, placeholders and filters. It's added
// on the first run (no macros.json yet) and with "Add the example macros" on the options page.
// Names and lines are in the addon's current language. No hotkeys and no cooldowns are set.
// No Nexus or ImGui dependency. Tested by tests/ExamplesTests.cpp.

#include "MacroTree.h"

namespace Examples
{
    // The top Examples folder's id, reserved so it can be found by id (MacroTree.h).
    constexpr uint64_t kFolderId = MacroTree::kReservedExamplesId;

    // Adds the Examples folder at the top level of aTree, as "Examples (2)" etc. when the name is
    // taken. The first one gets kFolderId; a copy while one with that id exists (in the tree or
    // the bin) gets an ordinary id. Returns the new folder, or nullptr if it couldn't be added.
    MacroTree::Node* Add(MacroTree::Tree& aTree);

    // Lion's Arch, the map the map-filter example is limited to.
    constexpr int kLionsArchMapId = 50;
}
