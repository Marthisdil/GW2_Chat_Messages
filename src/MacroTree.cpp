#include "MacroTree.h"

#include "ChatLine.h"
#include "Cooldown.h"
#include "Localization.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <random>
#include <map>
#include <set>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace MacroTree
{
    namespace
    {
        struct ChannelInfo { const char* Key; const char* Label; };

        constexpr ChannelInfo kChannels[] = {
            {"current", TX("Current channel")},
            {"emote",   TX("Emote")},
            {"say",     TX("Say")},
            {"map",     TX("Map")},
            {"party",   TX("Party")},
            {"squad",   TX("Squad")},
            {"team",    TX("Team")},
            {"reply",   TX("Reply")},
            {"whisper", TX("Whisper")},
            {"guild",   TX("Guild")},
            {"guild1",  TX("Guild 1")},
            {"guild2",  TX("Guild 2")},
            {"guild3",  TX("Guild 3")},
            {"guild4",  TX("Guild 4")},
            {"guild5",  TX("Guild 5")},
            {"guild6",  TX("Guild 6")},
        };
        static_assert(sizeof(kChannels) / sizeof(kChannels[0]) == static_cast<size_t>(EChannel::Count),
                      "kChannels must list every EChannel");

        // Guards against stack overflow on hostile or corrupt files. Far above kMaxFolderDepth.
        constexpr int kMaxParseDepth = 64;

        std::string Lower(const std::string& s)
        {
            std::string out = s;
            for (char& c : out)
                if (static_cast<unsigned char>(c) < 0x80) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return out;
        }

        std::string IdToString(uint64_t aId)
        {
            char buf[17];
            std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(aId));
            return buf;
        }

        uint64_t IdFromString(const std::string& s)
        {
            if (s.empty() || s.size() > 16) return 0;
            uint64_t v = 0;
            for (char c : s)
            {
                int d;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else return 0;
                v = (v << 4) | static_cast<uint64_t>(d);
            }
            return v;
        }

        const char* TypeWord(ENodeType t) { return t == ENodeType::Folder ? "folder" : "macro"; }

        Node* FindIn(const Node& aNode, uint64_t aId)
        {
            for (const auto& child : aNode.Children)
            {
                if (child->Id == aId) return child.get();
                if (Node* found = FindIn(*child, aId)) return found;
            }
            return nullptr;
        }

        size_t IndexInParent(const Node& aNode)
        {
            const auto& siblings = aNode.Parent->Children;
            for (size_t i = 0; i < siblings.size(); i++)
                if (siblings[i].get() == &aNode) return i;
            return siblings.size();
        }

        json ModesJson(uint8_t aModes)
        {
            json modes = json::array();
            if (aModes & GM_PvE) modes.push_back("pve");
            if (aModes & GM_WvW) modes.push_back("wvw");
            if (aModes & GM_PvP) modes.push_back("pvp");
            return modes;
        }

        json NodeJson(const Node& n)
        {
            json j;
            j["type"] = TypeWord(n.Type);
            j["id"]   = IdToString(n.Id);
            j["name"] = n.Name;
            if (n.Favourite) j["favourite"] = true;
            if (n.IsFolder())
            {
                j["expanded"] = n.Expanded;
                if (HasFilter(n)) // a folder without a filter keeps the format it had before
                {
                    j["game_modes"] = ModesJson(n.GameModes);
                    j["map_ids"]    = n.MapIds;
                }
                json children = json::array();
                for (const auto& c : n.Children) children.push_back(NodeJson(*c));
                j["children"] = std::move(children);
            }
            else
            {
                json lines = json::array();
                for (const Line& l : n.Lines)
                {
                    json jl = {{"channel", ChannelKey(l.Channel)}, {"message", l.Message}};
                    if (l.Channel == EChannel::Whisper) jl["whisper_to"] = l.WhisperTo;
                    if (l.Channel == EChannel::Squad) jl["squad_broadcast"] = l.SquadBroadcast;
                    lines.push_back(std::move(jl));
                }
                j["lines"] = std::move(lines);
                j["game_modes"] = ModesJson(n.GameModes);
                j["map_ids"]    = n.MapIds;
                if (!n.LinkFile.empty()) j["link_file"] = n.LinkFile;
                if (!n.Hotkey.Empty()) j["hotkey"] = Hotkey::ToString(n.Hotkey);
                if (n.CooldownSec > 0) j["cooldown"] = n.CooldownSec;
            }
            return j;
        }

        std::string StringField(const json& j, const char* aKey)
        {
            auto it = j.find(aKey);
            return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
        }

        struct Parser
        {
            std::set<uint64_t> SeenIds;
            std::vector<Node*>           NeedIdNodes; // given ids after parsing, so fresh ids can't collide
            std::vector<std::string>*    Warnings = nullptr;
            bool                         WarnedDepth = false;

            size_t                       Dropped = 0; // warnings past kMaxWarnings

            void Warn(std::string s)
            {
                if (!Warnings) return;
                if (Warnings->size() < kMaxWarnings) Warnings->push_back(std::move(s));
                else Dropped++;
            }

            // Parses the children of a folder json into aFolder. aLevel = aFolder's folder level.
            void ParseChildren(const json& aJson, Node& aFolder, int aLevel)
            {
                auto it = aJson.find("children");
                if (it == aJson.end() || !it->is_array()) return;
                for (const json& jc : *it) ParseNode(jc, aFolder, aLevel);
                MakeNamesUnique(aFolder);
            }

            // Parses one folder or macro json into a new last child of aFolder. aLevel = aFolder's
            // folder level. Reads jc in place: never copy untrusted json, because a json copy recurses
            // once per nesting level and a hostile file can nest it deep enough to overflow the stack.
            void ParseNode(const json& jc, Node& aFolder, int aLevel)
            {
                if (!jc.is_object()) { Warn(TR("Skipped an entry that isn't a folder or macro.")); return; }
                const std::string type = StringField(jc, "type");
                ENodeType t;
                if (type == "folder") t = ENodeType::Folder;
                else if (type == "macro") t = ENodeType::Macro;
                else { Warn(Localization::Fmt(TR("Skipped an entry of unknown type \"%s\"."), type.c_str())); return; }

                if (t == ENodeType::Folder && aLevel + 1 > kMaxParseDepth)
                {
                    Warn(TR("Skipped folders nested too deeply to read."));
                    return;
                }

                auto node = std::make_unique<Node>();
                node->Type   = t;
                node->Parent = &aFolder;
                node->Name   = Tree::CleanName(StringField(jc, "name"));
                if (node->Name.empty()) node->Name = t == ENodeType::Folder ? TR("New folder") : TR("New macro");

                const uint64_t id = IdFromString(StringField(jc, "id"));
                // The reserved Examples id is kept only on a folder (and only once, like any id).
                const bool allowed = id != 0 && (id != kReservedExamplesId || t == ENodeType::Folder);
                if (allowed && SeenIds.insert(id).second) node->Id = id;
                else NeedIdNodes.push_back(node.get());

                if (t == ENodeType::Folder)
                {
                    if (aLevel + 1 > kMaxFolderDepth && !WarnedDepth)
                    {
                        Warn(Localization::Fmt(TR("Some folders are nested deeper than %d levels. They were kept, "
                                                  "but you can't add more folders inside them."), kMaxFolderDepth));
                        WarnedDepth = true;
                    }
                    // Not value(): it throws on a wrongly typed key. Missing or not a bool = open.
                    auto ex = jc.find("expanded");
                    node->Expanded = ex == jc.end() || !ex->is_boolean() || ex->get<bool>();
                    ParseChildren(jc, *node, aLevel + 1);
                }
                else
                {
                    ParseMacro(jc, *node);
                }
                ParseFilter(jc, *node);
                // "favourite" (folders and macros): true or false; anything else is dropped.
                auto fav = jc.find("favourite");
                if (fav != jc.end())
                {
                    if (fav->is_boolean()) node->Favourite = fav->get<bool>();
                    else Warn(Localization::Fmt(TR("\"%s\": its favourite mark isn't true or false and was removed."),
                                                node->Name.c_str()));
                }
                aFolder.Children.push_back(std::move(node));
            }

            // "game_modes" and "map_ids" (folders and macros). Missing = everywhere.
            void ParseFilter(const json& j, Node& n)
            {
                auto modes = j.find("game_modes");
                if (modes != j.end() && modes->is_array())
                {
                    uint8_t m = GM_None;
                    for (const json& jm : *modes)
                    {
                        if (!jm.is_string()) continue;
                        const std::string s = jm.get<std::string>();
                        if (s == "pve") m |= GM_PvE;
                        else if (s == "wvw") m |= GM_WvW;
                        else if (s == "pvp") m |= GM_PvP;
                    }
                    n.GameModes = m == GM_None ? static_cast<uint8_t>(GM_All) : m;
                }
                // At most kMaxMapIds, as in the editor (SEC-033): every frame walks these lists for
                // each item inside a folder, so a file must not bring a huge one.
                auto maps = j.find("map_ids");
                if (maps != j.end() && maps->is_array())
                {
                    for (const json& jm : *maps)
                    {
                        if (!jm.is_number_integer()) continue;
                        if (n.MapIds.size() >= kMaxMapIds)
                        {
                            Warn(Localization::Fmt(TR("\"%s\": it listed more than %zu maps; only the first %zu were kept."),
                                                   n.Name.c_str(), kMaxMapIds, kMaxMapIds));
                            break;
                        }
                        n.MapIds.push_back(jm.get<int>());
                    }
                }
            }

            void ParseMacro(const json& j, Node& n)
            {
                auto lines = j.find("lines");
                if (lines != j.end() && lines->is_array())
                {
                    for (const json& jl : *lines)
                    {
                        if (!jl.is_object()) continue;
                        // At most kMaxLines, as in the editor and the .txt import (SEC-034): a file
                        // could otherwise bring a million lines, drawn every frame and saved each time.
                        if (n.Lines.size() >= kMaxLines)
                        {
                            Warn(Localization::Fmt(TR("\"%s\" has more than %zu lines. Only the first %zu were kept."),
                                                   n.Name.c_str(), kMaxLines, kMaxLines));
                            break;
                        }
                        Line l;
                        l.Channel        = ChannelFromKey(StringField(jl, "channel"));
                        // From a file (maybe someone else's): no control characters to type.
                        l.Message        = ChatLine::ControlsToSpaces(StringField(jl, "message"));
                        l.WhisperTo      = ChatLine::ControlsToSpaces(StringField(jl, "whisper_to"));
                        auto sb = jl.find("squad_broadcast");
                        l.SquadBroadcast = sb != jl.end() && sb->is_boolean() && sb->get<bool>();
                        n.Lines.push_back(std::move(l));
                    }
                }
                n.LinkFile = StringField(j, "link_file");
                const std::string hk = StringField(j, "hotkey");
                if (!hk.empty())
                {
                    const Hotkey::Combo c = Hotkey::FromString(hk);
                    if (Hotkey::KeepOnLoad(c)) n.Hotkey = c; // an old plain mouse hotkey is kept, but never fires
                    else Warn(Localization::Fmt(TR("Macro \"%s\": hotkey \"%s\" isn't valid and was removed."), n.Name.c_str(), hk.c_str()));
                }
                // Whole seconds, 0 to Cooldown::kMaxSec. Longer is cut to the maximum; anything
                // else (negative, a fraction, text) is dropped, so the macro has no cooldown.
                auto cd = j.find("cooldown");
                if (cd != j.end())
                {
                    // Parsed text gives unsigned numbers; a json built in memory (ToJson) signed ones.
                    if (cd->is_number_unsigned() || (cd->is_number_integer() && cd->get<int64_t>() >= 0))
                    {
                        const uint64_t v = cd->is_number_unsigned() ? cd->get<uint64_t>()
                                                                     : static_cast<uint64_t>(cd->get<int64_t>());
                        if (v > static_cast<uint64_t>(Cooldown::kMaxSec))
                        {
                            n.CooldownSec = Cooldown::kMaxSec;
                            Warn(Localization::Fmt(TR("Macro \"%s\": its cooldown was longer than %d seconds and was cut to %d."),
                                                   n.Name.c_str(), Cooldown::kMaxSec, Cooldown::kMaxSec));
                        }
                        else n.CooldownSec = static_cast<int>(v);
                    }
                    else Warn(Localization::Fmt(TR("Macro \"%s\": its cooldown isn't a whole number of seconds and was removed."),
                                                n.Name.c_str()));
                }
            }

            // Two siblings of the same type with the same name: rename the later one "Name (2)" etc.
            // The first free number is picked, skipping names any sibling (earlier or later) holds.
            // Lookups use sets, and each name remembers the next number to try, so a file with
            // thousands of copies of one name takes N log N time, not a rescan per candidate.
            // Ordered sets, not hash sets: names crafted to share one hash bucket can't slow them
            // down (SEC-019).
            void MakeNamesUnique(Node& aFolder)
            {
                // Per type (0 = folder, 1 = macro), lower case: every name a sibling holds (a renamed
                // sibling's old name stays taken: the earlier sibling it repeated still holds it), the
                // names of the siblings before the current one, and the next number to try per name.
                std::set<std::string>      taken[2], earlier[2];
                std::map<std::string, int> next[2];
                int                                  cutNext[2] = {2, 2};
                for (const auto& c : aFolder.Children) taken[c->IsFolder() ? 0 : 1].insert(Lower(c->Name));

                for (const auto& c : aFolder.Children)
                {
                    Node& a = *c;
                    const int ti = a.IsFolder() ? 0 : 1;
                    const std::string old = a.Name;
                    const std::string lower = Lower(old);
                    if (earlier[ti].count(lower))
                    {
                        // A number already tried for this name is still taken, so carry on from there.
                        int& n = next[ti].try_emplace(lower, 2).first->second;
                        bool named = false;
                        for (; !named; n++)
                        {
                            const std::string raw = old + " (" + std::to_string(n) + ")";
                            // Once the cut to kMaxNameLength takes ")" and two more characters, this and
                            // every later candidate repeat one already tried (or the name itself), so
                            // all are taken and none is left (the old loop never ended here).
                            if (raw.size() >= kMaxNameLength + 3) break;
                            std::string candidate = Tree::CleanName(raw);
                            if (taken[ti].insert(Lower(candidate)).second) { a.Name = std::move(candidate); named = true; }
                        }
                        // No room left for a number: cut the name shorter so the whole " (n)" fits.
                        for (int& m = cutNext[ti]; !named; m++)
                        {
                            std::string candidate = Tree::NumberedName(old, m);
                            if (taken[ti].insert(Lower(candidate)).second) { a.Name = std::move(candidate); named = true; }
                        }
                        Warn(Localization::Fmt(a.Type == ENodeType::Folder ? TR("Renamed duplicate folder \"%s\" to \"%s\".")
                                                                      : TR("Renamed duplicate macro \"%s\" to \"%s\"."),
                                               old.c_str(), a.Name.c_str()));
                    }
                    earlier[ti].insert(Lower(a.Name));
                }
            }
        };
    }

    const char* ChannelKey(EChannel c)
    {
        const size_t i = static_cast<size_t>(c);
        return i < static_cast<size_t>(EChannel::Count) ? kChannels[i].Key : kChannels[0].Key;
    }

    const char* ChannelLabel(EChannel c)
    {
        const size_t i = static_cast<size_t>(c);
        return i < static_cast<size_t>(EChannel::Count) ? kChannels[i].Label : kChannels[0].Label;
    }

    EChannel ChannelFromKey(const std::string& aKey)
    {
        for (size_t i = 0; i < static_cast<size_t>(EChannel::Count); i++)
            if (aKey == kChannels[i].Key) return static_cast<EChannel>(i);
        return EChannel::Current;
    }

    json NodeToJson(const Node& aNode) { return NodeJson(aNode); }

    Tree::Tree() : m_Root(std::make_unique<Node>())
    {
        m_Root->Type = ENodeType::Folder;
        m_Root->Expanded = true;
    }

    Node* Tree::Find(uint64_t aId) const
    {
        if (aId == 0) return nullptr;
        return FindIn(*m_Root, aId);
    }

    int Tree::FolderLevel(const Node& aNode)
    {
        const Node* n = aNode.IsFolder() ? &aNode : aNode.Parent;
        int level = 0;
        while (n && n->Parent) { level++; n = n->Parent; }
        return level;
    }

    int Tree::FolderHeight(const Node& aNode)
    {
        if (!aNode.IsFolder()) return 0;
        int deepest = 0;
        for (const auto& c : aNode.Children) deepest = std::max(deepest, FolderHeight(*c));
        return 1 + deepest;
    }

    bool Tree::IsAncestorOf(const Node& aAncestor, const Node& aNode)
    {
        for (const Node* n = aNode.Parent; n; n = n->Parent)
            if (n == &aAncestor) return true;
        return false;
    }

    size_t Tree::CountDescendants(const Node& aNode, size_t* aFolders, size_t* aMacros)
    {
        size_t total = 0;
        for (const auto& c : aNode.Children)
        {
            total++;
            if (c->IsFolder()) { if (aFolders) (*aFolders)++; }
            else if (aMacros) (*aMacros)++;
            total += CountDescendants(*c, aFolders, aMacros);
        }
        return total;
    }

    std::string Tree::CleanName(const std::string& aName)
    {
        std::string s = aName;
        for (char& c : s)
            if (static_cast<unsigned char>(c) < 0x20) c = ' '; // no newlines or tabs in names
        const size_t first = s.find_first_not_of(' ');
        if (first == std::string::npos) return {};
        s = s.substr(first, s.find_last_not_of(' ') - first + 1);
        if (s.size() > kMaxNameLength)
        {
            size_t cut = kMaxNameLength;
            // Don't split a UTF-8 sequence: back up over continuation bytes (10xxxxxx).
            while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) cut--;
            s.resize(cut);
            const size_t last = s.find_last_not_of(' ');
            s.resize(last == std::string::npos ? 0 : last + 1);
        }
        return s;
    }

    std::string Tree::NumberedName(const std::string& aName, int aN)
    {
        const std::string suffix = " (" + std::to_string(aN) + ")";
        std::string base = aName;
        if (base.size() + suffix.size() > kMaxNameLength)
        {
            size_t cut = kMaxNameLength - suffix.size();
            while (cut > 0 && (static_cast<unsigned char>(base[cut]) & 0xC0) == 0x80) cut--;
            base.resize(cut);
            while (!base.empty() && base.back() == ' ') base.pop_back();
        }
        return CleanName(base + suffix);
    }

    namespace
    {
        void CollectIds(const Node& aNode, std::set<uint64_t>& aOut)
        {
            for (const auto& c : aNode.Children)
            {
                aOut.insert(c->Id);
                CollectIds(*c, aOut);
            }
        }
    }

    uint64_t Tree::NewId() const
    {
        static std::mt19937_64 rng{std::random_device{}()};
        if (!m_UsedIdsReady)
        {
            m_UsedIds.clear();
            CollectIds(*m_Root, m_UsedIds);
            for (const TrashItem& t : m_Trash)
            {
                m_UsedIds.insert(t.Item->Id);
                CollectIds(*t.Item, m_UsedIds);
            }
            m_UsedIdsReady = true;
        }
        for (;;)
        {
            const uint64_t id = rng();
            if (id != 0 && id != kReservedExamplesId && m_UsedIds.insert(id).second) return id;
        }
    }

    bool Tree::ClaimReservedId(Node& aFolder)
    {
        if (!aFolder.IsFolder() || &aFolder == m_Root.get() || Find(aFolder.Id) != &aFolder) return false;
        if (IdInUse(kReservedExamplesId)) return false;
        aFolder.Id = kReservedExamplesId;
        if (m_UsedIdsReady) m_UsedIds.insert(kReservedExamplesId);
        return true;
    }

    bool Tree::ReleaseReservedId()
    {
        Node* holder = Find(kReservedExamplesId);
        for (TrashItem& t : m_Trash)
        {
            if (holder) break;
            if (t.Item->Id == kReservedExamplesId) holder = t.Item.get();
            else holder = FindIn(*t.Item, kReservedExamplesId);
        }
        if (!holder) return false;
        holder->Id = NewId(); // NewId never returns the reserved id
        return true;
    }

    bool Tree::IdInUse(uint64_t aId) const
    {
        if (Find(aId)) return true;
        for (const TrashItem& t : m_Trash)
            if (t.Item->Id == aId || FindIn(*t.Item, aId)) return true;
        return false;
    }

    std::string Tree::FreeName(const Node& aFolder, ENodeType aType, const std::string& aName) const
    {
        if (CheckName(aFolder, aType, aName, nullptr).Ok) return aName;
        // NumberedName, not CleanName(aName + " (n)"): for a long name that cut every candidate
        // back to the same taken name and the loop never ended (SEC-018).
        for (int n = 2;; n++)
        {
            const std::string candidate = NumberedName(aName, n);
            if (CheckName(aFolder, aType, candidate, nullptr).Ok) return candidate;
        }
    }

    Result Tree::CheckName(const Node& aParent, ENodeType aType, const std::string& aName, const Node* aIgnore) const
    {
        if (aName.empty()) return Result::Fail(TR("The name can't be empty."));
        const std::string lower = Lower(aName);
        for (const auto& c : aParent.Children)
            if (c.get() != aIgnore && c->Type == aType && Lower(c->Name) == lower)
                return Result::Fail(Localization::Fmt(aType == ENodeType::Folder ? TR("There is already a folder called \"%s\" here.")
                                                                                   : TR("There is already a macro called \"%s\" here."),
                                                      c->Name.c_str()));
        return {};
    }

    Result Tree::Add(Node* aParent, ENodeType aType, const std::string& aName, bool aCheckName)
    {
        Node& parent = aParent ? *aParent : *m_Root;
        if (!parent.IsFolder()) return Result::Fail(TR("Items can only be added to a folder."));
        if (aType == ENodeType::Folder && FolderLevel(parent) + 1 > kMaxFolderDepth)
            return Result::Fail(Localization::Fmt(TR("Folders can be nested at most %d levels deep."), kMaxFolderDepth));

        const std::string name = CleanName(aName);
        if (aCheckName)
        {
            Result check = CheckName(parent, aType, name, nullptr);
            if (!check.Ok) return check;
        }
        else if (name.empty()) return Result::Fail(TR("The name can't be empty."));

        auto node = std::make_unique<Node>();
        node->Id     = NewId();
        node->Type   = aType;
        node->Name   = name;
        node->Parent = &parent;
        Result r;
        r.Target = node.get();
        parent.Children.push_back(std::move(node));
        parent.Expanded = true;
        return r;
    }

    Result Tree::AddFolder(Node* aParent, const std::string& aName) { return Add(aParent, ENodeType::Folder, aName); }
    Result Tree::AddMacro(Node* aParent, const std::string& aTitle) { return Add(aParent, ENodeType::Macro, aTitle); }
    Result Tree::AddKnownFree(Node* aParent, ENodeType aType, const std::string& aName) { return Add(aParent, aType, aName, false); }

    Result Tree::Rename(Node& aNode, const std::string& aName)
    {
        if (!aNode.Parent) return Result::Fail(TR("The top level can't be renamed."));
        const std::string name = CleanName(aName);
        Result check = CheckName(*aNode.Parent, aNode.Type, name, &aNode);
        if (!check.Ok) return check;
        aNode.Name = name;
        Result r;
        r.Target = &aNode;
        return r;
    }

    Result Tree::Delete(Node& aNode)
    {
        if (!aNode.Parent) return Result::Fail(TR("The top level can't be deleted."));
        auto& siblings = aNode.Parent->Children;
        siblings.erase(siblings.begin() + static_cast<std::ptrdiff_t>(IndexInParent(aNode)));
        return {};
    }

    Result Tree::CanMove(const Node& aNode, const Node& aTarget, EDrop aDrop) const
    {
        if (!aNode.Parent) return Result::Fail(TR("The top level can't be moved."));
        const Node* dest = aDrop == EDrop::Into ? &aTarget : aTarget.Parent;
        if (!dest) return Result::Fail(TR("Items can't be placed there."));
        if (!dest->IsFolder()) return Result::Fail(TR("Items can only be moved into a folder."));
        if (&aNode == &aTarget) return aDrop == EDrop::Into ? Result::Fail(TR("A folder can't be moved into itself.")) : Result();
        if (dest == &aNode || IsAncestorOf(aNode, *dest))
            return Result::Fail(TR("A folder can't be moved into one of its own subfolders."));
        if (aNode.IsFolder() && FolderLevel(*dest) + FolderHeight(aNode) > kMaxFolderDepth)
            return Result::Fail(Localization::Fmt(TR("That would nest folders more than %d levels deep."), kMaxFolderDepth));
        if (dest != aNode.Parent)
        {
            Result check = CheckName(*dest, aNode.Type, aNode.Name, &aNode);
            if (!check.Ok) return check;
        }
        return {};
    }

    Result Tree::Move(Node& aNode, Node& aTarget, EDrop aDrop)
    {
        Result check = CanMove(aNode, aTarget, aDrop);
        if (!check.Ok) return check;
        if (&aNode == &aTarget) { Result r; r.Target = &aNode; return r; }

        Node* dest = aDrop == EDrop::Into ? &aTarget : aTarget.Parent;

        auto& from = aNode.Parent->Children;
        const size_t fromIndex = IndexInParent(aNode);
        std::unique_ptr<Node> moving = std::move(from[fromIndex]);
        from.erase(from.begin() + static_cast<std::ptrdiff_t>(fromIndex));

        auto& to = dest->Children;
        size_t at = to.size();
        if (aDrop != EDrop::Into)
        {
            at = IndexInParent(aTarget); // aTarget is still in dest; aNode is already out
            if (aDrop == EDrop::After) at++;
        }
        moving->Parent = dest;
        Result r;
        r.Target = moving.get();
        to.insert(to.begin() + static_cast<std::ptrdiff_t>(at), std::move(moving));
        if (aDrop == EDrop::Into) dest->Expanded = true;
        return r;
    }

    bool Tree::Shift(Node& aNode, int aDelta)
    {
        if (!aNode.Parent || aDelta == 0) return false;
        auto& siblings = aNode.Parent->Children;
        const size_t i = IndexInParent(aNode);
        const long long j = static_cast<long long>(i) + aDelta;
        if (j < 0 || j >= static_cast<long long>(siblings.size())) return false;
        std::swap(siblings[i], siblings[static_cast<size_t>(j)]);
        return true;
    }

    void Tree::SortChildren(Node& aFolder, bool aRecursive)
    {
        std::stable_sort(aFolder.Children.begin(), aFolder.Children.end(),
            [](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b)
            {
                if (a->Type != b->Type) return a->IsFolder();
                return Lower(a->Name) < Lower(b->Name);
            });
        if (aRecursive)
            for (auto& c : aFolder.Children)
                if (c->IsFolder()) SortChildren(*c, true);
    }

    // --- Trash bin ---------------------------------------------------------------------------

    Result Tree::MoveToTrash(Node& aNode, const std::string& aWhen)
    {
        if (!aNode.Parent) return Result::Fail(TR("The top level can't be deleted."));
        TrashItem t;
        for (const Node* n = aNode.Parent; n && n->Parent; n = n->Parent)
        {
            t.FromIds.insert(t.FromIds.begin(), n->Id);
            t.FromNames.insert(t.FromNames.begin(), n->Name);
        }
        t.FromIndex = IndexInParent(aNode);
        t.DeletedAt = aWhen;
        auto& siblings = aNode.Parent->Children;
        t.Item = std::move(siblings[t.FromIndex]);
        siblings.erase(siblings.begin() + static_cast<std::ptrdiff_t>(t.FromIndex));
        t.Item->Parent = nullptr;
        m_Trash.insert(m_Trash.begin(), std::move(t));
        return {};
    }

    int Tree::TrashIndexOf(uint64_t aId) const
    {
        for (size_t i = 0; i < m_Trash.size(); i++)
            if (aId != 0 && m_Trash[i].Item->Id == aId) return static_cast<int>(i);
        return -1;
    }

    bool Tree::OriginalFolderExists(size_t aIndex) const
    {
        if (aIndex >= m_Trash.size()) return false;
        const TrashItem& t = m_Trash[aIndex];
        if (t.FromIds.empty()) return true;
        const Node* f = Find(t.FromIds.back());
        return f && f->IsFolder();
    }

    std::string Tree::FromPathText(const TrashItem& aItem)
    {
        if (aItem.FromNames.empty()) return TR("the top level");
        std::string s;
        for (const std::string& n : aItem.FromNames) s += (s.empty() ? "" : " > ") + n;
        return s;
    }

    Result Tree::Restore(size_t aIndex, ERestoreTo aTo)
    {
        if (aIndex >= m_Trash.size()) return Result::Fail(TR("That item is no longer in the trash bin."));
        TrashItem& t = m_Trash[aIndex];
        Node& item = *t.Item;
        Node* dest = m_Root.get();
        bool atOriginalPlace = false;
        if (aTo == ERestoreTo::Original)
        {
            if (!t.FromIds.empty())
            {
                dest = Find(t.FromIds.back());
                if (!dest || !dest->IsFolder())
                    return Result::Fail(Localization::Fmt(TR("The folder it came from (%s) no longer exists."), FromPathText(t).c_str()));
            }
            atOriginalPlace = true;
        }
        else if (aTo == ERestoreTo::RecreateFolders)
        {
            // The old path's folders keep their old levels, so the item fits as it did. Walk down it:
            // reuse a folder with the same id, or else the same name, or else recreate it.
            if (item.IsFolder() && static_cast<int>(t.FromIds.size()) + FolderHeight(item) > kMaxFolderDepth)
                return Result::Fail(Localization::Fmt(TR("Restored there, folders would be nested more than %d levels deep. "
                                                         "Restore it to the top level instead."), kMaxFolderDepth));
            for (size_t i = 0; i < t.FromIds.size(); i++)
            {
                const std::string want = CleanName(t.FromNames[i]).empty() ? std::string(TR("Restored folder")) : CleanName(t.FromNames[i]);
                Node* next = nullptr;
                for (const auto& c : dest->Children)
                    if (c->IsFolder() && c->Id == t.FromIds[i]) next = c.get();
                if (!next)
                    for (const auto& c : dest->Children)
                        if (c->IsFolder() && Lower(c->Name) == Lower(want)) next = c.get();
                if (!next)
                {
                    auto folder = std::make_unique<Node>();
                    // A folder rebuilt for a restore never takes the reserved Examples id: it isn't
                    // the Examples folder, only a place for the restored item.
                    folder->Id       = t.FromIds[i] == 0 || t.FromIds[i] == kReservedExamplesId || IdInUse(t.FromIds[i])
                                           ? NewId() : t.FromIds[i];
                    if (m_UsedIdsReady) m_UsedIds.insert(folder->Id); // an old id back in use
                    folder->Type     = ENodeType::Folder;
                    folder->Name     = want;
                    folder->Parent   = dest;
                    folder->Expanded = true;
                    next = folder.get();
                    dest->Children.push_back(std::move(folder));
                }
                dest = next;
            }
        }

        if (item.IsFolder() && FolderLevel(*dest) + FolderHeight(item) > kMaxFolderDepth)
            return Result::Fail(Localization::Fmt(TR("Restored there, folders would be nested more than %d levels deep. "
                                                     "Restore it to the top level instead."), kMaxFolderDepth));

        Result r;
        const std::string name = FreeName(*dest, item.Type, item.Name);
        if (name != item.Name)
        {
            r.Note = Localization::Fmt(item.Type == ENodeType::Folder
                                           ? TR("There was already a folder called \"%s\" there, so it was restored as \"%s\".")
                                           : TR("There was already a macro called \"%s\" there, so it was restored as \"%s\"."),
                                       item.Name.c_str(), name.c_str());
            item.Name = name;
        }
        const size_t at = atOriginalPlace ? std::min(t.FromIndex, dest->Children.size()) : dest->Children.size();
        item.Parent = dest;
        r.Target = &item;
        dest->Children.insert(dest->Children.begin() + static_cast<std::ptrdiff_t>(at), std::move(t.Item));
        for (Node* f = dest; f; f = f->Parent) f->Expanded = true;
        m_Trash.erase(m_Trash.begin() + static_cast<std::ptrdiff_t>(aIndex));
        return r;
    }

    Result Tree::DeleteFromTrash(size_t aIndex)
    {
        if (aIndex >= m_Trash.size()) return Result::Fail(TR("That item is no longer in the trash bin."));
        m_Trash.erase(m_Trash.begin() + static_cast<std::ptrdiff_t>(aIndex));
        return {};
    }

    json Tree::ToJson() const
    {
        json root = json::object();
        json children = json::array();
        for (const auto& c : m_Root->Children) children.push_back(NodeJson(*c));
        root["children"] = std::move(children);
        json out{{"format", kFormatVersion}, {"root", std::move(root)}};
        if (!m_Trash.empty())
        {
            json trash = json::array();
            for (const TrashItem& t : m_Trash)
            {
                json ids = json::array();
                for (uint64_t id : t.FromIds) ids.push_back(IdToString(id));
                trash.push_back({{"item", NodeJson(*t.Item)}, {"from_ids", std::move(ids)}, {"from_names", t.FromNames},
                                 {"from_index", t.FromIndex}, {"deleted_at", t.DeletedAt}});
            }
            out["trash"] = std::move(trash);
        }
        return out;
    }

    bool Tree::FromJson(const json& aJson, std::vector<std::string>* aWarnings)
    {
        if (!aJson.is_object()) return false;
        auto root = aJson.find("root");
        if (root == aJson.end() || !root->is_object()) return false;

        // "format" is untrusted, and value() throws on a type get<int>() can't read. Those types
        // (null, text, an object or array) aren't a file we can read; missing means 0, and
        // numbers and true/false are read as get<int>() always read them.
        auto jf = aJson.find("format");
        if (jf != aJson.end() && !jf->is_number() && !jf->is_boolean()) return false;
        const int format = jf != aJson.end() ? jf->get<int>() : 0;
        if (format > kFormatVersion && aWarnings)
            aWarnings->push_back(TR("This file was saved by a newer version of the addon. Some details may be lost."));

        auto fresh = std::make_unique<Node>();
        fresh->Type = ENodeType::Folder;
        Parser p;
        p.Warnings = aWarnings;
        p.ParseChildren(*root, *fresh, 0);

        // The trash bin, parsed with the same parser so an id can't be both in the tree and the bin.
        std::vector<TrashItem> trash;
        auto jt = aJson.find("trash");
        if (jt != aJson.end() && jt->is_array())
        {
            for (const json& je : *jt)
            {
                if (!je.is_object()) continue;
                auto item = je.find("item");
                if (item == je.end() || !item->is_object()) continue;
                TrashItem t;
                auto ids   = je.find("from_ids");
                auto names = je.find("from_names");
                if (ids != je.end() && ids->is_array() && names != je.end() && names->is_array() && ids->size() == names->size())
                {
                    for (size_t i = 0; i < ids->size(); i++)
                    {
                        const json& ji = (*ids)[i];
                        const json& jn = (*names)[i];
                        t.FromIds.push_back(ji.is_string() ? IdFromString(ji.get<std::string>()) : 0);
                        t.FromNames.push_back(jn.is_string() ? jn.get<std::string>() : std::string());
                    }
                }
                auto idx = je.find("from_index");
                if (idx != je.end() && idx->is_number_unsigned()) t.FromIndex = idx->get<size_t>();
                t.DeletedAt = StringField(je, "deleted_at");
                Node holder;
                holder.Type = ENodeType::Folder;
                p.ParseNode(*item, holder, static_cast<int>(t.FromIds.size())); // in place: no json copy
                if (holder.Children.empty()) continue;
                t.Item = std::move(holder.Children.front());
                t.Item->Parent = nullptr;
                trash.push_back(std::move(t));
            }
        }

        m_Root  = std::move(fresh);
        m_Trash = std::move(trash);
        // The parser saw every id kept in the tree and the bin: NewId starts from those instead of
        // walking the tree once per new id, which froze the game on many id-less nodes (SEC-017).
        m_UsedIds      = std::move(p.SeenIds);
        m_UsedIdsReady = true;
        if (!p.NeedIdNodes.empty())
        {
            for (Node* n : p.NeedIdNodes) n->Id = NewId();
            if (aWarnings) aWarnings->push_back(Localization::Fmt(NewIdsFormat(), p.NeedIdNodes.size()));
        }
        if (aWarnings && p.Dropped) aWarnings->push_back(MoreWarnings(p.Dropped));
        return true;
    }

    const char* NewIdsFormat() { return TR("Gave new ids to %zu item(s) with a missing or repeated id."); }

    bool IsNewIdsWarning(const std::string& aWarning)
    {
        // The text around the number, in the language it was made in (the active one).
        const std::string f = NewIdsFormat();
        const size_t at = f.find("%zu");
        if (at == std::string::npos) return false;
        const std::string before = f.substr(0, at), after = f.substr(at + 3);
        return aWarning.size() > before.size() + after.size() && aWarning.compare(0, before.size(), before) == 0 &&
               aWarning.compare(aWarning.size() - after.size(), after.size(), after) == 0;
    }

    std::string MoreWarnings(size_t aCount)
    {
        return Localization::Fmt(TR("...and %zu more problem(s) like these."), aCount);
    }
}

namespace
{
    void CollectHotkey(MacroTree::Node& aNode, const Hotkey::Combo& aCombo, std::vector<MacroTree::Node*>& aOut)
    {
        for (const auto& c : aNode.Children)
        {
            if (c->IsMacro() && c->Hotkey == aCombo) aOut.push_back(c.get());
            else if (c->IsFolder()) CollectHotkey(*c, aCombo, aOut);
        }
    }
}

std::vector<MacroTree::Node*> MacroTree::Tree::MacrosWithHotkey(const Hotkey::Combo& aCombo) const
{
    std::vector<Node*> out;
    if (!aCombo.Empty()) CollectHotkey(*m_Root, aCombo, out);
    return out;
}

uint8_t MacroTree::GameModeOfMapType(uint32_t aMapType)
{
    switch (aMapType)
    {
    case 2: case 3: case 6: case 8:                                  // PvP, GvG, Tournament, UserTournament
        return GM_PvP;
    case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 18: // WvW maps, Edge of the Mists, lounge
        return GM_WvW;
    case 4: case 5: case 7: case 16: case 17:                        // Instance, Public, Tutorial, PublicMini, BigBattle
        return GM_PvE;
    default:                                                         // redirect, character creation, unknown
        return GM_None;
    }
}

MacroTree::Reach MacroTree::ReachOf(const Node& aNode)
{
    Reach r;
    for (const Node* n = &aNode; n; n = n->Parent)
    {
        // GM_None in a file means "not set": every mode, as for a macro.
        const uint8_t modes = n->GameModes == GM_None ? static_cast<uint8_t>(GM_All) : n->GameModes;
        r.Modes = static_cast<uint8_t>(r.Modes & modes);
        if (n->MapIds.empty()) continue;
        std::vector<int> own;
        for (int id : n->MapIds)
            if (id > 0) own.push_back(id); // only real ids can match a map
        std::sort(own.begin(), own.end());
        own.erase(std::unique(own.begin(), own.end()), own.end());
        if (r.AnyMap)
        {
            r.AnyMap = false;
            r.Maps   = std::move(own);
        }
        else
        {
            std::vector<int> both;
            std::set_intersection(r.Maps.begin(), r.Maps.end(), own.begin(), own.end(), std::back_inserter(both));
            r.Maps = std::move(both);
        }
    }
    return r;
}

bool MacroTree::HasFilter(const Node& aNode)
{
    const uint8_t modes = aNode.GameModes == GM_None ? static_cast<uint8_t>(GM_All) : aNode.GameModes;
    return modes != GM_All || !aNode.MapIds.empty();
}

bool MacroTree::IsActive(const Node& aNode, uint8_t aMode, uint32_t aMapId)
{
    const Reach r = ReachOf(aNode);
    if (r.Modes != GM_All && (r.Modes & aMode) == 0) return false;
    if (r.AnyMap) return true;
    return aMapId > 0 && aMapId <= static_cast<uint32_t>(INT32_MAX) &&
           std::binary_search(r.Maps.begin(), r.Maps.end(), static_cast<int>(aMapId));
}

bool MacroTree::NeverActive(const Node& aNode) { return ReachOf(aNode).Never(); }

bool MacroTree::FiltersOverlap(const Node& aA, const Node& aB)
{
    const Reach a = ReachOf(aA);
    const Reach b = ReachOf(aB);
    if (a.Never() || b.Never() || (a.Modes & b.Modes) == 0) return false;
    if (a.AnyMap || b.AnyMap) return true;
    std::vector<int> both;
    std::set_intersection(a.Maps.begin(), a.Maps.end(), b.Maps.begin(), b.Maps.end(), std::back_inserter(both));
    return !both.empty();
}
