// Tests for QuickLists (favourites and recently sent in the quick menu) and the favourite mark.

#include <string>

#include <nlohmann/json.hpp>

#include "Exchange.h"
#include "MacroTree.h"
#include "QuickLists.h"
#include "TestCheck.h"

using namespace MacroTree;
using namespace QuickLists;

namespace
{
    struct Lib
    {
        Tree  t;
        Node* raid = nullptr;
        Node* a = nullptr; // top level
        Node* b = nullptr; // in Raid
        Node* c = nullptr; // in Raid
        Node* d = nullptr; // top level
        Lib()
        {
            a    = t.AddMacro(nullptr, "A").Target;
            raid = t.AddFolder(nullptr, "Raid").Target;
            b    = t.AddMacro(raid, "B").Target;
            c    = t.AddMacro(raid, "C").Target;
            d    = t.AddMacro(nullptr, "D").Target;
        }
    };
}

static void TestRecent()
{
    Recent r;
    r.Sent(1);
    r.Sent(2);
    r.Sent(1); // moves to the front, once
    CHECK(r.Ids().size() == 2 && r.Ids()[0] == 1 && r.Ids()[1] == 2);
    r.Sent(0); // never an id
    CHECK(r.Ids().size() == 2);
    for (uint64_t i = 10; i < 30; i++) r.Sent(i);
    CHECK(r.Ids().size() == static_cast<size_t>(kMaxRecent) && r.Ids()[0] == 29);

    Recent back;
    back.FromJson(r.ToJson());
    CHECK(back.Ids() == r.Ids());
    CHECK(r.ToJson()[0] == "000000000000001d");

    // Anything that isn't a 16-digit hex id is skipped; duplicates and extras too.
    nlohmann::json j = nlohmann::json::array({"000000000000000a", 5, "zz", "", "0000000000000000", "000000000000000A",
                                              "00000000000000001", "00000000000000ff"});
    back.FromJson(j);
    CHECK(back.Ids().size() == 2 && back.Ids()[0] == 10 && back.Ids()[1] == 255);
    back.FromJson(nlohmann::json::object());
    CHECK(back.Ids().empty());
    nlohmann::json many = nlohmann::json::array();
    for (int i = 1; i <= 50; i++)
    {
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016x", i);
        many.push_back(buf);
    }
    back.FromJson(many);
    CHECK(back.Ids().size() == static_cast<size_t>(kMaxRecent));
}

static void TestCount()
{
    CHECK(CountFromJson(nlohmann::json()) == kDefaultRecent);
    CHECK(CountFromJson(nlohmann::json(0)) == 0);
    CHECK(CountFromJson(nlohmann::json(10)) == 10);
    CHECK(CountFromJson(nlohmann::json(11)) == kDefaultRecent);
    CHECK(CountFromJson(nlohmann::json(-1)) == kDefaultRecent);
    CHECK(CountFromJson(nlohmann::json(3.0)) == kDefaultRecent);
    CHECK(CountFromJson(nlohmann::json("3")) == kDefaultRecent);
    CHECK(CountFromJson(nlohmann::json::parse("18446744073709551615")) == kDefaultRecent);
    CHECK(CountFromJson(nlohmann::json::parse("7")) == 7);
}

static void TestBuild()
{
    Lib l;
    l.c->Favourite = true;
    l.a->Favourite = true;
    Recent r;
    r.Sent(l.d->Id);
    r.Sent(l.c->Id); // a favourite: not repeated under Recently sent
    r.Sent(l.b->Id);
    r.Sent(l.raid->Id); // a folder id never shows
    r.Sent(12345);      // gone

    Lists ls = Build(l.t, r, 5, nullptr);
    CHECK(ls.Favourites.size() == 2 && ls.Favourites[0].Item == l.a && ls.Favourites[1].Item == l.c); // tree order
    CHECK(ls.Favourites[0].Active && ls.Favourites[1].Active);
    CHECK(ls.Recent.size() == 2 && ls.Recent[0] == l.b && ls.Recent[1] == l.d);                        // newest first

    ls = Build(l.t, r, 1, nullptr);
    CHECK(ls.Recent.size() == 1 && ls.Recent[0] == l.b);
    ls = Build(l.t, r, 0, nullptr);
    CHECK(ls.Recent.empty() && ls.Favourites.size() == 2);

    // Favourites show wherever you are (only marked inactive); recently sent follows the filter.
    ls = Build(l.t, r, 5, [&](const Node& n) { return &n != l.a && &n != l.b; });
    CHECK(ls.Favourites.size() == 2 && ls.Favourites[0].Item == l.a && !ls.Favourites[0].Active);
    CHECK(ls.Favourites[1].Item == l.c && ls.Favourites[1].Active);
    CHECK(ls.Recent.size() == 1 && ls.Recent[0] == l.d);

    // A macro in the trash bin is gone from both.
    CHECK(l.t.MoveToTrash(*l.a, "2026-11-23 23:00").Ok);
    CHECK(l.t.MoveToTrash(*l.d, "2026-11-23 23:00").Ok);
    ls = Build(l.t, r, 5, nullptr);
    CHECK(ls.Favourites.size() == 1 && ls.Favourites[0].Item == l.c);
    CHECK(ls.Recent.size() == 1 && ls.Recent[0] == l.b);
}

static void TestFavouriteFolders()
{
    Lib l;
    l.raid->Favourite = true;
    l.c->Favourite = true; // a favourite inside a favourite folder: listed too, after it
    CHECK(CountFavourites(l.t) == 2);
    Lists ls = Build(l.t, Recent(), 5, nullptr);
    CHECK(ls.Favourites.size() == 2 && ls.Favourites[0].Item == l.raid && ls.Favourites[1].Item == l.c);
    CHECK(ls.Favourites[0].Active);
    // A folder is active when a macro inside it is; otherwise it's still listed, dimmed.
    ls = Build(l.t, Recent(), 5, [&](const Node& n) { return &n == l.c; });
    CHECK(ls.Favourites[0].Active);
    ls = Build(l.t, Recent(), 5, [&](const Node&) { return false; });
    CHECK(ls.Favourites.size() == 2 && !ls.Favourites[0].Active && !ls.Favourites[1].Active);
    CHECK(HasActive(*l.raid, nullptr) && !HasActive(*l.raid, [](const Node&) { return false; }));
    // Macros inside a favourite folder still show under Recently sent (only marked ones don't).
    Recent r;
    r.Sent(l.b->Id);
    CHECK(Build(l.t, r, 5, nullptr).Recent.size() == 1);
    // An empty favourite folder is listed too.
    Node* empty = l.t.AddFolder(nullptr, "Empty").Target;
    empty->Favourite = true;
    CHECK(Build(l.t, Recent(), 5, nullptr).Favourites.size() == 3);
    CHECK(!Build(l.t, Recent(), 5, nullptr).Favourites[2].Active);
}

static void TestFavouriteLimit()
{
    Tree t;
    std::vector<Node*> m;
    for (int i = 0; i < kMaxFavourites + 3; i++) m.push_back(t.AddMacro(nullptr, "M" + std::to_string(i)).Target);
    for (int i = 0; i < kMaxFavourites - 1; i++) m[static_cast<size_t>(i)]->Favourite = true;
    CHECK(CanAddFavourite(t));
    m[static_cast<size_t>(kMaxFavourites - 1)]->Favourite = true;
    CHECK(!CanAddFavourite(t) && CountFavourites(t) == static_cast<size_t>(kMaxFavourites));
    // A file with more marked (hand-edited): all are kept, the first ones in tree order show.
    for (Node* n : m) n->Favourite = true;
    CHECK(CountFavourites(t) == m.size());
    const Lists ls = Build(t, Recent(), 5, nullptr);
    CHECK(ls.Favourites.size() == static_cast<size_t>(kMaxFavourites) && ls.Favourites[0].Item == m[0]);
    // Trashed favourites don't count.
    CHECK(t.MoveToTrash(*m[0], "2026-11-24 01:00").Ok);
    CHECK(CountFavourites(t) == m.size() - 1);
}

static void TestFavouriteJson()
{
    Lib l;
    l.b->Favourite = true;
    const nlohmann::json j = l.t.ToJson();
    Tree back;
    CHECK(back.FromJson(nlohmann::json::parse(j.dump())));
    const Node* b = back.Find(l.b->Id);
    const Node* c = back.Find(l.c->Id);
    CHECK(b && b->Favourite && c && !c->Favourite);
    CHECK(j.dump().find("\"favourite\"") != std::string::npos);

    // Folders keep the mark too.
    l.raid->Favourite = true;
    Tree withFolder;
    CHECK(withFolder.FromJson(nlohmann::json::parse(l.t.ToJson().dump())));
    CHECK(withFolder.Find(l.raid->Id)->Favourite);
    l.raid->Favourite = false;

    // Not true/false: dropped, with a warning.
    nlohmann::json bad = j;
    for (auto& top : bad["root"]["children"])
        if (top["name"] == "A") top["favourite"] = "yes";
    std::vector<std::string> warnings;
    Tree t2;
    CHECK(t2.FromJson(bad, &warnings));
    CHECK(!t2.Find(l.a->Id)->Favourite && !warnings.empty());

    // Exports leave the mark out, at every level: favourites are your own.
    l.raid->Favourite = true;
    const nlohmann::json ex = Exchange::ExportJson({l.raid});
    CHECK(ex.dump().find("favourite") == std::string::npos);
}

void RunQuickListsTests()
{
    TestRecent();
    TestCount();
    TestBuild();
    TestFavouriteFolders();
    TestFavouriteLimit();
    TestFavouriteJson();
}
