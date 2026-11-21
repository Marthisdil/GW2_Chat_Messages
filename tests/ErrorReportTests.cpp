// Standalone tests for src/ErrorReport.cpp (the error window's queue and clipboard text).

#include <string>
#include <vector>

#include "ErrorReport.h"
#include "TestCheck.h"

using namespace ErrorReport;

namespace
{
    void TestText()
    {
        Report r;
        r.Action  = "Created a folder named \"test\" in \"Raids\"";
        r.Message = "There is already a folder called \"Test\" here.";
        r.When    = "2026-11-21 14:03:22";
        r.Where   = "PvE, Lion's Arch (50)";
        CHECK(ToText(r, "v0.1.0") ==
              "Chat Messages v0.1.0 error\r\n"
              "When: 2026-11-21 14:03:22\r\n"
              "What I did: Created a folder named \"test\" in \"Raids\"\r\n"
              "Error: There is already a folder called \"Test\" here.\r\n"
              "In game: PvE, Lion's Arch (50)\r\n");

        // Unknown parts are left out; the error is always there.
        Report bare;
        bare.Message = "Oops.";
        CHECK(ToText(bare, "v1") == "Chat Messages v1 error\r\nError: Oops.\r\n");
    }

    void TestQueue()
    {
        std::vector<Report> q;
        Add(q, {"Clicked Export all", "There's nothing to export yet.", "t1", ""});
        Add(q, {"Clicked Export all", "There's nothing to export yet.", "t2", ""}); // repeated click
        CHECK(q.size() == 1 && q[0].When == "t1");
        Add(q, {"Clicked New folder", "Too deep.", "t3", ""});
        Add(q, {"Clicked Export all", "There's nothing to export yet.", "t4", ""}); // not the last one: kept
        CHECK(q.size() == 3 && q[2].When == "t4");

        // At most kMaxQueued; the oldest go first.
        std::vector<Report> many;
        for (size_t i = 0; i < kMaxQueued + 5; i++) Add(many, {"Action " + std::to_string(i), "Error", "", ""});
        CHECK(many.size() == kMaxQueued);
        CHECK(many.front().Action == "Action 5" && many.back().Action == "Action " + std::to_string(kMaxQueued + 4));
    }
}

void RunErrorReportTests()
{
    TestText();
    TestQueue();
}
