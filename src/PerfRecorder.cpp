#include "PerfRecorder.h"

// Only in builds made on the owner's PC (PerfRecorder.h). Without CHATMSG_PERF_RECORDER this
// file is empty.
#ifdef CHATMSG_PERF_RECORDER

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <ctime>
#include <filesystem>

#include "BuildStamp.h"
#include "Globals.h"
#include "Localization.h"
#include "PathText.h"
#include "Storage.h"

namespace
{
    // CI looks for this text in its DLLs and fails if it's there (.github/workflows): a published
    // build must never have the recorder. Logged at load so the linker keeps it.
    constexpr char kMarker[] = "CHATMSG-FRAME-RECORDER: built locally";

    int64_t Now()
    {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t.QuadPart;
    }

    double MsPerTick()
    {
        static const double ms = [] {
            LARGE_INTEGER f;
            QueryPerformanceFrequency(&f);
            return f.QuadPart > 0 ? 1000.0 / static_cast<double>(f.QuadPart) : 0.0;
        }();
        return ms;
    }

    double Ms(int64_t aTicks) { return static_cast<double>(aTicks) * MsPerTick(); }

    // Read by the WndProc hooks' thread.
    std::atomic<bool>    s_WndOn{false};
    std::atomic<int64_t> s_WndMaxTicks{0};
    // Set by a writer job whose write failed.
    std::atomic<bool>    s_WriteFailed{false};

    // Render thread only.
    bool                  s_On = false;
    std::filesystem::path s_File;
    std::string           s_FileName;
    std::time_t           s_FileUtc  = 0;  // the second s_FileName was made in
    unsigned              s_FileCopy = 1;  // its copy number (several starts in one second)
    int64_t               s_StartTick  = 0;
    double                s_LimitSec   = 0.0;
    uint64_t              s_Bytes      = 0;
    std::string           s_Pending;       // rows not handed to the writer yet
    Perf::Second          s_Second;
    int64_t               s_SecondTick = 0; // when this second started
    size_t                s_Spikes     = 0; // spike rows this second
    std::string           s_Clock;          // UTC "HH:MM:SS", updated each second
    Perf::Frame           s_Frame;          // the frame being timed
    int64_t               s_FrameTick  = 0; // its AddonRender's start
    bool                  s_HaveFrame  = false;
    int64_t               s_RenderTick = 0; // this AddonRender's start
    Perf::Context         s_Ctx;
    bool                  s_HaveCtx    = false;
    uint64_t              s_Frames = 0, s_Slow = 0, s_Marks = 0;
    double                s_AddonSum = 0.0, s_AddonMax = 0.0, s_LastGameAvg = 0.0;
    PerfRec::EStop        s_LastStop = PerfRec::EStop::None;

    double Elapsed(int64_t aNow) { return Ms(aNow - s_StartTick) / 1000.0; }

    const char* StopName(PerfRec::EStop aWhy)
    {
        switch (aWhy)
        {
        case PerfRec::EStop::ByUser:      return "stop: by you";
        case PerfRec::EStop::TimeLimit:   return "stop: time limit";
        case PerfRec::EStop::SizeLimit:   return "stop: size limit";
        case PerfRec::EStop::Unload:      return "stop: unload";
        case PerfRec::EStop::AdvancedOff: return "stop: advanced options off";
        case PerfRec::EStop::WriteFailed: return "stop: write failed";
        default:                          return "stop";
        }
    }

    void Add(const std::string& aRow)
    {
        s_Pending += aRow;
        s_Bytes += aRow.size();
    }

    // Hands the waiting rows to the writer thread (the file is only written there).
    void Flush()
    {
        if (s_Pending.empty()) return;
        std::string text;
        text.swap(s_Pending);
        const std::filesystem::path file = s_File;
        Storage::Post([file, text = std::move(text)] {
            if (!Storage::AppendText(file, text, false)) s_WriteFailed = true;
        });
    }

    void EndSecond(int64_t aNow)
    {
        const double wnd = Ms(s_WndMaxTicks.exchange(0)) * 1000.0;
        const Perf::Summary sum = s_Second.Finish(wnd);
        if (sum.Frames > 0)
        {
            s_LastGameAvg = sum.GameAvg;
            Add(Perf::SecondRow(Elapsed(aNow), s_Clock, sum, s_Ctx));
        }
        s_SecondTick = aNow;
        s_Spikes     = 0;
        s_Clock      = Perf::ClockText(std::time(nullptr));
    }
}

bool PerfRec::Available() { return std::strcmp(kVersionChannel, "Dev") == 0; }

void PerfRec::Init()
{
    if (Available()) LogF(LOGL_INFO, "Frame recorder: in this build (%s). Start it on the options page (Advanced options).", kMarker);
}

void PerfRec::Shutdown() { Stop(EStop::Unload); }

bool PerfRec::Recording() { return s_On; }

bool PerfRec::Start(int aMinutes, std::string& aError)
{
    aError.clear();
    if (!Available()) { aError = TR("The frame recorder is only in Dev builds made on your own PC."); return false; }
    if (s_On) return true;
    // No disk work here (the options page): the writer job makes the folder (AppendText).
    const std::filesystem::path addon = Storage::AddonDir();
    if (addon.empty()) { aError = TR("The addon folder isn't available."); return false; }
    const std::filesystem::path dir = addon / "perf";
    const std::time_t utc = std::time(nullptr);
    // A new name every time: a restart within the same second must not empty the last file.
    s_FileCopy = (utc == s_FileUtc && !s_FileName.empty()) ? s_FileCopy + 1 : 1;
    s_FileUtc  = utc;
    s_FileName = Perf::FileName(utc, s_FileCopy);
    s_File     = dir / PathText::FromUtf8(s_FileName);
    s_LimitSec = std::max(aMinutes, 1) * 60.0;
    s_WriteFailed = false;
    s_Pending.clear();
    s_Bytes = 0;
    s_Second.Finish(0.0);
    s_Frame     = Perf::Frame{};
    s_HaveFrame = false;
    s_HaveCtx   = false;
    s_Frames = s_Slow = s_Marks = 0;
    s_AddonSum = s_AddonMax = s_LastGameAvg = 0.0;
    s_Clock      = Perf::ClockText(utc);
    s_StartTick  = Now();
    s_SecondTick = s_StartTick;
    s_Spikes     = 0;
    s_WndMaxTicks = 0;

    // The header starts the file (emptied, in case the name exists); rows are appended after it.
    const std::string header = Perf::Header(kVersionText, BuildStamp::Text(), aMinutes);
    s_Bytes = header.size();
    const std::filesystem::path file = s_File;
    Storage::Post([file, header] {
        if (!Storage::AppendText(file, header, true)) s_WriteFailed = true;
    });
    s_On    = true;
    s_WndOn = true;
    LogF(LOGL_INFO, "Frame recorder: recording to perf\\%s (at most %d min).", s_FileName.c_str(), aMinutes);
    return true;
}

void PerfRec::Stop(EStop aWhy)
{
    if (!s_On) return;
    const int64_t now = Now();
    EndSecond(now);
    Add(Perf::EventRow(Elapsed(now), s_Clock, StopName(aWhy), s_Ctx));
    Flush();
    s_On       = false;
    s_WndOn    = false;
    s_LastStop = aWhy;
    LogF(aWhy == EStop::WriteFailed ? LOGL_WARNING : LOGL_INFO, "Frame recorder: %s after %.0f s, %llu frames: perf\\%s.",
         StopName(aWhy), Elapsed(now), static_cast<unsigned long long>(s_Frames), s_FileName.c_str());
}

void PerfRec::Mark()
{
    if (!s_On) return;
    char name[32];
    std::snprintf(name, sizeof(name), "mark %llu", static_cast<unsigned long long>(++s_Marks));
    Event(name);
}

void PerfRec::Event(const char* aName)
{
    if (!s_On || !aName) return;
    Add(Perf::EventRow(Elapsed(Now()), s_Clock, aName, s_Ctx));
}

void PerfRec::SetContext(const Perf::Context& c)
{
    if (!s_On) return;
    if (s_HaveCtx)
    {
        const Perf::Context old = s_Ctx;
        s_Ctx = c; // the event rows show the new state
        if (c.LibraryOpen != old.LibraryOpen) Event(c.LibraryOpen ? "library open" : "library closed");
        if (c.MenuOpen != old.MenuOpen) Event(c.MenuOpen ? "quick menu open" : "quick menu closed");
        if (c.Sending != old.Sending) Event(c.Sending ? "send start" : "send end");
        if (c.Gameplay != old.Gameplay) Event(c.Gameplay ? "gameplay" : "not in gameplay");
        if (c.MapId != old.MapId) Event("map change");
        return;
    }
    s_Ctx     = c;
    s_HaveCtx = true;
}

PerfRec::Live PerfRec::GetLive()
{
    Live l;
    l.Recording  = s_On;
    l.ElapsedSec = s_On ? Elapsed(Now()) : 0.0;
    l.LimitSec   = s_LimitSec;
    l.GameAvgMs  = s_LastGameAvg;
    l.AddonAvgMs = s_Frames > 0 ? s_AddonSum / static_cast<double>(s_Frames) : 0.0;
    l.AddonMaxMs = s_AddonMax;
    l.Frames     = s_Frames;
    l.SlowFrames = s_Slow;
    l.Bytes      = s_Bytes;
    l.File       = s_FileName;
    l.LastStop   = s_LastStop;
    return l;
}

void PerfRec::FrameBegin()
{
    if (!s_On) return;
    const int64_t now = Now();
    if (s_HaveFrame)
    {
        s_Frame.GameMs = Ms(now - s_FrameTick);
        s_Second.Add(s_Frame);
        s_Frames++;
        s_AddonSum += s_Frame.AddonMs;
        s_AddonMax = std::max(s_AddonMax, s_Frame.AddonMs);
        if (s_Frame.AddonMs > Perf::kSlowAddonMs) s_Slow++;
        if (Perf::IsSpike(s_Frame) && s_Spikes < Perf::kSpikesPerSec)
        {
            s_Spikes++;
            Add(Perf::SpikeRow(Elapsed(now), s_Clock, s_Frame, s_Ctx));
        }
    }
    s_Frame      = Perf::Frame{};
    s_FrameTick  = now;
    s_HaveFrame  = true;
    s_RenderTick = now;

    if (Ms(now - s_SecondTick) >= 1000.0)
    {
        EndSecond(now);
        Flush();
        if (s_WriteFailed) { Stop(EStop::WriteFailed); return; }
        switch (Perf::Due(Elapsed(now), s_LimitSec, s_Bytes))
        {
        case Perf::EStop::TimeLimit: Stop(EStop::TimeLimit); return;
        case Perf::EStop::SizeLimit: Stop(EStop::SizeLimit); return;
        default: break;
        }
    }
}

void PerfRec::FrameEnd()
{
    if (!s_On) return;
    s_Frame.AddonMs += Ms(Now() - s_RenderTick);
}

PerfRec::Scope::Scope(Perf::Section aSection, bool aTopLevel) : m_Section(aSection), m_TopLevel(aTopLevel)
{
    if (s_On) m_Start = Now();
}

PerfRec::Scope::~Scope()
{
    if (m_Start == 0 || !s_On) return;
    const double ms = Ms(Now() - m_Start);
    const size_t i  = static_cast<size_t>(m_Section);
    if (i < Perf::kSections) s_Frame.SectionMs[i] += ms;
    if (m_TopLevel) s_Frame.AddonMs += ms;
}

PerfRec::WndProcTimer::WndProcTimer()
{
    if (s_WndOn.load(std::memory_order_relaxed)) m_Start = Now();
}

PerfRec::WndProcTimer::~WndProcTimer()
{
    if (m_Start == 0) return;
    const int64_t d = Now() - m_Start;
    int64_t seen = s_WndMaxTicks.load(std::memory_order_relaxed);
    while (d > seen && !s_WndMaxTicks.compare_exchange_weak(seen, d, std::memory_order_relaxed)) {}
}

#endif
