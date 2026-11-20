// Standalone tests for the macro filters in src/MacroTree.cpp (game mode + map ids).

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
    CHECK(!IsActive(folder, GM_PvE, 15));            // folders are never active
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
}
