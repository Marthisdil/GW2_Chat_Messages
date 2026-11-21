#pragma once

#include <cstdarg>
#include <cstdio>

#include "Nexus.h"

// Shared addon state. Set in AddonLoad and cleared in AddonUnload.
extern AddonAPI_t* g_Api;

constexpr const char* kAddonName  = "Chat Messages";
constexpr const char* kLogChannel = "Chat Messages";
// The data folder under <GW2>ddons\: named like the DLL (GW2_Chat_Messages.dll).
constexpr const char* kDataFolder = "GW2_Chat_Messages";

// Addon version: bump all of these together for every release (GetAddonDef uses the numbers,
// the main window shows the text).
constexpr signed short kVersionMajor    = 0;
constexpr signed short kVersionMinor    = 1;
constexpr signed short kVersionBuild    = 0;
constexpr signed short kVersionRevision = 0;
constexpr const char*  kVersionText     = "v0.1.0";
constexpr const char*  kRepositoryUrl   = "https://github.com/Marthisdil/GW2_Chat_Messages";

// printf-style wrapper around the Nexus logger. Safe to call from any thread
// while the addon is loaded.
inline void LogF(ELogLevel aLevel, const char* aFmt, ...)
{
    if (!g_Api) return;
    char buf[1024];
    va_list args;
    va_start(args, aFmt);
    vsnprintf(buf, sizeof(buf), aFmt, args);
    va_end(args);
    g_Api->Log(aLevel, kLogChannel, buf);
}
