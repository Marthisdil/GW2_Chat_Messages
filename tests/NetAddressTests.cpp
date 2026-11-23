// Standalone tests for src/NetAddress.cpp: which servers {json} may connect to.

#include <initializer_list>
#include <string>
#include <vector>

#include "NetAddress.h"
#include "TestCheck.h"

using NetAddress::IsPublic;

// A SOCKADDR_STORAGE (128 bytes) as WinHTTP's CONNECTION_INFO fills it: AF_INET, port 443, a.b.c.d.
static std::vector<unsigned char> V4(int a, int b, int c, int d)
{
    std::vector<unsigned char> s(128, 0);
    s[0] = 2; s[2] = 0x01; s[3] = 0xBB;
    s[4] = static_cast<unsigned char>(a); s[5] = static_cast<unsigned char>(b);
    s[6] = static_cast<unsigned char>(c); s[7] = static_cast<unsigned char>(d);
    return s;
}

// AF_INET6 (23), port 443, the eight groups, flow info and scope id 0.
static std::vector<unsigned char> V6(std::initializer_list<unsigned> aGroups)
{
    std::vector<unsigned char> s(128, 0);
    s[0] = 23; s[2] = 0x01; s[3] = 0xBB;
    size_t i = 8;
    for (unsigned g : aGroups) { s[i++] = static_cast<unsigned char>(g >> 8); s[i++] = static_cast<unsigned char>(g & 0xFF); }
    return s;
}

static bool Pub(const std::vector<unsigned char>& s) { return NetAddress::IsPublicSockaddr(s.data(), s.size()); }
static std::string Text(const std::vector<unsigned char>& s) { return NetAddress::SockaddrText(s.data(), s.size()); }

static void TestPublic()
{
    CHECK(IsPublic("8.8.8.8"));
    CHECK(IsPublic("104.18.2.3"));
    CHECK(IsPublic("1.0.0.1"));
    CHECK(IsPublic("100.63.255.255"));                 // just below CGNAT
    CHECK(IsPublic("100.128.0.0"));                    // just above CGNAT
    CHECK(IsPublic("172.15.255.255"));                 // just below 172.16/12
    CHECK(IsPublic("172.32.0.0"));                     // just above it
    CHECK(IsPublic("169.253.1.1"));
    CHECK(IsPublic("192.169.0.1"));
    CHECK(IsPublic("223.255.255.255"));                // last before multicast
    CHECK(IsPublic("2001:4860:4860::8888"));
    CHECK(IsPublic("[2606:4700:4700::1111]"));         // as WinHTTP writes IPv6
    CHECK(IsPublic("2a00:1450:4001:82b:0:0:0:200e"));  // all eight groups
    CHECK(IsPublic("::ffff:8.8.8.8"));                 // IPv4 inside IPv6
    CHECK(IsPublic("::FFFF:808:808"));                 // the same in hex
    CHECK(IsPublic("64:ff9b::8.8.8.8"));               // NAT64 of a public address
    CHECK(IsPublic("2002:808:808::1"));                // 6to4 of a public address
    CHECK(IsPublic("fe00::1"));                        // below fe80::/10
}

static void TestThisPc()
{
    CHECK(!IsPublic("127.0.0.1"));
    CHECK(!IsPublic("127.255.255.254"));
    CHECK(!IsPublic("0.0.0.0"));
    CHECK(!IsPublic("0.1.2.3"));
    CHECK(!IsPublic("::1"));
    CHECK(!IsPublic("[::1]"));
    CHECK(!IsPublic("0:0:0:0:0:0:0:1"));
    CHECK(!IsPublic("::"));
    CHECK(!IsPublic("::127.0.0.1"));                   // old IPv4-compatible form
    CHECK(!IsPublic("::ffff:127.0.0.1"));
    CHECK(!IsPublic("[::ffff:7f00:1]"));
    CHECK(!IsPublic("64:ff9b::127.0.0.1"));
    CHECK(!IsPublic("2002:7f00:1::"));
}

static void TestLocalNetworks()
{
    CHECK(!IsPublic("10.0.0.1"));
    CHECK(!IsPublic("10.255.255.255"));
    CHECK(!IsPublic("172.16.0.1"));
    CHECK(!IsPublic("172.31.255.255"));
    CHECK(!IsPublic("192.168.1.1"));
    CHECK(!IsPublic("169.254.169.254"));               // link-local (cloud metadata too)
    CHECK(!IsPublic("100.64.0.1"));                    // CGNAT
    CHECK(!IsPublic("100.127.255.255"));
    CHECK(!IsPublic("224.0.0.1"));                     // multicast
    CHECK(!IsPublic("239.255.255.250"));
    CHECK(!IsPublic("240.0.0.1"));                     // reserved
    CHECK(!IsPublic("255.255.255.255"));               // broadcast
    CHECK(!IsPublic("fc00::1"));                       // unique local
    CHECK(!IsPublic("fd12:3456:789a::1"));
    CHECK(!IsPublic("fe80::1"));                       // link-local
    CHECK(!IsPublic("fe80::1%12"));                    // with a zone
    CHECK(!IsPublic("[fe80::abcd%eth0]"));
    CHECK(!IsPublic("febf::1"));
    CHECK(!IsPublic("fec0::1"));                       // old site-local
    CHECK(!IsPublic("ff02::1"));                       // multicast
    CHECK(!IsPublic("::ffff:192.168.0.10"));
    CHECK(!IsPublic("::ffff:a00:1"));                  // 10.0.0.1 in hex
    CHECK(!IsPublic("64:ff9b::10.0.0.1"));
    CHECK(!IsPublic("2002:c0a8:101::1"));              // 6to4 of 192.168.1.1
}

static void TestNotAnAddress()
{
    // Anything that isn't a plain IP address is refused.
    CHECK(!IsPublic(""));
    CHECK(!IsPublic("localhost"));
    CHECK(!IsPublic("example.com"));
    CHECK(!IsPublic("1.2.3"));
    CHECK(!IsPublic("1.2.3.4.5"));
    CHECK(!IsPublic("256.1.1.1"));
    CHECK(!IsPublic("1.2.3.1000"));
    CHECK(!IsPublic("08.8.8.8"));                      // leading zero: octal elsewhere
    CHECK(!IsPublic("8.8.8.8 "));
    CHECK(!IsPublic(" 8.8.8.8"));
    CHECK(!IsPublic("8.8.8.8:80"));
    CHECK(!IsPublic("[8.8.8.8]"));
    CHECK(!IsPublic("8.8.8.8%1"));
    CHECK(!IsPublic("0x8.8.8.8"));
    CHECK(!IsPublic("134744072"));                     // 8.8.8.8 as one number
    CHECK(!IsPublic("[2001:4860::8888"));
    CHECK(!IsPublic("2001:4860::8888]"));
    CHECK(!IsPublic(":::"));
    CHECK(!IsPublic("2001::4860::8888"));              // two "::"
    CHECK(!IsPublic(":2001:4860::8888"));
    CHECK(!IsPublic("2001:4860::8888:"));
    CHECK(!IsPublic("2001:4860:0:0:0:0:0:8888:1"));    // nine groups
    CHECK(!IsPublic("2001:4860:0:0:0:0:8888"));        // seven groups, no "::"
    CHECK(!IsPublic("2001:48600::8888"));              // a group over four digits
    CHECK(!IsPublic("2001:4g60::8888"));
    CHECK(!IsPublic("::ffff:8.8.8.8:1"));              // IPv4 must be last
    CHECK(!IsPublic("1:2:3:4:5:6:7:8.8.8.8"));         // too long with the IPv4 part
    CHECK(!IsPublic(std::string(65, '1')));
}

// The connection's real address, as checked before every request (reused connections too).
static void TestSockaddr()
{
    CHECK(Pub(V4(8, 8, 8, 8)));
    CHECK(Pub(V4(104, 18, 2, 3)));
    CHECK(!Pub(V4(127, 0, 0, 1)));
    CHECK(!Pub(V4(0, 0, 0, 0)));
    CHECK(!Pub(V4(10, 1, 2, 3)));
    CHECK(!Pub(V4(172, 20, 0, 1)));
    CHECK(!Pub(V4(192, 168, 1, 1)));
    CHECK(!Pub(V4(169, 254, 169, 254)));
    CHECK(!Pub(V4(100, 64, 0, 1)));
    CHECK(!Pub(V4(224, 0, 0, 251)));
    CHECK(!Pub(V4(255, 255, 255, 255)));
    CHECK(Pub(V6({0x2001, 0x4860, 0x4860, 0, 0, 0, 0, 0x8888})));
    CHECK(!Pub(V6({0, 0, 0, 0, 0, 0, 0, 1})));                  // ::1
    CHECK(!Pub(V6({0, 0, 0, 0, 0, 0, 0, 0})));                  // ::
    CHECK(!Pub(V6({0, 0, 0, 0, 0, 0xFFFF, 0x7F00, 0x0001})));   // ::ffff:127.0.0.1
    CHECK(!Pub(V6({0, 0, 0, 0, 0, 0xFFFF, 0xC0A8, 0x0101})));   // ::ffff:192.168.1.1
    CHECK(Pub(V6({0, 0, 0, 0, 0, 0xFFFF, 0x0808, 0x0808})));    // ::ffff:8.8.8.8
    CHECK(!Pub(V6({0xFE80, 0, 0, 0, 0, 0, 0, 1})));
    CHECK(!Pub(V6({0xFD00, 0, 0, 0, 0, 0, 0, 1})));
    CHECK(!Pub(V6({0xFF02, 0, 0, 0, 0, 0, 0, 1})));
    CHECK(!Pub(V6({0x0064, 0xFF9B, 0, 0, 0, 0, 0x0A00, 0x0001}))); // NAT64 of 10.0.0.1

    // Fail closed: no address (a failed query leaves it zeroed), another family, too short.
    std::vector<unsigned char> zero(128, 0);
    CHECK(!Pub(zero));
    std::vector<unsigned char> other = V4(8, 8, 8, 8);
    other[0] = 1;
    CHECK(!Pub(other));
    other = V4(8, 8, 8, 8);
    other[1] = 2;                                                // family 0x0202
    CHECK(!Pub(other));
    CHECK(!NetAddress::IsPublicSockaddr(V4(8, 8, 8, 8).data(), 7));
    CHECK(!NetAddress::IsPublicSockaddr(V6({0x2001, 0x4860, 0x4860, 0, 0, 0, 0, 0x8888}).data(), 23));
    CHECK(!NetAddress::IsPublicSockaddr(nullptr, 128));
}

static void TestSockaddrText()
{
    CHECK(Text(V4(127, 0, 0, 1)) == "127.0.0.1");
    CHECK(Text(V4(192, 168, 10, 254)) == "192.168.10.254");
    CHECK(Text(V6({0, 0, 0, 0, 0, 0, 0, 1})) == "::1");
    CHECK(Text(V6({0, 0, 0, 0, 0, 0, 0, 0})) == "::");
    CHECK(Text(V6({0xFE80, 0, 0, 0, 0, 0, 0, 1})) == "fe80::1");
    CHECK(Text(V6({1, 0, 0, 0, 0, 0, 0, 0})) == "1::");
    CHECK(Text(V6({1, 0, 0, 2, 0, 0, 0, 3})) == "1:0:0:2::3");      // the longest run
    CHECK(Text(V6({1, 0, 0, 2, 0, 0, 3, 4})) == "1::2:0:0:3:4");    // the first of equal runs
    CHECK(Text(V6({1, 0, 2, 3, 4, 5, 6, 7})) == "1:0:2:3:4:5:6:7"); // one zero isn't shortened
    CHECK(Text(V6({0, 0, 0, 0, 0, 0xFFFF, 0x7F00, 1})) == "::ffff:7f00:1");
    CHECK(Text(V6({0x2001, 0xDB8, 0x85A3, 0, 0, 0x8A2E, 0x370, 0x7334})) == "2001:db8:85a3::8a2e:370:7334");
    std::vector<unsigned char> zero(128, 0);
    CHECK(Text(zero).empty());
    CHECK(NetAddress::SockaddrText(V4(1, 2, 3, 4).data(), 7).empty());
}

void RunNetAddressTests()
{
    TestPublic();
    TestThisPc();
    TestLocalNetworks();
    TestNotAnAddress();
    TestSockaddr();
    TestSockaddrText();
}
