// Standalone tests for src/Hotkey.cpp and the hotkey parts of src/MacroTree.cpp.

#include <string>

#include <nlohmann/json.hpp>

#include "Hotkey.h"
#include "Localization.h"
#include "MacroTree.h"
#include "TestCheck.h"

using namespace Hotkey;

static Combo C(uint16_t key, bool ctrl = false, bool alt = false, bool shift = false)
{
    Combo c;
    c.Key = key;
    c.Ctrl = ctrl;
    c.Alt = alt;
    c.Shift = shift;
    return c;
}

static void TestNames()
{
    CHECK(ToString(Combo{}) == "");
    CHECK(ToString(C(0x70, true, false, true)) == "Ctrl+Shift+F1");
    CHECK(ToString(C('A', false, true)) == "Alt+A");
    CHECK(ToString(C(kVkXButton1)) == "Mouse 4");
    CHECK(ToString(C(0x65, true)) == "Ctrl+Num 5");
    CHECK(ToString(C(0x6B, true)) == "Ctrl+Num +");
    CHECK(KeyName(0xE2) == "Key 0xE2");
    CHECK(KeyName(0x87) == "F24");
}

static void TestParse()
{
    CHECK(FromString("Ctrl+Shift+F1") == C(0x70, true, false, true));
    CHECK(FromString(" ctrl + alt + q ") == C('Q', true, true));
    CHECK(FromString("Mouse 5") == C(kVkXButton2));
    CHECK(FromString("Ctrl+Num +") == C(0x6B, true));
    CHECK(FromString("Num +") == C(0x6B));
    CHECK(FromString("Shift+Num 0") == C(0x60, false, false, true));
    CHECK(FromString("Alt+Key 0xE2") == C(0xE2, false, true));
    CHECK(FromString("").Empty());
    CHECK(FromString("Ctrl+").Empty());
    CHECK(FromString("Hyper+A").Empty());
    CHECK(FromString("F25").Empty());
    CHECK(FromString("Ctrl+Banana").Empty());

    // Every named key round-trips.
    for (uint32_t k = 1; k < 0xFF; k++)
    {
        const Combo c = C(static_cast<uint16_t>(k), true, true, true);
        CHECK(FromString(ToString(c)) == c);
    }
}

static void TestPack()
{
    const Combo c = C(0x70, true, false, true);
    CHECK(Combo::Unpack(c.Pack()) == c);
    CHECK(C('A').Pack() != C('A', true).Pack());
}

static void TestRefusal()
{
    CHECK(Refusal(C(0x70)).empty());          // F1
    // Mouse 3/4/5 need Ctrl, Alt or Shift.
    CHECK(!Refusal(C(kVkXButton1)).empty());  // Mouse 4 alone
    CHECK(!Refusal(C(kVkXButton2)).empty());
    CHECK(!Refusal(C(kVkMButton)).empty());
    CHECK(Refusal(C(kVkXButton1, true)).empty());
    CHECK(Refusal(C(kVkXButton2, false, true)).empty());
    CHECK(Refusal(C(kVkMButton, false, false, true)).empty());
    CHECK(IsMouseKey(kVkMButton) && IsMouseKey(kVkXButton1) && IsMouseKey(kVkXButton2));
    CHECK(!IsMouseKey(0x01) && !IsMouseKey('A'));
    CHECK(Refusal(C('1', true)).empty());
    CHECK(!Refusal(Combo{}).empty());
    CHECK(!Refusal(C(0x01)).empty());         // left mouse
    CHECK(!Refusal(C(0x02, true)).empty());   // right mouse, even with Ctrl
    // Never left or right mouse, with any modifiers (owner's decision, 2026-11-23).
    for (uint16_t button : {uint16_t(0x01), uint16_t(0x02)})
        for (int mods = 0; mods < 8; mods++)
        {
            const Combo c = C(button, mods & 1, (mods & 2) != 0, (mods & 4) != 0);
            CHECK(!Refusal(c).empty());
            CHECK(!Fires(c));
            CHECK(!KeepOnLoad(c));
        }
    CHECK(!Refusal(C(kVkEscape, true)).empty());
    CHECK(!Refusal(C(0x10)).empty());         // Shift alone
    CHECK(!Refusal(C(0xA2, true)).empty());   // Left Ctrl as the key
    CHECK(!Refusal(C(0x09)).empty());         // Tab alone
    CHECK(Refusal(C(0x09, true)).empty());    // Ctrl+Tab is allowed
    CHECK(!Refusal(C(0x0D)).empty());         // Enter alone
    CHECK(!Refusal(C(0x20)).empty());         // Space alone
}

// Plain mouse hotkeys saved before the modifier rule: kept on load, flagged, never fire.
static void TestOldMouseHotkeys()
{
    CHECK(NeedsModifier(C(kVkXButton1)));
    CHECK(NeedsModifier(C(kVkMButton)));
    CHECK(!NeedsModifier(C(kVkXButton1, true)));
    CHECK(!NeedsModifier(C(0x70)));
    CHECK(!NeedsModifier(Combo{}));

    CHECK(KeepOnLoad(C(kVkXButton1)));        // kept so the user sees it
    CHECK(KeepOnLoad(C(kVkXButton1, true)));
    CHECK(KeepOnLoad(C(0x70)));
    CHECK(!KeepOnLoad(C(kVkEscape)));
    CHECK(!KeepOnLoad(C(0x09)));
    CHECK(!KeepOnLoad(Combo{}));

    CHECK(!Fires(C(kVkXButton1)));            // but it never sends
    CHECK(!Fires(C(kVkXButton2)));
    CHECK(Fires(C(kVkXButton1, false, false, true)));
    CHECK(Fires(C(0x70)));
    CHECK(!Fires(Combo{}));

    // Through macros.json: the old hotkey stays on the macro, with no warning.
    using namespace MacroTree;
    Tree t;
    Node* m = t.AddMacro(nullptr, "Old").Target;
    m->Hotkey = C(kVkXButton1);
    Tree u;
    std::vector<std::string> warnings;
    CHECK(u.FromJson(t.ToJson(), &warnings));
    CHECK(warnings.empty());
    CHECK(u.MacrosWithHotkey(C(kVkXButton1)).size() == 1);
    CHECK(t.ToJson()["root"]["children"][0]["hotkey"] == "Mouse 4");
}

static void TestTree()
{
    using namespace MacroTree;
    Tree t;
    Node* f = t.AddFolder(nullptr, "F").Target;
    Node* a = t.AddMacro(nullptr, "A").Target;
    Node* b = t.AddMacro(f, "B").Target;
    a->Hotkey = C(0x70, true);
    b->Hotkey = C(0x70, true);
    f->Hotkey = C(0x70, true); // folders never count

    CHECK(t.MacrosWithHotkey(C(0x70, true)).size() == 2);
    CHECK(t.MacrosWithHotkey(C(0x70)).empty());
    CHECK(t.MacrosWithHotkey(Combo{}).empty());

    // JSON keeps the hotkey as text; a refused or unknown one is dropped with a warning.
    Tree u;
    CHECK(u.FromJson(t.ToJson()));
    CHECK(u.MacrosWithHotkey(C(0x70, true)).size() == 2);
    CHECK(t.ToJson()["root"]["children"][1]["hotkey"] == "Ctrl+F1");

    nlohmann::json j = t.ToJson();
    j["root"]["children"][1]["hotkey"] = "Escape";
    j["root"]["children"][0]["children"][0]["hotkey"] = "Ctrl+Nonsense";
    std::vector<std::string> warnings;
    CHECK(u.FromJson(j, &warnings));
    CHECK(u.MacrosWithHotkey(C(0x70, true)).empty());
    CHECK(warnings.size() == 2);
}

// What the user sees is translated; what's saved (ToString) never is.
static void TestDisplay()
{
    Localization::SetActive(nullptr);
    CHECK(Display(C(0x70, true, false, true)) == "Ctrl+Shift+F1");
    CHECK(Display(C(kVkXButton1, false, true)) == "Alt+Mouse 4");
    CHECK(Display(Combo{}) == "");
    CHECK(DisplayKeyName(0xE2) == "Key 0xE2");
    CHECK(DisplayKeyName(0x65) == "Num 5");
    CHECK(DisplayKeyName(0xBF) == "/");

    Localization::Catalog de;
    de.Id = "de";
    de.Texts["Ctrl"] = "Strg";
    de.Texts["Shift"] = "Umschalt";
    de.Texts["Mouse 4"] = "Maus 4";
    de.Texts["Page Up"] = "Bild auf";
    de.Texts["Key 0x%02X"] = "Taste 0x%02X";
    Localization::SetActive(&de);
    CHECK(Display(C(0x70, true, false, true)) == "Strg+Umschalt+F1");
    CHECK(Display(C(kVkXButton1, true)) == "Strg+Maus 4");
    CHECK(Display(C(0x21)) == "Bild auf");
    CHECK(DisplayKeyName(0xE2) == "Taste 0xE2");
    CHECK(ToString(C(kVkXButton1, true)) == "Ctrl+Mouse 4"); // the stored form stays English
    CHECK(FromString(ToString(C(0x21, false, false, true))) == C(0x21, false, false, true));
    Localization::SetActive(nullptr);
}

void RunHotkeyTests()
{
    TestNames();
    TestDisplay();
    TestParse();
    TestPack();
    TestRefusal();
    TestOldMouseHotkeys();
    TestTree();
}
