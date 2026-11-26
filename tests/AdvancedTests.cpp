// Tests for Advanced (the features behind the "Advanced options" checkbox).

#include <nlohmann/json.hpp>

#include "Advanced.h"
#include "MacroTree.h"
#include "TestCheck.h"

using namespace MacroTree;

static void TestBlockedAndCooldown()
{
    Tree t;
    Node* folder = t.AddFolder(nullptr, "Raid").Target;
    Node* plain  = t.AddMacro(folder, "Plain").Target;
    Node* linked = t.AddMacro(folder, "Linked").Target;
    Node* slow   = t.AddMacro(folder, "Slow").Target;
    linked->LinkFile = "callouts.txt";
    slow->CooldownSec = 30;

    CHECK(!Advanced::Uses(*folder));
    CHECK(!Advanced::Uses(*plain));
    CHECK(Advanced::Uses(*linked) && Advanced::Blocks(*linked) && !Advanced::HasCooldown(*linked));
    CHECK(Advanced::Uses(*slow) && !Advanced::Blocks(*slow) && Advanced::HasCooldown(*slow));

    // Off: only a linked macro is blocked; a cooldown is ignored instead.
    CHECK(Advanced::Blocked(*linked, false));
    CHECK(!Advanced::Blocked(*slow, false));
    CHECK(!Advanced::Blocked(*plain, false));
    CHECK(!Advanced::Blocked(*folder, false));
    CHECK(Advanced::CooldownSec(*slow, false) == 0);
    // On: nothing is blocked, and the cooldown applies.
    CHECK(!Advanced::Blocked(*linked, true));
    CHECK(Advanced::CooldownSec(*slow, true) == 30);
    CHECK(Advanced::CooldownSec(*plain, true) == 0);

    // Nothing is changed while it's off: the cooldown and the hotkey stay set.
    linked->Hotkey.Key = 0x70; // F1
    CHECK(Advanced::Blocked(*linked, false));
    CHECK(slow->CooldownSec == 30);
    CHECK(!linked->Hotkey.Empty());
    CHECK(t.MacrosWithHotkey(linked->Hotkey).size() == 1);
}

static void TestCount()
{
    Tree t;
    CHECK(!Advanced::AnyUses(t));
    Node* deep = t.AddFolder(nullptr, "A").Target;
    deep = t.AddFolder(deep, "B").Target;
    Node* m = t.AddMacro(deep, "M").Target;
    Node* n = t.AddMacro(nullptr, "N").Target;
    CHECK(!Advanced::AnyUses(t));
    m->LinkFile = "x.txt";
    n->CooldownSec = 5;
    const Advanced::InUse in = Advanced::Count(t);
    CHECK(in.Linked == 1 && in.Cooldowns == 1 && in.Any()); // found in a sub-folder too
    m->LinkFile.clear();
    n->CooldownSec = 0;
    CHECK(!Advanced::AnyUses(t));
}

static void TestFromSettings()
{
    // Off unless saved as true: the default for everyone, whatever the library holds.
    CHECK(!Advanced::FromSettings(nlohmann::json::object()));
    CHECK(!Advanced::FromSettings(nlohmann::json()));          // no settings file at all
    CHECK(Advanced::FromSettings({{Advanced::kSetting, true}}));
    CHECK(!Advanced::FromSettings({{Advanced::kSetting, false}}));
    CHECK(!Advanced::FromSettings({{Advanced::kSetting, "yes"}})); // anything but true or false: off
    CHECK(!Advanced::FromSettings({{Advanced::kSetting, 1}}));
}

void RunAdvancedTests()
{
    TestBlockedAndCooldown();
    TestCount();
    TestFromSettings();
}
