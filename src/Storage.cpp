#include "Storage.h"

#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>

#include <windows.h>
#include <shellapi.h>

#include <nlohmann/json.hpp>

#include "Globals.h"
#include "MacroTree.h"

namespace fs = std::filesystem;

namespace
{
    constexpr const char* kLibraryFile  = "macros.json";
    constexpr const char* kSettingsFile = "settings.json";

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
            LogF(LOGL_WARNING, "Could not read %s: %s", aFile.u8string().c_str(), e.what());
            return ERead::Unreadable;
        }
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

fs::path Storage::AddonDir()
{
    const char* dir = g_Api ? g_Api->Paths_GetAddonDirectory(kDataFolder) : nullptr;
    if (!dir) return {};
    fs::path path = fs::u8path(dir);
    std::error_code ec;
    fs::create_directories(path, ec);
    return path;
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

fs::path Storage::FreeFile(const fs::path& aDir, const std::string& aFileName)
{
    const fs::path name = fs::u8path(aFileName);
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
    const fs::path tmp = WithSuffix(aFile, ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << aText;
        out.flush();
        if (!out)
        {
            LogF(LOGL_WARNING, "Could not write %s.", aFile.u8string().c_str());
            std::error_code ec;
            fs::remove(tmp, ec);
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, aFile, ec);
    if (ec)
    {
        LogF(LOGL_WARNING, "Could not write %s: %s", aFile.u8string().c_str(), ec.message().c_str());
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

bool Storage::WriteJson(const fs::path& aFile, const nlohmann::json& aJson, bool aKeepBackup)
{
    if (aFile.empty()) return false;
    const fs::path tmp = WithSuffix(aFile, ".tmp");
    try
    {
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out << aJson.dump(2);
            out.flush();
            if (!out) throw std::runtime_error("write failed");
        }
        std::error_code ec;
        if (aKeepBackup && fs::exists(aFile, ec))
        {
            fs::copy_file(aFile, WithSuffix(aFile, ".bak"), fs::copy_options::overwrite_existing, ec);
            if (ec) LogF(LOGL_WARNING, "Could not back up %s: %s", aFile.u8string().c_str(), ec.message().c_str());
        }
        fs::rename(tmp, aFile, ec); // replaces an existing file on Windows (MoveFileEx semantics)
        if (ec) throw std::runtime_error(ec.message());
        return true;
    }
    catch (const std::exception& e)
    {
        LogF(LOGL_WARNING, "Could not write %s: %s", aFile.u8string().c_str(), e.what());
        std::error_code ec;
        fs::remove(tmp, ec);
        return false;
    }
}

void Storage::LoadMacros(MacroTree::Tree& aTree, std::vector<std::string>& aMessages)
{
    const fs::path file = LibraryFile();
    if (file.empty())
    {
        aMessages.push_back("The addon folder isn't available, so macros can't be loaded or saved.");
        return;
    }

    std::vector<std::string> warnings;
    switch (ReadLibrary(file, aTree, warnings))
    {
    case ERead::Ok:
        for (auto& w : warnings) aMessages.push_back(std::move(w));
        return;
    case ERead::Missing:
        return; // first run, or the user removed the file on purpose: start empty
    case ERead::Unreadable:
    {
        // Keep the unreadable file for the user, under a name we will never overwrite.
        const fs::path aside = WithSuffix(file, ".unreadable-" + TimeStamp());
        std::error_code ec;
        fs::rename(file, aside, ec);
        aMessages.push_back("Your macros file couldn't be read. It was kept as \"" + aside.filename().u8string() + "\".");
        break;
    }
    }

    const fs::path backup = WithSuffix(file, ".bak");
    warnings.clear();
    MacroTree::Tree fromBackup;
    if (ReadLibrary(backup, fromBackup, warnings) == ERead::Ok)
    {
        aTree = std::move(fromBackup);
        aMessages.push_back("Your macros were restored from the backup (" + backup.filename().u8string() +
                            "). Changes from the last save may be missing.");
        for (auto& w : warnings) aMessages.push_back(std::move(w));
    }
}

bool Storage::SaveMacros(const MacroTree::Tree& aTree)
{
    return WriteJson(LibraryFile(), aTree.ToJson(), true);
}

nlohmann::json Storage::LoadSettings()
{
    const fs::path dir = AddonDir();
    if (dir.empty()) return nlohmann::json::object();
    const fs::path file = dir / kSettingsFile;
    std::error_code ec;
    if (!fs::exists(file, ec)) return nlohmann::json::object();
    try
    {
        std::ifstream in(file, std::ios::binary);
        nlohmann::json j = nlohmann::json::parse(in);
        if (j.is_object()) return j;
        LogF(LOGL_WARNING, "%s isn't a settings file; using the default settings.", file.u8string().c_str());
    }
    catch (const std::exception& e)
    {
        LogF(LOGL_WARNING, "Could not read %s: %s. Using the default settings.", file.u8string().c_str(), e.what());
    }
    return nlohmann::json::object();
}

bool Storage::SaveSetting(const char* aKey, const nlohmann::json& aValue)
{
    nlohmann::json s = LoadSettings(); // re-read so one setting never overwrites another
    s[aKey] = aValue;
    return SaveSettings(s);
}

bool Storage::SaveSettings(const nlohmann::json& aSettings)
{
    const fs::path dir = AddonDir();
    if (dir.empty()) return false;
    return WriteJson(dir / kSettingsFile, aSettings, false);
}
