#include "Perf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    // Fixed-point text for a CSV cell ("12.345"), never in the user's locale.
    void AddNumber(std::string& aOut, double aValue, int aDecimals)
    {
        if (!std::isfinite(aValue)) aValue = 0.0;
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%.*f", aDecimals, aValue);
        aOut += buf;
    }

    void AddUint(std::string& aOut, uint64_t aValue)
    {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(aValue));
        aOut += buf;
    }

    void AddContext(std::string& aOut, const Perf::Context& c)
    {
        aOut += c.Gameplay ? ",1," : ",0,";
        AddUint(aOut, c.MapId);
        aOut += c.LibraryOpen ? ",1" : ",0";
        aOut += c.MenuOpen ? ",1" : ",0";
        aOut += c.Sending ? ",1," : ",0,";
        AddUint(aOut, c.Macros);
    }

    void AddStart(std::string& aOut, const char* aKind, double aT, std::string_view aClock)
    {
        aOut += aKind;
        aOut += ',';
        aOut.append(aClock.data(), aClock.size());
        aOut += ',';
        AddNumber(aOut, aT, 3);
        aOut += ',';
    }

    bool TimeParts(std::time_t aUtc, std::tm& aOut)
    {
#ifdef _WIN32
        return gmtime_s(&aOut, &aUtc) == 0;
#else
        return gmtime_r(&aUtc, &aOut) != nullptr;
#endif
    }
}

const char* Perf::SectionKey(Section aSection)
{
    switch (aSection)
    {
    case Section::Languages: return "languages_ms";
    case Section::Poll:      return "poll_ms";
    case Section::Library:   return "library_ms";
    case Section::QuickMenu: return "quickmenu_ms";
    case Section::Windows:   return "windows_ms";
    case Section::Saves:     return "saves_ms";
    case Section::Options:   return "options_ms";
    default:                 return "other_ms";
    }
}

void Perf::Second::Add(const Frame& aFrame)
{
    m_Game.push_back(aFrame.GameMs);
    m_Addon.push_back(aFrame.AddonMs);
    for (size_t i = 0; i < kSections; i++) m_SectionSum[i] += aFrame.SectionMs[i];
}

Perf::Summary Perf::Second::Finish(double aWndProcMaxUs)
{
    Summary s;
    s.WndProcMaxUs = aWndProcMaxUs;
    s.Frames       = m_Game.size();
    if (s.Frames > 0)
    {
        double game = 0.0, addon = 0.0;
        for (size_t i = 0; i < s.Frames; i++)
        {
            game  += m_Game[i];
            addon += m_Addon[i];
            s.GameMax  = std::max(s.GameMax, m_Game[i]);
            s.AddonMax = std::max(s.AddonMax, m_Addon[i]);
            if (m_Addon[i] > kSlowAddonMs) s.AddonSlow++;
        }
        const double n = static_cast<double>(s.Frames);
        s.GameAvg  = game / n;
        s.AddonAvg = addon / n;
        s.GameP99  = Percentile(m_Game, 0.99);
        s.AddonP99 = Percentile(m_Addon, 0.99);
        for (size_t i = 0; i < kSections; i++) s.SectionAvg[i] = m_SectionSum[i] / n;
    }
    m_Game.clear();
    m_Addon.clear();
    for (double& v : m_SectionSum) v = 0.0;
    return s;
}

double Perf::Percentile(std::vector<double>& aValues, double aP)
{
    if (aValues.empty()) return 0.0;
    aP = std::clamp(aP, 0.0, 1.0);
    // Nearest rank: the smallest value with at least aP of the values at or below it.
    const size_t n    = aValues.size();
    size_t       rank = static_cast<size_t>(std::ceil(aP * static_cast<double>(n)));
    if (rank == 0) rank = 1;
    const auto at = aValues.begin() + static_cast<std::ptrdiff_t>(rank - 1);
    std::nth_element(aValues.begin(), at, aValues.end());
    return *at;
}

bool Perf::IsSpike(const Frame& aFrame) { return aFrame.AddonMs > kSlowAddonMs || aFrame.GameMs > kSlowGameMs; }

std::string Perf::CleanName(std::string_view aName)
{
    std::string out;
    for (const char ch : aName)
    {
        if (out.size() >= 40) break;
        const unsigned char c = static_cast<unsigned char>(ch);
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
                        c == '_' || c == '.' || c == ':';
        out += ok ? static_cast<char>(c) : '_';
    }
    return out;
}

std::string Perf::FileName(std::time_t aUtc)
{
    std::tm t{};
    if (!TimeParts(aUtc, t)) return "perf_unknown_UTC.csv";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "perf_%04d-%02d-%02d_%02d-%02d-%02d_UTC.csv", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

std::string Perf::ClockText(std::time_t aUtc)
{
    std::tm t{};
    if (!TimeParts(aUtc, t)) return "00:00:00";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

std::string Perf::Header(std::string_view aVersion, std::string_view aBuild, int aMinutes)
{
    std::string h = "# Frame recording, GW2 Chat Messages ";
    h += CleanName(aVersion);
    h += ", build ";
    h += CleanName(aBuild);
    h += ", limit ";
    AddUint(h, static_cast<uint64_t>(std::max(aMinutes, 0)));
    h += " min. Times in ms (wndproc_max_us in microseconds). tools\\perf_report.ps1 summarises it.\n";
    h += "kind,utc,t_sec,frames,game_avg_ms,game_max_ms,game_p99_ms,addon_avg_ms,addon_max_ms,addon_p99_ms,addon_slow";
    for (size_t i = 0; i < kSections; i++)
    {
        h += ',';
        h += SectionKey(static_cast<Section>(i));
    }
    h += ",wndproc_max_us,gameplay,map_id,library_open,menu_open,sending,macros,note\n";
    return h;
}

std::string Perf::SecondRow(double aT, std::string_view aClock, const Summary& s, const Context& aContext)
{
    std::string r;
    r.reserve(256);
    AddStart(r, "second", aT, aClock);
    AddUint(r, s.Frames);
    for (const double v : {s.GameAvg, s.GameMax, s.GameP99, s.AddonAvg, s.AddonMax, s.AddonP99})
    {
        r += ',';
        AddNumber(r, v, 3);
    }
    r += ',';
    AddUint(r, s.AddonSlow);
    for (const double v : s.SectionAvg)
    {
        r += ',';
        AddNumber(r, v, 3);
    }
    r += ',';
    AddNumber(r, s.WndProcMaxUs, 1);
    AddContext(r, aContext);
    r += ",\n";
    return r;
}

std::string Perf::SpikeRow(double aT, std::string_view aClock, const Frame& f, const Context& aContext)
{
    std::string r;
    r.reserve(256);
    AddStart(r, "spike", aT, aClock);
    r += '1';
    // One frame: its value is the average, the maximum and the 99th percentile.
    for (const double v : {f.GameMs, f.GameMs, f.GameMs, f.AddonMs, f.AddonMs, f.AddonMs})
    {
        r += ',';
        AddNumber(r, v, 3);
    }
    r += f.AddonMs > kSlowAddonMs ? ",1" : ",0";
    for (const double v : f.SectionMs)
    {
        r += ',';
        AddNumber(r, v, 3);
    }
    r += ',';
    AddContext(r, aContext);
    r += ",\n";
    return r;
}

std::string Perf::EventRow(double aT, std::string_view aClock, std::string_view aName, const Context& aContext)
{
    std::string r;
    AddStart(r, "event", aT, aClock);
    // No frame values: 16 empty cells (frames, 6 times, slow, the sections, wndproc). The comma
    // after the last one is AddContext's first.
    constexpr size_t kEmpty = 1 + 6 + 1 + kSections + 1;
    r += std::string(kEmpty - 1, ',');
    AddContext(r, aContext);
    r += ',';
    r += CleanName(aName);
    r += '\n';
    return r;
}

Perf::EStop Perf::Due(double aElapsedSec, double aLimitSec, uint64_t aBytes)
{
    if (aBytes >= kMaxBytes) return EStop::SizeLimit;
    if (aLimitSec > 0.0 && aElapsedSec >= aLimitSec) return EStop::TimeLimit;
    return EStop::None;
}
