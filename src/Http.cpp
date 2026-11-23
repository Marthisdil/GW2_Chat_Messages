#include "Http.h"

#include <windows.h>
#include <winsock2.h> // GetAddrInfoW, SOCKADDR_STORAGE (WIN32_LEAN_AND_MEAN keeps the old winsock.h out)
#include <ws2tcpip.h>
#include <winhttp.h>

#include <cstddef>
#include <vector>

#include "Globals.h"
#include "NetAddress.h"

namespace
{
    std::wstring Widen(const std::string& aStr)
    {
        if (aStr.empty()) return {};
        const int len = MultiByteToWideChar(CP_UTF8, 0, aStr.data(), static_cast<int>(aStr.size()), nullptr, 0);
        std::wstring out(static_cast<size_t>(len), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, aStr.data(), static_cast<int>(aStr.size()), out.data(), len);
        return out;
    }

    // Closes a WinHTTP handle when it goes out of scope.
    struct Handle
    {
        HINTERNET H = nullptr;
        ~Handle() { if (H) WinHttpCloseHandle(H); }
    };

    std::string Failed(const char* aWhat)
    {
        const DWORD e = GetLastError();
        if (e == ERROR_WINHTTP_TIMEOUT) return "the address didn't answer in time.";
        if (e == ERROR_WINHTTP_NAME_NOT_RESOLVED) return "the address couldn't be found (no internet?).";
        if (e == ERROR_WINHTTP_CANNOT_CONNECT) return "couldn't connect to the address.";
        if (e == ERROR_WINHTTP_SECURE_FAILURE) return "the secure connection failed.";
        return std::string(aWhat) + " failed (error " + std::to_string(e) + ").";
    }

    // NetAddress::IsPublicSockaddr reads addresses as bytes, with these Windows layouts.
    static_assert(AF_INET == NetAddress::kFamilyV4 && AF_INET6 == NetAddress::kFamilyV6, "address families");
    static_assert(offsetof(SOCKADDR_STORAGE, ss_family) == 0 && sizeof(ADDRESS_FAMILY) == 2, "family field");
    static_assert(offsetof(SOCKADDR_IN, sin_addr) == NetAddress::kV4Offset, "IPv4 address offset");
    static_assert(offsetof(SOCKADDR_IN6, sin6_addr) == NetAddress::kV6Offset, "IPv6 address offset");

    // Whether every address a host name has is public (behind a proxy, where the connection we see
    // is the proxy's). Fails closed: a name we can't look up is refused (owner's decision,
    // SEC-012). aWhy gets the reason, aAddress the refused address if there is one.
    bool HostIsPublic(std::wstring aHost, std::string& aWhy, std::string& aAddress)
    {
        if (aHost.size() >= 2 && aHost.front() == L'[' && aHost.back() == L']') aHost = aHost.substr(1, aHost.size() - 2);
        if (aHost.empty()) { aWhy = "unknown"; return false; }
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { aWhy = "lookup"; return false; }
        ADDRINFOW hints{};
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        ADDRINFOW* list = nullptr;
        const int rc = GetAddrInfoW(aHost.c_str(), nullptr, &hints, &list);
        bool ok = rc == 0 && list != nullptr;
        if (!ok) aWhy = "lookup";
        for (const ADDRINFOW* a = list; ok && a; a = a->ai_next)
        {
            const auto* bytes = reinterpret_cast<const unsigned char*>(a->ai_addr);
            const size_t size = a->ai_addr ? a->ai_addrlen : 0;
            if (!NetAddress::IsPublicSockaddr(bytes, size))
            {
                ok = false;
                aWhy = "local";
                aAddress = NetAddress::SockaddrText(bytes, size);
            }
        }
        if (list) FreeAddrInfoW(list);
        WSACleanup();
        return ok;
    }

    // The proxy WinHTTP's default setting would use (netsh winhttp), copied so the session uses
    // exactly that, and we know whether the connection we see is the proxy's.
    struct ProxyConfig
    {
        bool         Named = false;
        std::wstring Proxy, Bypass;

        ProxyConfig()
        {
            WINHTTP_PROXY_INFO info{};
            if (!WinHttpGetDefaultProxyConfiguration(&info)) return;
            Named = info.dwAccessType == WINHTTP_ACCESS_TYPE_NAMED_PROXY && info.lpszProxy && *info.lpszProxy;
            if (info.lpszProxy) Proxy = info.lpszProxy;
            if (info.lpszProxyBypass) Bypass = info.lpszProxyBypass;
            if (info.lpszProxy) GlobalFree(info.lpszProxy);
            if (info.lpszProxyBypass) GlobalFree(info.lpszProxyBypass);
        }
    };

    // GetPublic's check (SEC-012). The session is synchronous, so WinHTTP calls OnStatus on this
    // thread, inside WinHttpSendRequest and WinHttpReceiveResponse.
    //  - Without a proxy, the address WinHTTP really connects to:
    //    - CONNECTING/CONNECTED_TO_SERVER: a new connection, before any byte goes out (a TLS hello
    //      too), so a LAN address is refused at once instead of after the connect timeout;
    //    - SENDING_REQUEST: every request, after every redirect and on a reused keep-alive
    //      connection (WinHTTP shares those across the whole process). It reads the connection's
    //      real address, so it can't be skipped, and DNS tricks don't help.
    //  - Behind a proxy, that address is the proxy's (or, for a host on the bypass list, the
    //    target's). The target's name was looked up before sending (HostIsPublic), and REDIRECT
    //    checks each new address the same way. Accepted limit: a DNS server that answers us with a
    //    public address and the proxy with a local one.
    // Closing the request in the callback cancels it: the call returns FALSE with
    // ERROR_WINHTTP_OPERATION_CANCELLED.
    struct Guard
    {
        bool        ViaProxy = false;
        bool        Refused  = false; // OnStatus closed the request
        std::string Why;              // "local", "lookup" or "unknown"
        std::string Address;          // the refused address, "" if unknown
    };

    constexpr DWORD kDirectEvents = WINHTTP_CALLBACK_FLAG_CONNECT_TO_SERVER | WINHTTP_CALLBACK_STATUS_SENDING_REQUEST; // (a flag of its own)
    constexpr DWORD kProxyEvents  = WINHTTP_CALLBACK_FLAG_REDIRECT;

    bool CheckRedirect(const wchar_t* aUrl, DWORD aLength, Guard& g)
    {
        if (!aUrl) { g.Why = "unknown"; return false; }
        // The new URL as text; the length is in characters, maybe with the closing zero.
        const std::wstring url(aUrl, wcsnlen(aUrl, aLength ? aLength : 4096));
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        wchar_t host[256] = {};
        parts.lpszHostName = host;
        parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
        if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts) ||
            (parts.nScheme != INTERNET_SCHEME_HTTPS && parts.nScheme != INTERNET_SCHEME_HTTP))
        {
            g.Why = "unknown";
            return false;
        }
        return HostIsPublic(host, g.Why, g.Address);
    }

    void CALLBACK OnStatus(HINTERNET aRequest, DWORD_PTR aContext, DWORD aStatus, LPVOID aInfo, DWORD aInfoLength)
    {
        Guard* g = reinterpret_cast<Guard*>(aContext);
        if (!g || g->Refused) return;
        bool ok = false;
        try // never throw into WinHTTP; anything unexpected refuses
        {
            if (g->ViaProxy)
            {
                if (aStatus != WINHTTP_CALLBACK_STATUS_REDIRECT) return;
                ok = CheckRedirect(static_cast<const wchar_t*>(aInfo), aInfoLength, *g);
            }
            else if (aStatus == WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER || aStatus == WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER)
            {
                // aInfo is the server's IP address as text: "93.184.215.14" or "[2001:db8::1]".
                std::string text;
                const wchar_t* p = static_cast<const wchar_t*>(aInfo);
                for (DWORD i = 0; p && i < aInfoLength && i < 80 && p[i]; i++) text += p[i] < 0x80 ? static_cast<char>(p[i]) : '?';
                ok = NetAddress::IsPublic(text);
                if (!ok) { g->Why = "local"; g->Address = text; }
            }
            else if (aStatus == WINHTTP_CALLBACK_STATUS_SENDING_REQUEST)
            {
                WINHTTP_CONNECTION_INFO info{};
                info.cbSize = sizeof(info);
                DWORD size = sizeof(info);
                const auto* remote = reinterpret_cast<const unsigned char*>(&info.RemoteAddress);
                ok = WinHttpQueryOption(aRequest, WINHTTP_OPTION_CONNECTION_INFO, &info, &size) &&
                     NetAddress::IsPublicSockaddr(remote, sizeof(info.RemoteAddress));
                if (!ok) { g->Why = "local"; g->Address = NetAddress::SockaddrText(remote, sizeof(info.RemoteAddress)); }
            }
            else return;
        }
        catch (...) { ok = false; g->Why = "unknown"; }
        if (ok) return;
        g->Refused = true;
        WinHttpCloseHandle(aRequest);
    }

    std::string Refusal(const Guard& g)
    {
        if (g.Why == "lookup")
            return "the address's name couldn't be looked up, so it couldn't be checked and wasn't fetched "
                   "(behind a proxy, {json} needs that lookup).";
        return "the address leads to " + (g.Address.empty() ? std::string("an unknown address") : g.Address) +
               ", which is this PC or a local network: only internet addresses are allowed.";
    }
}

static Http::Response Fetch(const std::string& aUrl, int aTimeoutMs, size_t aMaxBytes, bool aPublicOnly)
{
    Http::Response r;
    const std::wstring url = Widen(aUrl);

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    wchar_t extra[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = static_cast<DWORD>(std::size(extra));
    if (url.empty() || !WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts) ||
        (parts.nScheme != INTERNET_SCHEME_HTTPS && parts.nScheme != INTERNET_SCHEME_HTTP))
    {
        r.Error = "that isn't a valid web address.";
        return r;
    }

    Guard guard; // outlives req
    const std::wstring agent = Widen(std::string("GW2-Chat-Messages/") + kVersionText + " (Nexus addon)");
    Handle session;
    if (aPublicOnly)
    {
        // Pin the proxy setting for this request: the same proxy the default would use, or none.
        const ProxyConfig proxy;
        guard.ViaProxy = proxy.Named;
        if (proxy.Named)
        {
            if (!HostIsPublic(host, guard.Why, guard.Address))
            {
                r.Error = Refusal(guard);
                return r;
            }
            session.H = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_NAMED_PROXY, proxy.Proxy.c_str(),
                                    proxy.Bypass.empty() ? WINHTTP_NO_PROXY_BYPASS : proxy.Bypass.c_str(), 0);
        }
        else
            session.H = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    else
        session.H = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.H) { r.Error = Failed("Starting the download"); return r; }
    WinHttpSetTimeouts(session.H, aTimeoutMs, aTimeoutMs, aTimeoutMs, aTimeoutMs);

    Handle connect{WinHttpConnect(session.H, host, parts.nPort, 0)};
    if (!connect.H) { r.Error = Failed("Connecting"); return r; }

    const std::wstring target = std::wstring(path) + extra;
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    Handle req{WinHttpOpenRequest(connect.H, L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, flags)};
    if (!req.H) { r.Error = Failed("Opening the request"); return r; }
    if (aPublicOnly &&
        WinHttpSetStatusCallback(req.H, OnStatus, guard.ViaProxy ? kProxyEvents : kDirectEvents, 0) == WINHTTP_INVALID_STATUS_CALLBACK)
    {
        r.Error = Failed("Opening the request");
        return r;
    }

    const DWORD_PTR context = aPublicOnly ? reinterpret_cast<DWORD_PTR>(&guard) : 0;
    const bool sent = WinHttpSendRequest(req.H, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, context) &&
                      !guard.Refused && WinHttpReceiveResponse(req.H, nullptr);
    if (guard.Refused)
    {
        req.H = nullptr; // OnStatus closed it
        r.Error = Refusal(guard);
        return r;
    }
    if (!sent)
    {
        r.Error = Failed("The request");
        return r;
    }

    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(req.H, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    r.Status = static_cast<int>(status);

    std::vector<char> chunk;
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(req.H, &available)) { r.Error = Failed("Reading the answer"); return r; }
        if (available == 0) break;
        if (r.Body.size() + available > aMaxBytes) { r.Error = "the answer is too big."; return r; }
        chunk.resize(available);
        DWORD read = 0;
        if (!WinHttpReadData(req.H, chunk.data(), available, &read)) { r.Error = Failed("Reading the answer"); return r; }
        r.Body.append(chunk.data(), read);
    }
    if (r.Status < 200 || r.Status >= 300) r.Error = "the address answered with HTTP " + std::to_string(r.Status) + ".";
    return r;
}

Http::Response Http::Get(const std::string& aUrl, int aTimeoutMs, size_t aMaxBytes)
{
    return Fetch(aUrl, aTimeoutMs, aMaxBytes, false);
}

Http::Response Http::GetPublic(const std::string& aUrl, int aTimeoutMs, size_t aMaxBytes)
{
    return Fetch(aUrl, aTimeoutMs, aMaxBytes, true);
}
