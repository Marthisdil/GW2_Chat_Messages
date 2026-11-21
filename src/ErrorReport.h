#pragma once

// Error messages shown in their own window (docs/design.md, "Error window"): what went wrong,
// what the user did to get it, and a text version for "Copy to clipboard" so it can be pasted
// into a bug report. No Nexus or ImGui dependency (tested by tests/ErrorReportTests.cpp).

#include <cstddef>
#include <string>
#include <vector>

namespace ErrorReport
{
    constexpr size_t kMaxQueued = 20; // older errors are dropped beyond this

    struct Report
    {
        std::string Action;  // what the user did, e.g. "Created a folder named "test" in "Raids""
        std::string Message; // the error itself
        std::string When;    // local time, "2026-11-21 14:03:22"
        std::string Where;   // where in game, e.g. "PvE, Lion's Arch (50)" ("" if unknown)
    };

    // The text copied to the clipboard: addon and version, time, what was done, the error, where.
    std::string ToText(const Report& aReport, const std::string& aAddonVersion);

    // Adds a report to the end of the queue, unless it's the same action and message as the last
    // one (a repeated click). Keeps at most kMaxQueued, dropping the oldest.
    void Add(std::vector<Report>& aQueue, Report aReport);
}
