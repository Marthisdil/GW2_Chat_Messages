// Standalone tests for src/MapCatalog.cpp (map list, maps.json, the map picker's search).

#include <string>

#include <nlohmann/json.hpp>

#include "MapCatalog.h"
#include "TestCheck.h"

using namespace MapCatalog;

static void TestApiLang()
{
    CHECK(ApiLang("de") == "de");
    CHECK(ApiLang("fr") == "fr");
    CHECK(ApiLang("es") == "es");
    CHECK(ApiLang("zh") == "zh");
    CHECK(ApiLang("en") == "en");
    CHECK(ApiLang("pt-br") == "en");
    CHECK(ApiLang("") == "en");
}

static void TestParse()
{
    std::vector<uint32_t> ids;
    CHECK(ParseIds("[15, 50, 15, 0, -3, \"x\", 1.5, 4294967296, 26]", ids));
    CHECK(ids.size() == 3 && ids[0] == 15 && ids[1] == 26 && ids[2] == 50);
    CHECK(!ParseIds("{\"text\":\"no\"}", ids));
    CHECK(!ParseIds("not json", ids));

    std::unordered_map<uint32_t, Map> maps;
    CHECK(ParseMaps(R"([{"id":15,"name":"Queensdale","region_name":"Kryta","floors":[1]},
                        {"id":50,"name":"Lion's Arch"},
                        {"id":0,"name":"Zero"},
                        {"id":"7","name":"Text id"},
                        {"id":8},
                        {"id":9,"name":""},
                        {"id":10,"name":"Bad\u0007bell"},)"
                    "{\"id\":11,\"name\":\"Bidi \\u202e\"},"
                    R"({"id":12,"name":"Ok","region_name":"Bad\u0001"}])", maps));
    CHECK(maps.size() == 3);
    CHECK(maps.count(15) && maps[15].Name == "Queensdale" && maps[15].Region == "Kryta" && maps[15].Folded == "queensdale");
    CHECK(maps.count(50) && maps[50].Region.empty());
    CHECK(maps.count(12) && maps[12].Region.empty()); // a bad region is dropped, the map kept
    CHECK(!ParseMaps("{}", maps));
}

static Cache Sample()
{
    Cache c;
    c.Lang = "en";
    c.Maps[15] = MakeMap("Queensdale", "Kryta");
    c.Maps[50] = MakeMap("Lion's Arch", "Kryta");
    c.Maps[1206] = MakeMap("Mistlock Sanctuary", "Fractals of the Mists");
    c.Maps[38] = MakeMap("Eternal Battlegrounds", "World vs. World");
    c.Maps[1155] = MakeMap("Lion's Arch Aerodrome", "Kryta");
    c.Maps[900] = MakeMap("Café Map", "");
    c.Listed = {15, 38, 50, 1155, 1206, 900, 777};
    c.ListedAt = 1000;
    return c;
}

static void TestJsonRoundTrip()
{
    Cache c = Sample();
    NoteVisit(c, 50);
    NoteVisit(c, 15);
    const std::string text = ToJson(c);

    Cache back;
    CHECK(FromJson(text, "en", back));
    CHECK(back.Maps.size() == c.Maps.size());
    CHECK(back.Maps[1155].Name == "Lion's Arch Aerodrome" && back.Maps[1155].Region == "Kryta");
    CHECK(back.Maps[900].Folded == "cafe map");
    CHECK(back.Listed == c.Listed && back.ListedAt == 1000);
    // The maps visited aren't in maps.json (a cache): they're in settings.json.
    CHECK(back.Visited.empty() && text.find("visited") == std::string::npos);
    const std::vector<uint32_t> visited = VisitedFromJson(VisitedToJson(c));
    CHECK(visited.size() == 2 && visited[0] == 15 && visited[1] == 50);
    CHECK(VisitedFromJson(nlohmann::json::parse(R"([3,3,"z",0,4,-1,4294967296,5.5])")) == std::vector<uint32_t>({3, 4}));
    CHECK(VisitedFromJson(nlohmann::json::parse(R"({"a":1})")).empty());
    {
        nlohmann::json many = nlohmann::json::array();
        for (uint32_t i = 1; i <= kMaxVisited + 5; i++) many.push_back(i);
        CHECK(VisitedFromJson(many).size() == kMaxVisited);
    }

    // Another language: names and the list are thrown away (nothing usable is left).
    Cache de;
    CHECK(!FromJson(text, "de", de));
    CHECK(de.Lang == "de" && de.Maps.empty() && de.Listed.empty() && de.ListedAt == 0);

    // Damaged or old files.
    Cache bad;
    CHECK(!FromJson("", "en", bad));
    CHECK(!FromJson("[1,2]", "en", bad));
    CHECK(!FromJson(R"({"format":1,"lang":"en","names":{"15":"Queensdale"}})", "en", bad)); // the old format
    CHECK(!FromJson(R"({"format":"2"})", "en", bad));
    CHECK(!FromJson(R"({"format":2,"lang":"en","maps":"x","listed":5,"listed_at":"y","visited":[3,4]})", "en", bad));
    CHECK(bad.Maps.empty() && bad.Listed.empty() && bad.Visited.empty());
    CHECK(FromJson(R"({"format":2,"lang":"en","maps":[{"id":5,"name":"Five"},{"id":5.5,"name":"x"},7],"listed":[5],"listed_at":3})",
                   "en", bad));
    CHECK(bad.Maps.size() == 1 && bad.Maps[5].Name == "Five" && bad.Listed.count(5) && bad.ListedAt == 3);
    // A list without a time doesn't count (it would never be refreshed).
    CHECK(FromJson(R"({"format":2,"lang":"en","maps":[{"id":5,"name":"Five"}],"listed":[5]})", "en", bad));
    CHECK(bad.Listed.empty());
}

static void TestRefreshAndVisits()
{
    Cache c;
    CHECK(RefreshDue(c, 5000));
    c = Sample();
    CHECK(!RefreshDue(c, 1000 + kRefreshSeconds - 1));
    CHECK(RefreshDue(c, 1000 + kRefreshSeconds));
    CHECK(RefreshDue(c, -100000)); // the list seems to be from the future

    CHECK(!NoteVisit(c, 0));
    CHECK(NoteVisit(c, 15));
    CHECK(!NoteVisit(c, 15)); // already first
    CHECK(NoteVisit(c, 50));
    CHECK(NoteVisit(c, 15));
    CHECK(c.Visited.size() == 2 && c.Visited[0] == 15);
    for (uint32_t i = 1; i <= kMaxVisited + 20; i++) NoteVisit(c, 10000 + i);
    CHECK(c.Visited.size() == kMaxVisited && c.Visited[0] == 10000 + kMaxVisited + 20);

    CHECK(!Unlisted(c, 15));
    CHECK(Unlisted(c, 16));
    CHECK(!Unlisted(c, 0));
    Cache empty;
    CHECK(!Unlisted(empty, 16)); // no full list yet: nothing is flagged
}

static void TestFind()
{
    Cache c = Sample();
    CHECK(Find(c, "", 10).empty());
    CHECK(Find(c, "   ", 10).empty());

    auto r = Find(c, "lion", 10);
    CHECK(r.size() == 2 && r[0] == 50 && r[1] == 1155); // by name
    NoteVisit(c, 1155);
    r = Find(c, "LION'S", 10);
    CHECK(r.size() == 2 && r[0] == 1155 && r[1] == 50); // visited first
    CHECK(Find(c, "lion", 1).size() == 1);
    CHECK(Find(c, "lion", 0).empty());

    r = Find(c, "cafe", 10);
    CHECK(r.size() == 1 && r[0] == 900); // accents ignored
    r = Find(c, "38", 10);
    CHECK(r.size() == 1 && r[0] == 38); // by id
    r = Find(c, "777", 10);
    CHECK(r.size() == 1 && r[0] == 777); // listed, name not known yet
    CHECK(Find(c, "778", 10).empty());
    CHECK(Find(c, "zzz", 10).empty());
}

void RunMapCatalogTests()
{
    TestApiLang();
    TestParse();
    TestJsonRoundTrip();
    TestRefreshAndVisits();
    TestFind();
}
