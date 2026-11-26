#pragma once

// Advanced options (docs/design.md, "Advanced options"): features most players never need, kept
// out of sight behind one checkbox on the options page so the addon stays simple to learn. While
// the box is off, those features are hidden and a macro that uses one can't be sent (by a hotkey,
// the quick menu or Send now); its settings, hotkey included, are kept as they are. No Nexus or
// ImGui dependency. Tested by tests/AdvancedTests.cpp.
//
// The features behind the box: link files (a macro kept in step with a .txt file, LinkSync).

#include <nlohmann/json_fwd.hpp>

#include "MacroTree.h"

namespace Advanced
{
    constexpr const char* kSetting = "advanced_options"; // settings.json: true or false

    // Whether this macro uses a feature that's behind the box. Folders never do.
    bool Uses(const MacroTree::Node& aNode);

    // Whether the macro can't be sent now: it uses an advanced feature and the box is off.
    bool Blocked(const MacroTree::Node& aNode, bool aAdvancedOn);

    // Whether any macro in the library (not the trash bin) uses an advanced feature.
    bool AnyUses(const MacroTree::Tree& aTree);

    // The setting as saved. With none saved yet, it's on only if the library already uses an
    // advanced feature, so a link made before the box existed keeps working after an update.
    bool FromSettings(const nlohmann::json& aSettings, const MacroTree::Tree& aTree);
}
