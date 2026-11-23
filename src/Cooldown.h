#pragma once
// Hotkey cooldowns (docs/design.md, "Cooldown"): how long a macro's hotkey waits before it sends
// that macro again. Times are seconds on any steady clock (the UI uses ImGui::GetTime()).
// No Nexus or ImGui dependency. Tested by tests/CooldownTests.cpp.

#include <cstdint>
#include <map>

namespace Cooldown
{
    // After a send of a macro ends, its hotkey ignores presses for this long, cooldown or not,
    // so a key held down or pressed twice by accident doesn't send the same macro twice.
    constexpr double kGuardSec = 1.0;
    constexpr int    kMaxSec   = 600; // the longest cooldown a macro can have (10 minutes)

    // Why a hotkey press is ignored, and for how long.
    struct Wait
    {
        double Seconds    = 0.0;   // 0 = the hotkey may send
        bool   ByCooldown = false; // true: the macro's own cooldown; false: only the 1 s guard
    };

    // Seconds shown to the user: always rounded up, so "1 s" until it's really over.
    int WholeSeconds(double aSeconds);

    // The last send of each macro, by macro id.
    class Tracker
    {
    public:
        // A send of the macro started (from anywhere: hotkey, quick menu or Send now).
        void Started(uint64_t aId, double aNow);
        // The send ended. A send that failed doesn't start the cooldown (nothing may have been
        // sent), only the guard.
        void Ended(uint64_t aId, double aNow, bool aSent);

        // How long the macro's hotkey must still wait. aCooldownSec 0 = no cooldown.
        Wait ForHotkey(uint64_t aId, int aCooldownSec, double aNow) const;

        // True once per cooldown: the first ignored press shows an alert, later ones are silent.
        // A new send starts a new cooldown.
        bool TakeAlert(uint64_t aId);

        void Forget(uint64_t aId) { m_Last.erase(aId); }
        void Clear() { m_Last.clear(); }

    private:
        struct Last
        {
            bool   HasStart = false;
            double Start    = 0.0;
            bool   HasEnd   = false;
            double End      = 0.0;
            bool   Alerted  = false;
        };
        std::map<uint64_t, Last> m_Last;
    };
}
