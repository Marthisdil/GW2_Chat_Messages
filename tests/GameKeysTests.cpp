// Standalone tests for src/GameKeys.cpp (GW2's default keybinds, for the hotkey editor's warning).

#include <cstring>
#include <set>

#include "GameKeys.h"
#include "TestCheck.h"

using Hotkey::Combo;

static Combo C(uint16_t key, bool ctrl = false, bool alt = false, bool shift = false)
{
    Combo c;
    c.Key = key;
    c.Ctrl = ctrl;
    c.Alt = alt;
    c.Shift = shift;
    return c;
}

static bool Is(const Combo& aCombo, const char* aUse)
{
    const char* use = GameKeys::DefaultUse(aCombo);
    return use && std::strcmp(use, aUse) == 0;
}

static void TestTable()
{
    size_t n = 0;
    const GameKeys::Bind* all = GameKeys::All(n);
    CHECK(n > 60);
    std::set<uint32_t> seen;
    for (size_t i = 0; i < n; i++)
    {
        CHECK(all[i].Use != nullptr && all[i].Use[0] != '\0');
        CHECK(!all[i].Combo.Empty());
        CHECK(seen.insert(all[i].Combo.Pack()).second); // each combo once
        // Only keys that could be a hotkey: a refused one never needs the warning.
        CHECK(Hotkey::Refusal(all[i].Combo).empty());
    }
}

static void TestLookups()
{
    CHECK(Is(C(0x70), "Profession skill 1"));          // F1
    CHECK(Is(C(0x76), "Profession skill 7"));          // F7
    CHECK(GameKeys::DefaultUse(C(0x77)) == nullptr);   // F8 isn't bound by default
    CHECK(Is(C('1'), "Weapon skill 1"));
    CHECK(Is(C('0'), "Elite skill"));
    CHECK(Is(C('T', true), "Call target"));
    CHECK(Is(C('T', false, false, true), "Alert target"));
    CHECK(Is(C('T'), "Take target"));
    CHECK(Is(C('V'), "Dodge, and mount ability 1"));
    CHECK(Is(C(0x09, false, false, true), "Previous enemy")); // Shift+Tab
    CHECK(Is(C('H', true, false, true), "Show or hide the whole UI"));
    CHECK(Is(C('3', false, true), "Squad location marker"));
    CHECK(Is(C('9', false, true, true), "Clear all squad object markers"));
    CHECK(Is(C(0x0D, false, true), "Full screen on or off")); // Alt+Enter
    // Exact matches only: a modifier makes it a different key.
    CHECK(GameKeys::DefaultUse(C('1', true)) == nullptr);
    CHECK(GameKeys::DefaultUse(C(0x70, false, false, true)) == nullptr);
    CHECK(GameKeys::DefaultUse(C('T', true, true)) == nullptr);
    CHECK(GameKeys::DefaultUse(Combo{}) == nullptr);
}

void RunGameKeysTests()
{
    TestTable();
    TestLookups();
}
