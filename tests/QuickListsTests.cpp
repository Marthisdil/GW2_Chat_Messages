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
    CHECK(ls.Favourites.size() == 2 && ls.Favourites[0] == l.a && ls.Favourites[1] == l.c); // tree order
    CHECK(ls.Recent.size() == 2 && ls.Recent[0] == l.b && ls.Recent[1] == l.d);              // newest first

    ls = Build(l.t, r, 1, nullptr);
    CHECK(ls.Recent.size() == 1 && ls.Recent[0] == l.b);
    ls = Build(l.t, r, 0, nullptr);
    CHECK(ls.Recent.empty() && ls.Favourites.size() == 2);

    // Only what's active here (the filter the menu uses).
    ls = Build(l.t, r, 5, [&](const Node& n) { return &n != l.a && &n != l.b; });
    CHECK(ls.Favourites.size() == 1 && ls.Favourites[0] == l.c);
    CHECK(ls.Recent.size() == 1 && ls.Recent[0] == l.d);

    // A macro in the trash bin is gone from both.
    CHECK(l.t.MoveToTrash(*l.a, "2026-11-23 23:00").Ok);
    CHECK(l.t.MoveToTrash(*l.d, "2026-11-23 23:00").Ok);
    ls = Build(l.t, r, 5, nullptr);
    CHECK(ls.Favourites.size() == 1 && ls.Favourites[0] == l.c);
    CHECK(ls.Recent.size() == 1 && ls.Recent[0] == l.b);
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

    // Not true/false: dropped, with a warning.
    nlohmann::json bad = j;
    for (auto& top : bad["root"]["children"])
        if (top["name"] == "A") top["favourite"] = "yes";
    std::vector<std::string> warnings;
    Tree t2;
    CHECK(t2.FromJson(bad, &warnings));
    CHECK(!t2.Find(l.a->Id)->Favourite && !warnings.empty());

    // Exports leave the mark out: favourites are your own.
    const nlohmann::json ex = Exchange::ExportJson({l.b});
    CHECK(ex.dump().find("favourite") == std::string::npos);
}

void RunQuickListsTests()
{
    TestRecent();
    TestCount();
    TestBuild();
    TestFavouriteJson();
}
