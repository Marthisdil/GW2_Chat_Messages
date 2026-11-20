#pragma once

// Map names from the official GW2 API (/v2/maps, no API key), cached on disk in
// <GW2>\addons\Chat Messages\maps.json so each map is looked up once (docs/design.md,
// "Placeholders"). Only names are kept: no waypoint or point-of-interest data.
//  - Request() queues a lookup for a background thread (cheap; call it from any thread).
//  - Get() returns a name, looking it up right away if needed (blocks: worker threads only).
//  - Lookup()/Label() only read the cache (safe on the render thread).

#include <cstdint>
#include <string>

namespace MapNames
{
    void Init();     // in AddonLoad: loads the cache and starts the background thread
    void Shutdown(); // in AddonUnload: stops the thread (waits for a running lookup) and saves

    bool        Lookup(uint32_t aMapId, std::string& aName);
    void        Request(uint32_t aMapId);
    // "Queensdale (15)", or "Map 15" while the name isn't known yet (and asks for it).
    std::string Label(uint32_t aMapId);

    bool Get(uint32_t aMapId, std::string& aName, std::string& aError);
}
