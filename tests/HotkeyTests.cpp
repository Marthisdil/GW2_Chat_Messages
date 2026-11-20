// Standalone tests for src/Hotkey.cpp and the hotkey parts of src/MacroTree.cpp.

#include <string>

#include <nlohmann/json.hpp>

#include "Hotkey.h"
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
    CHECK(Refusal(C(kVkXButton1)).empty());   // Mouse 4
    CHECK(Refusal(C('1', true)).empty());
    CHECK(!Refusal(Combo{}).empty());
    CHECK(!Refusal(C(0x01)).empty());         // left mouse
    CHECK(!Refusal(C(0x02, true)).empty());   // right mouse, even with Ctrl
    CHECK(!Refusal(C(kVkEscape, true)).empty());
    CHECK(!Refusal(C(0x10)).empty());         // Shift alone
    CHECK(!Refusal(C(0xA2, true)).empty());   // Left Ctrl as the key
    CHECK(!Refusal(C(0x09)).empty());         // Tab alone
    CHECK(Refusal(C(0x09, true)).empty());    // Ctrl+Tab is allowed
    CHECK(!Refusal(C(0x0D)).empty());         // Enter alone
    CHECK(!Refusal(C(0x20)).empty());         // Space alone
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

void RunHotkeyTests()
{
    TestNames();
    TestParse();
    TestPack();
    TestRefusal();
    TestTree();
}
