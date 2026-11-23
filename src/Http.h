#pragma once

// A small blocking HTTP(S) GET built on WinHTTP (same approach as the GW2_WvW_Info addon).
// Used for GW2 API map names and the {json} placeholder. Call it from worker threads only,
// never from the render thread: it can take up to the timeout.

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

    // Get for an address written in a macro ({json}), which may come from someone else's import:
    // it only talks to public internet addresses (NetAddress), never to this PC or a local
    // network (SEC-012). It uses the same proxy as Get (WinHTTP's default setting), pinned for
    // the request:
    //  - no proxy: the address of the connection WinHTTP really uses is checked before every
    //    request (redirects and reused keep-alive connections too), so a name that resolves to
    //    127.0.0.1 or a LAN address, even only on a second lookup, is refused before it is sent;
    //  - a proxy: the connection is the proxy's, so the target's name is looked up first, and
    //    every redirect's, and any local address refuses it. A name that can't be looked up, or
    //    isn't a plain name or IP literal, is refused. The real connection is still checked: a
    //    local address passes only if it is the proxy's own (so bypass-list hosts are checked
    //    too). Accepted limit: a DNS server that tells the proxy something else.
    Response GetPublic(const std::string& aUrl, int aTimeoutMs = kDefaultTimeoutMs, size_t aMaxBytes = kDefaultMaxBytes);
}
