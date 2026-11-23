#include "Cooldown.h"

#include <algorithm>
#include <cmath>

namespace Cooldown
{
    int WholeSeconds(double aSeconds)
    {
        if (!(aSeconds > 0.0)) return 0;
        const double up = std::ceil(aSeconds - 1e-9);
        return up > kMaxSec + 1 ? kMaxSec + 1 : std::max(1, static_cast<int>(up));
    }

    void Tracker::Started(uint64_t aId, double aNow)
    {
        Last& l    = m_Last[aId];
        l.HasStart = true;
        l.Start    = aNow;
        l.HasEnd   = false; // running: the sender itself ignores presses until it ends
        l.Alerted  = false;
    }

    void Tracker::Ended(uint64_t aId, double aNow, bool aSent)
    {
        Last& l  = m_Last[aId];
        l.HasEnd = true;
        l.End    = aNow;
        if (!aSent) l.HasStart = false;
    }

    Wait Tracker::ForHotkey(uint64_t aId, int aCooldownSec, double aNow) const
    {
        Wait w;
        auto it = m_Last.find(aId);
        if (it == m_Last.end()) return w;
        const Last& l = it->second;
        const int cooldown = std::clamp(aCooldownSec, 0, kMaxSec);
        if (cooldown > 0 && l.HasStart)
        {
            const double left = cooldown - (aNow - l.Start);
            if (left > 0.0)
            {
                w.Seconds    = left;
                w.ByCooldown = true;
            }
        }
        if (l.HasEnd)
        {
            const double left = kGuardSec - (aNow - l.End);
            if (left > w.Seconds) w.Seconds = left; // stays ByCooldown if the cooldown blocks too
        }
        return w;
    }

    bool Tracker::TakeAlert(uint64_t aId)
    {
        auto it = m_Last.find(aId);
        if (it == m_Last.end() || it->second.Alerted) return false;
        it->second.Alerted = true;
        return true;
    }
}
