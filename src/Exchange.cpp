#include "Exchange.h"

#include "Localization.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "ChatLine.h"

namespace fs = std::filesystem;
using nlohmann::json;
using namespace MacroTree;

namespace
{
    constexpr size_t kMaxFileNameBytes = 80;
    constexpr int    kMaxReadDepth     = 16; // folder levels read from disk (junction loops, silly trees)

    std::string Lower(const std::string& s)
    {
        std::string out = s;
        for (char& c : out)
            if (static_cast<unsigned char>(c) < 0x80) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return out;
    }

    void ClearLinkFiles(Node& aNode)
    {
        aNode.LinkFile.clear();
        for (auto& c : aNode.Children) ClearLinkFiles(*c);
    }

    // Checks what a source gave: something there, and not too much.
    void Finish(Exchange::Package& aPkg)
    {
        if (!aPkg.Ok()) return;
        const size_t count = Tree::CountDescendants(aPkg.Items.Root());
        if (count == 0) aPkg.Error = "There's nothing to import in it.";
        else if (count > Exchange::kMaxImportItems)
            aPkg.Error = "It has " + std::to_string(count) + " folders and macros. At most " +
                         std::to_string(Exchange::kMaxImportItems) + " can be imported at once.";
    }

    bool ReadFile(const fs::path& aFile, size_t aMaxBytes, std::string& aOut, std::string& aError)
    {
        std::error_code ec;
        const auto size = fs::file_size(aFile, ec);
        if (ec) { aError = "It couldn't be read."; return false; }
        if (size > aMaxBytes)
        {
            aError = "It's too big (over " + std::to_string(aMaxBytes / 1024) + " KB).";
            return false;
        }
        std::ifstream in(aFile, std::ios::binary);
        if (!in) { aError = "It couldn't be opened."; return false; }
        aOut.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        if (aOut.size() >= 3 && aOut.compare(0, 3, "\xEF\xBB\xBF") == 0) aOut.erase(0, 3); // UTF-8 BOM
        return true;
    }

    std::string Utf8Name(const fs::path& aPath) { return aPath.u8string(); } // std::string in C++17

    // A macro from raw text; false (with a warning) if the text has no lines.
    bool AddTextMacro(Tree& aTree, Node* aParent, const std::string& aName, const std::string& aText,
                      std::vector<std::string>& aWarnings)
    {
        std::vector<Line> lines = ChatLine::ParseRaw(aText);
        if (lines.empty())
        {
            aWarnings.push_back("\"" + aName + "\" has no lines, so it was left out.");
            return false;
        }
        if (lines.size() > kMaxLines)
        {
            lines.resize(kMaxLines);
            aWarnings.push_back("\"" + aName + "\" has more than " + std::to_string(kMaxLines) +
                                " lines. Only the first " + std::to_string(kMaxLines) + " were kept.");
        }
        std::string name = Tree::CleanName(aName);
        if (name.empty()) name = "Imported macro";
        Result r = aTree.AddMacro(aParent, name);
        if (!r.Ok)
        {
            aWarnings.push_back("\"" + aName + "\" was left out: " + r.Error);
            return false;
        }
        r.Target->Lines = std::move(lines);
        return true;
    }

    struct FolderReader
    {
        Tree&                     Items;
        std::vector<std::string>& Warnings;
        size_t                    Files = 0;
        bool                      WarnedCount = false;
        bool                      WarnedDepth = false;

        void Read(const fs::path& aDir, Node* aParent, int aDepth)
        {
            std::error_code ec;
            std::vector<fs::directory_entry> entries;
            for (fs::directory_iterator it(aDir, fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec))
                entries.push_back(*it);
            std::sort(entries.begin(), entries.end(),
                      [](const fs::directory_entry& a, const fs::directory_entry& b) { return Lower(Utf8Name(a.path().filename())) < Lower(Utf8Name(b.path().filename())); });

            for (const fs::directory_entry& e : entries)
            {
                std::error_code ec2;
                if (e.is_directory(ec2))
                {
                    if (aDepth >= kMaxReadDepth) continue;
                    Node* into = aParent;
                    const std::string name = Utf8Name(e.path().filename());
                    Result r = Items.AddFolder(aParent, name);
                    if (r.Ok) into = r.Target;
                    else if (!WarnedDepth)
                    {
                        // Too deep (or an odd name): its files go into the folder above.
                        Warnings.push_back("Some folders are nested too deeply; their files were put in the folder above.");
                        WarnedDepth = true;
                    }
                    Read(e.path(), into, aDepth + 1);
                    if (into != aParent && into->Children.empty()) Items.Delete(*into); // no text files in it
                }
                else if (e.is_regular_file(ec2) && Lower(Utf8Name(e.path().extension())) == ".txt")
                {
                    if (Files >= Exchange::kMaxFolderFiles)
                    {
                        if (!WarnedCount)
                            Warnings.push_back("Only the first " + std::to_string(Exchange::kMaxFolderFiles) + " text files were read.");
                        WarnedCount = true;
                        return;
                    }
                    Files++;
                    std::string text, error;
                    const std::string name = Utf8Name(e.path().stem());
                    if (!ReadFile(e.path(), Exchange::kMaxTextBytes, text, error))
                    {
                        Warnings.push_back("\"" + Utf8Name(e.path().filename()) + "\" was left out. " + error);
                        continue;
                    }
                    AddTextMacro(Items, aParent, name, text, Warnings);
                }
            }
        }
    };

    // --- Planning -----------------------------------------------------------------------------

    // A folder things are planned into: the existing folder (if any) and the names taken there.
    struct Dest
    {
        const Node*           Existing = nullptr;
        int                   Level = 0;
        std::set<std::string> Folders, Macros; // lower-case names already there or planned

        Dest(const Node* aExisting, int aLevel) : Existing(aExisting), Level(aLevel)
        {
            if (!aExisting) return;
            for (const auto& c : aExisting->Children) (c->IsFolder() ? Folders : Macros).insert(Lower(c->Name));
        }

        const Node* Find(ENodeType aType, const std::string& aName) const
        {
            if (!Existing) return nullptr;
            const std::string lower = Lower(aName);
            for (const auto& c : Existing->Children)
                if (c->Type == aType && Lower(c->Name) == lower) return c.get();
            return nullptr;
        }
    };

    std::string FreeName(const std::string& aName, const std::set<std::string>& aTaken)
    {
        for (int n = 2;; n++)
        {
            const std::string candidate = Tree::CleanName(aName + " (" + std::to_string(n) + ")");
            if (!aTaken.count(Lower(candidate))) return candidate;
        }
    }

    size_t SubtreeSize(const Node& aNode) { return 1 + Tree::CountDescendants(aNode); }

    struct Planner
    {
        const Exchange::Options& Opt;
        Exchange::Plan&          Out;

        Exchange::EChoice ChoiceFor(const Node& aSrc) const
        {
            auto it = Opt.PerItem.find(&aSrc);
            Exchange::EChoice c = it != Opt.PerItem.end() ? it->second : aSrc.IsFolder() ? Opt.FolderConflict : Opt.MacroConflict;
            if (aSrc.IsMacro() && c == Exchange::EChoice::Merge) c = Exchange::EChoice::Rename;
            return c;
        }

        void Children(const Node& aSrcFolder, Dest& aDest, std::vector<Exchange::PlanItem>& aItems)
        {
            for (const auto& child : aSrcFolder.Children)
            {
                const Node& src = *child;
                Exchange::PlanItem item;
                item.Src  = &src;
                item.Name = src.Name;
                if (Opt.Unselected.count(&src)) NotSelected(src, item);
                else if (src.IsFolder()) Folder(src, aDest, item);
                else Macro(src, aDest, item);
                aItems.push_back(std::move(item));
            }
        }

        // Unticked: left out. Its contents are listed too (also unticked), so one can be ticked.
        void NotSelected(const Node& aSrc, Exchange::PlanItem& aItem)
        {
            aItem.Outcome = Exchange::EOutcome::NotSelected;
            Out.NotSelected++;
            for (const auto& c : aSrc.Children)
            {
                Exchange::PlanItem child;
                child.Src  = c.get();
                child.Name = c->Name;
                NotSelected(*c, child);
                aItem.Children.push_back(std::move(child));
            }
        }

        void Folder(const Node& aSrc, Dest& aDest, Exchange::PlanItem& aItem)
        {
            using Exchange::EChoice;
            using Exchange::EOutcome;
            const int level = aDest.Level + 1;
            // Nothing can exist that deep, so there's no name conflict to look at.
            if (level > kMaxFolderDepth)
            {
                if (Opt.Flatten)
                {
                    aItem.Outcome = EOutcome::Flattened;
                    Children(aSrc, aDest, aItem.Children); // its contents go where it would have gone
                }
                else
                {
                    aItem.Outcome = EOutcome::TooDeep;
                    Out.TooDeep += SubtreeSize(aSrc);
                }
                return;
            }

            if (aDest.Folders.count(Lower(aSrc.Name)))
            {
                aItem.Conflict = true;
                Out.Conflicts++;
                aItem.Choice = ChoiceFor(aSrc);
                const Node* existing = aDest.Find(ENodeType::Folder, aSrc.Name);
                EChoice choice = aItem.Choice;
                // Clashing with a folder this import adds itself: only a new name works.
                if (!existing && (choice == EChoice::Merge || choice == EChoice::Replace)) choice = EChoice::Rename;
                switch (choice)
                {
                case EChoice::Skip:
                    aItem.Outcome = EOutcome::Skipped;
                    Out.Skipped += SubtreeSize(aSrc);
                    return;
                case EChoice::Merge:
                {
                    aItem.Outcome = EOutcome::Merged;
                    aItem.Name    = existing->Name;
                    Out.Merged++;
                    Dest inner(existing, level);
                    Children(aSrc, inner, aItem.Children);
                    return;
                }
                case EChoice::Replace:
                {
                    aItem.Outcome = EOutcome::Replaced;
                    aItem.Name    = existing->Name;
                    Out.Replaced++;
                    Dest inner(nullptr, level); // its old contents go
                    Children(aSrc, inner, aItem.Children);
                    return;
                }
                case EChoice::Rename:
                    aItem.Outcome = EOutcome::Renamed;
                    aItem.Name    = FreeName(aSrc.Name, aDest.Folders);
                    Out.Renamed++;
                    break;
                }
            }
            aDest.Folders.insert(Lower(aItem.Name));
            Out.NewFolders++;
            Dest inner(nullptr, level);
            Children(aSrc, inner, aItem.Children);
        }

        void Macro(const Node& aSrc, Dest& aDest, Exchange::PlanItem& aItem)
        {
            using Exchange::EChoice;
            using Exchange::EOutcome;
            if (aDest.Macros.count(Lower(aSrc.Name)))
            {
                aItem.Conflict = true;
                Out.Conflicts++;
                aItem.Choice = ChoiceFor(aSrc);
                EChoice choice = aItem.Choice;
                const Node* existing = aDest.Find(ENodeType::Macro, aSrc.Name);
                if (!existing && choice == EChoice::Replace) choice = EChoice::Rename;
                if (choice == EChoice::Skip)
                {
                    aItem.Outcome = EOutcome::Skipped;
                    Out.Skipped++;
                    return;
                }
                if (choice == EChoice::Replace)
                {
                    aItem.Outcome = EOutcome::Replaced;
                    aItem.Name    = existing->Name;
                    Out.Replaced++;
                    return;
                }
                aItem.Outcome = EOutcome::Renamed;
                aItem.Name    = FreeName(aSrc.Name, aDest.Macros);
                Out.Renamed++;
            }
            aDest.Macros.insert(Lower(aItem.Name));
            Out.NewMacros++;
        }
    };

    // --- Applying -----------------------------------------------------------------------------

    Node* FindChild(Node& aFolder, ENodeType aType, const std::string& aName)
    {
        const std::string lower = Lower(aName);
        for (auto& c : aFolder.Children)
            if (c->Type == aType && Lower(c->Name) == lower) return c.get();
        return nullptr;
    }

    void CopyMacro(const Node& aSrc, Node& aDst, bool aKeepHotkey)
    {
        aDst.Lines     = aSrc.Lines;
        aDst.GameModes = aSrc.GameModes;
        aDst.MapIds    = aSrc.MapIds;
        aDst.LinkFile.clear();
        aDst.Hotkey    = aKeepHotkey ? aSrc.Hotkey : Hotkey::Combo();
    }

    // Adds a folder or macro, with "Name (2)" if the plan's name is taken after all.
    Node* AddNamed(Tree& aTree, Node& aDest, ENodeType aType, const std::string& aName)
    {
        Result r = aType == ENodeType::Folder ? aTree.AddFolder(&aDest, aName) : aTree.AddMacro(&aDest, aName);
        for (int n = 2; !r.Ok && n < 1000; n++)
        {
            if (aType == ENodeType::Folder && Tree::FolderLevel(aDest) + 1 > kMaxFolderDepth) return nullptr;
            const std::string name = Tree::CleanName(aName + " (" + std::to_string(n) + ")");
            r = aType == ENodeType::Folder ? aTree.AddFolder(&aDest, name) : aTree.AddMacro(&aDest, name);
        }
        return r.Ok ? r.Target : nullptr;
    }

    void ApplyItems(Tree& aTree, Node& aDest, const std::vector<Exchange::PlanItem>& aItems, const Exchange::Options& aOpt, size_t& aCount)
    {
        using Exchange::EOutcome;
        for (const Exchange::PlanItem& item : aItems)
        {
            const Node& src = *item.Src;
            switch (item.Outcome)
            {
            case EOutcome::Skipped:
            case EOutcome::TooDeep:
            case EOutcome::NotSelected:
                continue;
            case EOutcome::Flattened:
                ApplyItems(aTree, aDest, item.Children, aOpt, aCount);
                continue;
            case EOutcome::Merged:
                if (Node* existing = FindChild(aDest, ENodeType::Folder, item.Name))
                {
                    ApplyItems(aTree, *existing, item.Children, aOpt, aCount);
                    continue;
                }
                break; // gone since the plan was made: add it instead
            case EOutcome::Replaced:
                if (Node* existing = FindChild(aDest, src.Type, item.Name))
                {
                    aCount++;
                    if (src.IsFolder())
                    {
                        existing->Children.clear();
                        existing->Expanded = src.Expanded;
                        ApplyItems(aTree, *existing, item.Children, aOpt, aCount);
                    }
                    else CopyMacro(src, *existing, aOpt.KeepHotkeys);
                    continue;
                }
                break;
            case EOutcome::New:
            case EOutcome::Renamed:
                break;
            }

            Node* added = AddNamed(aTree, aDest, src.Type, item.Name);
            if (!added) continue;
            aCount++;
            if (src.IsFolder())
            {
                added->Expanded = src.Expanded;
                ApplyItems(aTree, *added, item.Children, aOpt, aCount);
            }
            else CopyMacro(src, *added, aOpt.KeepHotkeys);
        }
    }
}

namespace Exchange
{
    json ExportJson(const std::vector<const Node*>& aNodes)
    {
        json children = json::array();
        for (const Node* n : aNodes)
        {
            if (!n) continue;
            json j = MacroTree::NodeToJson(*n);
            // Link files are paths on this PC; they mean nothing elsewhere.
            std::vector<json*> stack{&j};
            while (!stack.empty())
            {
                json* cur = stack.back();
                stack.pop_back();
                cur->erase("link_file");
                auto it = cur->find("children");
                if (it != cur->end() && it->is_array())
                    for (json& c : *it) stack.push_back(&c);
            }
            children.push_back(std::move(j));
        }
        return json{{"format", kFormatVersion}, {"root", {{"children", std::move(children)}}}};
    }

    std::string ExportJsonText(const std::vector<const Node*>& aNodes) { return ExportJson(aNodes).dump(2); }

    std::string ExportText(const Node& aMacro) { return ChatLine::SerializeRaw(aMacro.Lines); }

    std::string FileNameFor(const std::string& aName, const std::string& aExt)
    {
        std::string s;
        for (char c : aName)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            s += (u < 0x20 || std::string("<>:\"/\\|?*").find(c) != std::string::npos) ? '_' : c;
        }
        // No spaces or dots at either end (Windows drops trailing ones).
        const size_t first = s.find_first_not_of(" .");
        s = first == std::string::npos ? std::string() : s.substr(first, s.find_last_not_of(" .") - first + 1);
        if (s.size() > kMaxFileNameBytes)
        {
            size_t cut = kMaxFileNameBytes;
            while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) cut--;
            s.resize(cut);
            const size_t last = s.find_last_not_of(" .");
            s.resize(last == std::string::npos ? 0 : last + 1);
        }
        if (s.empty()) s = "export";
        // Names Windows reserves for devices, with or without an extension.
        const std::string base = Lower(s.substr(0, s.find('.')));
        static const char* const kReserved[] = {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5",
                                                "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4",
                                                "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
        for (const char* r : kReserved)
            if (base == r) { s = "_" + s; break; }
        return s + aExt;
    }

    Package FromJsonText(const std::string& aText)
    {
        Package p;
        p.Source = "JSON";
        if (aText.size() > kMaxJsonBytes)
        {
            p.Error = "It's too big (over " + std::to_string(kMaxJsonBytes / (1024 * 1024)) + " MB).";
            return p;
        }
        // The text is untrusted (pasted, or a file from imports\ or a library file): anything that
        // throws while reading it becomes the package's error, so it never reaches the UI and the
        // game. FromPath, FromText and FromLibraryFile read their JSON through here.
        try
        {
            const json j = json::parse(aText, nullptr, false);
            if (j.is_discarded()) { p.Error = "It isn't valid JSON."; return p; }
            std::vector<std::string> warnings;
            if (!p.Items.FromJson(j, &warnings))
            {
                p.Error = "It isn't a Chat Messages export.";
                return p;
            }
            for (std::string& w : warnings)
                if (w.find("new ids") == std::string::npos) p.Warnings.push_back(std::move(w)); // ids are always new on import
            if (!p.Items.Trash().empty())
            {
                p.Warnings.push_back("Its trash bin (" + std::to_string(p.Items.Trash().size()) +
                                     " item(s)) isn't imported: only its library is.");
                p.Items.EmptyTrash();
            }
            ClearLinkFiles(p.Items.Root());
            Finish(p);
            return p;
        }
        catch (const std::exception&)
        {
            Package failed; // nothing half-read is kept
            failed.Source = "JSON";
            failed.Error  = "It couldn't be read.";
            return failed;
        }
    }

    Package FromRawText(const std::string& aName, const std::string& aText)
    {
        Package p;
        p.Source = aName;
        if (aText.size() > kMaxTextBytes)
        {
            p.Error = "It's too big (over " + std::to_string(kMaxTextBytes / 1024) + " KB).";
            return p;
        }
        std::vector<std::string> warnings;
        if (!AddTextMacro(p.Items, nullptr, aName, aText, warnings))
        {
            p.Error = "It has no lines to send.";
            return p;
        }
        p.Warnings = std::move(warnings);
        Finish(p);
        return p;
    }

    Package FromText(const std::string& aName, const std::string& aText)
    {
        const size_t first = aText.find_first_not_of(" \t\r\n");
        if (first != std::string::npos && aText[first] == '{') return FromJsonText(aText);
        return FromRawText(aName, aText);
    }

    Package FromPath(const fs::path& aPath)
    {
        Package p;
        p.Source = Utf8Name(aPath.filename());
        std::error_code ec;
        if (fs::is_directory(aPath, ec))
        {
            Result r = p.Items.AddFolder(nullptr, Utf8Name(aPath.filename()));
            Node* top = r.Ok ? r.Target : nullptr;
            FolderReader reader{p.Items, p.Warnings};
            reader.Read(aPath, top, 1);
            if (top && top->Children.empty()) p.Items.Delete(*top);
            if (Tree::CountDescendants(p.Items.Root()) == 0)
                p.Error = "No .txt files with lines were found in it.";
            Finish(p);
            return p;
        }
        if (!fs::is_regular_file(aPath, ec))
        {
            p.Error = "It wasn't found.";
            return p;
        }
        const std::string ext = Lower(Utf8Name(aPath.extension()));
        if (ext != ".json" && ext != ".txt")
        {
            p.Error = "Only .json and .txt files (or a folder of .txt files) can be imported.";
            return p;
        }
        std::string text, error;
        if (!ReadFile(aPath, ext == ".json" ? kMaxJsonBytes : kMaxTextBytes, text, error))
        {
            p.Error = error;
            return p;
        }
        Package read = ext == ".json" ? FromJsonText(text) : FromRawText(Utf8Name(aPath.stem()), text);
        read.Source = p.Source;
        return read;
    }

    bool IsLibraryFileName(const fs::path& aPath)
    {
        const std::string ext = Lower(Utf8Name(aPath.extension()));
        return ext == ".json" || ext == ".bak";
    }

    Package FromLibraryFile(const fs::path& aPath)
    {
        Package p;
        p.Source = Utf8Name(aPath.filename());
        std::error_code ec;
        if (!fs::is_regular_file(aPath, ec))
        {
            p.Error = fs::is_directory(aPath, ec) ? "It's a folder. A library is one file: macros.json (or macros.json.bak)."
                                                  : "It wasn't found.";
            return p;
        }
        if (!IsLibraryFileName(aPath))
        {
            p.Error = "A library file is a macros.json (or its backup, macros.json.bak).";
            return p;
        }
        std::string text, error;
        if (!ReadFile(aPath, kMaxJsonBytes, text, error))
        {
            p.Error = error;
            return p;
        }
        Package read = FromJsonText(text);
        if (!read.Ok() && read.Error == "It isn't a Chat Messages export.")
            read.Error = "It isn't a Chat Messages library (macros.json).";
        read.Source = p.Source;
        return read;
    }

    std::string LibraryFolderName(const std::string& aSource)
    {
        std::string stem = aSource;
        for (const char* ext : {".bak", ".json"})
        {
            const std::string e = ext;
            if (stem.size() >= e.size() && Lower(stem.substr(stem.size() - e.size())) == e) stem.erase(stem.size() - e.size());
        }
        stem = Tree::CleanName(stem);
        if (stem.empty() || Lower(stem) == "macros") return kLibraryFolderFallback;
        return Tree::CleanName("From " + stem);
    }

    Node* WrapInFolder(Package& aPkg, const std::string& aName)
    {
        Node& root = aPkg.Items.Root();
        if (root.Children.empty()) return nullptr;
        std::string name = Tree::CleanName(aName);
        if (name.empty()) name = kLibraryFolderFallback;
        // Take the top level out first, so the new folder's name can't clash with one of them.
        std::vector<std::unique_ptr<Node>> items = std::move(root.Children);
        root.Children.clear();
        Result r = aPkg.Items.AddFolder(nullptr, name);
        Node* folder = r.Ok ? r.Target : nullptr;
        if (!folder) // can't happen with an empty top level; put everything back
        {
            root.Children = std::move(items);
            return nullptr;
        }
        for (auto& item : items)
        {
            item->Parent = folder;
            folder->Children.push_back(std::move(item));
        }
        return folder;
    }

    const char* ChoiceLabel(EChoice aChoice)
    {
        switch (aChoice)
        {
        case EChoice::Rename:  return TX("Keep both (rename)");
        case EChoice::Skip:    return TX("Skip");
        case EChoice::Replace: return TX("Replace");
        case EChoice::Merge:   return TX("Merge");
        }
        return "";
    }

    bool IsSelected(const Options& aOptions, const Node& aNode) { return !aOptions.Unselected.count(&aNode); }

    namespace
    {
        void SetSubtree(Options& aOptions, const Node& aNode, bool aOn)
        {
            if (aOn) aOptions.Unselected.erase(&aNode);
            else aOptions.Unselected.insert(&aNode);
            for (const auto& c : aNode.Children) SetSubtree(aOptions, *c, aOn);
        }

        bool AnySelectedInside(const Options& aOptions, const Node& aFolder)
        {
            for (const auto& c : aFolder.Children)
                if (IsSelected(aOptions, *c)) return true;
            return false;
        }

        bool AnyUnselectedInside(const Options& aOptions, const Node& aFolder)
        {
            for (const auto& c : aFolder.Children)
                if (!IsSelected(aOptions, *c) || AnyUnselectedInside(aOptions, *c)) return true;
            return false;
        }
    }

    void SetSelected(Options& aOptions, const Node& aNode, bool aOn)
    {
        SetSubtree(aOptions, aNode, aOn);
        // The folders above (the package's root has no parent and is never listed).
        for (const Node* p = aNode.Parent; p && p->Parent; p = p->Parent)
        {
            if (aOn) aOptions.Unselected.erase(p);
            else if (!AnySelectedInside(aOptions, *p)) aOptions.Unselected.insert(p);
        }
    }

    void SelectAll(Options& aOptions, const Tree& aPackage, bool aOn)
    {
        aOptions.Unselected.clear();
        if (!aOn)
            for (const auto& c : aPackage.Root().Children) SetSubtree(aOptions, *c, false);
    }

    bool IsPartlySelected(const Options& aOptions, const Node& aFolder)
    {
        return aFolder.IsFolder() && IsSelected(aOptions, aFolder) && AnyUnselectedInside(aOptions, aFolder);
    }

    Plan BuildPlan(const Tree& aTree, const Node& aTarget, const Tree& aPackage, const Options& aOptions)
    {
        (void)aTree;
        Plan plan;
        Planner planner{aOptions, plan};
        Dest dest(&aTarget, Tree::FolderLevel(aTarget));
        planner.Children(aPackage.Root(), dest, plan.Items);
        return plan;
    }

    size_t Apply(Tree& aTree, Node& aTarget, const Plan& aPlan, const Options& aOptions)
    {
        size_t count = 0;
        ApplyItems(aTree, aTarget, aPlan.Items, aOptions, count);
        if (count) aTarget.Expanded = true;
        return count;
    }
}
