// Standalone tests for src/Localization.cpp (language files and their checks), plus a check of
// the shipped files in localization\ (run from the repository root: tools\run_tests.ps1 does).

#include <fstream>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "Localization.h"
#include "TestCheck.h"

using namespace Localization;

static std::string File(const char* aPath)
{
    std::ifstream in(aPath, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static void TestIds()
{
    CHECK(ValidId("en") && ValidId("de") && ValidId("pt-br") && ValidId("zh-hant") && ValidId("fil"));
    CHECK(!ValidId("") && !ValidId("e") && !ValidId("engl") && !ValidId("EN") && !ValidId("de-") &&
          !ValidId("de-x") && !ValidId("de_DE") && !ValidId("../en") && !ValidId("de-abcdefghi"));
}

static void TestSafeText()
{
    std::string why;
    CHECK(SafeText("Makrobibliothek \xC3\xB6" "ffnen", false));          // öffnen
    CHECK(SafeText("\xE4\xB8\xAD\xE6\x96\x87", false));                  // 中文
    CHECK(SafeText("two\nlines", true) && !SafeText("two\nlines", false));
    CHECK(!SafeText("tab\there", true, &why) && !why.empty());
    CHECK(!SafeText(std::string("nul\0x", 5), true));
    CHECK(!SafeText("\x7F", true));
    CHECK(!SafeText("\xC2\x85", true));                                  // C1 control (NEL)
    CHECK(!SafeText("\xE2\x80\xAE" "evil", true));                       // right-to-left override
    CHECK(!SafeText("\xE2\x81\xA6" "x", true));                          // left-to-right isolate
    CHECK(!SafeText("a\xE2\x80\x8B" "b", true));                         // zero-width space
    CHECK(!SafeText("\xC3", true) && !SafeText("\xC0\xAF", true));       // cut short; overlong '/'
    CHECK(!SafeText("\xED\xA0\x80", true));                              // surrogate
    CHECK(!SafeText("\xF4\x90\x80\x80", true));                          // above U+10FFFF
    CHECK(!SafeText("Label##other", true, &why));                        // ImGui ID separator
}

static void TestPlaceholders()
{
    std::vector<std::string> p;
    CHECK(Placeholders("Found: %zu folder(s), %s, %02d, %.1f, 100%%", p));
    CHECK(p.size() == 4 && p[0] == "%zu" && p[1] == "%s" && p[2] == "%02d" && p[3] == "%.1f");
    CHECK(Placeholders("no codes", p) && p.empty());
    CHECK(!Placeholders("%n", p) && !Placeholders("%p", p) && !Placeholders("%*d", p) &&
          !Placeholders("%1$s", p) && !Placeholders("ends with %", p) && !Placeholders("%123d", p));
    CHECK(Placeholders("% s", p) && p.size() == 1 && p[0] == "% s"); // a flag: allowed, and must match exactly
    CHECK(SamePlaceholders("Sent %s from %s", "%s gesendet aus %s"));
    CHECK(!SamePlaceholders("Sent %s from %s", "%s gesendet"));              // one missing
    CHECK(!SamePlaceholders("%zu item(s)", "%s Elemente"));                  // type changed
    CHECK(!SamePlaceholders("%s and %zu", "%zu und %s"));                    // order changed
    CHECK(!SamePlaceholders("Delete", "L\xC3\xB6schen %s"));                 // one added
    CHECK(!SamePlaceholders("Delete", "%n"));
    CHECK(SamePlaceholders("100%% sure", "s\xC3\xBBr \xC3\xA0 100%%"));      // %% doesn't count
    CHECK(SamePlaceholders("Delete", "Supprimer (100%%)"));
}

static void TestParse()
{
    const std::unordered_set<std::string> known = {"Open macro library", "Sent %s", "Delete"};
    Catalog c;
    std::string err;
    const std::string good = "\xEF\xBB\xBF{ \"Identifier\": \"de\", \"DisplayName\": \"Deutsch\", \"AddonVersion\": \"1\","
                             " \"About\": [\"x\"], \"Texts\": {"
                             " \"Open macro library\": \"Makrobibliothek \\u00f6ffnen\","
                             " \"Sent %s\": \"%s gesendet\","
                             " \"Delete\": \"\","
                             " \"Unknown text\": \"Unbekannt\" } }";
    CHECK(ParseCatalog(good, &known, c, err));
    CHECK(c.Id == "de" && c.DisplayName == "Deutsch" && c.AddonVersion == "1");
    CHECK(c.Texts.size() == 2 && c.Texts["Sent %s"] == "%s gesendet");
    CHECK(c.Texts["Open macro library"] == "Makrobibliothek \xC3\xB6" "ffnen");
    // "Unknown text" isn't one this version uses: left out quietly, not a problem; nor is the empty one.
    CHECK(c.Skipped == 0 && c.SkipNotes.empty() && c.Unused == 1);
    CHECK(c.Format == 1 && !c.FromNewer); // no "Format": format 1
    CHECK(PercentDone(c, known) == 66);

    // Texts that fail a check are skipped, the rest of the file is used.
    const std::string mixed = "{\"Identifier\":\"fr\",\"DisplayName\":\"Fran\\u00e7ais\",\"Texts\":{"
                              "\"Sent %s\":\"Envoy\\u00e9 %n\","
                              "\"Delete\":\"\\u202eSupprimer\","
                              "\"Open macro library\":42}}";
    CHECK(ParseCatalog(mixed, &known, c, err) && c.Texts.empty() && c.Skipped == 3);

    // Whole files that are refused.
    CHECK(!ParseCatalog("not json", &known, c, err) && !err.empty());
    CHECK(!ParseCatalog("[1,2]", &known, c, err));
    CHECK(!ParseCatalog("{\"DisplayName\":\"x\",\"Texts\":{}}", &known, c, err));                       // no id
    CHECK(!ParseCatalog("{\"Identifier\":\"..\\\\x\",\"DisplayName\":\"x\",\"Texts\":{}}", &known, c, err)); // bad id
    CHECK(!ParseCatalog("{\"Identifier\":\"de\",\"DisplayName\":\"\",\"Texts\":{}}", &known, c, err));    // empty name
    CHECK(!ParseCatalog("{\"Identifier\":\"de\",\"DisplayName\":\"a\\nb\",\"Texts\":{}}", &known, c, err)); // newline in name
    CHECK(!ParseCatalog("{\"Identifier\":\"de\",\"DisplayName\":\"x\"}", &known, c, err));                 // no Texts
    CHECK(!ParseCatalog("{\"Identifier\":\"de\",\"DisplayName\":\"x\",\"Texts\":[]}", &known, c, err));
    std::string deep = "{\"Identifier\":\"de\",\"DisplayName\":\"x\",\"Texts\":{},\"About\":";
    for (int i = 0; i < 100; i++) deep += "[";
    for (int i = 0; i < 100; i++) deep += "]";
    deep += "}";
    CHECK(!ParseCatalog(deep, &known, c, err));
    CHECK(!ParseCatalog(std::string(kMaxFileBytes + 1, ' '), &known, c, err));
    // Brackets inside strings don't count as nesting.
    CHECK(ParseCatalog("{\"Identifier\":\"de\",\"DisplayName\":\"[[[[[{{{{\",\"Texts\":{}}", &known, c, err));
}

static void TestLookup()
{
    static const char kLabel[] = "Delete##ctx";
    Catalog de;
    de.Id = "de";
    de.Texts["Delete"] = "L\xC3\xB6schen";
    de.Texts["Sent %s"] = "%s gesendet";

    SetActive(nullptr);
    CHECK(std::string(T("Sent %s")) == "Sent %s");
    CHECK(std::string(T(kLabel)) == "Delete##ctx");
    SetActive(&de);
    CHECK(std::string(T("Sent %s")) == "%s gesendet");
    CHECK(std::string(T(kLabel)) == "L\xC3\xB6schen##ctx"); // the ID part is kept
    CHECK(T(kLabel) == T(kLabel));                          // cached per literal
    CHECK(std::string(T("Not translated")) == "Not translated");
    CHECK(std::string(T("##idonly")) == "##idonly");
    CHECK(LookupText("Delete###x") == "L\xC3\xB6schen###x");
    CHECK(Fmt(T("Sent %s"), "Hallo") == "Hallo gesendet");
    CHECK(Fmt("%zu/%zu", static_cast<size_t>(3), static_cast<size_t>(7)) == "3/7");
    CHECK(Fmt("%s", std::string(2000, 'x').c_str()).size() == 2000); // longer than the stack buffer
    SetActive(nullptr);
    CHECK(std::string(T("Sent %s")) == "Sent %s");
    std::string vis, suf;
    SplitId("Name###id", vis, suf);
    CHECK(vis == "Name" && suf == "###id");
}

// TR: any thread, and a text it returned stays valid after the language changes (the catalog it
// came from may be gone by then).
static void TestTranslate()
{
    SetActive(nullptr);
    CHECK(std::string(TR("Sent %s")) == "Sent %s");
    CHECK(std::string(Translate(nullptr)) == "");

    const char* held = nullptr;
    {
        Catalog de;
        de.Id = "de";
        de.Texts["Sent %s"] = "%s gesendet";
        SetActive(&de);
        held = TR("Sent %s");
        CHECK(std::string(held) == "%s gesendet");
        CHECK(std::string(TR("Not translated")) == "Not translated");
        CHECK(Fmt(TR("Sent %s"), "Hallo") == "Hallo gesendet");
        SetActive(nullptr); // before the catalog goes, as Languages does
    }
    CHECK(std::string(held) == "%s gesendet"); // the shared copy is still there
    CHECK(std::string(TR("Sent %s")) == "Sent %s");

    Catalog fr;
    fr.Id = "fr";
    fr.Texts["Sent %s"] = "%s envoy\xC3\xA9";
    SetActive(&fr);
    CHECK(std::string(TR("Sent %s")) == "%s envoy\xC3\xA9");
    SetActive(&fr); // the same texts again: the same copy is reused
    const char* a = TR("Sent %s");
    SetActive(&fr);
    CHECK(TR("Sent %s") == a);
    CHECK(std::string(held) == "%s gesendet");
    SetActive(nullptr);
    ClearShared();
    CHECK(std::string(TR("Sent %s")) == "Sent %s");
}

// The files in localization\: every one parses with nothing skipped, and the built-in languages
// translate every text.
// Files from other versions (docs/design.md, "Languages", "Language file format").
static void TestFormats()
{
    const std::unordered_set<std::string> known = {"Open macro library", "Sent %s", "Delete"};
    Catalog c;
    std::string err;

    // The sample of format 1: every field it can have; "About" and "Unused" are ignored.
    CHECK(ParseCatalog(File("tests/samples/language_format1.json"), &known, c, err));
    CHECK(c.Format == 1 && !c.FromNewer && c.Id == "de" && c.AddonVersion == "2026.11.26.0000.Dev");
    CHECK(c.Texts.size() == 2 && c.Texts.count("Open macro library") && !c.Texts.count("Open the macro library"));
    CHECK(c.Skipped == 0 && c.Unused == 0);

    // A newer file: what this version understands is used; new fields and new texts are ignored
    // quietly; a text that fails a check is still skipped on its own.
    const std::string newer = "{\"Format\": 2, \"Identifier\": \"de\", \"DisplayName\": \"Deutsch\","
                              " \"Plurals\": {\"one\": 1}, \"Notes\": \"new\", \"Texts\": {"
                              " \"Sent %s\": \"%s gesendet\","
                              " \"A text from a newer version\": \"Neu\","
                              " \"Delete\": \"Löschen %d\" } }";
    CHECK(ParseCatalog(newer, &known, c, err));
    CHECK(c.Format == 2 && c.FromNewer);
    CHECK(c.Texts.size() == 1 && c.Texts["Sent %s"] == "%s gesendet");
    CHECK(c.Unused == 1 && c.Skipped == 1); // the new text; the %-code mismatch

    // An older file (texts since reworded or removed): the rest is used, text by text.
    const std::string older = "{\"Identifier\": \"fr\", \"DisplayName\": \"Français\", \"AddonVersion\": \"0.1\","
                              " \"Texts\": { \"Open the library\": \"Ouvrir\", \"Delete\": \"Supprimer\" } }";
    CHECK(ParseCatalog(older, &known, c, err) && c.Texts.size() == 1 && c.Texts["Delete"] == "Supprimer");
    CHECK(c.Unused == 1 && c.Skipped == 0 && PercentDone(c, known) == 33);

    // Odd "Format" values: never an error; huge ones stay newer, the rest read as 1.
    const char* const odd[][2] = {{"\"x\"", "1"}, {"0", "1"}, {"-5", "1"}, {"1.9", "1"}, {"null", "1"},
                                  {"[2]", "1"}, {"3", "3"}, {"1e300", "1000000"}, {"2.5", "2"}};
    for (const auto& o : odd)
    {
        const std::string doc = std::string("{\"Format\": ") + o[0] + ", \"Identifier\": \"de\", \"DisplayName\": \"x\", \"Texts\": {}}";
        CHECK(ParseCatalog(doc, &known, c, err) && c.Format == std::stoi(o[1]));
        CHECK(c.FromNewer == (c.Format > kFormat));
    }

    // The opt-in for your own language files: off unless saved as true.
    CHECK(!OwnFilesFromSettings(nlohmann::json::object()));
    CHECK(!OwnFilesFromSettings(nlohmann::json::parse("{\"own_language_files\": false}")));
    CHECK(!OwnFilesFromSettings(nlohmann::json::parse("{\"own_language_files\": 1}")));
    CHECK(!OwnFilesFromSettings(nlohmann::json::parse("{\"own_language_files\": \"true\"}")));
    CHECK(!OwnFilesFromSettings(nlohmann::json::parse("[true]")));
    CHECK(OwnFilesFromSettings(nlohmann::json::parse("{\"own_language_files\": true}")));
    // Not tied to Advanced options any more.
    CHECK(!OwnFilesFromSettings(nlohmann::json::parse("{\"advanced_options\": true}")));
}

static void TestShippedFiles()
{
    const std::string enText = File("localization/en.json");
    CHECK(!enText.empty());
    Catalog en;
    std::string err;
    CHECK(ParseCatalog(enText, nullptr, en, err) && en.Id == "en" && en.Skipped == 0);
    std::unordered_set<std::string> known;
    for (const auto& kv : en.Texts)
    {
        known.insert(kv.first);
        CHECK(kv.first == kv.second);
    }
    CHECK(known.size() > 300);
    for (const char* path : {"localization/de.json", "localization/fr.json", "localization/es.json"})
    {
        Catalog c;
        const bool ok = ParseCatalog(File(path), &known, c, err);
        CHECK(ok);
        CHECK(c.Skipped == 0 && c.Unused == 0 && c.Format == kFormat);
        for (const std::string& n : c.SkipNotes) std::printf("  %s: %s\n", path, n.c_str());
        CHECK(PercentDone(c, known) == 100);
        if (PercentDone(c, known) < 100)
            for (const std::string& k : known)
                if (!c.Texts.count(k)) { std::printf("  %s is missing: %s\n", path, k.c_str()); break; }
    }
    Catalog t;
    CHECK(ParseCatalog(File("localization/template.json"), &known, t, err) && t.Skipped == 0 && t.Texts.empty());
}

void RunLocalizationTests()
{
    TestIds();
    TestSafeText();
    TestPlaceholders();
    TestParse();
    TestFormats();
    TestLookup();
    TestTranslate();
    TestShippedFiles();
}
