#pragma once

// Advanced options (docs/design.md, "Advanced options"): features most players never need, kept
// out of sight behind one checkbox on the options page so the addon stays simple to learn. While
// the box is off, those features are hidden and don't work; nothing the user set is deleted. No
// Nexus or ImGui dependency. Tested by tests/AdvancedTests.cpp.
//
// Behind the box (the UI side is in Ui.cpp):
// - link files: a linked macro can't be sent while it's off (Blocks, Blocked);
// - a macro's hotkey cooldown: ignored while it's off (CooldownSec);
// - the Raw text tab, importing a whole library, Keep hotkeys on import, the Refresh map list
//   button: hidden while it's off.
// Your own language files have their own opt-in instead (Localization::kOwnFilesSetting).

#include <cstddef>

#include <nlohmann/json_fwd.hpp>

#include "MacroTree.h"

namespace Advanced
{
    constexpr const char* kSetting = "advanced_options"; // settings.json: true or false

    // Whether the macro can only be sent with the box on (it's linked to a text file). Folders never.
    bool Blocks(const MacroTree::Node& aNode);
    // Whether the macro has a hotkey cooldown, which only applies with the box on.
    bool HasCooldown(const MacroTree::Node& aNode);
    // Whether the macro uses anything behind the box.
    bool Uses(const MacroTree::Node& aNode);

    // Whether the macro can't be sent now: Blocks, and the box is off.
    bool Blocked(const MacroTree::Node& aNode, bool aAdvancedOn);
    // The cooldown in effect: the macro's own with the box on, none with it off.
    int CooldownSec(const MacroTree::Node& aNode, bool aAdvancedOn);

    // What the library (not the trash bin) uses that's behind the box.
    struct InUse
    {
        size_t Linked    = 0; // macros linked to a text file
        size_t Cooldowns = 0; // macros with a cooldown
        bool Any() const { return Linked + Cooldowns > 0; }
    };
    InUse Count(const MacroTree::Tree& aTree);
    bool AnyUses(const MacroTree::Tree& aTree);

    // The setting as saved: off unless it's saved as true (the default for everyone).
    bool FromSettings(const nlohmann::json& aSettings);
}
