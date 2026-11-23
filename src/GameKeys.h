#pragma once

// Guild Wars 2's default keybinds (PC, US keyboard), so the hotkey editor can warn when a macro's
// hotkey is also a game key (docs/design.md, "Hotkeys"). A hotkey never blocks its key, so a
// shared key does both things. These are the defaults the game ships with, from the GW2 wiki's
// Options page (Control Options), checked 2026-11-23: the user's own binds may differ, and the
// warning says so. No Nexus, ImGui or windows.h dependency (tests/GameKeysTests.cpp).

#include <cstddef>

#include "Hotkey.h"

namespace GameKeys
{
    // What aCombo does in GW2 by default (English, marked TX for translation: look it up with
    // Localization::Lookup or TR before showing it), or nullptr when it isn't a default key.
    // Only exact matches count: Ctrl+1 isn't reported as Weapon skill 1.
    const char* DefaultUse(const Hotkey::Combo& aCombo);

    struct Bind
    {
        Hotkey::Combo Combo;
        const char*   Use = nullptr;
    };
    // Every default bind, for the tests (each combo once).
    const Bind* All(size_t& aCount);
}
