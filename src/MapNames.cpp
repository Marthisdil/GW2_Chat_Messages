#include "MapNames.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Globals.h"
#include "Http.h"
#include "Storage.h"

namespace
{
    using json  = nlohmann::json;
    using Clock = std::chrono::steady_clock;

    constexpr const char* kApi        = "https://api.guildwars2.com/v2/maps";
    constexpr const char* kLang       = "en"; // the UI is English for now (multi-language: docs/todo.md)
    constexpr int         kFormat     = 1;
    constexpr size_t      kBatch      = 50;   // ids per background request
    constexpr auto        kRetryAfter = std::chrono::minutes(5); // after a failed lookup

    std::mutex                                  s_Mutex;
    std::condition_variable                     s_Cv;
    std::unordered_map<uint32_t, std::string>   s_Names;
    std::set<uint32_t>                          s_Queued;
    std::deque<uint32_t>                        s_Queue;
    std::unordered_map<uint32_t, Clock::time_point> s_NotBefore; // failed or unknown ids: don't ask again yet
    bool                                        s_Dirty = false;
    std::filesystem::path                       s_File;

    std::thread       s_Thread;
    std::atomic<bool> s_Stop{false};

    void LoadFile()
    {
        std::error_code ec;
        if (s_File.empty() || !std::filesystem::exists(s_File, ec)) return;
        std::ifstream in(s_File, std::ios::binary);
        const json j = json::parse(in, nullptr, false);
        // A bad cache is just thrown away: it's rebuilt from the API.
        if (j.is_discarded() || !j.is_object() || j.value("format", 0) != kFormat || j.value("lang", "") != kLang) return;
        auto names = j.find("names");
        if (names == j.end() || !names->is_object()) return;
        for (auto it = names->begin(); it != names->end(); ++it)
        {
            if (!it.value().is_string()) continue;
            try { s_Names[static_cast<uint32_t>(std::stoul(it.key()))] = it.value().get<std::string>(); }
            catch (...) {}
        }
    }

    void SaveFile() // caller holds s_Mutex
    {
        if (!s_Dirty || s_File.empty()) return;
        json names = json::object();
        for (const auto& [id, name] : s_Names) names[std::to_string(id)] = name;
        const json j = {{"format", kFormat}, {"lang", kLang}, {"names", std::move(names)}};
        if (Storage::WriteJson(s_File, j, false)) s_Dirty = false;
    }

    // Asks the API for these ids. Returns false (and sets aError) if the API couldn't be reached.
    bool Fetch(const std::vector<uint32_t>& aIds, std::unordered_map<uint32_t, std::string>& aOut, std::string& aError)
    {
        std::string ids;
        for (uint32_t id : aIds) ids += (ids.empty() ? "" : ",") + std::to_string(id);
        const Http::Response r = Http::Get(std::string(kApi) + "?ids=" + ids + "&lang=" + kLang);
        if (r.Status == 404) return true; // none of the ids exist: nothing to add
        if (!r.Ok())
        {
            aError = "the GW2 API couldn't be reached: " + r.Error;
            return false;
        }
        const json j = json::parse(r.Body, nullptr, false);
        if (!j.is_array())
        {
            aError = "the GW2 API sent an unexpected answer.";
            return false;
        }
        for (const json& m : j)
            if (m.is_object() && m.contains("id") && m["id"].is_number_unsigned() && m.contains("name") && m["name"].is_string())
                aOut[m["id"].get<uint32_t>()] = m["name"].get<std::string>();
        return true;
    }

    // Stores a finished lookup. Ids the API didn't name aren't asked about again for a while.
    void Store(const std::vector<uint32_t>& aIds, const std::unordered_map<uint32_t, std::string>& aFound, bool aReached)
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        for (uint32_t id : aIds)
        {
            s_Queued.erase(id);
            auto it = aFound.find(id);
            if (it != aFound.end() && !it->second.empty())
            {
                s_Names[id] = it->second;
                s_NotBefore.erase(id);
                s_Dirty = true;
            }
            else s_NotBefore[id] = Clock::now() + kRetryAfter;
        }
        if (aReached) SaveFile();
    }

    void ThreadMain()
    {
        for (;;)
        {
            std::vector<uint32_t> batch;
            {
                std::unique_lock<std::mutex> lock(s_Mutex);
                s_Cv.wait(lock, [] { return s_Stop.load() || !s_Queue.empty(); });
                if (s_Stop) return;
                while (!s_Queue.empty() && batch.size() < kBatch)
                {
                    batch.push_back(s_Queue.front());
                    s_Queue.pop_front();
                }
            }
            std::unordered_map<uint32_t, std::string> found;
            std::string error;
            const bool reached = Fetch(batch, found, error);
            if (!reached) LogF(LOGL_WARNING, "Map names: %s", error.c_str());
            Store(batch, found, reached);
        }
    }
}

void MapNames::Init()
{
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_Names.clear();
    s_Queued.clear();
    s_Queue.clear();
    s_NotBefore.clear();
    s_Dirty = false;
    const std::filesystem::path dir = Storage::AddonDir();
    s_File = dir.empty() ? std::filesystem::path() : dir / "maps.json";
    LoadFile();
    s_Stop = false;
    s_Thread = std::thread(ThreadMain);
}

void MapNames::Shutdown()
{
    s_Stop = true;
    s_Cv.notify_all();
    if (s_Thread.joinable()) s_Thread.join();
    std::lock_guard<std::mutex> lock(s_Mutex);
    SaveFile();
}

bool MapNames::Lookup(uint32_t aMapId, std::string& aName)
{
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Names.find(aMapId);
    if (it == s_Names.end()) return false;
    aName = it->second;
    return true;
}

void MapNames::Request(uint32_t aMapId)
{
    if (aMapId == 0) return;
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Names.count(aMapId) || s_Queued.count(aMapId)) return;
        auto wait = s_NotBefore.find(aMapId);
        if (wait != s_NotBefore.end() && Clock::now() < wait->second) return;
        s_Queued.insert(aMapId);
        s_Queue.push_back(aMapId);
    }
    s_Cv.notify_one();
}

std::string MapNames::Label(uint32_t aMapId)
{
    std::string name;
    if (Lookup(aMapId, name)) return name + " (" + std::to_string(aMapId) + ")";
    Request(aMapId);
    return "Map " + std::to_string(aMapId);
}

bool MapNames::Get(uint32_t aMapId, std::string& aName, std::string& aError)
{
    if (aMapId == 0)
    {
        aError = "you're not on a map right now.";
        return false;
    }
    if (Lookup(aMapId, aName)) return true;

    const std::vector<uint32_t> ids{aMapId};
    std::unordered_map<uint32_t, std::string> found;
    const bool reached = Fetch(ids, found, aError);
    Store(ids, found, reached);
    if (!reached) return false;
    auto it = found.find(aMapId);
    if (it == found.end() || it->second.empty())
    {
        aError = "the GW2 API has no name for map " + std::to_string(aMapId) + ".";
        return false;
    }
    aName = it->second;
    return true;
}
