// Standalone tests for the trash bin in src/MacroTree.cpp (no Nexus, no ImGui).

#include <string>

#include <nlohmann/json.hpp>

#include "MacroTree.h"
#include "TestCheck.h"

using namespace MacroTree;
using nlohmann::json;

namespace
{
    // Raids > Wing 3 > (Gorseval macro, Sabetha macro)
    struct Lib
    {
        Tree  t;
        Node* raids = nullptr;
        Node* wing  = nullptr;
        Node* gors  = nullptr;
        Node* sab   = nullptr;
        Lib()
        {
            raids = t.AddFolder(nullptr, "Raids").Target;
            wing  = t.AddFolder(raids, "Wing 3").Target;
            gors  = t.AddMacro(wing, "Gorseval").Target;
            sab   = t.AddMacro(wing, "Sabetha").Target;
            gors->Lines.push_back({EChannel::Squad, "Eggs!", "", false});
        }
    };

    void TestMoveAndRestore()
    {
        Lib l;
        const uint64_t id = l.gors->Id;
        CHECK(l.t.MoveToTrash(*l.gors, "2026-11-21 14:03").Ok);
        CHECK(l.t.Find(id) == nullptr);                    // out of the tree: no send, edit or hotkey
        CHECK(l.wing->Children.size() == 1);
        CHECK(l.t.Trash().size() == 1);
        CHECK(l.t.TrashIndexOf(id) == 0);
        CHECK(l.t.Trash()[0].Item->Parent == nullptr);
        CHECK(Tree::FromPathText(l.t.Trash()[0]) == "Raids > Wing 3");
        CHECK(l.t.OriginalFolderExists(0));

        Result r = l.t.Restore(0, ERestoreTo::Original);
        CHECK(r.Ok && r.Note.empty());
        CHECK(l.t.Trash().empty());
        CHECK(l.t.Find(id) && l.t.Find(id)->Parent == l.wing);
        CHECK(l.wing->Children[0]->Id == id);                // back in its old place (first)
        CHECK(l.t.Find(id)->Lines.size() == 1 && l.t.Find(id)->Lines[0].Message == "Eggs!");

        // Newest first.
        l.t.MoveToTrash(*l.t.Find(id), "a");
        l.t.MoveToTrash(*l.sab, "b");
        CHECK(l.t.Trash().size() == 2 && l.t.Trash()[0].DeletedAt == "b");

        CHECK(!l.t.MoveToTrash(l.t.Root(), "x").Ok);
        CHECK(!l.t.Restore(5, ERestoreTo::TopLevel).Ok);
    }

    void TestMissingFolder()
    {
        Lib l;
        const uint64_t gorsId = l.gors->Id, wingId = l.wing->Id;
        l.t.MoveToTrash(*l.gors, "t");
        CHECK(l.t.Delete(*l.wing).Ok); // the folder it came from is gone (deleted for good)
        CHECK(!l.t.OriginalFolderExists(0));
        Result r = l.t.Restore(0, ERestoreTo::Original);
        CHECK(!r.Ok && !r.Error.empty());
        CHECK(l.t.Trash().size() == 1); // still in the bin

        // Recreate the folders: "Raids" is reused, "Wing 3" is made again with its old id.
        r = l.t.Restore(0, ERestoreTo::RecreateFolders);
        CHECK(r.Ok);
        Node* g = l.t.Find(gorsId);
        CHECK(g && g->Parent->Name == "Wing 3" && g->Parent->Parent == l.raids);
        CHECK(g->Parent->Id == wingId);
        CHECK(l.raids->Children.size() == 1);

        // A folder of the same name is reused rather than doubled.
        Lib m;
        m.t.MoveToTrash(*m.gors, "t");
        m.t.Delete(*m.wing);
        m.t.AddFolder(m.raids, "wing 3");
        CHECK(m.t.Restore(0, ERestoreTo::RecreateFolders).Ok);
        CHECK(m.raids->Children.size() == 1 && m.raids->Children[0]->Name == "wing 3");

        // Or to the top level.
        Lib n;
        n.t.MoveToTrash(*n.gors, "t");
        n.t.Delete(*n.raids);
        CHECK(n.t.Restore(0, ERestoreTo::TopLevel).Ok);
        CHECK(n.t.Root().Children.size() == 1 && n.t.Root().Children[0]->Name == "Gorseval");
    }

    void TestNameTaken()
    {
        Lib l;
        l.t.MoveToTrash(*l.gors, "t");
        l.t.AddMacro(l.wing, "gorseval");
        Result r = l.t.Restore(0, ERestoreTo::Original);
        CHECK(r.Ok && !r.Note.empty());
        CHECK(r.Target && r.Target->Name == "Gorseval (2)");
    }

    void TestDepth()
    {
        // A folder 2 levels tall, deleted from level 4, then its old parent moved deeper.
        Tree t;
        Node* cur = nullptr;
        for (int i = 1; i <= 4; i++) cur = t.AddFolder(cur, "L" + std::to_string(i)).Target;
        Node* tall = t.AddFolder(cur, "Tall").Target;   // level 5
        t.AddFolder(tall, "Inner");                      // level 6
        t.MoveToTrash(*tall, "t");
        Node* other = t.AddFolder(nullptr, "Other").Target;
        CHECK(t.Move(*t.Root().Children[0], *other, EDrop::Into).Ok); // L1..L4 now one level deeper
        Result r = t.Restore(0, ERestoreTo::Original);
        CHECK(!r.Ok);                                    // would be 7 deep
        CHECK(t.Trash().size() == 1);
        CHECK(t.Restore(0, ERestoreTo::TopLevel).Ok);
    }

    void TestJson()
    {
        Lib l;
        const uint64_t id = l.gors->Id;
        l.t.MoveToTrash(*l.gors, "2026-11-21 14:03");
        const json j = l.t.ToJson();
        CHECK(j.contains("trash") && j["trash"].size() == 1);

        Tree back;
        std::vector<std::string> warnings;
        CHECK(back.FromJson(j, &warnings));
        CHECK(warnings.empty());
        CHECK(back.Trash().size() == 1);
        const TrashItem& ti = back.Trash()[0];
        CHECK(ti.Item->Id == id && ti.Item->Name == "Gorseval" && ti.DeletedAt == "2026-11-21 14:03");
        CHECK(ti.FromNames.size() == 2 && ti.FromNames[1] == "Wing 3" && ti.FromIndex == 0);
        CHECK(back.Find(id) == nullptr);
        CHECK(back.Restore(0, ERestoreTo::Original).Ok);
        CHECK(back.Find(id) != nullptr);

        // No bin: no "trash" key (files stay as before).
        Tree empty;
        CHECK(!empty.ToJson().contains("trash"));

        // An id in both the tree and the bin gets a new id in the bin.
        json dup = l.t.ToJson();
        dup["trash"][0]["item"]["id"] = dup["root"]["children"][0]["id"];
        Tree d;
        warnings.clear();
        CHECK(d.FromJson(dup, &warnings));
        CHECK(d.Trash()[0].Item->Id != d.Root().Children[0]->Id);
        CHECK(!warnings.empty());

        // Damaged entries are skipped.
        json bad = l.t.ToJson();
        bad["trash"].push_back(42);
        bad["trash"].push_back({{"from_ids", json::array()}});
        Tree b;
        CHECK(b.FromJson(bad));
        CHECK(b.Trash().size() == 1);
    }
}

void RunTrashTests()
{
    TestMoveAndRestore();
    TestMissingFolder();
    TestNameTaken();
    TestDepth();
    TestJson();
}
