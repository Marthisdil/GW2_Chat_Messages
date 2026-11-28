#pragma once

// The frame recorder's rules (docs/performance.md, "The frame recorder"): what one frame and one
// second hold, the CSV rows, the file name and when a recording stops. No Nexus, ImGui or
// windows.h dependency. Tested by tests/PerfTests.cpp. The recorder itself (timing, the file) is
// src/PerfRecorder.*, compiled in only for builds made on the owner's PC.

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

namespace Perf
{
    // The parts of the addon's frame that are timed on their own.
    enum class Section : uint8_t
    {
        Languages, // Languages::Update (a language change or reload)
        Poll,      // the sender, the map, link files, hotkeys (Ui::Render's start)
        Library,   // the library window
        QuickMenu, // the quick menu (the pop-up and the Quick Access menu)
        Windows,   // the other windows: tour, what's new, import, export, errors
        Saves,     // saves, backups and the map list check (Ui::Render's end)
        Options,   // the addon's page in Nexus > Options
        Count
    };
    constexpr size_t kSections = static_cast<size_t>(Section::Count);
    const char* SectionKey(Section aSection); // the CSV column's name, e.g. "library_ms"

    constexpr double   kSlowAddonMs  = 2.0;  // an addon frame over this counts as slow (and is a spike)
    constexpr double   kSlowGameMs   = 50.0; // a game frame over this is a spike (under 20 fps)
    constexpr size_t   kSpikesPerSec = 20;   // spike rows kept per second, at most
    constexpr uint64_t kMaxBytes     = 50ull * 1024 * 1024; // the file never grows past this
    constexpr int      kMinutes[]    = {5, 10, 30, 60};     // the lengths to choose from

    struct Frame
    {
        double GameMs  = 0.0; // from the start of this frame's AddonRender to the next one's
        double AddonMs = 0.0; // our callbacks' time in it
        double SectionMs[kSections] = {};
    };

    // What the player was doing, for the rows.
    struct Context
    {
        bool     Gameplay    = false;
        uint32_t MapId       = 0;
        bool     LibraryOpen = false;
        bool     MenuOpen    = false;
        bool     Sending     = false;
        uint32_t Macros      = 0;
        bool operator==(const Context& o) const
        {
            return Gameplay == o.Gameplay && MapId == o.MapId && LibraryOpen == o.LibraryOpen && MenuOpen == o.MenuOpen &&
                   Sending == o.Sending && Macros == o.Macros;
        }
    };

    struct Summary
    {
        size_t Frames   = 0;
        double GameAvg  = 0.0, GameMax = 0.0, GameP99 = 0.0;
        double AddonAvg = 0.0, AddonMax = 0.0, AddonP99 = 0.0;
        size_t AddonSlow = 0; // frames over kSlowAddonMs
        double SectionAvg[kSections] = {};
        double WndProcMaxUs = 0.0; // the slowest pass through our hotkey hook (Hotkeys.cpp)
    };

    // One second's frames. Keeps its memory between seconds (no allocation per frame).
    class Second
    {
    public:
        void Add(const Frame& aFrame);
        bool Empty() const { return m_Game.empty(); }
        // The summary of the frames added since the last Finish; starts the next second.
        Summary Finish(double aWndProcMaxUs);

    private:
        std::vector<double> m_Game, m_Addon;
        double m_SectionSum[kSections] = {};
    };

    // The value below which aP (0..1) of the values lie (nearest rank). Reorders aValues.
    double Percentile(std::vector<double>& aValues, double aP);

    bool IsSpike(const Frame& aFrame);

    // A name that's safe in a CSV cell: letters, digits, space, '-', '_', '.', ':', at most 40.
    std::string CleanName(std::string_view aName);

    // "perf_YYYY-MM-DD_HH-MM-SS_UTC.csv"
    // aCopy > 1: a second recording started in the same second ("..._01-02-03_2_UTC.csv").
    std::string FileName(std::time_t aUtc, unsigned aCopy = 1);
    // "HH:MM:SS" (UTC)
    std::string ClockText(std::time_t aUtc);

    // The file's first lines: what made it, then the column names.
    std::string Header(std::string_view aVersion, std::string_view aBuild, int aMinutes);
    // One row each. aT is the seconds since the recording started.
    std::string SecondRow(double aT, std::string_view aClock, const Summary& aSummary, const Context& aContext);
    std::string SpikeRow(double aT, std::string_view aClock, const Frame& aFrame, const Context& aContext);
    std::string EventRow(double aT, std::string_view aClock, std::string_view aName, const Context& aContext);

    enum class EStop
    {
        None,
        TimeLimit,
        SizeLimit
    };
    // Whether a recording that has run aElapsedSec and written aBytes must stop now.
    EStop Due(double aElapsedSec, double aLimitSec, uint64_t aBytes);
}
