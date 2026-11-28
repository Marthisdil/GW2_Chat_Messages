#pragma once

// The frame recorder (docs/performance.md, "The frame recorder"): times every frame of the game
// and the addon's part of it, and writes a CSV file to <addon>\perf\ in the background.
//
// It's only in builds made on the owner's PC: the .vcxproj defines CHATMSG_PERF_RECORDER unless
// the build runs on GitHub Actions, so no published release (Dev, Beta or Rel) has it, and CI
// checks that its DLLs don't. It also needs a Dev version. Without the define, everything here is
// an empty inline function and the recorder isn't in the DLL at all.
//
// Render thread only, except WndProcTimer (the WndProc hooks' thread).

#include <cstdint>
#include <string>

#include "Perf.h"

namespace PerfRec
{
    enum class EStop
    {
        None,
        ByUser,
        TimeLimit,
        SizeLimit,
        Unload,
        AdvancedOff, // Advanced options were turned off
        WriteFailed
    };

    struct Live
    {
        bool        Recording  = false;
        double      ElapsedSec = 0.0;
        double      LimitSec   = 0.0;
        double      GameAvgMs  = 0.0; // the last second
        double      AddonAvgMs = 0.0; // the whole recording
        double      AddonMaxMs = 0.0;
        uint64_t    Frames     = 0;
        uint64_t    SlowFrames = 0;   // addon over Perf::kSlowAddonMs
        uint64_t    Bytes      = 0;
        std::string File;             // the file's name (this recording, or the last one)
        EStop       LastStop   = EStop::None;
    };

#ifdef CHATMSG_PERF_RECORDER
    bool Available(); // compiled in, and a Dev version
    void Init();      // AddonLoad: says in the log that the recorder is in this build
    void Shutdown();  // AddonUnload, before Storage::Shutdown: stops a recording

    bool Recording();
    bool Start(int aMinutes, std::string& aError); // false with aError if it can't start
    void Stop(EStop aWhy);
    void Mark(); // an event row "mark N"
    void Event(const char* aName);
    void SetContext(const Perf::Context& aContext); // once a frame; changes become event rows
    Live GetLive();

    void FrameBegin(); // first thing in AddonRender: the previous frame ends here
    void FrameEnd();   // last thing in AddonRender

    // Times a part of the frame. aTopLevel: a callback of its own (the options page, the Quick
    // Access menu), whose time also counts as the addon's.
    class Scope
    {
    public:
        explicit Scope(Perf::Section aSection, bool aTopLevel = false);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        int64_t       m_Start = 0; // 0: not recording
        Perf::Section m_Section;
        bool          m_TopLevel;
    };

    // Times one pass through a WndProc hook (any thread; the slowest per second is kept).
    class WndProcTimer
    {
    public:
        WndProcTimer();
        ~WndProcTimer();
        WndProcTimer(const WndProcTimer&) = delete;
        WndProcTimer& operator=(const WndProcTimer&) = delete;

    private:
        int64_t m_Start = 0;
    };
#else
    inline bool Available() { return false; }
    inline void Init() {}
    inline void Shutdown() {}
    inline bool Recording() { return false; }
    inline bool Start(int, std::string& aError) { aError.clear(); return false; }
    inline void Stop(EStop) {}
    inline void Mark() {}
    inline void Event(const char*) {}
    inline void SetContext(const Perf::Context&) {}
    inline Live GetLive() { return {}; }
    inline void FrameBegin() {}
    inline void FrameEnd() {}
    class Scope
    {
    public:
        explicit Scope(Perf::Section, bool = false) {}
    };
    class WndProcTimer
    {
    };
#endif
}
