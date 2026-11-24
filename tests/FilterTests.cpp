// Standalone tests for the macro filters in src/MacroTree.cpp (game mode + map ids).

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "MacroTree.h"
#include "TestCheck.h"

using namespace MacroTree;

static Node M(uint8_t modes, std::vector<int> maps = {})
{
    Node n;
    n.Type = ENodeType::Macro;
    n.GameModes = modes;
    n.MapIds = std::move(maps);
    return n;
}

static void TestGameModeOfMapType()
{
    CHECK(GameModeOfMapType(5) == GM_PvE);   // Public
    CHECK(GameModeOfMapType(4) == GM_PvE);   // Instance (fractals, raids, story)
    CHECK(GameModeOfMapType(16) == GM_PvE);  // PublicMini
    CHECK(GameModeOfMapType(2) == GM_PvP);
    CHECK(GameModeOfMapType(6) == GM_PvP);   // Tournament
    CHECK(GameModeOfMapType(9) == GM_WvW);   // Eternal Battlegrounds
    CHECK(GameModeOfMapType(15) == GM_WvW);  // Edge of the Mists
    CHECK(GameModeOfMapType(18) == GM_WvW);  // Armistice Bastion
    CHECK(GameModeOfMapType(0) == GM_None);  // redirect
    CHECK(GameModeOfMapType(1) == GM_None);  // character creation
    CHECK(GameModeOfMapType(99) == GM_None);
}

static void TestIsActive()
{
    CHECK(IsActive(M(GM_All), GM_WvW, 38));
    CHECK(IsActive(M(GM_All), GM_None, 0));     // "everywhere" macros match even when unknown
    CHECK(IsActive(M(GM_None), GM_PvP, 1));     // no mode set behaves like all modes
    CHECK(IsActive(M(GM_WvW), GM_WvW, 38));
    CHECK(!IsActive(M(GM_WvW), GM_PvE, 15));
    CHECK(!IsActive(M(GM_WvW), GM_None, 0));
    CHECK(IsActive(M(GM_PvE | GM_PvP), GM_PvP, 350));

    CHECK(IsActive(M(GM_All, {15, 18}), GM_PvE, 18));
    CHECK(!IsActive(M(GM_All, {15, 18}), GM_PvE, 50));
    CHECK(!IsActive(M(GM_WvW, {15}), GM_PvE, 15));  // both filters must match

    Node folder;
    CHECK(IsActive(folder, GM_PvE, 15));             // a folder without a filter: everywhere
}

static void TestFolderFilters()
{
    // Raids [PvE] > Wing [maps 1, 2] > macro [maps 2, 3]; Roam [WvW] > macro [PvE]
    Tree t;
    Node* raids = t.AddFolder(nullptr, "Raids").Target;
    raids->GameModes = GM_PvE;
    Node* wing = t.AddFolder(raids, "Wing").Target;
    wing->MapIds = {1, 2};
    Node* m = t.AddMacro(wing, "Go").Target;
    m->MapIds = {3, 2};
    Node* free = t.AddMacro(raids, "Free").Target;
    Node* top = t.AddMacro(nullptr, "Top").Target;

    CHECK(HasFilter(*raids) && HasFilter(*wing) && !HasFilter(*free) && !HasFilter(*top));
    // The filters add up: only PvE, only map 2.
    CHECK(IsActive(*m, GM_PvE, 2));
    CHECK(!IsActive(*m, GM_PvE, 3));  // the macro allows 3, its folder doesn't
    CHECK(!IsActive(*m, GM_PvE, 1));  // the folder allows 1, the macro doesn't
    CHECK(!IsActive(*m, GM_WvW, 2));  // the top folder is PvE only
    CHECK(!IsActive(*m, GM_None, 2)); // unknown mode: a mode limit somewhere means no
    const Reach r = ReachOf(*m);
    CHECK(r.Modes == GM_PvE && !r.AnyMap && r.Maps == std::vector<int>{2} && !r.Never());
    // A macro with no filter of its own follows its folder.
    CHECK(IsActive(*free, GM_PvE, 99) && !IsActive(*free, GM_PvP, 99));
    CHECK(IsActive(*top, GM_PvP, 99));
    // Folders are active where they and the folders above allow.
    CHECK(IsActive(*raids, GM_PvE, 5) && !IsActive(*raids, GM_WvW, 5));
    CHECK(IsActive(*wing, GM_PvE, 1) && !IsActive(*wing, GM_PvE, 3));

    // Never active: no mode in common, or no map in common.
    Node* roam = t.AddFolder(nullptr, "Roam").Target;
    roam->GameModes = GM_WvW;
    Node* pve = t.AddMacro(roam, "PvE thing").Target;
    pve->GameModes = GM_PvE;
    CHECK(NeverActive(*pve) && !NeverActive(*roam));
    CHECK(!IsActive(*pve, GM_PvE, 1) && !IsActive(*pve, GM_WvW, 1));
    m->MapIds = {3};
    CHECK(NeverActive(*m)); // folder maps 1, 2; macro map 3
    m->MapIds = {2, 2, 0, -4}; // duplicates and bad ids don't matter
    CHECK(!NeverActive(*m) && IsActive(*m, GM_PvE, 2));
    wing->MapIds = {0};
    CHECK(NeverActive(*wing)); // a list with no real id never matches, as for macros

    // Hotkey clashes take the folders into account.
    wing->MapIds = {1, 2};
    m->MapIds.clear();
    CHECK(!FiltersOverlap(*m, *pve));  // Raids (PvE) vs Roam (WvW)
    CHECK(FiltersOverlap(*m, *free));  // both PvE, m on maps 1-2, free anywhere
    Node* elsewhere = t.AddMacro(raids, "Elsewhere").Target;
    elsewhere->MapIds = {7};
    CHECK(!FiltersOverlap(*m, *elsewhere)); // maps 1-2 vs map 7
    CHECK(!FiltersOverlap(*pve, *top));     // a macro that's never active clashes with nothing

    // Moving a macro into a filtered folder makes it follow that folder.
    CHECK(IsActive(*top, GM_PvP, 1));
    CHECK(t.Move(*top, *roam, EDrop::Into).Ok);
    CHECK(!IsActive(*top, GM_PvP, 1) && IsActive(*top, GM_WvW, 1));
}

static void TestFolderFilterJson()
{
    Tree t;
    Node* plain = t.AddFolder(nullptr, "Plain").Target;
    Node* roam = t.AddFolder(nullptr, "Roam").Target;
    roam->GameModes = GM_WvW | GM_PvP;
    roam->MapIds = {38, 95};
    t.AddMacro(roam, "Go");
    const nlohmann::json j = t.ToJson();
    const nlohmann::json& kids = j["root"]["children"];
    CHECK(!kids[0].contains("game_modes") && !kids[0].contains("map_ids")); // no filter: as before
    CHECK(kids[1]["game_modes"] == nlohmann::json::array({"wvw", "pvp"}));
    CHECK(kids[1]["map_ids"] == nlohmann::json::array({38, 95}));

    Tree back;
    CHECK(back.FromJson(nlohmann::json::parse(j.dump())));
    const Node* b = back.Find(roam->Id);
    CHECK(b && b->GameModes == (GM_WvW | GM_PvP) && b->MapIds == std::vector<int>({38, 95}));
    CHECK(back.Find(plain->Id) && !HasFilter(*back.Find(plain->Id)));
    CHECK(b && !b->Children.empty() && !IsActive(*b->Children[0], GM_PvE, 38));

    // A file can't bring more maps than the editor allows (SEC-033), on a folder or a macro.
    nlohmann::json big = j;
    nlohmann::json many = nlohmann::json::array();
    for (int i = 1; i <= static_cast<int>(kMaxMapIds) + 20; i++) many.push_back(i);
    big["root"]["children"][1]["map_ids"] = many;
    big["root"]["children"][1]["children"][0]["map_ids"] = many;
    Tree capped;
    std::vector<std::string> warnings;
    CHECK(capped.FromJson(big, &warnings));
    const Node* cf = capped.Find(roam->Id);
    CHECK(cf && cf->MapIds.size() == kMaxMapIds && cf->MapIds.front() == 1);
    CHECK(cf && !cf->Children.empty() && cf->Children[0]->MapIds.size() == kMaxMapIds);
    CHECK(warnings.size() == 2);
}

static void TestOverlap()
{
    CHECK(FiltersOverlap(M(GM_All), M(GM_WvW)));
    CHECK(!FiltersOverlap(M(GM_WvW), M(GM_PvE)));
    CHECK(!FiltersOverlap(M(GM_PvE, {15}), M(GM_PvE, {18})));
    CHECK(FiltersOverlap(M(GM_PvE, {15, 18}), M(GM_PvE, {18})));
    CHECK(FiltersOverlap(M(GM_PvE), M(GM_PvE, {18})));
    CHECK(FiltersOverlap(M(GM_None), M(GM_PvP)));
}

void RunFilterTests()
{
    TestGameModeOfMapType();
    TestIsActive();
    TestOverlap();
    TestFolderFilters();
    TestFolderFilterJson();
}
