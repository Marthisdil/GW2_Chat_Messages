#include "ErrorReport.h"

namespace ErrorReport
{
    std::string ToText(const Report& aReport, const std::string& aAddonVersion)
    {
        std::string t = "Chat Messages " + aAddonVersion + " error\r\n";
        if (!aReport.When.empty())   t += "When: " + aReport.When + "\r\n";
        if (!aReport.Action.empty()) t += "What I did: " + aReport.Action + "\r\n";
        t += "Error: " + aReport.Message + "\r\n";
        if (!aReport.Where.empty())  t += "In game: " + aReport.Where + "\r\n";
        return t;
    }

    void Add(std::vector<Report>& aQueue, Report aReport)
    {
        if (!aQueue.empty() && aQueue.back().Action == aReport.Action && aQueue.back().Message == aReport.Message)
            return;
        aQueue.push_back(std::move(aReport));
        while (aQueue.size() > kMaxQueued) aQueue.erase(aQueue.begin());
    }
}
