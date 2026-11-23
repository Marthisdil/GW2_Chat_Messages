#include "Backups.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <nlohmann/json.hpp>

namespace
{
    constexpr const char* kUnitNames[] = {"hours", "days", "weeks", "months"};
    constexpr std::time_t kDay = 24 * 60 * 60;

    // Days since 1970-01-01 for a date, and back (proleptic Gregorian; H. Hinnant's algorithms).
    long long DaysFromCivil(long long y, unsigned m, unsigned d)
    {
        y -= m <= 2;
        const long long era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(y - era * 400);
        const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + static_cast<long long>(doe) - 719468;
    }

    void CivilFromDays(long long z, long long& y, unsigned& m, unsigned& d)
    {
        z += 719468;
        const long long era = (z >= 0 ? z : z - 146096) / 146097;
        const unsigned doe = static_cast<unsigned>(z - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        y = static_cast<long long>(yoe) + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        d = doy - (153 * mp + 2) / 5 + 1;
        m = mp < 10 ? mp + 3 : mp - 9;
        y += m <= 2;
    }

    bool IsLeap(long long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

    unsigned DaysInMonth(long long y, unsigned m)
    {
        static const unsigned kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        return m == 2 && IsLeap(y) ? 29 : kDays[m - 1];
    }

    struct Civil
    {
        long long Y = 1970;
        unsigned  Mo = 1, D = 1, H = 0, Mi = 0, S = 0;
    };

    Civil Split(std::time_t aUtc)
    {
        Civil c;
        long long days = static_cast<long long>(aUtc) / kDay;
        long long secs = static_cast<long long>(aUtc) % kDay;
        if (secs < 0) { secs += kDay; --days; }
        CivilFromDays(days, c.Y, c.Mo, c.D);
        c.H  = static_cast<unsigned>(secs / 3600);
        c.Mi = static_cast<unsigned>(secs / 60 % 60);
        c.S  = static_cast<unsigned>(secs % 60);
        return c;
    }

    const char* KindWord(Backups::EKind aKind) { return aKind == Backups::EKind::Safety ? "safety" : "backup"; }

    bool Digits(const char* p, int n, unsigned& aOut)
    {
        aOut = 0;
        for (int i = 0; i < n; ++i)
        {
            if (p[i] < '0' || p[i] > '9') return false;
            aOut = aOut * 10 + static_cast<unsigned>(p[i] - '0');
        }
        return true;
    }

    struct Entry
    {
        std::time_t Time = 0;
        std::string Name;
    };

    std::vector<Entry> OfKind(const std::vector<std::string>& aNames, Backups::EKind aKind)
    {
        std::vector<Entry> out;
        for (const std::string& n : aNames)
        {
            Backups::EKind k;
            std::time_t t = 0;
            if (Backups::ParseFileName(n, k, t) && k == aKind) out.push_back({t, n});
        }
        std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b)
                  { return a.Time != b.Time ? a.Time < b.Time : a.Name < b.Name; });
        return out;
    }
}

int Backups::MaxEvery(EUnit aUnit)
{
    switch (aUnit)
    {
    case EUnit::Hours:  return 48;
    case EUnit::Days:   return 30;
    case EUnit::Weeks:  return 8;
    case EUnit::Months: return 12;
    default:            return 1;
    }
}

Backups::Settings Backups::Clamped(Settings s)
{
    if (s.Unit < EUnit::Hours || s.Unit >= EUnit::Count) s.Unit = EUnit::Days;
    s.Every = std::clamp(s.Every, 1, MaxEvery(s.Unit));
    s.Keep  = std::clamp(s.Keep, kMinKeep, kMaxKeep);
    return s;
}

Backups::Settings Backups::FromJson(const nlohmann::json& aJson)
{
    Settings s;
    if (!aJson.is_object()) return s;
    const auto enabled = aJson.find("enabled");
    if (enabled != aJson.end() && enabled->is_boolean()) s.Enabled = enabled->get<bool>();
    const auto unit = aJson.find("unit");
    if (unit != aJson.end() && unit->is_string())
    {
        const std::string u = unit->get<std::string>();
        for (int i = 0; i < static_cast<int>(EUnit::Count); ++i)
            if (u == kUnitNames[i]) s.Unit = static_cast<EUnit>(i);
    }
    // Numbers outside an int's range are left at the default, not wrapped.
    const auto readInt = [&](const char* aKey, int& aOut)
    {
        const auto it = aJson.find(aKey);
        if (it == aJson.end() || !it->is_number_integer()) return;
        const long long v = it->is_number_unsigned() ? static_cast<long long>(std::min<uint64_t>(it->get<uint64_t>(), 1u << 30))
                                                     : it->get<long long>();
        aOut = static_cast<int>(std::clamp<long long>(v, -(1 << 30), 1 << 30));
    };
    readInt("every", s.Every);
    readInt("keep", s.Keep);
    return Clamped(s);
}

nlohmann::json Backups::ToJson(const Settings& aSettings)
{
    const Settings s = Clamped(aSettings);
    return {{"enabled", s.Enabled}, {"unit", kUnitNames[static_cast<int>(s.Unit)]}, {"every", s.Every}, {"keep", s.Keep}};
}

Backups::EKind Backups::Kind(const Settings& s) { return s.Enabled ? EKind::Backup : EKind::Safety; }
Backups::EUnit Backups::UnitInEffect(const Settings& s) { return s.Enabled ? Clamped(s).Unit : EUnit::Weeks; }
int Backups::EveryInEffect(const Settings& s) { return s.Enabled ? Clamped(s).Every : 1; }
int Backups::KeepInEffect(const Settings& s) { return s.Enabled ? Clamped(s).Keep : kSafetyKeep; }

std::time_t Backups::Add(std::time_t aFrom, EUnit aUnit, int aEvery)
{
    const long long n = aEvery;
    switch (aUnit)
    {
    case EUnit::Hours: return aFrom + static_cast<std::time_t>(n * 3600);
    case EUnit::Days:  return aFrom + static_cast<std::time_t>(n * kDay);
    case EUnit::Weeks: return aFrom + static_cast<std::time_t>(n * 7 * kDay);
    case EUnit::Months:
    {
        const Civil c = Split(aFrom);
        long long months = c.Y * 12 + (c.Mo - 1) + n;
        const long long y = months >= 0 ? months / 12 : (months - 11) / 12;
        const unsigned m = static_cast<unsigned>(months - y * 12) + 1;
        const unsigned d = std::min(c.D, DaysInMonth(y, m));
        const long long days = DaysFromCivil(y, m, d);
        return static_cast<std::time_t>(days * kDay + c.H * 3600LL + c.Mi * 60LL + c.S);
    }
    default: return aFrom;
    }
}

bool Backups::IsDue(std::time_t aNewest, std::time_t aNow, EUnit aUnit, int aEvery)
{
    if (aNewest == 0) return true;
    if (aNewest > aNow) return false; // a copy from "the future" (clock changed): wait for it
    return aNow >= Add(aNewest, aUnit, aEvery);
}

std::string Backups::FileName(EKind aKind, std::time_t aUtc)
{
    const Civil c = Split(aUtc);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "macros_%s_%04lld-%02u-%02u_%02u-%02u-%02u_UTC.json", KindWord(aKind),
                  c.Y, c.Mo, c.D, c.H, c.Mi, c.S);
    return buf;
}

bool Backups::ParseFileName(const std::string& aName, EKind& aKind, std::time_t& aUtc)
{
    // macros_backup_YYYY-MM-DD_HH-MM-SS_UTC.json (the kind word is 6 letters either way)
    constexpr size_t kLen = sizeof("macros_backup_2026-11-23_21-46-05_UTC.json") - 1;
    if (aName.size() != kLen || aName.compare(0, 7, "macros_") != 0) return false;
    EKind kind;
    if (aName.compare(7, 6, "backup") == 0) kind = EKind::Backup;
    else if (aName.compare(7, 6, "safety") == 0) kind = EKind::Safety;
    else return false;
    const char* p = aName.c_str() + 13; // "_YYYY-MM-DD_HH-MM-SS_UTC.json"
    if (std::strcmp(p + 20, "_UTC.json") != 0) return false;
    if (p[0] != '_' || p[5] != '-' || p[8] != '-' || p[11] != '_' || p[14] != '-' || p[17] != '-') return false;
    unsigned y, mo, d, h, mi, s;
    if (!Digits(p + 1, 4, y) || !Digits(p + 6, 2, mo) || !Digits(p + 9, 2, d) || !Digits(p + 12, 2, h) ||
        !Digits(p + 15, 2, mi) || !Digits(p + 18, 2, s))
        return false;
    if (y < 1970 || mo < 1 || mo > 12 || d < 1 || d > DaysInMonth(y, mo) || h > 23 || mi > 59 || s > 59) return false;
    aKind = kind;
    aUtc  = static_cast<std::time_t>(DaysFromCivil(y, mo, d) * kDay + h * 3600LL + mi * 60LL + s);
    return true;
}

std::string Backups::DateText(std::time_t aUtc)
{
    const Civil c = Split(aUtc);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04lld-%02u-%02u", c.Y, c.Mo, c.D);
    return buf;
}

std::string Backups::TimeText(std::time_t aUtc)
{
    const Civil c = Split(aUtc);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u", c.H, c.Mi, c.S);
    return buf;
}

std::vector<std::string> Backups::ToDelete(const std::vector<std::string>& aNames, EKind aKind, int aKeep)
{
    std::vector<Entry> all = OfKind(aNames, aKind);
    const size_t keep = static_cast<size_t>(std::max(aKeep, kMinKeep));
    std::vector<std::string> out;
    for (size_t i = 0; i + keep < all.size(); ++i) out.push_back(all[i].Name);
    return out;
}

std::time_t Backups::Newest(const std::vector<std::string>& aNames, EKind aKind)
{
    const std::vector<Entry> all = OfKind(aNames, aKind);
    return all.empty() ? 0 : all.back().Time;
}
