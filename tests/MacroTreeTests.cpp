// Standalone tests for src/MacroTree.cpp (no Nexus, no ImGui).
// Build and run: tools\run_tests.ps1 (CI runs it too).

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "MacroTree.h"
#include "TestCheck.h"

using namespace MacroTree;
using nlohmann::json;

// Builds a chain of nested folders aCount deep under aParent and returns the deepest.
static Node* Chain(Tree& t, Node* aParent, int aCount, const std::string& aPrefix = "L")
{
    Node* cur = aParent;
    for (int i = 1; i <= aCount; i++)
    {
        Result r = t.AddFolder(cur, aPrefix + std::to_string(i));
        if (!r.Ok) return nullptr;
        cur = r.Target;
    }
    return cur;
}

static void TestDepthLimit()
{
    Tree t;
    Node* deepest = Chain(t, nullptr, kMaxFolderDepth);
    CHECK(deepest != nullptr);
    if (!deepest) return;
    CHECK(Tree::FolderLevel(*deepest) == kMaxFolderDepth);

    Result tooDeep = t.AddFolder(deepest, "Seven");
    CHECK(!tooDeep.Ok);
    CHECK(!tooDeep.Error.empty());

    Result macro = t.AddMacro(deepest, "Deep macro"); // macros are fine at any level
    CHECK(macro.Ok);
    CHECK(Tree::FolderLevel(*macro.Target) == kMaxFolderDepth);

    // Root-level macro is allowed.
    CHECK(t.AddMacro(nullptr, "Root macro").Ok);
}

static void TestMoveDepth()
{
    Tree t;
    Node* a = Chain(t, nullptr, 3, "A"); // A1/A2/A3
    Node* b = t.AddFolder(nullptr, "B").Target;
    Node* b2 = t.AddFolder(b, "B2").Target;
    Node* b3 = t.AddFolder(b2, "B3").Target; // B is 3 levels high

    // B (height 3) into A3 (level 3) -> 6: allowed.
    CHECK(t.CanMove(*b, *a, EDrop::Into).Ok);
    // One more level would make it 7.
    t.AddFolder(b3, "B4");
    CHECK(!t.CanMove(*b, *a, EDrop::Into).Ok);
    // Next to A3 (level 3, so B starts at level 3): 3 + 4 - 1 = 6 -> allowed.
    CHECK(t.CanMove(*b, *a, EDrop::After).Ok);
    Result moved = t.Move(*b, *a, EDrop::After);
    CHECK(moved.Ok);
    CHECK(b->Parent == a->Parent);
}

static void TestMoveIntoSelf()
{
    Tree t;
    Node* a = t.AddFolder(nullptr, "A").Target;
    Node* child = t.AddFolder(a, "Child").Target;
    CHECK(!t.CanMove(*a, *a, EDrop::Into).Ok);
    CHECK(!t.CanMove(*a, *child, EDrop::Into).Ok);
    CHECK(!t.CanMove(*a, *child, EDrop::Before).Ok); // Before child = inside a
    Node* macro = t.AddMacro(nullptr, "M").Target;
    CHECK(!t.CanMove(*a, *macro, EDrop::Into).Ok); // can't drop into a macro
    CHECK(t.CanMove(*a, *a, EDrop::Before).Ok);    // no-op
}

static void TestMoveOrder()
{
    Tree t;
    Node* m1 = t.AddMacro(nullptr, "One").Target;
    Node* m2 = t.AddMacro(nullptr, "Two").Target;
    Node* m3 = t.AddMacro(nullptr, "Three").Target;
    CHECK(t.Move(*m3, *m1, EDrop::Before).Ok);
    CHECK(t.Root().Children[0].get() == m3);
    CHECK(t.Root().Children[1].get() == m1);
    CHECK(t.Root().Children[2].get() == m2);
    CHECK(t.Move(*m3, *m2, EDrop::After).Ok);
    CHECK(t.Root().Children[2].get() == m3);

    Node* f = t.AddFolder(nullptr, "F").Target;
    CHECK(t.Move(*m1, *f, EDrop::Into).Ok);
    CHECK(m1->Parent == f);
    CHECK(f->Children.size() == 1);
    CHECK(t.Root().Children.size() == 3);

    CHECK(t.Shift(*m3, -1));
    CHECK(!t.Shift(*t.Root().Children[0], -1));
}

static void TestNames()
{
    Tree t;
    CHECK(t.AddFolder(nullptr, "Raids").Ok);
    CHECK(!t.AddFolder(nullptr, "raids").Ok);      // case-insensitive duplicate
    CHECK(t.AddMacro(nullptr, "Raids").Ok);        // a macro may share a folder's name
    CHECK(!t.AddMacro(nullptr, "   ").Ok);         // empty after trimming
    Node* m = t.AddMacro(nullptr, "  Hello\tworld  ").Target;
    CHECK(m && m->Name == "Hello world");
    CHECK(t.Rename(*m, "hello WORLD").Ok);         // renaming to itself in other case is fine
    CHECK(!t.Rename(*m, "raids").Ok);              // clashes with the macro "Raids"

    std::string longName(150, 'x');
    CHECK(Tree::CleanName(longName).size() == kMaxNameLength);
    // A 2-byte UTF-8 char straddling the cut must not be split.
    std::string utf = std::string(kMaxNameLength - 1, 'a') + "\xC3\xA9" + "zz";
    std::string cut = Tree::CleanName(utf);
    CHECK(cut.size() == kMaxNameLength - 1);

    // Moving into a folder that already has that name is refused.
    Node* f = t.AddFolder(nullptr, "Box").Target;
    t.AddMacro(f, "Raids");
    Node* rootRaids = nullptr;
    for (auto& c : t.Root().Children) if (c->IsMacro() && c->Name == "Raids") rootRaids = c.get();
    CHECK(rootRaids && !t.CanMove(*rootRaids, *f, EDrop::Into).Ok);
}

static void TestDeleteAndCount()
{
    Tree t;
    Node* a = t.AddFolder(nullptr, "A").Target;
    t.AddMacro(a, "m1");
    Node* b = t.AddFolder(a, "B").Target;
    t.AddMacro(b, "m2");
    size_t folders = 0, macros = 0;
    CHECK(Tree::CountDescendants(*a, &folders, &macros) == 3);
    CHECK(folders == 1 && macros == 2);
    const uint64_t bId = b->Id;
    CHECK(t.Delete(*a).Ok);
    CHECK(t.Root().Children.empty());
    CHECK(t.Find(bId) == nullptr);
    CHECK(!t.Delete(t.Root()).Ok);
}

static void TestSort()
{
    Tree t;
    t.AddMacro(nullptr, "zeta");
    t.AddFolder(nullptr, "beta");
    t.AddMacro(nullptr, "Alpha");
    t.AddFolder(nullptr, "Alpha");
    t.SortChildren(t.Root(), true);
    CHECK(t.Root().Children[0]->IsFolder() && t.Root().Children[0]->Name == "Alpha");
    CHECK(t.Root().Children[1]->IsFolder() && t.Root().Children[1]->Name == "beta");
    CHECK(t.Root().Children[2]->IsMacro() && t.Root().Children[2]->Name == "Alpha");
    CHECK(t.Root().Children[3]->IsMacro() && t.Root().Children[3]->Name == "zeta");
}

static void TestJsonRoundTrip()
{
    Tree t;
    Node* f = t.AddFolder(nullptr, "Expansion").Target;
    Node* g = t.AddFolder(f, "Map").Target;
    Node* m = t.AddMacro(g, "Call").Target;
    g->Expanded = false; // after AddMacro, which opens the folder it adds to
    m->Lines.push_back({EChannel::Squad, "Stack on tag", "", true});
    m->Lines.push_back({EChannel::Whisper, "hi", "Some Name", false});
    m->Lines.push_back({EChannel::Guild3, "\xE2\x9C\x93 done", "", false});
    m->GameModes = GM_WvW;
    m->MapIds = {38, 95};
    m->LinkFile = "calls.txt";

    const json j = t.ToJson();
    Tree u;
    std::vector<std::string> warnings;
    CHECK(u.FromJson(j, &warnings));
    CHECK(warnings.empty());
    CHECK(u.ToJson() == j);

    Node* m2 = u.Find(m->Id);
    CHECK(m2 != nullptr);
    CHECK(m2 && m2->Lines.size() == 3);
    CHECK(m2 && m2->Lines[0].Channel == EChannel::Squad && m2->Lines[0].SquadBroadcast);
    CHECK(m2 && m2->Lines[1].WhisperTo == "Some Name");
    CHECK(m2 && m2->Parent && !m2->Parent->Expanded);
    CHECK(m2 && m2->Parent->Parent && m2->Parent->Parent->Parent == &u.Root());
}

static void TestJsonDamaged()
{
    Tree t;
    CHECK(!t.FromJson(json::array()));
    CHECK(!t.FromJson(json{{"format", 1}}));

    const json j = json::parse(R"({
        "format": 1,
        "root": { "children": [
            {"type": "folder", "id": "00000000000000aa", "name": "Dup", "children": [
                {"type": "macro", "id": "00000000000000aa", "name": "Same id"},
                {"type": "macro", "name": "No id", "lines": [{"channel": "nope", "message": 5}]}
            ]},
            {"type": "folder", "name": "dup"},
            {"type": "weird"},
            42,
            {"type": "macro", "id": "zz", "name": "", "game_modes": []}
        ]}
    })");
    Tree u;
    std::vector<std::string> warnings;
    CHECK(u.FromJson(j, &warnings));
    CHECK(!warnings.empty());
    CHECK(u.Root().Children.size() == 3);
    const Node& dup = *u.Root().Children[0];
    const Node& dup2 = *u.Root().Children[1];
    CHECK(dup.Name == "Dup" && dup2.Name == "dup (2)");
    CHECK(dup.Children[0]->Id != dup.Id && dup.Children[0]->Id != 0);
    CHECK(dup.Children[1]->Id != 0);
    CHECK(dup.Children[1]->Lines.size() == 1 && dup.Children[1]->Lines[0].Channel == EChannel::Current);
    CHECK(dup.Children[1]->Lines[0].Message.empty());
    const Node& unnamed = *u.Root().Children[2];
    CHECK(unnamed.Name == "New macro" && unnamed.GameModes == GM_All);

    // A wrongly typed "expanded" (a crafted import or paste) must not throw: the folder opens,
    // as when the key is missing. In the tree and in the trash bin.
    const json badExpanded = json::parse(R"({
        "format": 1,
        "root": { "children": [
            {"type": "folder", "name": "Str", "expanded": "yes"},
            {"type": "folder", "name": "Num", "expanded": 0},
            {"type": "folder", "name": "Null", "expanded": null},
            {"type": "folder", "name": "Obj", "expanded": {"a": false}},
            {"type": "folder", "name": "Arr", "expanded": [false]},
            {"type": "folder", "name": "Closed", "expanded": false},
            {"type": "folder", "name": "Missing"}
        ]},
        "trash": [
            {"item": {"type": "folder", "name": "Binned", "expanded": "no"}, "from_ids": [], "from_names": []}
        ]
    })");
    Tree e;
    bool threw = false;
    bool parsed = false;
    try { parsed = e.FromJson(badExpanded, &warnings); }
    catch (...) { threw = true; }
    CHECK(!threw);
    CHECK(parsed);
    CHECK(e.Root().Children.size() == 7);
    if (e.Root().Children.size() == 7)
    {
        for (size_t i = 0; i < 7; i++)
            CHECK(e.Root().Children[i]->Expanded == (e.Root().Children[i]->Name != "Closed"));
    }
    CHECK(e.Trash().size() == 1 && e.Trash()[0].Item->Expanded);

    // Deeper than the limit on disk: kept, with a warning.
    json deep = json::parse(R"({"format":1,"root":{"children":[]}})");
    json* cur = &deep["root"];
    for (int i = 0; i < kMaxFolderDepth + 2; i++)
    {
        (*cur)["children"] = json::array({json{{"type", "folder"}, {"name", "D" + std::to_string(i)}}});
        cur = &(*cur)["children"][0];
    }
    Tree d;
    warnings.clear();
    CHECK(d.FromJson(deep, &warnings));
    CHECK(Tree::FolderHeight(*d.Root().Children[0]) == kMaxFolderDepth + 2);
    CHECK(!warnings.empty());
}

// {"format":1,"root":{"children":[...]}} with one {"type","name"} entry per item (and an id, if asked).
static json Library(const std::vector<std::pair<std::string, std::string>>& aItems, bool aIds = false)
{
    json children = json::array();
    for (size_t i = 0; i < aItems.size(); i++)
    {
        json c = {{"type", aItems[i].first}, {"name", aItems[i].second}};
        if (aIds)
        {
            char id[17];
            std::snprintf(id, sizeof(id), "%016llx", static_cast<unsigned long long>(i + 1));
            c["id"] = id;
        }
        children.push_back(std::move(c));
    }
    return {{"format", 1}, {"root", {{"children", std::move(children)}}}};
}

static void TestDuplicateNames()
{
    // A repeated name gets the first free number, skipping names any sibling of the same type
    // holds (before or after it), ignoring case. A folder and a macro may share a name.
    {
        Tree u;
        std::vector<std::string> warnings;
        CHECK(u.FromJson(Library({{"macro", "x"}, {"macro", "X"}, {"macro", "x (2)"}, {"folder", "x"},
                                  {"macro", "x"}, {"folder", "X"}}), &warnings));
        const auto& c = u.Root().Children;
        CHECK(c.size() == 6);
        CHECK(c.size() == 6 && c[0]->Name == "x" && c[1]->Name == "X (3)" && c[2]->Name == "x (2)");
        CHECK(c.size() == 6 && c[3]->Name == "x" && c[4]->Name == "x (4)" && c[5]->Name == "X (2)");
        CHECK(std::find(warnings.begin(), warnings.end(), "Renamed duplicate macro \"X\" to \"X (3)\".") != warnings.end());
    }

    // Thousands of copies of one name (a crafted import): each gets the next number, without
    // rescanning every sibling for every candidate (that took cubic time and froze the game).
    {
        const size_t kCopies = 20000;
        Tree u;
        CHECK(u.FromJson(Library(std::vector<std::pair<std::string, std::string>>(kCopies, {"macro", "a"}), true)));
        const auto& c = u.Root().Children;
        CHECK(c.size() == kCopies);
        bool numbered = c.size() == kCopies && c[0]->Name == "a";
        for (size_t i = 1; i < c.size() && numbered; i++) numbered = c[i]->Name == "a (" + std::to_string(i + 1) + ")";
        CHECK(numbered);
    }

    // A name too long for the number: the cut to kMaxNameLength dropped it, so every candidate was
    // taken and the loop never ended. What the cut left is still used while it's new; then the name
    // is cut shorter so all of " (n)" fits.
    {
        const std::string full(kMaxNameLength, 'n');
        Tree u;
        CHECK(u.FromJson(Library({{"macro", full}, {"macro", full}, {"macro", full}})));
        const auto& c = u.Root().Children;
        CHECK(c.size() == 3 && c[0]->Name == full);
        CHECK(c.size() == 3 && c[1]->Name == std::string(kMaxNameLength - 4, 'n') + " (2)");
        CHECK(c.size() == 3 && c[2]->Name == std::string(kMaxNameLength - 4, 'n') + " (3)");
    }
    {
        const std::string name(kMaxNameLength - 2, 'e'); // room for " (" only
        Tree u;
        CHECK(u.FromJson(Library({{"macro", name}, {"macro", name}, {"macro", name}})));
        const auto& c = u.Root().Children;
        CHECK(c.size() == 3 && c[1]->Name == name + " (");
        CHECK(c.size() == 3 && c[2]->Name == std::string(kMaxNameLength - 4, 'e') + " (2)");
    }
    {
        const std::string name(kMaxNameLength - 3, 's'); // room for " (" and one digit
        Tree u;
        CHECK(u.FromJson(Library(std::vector<std::pair<std::string, std::string>>(11, {"folder", name}))));
        const auto& c = u.Root().Children;
        CHECK(c.size() == 11);
        bool cut = c.size() == 11;
        for (size_t i = 1; i < 9 && cut; i++) cut = c[i]->Name == name + " (" + std::to_string(i + 1);
        CHECK(cut);
        CHECK(c.size() == 11 && c[9]->Name == name + " (1");
        CHECK(c.size() == 11 && c[10]->Name == std::string(kMaxNameLength - 4, 's') + " (2)");
    }
    {
        std::string name = "a";
        for (int i = 0; i < 49; i++) name += "\xC3\xA9"; // 99 bytes
        name += "b";                                     // 100
        std::string kept = "a";
        for (int i = 0; i < 47; i++) kept += "\xC3\xA9"; // the cut backs up off the split "é"
        Tree u;
        CHECK(u.FromJson(Library({{"macro", name}, {"macro", name}})));
        const auto& c = u.Root().Children;
        CHECK(c.size() == 2 && c[1]->Name == kept + " (2)");
    }
}

// aTree.FromJson on a one-macro file whose "format" is aFormat (JSON text; "" leaves it out).
// aThrew is set if FromJson threw.
static bool ReadWithFormat(Tree& aTree, const std::string& aFormat, std::vector<std::string>* aWarnings, bool& aThrew)
{
    const std::string format = aFormat.empty() ? std::string() : R"("format":)" + aFormat + ",";
    const json j = json::parse("{" + format + R"("root":{"children":[{"type":"macro","id":"00000000000000aa","name":"M"}]}})");
    aThrew = false;
    try { return aTree.FromJson(j, aWarnings); }
    catch (...) { aThrew = true; return false; }
}

// "format" of any type never throws. Types value() used to throw on (null, text, an object or
// array) aren't a library: FromJson fails and leaves the tree as it was. Numbers and true/false
// read as before (as an int: above 1 is newer, with a warning); missing is 0.
static void TestJsonFormat()
{
    std::vector<std::string> w;
    bool threw = true;
    for (const char* bad : {"null", "\"1\"", "{}", "{\"v\":1}", "[1]"})
    {
        Tree t;
        t.AddFolder(nullptr, "Mine");
        w.clear();
        CHECK(!ReadWithFormat(t, bad, &w, threw) && !threw);
        CHECK(w.empty() && t.Root().Children.size() == 1 && t.Root().Children[0]->Name == "Mine");
        Tree u;
        CHECK(!ReadWithFormat(u, bad, nullptr, threw) && !threw);
    }
    for (const char* known : {"", "0", "1", "-1", "1.5", "true"})
    {
        Tree t;
        w.clear();
        CHECK(ReadWithFormat(t, known, &w, threw) && !threw);
        CHECK(w.empty() && t.Root().Children.size() == 1);
    }
    for (const char* newer : {"2", "2.5"})
    {
        Tree t;
        w.clear();
        CHECK(ReadWithFormat(t, newer, &w, threw) && !threw);
        CHECK(w.size() == 1 && w[0].find("newer version") != std::string::npos && t.Root().Children.size() == 1);
    }
}

static void TestChannels()
{
    for (int i = 0; i < static_cast<int>(EChannel::Count); i++)
    {
        const EChannel c = static_cast<EChannel>(i);
        CHECK(ChannelFromKey(ChannelKey(c)) == c);
    }
}

void RunMacroTreeTests()
{
    TestDepthLimit();
    TestMoveDepth();
    TestMoveIntoSelf();
    TestMoveOrder();
    TestNames();
    TestDeleteAndCount();
    TestSort();
    TestJsonRoundTrip();
    TestJsonDamaged();
    TestDuplicateNames();
    TestJsonFormat();
    TestChannels();
}
