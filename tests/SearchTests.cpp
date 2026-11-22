// Standalone tests for src/Search.cpp (the library's search box).

#include <string>

#include "MacroTree.h"
#include "Search.h"
#include "TestCheck.h"

using namespace MacroTree;

static void TestFold()
{
    CHECK(Search::Fold("Hello World") == "hello world");
    CHECK(Search::Fold("Caf\xC3\xA9") == "cafe");                 // Café
    CHECK(Search::Fold("\xC3\x84rger") == "arger");               // Ärger
    CHECK(Search::Fold("Stra\xC3\x9F" "e") == "strasse");         // Straße
    CHECK(Search::Fold("\xC5\x92uvre") == "oeuvre");              // Œuvre
    CHECK(Search::Fold("\xC3\x97") == "\xC3\x97");                // × kept
    CHECK(Search::Fold("\xE2\x9C\x93 ok") == "\xE2\x9C\x93 ok");  // ✓ kept
    std::vector<size_t> map;
    CHECK(Search::Fold("a\xC3\xA9z", &map) == "aez");
    CHECK(map.size() == 3 && map[0] == 0 && map[1] == 1 && map[2] == 3);
    CHECK(Search::Trim("  x y \t") == "x y");
}

static void TestFindRange()
{
    size_t at = 0, len = 0;
    CHECK(Search::FindRange("Gorseval", "SEV", at, len) && at == 3 && len == 3);
    // "cafe" in "Le Café bleu": the range covers the 2-byte é.
    CHECK(Search::FindRange("Le Caf\xC3\xA9 bleu", "cafe", at, len) && at == 3 && len == 5);
    CHECK(Search::FindRange("Le Caf\xC3\xA9 bleu", "  CAF\xC3\x89 ", at, len) && at == 3 && len == 5);
    CHECK(!Search::FindRange("Gorseval", "xyz", at, len));
    CHECK(!Search::FindRange("Gorseval", "   ", at, len));
    // Ends inside "ss" from ß: the whole ß is taken.
    CHECK(Search::FindRange("Stra\xC3\x9F" "e", "stras", at, len) && at == 0 && len == 6);
}

static void TestRun()
{
    Tree t;
    Node* raids = t.AddFolder(nullptr, "Raids").Target;
    Node* w3    = t.AddFolder(raids, "Wing 3").Target;
    Node* gors  = t.AddMacro(w3, "Gorseval").Target;
    Node* sab   = t.AddMacro(w3, "Sabetha").Target;
    Node* wvw   = t.AddFolder(nullptr, "WvW").Target;
    Node* tag   = t.AddMacro(wvw, "Tag up").Target;
    Node* empty = t.AddFolder(nullptr, "Empty").Target;
    gors->Lines.push_back({EChannel::Squad, "Stack on the egg", "", false});
    sab->Lines.push_back({EChannel::Whisper, "hi", "Some Friend", false});
    tag->Hotkey.Key = 0x70; // F1
    tag->Hotkey.Ctrl = true;

    // Empty query: inactive, everything shows.
    Search::Result r;
    Search::Run(t.Root(), "   ", r);
    CHECK(!r.Active);
    CHECK(r.Shows(gors->Id) && r.Shows(empty->Id));

    // A macro's name: it and the folders above it show; the folders open.
    Search::Run(t.Root(), "gorse", r);
    CHECK(r.Active && r.MacroMatches == 1 && r.FolderMatches == 0);
    CHECK(r.Matches.count(gors->Id) == 1);
    CHECK(r.Shows(gors->Id) && r.Shows(w3->Id) && r.Shows(raids->Id));
    CHECK(r.ForcedOpen(raids->Id) && r.ForcedOpen(w3->Id));
    CHECK(!r.Shows(sab->Id) && !r.Shows(wvw->Id) && !r.Shows(tag->Id) && !r.Shows(empty->Id));

    // Line text, whisper recipient and hotkey.
    Search::Run(t.Root(), "EGG", r);
    CHECK(r.Matches.count(gors->Id) == 1);
    Search::Run(t.Root(), "friend", r);
    CHECK(r.Matches.count(sab->Id) == 1 && r.MacroMatches == 1); // and the last search was cleared
    Search::Run(t.Root(), "ctrl+f1", r);
    CHECK(r.Matches.count(tag->Id) == 1 && r.MacroMatches == 1 && r.ForcedOpen(wvw->Id));

    // A folder's name: it and everything inside show; it isn't forced open.
    Search::Run(t.Root(), "wing", r);
    CHECK(r.FolderMatches == 1 && r.MacroMatches == 0);
    CHECK(r.Matches.count(w3->Id) == 1);
    CHECK(r.Shows(gors->Id) && r.Shows(sab->Id) && r.Inside.count(sab->Id) == 1);
    CHECK(r.ForcedOpen(raids->Id) && !r.ForcedOpen(w3->Id));

    // Nothing matches.
    Search::Run(t.Root(), "zzz", r);
    CHECK(r.Active && r.Matches.empty() && !r.Shows(raids->Id));

    // A folder never matches on its contents' text, only its name.
    CHECK(!Search::NodeMatches(*w3, "egg"));
}

// "Only active here": the filter alone, and together with search text.
static void TestActiveOnly()
{
    Tree t;
    Node* raids = t.AddFolder(nullptr, "Raids").Target;
    Node* w3    = t.AddFolder(raids, "Wing 3").Target;
    Node* gors  = t.AddMacro(w3, "Gorseval").Target; // PvE only
    Node* sab   = t.AddMacro(w3, "Sabetha").Target;  // everywhere
    Node* wvw   = t.AddFolder(nullptr, "WvW").Target;
    Node* tag   = t.AddMacro(wvw, "Tag up").Target;  // WvW, map 38 only
    Node* empty = t.AddFolder(nullptr, "Empty").Target;
    gors->GameModes = GM_PvE;
    tag->GameModes  = GM_WvW;
    tag->MapIds     = {38};

    Search::Where wvw38;
    wvw38.ActiveOnly = true;
    wvw38.Mode       = GM_WvW;
    wvw38.MapId      = 38;

    // The filter alone: only active macros, with their folders opened.
    Search::Result r;
    Search::Run(t.Root(), "", wvw38, r);
    CHECK(r.Active && r.Matches.empty() && r.ShownMacros == 2);
    CHECK(r.Shows(sab->Id) && r.Shows(tag->Id) && !r.Shows(gors->Id));
    CHECK(r.Shows(raids->Id) && r.Shows(w3->Id) && r.Shows(wvw->Id) && !r.Shows(empty->Id));
    CHECK(r.ForcedOpen(raids->Id) && r.ForcedOpen(w3->Id) && r.ForcedOpen(wvw->Id));

    // Another map in WvW: "Tag up" (map 38 only) isn't active.
    Search::Where wvw95 = wvw38;
    wvw95.MapId = 95;
    Search::Run(t.Root(), "   ", wvw95, r);
    CHECK(r.ShownMacros == 1 && !r.Shows(tag->Id) && !r.Shows(wvw->Id));

    // With text: a matched folder shows only its active macros, and is opened.
    Search::Run(t.Root(), "wing", wvw38, r);
    CHECK(r.FolderMatches == 1 && r.Matches.count(w3->Id) == 1);
    CHECK(r.Shows(sab->Id) && !r.Shows(gors->Id) && r.ShownMacros == 1);
    CHECK(r.ForcedOpen(w3->Id) && r.ForcedOpen(raids->Id));
    CHECK(!r.Shows(tag->Id));

    // A matching macro that isn't active here isn't shown.
    Search::Where pve = wvw38;
    pve.Mode = GM_PvE;
    pve.MapId = 15;
    Search::Run(t.Root(), "tag", pve, r);
    CHECK(r.Active && r.Visible.empty() && r.MacroMatches == 0);
    Search::Run(t.Root(), "gorse", pve, r);
    CHECK(r.MacroMatches == 1 && r.Shows(gors->Id) && r.Shows(raids->Id));

    // No game mode (character select, loading): only macros active everywhere.
    Search::Where none;
    none.ActiveOnly = true;
    Search::Run(t.Root(), "", none, r);
    CHECK(r.ShownMacros == 1 && r.Shows(sab->Id));

    // Filter off and no text: inactive, the whole tree.
    Search::Run(t.Root(), "", Search::Where{}, r);
    CHECK(!r.Active && r.Shows(gors->Id) && r.Shows(empty->Id));
}

void RunSearchTests()
{
    TestFold();
    TestFindRange();
    TestRun();
    TestActiveOnly();
}
