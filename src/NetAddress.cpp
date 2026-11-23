#include "NetAddress.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
    int Digit(char c) { return c >= '0' && c <= '9' ? c - '0' : -1; }

    int HexDigit(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    // Exactly four decimal parts of 0-255. No leading zeros ("010" could mean octal elsewhere).
    bool ParseV4(const std::string& s, uint8_t aOut[4])
    {
        size_t i = 0;
        for (int part = 0; part < 4; part++)
        {
            if (part > 0)
            {
                if (i >= s.size() || s[i] != '.') return false;
                i++;
            }
            const size_t start = i;
            int v = 0;
            while (i < s.size() && i - start < 3 && Digit(s[i]) >= 0) v = v * 10 + Digit(s[i++]);
            if (i == start || v > 255 || (i - start > 1 && s[start] == '0')) return false;
            aOut[part] = static_cast<uint8_t>(v);
        }
        return i == s.size();
    }

    // Hex groups with at most one "::", optionally ending in an IPv4 address.
    bool ParseV6(const std::string& s, uint8_t aOut[16])
    {
        uint16_t groups[8] = {};
        int count = 0, gapAt = -1; // gapAt: how many groups came before the "::"
        size_t i = 0;
        if (s.compare(0, 2, "::") == 0) { gapAt = 0; i = 2; }
        else if (!s.empty() && s[0] == ':') return false;
        while (i < s.size())
        {
            size_t end = s.find(':', i);
            if (end == std::string::npos) end = s.size();
            const std::string group = s.substr(i, end - i);
            if (group.find('.') != std::string::npos)
            {
                uint8_t v4[4];
                if (end != s.size() || count > 6 || !ParseV4(group, v4)) return false; // only last
                groups[count++] = static_cast<uint16_t>(v4[0] << 8 | v4[1]);
                groups[count++] = static_cast<uint16_t>(v4[2] << 8 | v4[3]);
                break;
            }
            if (group.empty() || group.size() > 4 || count >= 8) return false;
            int v = 0;
            for (char c : group)
            {
                const int d = HexDigit(c);
                if (d < 0) return false;
                v = v * 16 + d;
            }
            groups[count++] = static_cast<uint16_t>(v);
            i = end;
            if (i < s.size()) // at a ':'
            {
                if (i + 1 < s.size() && s[i + 1] == ':')
                {
                    if (gapAt >= 0) return false; // a second "::"
                    gapAt = count;
                    i += 2;
                }
                else if (++i == s.size()) return false; // ends in a single ':'
            }
        }
        if (gapAt < 0 ? count != 8 : count > 7) return false;

        uint16_t full[8] = {};
        const int tail = gapAt < 0 ? 0 : count - gapAt;
        for (int g = 0; g < count - tail; g++) full[g] = groups[g];
        for (int g = 0; g < tail; g++) full[8 - tail + g] = groups[count - tail + g];
        for (int g = 0; g < 8; g++)
        {
            aOut[g * 2] = static_cast<uint8_t>(full[g] >> 8);
            aOut[g * 2 + 1] = static_cast<uint8_t>(full[g] & 0xFF);
        }
        return true;
    }

    bool PublicV4(const uint8_t* a)
    {
        if (a[0] == 0) return false;                          // 0.0.0.0/8: "this network"
        if (a[0] == 10) return false;                         // 10.0.0.0/8: private
        if (a[0] == 100 && (a[1] & 0xC0) == 64) return false; // 100.64.0.0/10: CGNAT
        if (a[0] == 127) return false;                        // 127.0.0.0/8: this PC
        if (a[0] == 169 && a[1] == 254) return false;         // 169.254.0.0/16: link-local
        if (a[0] == 172 && (a[1] & 0xF0) == 16) return false; // 172.16.0.0/12: private
        if (a[0] == 192 && a[1] == 168) return false;         // 192.168.0.0/16: private
        if (a[0] >= 224) return false;                        // multicast, reserved, broadcast
        return true;
    }

    bool PublicV6(const uint8_t* b)
    {
        static const uint8_t kMapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
        static const uint8_t kZero[12] = {};
        static const uint8_t kNat64[12] = {0x00, 0x64, 0xFF, 0x9B, 0, 0, 0, 0, 0, 0, 0, 0};
        if (std::memcmp(b, kMapped, 12) == 0) return PublicV4(b + 12); // ::ffff:a.b.c.d
        if (std::memcmp(b, kZero, 12) == 0) return false;              // ::, ::1, ::a.b.c.d
        if (std::memcmp(b, kNat64, 12) == 0) return PublicV4(b + 12);  // 64:ff9b::a.b.c.d
        if (b[0] == 0x20 && b[1] == 0x02) return PublicV4(b + 2);      // 2002:aabb:ccdd::/48 (6to4)
        if ((b[0] & 0xFE) == 0xFC) return false;                       // fc00::/7: private
        if (b[0] == 0xFE && (b[1] & 0x80) == 0x80) return false;       // fe80::/10, fec0::/10
        if (b[0] == 0xFF) return false;                                // ff00::/8: multicast
        return true;
    }

    // The family of a SOCKADDR_STORAGE given as bytes, or 0 if it's too short for its address.
    unsigned Family(const unsigned char* aData, size_t aSize)
    {
        if (!aData || aSize < 2) return 0;
        const unsigned family = aData[0] | aData[1] << 8; // little-endian, as on every Windows
        if (family == NetAddress::kFamilyV4 && aSize >= NetAddress::kV4Offset + 4) return family;
        if (family == NetAddress::kFamilyV6 && aSize >= NetAddress::kV6Offset + 16) return family;
        return 0;
    }
}

bool NetAddress::IsPublicSockaddr(const unsigned char* aData, size_t aSize)
{
    const unsigned family = Family(aData, aSize);
    if (family == kFamilyV4) return PublicV4(aData + kV4Offset);
    if (family == kFamilyV6) return PublicV6(aData + kV6Offset);
    return false;
}

std::string NetAddress::SockaddrText(const unsigned char* aData, size_t aSize)
{
    const unsigned family = Family(aData, aSize);
    char buf[8];
    std::string out;
    if (family == kFamilyV4)
    {
        const unsigned char* a = aData + kV4Offset;
        for (int i = 0; i < 4; i++) out += (i ? "." : "") + std::to_string(a[i]);
        return out;
    }
    if (family != kFamilyV6) return out;

    const unsigned char* b = aData + kV6Offset;
    unsigned groups[8];
    for (int i = 0; i < 8; i++) groups[i] = b[i * 2] << 8 | b[i * 2 + 1];
    int gapAt = -1, gapLength = 1; // the first longest run of two or more zero groups becomes "::"
    for (int i = 0; i < 8;)
    {
        int end = i;
        while (end < 8 && groups[end] == 0) end++;
        if (end - i > gapLength) { gapAt = i; gapLength = end - i; }
        i = end > i ? end : i + 1;
    }
    for (int i = 0; i < 8; i++)
    {
        if (i == gapAt)
        {
            out += "::";
            i += gapLength - 1;
            continue;
        }
        if (!out.empty() && out.back() != ':') out += ':';
        std::snprintf(buf, sizeof(buf), "%x", groups[i]);
        out += buf;
    }
    return out;
}

bool NetAddress::IsPublic(const std::string& aAddress)
{
    if (aAddress.empty() || aAddress.size() > 64) return false;
    std::string s = aAddress;
    if (s.front() == '[')
    {
        if (s.size() < 2 || s.back() != ']') return false;
        s = s.substr(1, s.size() - 2);
    }
    if (s.find(':') == std::string::npos)
    {
        uint8_t v4[4];
        return s.size() == aAddress.size() && ParseV4(s, v4) && PublicV4(v4); // no "[1.2.3.4]"
    }
    const size_t zone = s.find('%'); // "fe80::1%12": the zone doesn't change the range
    if (zone != std::string::npos) s.resize(zone);
    uint8_t v6[16];
    return ParseV6(s, v6) && PublicV6(v6);
}
