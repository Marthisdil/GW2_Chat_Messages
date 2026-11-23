// Standalone tests for src/PathText.cpp: path text that never throws (SEC-020, SEC-024).

#include <filesystem>
#include <string>

#include "PathText.h"
#include "TestCheck.h"

namespace fs = std::filesystem;

static void TestFromWide()
{
    CHECK(PathText::FromWide(L"") == "");
    CHECK(PathText::FromWide(L"macros.json") == "macros.json");
    CHECK(PathText::FromWide(L"Käse €") == "K\xC3\xA4se \xE2\x82\xAC");
    // A pair: U+1F600 as D83D DE00.
    CHECK(PathText::FromWide(std::wstring{0xD83D, 0xDE00}) == "\xF0\x9F\x98\x80");
    // An unpaired surrogate (a name Windows allows, UTF-8 can't hold): U+FFFD, no throw.
    CHECK(PathText::FromWide(std::wstring{L'a', 0xD800, L'.', L'j'}) == "a\xEF\xBF\xBD.j");
    CHECK(PathText::FromWide(std::wstring{0xDC00}) == "\xEF\xBF\xBD");
    CHECK(PathText::FromWide(std::wstring{0xD800}) == "\xEF\xBF\xBD");
    CHECK(PathText::FromWide(std::wstring{0xD800, 0xD800, 0xDC00}) == "\xEF\xBF\xBD\xF0\x90\x80\x80");
}

static void TestToWide()
{
    CHECK(PathText::ToWide("") == L"");
    CHECK(PathText::ToWide("exports\\a.txt") == L"exports\\a.txt");
    CHECK(PathText::ToWide("K\xC3\xA4se \xE2\x82\xAC") == L"Käse €");
    CHECK(PathText::ToWide("\xF0\x9F\x98\x80") == (std::wstring{0xD83D, 0xDE00}));
    // What ImGui 1.80 makes of a lone surrogate WM_CHAR: ED A0 80, an encoded surrogate.
    CHECK(PathText::ToWide("a\xED\xA0\x80" "b") == (std::wstring{L'a', 0xFFFD, L'b'}));
    // Invalid bytes, overlong forms, cut-off sequences, past U+10FFFF.
    CHECK(PathText::ToWide("\xFF") == (std::wstring{0xFFFD}));
    CHECK(PathText::ToWide("\x80" "a") == (std::wstring{0xFFFD, L'a'}));
    CHECK(PathText::ToWide("\xC0\xAF") == (std::wstring{0xFFFD, 0xFFFD}));
    CHECK(PathText::ToWide("\xE0\x80\xAF") == (std::wstring{0xFFFD}));
    CHECK(PathText::ToWide("\xE2\x82") == (std::wstring{0xFFFD}));
    CHECK(PathText::ToWide("\xE2\x82" "a") == (std::wstring{0xFFFD, L'a'}));
    CHECK(PathText::ToWide("\xF4\x90\x80\x80") == (std::wstring{0xFFFD}));
    CHECK(PathText::ToWide("\xF0\x9F\x98") == (std::wstring{0xFFFD}));
}

static void TestPaths()
{
    // Valid text round-trips both ways.
    const std::string name = "raid\\K\xC3\xA4se \xF0\x9F\x98\x80.txt";
    CHECK(PathText::Utf8(PathText::FromUtf8(name)) == name);
    CHECK(PathText::FromUtf8(name) == fs::path(L"raid\\Käse \U0001F600.txt"));
    // The calls that threw before: no exception now.
    bool threw = false;
    try
    {
        const fs::path bad = PathText::FromUtf8("a\xED\xA0\x80.json");
        CHECK(PathText::Utf8(bad) == "a\xEF\xBF\xBD.json");
        const fs::path lone(std::wstring{L'x', 0xD800, L'.', L't', L'x', L't'});
        CHECK(PathText::Utf8(lone.filename()) == "x\xEF\xBF\xBD.txt");
        CHECK(PathText::Utf8(lone.extension()) == ".txt");
        CHECK(PathText::Utf8(lone.stem()) == "x\xEF\xBF\xBD");
    }
    catch (...) { threw = true; }
    CHECK(!threw);
}

void RunPathTextTests()
{
    TestFromWide();
    TestToWide();
    TestPaths();
}
