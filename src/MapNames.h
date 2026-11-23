#pragma once

// Map names from the official GW2 API (/v2/maps, no API key), cached on disk in
// <GW2>\addons\GW2_Chat_Messages\maps.json (docs/design.md, "Filters" and "Placeholders"). Only
// names and regions are kept: no waypoint or point-of-interest data. The rules (parsing, the
// cache file, search) are in MapCatalog.*; this file adds the network and the threads.
//  - Request() queues a lookup for a background thread (cheap; call it from any thread).
//  - Get() returns a name, looking it up right away if needed (blocks: worker threads only).
//  - Lookup()/Label()/Find()/Visited()/Unlisted()/State() only read the cache (render thread OK).
//  - The API's full list of maps is fetched in the background on the first run, once a week, when
//    the language changes, and on RefreshNow(). It's what the filter editor's map search uses.

#include <cstdint>
#include <string>
#include <vector>

namespace MapNames
{
    void Init();     // in AddonLoad: loads the cache and starts the background thread
    void Shutdown(); // in AddonUnload: stops the thread (waits for a running lookup) and saves

    // The addon's language (Languages, on every change). Names follow it where the API has them
    // (German, French, Spanish, Chinese; English otherwise): a change throws the names away and
    // fetches them again. The first call also decides whether the weekly refresh is due.
    void SetLanguage(const std::string& aAddonLang);

    bool        Lookup(uint32_t aMapId, std::string& aName);
    void        Request(uint32_t aMapId);
    // "Queensdale (15)", or "Map 15" while the name isn't known yet (and asks for it).
    std::string Label(uint32_t aMapId);

    bool Get(uint32_t aMapId, std::string& aName, std::string& aError);

    // The player arrived on this map (the render thread, on a map change): remembered for the
    // picker's "Maps you've been to", and its name is looked up.
    void NoteVisit(uint32_t aMapId);

    struct Entry
    {
        uint32_t    Id = 0;
        std::string Name;   // "" while not known
        std::string Region; // may be empty
        bool        Visited = false;
    };
    // The filter editor's map search (MapCatalog::Find), at most aMax.
    std::vector<Entry> Find(const std::string& aQuery, size_t aMax);
    // The maps the player has been to, most recent first, at most aMax.
    std::vector<Entry> Visited(size_t aMax);
    // Whether the API's full list is known and doesn't have this id (MapCatalog::Unlisted).
    bool Unlisted(uint32_t aMapId);

    // Fetch the full list again now (the options page's button).
    void RefreshNow();
    struct State
    {
        bool        Refreshing = false;
        size_t      Listed = 0;     // maps in the API's full list (0: not fetched yet)
        int64_t     ListedAt = 0;   // when (UTC, seconds)
        std::string LastError;      // why the last refresh failed ("" if it didn't)
    };
    State GetState();
}
