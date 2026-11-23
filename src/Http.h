#pragma once

// A small blocking HTTP(S) GET built on WinHTTP (same approach as the GW2_WvW_Info addon).
// Used for GW2 API map names, with an address built into the addon: never one written in a macro
// or read from a file. Call it from worker threads only, never from the render thread: it can
// take up to the timeout.

#include <cstddef>
#include <string>

namespace Http
{
    struct Response
    {
        int         Status = 0; // HTTP status, or 0 if the request didn't get that far
        std::string Body;
        std::string Error;      // why it failed, in words the user can read

        bool Ok() const { return Status >= 200 && Status < 300 && Error.empty(); }
    };

    constexpr int    kDefaultTimeoutMs = 3000;
    constexpr size_t kDefaultMaxBytes  = 512 * 1024;

    // aUrl must start with http:// or https://. Bodies over aMaxBytes are refused.
    Response Get(const std::string& aUrl, int aTimeoutMs = kDefaultTimeoutMs, size_t aMaxBytes = kDefaultMaxBytes);
}
