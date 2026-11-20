#include "Storage.h"

#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "Globals.h"
#include "MacroTree.h"

namespace fs = std::filesystem;

namespace
{
    constexpr const char* kLibraryFile = "macros.json";

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
    const char* dir = g_Api ? g_Api->Paths_GetAddonDirectory(kAddonName) : nullptr;
    if (!dir) return {};
    fs::path path = fs::u8path(dir);
    std::error_code ec;
    fs::create_directories(path, ec);
    return path;
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
