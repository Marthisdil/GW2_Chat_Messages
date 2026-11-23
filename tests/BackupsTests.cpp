// Tests for Backups (schedule, file names, which copies to delete, the setting).

#include "Backups.h"
#include "TestCheck.h"

#include <nlohmann/json.hpp>

using namespace Backups;

namespace
{
    // 2026-11-23 21:46:05 UTC
    constexpr std::time_t kT = 1795470365;
}

static void TestFileNames()
{
    CHECK(DateText(kT) == "2026-11-23");
    CHECK(TimeText(kT) == "21:46:05");
    CHECK(FileName(EKind::Backup, kT) == "macros_backup_2026-11-23_21-46-05_UTC.json");
    CHECK(FileName(EKind::Safety, kT) == "macros_safety_2026-11-23_21-46-05_UTC.json");
    CHECK(FileName(EKind::Backup, 0) == "macros_backup_1970-01-01_00-00-00_UTC.json");

    EKind k = EKind::Safety;
    std::time_t t = 0;
    CHECK(ParseFileName("macros_backup_2026-11-23_21-46-05_UTC.json", k, t) && k == EKind::Backup && t == kT);
    CHECK(ParseFileName("macros_safety_2026-11-23_21-46-05_UTC.json", k, t) && k == EKind::Safety && t == kT);
    CHECK(ParseFileName("macros_backup_2024-02-29_00-00-00_UTC.json", k, t)); // a leap day
    // Anything else in the folder is never a backup.
    for (const char* bad : {"macros.json", "macros_backup_2026-11-23_21-46-05_UTC.json.tmp",
                            "macros_backup_2026-11-23_21-46-05_UTC.JSON", "macros_other_2026-11-23_21-46-05_UTC.json",
                            "macros_backup_2026-13-23_21-46-05_UTC.json", "macros_backup_2025-02-29_21-46-05_UTC.json",
                            "macros_backup_2026-11-23_24-00-00_UTC.json", "macros_backup_2026-11-23_21-46-0x_UTC.json",
                            "macros_backup_2026-11-23 21-46-05_UTC.json", "macros_backup_1969-12-31_23-59-59_UTC.json",
                            "Macros_backup_2026-11-23_21-46-05_UTC.json", ""})
        CHECK(!ParseFileName(bad, k, t));
    // Round trip over many times.
    for (std::time_t x = 0; x < 4102444800; x += 86400 * 37 + 3601)
    {
        CHECK(ParseFileName(FileName(EKind::Backup, x), k, t) && t == x);
    }
}

static void TestSchedule()
{
    CHECK(IsDue(0, kT, EUnit::Hours, 1)); // none yet
    CHECK(!IsDue(kT, kT + 3599, EUnit::Hours, 1));
    CHECK(IsDue(kT, kT + 3600, EUnit::Hours, 1));
    CHECK(!IsDue(kT, kT + 2 * 86400 - 1, EUnit::Days, 2));
    CHECK(IsDue(kT, kT + 2 * 86400, EUnit::Days, 2));
    CHECK(!IsDue(kT, kT + 7 * 86400 - 1, EUnit::Weeks, 1));
    CHECK(IsDue(kT, kT + 7 * 86400, EUnit::Weeks, 1));
    CHECK(!IsDue(kT + 100, kT, EUnit::Hours, 1)); // newest in the future: not due

    // Months are calendar months.
    std::time_t t = 0;
    EKind k;
    CHECK(ParseFileName("macros_backup_2026-01-31_10-00-00_UTC.json", k, t));
    CHECK(FileName(EKind::Backup, Add(t, EUnit::Months, 1)) == "macros_backup_2026-02-28_10-00-00_UTC.json");
    CHECK(FileName(EKind::Backup, Add(t, EUnit::Months, 12)) == "macros_backup_2027-01-31_10-00-00_UTC.json");
    CHECK(ParseFileName("macros_backup_2024-01-31_10-00-00_UTC.json", k, t));
    CHECK(FileName(EKind::Backup, Add(t, EUnit::Months, 1)) == "macros_backup_2024-02-29_10-00-00_UTC.json");
    CHECK(ParseFileName("macros_backup_2026-11-15_10-00-00_UTC.json", k, t));
    CHECK(FileName(EKind::Backup, Add(t, EUnit::Months, 3)) == "macros_backup_2027-02-15_10-00-00_UTC.json");
    CHECK(!IsDue(t, Add(t, EUnit::Months, 1) - 1, EUnit::Months, 1));
    CHECK(IsDue(t, Add(t, EUnit::Months, 1), EUnit::Months, 1));
}

static void TestToDelete()
{
    const std::vector<std::string> names = {
        "macros_backup_2026-11-20_10-00-00_UTC.json", "macros_backup_2026-11-22_10-00-00_UTC.json",
        "macros_backup_2026-11-21_10-00-00_UTC.json", "macros_safety_2026-11-01_10-00-00_UTC.json",
        "macros_safety_2026-11-08_10-00-00_UTC.json", "notes.txt", "macros.json",
        "macros_backup_2026-11-19_10-00-00_UTC.json.tmp"};
    std::vector<std::string> d = ToDelete(names, EKind::Backup, 2);
    CHECK(d.size() == 1 && d[0] == "macros_backup_2026-11-20_10-00-00_UTC.json");
    d = ToDelete(names, EKind::Backup, 1);
    CHECK(d.size() == 2 && d[0] == "macros_backup_2026-11-20_10-00-00_UTC.json" &&
          d[1] == "macros_backup_2026-11-21_10-00-00_UTC.json");
    CHECK(ToDelete(names, EKind::Backup, 3).empty());
    CHECK(ToDelete(names, EKind::Backup, 0).size() == 2); // at least one is always kept
    d = ToDelete(names, EKind::Safety, 1);
    CHECK(d.size() == 1 && d[0] == "macros_safety_2026-11-01_10-00-00_UTC.json");

    EKind k;
    std::time_t t = 0;
    CHECK(ParseFileName("macros_backup_2026-11-22_10-00-00_UTC.json", k, t) && Newest(names, EKind::Backup) == t);
    CHECK(ParseFileName("macros_safety_2026-11-08_10-00-00_UTC.json", k, t) && Newest(names, EKind::Safety) == t);
    CHECK(Newest({"notes.txt"}, EKind::Backup) == 0);
}

static void TestSettings()
{
    Settings s = FromJson(nlohmann::json());
    CHECK(!s.Enabled && s.Unit == EUnit::Days && s.Every == 1 && s.Keep == 7);
    // Off: the weekly safety copy, 4 kept.
    CHECK(Kind(s) == EKind::Safety && UnitInEffect(s) == EUnit::Weeks && EveryInEffect(s) == 1 && KeepInEffect(s) == kSafetyKeep);

    s = FromJson(nlohmann::json{{"enabled", true}, {"unit", "hours"}, {"every", 6}, {"keep", 12}});
    CHECK(s.Enabled && s.Unit == EUnit::Hours && s.Every == 6 && s.Keep == 12);
    CHECK(Kind(s) == EKind::Backup && UnitInEffect(s) == EUnit::Hours && EveryInEffect(s) == 6 && KeepInEffect(s) == 12);
    CHECK(FromJson(ToJson(s)).Every == 6 && FromJson(ToJson(s)).Unit == EUnit::Hours && FromJson(ToJson(s)).Keep == 12);
    CHECK(ToJson(s)["unit"] == "hours");

    // Out of range is brought into range; wrong types fall back to the default.
    s = FromJson(nlohmann::json{{"enabled", "yes"}, {"unit", "years"}, {"every", 1000}, {"keep", -3}});
    CHECK(!s.Enabled && s.Unit == EUnit::Days && s.Every == MaxEvery(EUnit::Days) && s.Keep == kMinKeep);
    s = FromJson(nlohmann::json{{"unit", "months"}, {"every", 99999999999LL}, {"keep", 18446744073709551615ULL}});
    CHECK(s.Unit == EUnit::Months && s.Every == 12 && s.Keep == kMaxKeep);
    s = FromJson(nlohmann::json{{"every", 2.5}, {"keep", "9"}});
    CHECK(s.Every == 1 && s.Keep == 7);
    CHECK(FromJson(nlohmann::json::array()).Keep == 7);
    for (int u = 0; u < static_cast<int>(EUnit::Count); ++u)
        CHECK(MaxEvery(static_cast<EUnit>(u)) >= 1);
}

void RunBackupsTests()
{
    TestFileNames();
    TestSchedule();
    TestToDelete();
    TestSettings();
}
