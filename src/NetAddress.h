#pragma once

// Which server addresses {json} may connect to (Http::GetPublic): a macro may come from someone
// else's import, so it must not reach this PC or the local network. No Windows, Nexus or ImGui
// dependency. Tested by tests/NetAddressTests.cpp.
//
// Not public (refused):
//  - this PC: 127.0.0.0/8, ::1, and "no address": 0.0.0.0/8, ::/96 (also old IPv4-compatible);
//  - private networks: 10/8, 172.16/12, 192.168/16, fc00::/7;
//  - link-local: 169.254/16, fe80::/10 (and the old site-local fec0::/10);
//  - the ISP's shared network (CGNAT): 100.64/10;
//  - multicast, reserved and broadcast: 224.0.0.0 and up, ff00::/8;
//  - those IPv4 addresses inside IPv6: ::ffff:a.b.c.d, NAT64 64:ff9b::/96, 6to4 2002::/16;
//  - anything that can't be read as an IP address (fail closed).

#include <cstddef>
#include <string>

namespace NetAddress
{
    // aAddress is an IPv4 or IPv6 address as text, as WinHTTP reports a new connection:
    // "203.0.113.7", "2001:db8::1", "[::1]", "fe80::1%12".
    bool IsPublic(const std::string& aAddress);

    // aData is a Windows SOCKADDR_STORAGE as bytes (WINHTTP_CONNECTION_INFO's RemoteAddress):
    // the family in the first two bytes (little-endian), then for IPv4 (AF_INET) the address at
    // byte 4, for IPv6 (AF_INET6) at byte 8. Http.cpp checks these against the real structs.
    constexpr unsigned kFamilyV4 = 2;
    constexpr unsigned kFamilyV6 = 23;
    constexpr size_t   kV4Offset = 4;
    constexpr size_t   kV6Offset = 8;
    bool IsPublicSockaddr(const unsigned char* aData, size_t aSize);

    // The same address as text for messages ("127.0.0.1", "::1", "fe80::1"), or "" if aData
    // isn't an IPv4 or IPv6 address.
    std::string SockaddrText(const unsigned char* aData, size_t aSize);
}
