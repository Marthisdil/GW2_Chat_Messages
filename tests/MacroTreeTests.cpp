// Standalone tests for src/MacroTree.cpp (no Nexus, no ImGui).
// Build and run: tools\run_tests.ps1 (CI runs it too).

#include <string>

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
    TestChannels();
}
