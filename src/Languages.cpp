#include "Languages.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <memory>
#include <system_error>
#include <unordered_set>

#include <windows.h>

#include <nlohmann/json.hpp>

#include "Globals.h"
#include "Localization.h"
#include "Storage.h"
#include "resource.h"

namespace fs = std::filesystem;

namespace
{
    constexpr const char* kSetting    = "language";
    constexpr const char* kProbeKey   = "CHATMSG_LANGUAGE_PROBE";
    // The languages Nexus ships (RaidcoreGG/Nexus-Translations). Its API can't say which one is
    // active, so each gets our probe text set to its own id, and translating the probe tells us.
    const char* const kNexusIds[] = {"en", "de", "fr", "es", "cn", "kr", "tw", "pt-br", "cz", "it", "pl", "ru"};
    constexpr DWORD kNexusPollMs = 1000;

    struct Loaded
    {
        Languages::Entry                       Info;
        std::unique_ptr<Localization::Catalog> Cat;
    };

    struct NexusText { std::string Identifier, English; };

    std::unordered_set<std::string> s_Known;   // every English text (en.json)
    std::vector<Loaded>             s_Loaded;  // not English
    std::vector<Languages::Entry>   s_List;
    std::vector<std::string>        s_Problems;
    std::vector<NexusText>          s_NexusTexts;
    std::string                     s_Choice = Languages::kFollowNexus;
    std::string                     s_Nexus  = "en";
    std::string                     s_ActiveId = "en";
    bool                            s_Apply  = false;
    bool                            s_Reload = false;
    DWORD                           s_LastPoll = 0;

    HMODULE ThisModule()
    {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&ThisModule), &module);
        return module;
    }

    bool ReadResource(int aId, std::string& aOut)
    {
        const HMODULE module = ThisModule();
        HRSRC res = module ? FindResourceW(module, MAKEINTRESOURCEW(aId), L"LOCALE") : nullptr;
        HGLOBAL data = res ? LoadResource(module, res) : nullptr;
        const void* bytes = data ? LockResource(data) : nullptr;
        if (!bytes) return false;
        aOut.assign(static_cast<const char*>(bytes), SizeofResource(module, res));
        return true;
    }

    // Reads at most kMaxFileBytes + 1 bytes, so a file that grows while we read it can't get past
    // the size check.
    bool ReadCapped(const fs::path& aFile, std::string& aOut)
    {
        std::ifstream in(aFile, std::ios::binary);
        if (!in) return false;
        aOut.resize(Localization::kMaxFileBytes + 1);
        in.read(&aOut[0], static_cast<std::streamsize>(aOut.size()));
        aOut.resize(static_cast<size_t>(in.gcount()));
        return true;
    }

    // Adds a language, replacing one with the same id (a file can replace a built-in one).
    void Add(std::unique_ptr<Localization::Catalog> aCat, bool aFromFolder, const std::string& aFile)
    {
        Languages::Entry info;
        info.Id           = aCat->Id;
        info.DisplayName  = aCat->DisplayName;
        info.FromFolder   = aFromFolder;
        info.File         = aFile;
        info.AddonVersion = aCat->AddonVersion;
        info.Percent      = Localization::PercentDone(*aCat, s_Known);
        info.Skipped      = aCat->Skipped;
        for (const std::string& note : aCat->SkipNotes)
            LogF(LOGL_WARNING, "Language %s: skipped %s", info.Id.c_str(), note.c_str());
        for (Loaded& l : s_Loaded)
            if (l.Info.Id == info.Id) { l.Info = info; l.Cat = std::move(aCat); return; }
        s_Loaded.push_back({info, std::move(aCat)});
    }

    void LoadBuiltIn()
    {
        std::string text, error;
        Localization::Catalog en;
        if (!ReadResource(IDR_LANG_EN, text) || !Localization::ParseCatalog(text, nullptr, en, error))
        {
            LogF(LOGL_CRITICAL, "The built-in English text list couldn't be read (%s). Only English is available.", error.c_str());
            return;
        }
        for (const auto& kv : en.Texts) s_Known.insert(kv.first);
        const int ids[] = {IDR_LANG_DE, IDR_LANG_FR, IDR_LANG_ES};
        for (const int id : ids)
        {
            auto cat = std::make_unique<Localization::Catalog>();
            if (!ReadResource(id, text) || !Localization::ParseCatalog(text, &s_Known, *cat, error))
            {
                LogF(LOGL_CRITICAL, "A built-in language (resource %d) couldn't be read: %s", id, error.c_str());
                continue;
            }
            Add(std::move(cat), false, std::string());
        }
    }

    // <addon>\localization\*.json: plain files only (no links), at most kMaxFiles, each checked.
    void LoadFolder()
    {
        if (s_Known.empty()) return; // without the English list nothing can be checked
        const fs::path dir = Languages::Folder();
        if (dir.empty()) return;
        std::vector<fs::path> files;
        std::error_code ec;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            std::error_code sec;
            const fs::file_status st = it->symlink_status(sec);
            if (sec || !fs::is_regular_file(st)) continue; // no links, folders or devices
            std::wstring ext = it->path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), std::towlower);
            if (ext != L".json") continue;
            files.push_back(it->path());
        }
        std::sort(files.begin(), files.end());
        if (files.size() > Localization::kMaxFiles)
        {
            s_Problems.push_back(Localization::Fmt(T("The localization folder has more than %zu files. Only the first %zu were read."),
                                                   Localization::kMaxFiles, Localization::kMaxFiles));
            files.resize(Localization::kMaxFiles);
        }
        std::unordered_set<std::string> seen;
        for (const fs::path& file : files)
        {
            const std::string name = file.filename().u8string();
            std::string text, error;
            auto cat = std::make_unique<Localization::Catalog>();
            if (!ReadCapped(file, text)) error = T("It couldn't be opened.");
            else if (!Localization::ParseCatalog(text, &s_Known, *cat, error)) error = Localization::LookupText(error);
            else if (cat->Id == "en") error = T("English is built in and can't be replaced.");
            else if (!seen.insert(cat->Id).second)
                error = Localization::Fmt(T("Another file in the folder already has the language \"%s\"."), cat->Id.c_str());
            if (!error.empty())
            {
                s_Problems.push_back(Localization::Fmt(T("The language file %s wasn't loaded: %s"), name.c_str(), error.c_str()));
                LogF(LOGL_WARNING, "Language file %s refused: %s", name.c_str(), error.c_str());
                continue;
            }
            if (cat->Skipped)
                s_Problems.push_back(Localization::Fmt(T("The language file %s: %zu text(s) were left out because they didn't pass "
                                                         "the checks (English is shown for them). The Nexus log lists the first few."),
                                                       name.c_str(), cat->Skipped));
            LogF(LOGL_INFO, "Language file %s loaded: %s (%s).", name.c_str(), cat->DisplayName.c_str(), cat->Id.c_str());
            Add(std::move(cat), true, name);
        }
    }

    void BuildList()
    {
        s_List.clear();
        Languages::Entry en;
        en.Id = "en";
        en.DisplayName = "English (US)";
        s_List.push_back(en);
        std::vector<Languages::Entry> rest;
        for (const Loaded& l : s_Loaded) rest.push_back(l.Info);
        std::sort(rest.begin(), rest.end(), [](const Languages::Entry& a, const Languages::Entry& b) { return a.DisplayName < b.DisplayName; });
        s_List.insert(s_List.end(), rest.begin(), rest.end());
    }

    void TellNexus()
    {
        if (!g_Api) return;
        for (const char* id : kNexusIds) g_Api->Localization_Set(kProbeKey, id, id);
        for (const Loaded& l : s_Loaded)
            for (const NexusText& t : s_NexusTexts)
            {
                const auto it = l.Cat->Texts.find(t.English);
                if (it != l.Cat->Texts.end()) g_Api->Localization_Set(t.Identifier.c_str(), l.Info.Id.c_str(), it->second.c_str());
            }
    }

    std::string AskNexus()
    {
        if (!g_Api) return "en";
        const char* got = g_Api->Localization_Translate(kProbeKey);
        const std::string id = got ? std::string(got) : std::string(); // copied straight away: Nexus owns it
        return Localization::ValidId(id) ? id : std::string("en");
    }

    void Apply()
    {
        const std::string want = s_Choice == Languages::kFollowNexus ? s_Nexus : s_Choice;
        const Localization::Catalog* cat = nullptr;
        for (const Loaded& l : s_Loaded)
            if (l.Info.Id == want) cat = l.Cat.get();
        // A chosen language whose file is gone: follow Nexus instead (the choice is kept).
        if (!cat && want != "en" && s_Choice != Languages::kFollowNexus)
            for (const Loaded& l : s_Loaded)
                if (l.Info.Id == s_Nexus) cat = l.Cat.get();
        Localization::SetActive(cat);
        const std::string now = cat ? cat->Id : std::string("en");
        if (now != s_ActiveId) LogF(LOGL_INFO, "Language: %s.", now.c_str());
        s_ActiveId = now;
    }

    void LoadAll()
    {
        Localization::SetActive(nullptr); // before the catalogs it points at go
        s_Loaded.clear();
        s_Known.clear();
        LoadBuiltIn();
        LoadFolder();
        BuildList();
        TellNexus();
    }
}

void Languages::AddNexusText(const char* aIdentifier, const char* aEnglish)
{
    s_NexusTexts.push_back({aIdentifier, aEnglish});
}

void Languages::Init()
{
    const nlohmann::json settings = Storage::LoadSettings();
    const auto it = settings.find(kSetting);
    if (it != settings.end() && it->is_string() && (it->get<std::string>() == kFollowNexus || Localization::ValidId(it->get<std::string>())))
        s_Choice = it->get<std::string>();
    LoadAll();
    s_Nexus = AskNexus(); // Nexus applies our probe texts on its next frame: Update corrects this
    Apply();
    s_LastPoll = GetTickCount();
}

void Languages::Shutdown()
{
    Localization::SetActive(nullptr);
    s_Loaded.clear();
    s_List.clear();
    s_Known.clear();
}

void Languages::Update()
{
    if (s_Reload)
    {
        s_Reload = false;
        LoadAll();
        s_Apply = true;
    }
    const DWORD now = GetTickCount();
    if (now - s_LastPoll >= kNexusPollMs)
    {
        s_LastPoll = now;
        const std::string nexus = AskNexus();
        if (nexus != s_Nexus)
        {
            s_Nexus = nexus;
            if (s_Choice == kFollowNexus) s_Apply = true;
        }
    }
    if (s_Apply)
    {
        s_Apply = false;
        Apply();
    }
}

const std::vector<Languages::Entry>& Languages::List() { return s_List; }
const std::string& Languages::Choice() { return s_Choice; }

void Languages::Choose(const std::string& aId)
{
    if (aId != kFollowNexus && !Localization::ValidId(aId)) return;
    s_Choice = aId;
    s_Apply = true;
    if (!Storage::SaveSetting(kSetting, s_Choice)) LogF(LOGL_WARNING, "The language setting couldn't be saved.");
}

void Languages::RequestReload() { s_Reload = true; }
std::string Languages::NexusLanguage() { return s_Nexus; }
std::string Languages::ActiveId() { return s_ActiveId; }
fs::path Languages::Folder() { return Storage::LocalizationDir(); }

std::vector<std::string> Languages::TakeProblems()
{
    std::vector<std::string> out;
    out.swap(s_Problems);
    return out;
}
