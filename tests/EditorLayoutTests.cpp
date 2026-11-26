// Standalone tests for src/EditorLayout.cpp (the macro editor's folding sections and note summary).

#include <string>

#include <nlohmann/json.hpp>

#include "EditorLayout.h"
#include "TestCheck.h"

using namespace EditorLayout;

static void TestSections()
{
    const Sections def;
    CHECK(!def.Where && def.Hotkey && def.Note && !def.Advanced);

    // Round trip.
    Sections s;
    s.Where = true;
    s.Note = false;
    s.Advanced = true;
    CHECK(FromJson(ToJson(s)) == s);
    CHECK(ToJson(s).dump() == R"({"advanced":true,"hotkey":true,"note":false,"where":true})");

    // Missing, wrong types and unknown keys keep the defaults.
    CHECK(FromJson(nlohmann::json()) == def);
    CHECK(FromJson(nlohmann::json::array({1, 2})) == def);
    CHECK(FromJson(nlohmann::json::parse(R"({"where":"yes","hotkey":0,"note":null})")) == def);
    const Sections partial = FromJson(nlohmann::json::parse(R"({"where":true,"timing":true})"));
    CHECK(partial.Where && partial.Hotkey && partial.Note && !partial.Advanced);
}

static void TestShortNote()
{
    CHECK(ShortNote("") == "");
    CHECK(ShortNote("Before each boss") == "Before each boss");
    CHECK(ShortNote("Before each boss\nNeeds a tag") == "Before each boss...");
    CHECK(ShortNote("abcdefghij", 4) == "abcd...");
    CHECK(ShortNote("ab   cdef", 4) == "ab...");             // no spaces before the dots
    CHECK(ShortNote("\xC3\xA4\xC3\xA4\xC3\xA4", 3) == "\xC3\xA4..."); // never inside a character
    CHECK(ShortNote("1234", 4) == "1234");
    CHECK(ShortNote("\nsecond", 10) == "...");
}

void RunEditorLayoutTests()
{
    TestSections();
    TestShortNote();
}
