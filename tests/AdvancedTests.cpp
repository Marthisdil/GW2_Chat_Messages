// Tests for Advanced (the features behind the "Advanced options" checkbox).

#include <nlohmann/json.hpp>

#include "Advanced.h"
#include "MacroTree.h"
#include "TestCheck.h"

using namespace MacroTree;

static void TestUsesAndBlocked()
{
    Tree t;
    Node* folder = t.AddFolder(nullptr, "Raid").Target;
    Node* plain  = t.AddMacro(folder, "Plain").Target;
    Node* linked = t.AddMacro(folder, "Linked").Target;
    linked->LinkFile = "callouts.txt";

    CHECK(!Advanced::Uses(*folder));
    CHECK(!Advanced::Uses(*plain));
    CHECK(Advanced::Uses(*linked));

    // Off: only the macro that uses an advanced feature is blocked.
    CHECK(Advanced::Blocked(*linked, false));
    CHECK(!Advanced::Blocked(*plain, false));
    CHECK(!Advanced::Blocked(*folder, false));
    // On: nothing is.
    CHECK(!Advanced::Blocked(*linked, true));
    CHECK(!Advanced::Blocked(*plain, true));

    // Blocking keeps the macro's settings: the hotkey stays, for the duplicate check.
    linked->Hotkey.Key = 0x70; // F1
    CHECK(Advanced::Blocked(*linked, false));
    CHECK(!linked->Hotkey.Empty());
    CHECK(t.MacrosWithHotkey(linked->Hotkey).size() == 1);
}

static void TestAnyUses()
{
    Tree t;
    CHECK(!Advanced::AnyUses(t));
    Node* deep = t.AddFolder(nullptr, "A").Target;
    deep = t.AddFolder(deep, "B").Target;
    Node* m = t.AddMacro(deep, "M").Target;
    CHECK(!Advanced::AnyUses(t));
    m->LinkFile = "x.txt";
    CHECK(Advanced::AnyUses(t)); // found in a sub-folder
    m->LinkFile.clear();
    CHECK(!Advanced::AnyUses(t));
}

static void TestFromSettings()
{
    Tree plain;
    plain.AddMacro(nullptr, "M");
    Tree linked;
    linked.AddMacro(nullptr, "L").Target->LinkFile = "x.txt";

    // Nothing saved: off, unless the library already has a link (an update keeps it working).
    CHECK(!Advanced::FromSettings(nlohmann::json::object(), plain));
    CHECK(Advanced::FromSettings(nlohmann::json::object(), linked));
    CHECK(!Advanced::FromSettings(nlohmann::json(), plain)); // no settings file at all
    // A saved choice always wins.
    CHECK(Advanced::FromSettings({{Advanced::kSetting, true}}, plain));
    CHECK(!Advanced::FromSettings({{Advanced::kSetting, false}}, linked));
    // Anything but true or false counts as not saved.
    CHECK(!Advanced::FromSettings({{Advanced::kSetting, "yes"}}, plain));
    CHECK(Advanced::FromSettings({{Advanced::kSetting, 1}}, linked));
}

void RunAdvancedTests()
{
    TestUsesAndBlocked();
    TestAnyUses();
    TestFromSettings();
}
