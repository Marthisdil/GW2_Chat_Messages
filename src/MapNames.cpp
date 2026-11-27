#include "MapNames.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "Globals.h"
#include "Http.h"
#include "Localization.h"
#include "MapCatalog.h"
#include "Storage.h"

namespace
{
    using Clock = std::chrono::steady_clock;

    constexpr const char* kApi          = "https://api.guildwars2.com/v2/maps";
    constexpr size_t      kBatch        = 50;   // ids per background lookup
    constexpr size_t      kListBatch    = 200;  // ids per request while fetching the full list (the API's limit)
    constexpr size_t      kMaxFileBytes = 4 * 1024 * 1024;
    constexpr auto        kRetryAfter   = std::chrono::minutes(5);  // after a failed lookup
    constexpr auto        kRefreshDelay = std::chrono::seconds(10); // let the language settle at load
    constexpr auto        kListPause    = std::chrono::milliseconds(250); // between list requests
    constexpr auto        kBuildDelay   = std::chrono::minutes(1);  // after a new game build: past the loading screen
    constexpr auto        kListRetry    = std::chrono::minutes(30); // a failed list fetch: Check waits this long

    std::mutex                                      s_Mutex;
    std::condition_variable                         s_Cv;
    MapCatalog::Cache                               s_Cache;
    std::set<uint32_t>                              s_Queued;
    std::deque<uint32_t>                            s_Queue;
    std::unordered_map<uint32_t, Clock::time_point> s_NotBefore; // failed or unknown ids: don't ask again yet
    bool                                            s_Dirty = false;
    bool                                            s_SaveWanted = false; // the lookup thread writes maps.json soon
    bool                                            s_LangSet = false;
    bool                                            s_RefreshWanted = false;
    Clock::time_point                               s_RefreshAt;
    bool                                            s_Refreshing = false;
    Clock::time_point                               s_ListRetryAt;  // Check starts no list fetch before this
    std::string                                     s_LastError;
    std::filesystem::path                           s_File;

    std::thread       s_Thread;
    std::atomic<bool> s_Stop{false};

    int64_t NowUtc() { return static_cast<int64_t>(std::time(nullptr)); }

    // Reads maps.json without knowing the language yet: SetLanguage throws the names away if
    // they're in another one. The file's own language is taken from it first.
    void LoadFile()
    {
        std::error_code ec;
        if (s_File.empty() || !std::filesystem::exists(s_File, ec)) return;
        const auto size = std::filesystem::file_size(s_File, ec);
        if (ec || size > kMaxFileBytes) return; // a bad cache is just rebuilt from the API
        std::ifstream in(s_File, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
        std::string lang = "en";
        if (j.is_object())
        {
            const auto l = j.find("lang");
            if (l != j.end() && l->is_string()) lang = MapCatalog::ApiLang(l->get<std::string>());
        }
        MapCatalog::FromJson(text, lang, s_Cache);
    }

    // Writes maps.json without holding s_Mutex: the render thread asks for names under it every
    // frame the library is open, so a file write under it would hold up those frames. Only a copy
    // of the cache is taken under the lock. Called by the lookup thread (and by Shutdown after it
    // has stopped), so two writes never overlap. Caller doesn't hold s_Mutex.
    void SaveOutsideLock()
    {
        MapCatalog::Cache copy;
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_SaveWanted = false;
            if (!s_Dirty || s_File.empty()) return;
            copy = s_Cache;
            s_Dirty = false;
        }
        bool written = false;
        try
        {
            written = Storage::WriteText(s_File, MapCatalog::ToJson(copy));
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_Dirty = true;
            throw; // ThreadMain logs it
        }
        if (written) return;
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Dirty = true; // tried again with the next change, or at unload
    }

    std::string Lang()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        return s_Cache.Lang;
    }

    // Asks the API for these ids. Returns false (and sets aError) if the API couldn't be reached.
    bool Fetch(const std::vector<uint32_t>& aIds, const std::string& aLang,
               std::unordered_map<uint32_t, MapCatalog::Map>& aOut, std::string& aError)
    {
        std::string ids;
        for (uint32_t id : aIds) ids += (ids.empty() ? "" : ",") + std::to_string(id);
        const Http::Response r = Http::Get(std::string(kApi) + "?ids=" + ids + "&lang=" + aLang);
        if (r.Status == 404) return true; // none of the ids exist: nothing to add
        if (!r.Ok())
        {
            aError = Localization::Fmt(TR("the GW2 API couldn't be reached: %s"), r.Error.c_str());
            return false;
        }
        if (!MapCatalog::ParseMaps(r.Body, aOut))
        {
            aError = TR("the GW2 API sent an unexpected answer.");
            return false;
        }
        return true;
    }

    // Stores a finished lookup. Ids the API didn't name aren't asked about again for a while.
    void Store(const std::vector<uint32_t>& aIds, const std::string& aLang,
               const std::unordered_map<uint32_t, MapCatalog::Map>& aFound, bool aReached)
    {
        bool save = false;
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            const bool sameLang = aLang == s_Cache.Lang; // the language may have changed meanwhile
            for (uint32_t id : aIds)
            {
                s_Queued.erase(id);
                auto it = aFound.find(id);
                if (it != aFound.end() && sameLang)
                {
                    s_Cache.Maps[id] = it->second;
                    s_NotBefore.erase(id);
                    s_Dirty = true;
                }
                else if (sameLang) s_NotBefore[id] = Clock::now() + kRetryAfter;
            }
            save = aReached && s_Dirty;
            if (save) s_SaveWanted = true; // written by the lookup thread (Get runs on the sender's)
        }
        if (save) s_Cv.notify_one();
    }

    // Fetches the API's full list: every id, then their names in batches. Replaces the names and
    // the list only when all of it arrived (a half list would flag maps as missing).
    void FetchList()
    {
        const std::string lang = Lang();
        std::string error;
        const Http::Response r = Http::Get(kApi);
        std::vector<uint32_t> ids;
        if (!r.Ok()) error = Localization::Fmt(TR("the GW2 API couldn't be reached: %s"), r.Error.c_str());
        else if (!MapCatalog::ParseIds(r.Body, ids) || ids.empty()) error = TR("the GW2 API sent an unexpected answer.");

        std::unordered_map<uint32_t, MapCatalog::Map> found;
        for (size_t i = 0; error.empty() && i < ids.size(); i += kListBatch)
        {
            if (s_Stop) return;
            const std::vector<uint32_t> batch(ids.begin() + static_cast<std::ptrdiff_t>(i),
                                              ids.begin() + static_cast<std::ptrdiff_t>(std::min(ids.size(), i + kListBatch)));
            if (!Fetch(batch, lang, found, error)) break;
            std::this_thread::sleep_for(kListPause);
        }

        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Refreshing = false;
        if (!error.empty())
        {
            s_ListRetryAt = Clock::now() + kListRetry;
            s_LastError = error;
            LogF(LOGL_WARNING, "Map list: %s", error.c_str());
            return;
        }
        if (lang != s_Cache.Lang) return; // the language changed meanwhile: a new refresh is queued
        s_LastError.clear();
        for (auto& kv : found) s_Cache.Maps[kv.first] = std::move(kv.second);
        s_Cache.Listed = std::set<uint32_t>(ids.begin(), ids.end());
        MapCatalog::ListFetched(s_Cache, NowUtc());
        s_Dirty = true;
        s_SaveWanted = true; // the next ThreadStep writes it, outside the lock
        LogF(LOGL_INFO, "Map list: %zu maps from the GW2 API.", ids.size());
    }

    void ThreadStep(); // one wait, then one batch of names or the full list

    // Nothing may leave this thread: an exception here would end the game (as SEC-002 for the
    // sender). A failed step is logged and the thread carries on (SEC-030).
    void ThreadMain()
    {
        while (!s_Stop)
        {
            bool failed = true;
            try
            {
                ThreadStep();
                failed = false;
            }
            catch (const std::exception& e)
            {
                LogF(LOGL_WARNING, "Map names: internal error: %s", e.what());
            }
            catch (...)
            {
                LogF(LOGL_WARNING, "Map names: internal error.");
            }
            if (!failed) continue;
            try
            {
                // A list fetch that threw must not stay "loading", and ids taken from the queue
                // may be asked for again.
                std::lock_guard<std::mutex> lock(s_Mutex);
                s_Refreshing = false;
                s_Queued = std::set<uint32_t>(s_Queue.begin(), s_Queue.end());
            }
            catch (...) // NOLINT(bugprone-empty-catch): out of memory again; the next step tries anew
            {
            }
        }
    }

    void ThreadStep()
    {
        std::vector<uint32_t> batch;
        bool list = false;
        bool save = false;
        std::string lang;
        {
            std::unique_lock<std::mutex> lock(s_Mutex);
            auto ready = [] {
                return s_Stop.load() || !s_Queue.empty() || s_SaveWanted || (s_RefreshWanted && Clock::now() >= s_RefreshAt);
            };
            if (s_RefreshWanted) s_Cv.wait_until(lock, s_RefreshAt, ready);
            else s_Cv.wait(lock, ready);
            if (s_Stop) return;
            if (s_RefreshWanted && Clock::now() >= s_RefreshAt)
            {
                s_RefreshWanted = false;
                s_Refreshing = true;
                list = true;
            }
            else if (!s_Queue.empty())
            {
                while (!s_Queue.empty() && batch.size() < kBatch)
                {
                    batch.push_back(s_Queue.front());
                    s_Queue.pop_front();
                }
                lang = s_Cache.Lang;
            }
            else save = s_SaveWanted;
        }
        if (save)
        {
            SaveOutsideLock();
            return;
        }
        if (list)
        {
            FetchList();
            return;
        }
        if (batch.empty()) return;
        std::unordered_map<uint32_t, MapCatalog::Map> found;
        std::string error;
        const bool reached = Fetch(batch, lang, found, error);
        if (!reached) LogF(LOGL_WARNING, "Map names: %s", error.c_str());
        Store(batch, lang, found, reached);
    }

    void WantRefresh(Clock::duration aDelay) // caller holds s_Mutex
    {
        s_RefreshWanted = true;
        s_RefreshAt = Clock::now() + aDelay;
    }

    MapNames::Entry MakeEntry(uint32_t aId) // caller holds s_Mutex
    {
        MapNames::Entry e;
        e.Id = aId;
        const auto it = s_Cache.Maps.find(aId);
        if (it != s_Cache.Maps.end())
        {
            e.Name = it->second.Name;
            e.Region = it->second.Region;
        }
        e.Visited = std::find(s_Cache.Visited.begin(), s_Cache.Visited.end(), aId) != s_Cache.Visited.end();
        return e;
    }
}

void MapNames::Init()
{
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_Cache = MapCatalog::Cache();
    s_Queued.clear();
    s_Queue.clear();
    s_NotBefore.clear();
    s_Dirty = false;
    s_LangSet = false;
    s_RefreshWanted = false;
    s_Refreshing = false;
    s_ListRetryAt = Clock::time_point();
    s_LastError.clear();
    const std::filesystem::path dir = Storage::AddonDir();
    s_File = dir.empty() ? std::filesystem::path() : dir / "maps.json";
    LoadFile();
    // The maps visited are the player's own, in settings.json; maps.json is only a cache.
    {
        const nlohmann::json settings = Storage::LoadSettings();
        const auto visited = settings.find(MapCatalog::kVisitedSetting);
        if (visited != settings.end()) s_Cache.Visited = MapCatalog::VisitedFromJson(*visited);
    }
    s_Stop = false;
    s_Thread = std::thread(ThreadMain);
}

void MapNames::Shutdown()
{
    {
        // Under the mutex, so the worker can't miss it between its check and its wait (SEC-036).
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Stop = true;
    }
    s_Cv.notify_all();
    if (s_Thread.joinable()) s_Thread.join();
    try
    {
        SaveOutsideLock(); // the lookup thread has stopped: nothing else writes the file now
    }
    catch (const std::exception& e)
    {
        LogF(LOGL_WARNING, "Map names: maps.json couldn't be saved: %s", e.what());
    }
    catch (...)
    {
        LogF(LOGL_WARNING, "Map names: maps.json couldn't be saved.");
    }
}

void MapNames::SetLanguage(const std::string& aAddonLang)
{
    const std::string lang = MapCatalog::ApiLang(aAddonLang);
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (lang != s_Cache.Lang)
        {
            // Names in another language: start again, keeping the maps you've been to and the
            // game build seen.
            std::vector<uint32_t> visited = std::move(s_Cache.Visited);
            const uint32_t build = s_Cache.Build;
            const int64_t seenAt = s_Cache.BuildSeenAt;
            s_Cache = MapCatalog::Cache();
            s_Cache.Lang = lang;
            s_Cache.Visited = std::move(visited);
            s_Cache.Build = build;
            s_Cache.BuildSeenAt = seenAt;
            s_NotBefore.clear();
            s_Dirty = true;
            WantRefresh(kRefreshDelay);
        }
        else if (!s_LangSet && MapCatalog::RefreshDue(s_Cache, NowUtc())) WantRefresh(kRefreshDelay);
        else if (!s_LangSet) { s_LangSet = true; return; }
        s_LangSet = true;
    }
    s_Cv.notify_one();
}

bool MapNames::Lookup(uint32_t aMapId, std::string& aName)
{
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_Cache.Maps.find(aMapId);
    if (it == s_Cache.Maps.end()) return false;
    aName = it->second.Name;
    return true;
}

void MapNames::Request(uint32_t aMapId)
{
    if (aMapId == 0) return;
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Cache.Maps.count(aMapId) || s_Queued.count(aMapId)) return;
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
    return Localization::Fmt(TR("Map %u"), static_cast<unsigned>(aMapId));
}

bool MapNames::Get(uint32_t aMapId, std::string& aName, std::string& aError)
{
    if (aMapId == 0)
    {
        aError = TR("you're not on a map right now.");
        return false;
    }
    if (Lookup(aMapId, aName)) return true;

    const std::string lang = Lang();
    const std::vector<uint32_t> ids{aMapId};
    std::unordered_map<uint32_t, MapCatalog::Map> found;
    const bool reached = Fetch(ids, lang, found, aError);
    Store(ids, lang, found, reached);
    if (!reached) return false;
    auto it = found.find(aMapId);
    if (it == found.end())
    {
        aError = Localization::Fmt(TR("the GW2 API has no name for map %u."), static_cast<unsigned>(aMapId));
        return false;
    }
    aName = it->second.Name;
    return true;
}

void MapNames::NoteVisit(uint32_t aMapId)
{
    nlohmann::json visited;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        changed = MapCatalog::NoteVisit(s_Cache, aMapId);
        if (changed) visited = MapCatalog::VisitedToJson(s_Cache);
    }
    // Outside the lock: a settings write never holds up the lookup thread.
    if (changed && !Storage::SaveSetting(MapCatalog::kVisitedSetting, visited))
        LogF(LOGL_WARNING, "The maps visited couldn't be saved.");
    Request(aMapId);
}

std::vector<MapNames::Entry> MapNames::Find(const std::string& aQuery, size_t aMax)
{
    std::lock_guard<std::mutex> lock(s_Mutex);
    std::vector<Entry> out;
    for (uint32_t id : MapCatalog::Find(s_Cache, aQuery, aMax)) out.push_back(MakeEntry(id));
    return out;
}

std::vector<MapNames::Entry> MapNames::Visited(size_t aMax)
{
    std::lock_guard<std::mutex> lock(s_Mutex);
    std::vector<Entry> out;
    for (uint32_t id : s_Cache.Visited)
    {
        if (out.size() >= aMax) break;
        out.push_back(MakeEntry(id));
    }
    return out;
}

bool MapNames::Unlisted(uint32_t aMapId)
{
    std::lock_guard<std::mutex> lock(s_Mutex);
    return MapCatalog::Unlisted(s_Cache, aMapId);
}

void MapNames::Check(uint32_t aGameBuild)
{
    bool wake = false;
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        const int64_t now = NowUtc();
        if (MapCatalog::NoteBuild(s_Cache, aGameBuild, now))
        {
            s_Dirty = true;
            s_SaveWanted = true; // the lookup thread writes it: never a file write on the render thread
            wake = true;
            if (s_Cache.BuildSeenAt == now)
                LogF(LOGL_INFO, "Map list: the game was updated (build %u), so the map list is fetched again.", aGameBuild);
        }
        // Only once the language is known (SetLanguage), never while one runs or is queued, and
        // not again soon after a failed one.
        if (s_LangSet && !s_Refreshing && !s_RefreshWanted && Clock::now() >= s_ListRetryAt && MapCatalog::RefreshDue(s_Cache, now))
        {
            WantRefresh(kBuildDelay);
            wake = true;
        }
    }
    if (wake) s_Cv.notify_one();
}

void MapNames::RefreshNow()
{
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Refreshing) return;
        WantRefresh(Clock::duration::zero());
    }
    s_Cv.notify_one();
}

MapNames::State MapNames::GetState()
{
    std::lock_guard<std::mutex> lock(s_Mutex);
    State s;
    s.Refreshing = s_Refreshing || s_RefreshWanted;
    s.Listed = s_Cache.Listed.size();
    s.ListedAt = s_Cache.ListedAt;
    s.LastError = s_LastError;
    return s;
}
