#pragma once
// Backups of the macro library (docs/design.md, "Backups"): the schedule the user picks, the
// file names (date and time in UTC), which old copies to delete, and the "backups" setting.
// Times are seconds since 1970 (UTC). No Nexus or ImGui dependency. Tested by
// tests/BackupsTests.cpp. Storage.cpp reads and writes the files; Ui.cpp decides when.

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace Backups
{
    enum class EUnit { Hours, Days, Weeks, Months, Count };

    // The user's own backups, or the weekly safety copies made while backups are off.
    enum class EKind { Backup, Safety };

    struct Settings
    {
        bool  Enabled = false;
        EUnit Unit    = EUnit::Days;
        int   Every   = 1; // every 1 day...
        int   Keep    = 7; // copies kept
    };

    constexpr int kMinKeep = 1;
    constexpr int kMaxKeep = 50;
    // While backups are off: one safety copy a week, the last 4 kept.
    constexpr int kSafetyKeep = 4;

    int MaxEvery(EUnit aUnit); // hours 48, days 30, weeks 8, months 12

    // settings.json "backups". Anything missing or out of range falls back to the default, or is
    // brought into range.
    Settings FromJson(const nlohmann::json& aJson);
    nlohmann::json ToJson(const Settings& aSettings);
    Settings Clamped(Settings aSettings);

    // The schedule in effect: the user's, or the weekly safety copy while backups are off.
    EKind Kind(const Settings& aSettings);
    EUnit UnitInEffect(const Settings& aSettings);
    int   EveryInEffect(const Settings& aSettings);
    int   KeepInEffect(const Settings& aSettings);

    // aFrom plus aEvery units. Months are calendar months in UTC: Jan 31 + 1 month = Feb 28/29.
    std::time_t Add(std::time_t aFrom, EUnit aUnit, int aEvery);
    // Is the next backup due? aNewest 0 = there's none yet.
    bool IsDue(std::time_t aNewest, std::time_t aNow, EUnit aUnit, int aEvery);

    // "macros_backup_2026-11-23_21-46-05_UTC.json" and "macros_safety_...".
    std::string FileName(EKind aKind, std::time_t aUtc);
    // Only names exactly in that form are backups; any other file in the folder is never one.
    bool ParseFileName(const std::string& aName, EKind& aKind, std::time_t& aUtc);

    // "2026-11-23" and "21:46:05".
    std::string DateText(std::time_t aUtc);
    std::string TimeText(std::time_t aUtc);

    // The backups of aKind among aNames that go when only aKeep may stay: the oldest first.
    // Other kinds and other files are never in it.
    std::vector<std::string> ToDelete(const std::vector<std::string>& aNames, EKind aKind, int aKeep);
    // The newest backup of aKind among aNames (0 = none).
    std::time_t Newest(const std::vector<std::string>& aNames, EKind aKind);
}
