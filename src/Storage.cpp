#include "Storage.h"

#include <atomic>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <system_error>

#include <windows.h>
#include <shellapi.h>

#include <nlohmann/json.hpp>

#include "Backups.h"
#include "Globals.h"
#include "Localization.h"
#include "MacroTree.h"
#include "PathText.h"
#include "WorkQueue.h"

namespace fs = std::filesystem;

namespace
{
    constexpr const char* kLibraryFile  = "macros.json";
    constexpr const char* kSettingsFile = "settings.json";

    // Set by Init (before any other thread starts) and cleared by Shutdown: read-only between.
    fs::path  s_Dir;
    WorkQueue s_Writer;

    // settings.json, read once (LoadSettings) and changed in memory (SaveSetting).
    std::mutex     s_SettingsMutex;
    nlohmann::json s_Settings = nlohmann::json::object();
    bool           s_SettingsLoaded = false;

    // The macros.json saves (SaveMacrosLater).
    std::atomic<uint64_t> s_MacrosSeq{0};
    std::mutex            s_ResultMutex;
    Storage::SaveResult   s_LastMacros;

    fs::path LookUpAddonDir()
    {
        const char* dir = g_Api ? g_Api->Paths_GetAddonDirectory(kDataFolder) : nullptr;
        if (!dir) return {};
        fs::path path = PathText::FromUtf8(dir);
        std::error_code ec;
        fs::create_directories(path, ec);
        return path;
    }

    // The folder a file goes in, made again if it was deleted while the game runs.
    void MakeParent(const fs::path& aFile)
    {
        std::error_code ec;
        if (aFile.has_parent_path()) fs::create_directories(aFile.parent_path(), ec);
    }

    fs::path LibraryFile()
    {
        fs::path dir = Storage::AddonDir();
        return dir.empty() ? dir : dir / kLibraryFile;
    }

    fs::path WithSuffix(const fs::path& aFile, const std::string& aSuffix)
    {
        fs::path p = aFile;
        p += aSuffix;
        return p;
    }

    enum class ERead { Missing, Ok, Unreadable };

    // Reads a library file into aTree. aTree is only changed on Ok.
    ERead ReadLibrary(const fs::path& aFile, MacroTree::Tree& aTree, std::vector<std::string>& aWarnings)
    {
        std::error_code ec;
        if (aFile.empty() || !fs::exists(aFile, ec)) return ERead::Missing;
        try
        {
            std::ifstream in(aFile, std::ios::binary);
            if (!in) throw std::runtime_error("could not open the file");
            const nlohmann::json j = nlohmann::json::parse(in);
            if (!aTree.FromJson(j, &aWarnings)) throw std::runtime_error("the file is not a macro library");
            return ERead::Ok;
        }
        catch (const std::exception& e)
        {
            LogF(LOGL_WARNING, "Could not read %s: %s", PathText::Utf8(aFile).c_str(), e.what());
            return ERead::Unreadable;
        }
    }

    // Text that isn't UTF-8 is written as U+FFFD, so one bad string can't stop every save.
    std::string Dump(const nlohmann::json& aJson)
    {
        return aJson.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
    }

    // The path of a backup in the backups folder, or empty when aName isn't a backup's name.
    fs::path BackupFile(const std::string& aName)
    {
        Backups::EKind kind;
        std::time_t when = 0;
        if (!Backups::ParseFileName(aName, kind, when)) return {};
        const fs::path dir = Storage::BackupsDir();
        return dir.empty() ? dir : dir / PathText::FromUtf8(aName);
    }

    std::string TimeStamp()
    {
        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &local);
        return buf;
    }
}

void Storage::Init()
{
    s_Dir = LookUpAddonDir();
    {
        std::lock_guard<std::mutex> lock(s_SettingsMutex);
        s_Settings = nlohmann::json::object();
        s_SettingsLoaded = false;
    }
    s_Writer.Start();
}

void Storage::Shutdown()
{
    s_Writer.Stop(); // runs every write still waiting
    if (const size_t n = s_Writer.Failures())
        LogF(LOGL_WARNING, "%zu background write(s) failed with an internal error.", n);
    {
        std::lock_guard<std::mutex> lock(s_SettingsMutex);
        s_Settings = nlohmann::json::object();
        s_SettingsLoaded = false; // a reload reads the file again
    }
    s_Dir.clear();
}

void Storage::Post(std::function<void()> aJob, const std::string& aKey) { s_Writer.Post(std::move(aJob), aKey); }

void Storage::Flush() { s_Writer.Flush(); }

fs::path Storage::AddonDir()
{
    // Looked up once: Nexus' answer never changes while the addon is loaded, and asking (and
    // creating the folder) every frame cost a file-system call each time.
    if (!s_Dir.empty()) return s_Dir;
    return LookUpAddonDir(); // before Init (or after Shutdown)
}

namespace
{
    fs::path SubDir(const char* aName)
    {
        const fs::path dir = Storage::AddonDir();
        if (dir.empty()) return {};
        fs::path sub = dir / aName;
        std::error_code ec;
        fs::create_directories(sub, ec);
        return sub;
    }
}

fs::path Storage::ExportsDir() { return SubDir("exports"); }
fs::path Storage::ImportsDir() { return SubDir("imports"); }
fs::path Storage::LocalizationDir() { return SubDir("localization"); }
fs::path Storage::BackupsDir() { return SubDir("backups"); }

fs::path Storage::FreeFile(const fs::path& aDir, const std::string& aFileName)
{
    const fs::path name = PathText::FromUtf8(aFileName);
    fs::path candidate = aDir / name;
    std::error_code ec;
    for (int n = 2; fs::exists(candidate, ec) && n < 10000; n++)
    {
        fs::path next = name.stem();
        next += " (" + std::to_string(n) + ")";
        next += name.extension();
        candidate = aDir / next;
    }
    return candidate;
}

bool Storage::WriteText(const fs::path& aFile, const std::string& aText)
{
    if (aFile.empty()) return false;
    MakeParent(aFile);
    const fs::path tmp = WithSuffix(aFile, ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << aText;
        out.flush();
        if (!out)
        {
            LogF(LOGL_WARNING, "Could not write %s.", PathText::Utf8(aFile).c_str());
            std::error_code ec;
            fs::remove(tmp, ec);
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, aFile, ec);
    if (ec)
    {
        LogF(LOGL_WARNING, "Could not write %s: %s", PathText::Utf8(aFile).c_str(), ec.message().c_str());
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

void Storage::OpenFolder(const fs::path& aDir)
{
    if (aDir.empty()) return;
    ShellExecuteW(nullptr, L"open", aDir.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void Storage::OpenWebPage(const char* aUrl)
{
    if (!aUrl || std::strncmp(aUrl, "https://", 8) != 0) return;
    const std::wstring url = PathText::FromUtf8(aUrl).wstring();
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool Storage::WriteJson(const fs::path& aFile, const nlohmann::json& aJson, bool aKeepBackup)
{
    if (aFile.empty()) return false;
    MakeParent(aFile);
    const fs::path tmp = WithSuffix(aFile, ".tmp");
    try
    {
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out << Dump(aJson);
            out.flush();
            if (!out) throw std::runtime_error("write failed");
        }
        std::error_code ec;
        if (aKeepBackup && fs::exists(aFile, ec))
        {
            fs::copy_file(aFile, WithSuffix(aFile, ".bak"), fs::copy_options::overwrite_existing, ec);
            if (ec) LogF(LOGL_WARNING, "Could not back up %s: %s", PathText::Utf8(aFile).c_str(), ec.message().c_str());
        }
        fs::rename(tmp, aFile, ec); // replaces an existing file on Windows (MoveFileEx semantics)
        if (ec) throw std::runtime_error(ec.message());
        return true;
    }
    catch (const std::exception& e)
    {
        LogF(LOGL_WARNING, "Could not write %s: %s", PathText::Utf8(aFile).c_str(), e.what());
        std::error_code ec;
        fs::remove(tmp, ec);
        return false;
    }
}

bool Storage::LoadMacros(MacroTree::Tree& aTree, std::vector<std::string>& aMessages)
{
    const fs::path file = LibraryFile();
    if (file.empty())
    {
        aMessages.push_back(TR("The addon folder isn't available, so macros can't be loaded or saved."));
        return false;
    }

    std::vector<std::string> warnings;
    switch (ReadLibrary(file, aTree, warnings))
    {
    case ERead::Ok:
        if (aTree.FromNewerVersion())
            aMessages.push_back(Localization::Fmt(TR("Your macros were saved by a newer version of the addon (%s). They're open "
                                                     "read-only: you can send them, but changes aren't saved. Update the addon "
                                                     "to edit them."),
                                                  aTree.SavedBy().empty() ? "?" : aTree.SavedBy().c_str()));
        for (auto& w : warnings) aMessages.push_back(std::move(w));
        return false;
    case ERead::Missing:
    {
        // First run, or the user removed the file on purpose. With a .bak still there it's not
        // a first run: saving examples would soon overwrite that backup.
        std::error_code ec;
        return !fs::exists(WithSuffix(file, ".bak"), ec) && !ec;
    }
    case ERead::Unreadable:
    {
        // Keep the unreadable file for the user, under a name we will never overwrite.
        const fs::path aside = WithSuffix(file, ".unreadable-" + TimeStamp());
        std::error_code ec;
        fs::rename(file, aside, ec);
        aMessages.push_back(Localization::Fmt(TR("Your macros file couldn't be read. It was kept as \"%s\"."),
                                              PathText::Utf8(aside.filename()).c_str()));
        break;
    }
    }

    const fs::path backup = WithSuffix(file, ".bak");
    warnings.clear();
    MacroTree::Tree fromBackup;
    if (ReadLibrary(backup, fromBackup, warnings) == ERead::Ok)
    {
        aTree = std::move(fromBackup);
        aMessages.push_back(Localization::Fmt(TR("Your macros were restored from the backup (%s). Changes from the "
                                                 "last save may be missing."), PathText::Utf8(backup.filename()).c_str()));
        for (auto& w : warnings) aMessages.push_back(std::move(w));
    }
    return false;
}

namespace
{
    // The writer thread's half of SaveMacrosLater.
    bool WriteLibrary(const nlohmann::json& aJson, int aFormatRead)
    {
        const fs::path file = LibraryFile();
        // An older format, upgraded as it was read: keep the file as it was, once, before the first save.
        if (!file.empty() && aFormatRead < MacroTree::kFormatVersion)
        {
            const fs::path old = WithSuffix(file, ".v" + std::to_string(aFormatRead) + ".bak");
            std::error_code ec;
            if (fs::exists(file, ec) && !fs::exists(old, ec))
            {
                fs::copy_file(file, old, ec);
                if (ec) LogF(LOGL_WARNING, "Could not keep the old macros.json as %s: %s", PathText::Utf8(old.filename()).c_str(),
                             ec.message().c_str());
            }
        }
        return Storage::WriteJson(file, aJson, true);
    }
}

uint64_t Storage::SaveMacrosLater(const MacroTree::Tree& aTree)
{
    // A newer version's file is never written over: this build would lose what it doesn't
    // understand of a breaking change (docs/design.md, "Saved files").
    if (aTree.FromNewerVersion())
    {
        LogF(LOGL_WARNING, "macros.json is from a newer version of the addon (format %d); it isn't saved.", aTree.FormatRead());
        return 0;
    }
    // The tree is only safe to read here (the render thread): the job gets its own copy.
    const auto json = std::make_shared<const nlohmann::json>(aTree.ToJson());
    const int formatRead = aTree.FormatRead();
    const uint64_t seq = ++s_MacrosSeq;
    s_Writer.Post([json, formatRead, seq] {
        bool ok = false;
        try { ok = WriteLibrary(*json, formatRead); }
        catch (const std::exception& e) { LogF(LOGL_WARNING, "Could not write macros.json: %s", e.what()); }
        std::lock_guard<std::mutex> lock(s_ResultMutex);
        s_LastMacros = {seq, ok};
    }, "macros");
    return seq;
}

Storage::SaveResult Storage::LastMacrosSave()
{
    std::lock_guard<std::mutex> lock(s_ResultMutex);
    return s_LastMacros;
}

bool Storage::SaveMacros(const MacroTree::Tree& aTree)
{
    const uint64_t seq = SaveMacrosLater(aTree);
    if (seq == 0) return false;
    Flush();
    const SaveResult r = LastMacrosSave();
    return r.Seq == seq && r.Ok;
}

bool Storage::ReadLibraryText(std::string& aText)
{
    aText.clear();
    const fs::path file = LibraryFile();
    std::error_code ec;
    if (file.empty()) return false;
    const auto size = fs::file_size(file, ec);
    if (ec || size > kMaxBackupBytes) return false;
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    aText.resize(static_cast<size_t>(size));
    in.read(aText.data(), static_cast<std::streamsize>(aText.size()));
    aText.resize(static_cast<size_t>(in.gcount()));
    return !in.bad();
}

std::string Storage::LibraryText(const MacroTree::Tree& aTree) { return Dump(aTree.ToJson()); }

std::string Storage::JsonText(const nlohmann::json& aJson) { return Dump(aJson); }

std::vector<std::string> Storage::BackupNames()
{
    std::vector<std::string> names;
    const fs::path dir = BackupsDir();
    if (dir.empty()) return names;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code fec;
        if (it->is_regular_file(fec)) names.push_back(PathText::Utf8(it->path().filename()));
        if (names.size() >= 10000) break; // a folder full of other files: stop looking
    }
    return names;
}

bool Storage::WriteBackup(const std::string& aName, const std::string& aText)
{
    const fs::path file = BackupFile(aName);
    std::error_code ec;
    if (file.empty() || fs::exists(file, ec)) return false;
    return WriteText(file, aText);
}

bool Storage::DeleteBackup(const std::string& aName)
{
    const fs::path file = BackupFile(aName);
    if (file.empty()) return false;
    std::error_code ec;
    if (!fs::remove(file, ec) || ec)
    {
        LogF(LOGL_WARNING, "Could not delete the old backup %s: %s", aName.c_str(), ec ? ec.message().c_str() : "it's gone");
        return false;
    }
    return true;
}

bool Storage::ReadBackupText(const std::string& aName, std::string& aText)
{
    aText.clear();
    const fs::path file = BackupFile(aName);
    if (file.empty()) return false;
    std::error_code ec;
    const auto size = fs::file_size(file, ec);
    if (ec || size > kMaxBackupBytes) return false;
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    aText.resize(static_cast<size_t>(size));
    in.read(aText.data(), static_cast<std::streamsize>(aText.size()));
    aText.resize(static_cast<size_t>(in.gcount()));
    return !in.bad();
}

bool Storage::ReadBackup(const std::string& aName, MacroTree::Tree& aTree, std::string& aError)
{
    std::string text;
    if (!ReadBackupText(aName, text))
    {
        aError = Localization::Fmt(TR("The file couldn't be read, or it's bigger than %zu MB."),
                                   static_cast<size_t>(kMaxBackupBytes / (1024 * 1024)));
        return false;
    }
    try
    {
        const nlohmann::json j = nlohmann::json::parse(text);
        MacroTree::Tree tree;
        std::vector<std::string> warnings;
        if (!tree.FromJson(j, &warnings))
        {
            aError = TR("The file isn't a macro library.");
            return false;
        }
        aTree = std::move(tree);
        return true;
    }
    catch (const std::exception&)
    {
        aError = TR("The file isn't a macro library.");
        return false;
    }
}

namespace
{
    nlohmann::json ReadSettingsFile();
    void WriteSettingsFile(const nlohmann::json& aSettings);

    // Caller holds s_SettingsMutex.
    void EnsureSettingsLoaded()
    {
        if (s_SettingsLoaded) return;
        s_Settings = ReadSettingsFile();
        s_SettingsLoaded = true;
    }

    // Writes the settings as they are when the job runs: several changes in a row write once.
    void PostSettingsWrite()
    {
        s_Writer.Post([] {
            nlohmann::json copy;
            {
                std::lock_guard<std::mutex> lock(s_SettingsMutex);
                copy = s_Settings;
            }
            WriteSettingsFile(copy);
        }, "settings");
    }
}

nlohmann::json Storage::LoadSettings()
{
    std::lock_guard<std::mutex> lock(s_SettingsMutex);
    EnsureSettingsLoaded();
    return s_Settings;
}

bool Storage::SaveSetting(const char* aKey, const nlohmann::json& aValue)
{
    {
        std::lock_guard<std::mutex> lock(s_SettingsMutex);
        EnsureSettingsLoaded(); // so one setting never overwrites another
        s_Settings[aKey] = aValue;
    }
    PostSettingsWrite();
    return true; // a failed write is logged by the writer
}

bool Storage::SaveSettings(const nlohmann::json& aSettings)
{
    if (!aSettings.is_object()) return false;
    {
        std::lock_guard<std::mutex> lock(s_SettingsMutex);
        s_Settings = aSettings;
        s_SettingsLoaded = true;
    }
    PostSettingsWrite();
    return true;
}

namespace
{
    // settings.json from disk (LoadSettings keeps it in memory after the first read).
    nlohmann::json ReadSettingsFile()
    {
        const fs::path dir = Storage::AddonDir();
        if (dir.empty()) return nlohmann::json::object();
        const fs::path file = dir / kSettingsFile;
        std::error_code ec;
        if (!fs::exists(file, ec)) return nlohmann::json::object();
        try
        {
            std::ifstream in(file, std::ios::binary);
            nlohmann::json j = nlohmann::json::parse(in);
            if (j.is_object()) return j;
            LogF(LOGL_WARNING, "%s isn't a settings file; using the default settings.", PathText::Utf8(file).c_str());
        }
        catch (const std::exception& e)
        {
            LogF(LOGL_WARNING, "Could not read %s: %s. Using the default settings.", PathText::Utf8(file).c_str(), e.what());
        }
        return nlohmann::json::object();
    }

    void WriteSettingsFile(const nlohmann::json& aSettings)
    {
        const fs::path dir = Storage::AddonDir();
        if (dir.empty()) return;
        // "format": added when missing; a newer number is kept (each setting is checked on its own
        // and unknown ones are kept, so a newer file reads fine).
        if (aSettings.is_object() && !aSettings.contains("format"))
        {
            nlohmann::json withFormat = aSettings;
            withFormat["format"] = Storage::kSettingsFormat;
            if (!Storage::WriteJson(dir / kSettingsFile, withFormat, false))
                LogF(LOGL_WARNING, "settings.json couldn't be written; your settings are kept until the next change or unload.");
            return;
        }
        if (!Storage::WriteJson(dir / kSettingsFile, aSettings, false))
            LogF(LOGL_WARNING, "settings.json couldn't be written; your settings are kept until the next change or unload.");
    }
}
