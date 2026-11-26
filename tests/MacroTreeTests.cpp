// Standalone tests for src/MacroTree.cpp (no Nexus, no ImGui).
// Build and run: tools\run_tests.ps1 (CI runs it too).

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "ChatLine.h"
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
    m->CooldownSec = 30;

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
    CHECK(m2 && m2->CooldownSec == 30);
    CHECK(j.dump().find("\"cooldown_sec\":30") != std::string::npos);
    m->CooldownSec = 0; // off: not written at all
    CHECK(t.ToJson().dump().find("cooldown") == std::string::npos);
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
        // Read, and marked as a newer version's (the caller tells the user: read-only, or a warning).
        CHECK(w.empty() && t.Root().Children.size() == 1 && t.FromNewerVersion() && t.Format() == 2);
    }
}

// "cooldown_sec": whole seconds, 0 to Cooldown::kMaxSec. Longer is cut with a warning; anything else
// is dropped with a warning. Missing is 0 (off).
static void TestJsonCooldown()
{
    struct Case { const char* Value; int Expect; size_t Warnings; };
    const Case cases[] = {
        {nullptr, 0, 0}, {"0", 0, 0}, {"5", 5, 0}, {"600", 600, 0}, {"601", 600, 1},
        {"18446744073709551615", 600, 1}, {"-1", 0, 1}, {"2.5", 0, 1}, {"\"10\"", 0, 1},
        {"null", 0, 1}, {"true", 0, 1}, {"[5]", 0, 1}, {"{}", 0, 1},
    };
    for (const Case& c : cases)
    {
        std::string text = R"({"format":1,"root":{"children":[{"type":"macro","id":"00000000000000aa","name":"M")";
        if (c.Value) text += std::string(R"(,"cooldown_sec":)") + c.Value;
        text += "}]}}";
        Tree t;
        std::vector<std::string> w;
        CHECK(t.FromJson(json::parse(text), &w));
        CHECK(t.Root().Children.size() == 1 && t.Root().Children[0]->CooldownSec == c.Expect);
        CHECK(w.size() == c.Warnings);
    }
}

static void TestJsonControlCharacters()
{
    // A file (an import, or a hand-edited macros.json) can't bring control characters into a
    // line's text or recipient: each one becomes a space. Other characters are kept.
    const json j = json::parse(R"({
        "format": 1,
        "root": { "children": [
            {"type": "macro", "id": "00000000000000b1", "name": "Bad", "lines": [
                {"channel": "current", "message": "gg\r/m text"},
                {"channel": "say", "message": "a\tb\u001b\u0000c\u0085"},
                {"channel": "whisper", "message": "hi\n", "whisper_to": "Na\tme\u007f"},
                {"channel": "map", "message": "caf\u00e9 \u2019 \u00a0ok"}
            ]}
        ]}
    })");
    Tree u;
    CHECK(u.FromJson(j));
    const Node* m = u.Find(0xb1);
    CHECK(m && m->Lines.size() == 4);
    if (!m || m->Lines.size() != 4) return;
    CHECK(m->Lines[0].Channel == EChannel::Current && m->Lines[0].Message == "gg /m text");
    CHECK(m->Lines[1].Message == "a b  c ");
    CHECK(m->Lines[2].Message == "hi " && m->Lines[2].WhisperTo == "Na me ");
    CHECK(m->Lines[3].Message == "caf\xC3\xA9 \xE2\x80\x99 \xC2\xA0ok");

    // Saved and loaded again, the text stays the same.
    Tree again;
    CHECK(again.FromJson(u.ToJson()));
    CHECK(again.ToJson() == u.ToJson());
}

static void TestJsonLineLimit()
{
    // A file (an import, a shared macros.json) can't bring more lines than the editor allows
    // (SEC-034): the first kMaxLines are kept, in order, with one warning naming the macro.
    json lines = json::array();
    for (size_t i = 0; i < kMaxLines + 5; i++)
        lines.push_back({{"channel", "say"}, {"message", "line " + std::to_string(i)}});
    json j = {{"format", 1},
              {"root", {{"children", json::array({{{"type", "macro"}, {"id", "00000000000000c1"},
                                                   {"name", "Long"}, {"lines", lines}}})}}}};
    Tree t;
    std::vector<std::string> warnings;
    CHECK(t.FromJson(j, &warnings));
    const Node* m = t.Find(0xc1);
    CHECK(m && m->Lines.size() == kMaxLines);
    CHECK(m && !m->Lines.empty() && m->Lines.front().Message == "line 0" &&
          m->Lines.back().Message == "line " + std::to_string(kMaxLines - 1));
    CHECK(warnings.size() == 1 && warnings[0].find("Long") != std::string::npos);

    // Exactly kMaxLines: all kept, no warning.
    json& kept = j["root"]["children"][0]["lines"];
    while (kept.size() > kMaxLines) kept.erase(kept.size() - 1);
    Tree exact;
    std::vector<std::string> none;
    CHECK(exact.FromJson(j, &none));
    CHECK(exact.Find(0xc1) && exact.Find(0xc1)->Lines.size() == kMaxLines && none.empty());
}

static void TestJsonRecipientLimit()
{
    // A file can't bring a whisper recipient longer than any name (SEC-035): it's cut to
    // kMaxRecipientLength with one warning naming the macro; a recipient that fits is kept.
    const size_t max = ChatLine::kMaxRecipientLength;
    json lines = json::array({{{"channel", "whisper"}, {"whisper_to", "Bob" + std::string(100000, 'x')}, {"message", "hi"}},
                              {{"channel", "whisper"}, {"whisper_to", std::string(max, 'y')}, {"message", "ok"}}});
    json j = {{"format", 1},
              {"root", {{"children", json::array({{{"type", "macro"}, {"id", "00000000000000c2"},
                                                   {"name", "Whisper"}, {"lines", lines}}})}}}};
    Tree t;
    std::vector<std::string> warnings;
    CHECK(t.FromJson(j, &warnings));
    const Node* m = t.Find(0xc2);
    CHECK(m && m->Lines.size() == 2);
    CHECK(m && m->Lines.size() == 2 && ChatLine::Utf16Length(m->Lines[0].WhisperTo) == max &&
          m->Lines[0].WhisperTo.compare(0, 3, "Bob") == 0 && m->Lines[1].WhisperTo == std::string(max, 'y'));
    CHECK(warnings.size() == 1 && warnings[0].find("Whisper") != std::string::npos);
}

static void TestChannels()
{
    for (int i = 0; i < static_cast<int>(EChannel::Count); i++)
    {
        const EChannel c = static_cast<EChannel>(i);
        CHECK(ChannelFromKey(ChannelKey(c)) == c);
    }
}

// SEC-017: every id-less node used to get its id by walking the whole tree (about N² work; 50,000
// id-less macros took about 40 s, freezing the game). Now it's one set lookup per id.
static void TestManyMissingIds()
{
    const int kCount = 50000;
    std::string text = R"({"format":1,"root":{"children":[)";
    for (int i = 0; i < kCount; i++)
        text += std::string(i ? "," : "") + R"({"type":"macro","name":"m)" + std::to_string(i) + R"(","lines":[]})";
    text += "]}}";
    const json j = json::parse(text);

    Tree t;
    std::vector<std::string> warnings;
    const auto start = std::chrono::steady_clock::now();
    CHECK(t.FromJson(j, &warnings));
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    CHECK(seconds < 10.0); // well under a second now; the old way took about 40 s
    CHECK(t.Root().Children.size() == static_cast<size_t>(kCount));
    std::unordered_set<uint64_t> ids;
    for (const auto& c : t.Root().Children) ids.insert(c->Id);
    CHECK(ids.size() == static_cast<size_t>(kCount) && !ids.count(0));
    CHECK(!warnings.empty());

    // Items added afterwards don't reuse a loaded id, nor one in the bin.
    Tree u;
    Node* a = u.AddMacro(nullptr, "A").Target;
    Node* b = u.AddMacro(nullptr, "B").Target;
    const uint64_t binned = b->Id;
    u.MoveToTrash(*b, "t");
    Tree v;
    CHECK(v.FromJson(u.ToJson()));
    Node* c = v.AddMacro(nullptr, "C").Target;
    CHECK(c && c->Id != 0 && c->Id != a->Id && c->Id != binned);
}

static void TestNumberedName()
{
    CHECK(Tree::NumberedName("Hello", 2) == "Hello (2)");
    const std::string longName(100, 'a');
    CHECK(Tree::NumberedName(longName, 2) == std::string(96, 'a') + " (2)");
    CHECK(Tree::NumberedName(longName, 10) == std::string(95, 'a') + " (10)");
    CHECK(Tree::NumberedName(longName, 2) != Tree::NumberedName(longName, 3));
    // Never cut inside a UTF-8 sequence: 50 two-byte characters (100 bytes).
    std::string umlauts;
    for (int i = 0; i < 50; i++) umlauts += "\xC3\xA4";
    const std::string n = Tree::NumberedName(umlauts, 2);
    CHECK(n.size() <= kMaxNameLength && n == std::string(umlauts, 0, 96) + " (2)");
}

// Right-click > Duplicate: the copy goes right after the original, numbered, with no hotkey,
// favourite or link file.
static void TestDuplicate()
{
    Tree t;
    Node* folder = t.AddFolder(nullptr, "Raids").Target;
    Node* hello = t.AddMacro(folder, "Hello").Target;
    Node* after = t.AddMacro(folder, "Zed").Target;
    hello->Lines.push_back({EChannel::Say, "Hi"});
    hello->GameModes = GM_WvW;
    hello->MapIds = {38, 95};
    hello->CooldownSec = 20;
    hello->Note = "for raids";
    hello->Hotkey = Hotkey::FromString("Ctrl+F1");
    hello->Favourite = true;
    hello->LinkFile = "hello.txt";
    hello->Extra = std::make_shared<const json>(json{{"future", 1}});

    Result r = t.Duplicate(*hello);
    CHECK(r.Ok && r.Target && r.Target != hello);
    Node* copy = r.Target;
    if (!copy) return;
    CHECK(copy->Name == "Hello (2)" && copy->Parent == folder && copy->IsMacro());
    CHECK(copy->Id != hello->Id && t.Find(copy->Id) == copy);
    CHECK(folder->Children.size() == 3 && folder->Children[1].get() == copy && folder->Children[2].get() == after);
    CHECK(copy->Lines.size() == 1 && copy->Lines[0].Message == "Hi" && copy->Lines[0].Channel == EChannel::Say);
    CHECK(copy->GameModes == GM_WvW && copy->MapIds == std::vector<int>({38, 95}));
    CHECK(copy->CooldownSec == 20 && copy->Note == "for raids" && copy->Extra == hello->Extra);
    CHECK(copy->Hotkey.Empty() && !copy->Favourite && copy->LinkFile.empty());
    CHECK(t.MacrosWithHotkey(hello->Hotkey).size() == 1);
    // The original is untouched.
    CHECK(!hello->Hotkey.Empty() && hello->Favourite && hello->LinkFile == "hello.txt");

    // "Hello (2)" again: "Hello (3)", not "Hello (2) (2)"; and the next free number.
    CHECK(t.Duplicate(*copy).Target && t.Duplicate(*copy).Target);
    CHECK(folder->Children.size() == 5);
    CHECK(folder->Children[2]->Name == "Hello (4)" && folder->Children[3]->Name == "Hello (3)");
    CHECK(t.Duplicate(*hello).Target->Name == "Hello (5)");

    // Names that only look numbered keep their text.
    Node* odd = t.AddMacro(folder, "Wing (02)").Target;
    CHECK(odd && t.Duplicate(*odd).Target->Name == "Wing (02) (2)");
    Node* bare = t.AddMacro(folder, "(7)").Target;
    CHECK(bare && t.Duplicate(*bare).Target->Name == "(7) (2)");
    Node* big = t.AddMacro(folder, "Big (1234567)").Target;
    CHECK(big && t.Duplicate(*big).Target->Name == "Big (1234567) (2)");
    // A long name still fits.
    Node* longOne = t.AddMacro(nullptr, std::string(100, 'a')).Target;
    Result rl = t.Duplicate(*longOne);
    CHECK(rl.Ok && rl.Target->Name.size() <= kMaxNameLength && rl.Target->Name != longOne->Name);

    // Folders and the root are refused.
    CHECK(!t.Duplicate(*folder).Ok);
    CHECK(!t.Duplicate(t.Root()).Ok);

    // The copy is saved like any macro, and loads back.
    Tree back;
    CHECK(back.FromJson(t.ToJson()));
    const Node* f = back.Root().Children.empty() ? nullptr : back.Root().Children[0].get();
    // Order: Hello, Hello (5) (the last copy of Hello goes right after it), Hello (2), ...
    CHECK(f && f->Children.size() >= 3 && f->Children[1]->Name == "Hello (5)" && f->Children[2]->Name == "Hello (2)");
    CHECK(f && f->Children.size() >= 3 && f->Children[2]->Note == "for raids" && f->Children[2]->Hotkey.Empty());
}

// A macro's note: never sent, cleaned and capped.
static void TestNotes()
{
    CHECK(Tree::CleanNote("") == "");
    CHECK(Tree::CleanNote("Use in raids") == "Use in raids");
    CHECK(Tree::CleanNote("a\r\nb\rc\nd") == "a\nb\nc\nd");
    CHECK(Tree::CleanNote("tab\there\x01x\x7F") == "tab here x");
    CHECK(Tree::CleanNote("x   \n  y  \n\n") == "x\n  y");
    CHECK(Tree::CleanNote("\xC2\x85" "C1") == " C1");
    // At most kMaxNoteLines lines: later breaks become spaces.
    std::string many;
    for (int i = 0; i < 15; i++) many += "l" + std::to_string(i) + "\n";
    const std::string kept = Tree::CleanNote(many);
    CHECK(std::count(kept.begin(), kept.end(), '\n') == static_cast<std::ptrdiff_t>(kMaxNoteLines - 1));
    CHECK(kept.find("l14") != std::string::npos);
    // At most kMaxNoteLength bytes, never inside a character.
    std::string umlauts;
    for (int i = 0; i < 600; i++) umlauts += "\xC3\xA4";
    const std::string cut = Tree::CleanNote(umlauts);
    CHECK(cut.size() <= kMaxNoteLength && cut.size() % 2 == 0);
    // Only real text cut off counts; spaces trimmed at the end don't.
    bool wasCut = true;
    CHECK(Tree::CleanNote(std::string(900, 'x') + std::string(200, ' '), &wasCut).size() == 900 && !wasCut);
    CHECK(Tree::CleanNote(umlauts, &wasCut).size() <= kMaxNoteLength && wasCut);

    // Saved only when there is one; read back cleaned; a too-long one shortened with a warning.
    Tree t;
    Node* m = t.AddMacro(nullptr, "M").Target;
    Node* plain = t.AddMacro(nullptr, "Plain").Target;
    m->Note = "line 1\nline 2";
    json j = t.ToJson();
    const std::string dumped = j.dump();
    CHECK(dumped.find("\"note\":\"line 1\\nline 2\"") != std::string::npos);
    CHECK(dumped.find("\"note\"") == dumped.rfind("\"note\"")); // only M's
    (void)plain;
    j["root"]["children"][0]["note"] = std::string("bad\tnote ") + std::string(2000, 'x');
    j["root"]["children"][1]["note"] = 42; // not text: no note
    Tree back;
    std::vector<std::string> w;
    CHECK(back.FromJson(j, &w));
    CHECK(back.Root().Children.size() == 2);
    if (back.Root().Children.size() == 2)
    {
        const std::string& note = back.Root().Children[0]->Note;
        CHECK(note.size() == kMaxNoteLength && note.compare(0, 9, "bad note ") == 0);
        CHECK(back.Root().Children[1]->Note.empty());
    }
    CHECK(w.size() == 1);
    // A folder never has one (it isn't read there).
    json jf = Tree().ToJson();
    jf["root"]["children"] = json::array({json{{"type", "folder"}, {"id", "0000000000000005"}, {"name", "F"}, {"note", "x"}, {"children", json::array()}}});
    Tree tf;
    CHECK(tf.FromJson(jf) && tf.Root().Children.size() == 1 && tf.Root().Children[0]->Note.empty());
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
    TestJsonCooldown();
    TestJsonControlCharacters();
    TestJsonLineLimit();
    TestJsonRecipientLimit();
    TestChannels();
    TestManyMissingIds();
    TestNumberedName();
    TestDuplicate();
    TestNotes();
}
