// Standalone tests for src/Exchange.cpp (import and export).

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "Exchange.h"
#include "TestCheck.h"

using namespace Exchange;
using namespace MacroTree;
namespace fs = std::filesystem;

namespace
{
    Node* AddM(Tree& t, Node* parent, const std::string& name, const std::string& message)
    {
        Node* m = t.AddMacro(parent, name).Target;
        Line l;
        l.Channel = EChannel::Say;
        l.Message = message;
        m->Lines.push_back(l);
        return m;
    }

    Node* Child(Node& folder, const std::string& name, ENodeType type)
    {
        for (auto& c : folder.Children)
            if (c->Name == name && c->Type == type) return c.get();
        return nullptr;
    }

    // Library with folder "Raids" holding macro "Stack" (says "old"), and macro "Hello" at the top.
    void MakeLibrary(Tree& t)
    {
        Node* raids = t.AddFolder(nullptr, "Raids").Target;
        AddM(t, raids, "Stack", "old");
        AddM(t, nullptr, "Hello", "hi");
    }

    // Package from text, checked to be readable.
    Package Pkg(const std::string& json)
    {
        Package p = FromJsonText(json);
        CHECK(p.Ok());
        return p;
    }

    void TestFileNames()
    {
        CHECK(FileNameFor("Raids", ".json") == "Raids.json");
        CHECK(FileNameFor("a/b\\c:d*e?f\"g<h>i|j", ".txt") == "a_b_c_d_e_f_g_h_i_j.txt");
        CHECK(FileNameFor("  .hidden. ", ".json") == "hidden.json");
        CHECK(FileNameFor("", ".json") == "export.json");
        CHECK(FileNameFor("...", ".json") == "export.json");
        CHECK(FileNameFor("con", ".txt") == "_con.txt");
        CHECK(FileNameFor("LPT1", ".txt") == "_LPT1.txt");
        CHECK(FileNameFor("console", ".txt") == "console.txt");
        CHECK(FileNameFor(std::string(200, 'x'), ".json").size() == 80 + 5);
        // A long UTF-8 name isn't cut in the middle of a character.
        std::string accents;
        for (int i = 0; i < 60; i++) accents += "\xC3\xA9"; // é
        const std::string cut = FileNameFor(accents, "");
        CHECK(cut.size() == 80);
        CHECK((static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80);
    }

    void TestExport()
    {
        Tree t;
        MakeLibrary(t);
        Node* raids = Child(t.Root(), "Raids", ENodeType::Folder);
        Node* stack = Child(*raids, "Stack", ENodeType::Macro);
        stack->LinkFile = "C:\\secret\\stack.txt";
        stack->Hotkey = Hotkey::FromString("Ctrl+F1");

        const nlohmann::json j = ExportJson({raids});
        CHECK(j["format"] == kFormatVersion);
        CHECK(j["root"]["children"].size() == 1);
        const nlohmann::json& m = j["root"]["children"][0]["children"][0];
        CHECK(m["name"] == "Stack");
        CHECK(!m.contains("link_file")); // a path on this PC: never exported
        CHECK(m["hotkey"] == "Ctrl+F1");
        CHECK(stack->LinkFile == "C:\\secret\\stack.txt"); // the library itself is untouched

        // An export reads back as the same items.
        Package p = FromJsonText(ExportJsonText({raids, Child(t.Root(), "Hello", ENodeType::Macro)}));
        CHECK(p.Ok());
        CHECK(p.Items.Root().Children.size() == 2);
        Node* back = Child(p.Items.Root(), "Raids", ENodeType::Folder);
        CHECK(back && back->Children.size() == 1 && back->Children[0]->Lines[0].Message == "old");
        CHECK(back && back->Children[0]->LinkFile.empty());
        CHECK(p.Warnings.empty());

        Node* hello = Child(t.Root(), "Hello", ENodeType::Macro);
        Line w;
        w.Channel = EChannel::Whisper;
        w.WhisperTo = "Some Name";
        w.Message = "psst";
        hello->Lines.push_back(w);
        CHECK(ExportText(*hello) == "/s hi\n/w [Some Name] psst");
    }

    void TestReadText()
    {
        Package p = FromRawText("Callouts", "/s one\n\n/p two\r\n");
        CHECK(p.Ok());
        CHECK(p.Items.Root().Children.size() == 1);
        Node* m = p.Items.Root().Children[0].get();
        CHECK(m->IsMacro() && m->Name == "Callouts" && m->Lines.size() == 2);
        CHECK(m->Lines[1].Channel == EChannel::Party);

        CHECK(!FromRawText("Empty", "\n  \n").Ok());
        CHECK(FromRawText("", "/s hi").Items.Root().Children[0]->Name == "Imported macro");

        std::string many;
        for (size_t i = 0; i < kMaxLines + 5; i++) many += "/s line\n";
        Package big = FromRawText("Many", many);
        CHECK(big.Ok() && big.Items.Root().Children[0]->Lines.size() == kMaxLines);
        CHECK(big.Warnings.size() == 1);

        CHECK(!FromRawText("Huge", std::string(kMaxTextBytes + 1, 'a')).Ok());

        // FromText picks the format.
        CHECK(FromText("Pasted", "  {\"format\":1,\"root\":{\"children\":[{\"type\":\"macro\",\"name\":\"J\",\"lines\":[]}]}}").Items.Root().Children[0]->Name == "J");
        CHECK(FromText("Pasted", "/s hi").Items.Root().Children[0]->Name == "Pasted");
    }

    // Whether the library (or any json) can be saved: dump() with nlohmann's default, strict
    // handling throws on text that isn't UTF-8.
    bool Dumps(const nlohmann::json& j)
    {
        try { (void)j.dump(2); return true; }
        catch (const std::exception&) { return false; }
    }

    void TestNotUtf8()
    {
        const std::string bad = "\xEF\xBF\xBD"; // U+FFFD
        // A .txt saved as Windows-1252 (ä, ö as single bytes): imported with U+FFFD in their place,
        // and a warning.
        Package p = FromRawText("Ansi", "/s K\xE4se\r\n/w [B\xF6" "b] hi\r\n");
        CHECK(p.Ok() && p.Warnings.size() == 1);
        Node* m = p.Ok() ? p.Items.Root().Children[0].get() : nullptr;
        CHECK(m && m->Lines.size() == 2);
        CHECK(m && m->Lines[0].Message == "K" + bad + "se");
        CHECK(m && m->Lines[1].Channel == EChannel::Whisper && m->Lines[1].WhisperTo == "B" + bad + "b");
        // So the library still saves, and the macro exports.
        CHECK(Dumps(p.Items.ToJson()));
        CHECK(m && Dumps(ExportJson({m})));
        // UTF-8 text: no warning.
        CHECK(FromRawText("Utf8", "/s K\xC3\xA4se").Warnings.empty());
        // Pasted text goes the same way.
        CHECK(FromText("Pasted", "/s \xFF").Items.Root().Children[0]->Lines[0].Message == bad);

        // Text that isn't UTF-8 from anywhere else still exports, as U+FFFD, instead of throwing.
        Tree t;
        Node* raw = AddM(t, nullptr, "Raw", "bad \xFF");
        std::string text;
        try { text = ExportJsonText({raw}); }
        catch (const std::exception&) { text.clear(); }
        Package back = FromJsonText(text);
        CHECK(back.Ok() && back.Items.Root().Children[0]->Lines[0].Message == "bad " + bad);
    }

    void TestReadJson()
    {
        CHECK(!FromJsonText("not json").Ok());
        CHECK(!FromJsonText("[1,2]").Ok());
        CHECK(!FromJsonText("{\"format\":1}").Ok());
        CHECK(!FromJsonText("{\"format\":1,\"root\":{\"children\":[]}}").Ok()); // nothing in it
        // A whole macros.json is fine too, and link files in it are dropped.
        Package p = Pkg(R"J({"format":1,"root":{"children":[
            {"type":"macro","id":"00000000000000aa","name":"M","link_file":"x.txt","lines":[{"channel":"say","message":"a"}]}]}})J");
        CHECK(p.Items.Root().Children[0]->LinkFile.empty());
    }

    bool HasWarning(const Package& aPkg, const std::string& aPart)
    {
        for (const std::string& w : aPkg.Warnings)
            if (w.find(aPart) != std::string::npos) return true;
        return false;
    }

    // FromJsonText(aText), with aThrew set (instead of the tests crashing) if it throws.
    Package ReadNoThrow(const std::string& aText, bool& aThrew)
    {
        aThrew = false;
        try { return FromJsonText(aText); }
        catch (...) { aThrew = true; return Package{}; }
    }

    // A "format" of the wrong type never throws out of FromJsonText: it isn't an export.
    void TestReadJsonBadFormat()
    {
        const std::string body = R"J(,"root":{"children":[{"type":"macro","name":"M","lines":[{"channel":"say","message":"a"}]}]}})J";
        bool threw = true;
        for (const char* bad : {"null", "\"1\"", "{}", "{\"v\":1}", "[1]"})
        {
            Package p = ReadNoThrow(std::string("{\"format\":") + bad + body, threw);
            CHECK(!threw);
            CHECK(!p.Ok() && p.Error == "It isn't a Chat Messages export." && p.Items.Root().Children.empty());
        }
        // The report's example.
        Package example = ReadNoThrow("{\"format\":null,\"root\":{\"children\":[]}}", threw);
        CHECK(!threw && !example.Ok());
        // Any other wrongly typed value that throws while reading becomes the package's error.
        Package odd = ReadNoThrow(R"J({"format":1,"root":{"children":[{"type":"folder","name":"F","expanded":null,"children":[
            {"type":"macro","name":"M","lines":[{"channel":"say","message":"a"}]}]}]}})J", threw);
        CHECK(!threw && (odd.Ok() || (odd.Error == "It couldn't be read." && odd.Items.Root().Children.empty())));

        // Numbers and true/false read as before: missing, 1, 1.5 and true aren't newer; 2 is.
        CHECK(!HasWarning(Pkg("{" + body.substr(1)), "newer version"));
        CHECK(!HasWarning(Pkg("{\"format\":1" + body), "newer version"));
        CHECK(!HasWarning(Pkg("{\"format\":1.5" + body), "newer version"));
        CHECK(!HasWarning(Pkg("{\"format\":true" + body), "newer version"));
        CHECK(HasWarning(Pkg("{\"format\":2" + body), "newer version"));
    }

    void TestPlanSimple()
    {
        Tree t;
        MakeLibrary(t);
        Package p = Pkg(R"J({"format":1,"root":{"children":[
            {"type":"macro","name":"New one","lines":[{"channel":"map","message":"m"}]},
            {"type":"folder","name":"Fractals","children":[{"type":"macro","name":"Cm","lines":[]}]}]}})J");
        Options o;
        Plan plan = BuildPlan(t, t.Root(), p.Items, o);
        CHECK(plan.Items.size() == 2);
        CHECK(plan.Items[0].Outcome == EOutcome::New && !plan.Items[0].Conflict);
        CHECK(plan.NewMacros == 2 && plan.NewFolders == 1 && plan.Conflicts == 0);
        CHECK(plan.Changes() == 3);

        // Nothing changed by planning.
        CHECK(t.Root().Children.size() == 2);

        CHECK(Apply(t, t.Root(), plan, o) == 3);
        Node* fr = Child(t.Root(), "Fractals", ENodeType::Folder);
        CHECK(fr && fr->Children.size() == 1 && fr->Children[0]->Name == "Cm");
        Node* nm = Child(t.Root(), "New one", ENodeType::Macro);
        CHECK(nm && nm->Lines.size() == 1 && nm->Lines[0].Channel == EChannel::Map);
        CHECK(nm && nm->Id != 0 && t.Find(nm->Id) == nm);
    }

    const char* kConflicts = R"J({"format":1,"root":{"children":[
        {"type":"macro","name":"hello","hotkey":"Ctrl+F2","lines":[{"channel":"say","message":"new hi"}]},
        {"type":"folder","name":"RAIDS","children":[
            {"type":"macro","name":"Stack","lines":[{"channel":"squad","message":"new"}]},
            {"type":"macro","name":"Spread","lines":[]}]}]}})J";

    void TestPlanConflicts()
    {
        // Defaults: macros are kept as both (renamed), folders merge, hotkeys aren't kept.
        {
            Tree t;
            MakeLibrary(t);
            Package p = Pkg(kConflicts);
            Options o;
            CHECK(!o.KeepHotkeys);
            Plan plan = BuildPlan(t, t.Root(), p.Items, o);
            CHECK(plan.Conflicts == 3); // hello, RAIDS, Stack
            CHECK(plan.Items[0].Conflict && plan.Items[0].Outcome == EOutcome::Renamed && plan.Items[0].Name == "hello (2)");
            CHECK(plan.Items[1].Outcome == EOutcome::Merged && plan.Items[1].Name == "Raids");
            CHECK(plan.Items[1].Children[0].Outcome == EOutcome::Renamed && plan.Items[1].Children[0].Name == "Stack (2)");
            CHECK(plan.Items[1].Children[1].Outcome == EOutcome::New);
            CHECK(plan.Merged == 1 && plan.Renamed == 2 && plan.NewMacros == 3 && plan.NewFolders == 0);

            Apply(t, t.Root(), plan, o);
            CHECK(t.Root().Children.size() == 3); // Raids, Hello, hello (2)
            Node* raids = Child(t.Root(), "Raids", ENodeType::Folder);
            CHECK(raids->Children.size() == 3);
            CHECK(Child(*raids, "Stack", ENodeType::Macro)->Lines[0].Message == "old");
            CHECK(Child(*raids, "Stack (2)", ENodeType::Macro)->Lines[0].Message == "new");
            CHECK(Child(t.Root(), "hello (2)", ENodeType::Macro)->Hotkey.Empty()); // a shared file's hotkey isn't bound
        }
        // Keep hotkeys ticked: the imported hotkey comes along, on a new macro and on a replaced one.
        {
            Tree t;
            MakeLibrary(t);
            Package p = Pkg(kConflicts);
            Options o;
            o.KeepHotkeys = true;
            Apply(t, t.Root(), BuildPlan(t, t.Root(), p.Items, o), o);
            CHECK(Child(t.Root(), "hello (2)", ENodeType::Macro)->Hotkey == Hotkey::FromString("Ctrl+F2"));

            Tree r;
            MakeLibrary(r);
            Node* hello = Child(r.Root(), "Hello", ENodeType::Macro);
            o.MacroConflict = EChoice::Replace;
            Apply(r, r.Root(), BuildPlan(r, r.Root(), p.Items, o), o);
            CHECK(hello->Lines[0].Message == "new hi" && hello->Hotkey == Hotkey::FromString("Ctrl+F2"));
        }
        // Replace keeps the existing item (its place and id) and swaps its contents.
        {
            Tree t;
            MakeLibrary(t);
            Node* hello = Child(t.Root(), "Hello", ENodeType::Macro);
            const uint64_t helloId = hello->Id;
            Package p = Pkg(kConflicts);
            Options o;
            o.MacroConflict = EChoice::Replace;
            o.KeepHotkeys = false;
            Plan plan = BuildPlan(t, t.Root(), p.Items, o);
            CHECK(plan.Items[0].Outcome == EOutcome::Replaced && plan.Items[0].Name == "Hello");
            CHECK(plan.Replaced == 2 && plan.NewMacros == 1);
            Apply(t, t.Root(), plan, o);
            CHECK(t.Root().Children.size() == 2);
            CHECK(hello->Id == helloId && hello->Lines[0].Message == "new hi" && hello->Name == "Hello");
            CHECK(hello->Hotkey.Empty()); // hotkeys not kept
            Node* raids = Child(t.Root(), "Raids", ENodeType::Folder);
            CHECK(raids->Children.size() == 2 && Child(*raids, "Stack", ENodeType::Macro)->Lines[0].Message == "new");
        }
        // Skip leaves the existing items alone, a skipped folder skips its contents.
        {
            Tree t;
            MakeLibrary(t);
            Package p = Pkg(kConflicts);
            Options o;
            o.MacroConflict  = EChoice::Skip;
            o.FolderConflict = EChoice::Skip;
            Plan plan = BuildPlan(t, t.Root(), p.Items, o);
            CHECK(plan.Skipped == 4); // hello + RAIDS with 2 macros
            CHECK(plan.Changes() == 0);
            CHECK(Apply(t, t.Root(), plan, o) == 0);
            CHECK(Tree::CountDescendants(t.Root()) == 3);
        }
        // Replacing a folder empties it first.
        {
            Tree t;
            MakeLibrary(t);
            AddM(t, Child(t.Root(), "Raids", ENodeType::Folder), "Old only", "x");
            Package p = Pkg(kConflicts);
            Options o;
            o.FolderConflict = EChoice::Replace;
            Plan plan = BuildPlan(t, t.Root(), p.Items, o);
            CHECK(plan.Items[1].Outcome == EOutcome::Replaced);
            CHECK(plan.Items[1].Children[0].Outcome == EOutcome::New); // the old Stack goes, so no clash
            Apply(t, t.Root(), plan, o);
            Node* raids = Child(t.Root(), "Raids", ENodeType::Folder);
            CHECK(raids->Children.size() == 2 && !Child(*raids, "Old only", ENodeType::Macro));
        }
        // Renaming a folder makes a second one.
        {
            Tree t;
            MakeLibrary(t);
            Package p = Pkg(kConflicts);
            Options o;
            o.FolderConflict = EChoice::Rename;
            Plan plan = BuildPlan(t, t.Root(), p.Items, o);
            CHECK(plan.Items[1].Name == "RAIDS (2)" && plan.Items[1].Children[0].Outcome == EOutcome::New);
            Apply(t, t.Root(), plan, o);
            CHECK(Child(t.Root(), "RAIDS (2)", ENodeType::Folder)->Children.size() == 2);
        }
        // A per-item choice wins over the general one; Merge makes no sense for a macro.
        {
            Tree t;
            MakeLibrary(t);
            Package p = Pkg(kConflicts);
            Options o;
            o.PerItem[p.Items.Root().Children[0].get()] = EChoice::Skip;
            o.PerItem[p.Items.Root().Children[1]->Children[0].get()] = EChoice::Merge;
            Plan plan = BuildPlan(t, t.Root(), p.Items, o);
            CHECK(plan.Items[0].Outcome == EOutcome::Skipped);
            CHECK(plan.Items[1].Children[0].Outcome == EOutcome::Renamed);
        }
        // A folder and a macro may share a name: no conflict.
        {
            Tree t;
            t.AddFolder(nullptr, "Same");
            Package p = Pkg(R"J({"format":1,"root":{"children":[{"type":"macro","name":"Same","lines":[]}]}})J");
            Plan plan = BuildPlan(t, t.Root(), p.Items, Options());
            CHECK(plan.Conflicts == 0 && plan.Items[0].Outcome == EOutcome::New);
        }
        // Into a folder other than the top level.
        {
            Tree t;
            MakeLibrary(t);
            Node* raids = Child(t.Root(), "Raids", ENodeType::Folder);
            Package p = Pkg(R"J({"format":1,"root":{"children":[{"type":"macro","name":"stack","lines":[]}]}})J");
            Plan plan = BuildPlan(t, *raids, p.Items, Options());
            CHECK(plan.Items[0].Outcome == EOutcome::Renamed && plan.Items[0].Name == "stack (2)");
        }
        // Two clashing names inside one import: the second gets "(3)".
        {
            Tree t;
            MakeLibrary(t);
            Package p = Pkg(R"J({"format":1,"root":{"children":[{"type":"macro","name":"Hello","lines":[]},{"type":"macro","name":"Hello (2)","lines":[]}]}})J");
            Plan plan = BuildPlan(t, t.Root(), p.Items, Options());
            CHECK(plan.Items[0].Name == "Hello (2)");
            CHECK(plan.Items[1].Conflict && plan.Items[1].Name == "Hello (2) (2)");
            Apply(t, t.Root(), plan, Options());
            CHECK(Child(t.Root(), "Hello (2) (2)", ENodeType::Macro) != nullptr);
        }
        // SEC-018: a taken name of 99-100 bytes. Cutting "name (n)" back to 100 bytes gave the taken
        // name again for every n, and planning never ended. The name is now cut to fit " (n)".
        for (size_t len : {size_t(98), size_t(99), size_t(100)})
        {
            const std::string longName(len, 'a');
            Tree t;
            t.AddMacro(nullptr, longName);
            Package p = Pkg(R"J({"format":1,"root":{"children":[{"type":"macro","name":")J" + longName + R"J(","lines":[]}]}})J");
            Plan plan = BuildPlan(t, t.Root(), p.Items, Options());
            CHECK(plan.Items.size() == 1 && plan.Items[0].Outcome == EOutcome::Renamed);
            const std::string& got = plan.Items[0].Name;
            CHECK(got.size() <= kMaxNameLength && got != longName);
            CHECK(got.size() >= 4 && got.compare(got.size() - 4, 4, " (2)") == 0);
            CHECK(Apply(t, t.Root(), plan, Options()) == 1);
            CHECK(Child(t.Root(), got, ENodeType::Macro) != nullptr);
        }
    }

    // SEC-027: each imported item was checked against every sibling (and found again by a scan),
    // so 5,000 macros (the import limit) into one folder took 19 s in this (unoptimised) test build.
    void TestApplyMany()
    {
        const int kCount = static_cast<int>(kMaxImportItems); // the most one import may hold
        std::string text = R"J({"format":1,"root":{"children":[)J";
        for (int i = 0; i < kCount; i++)
            text += std::string(i ? "," : "") + R"J({"type":"macro","name":"m)J" + std::to_string(i) + R"J(","lines":[]})J";
        text += "]}}";
        Package p = Pkg(text);
        CHECK(p.Ok());

        Tree t;
        const auto start = std::chrono::steady_clock::now();
        CHECK(Apply(t, t.Root(), BuildPlan(t, t.Root(), p.Items, Options()), Options()) == static_cast<size_t>(kCount));
        // Again into the same folder: every name is taken, so all are renamed ("m0 (2)"...).
        CHECK(Apply(t, t.Root(), BuildPlan(t, t.Root(), p.Items, Options()), Options()) == static_cast<size_t>(kCount));
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        CHECK(seconds < 10.0); // about a second now; the old way took over a minute
        CHECK(t.Root().Children.size() == static_cast<size_t>(2 * kCount));
        CHECK(Child(t.Root(), "m0 (2)", ENodeType::Macro) != nullptr);

        // Replace: the existing macros are updated in place, nothing is added.
        Options replace;
        replace.MacroConflict = EChoice::Replace;
        CHECK(Apply(t, t.Root(), BuildPlan(t, t.Root(), p.Items, replace), replace) == static_cast<size_t>(kCount));
        CHECK(t.Root().Children.size() == static_cast<size_t>(2 * kCount));
    }

    // A package of folders nested aDepth deep (F1 > F2 > ...), with a macro "M<n>" in each.
    std::string Nested(int aDepth)
    {
        std::string s = "{\"format\":1,\"root\":{\"children\":[";
        std::string close;
        for (int i = 1; i <= aDepth; i++)
        {
            s += "{\"type\":\"folder\",\"name\":\"F" + std::to_string(i) + "\",\"children\":[{\"type\":\"macro\",\"name\":\"M" +
                 std::to_string(i) + "\",\"lines\":[]}" + (i < aDepth ? "," : "");
            close += "]}";
        }
        return s + close + "]}}";
    }

    void TestPlanDepth()
    {
        // 4 levels into a folder at level 3: F4 would be level 7.
        Tree t;
        Node* a = t.AddFolder(nullptr, "A").Target;
        Node* b = t.AddFolder(a, "B").Target;
        Node* c = t.AddFolder(b, "C").Target;
        Package p = Pkg(Nested(4));

        Plan plan = BuildPlan(t, *c, p.Items, Options());
        const PlanItem& f3 = plan.Items[0].Children[1].Children[1];
        CHECK(f3.Name == "F3");
        CHECK(f3.Children[1].Outcome == EOutcome::TooDeep);
        CHECK(plan.TooDeep == 2); // F4 and M4
        CHECK(plan.NewFolders == 3 && plan.NewMacros == 3);
        Apply(t, *c, plan, Options());
        Node* f3n = Child(*Child(*Child(*c, "F1", ENodeType::Folder), "F2", ENodeType::Folder), "F3", ENodeType::Folder);
        CHECK(f3n && f3n->Children.size() == 1 && Tree::FolderLevel(*f3n) == 6);

        // Flattened: F4 goes, M4 lands in F3.
        Tree t2;
        Node* c2 = t2.AddFolder(t2.AddFolder(t2.AddFolder(nullptr, "A").Target, "B").Target, "C").Target;
        Options o;
        o.Flatten = true;
        Plan flat = BuildPlan(t2, *c2, p.Items, o);
        CHECK(flat.TooDeep == 0 && flat.NewMacros == 4 && flat.NewFolders == 3);
        CHECK(flat.Items[0].Children[1].Children[1].Children[1].Outcome == EOutcome::Flattened);
        Apply(t2, *c2, flat, o);
        Node* f3b = Child(*Child(*Child(*c2, "F1", ENodeType::Folder), "F2", ENodeType::Folder), "F3", ENodeType::Folder);
        CHECK(f3b && f3b->Children.size() == 2 && Child(*f3b, "M4", ENodeType::Macro));

        // Flattening can bring two macros of the same name together: the second is renamed.
        Package clash = Pkg(R"J({"format":1,"root":{"children":[{"type":"macro","name":"X","lines":[]},
            {"type":"folder","name":"Deep","children":[{"type":"macro","name":"X","lines":[]}]}]}})J");
        Tree t3;
        Node* lvl = nullptr;
        for (int i = 0; i < kMaxFolderDepth; i++) lvl = t3.AddFolder(lvl, "L" + std::to_string(i)).Target;
        Plan cp = BuildPlan(t3, *lvl, clash.Items, o);
        CHECK(cp.Items[1].Outcome == EOutcome::Flattened);
        CHECK(cp.Items[1].Children[0].Outcome == EOutcome::Renamed && cp.Items[1].Children[0].Name == "X (2)");
        Apply(t3, *lvl, cp, o);
        CHECK(lvl->Children.size() == 2);
    }

    void WriteFile(const fs::path& p, const std::string& text)
    {
        fs::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << text;
    }

    void TestFromPath()
    {
        const fs::path dir = fs::temp_directory_path() / "chatmsg_exchange_tests";
        std::error_code ec;
        fs::remove_all(dir, ec);
        WriteFile(dir / "Pack" / "b hello.txt", "\xEF\xBB\xBF/s hi\r\n/p party\r\n");
        WriteFile(dir / "Pack" / "a.txt", "/m map");
        WriteFile(dir / "Pack" / "notes.md", "ignored");
        WriteFile(dir / "Pack" / "empty.txt", "\n\n");
        WriteFile(dir / "Pack" / "Sub" / "deep.txt", "/g hi");
        WriteFile(dir / "Pack" / "Nothing" / "x.md", "no txt here");
        WriteFile(dir / "one.txt", "/s single");
        WriteFile(dir / "ansi.txt", "/s caf\xE9\r\n");
        WriteFile(dir / "bad.json", "{oops");
        WriteFile(dir / "other.csv", "a,b");

        Package folder = FromPath(dir / "Pack");
        CHECK(folder.Ok());
        Node* pack = folder.Items.Root().Children.empty() ? nullptr : folder.Items.Root().Children[0].get();
        CHECK(pack && pack->IsFolder() && pack->Name == "Pack");
        if (pack)
        {
            // Folders and files are read in name order; empty folders and .txt files with no lines are left out.
            CHECK(Child(*pack, "a", ENodeType::Macro) && Child(*pack, "b hello", ENodeType::Macro));
            CHECK(Child(*pack, "b hello", ENodeType::Macro)->Lines.size() == 2);
            CHECK(!Child(*pack, "empty", ENodeType::Macro));
            Node* sub = Child(*pack, "Sub", ENodeType::Folder);
            CHECK(sub && sub->Children.size() == 1);
            CHECK(!Child(*pack, "Nothing", ENodeType::Folder));
        }
        CHECK(folder.Warnings.size() == 1); // empty.txt

        Package one = FromPath(dir / "one.txt");
        CHECK(one.Ok() && one.Items.Root().Children[0]->Name == "one" && one.Source == "one.txt");
        Package ansi = FromPath(dir / "ansi.txt"); // not UTF-8: read with U+FFFD, and a warning
        CHECK(ansi.Ok() && ansi.Warnings.size() == 1 && ansi.Items.Root().Children[0]->Lines[0].Message == "caf\xEF\xBF\xBD");
        CHECK(!FromPath(dir / "bad.json").Ok());
        CHECK(!FromPath(dir / "other.csv").Ok());
        CHECK(!FromPath(dir / "missing.txt").Ok());
        CHECK(!FromPath(dir / "Pack" / "Nothing").Ok());

        fs::remove_all(dir, ec);
    }

    // Package: folder "Pack" > (macro "A", folder "Sub" > macro "B"), and macro "C" at the top.
    struct SelPkg
    {
        Tree  t;
        Node* pack = nullptr;
        Node* a = nullptr;
        Node* sub = nullptr;
        Node* b = nullptr;
        Node* c = nullptr;
        SelPkg()
        {
            pack = t.AddFolder(nullptr, "Pack").Target;
            a    = AddM(t, pack, "A", "a");
            sub  = t.AddFolder(pack, "Sub").Target;
            b    = AddM(t, sub, "B", "b");
            c    = AddM(t, nullptr, "C", "c");
        }
    };

    void TestSelection()
    {
        SelPkg k;
        Options o;
        CHECK(IsSelected(o, *k.pack) && IsSelected(o, *k.b));   // everything ticked at first

        SetSelected(o, *k.pack, false);                          // a folder takes everything inside
        CHECK(!IsSelected(o, *k.pack) && !IsSelected(o, *k.a) && !IsSelected(o, *k.sub) && !IsSelected(o, *k.b));
        CHECK(IsSelected(o, *k.c));

        SetSelected(o, *k.b, true);                              // one macro: its folders come too
        CHECK(IsSelected(o, *k.b) && IsSelected(o, *k.sub) && IsSelected(o, *k.pack));
        CHECK(!IsSelected(o, *k.a));
        CHECK(IsPartlySelected(o, *k.pack) && !IsPartlySelected(o, *k.sub));

        Tree lib;
        Plan plan = BuildPlan(lib, lib.Root(), k.t, o);
        CHECK(plan.NotSelected == 1);                            // A
        CHECK(plan.NewFolders == 2 && plan.NewMacros == 2);      // Pack, Sub, B, C
        CHECK(Apply(lib, lib.Root(), plan, o) == 4);
        Node* pack = Child(lib.Root(), "Pack", ENodeType::Folder);
        CHECK(pack && pack->Children.size() == 1 && pack->Children[0]->Name == "Sub");
        CHECK(Child(lib.Root(), "C", ENodeType::Macro) != nullptr);

        // Unticking the last ticked item inside a folder unticks the folder.
        SetSelected(o, *k.b, false);
        CHECK(!IsSelected(o, *k.sub) && !IsSelected(o, *k.pack));

        // Select all / deselect all; nothing ticked = nothing to import.
        SelectAll(o, k.t, false);
        CHECK(!IsSelected(o, *k.c) && !IsSelected(o, *k.b));
        Tree lib2;
        Plan none = BuildPlan(lib2, lib2.Root(), k.t, o);
        CHECK(none.Changes() == 0 && none.NotSelected == 5);
        CHECK(none.Items.size() == 2 && none.Items[0].Children.size() == 2); // still listed, to tick
        SelectAll(o, k.t, true);
        CHECK(o.Unselected.empty());

        // A not-selected item with a taken name isn't counted as a conflict.
        Tree lib3;
        lib3.AddMacro(nullptr, "C");
        SetSelected(o, *k.c, false);
        Plan p3 = BuildPlan(lib3, lib3.Root(), k.t, o);
        CHECK(p3.Conflicts == 0);
    }

    void TestTrashNotImported()
    {
        Tree src;
        MakeLibrary(src);
        src.MoveToTrash(*src.Root().Children[1], "t"); // "Hello" to the bin
        const std::string text = src.ToJson().dump();
        Package p = FromJsonText(text);                // a whole macros.json, bin and all
        CHECK(p.Ok());
        CHECK(p.Items.Trash().empty());
        CHECK(p.Items.Root().Children.size() == 1);   // only "Raids"
        bool warned = false;
        for (const std::string& w : p.Warnings) warned = warned || w.find("trash bin") != std::string::npos;
        CHECK(warned);

        // Exports never hold the bin: they're built from tree nodes.
        const nlohmann::json ex = ExportJson({src.Root().Children[0].get()});
        CHECK(!ex.contains("trash"));
    }

    // Importing someone's whole library (their macros.json): it goes into one new folder.
    void TestLibraryImport()
    {
        CHECK(LibraryFolderName("Alex.json") == "From Alex");
        CHECK(LibraryFolderName("macros.json") == kLibraryFolderFallback);
        CHECK(LibraryFolderName("MACROS.json.bak") == kLibraryFolderFallback);
        CHECK(LibraryFolderName("Alex macros.json.bak") == "From Alex macros");
        CHECK(LibraryFolderName("") == kLibraryFolderFallback);
        CHECK(IsLibraryFileName("macros.json") && IsLibraryFileName("macros.json.BAK"));
        CHECK(!IsLibraryFileName("one.txt"));

        Tree src;
        MakeLibrary(src); // "Raids" (folder) and "Hello" (macro) at the top
        src.MoveToTrash(*src.Root().Children[1], "t");
        src.AddMacro(nullptr, "Hello").Target->Hotkey.Key = 0x70; // F1, a hotkey of theirs
        src.Root().Children[0]->Children[0]->LinkFile = "theirs.txt";

        const fs::path dir = fs::temp_directory_path() / "chatmsg_library_tests";
        std::error_code ec;
        fs::remove_all(dir, ec);
        const std::string text = src.ToJson().dump();
        WriteFile(dir / "macros.json", text);
        WriteFile(dir / "macros.json.bak", text);
        WriteFile(dir / "notes.txt", "/s hi");
        WriteFile(dir / "broken.json", "{oops");
        WriteFile(dir / "export.json", R"({"format":1})");
        fs::create_directories(dir / "Folder", ec);

        Package p = FromLibraryFile(dir / "macros.json");
        CHECK(p.Ok() && p.Source == "macros.json");
        CHECK(p.Items.Trash().empty()); // their bin is left out, with a warning
        CHECK(!p.Warnings.empty());
        CHECK(FromLibraryFile(dir / "macros.json.bak").Ok());
        CHECK(!FromLibraryFile(dir / "notes.txt").Ok());
        CHECK(!FromLibraryFile(dir / "broken.json").Ok());
        CHECK(FromLibraryFile(dir / "export.json").Error.find("library") != std::string::npos);
        CHECK(!FromLibraryFile(dir / "Folder").Ok());
        CHECK(!FromLibraryFile(dir / "missing.json").Ok());
        fs::remove_all(dir, ec);

        // Wrapped: one folder at the top, with their whole top level inside, in order.
        Node* folder = WrapInFolder(p, "  From Alex  ");
        CHECK(folder && folder->Name == "From Alex" && folder->Parent == &p.Items.Root());
        CHECK(p.Items.Root().Children.size() == 1);
        CHECK(folder->Children.size() == 2 && folder->Children[0]->Name == "Raids" && folder->Children[1]->Name == "Hello");
        CHECK(folder->Children[0]->Parent == folder);
        CHECK(folder->Children[0]->Children[0]->LinkFile.empty()); // link files are never taken

        // A folder named like one of theirs is fine (it's a level above them).
        Package q = FromJsonText(text);
        Node* same = WrapInFolder(q, "Raids");
        CHECK(same && same->Name == "Raids" && Child(*same, "Raids", ENodeType::Folder));
        Package empty;
        CHECK(WrapInFolder(empty, "x") == nullptr);
        Package blank = FromJsonText(text);
        CHECK(WrapInFolder(blank, "   ")->Name == kLibraryFolderFallback);

        // Into my library: only the new folder at my top level; a taken name gets "(2)"; their
        // hotkeys are dropped when Keep hotkeys is off.
        Tree mine;
        mine.AddFolder(nullptr, "Raids");
        mine.AddFolder(nullptr, "From Alex");
        Options o;
        o.FolderConflict = EChoice::Rename;
        o.KeepHotkeys    = false;
        Plan plan = BuildPlan(mine, mine.Root(), p.Items, o);
        CHECK(plan.Items.size() == 1 && plan.Conflicts == 1 && plan.Items[0].Outcome == EOutcome::Renamed);
        CHECK(Apply(mine, mine.Root(), plan, o) > 0);
        Node* added = Child(mine.Root(), "From Alex (2)", ENodeType::Folder);
        CHECK(added && Child(*added, "Raids", ENodeType::Folder) && Child(*added, "Hello", ENodeType::Macro));
        CHECK(added && Child(*added, "Hello", ENodeType::Macro)->Hotkey.Empty());
        CHECK(mine.Root().Children.size() == 3);

        // Their deepest folders would be one level deeper: flagged as too deep, as usual.
        Tree deep;
        Node* cur = nullptr;
        for (int i = 1; i <= MacroTree::kMaxFolderDepth; i++) cur = deep.AddFolder(cur, "D" + std::to_string(i)).Target;
        Package d = FromJsonText(deep.ToJson().dump());
        WrapInFolder(d, "From Deep");
        Tree mine2;
        Plan dp = BuildPlan(mine2, mine2.Root(), d.Items, Options{});
        CHECK(dp.TooDeep == 1);
    }
}

void RunExchangeTests()
{
    TestFileNames();
    TestExport();
    TestReadText();
    TestNotUtf8();
    TestReadJson();
    TestReadJsonBadFormat();
    TestPlanSimple();
    TestPlanConflicts();
    TestPlanDepth();
    TestFromPath();
    TestSelection();
    TestTrashNotImported();
    TestLibraryImport();
    TestApplyMany();
}
