#include "Http.h"

#include <windows.h>
#include <winhttp.h>

#include <cstddef>
#include <vector>

#include "Globals.h"
#include "Localization.h"

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
        if (e == ERROR_WINHTTP_TIMEOUT) return TR("the address didn't answer in time.");
        if (e == ERROR_WINHTTP_NAME_NOT_RESOLVED) return TR("the address couldn't be found (no internet?).");
        if (e == ERROR_WINHTTP_CANNOT_CONNECT) return TR("couldn't connect to the address.");
        if (e == ERROR_WINHTTP_SECURE_FAILURE) return TR("the secure connection failed.");
        return Localization::Fmt(TR("%s failed (error %lu)."), aWhat, static_cast<unsigned long>(e));
    }
}

Http::Response Http::Get(const std::string& aUrl, int aTimeoutMs, size_t aMaxBytes)
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
        r.Error = TR("that isn't a valid web address.");
        return r;
    }

    const std::wstring agent = Widen(std::string("GW2-Chat-Messages/") + kVersionText + " (Nexus addon)");
    Handle session{WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.H) { r.Error = Failed(TR("Starting the download")); return r; }
    WinHttpSetTimeouts(session.H, aTimeoutMs, aTimeoutMs, aTimeoutMs, aTimeoutMs);

    Handle connect{WinHttpConnect(session.H, host, parts.nPort, 0)};
    if (!connect.H) { r.Error = Failed("Connecting"); return r; }

    const std::wstring target = std::wstring(path) + extra;
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    Handle req{WinHttpOpenRequest(connect.H, L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, flags)};
    if (!req.H) { r.Error = Failed(TR("Opening the request")); return r; }
    if (!WinHttpSendRequest(req.H, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req.H, nullptr))
    {
        r.Error = Failed(TR("The request"));
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
        if (!WinHttpQueryDataAvailable(req.H, &available)) { r.Error = Failed(TR("Reading the answer")); return r; }
        if (available == 0) break;
        if (r.Body.size() + available > aMaxBytes) { r.Error = TR("the answer is too big."); return r; }
        chunk.resize(available);
        DWORD read = 0;
        if (!WinHttpReadData(req.H, chunk.data(), available, &read)) { r.Error = Failed(TR("Reading the answer")); return r; }
        r.Body.append(chunk.data(), read);
    }
    if (r.Status < 200 || r.Status >= 300) r.Error = Localization::Fmt(TR("the address answered with HTTP %d."), r.Status);
    return r;
}

