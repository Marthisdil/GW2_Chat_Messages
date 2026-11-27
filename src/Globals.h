#pragma once

#include <cstdarg>
#include <cstdio>

#include "Nexus.h"
#include "Version.h"

// Shared addon state. Set in AddonLoad and cleared in AddonUnload.
extern AddonAPI_t* g_Api;

constexpr const char* kAddonName  = "Chat Messages";
constexpr const char* kLogChannel = "Chat Messages";
// The data folder under <GW2>\addons\: named like the DLL (GW2_Chat_Messages.dll).
constexpr const char* kDataFolder = "GW2_Chat_Messages";

// Addon version, from src/Version.h (tools/set_version.ps1 writes it: run it for every release).
// GetAddonDef uses the numbers (Nexus compares them with the GitHub release tag); the log, the
// main window and error reports show the text, e.g. "2026.11.21.1430.Dev".
constexpr uint16_t    kVersionMajor    = CHATMSG_VER_YEAR;
constexpr uint16_t    kVersionMinor    = CHATMSG_VER_MONTH;
constexpr uint16_t    kVersionBuild    = CHATMSG_VER_DAY;
constexpr uint16_t    kVersionRevision = CHATMSG_VER_TIME;
constexpr const char* kVersionText     = CHATMSG_VER_TEXT;
constexpr const char* kVersionTag      = CHATMSG_VER_TAG; // the numbers only, as the release tag
constexpr const char* kVersionChannel  = CHATMSG_VER_CHANNEL; // "Dev", "Beta" or "Rel"
constexpr const char*  kRepositoryUrl   = "https://github.com/Marthisdil/GW2_Chat_Messages";
// ArenaNet's macro policy, linked from the options page and the docs (README, wiki).
constexpr const char*  kMacroPolicyUrl  = "https://help.guildwars2.com/hc/en-us/articles/360013762153-Policy-Macros-and-Macro-Use";

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
