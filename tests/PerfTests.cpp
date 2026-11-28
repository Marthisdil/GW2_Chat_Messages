// Tests for src/Perf.*: the frame recorder's summaries, rows, file name and limits.

#include <string>
#include <vector>

#include "Perf.h"
#include "TestCheck.h"

namespace
{
    size_t Cells(const std::string& aRow)
    {
        size_t n = 1;
        for (const char c : aRow)
            if (c == ',') n++;
        return n;
    }

    std::string LastLine(const std::string& aText)
    {
        std::string t = aText;
        if (!t.empty() && t.back() == '\n') t.pop_back();
        const size_t nl = t.rfind('\n');
        return nl == std::string::npos ? t : t.substr(nl + 1);
    }
}

static void TestPercentile()
{
    std::vector<double> none;
    CHECK(Perf::Percentile(none, 0.99) == 0.0);
    std::vector<double> v;
    for (int i = 100; i >= 1; i--) v.push_back(i); // 1..100, reversed
    CHECK(Perf::Percentile(v, 0.99) == 99.0);
    CHECK(Perf::Percentile(v, 1.0) == 100.0);
    CHECK(Perf::Percentile(v, 0.5) == 50.0);
    CHECK(Perf::Percentile(v, 0.0) == 1.0);
    std::vector<double> one{7.0};
    CHECK(Perf::Percentile(one, 0.99) == 7.0);
}

static void TestSecond()
{
    Perf::Second s;
    CHECK(s.Empty());
    for (int i = 0; i < 4; i++)
    {
        Perf::Frame f;
        f.GameMs  = 10.0 + i;      // 10, 11, 12, 13
        f.AddonMs = i == 3 ? 3.0 : 0.5;
        f.SectionMs[static_cast<size_t>(Perf::Section::Library)] = 0.2;
        s.Add(f);
    }
    CHECK(!s.Empty());
    const Perf::Summary sum = s.Finish(42.0);
    CHECK(sum.Frames == 4);
    CHECK(sum.GameAvg == 11.5);
    CHECK(sum.GameMax == 13.0);
    CHECK(sum.GameP99 == 13.0);
    CHECK(sum.AddonMax == 3.0);
    CHECK(sum.AddonAvg == (0.5 * 3 + 3.0) / 4);
    CHECK(sum.AddonSlow == 1);
    CHECK(sum.SectionAvg[static_cast<size_t>(Perf::Section::Library)] > 0.199);
    CHECK(sum.SectionAvg[static_cast<size_t>(Perf::Section::Saves)] == 0.0);
    CHECK(sum.WndProcMaxUs == 42.0);
    // Finish starts the next second.
    CHECK(s.Empty());
    const Perf::Summary next = s.Finish(0.0);
    CHECK(next.Frames == 0);
    CHECK(next.GameAvg == 0.0);
    CHECK(next.SectionAvg[static_cast<size_t>(Perf::Section::Library)] == 0.0);
}

static void TestSpike()
{
    Perf::Frame f;
    f.GameMs = 16.0;
    f.AddonMs = 0.3;
    CHECK(!Perf::IsSpike(f));
    f.AddonMs = 2.5;
    CHECK(Perf::IsSpike(f));
    f.AddonMs = 0.3;
    f.GameMs  = 80.0;
    CHECK(Perf::IsSpike(f));
}

static void TestCleanName()
{
    CHECK(Perf::CleanName("send-start") == "send-start");
    CHECK(Perf::CleanName("a,b\nc\"d") == "a_b_c_d");
    CHECK(Perf::CleanName(std::string(100, 'x')).size() == 40);
    CHECK(Perf::CleanName("map 1234: Lion's Arch") == "map 1234: Lion_s Arch");
}

static void TestFileName()
{
    // 2026-11-28 01:02:03 UTC
    const std::time_t t = 1795827723;
    CHECK(Perf::FileName(t) == "perf_2026-11-28_01-02-03_UTC.csv");
    CHECK(Perf::FileName(t, 1) == "perf_2026-11-28_01-02-03_UTC.csv");
    CHECK(Perf::FileName(t, 2) == "perf_2026-11-28_01-02-03_2_UTC.csv");
    CHECK(Perf::FileName(t, 2) != Perf::FileName(t));
    CHECK(Perf::ClockText(t) == "01:02:03");
}

static void TestRows()
{
    const std::string header = Perf::Header("2026.11.21.1412.Dev", "abc1234, 2026-11-28 00:01 UTC", 10);
    CHECK(header.rfind("# Frame recording, GW2 Chat Messages 2026.11.21.1412.Dev", 0) == 0);
    CHECK(header.find("limit 10 min") != std::string::npos);
    const std::string columns = LastLine(header);
    const size_t cells = Cells(columns);
    CHECK(cells == 26);
    CHECK(columns.find("library_ms") != std::string::npos);

    Perf::Context c;
    c.Gameplay = true;
    c.MapId = 1206;
    c.LibraryOpen = true;
    c.Macros = 12;

    Perf::Second s;
    Perf::Frame f;
    f.GameMs = 16.667;
    f.AddonMs = 0.25;
    s.Add(f);
    const std::string second = Perf::SecondRow(1.0, "01:02:03", s.Finish(5.0), c);
    CHECK(second.back() == '\n');
    CHECK(Cells(LastLine(second)) == cells);
    CHECK(second.rfind("second,01:02:03,1.000,1,16.667,", 0) == 0);
    CHECK(second.find(",1,1206,1,0,0,12,\n") != std::string::npos);

    const std::string spike = Perf::SpikeRow(2.5, "01:02:04", f, c);
    CHECK(Cells(LastLine(spike)) == cells);
    CHECK(spike.rfind("spike,01:02:04,2.500,1,", 0) == 0);

    const std::string ev = Perf::EventRow(3.0, "01:02:05", "send,start\n", c);
    CHECK(Cells(LastLine(ev)) == cells);
    CHECK(ev.find(",send_start_\n") != std::string::npos);
    CHECK(ev.rfind("event,01:02:05,3.000,", 0) == 0);
}

static void TestDue()
{
    CHECK(Perf::Due(10.0, 600.0, 1000) == Perf::EStop::None);
    CHECK(Perf::Due(600.0, 600.0, 1000) == Perf::EStop::TimeLimit);
    CHECK(Perf::Due(1.0, 600.0, Perf::kMaxBytes) == Perf::EStop::SizeLimit);
    CHECK(Perf::Due(1e9, 0.0, 0) == Perf::EStop::None); // no time limit
}

void RunPerfTests()
{
    TestPercentile();
    TestSecond();
    TestSpike();
    TestCleanName();
    TestFileName();
    TestRows();
    TestDue();
}
