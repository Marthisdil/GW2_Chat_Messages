// Tests for src/LiteDb.*: reading LiteDB 5 files (Blish HUD Chat Shorts' macros.db), and that a
// damaged or crafted file never crashes the reader (docs/security-review.md, untrusted input).

#include "TestCheck.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include "LiteDb.h"

using nlohmann::json;

namespace
{
    constexpr size_t P = LiteDb::kPageSize;

    void Put32(std::string& s, size_t at, uint32_t v)
    {
        for (int i = 0; i < 4; i++) s[at + i] = static_cast<char>((v >> (8 * i)) & 0xFF);
    }
    void Put16(std::string& s, size_t at, uint16_t v)
    {
        s[at] = static_cast<char>(v & 0xFF);
        s[at + 1] = static_cast<char>(v >> 8);
    }
    void Append32(std::string& s, uint32_t v)
    {
        for (int i = 0; i < 4; i++) s += static_cast<char>((v >> (8 * i)) & 0xFF);
    }

    // A small BSON encoder for test documents: strings, integers, booleans, null, objects,
    // arrays, and {"$oid": "<24 hex>"} as an ObjectId.
    std::string Bson(const json& aDoc);
    void BsonElement(std::string& out, const std::string& aName, const json& v)
    {
        auto head = [&](char t) { out += t; out += aName; out += '\0'; };
        if (v.is_object() && v.size() == 1 && v.contains("$oid"))
        {
            head(0x07);
            const std::string hex = v["$oid"].get<std::string>();
            for (size_t i = 0; i < 12; i++) out += static_cast<char>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
        }
        else if (v.is_object()) { head(0x03); out += Bson(v); }
        else if (v.is_array())
        {
            head(0x04);
            std::string body; // keys "0", "1", ... in index order
            for (size_t i = 0; i < v.size(); i++) BsonElement(body, std::to_string(i), v[i]);
            std::string doc;
            Append32(doc, static_cast<uint32_t>(body.size() + 5));
            doc += body;
            doc += '\0';
            out += doc;
        }
        else if (v.is_string())
        {
            head(0x02);
            const std::string s = v.get<std::string>();
            Append32(out, static_cast<uint32_t>(s.size() + 1));
            out += s;
            out += '\0';
        }
        else if (v.is_boolean()) { head(0x08); out += v.get<bool>() ? '\1' : '\0'; }
        else if (v.is_number_integer()) { head(0x10); Append32(out, static_cast<uint32_t>(v.get<int32_t>())); }
        else { head(0x0A); }
    }
    std::string Bson(const json& aDoc)
    {
        std::string body;
        for (auto it = aDoc.begin(); it != aDoc.end(); ++it) BsonElement(body, it.key(), it.value());
        std::string doc;
        Append32(doc, static_cast<uint32_t>(body.size() + 5));
        doc += body;
        doc += '\0';
        return doc;
    }

    // Builds a LiteDB 5 file the way LiteDB lays it out: page 0 the header, then for each
    // collection a collection page followed by data pages. A document longer than aBlock bytes
    // is split into blocks, each in its own slot, chained with "next block" links; blocks go
    // into the current data page while they fit.
    struct Builder
    {
        std::string File;
        size_t      Block = 4000;

        size_t NewPage(unsigned char aType, uint32_t aCol)
        {
            const size_t id = File.size() / P;
            File.append(P, '\0');
            const size_t b = id * P;
            Put32(File, b, static_cast<uint32_t>(id));
            File[b + 4] = static_cast<char>(aType);
            Put32(File, b + 5, 0xFFFFFFFF);
            Put32(File, b + 9, 0xFFFFFFFF);
            Put32(File, b + 19, aCol);
            File[b + 30] = static_cast<char>(0xFF); // no slots yet
            Put16(File, b + 28, 32);                // next free position
            return id;
        }

        // Adds an item to a data page; false if it doesn't fit.
        bool AddItem(size_t aPage, const std::string& aItem, unsigned char& aSlot)
        {
            const size_t b = aPage * P;
            const unsigned char highest = static_cast<unsigned char>(File[b + 30]);
            const size_t slots = highest == 0xFF ? 0 : highest + 1u;
            const size_t free = static_cast<unsigned char>(File[b + 28]) | (static_cast<unsigned char>(File[b + 29]) << 8);
            if (free + aItem.size() > P - (slots + 1) * 4) return false;
            File.replace(b + free, aItem.size(), aItem);
            aSlot = static_cast<unsigned char>(slots);
            const size_t slotAt = b + P - (slots + 1) * 4;
            Put16(File, slotAt, static_cast<uint16_t>(aItem.size()));
            Put16(File, slotAt + 2, static_cast<uint16_t>(free));
            File[b + 30] = static_cast<char>(aSlot);
            File[b + 23] = static_cast<char>(slots + 1);
            Put16(File, b + 28, static_cast<uint16_t>(free + aItem.size()));
            return true;
        }

        void Build(const std::vector<std::pair<std::string, std::vector<json>>>& aCollections)
        {
            File.clear();
            NewPage(1, 0xFFFFFFFF);
            const char info[] = "** This is a LiteDB file **";
            File.replace(32, sizeof(info) - 1, info);
            File[59] = 8;
            json cols = json::object();
            for (const auto& c : aCollections)
            {
                const size_t colPage = NewPage(2, 0);
                Put32(File, colPage * P + 19, static_cast<uint32_t>(colPage));
                cols[c.first] = static_cast<int32_t>(colPage);
                size_t page = NewPage(4, static_cast<uint32_t>(colPage));
                for (const json& d : c.second)
                {
                    const std::string bytes = Bson(d);
                    // Blocks from the end, so each knows the next one's address.
                    std::vector<std::string> parts;
                    for (size_t at = 0; at < bytes.size(); at += Block) parts.push_back(bytes.substr(at, Block));
                    std::vector<std::pair<size_t, unsigned char>> where(parts.size());
                    for (size_t i = 0; i < parts.size(); i++)
                    {
                        // reserve: write later with the right links
                        std::string item(6 + parts[i].size(), '\0');
                        unsigned char slot = 0;
                        if (!AddItem(page, item, slot))
                        {
                            page = NewPage(4, static_cast<uint32_t>(colPage));
                            AddItem(page, item, slot);
                        }
                        where[i] = {page, slot};
                    }
                    for (size_t i = 0; i < parts.size(); i++)
                    {
                        const size_t b = where[i].first * P;
                        const size_t slotAt = b + P - (where[i].second + 1u) * 4;
                        const size_t pos = static_cast<unsigned char>(File[slotAt + 2]) | (static_cast<unsigned char>(File[slotAt + 3]) << 8);
                        File[b + pos] = i == 0 ? '\0' : '\1';
                        if (i + 1 < parts.size())
                        {
                            Put32(File, b + pos + 1, static_cast<uint32_t>(where[i + 1].first));
                            File[b + pos + 5] = static_cast<char>(where[i + 1].second);
                        }
                        else
                        {
                            Put32(File, b + pos + 1, 0xFFFFFFFF);
                            File[b + pos + 5] = '\0';
                        }
                        File.replace(b + pos + 6, parts[i].size(), parts[i]);
                    }
                }
            }
            const std::string colDoc = Bson(cols);
            File.replace(192, colDoc.size(), colDoc);
        }
    };

    std::string ReadSample(const char* aPath)
    {
        std::ifstream in(aPath, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    void TestBson()
    {
        json doc = {{"s", "hi"}, {"n", 5}, {"b", true}, {"z", nullptr}, {"a", json::array({1, "x"})},
                    {"o", {{"k", "v"}}}, {"id", {{"$oid", "6a637797c937df0352c60a94"}}}};
        const std::string b = Bson(doc);
        json out;
        CHECK(LiteDb::ParseBson(reinterpret_cast<const unsigned char*>(b.data()), b.size(), out));
        CHECK(out["s"] == "hi");
        CHECK(out["n"] == 5);
        CHECK(out["b"] == true);
        CHECK(out["z"].is_null());
        CHECK(out["a"] == json::array({1, "x"}));
        CHECK(out["o"]["k"] == "v");
        CHECK(out["id"]["$oid"] == "6a637797c937df0352c60a94");

        // Every cut and every one-byte change is either read or refused, never a crash.
        for (size_t n = 0; n < b.size(); n++)
        {
            json o;
            CHECK(!LiteDb::ParseBson(reinterpret_cast<const unsigned char*>(b.data()), n, o));
        }
        std::mt19937 rng(7); // NOLINT(bugprone-random-generator-seed): a fixed seed, so a failure can be repeated
        for (int i = 0; i < 3000; i++)
        {
            std::string m = b;
            m[rng() % m.size()] = static_cast<char>(rng() & 0xFF);
            json o;
            LiteDb::ParseBson(reinterpret_cast<const unsigned char*>(m.data()), m.size(), o);
        }

        // Nesting deeper than kMaxDepth is refused.
        json deep = "x";
        for (int i = 0; i < LiteDb::kMaxDepth + 2; i++) deep = json{{"d", deep}};
        const std::string db = Bson(deep);
        json o;
        CHECK(!LiteDb::ParseBson(reinterpret_cast<const unsigned char*>(db.data()), db.size(), o));
        // A string whose length runs past the document.
        std::string bad = Bson(json{{"s", "abc"}});
        Put32(bad, 4 + 1 + 2, 1000);
        CHECK(!LiteDb::ParseBson(reinterpret_cast<const unsigned char*>(bad.data()), bad.size(), o));
    }

    void TestBuiltFile()
    {
        Builder bld;
        std::vector<json> docs;
        for (int i = 0; i < 30; i++) docs.push_back(json{{"_id", i}, {"message", std::string(static_cast<size_t>(100 + i * 50), static_cast<char>('a' + i % 26))}});
        docs.push_back(json{{"_id", 99}, {"message", std::string(20000, 'L')}}); // over two pages: 5 blocks
        bld.Build({{"things", docs}, {"empty", {}}});
        LiteDb::Result r = LiteDb::Read(bld.File);
        CHECK(r.Ok());
        CHECK(r.Warnings.empty());
        CHECK(r.Collections.count("empty") == 1 && r.Collections["empty"].empty());
        CHECK(r.Collections["things"].size() == 31);
        bool sawLong = false;
        for (const json& d : r.Collections["things"])
            if (d["_id"] == 99) sawLong = d["message"].get<std::string>() == std::string(20000, 'L');
        CHECK(sawLong);

        // Not a LiteDB file, a password-protected one, another version, cut short.
        CHECK(!LiteDb::Read("").Ok());
        CHECK(!LiteDb::Read(std::string(P, 'x')).Ok());
        std::string enc(P * 2, '\0');
        enc[0] = 1;
        CHECK(!LiteDb::Read(enc).Ok());
        std::string ver = bld.File;
        ver[59] = 7;
        CHECK(!LiteDb::Read(ver).Ok());
        CHECK(!LiteDb::Read(bld.File.substr(0, bld.File.size() - 1)).Ok());
        CHECK(!LiteDb::Read(std::string(LiteDb::kMaxFileBytes + P, '\0')).Ok());

        // A block chain that loops back on itself: that document is left out with a warning.
        {
            Builder b2;
            b2.Block = 100;
            b2.Build({{"c", {json{{"m", std::string(300, 'q')}}}}});
            // The first data page is page 2; its slot 0 is the first block. Point the last block
            // (slot 3) back at slot 1.
            std::string f = b2.File;
            const size_t base = 2 * P;
            const size_t slotAt = base + P - 4 * 4;
            const size_t pos = static_cast<unsigned char>(f[slotAt + 2]) | (static_cast<unsigned char>(f[slotAt + 3]) << 8);
            Put32(f, base + pos + 1, 2);
            f[base + pos + 5] = 1;
            LiteDb::Result lr = LiteDb::Read(f);
            CHECK(lr.Ok());
            CHECK(lr.Collections["c"].empty());
            CHECK(lr.Warnings.size() == 1);
        }

        // A slot that points outside the page is skipped.
        {
            std::string f = bld.File;
            const size_t base = 2 * P;
            Put16(f, base + P - 2, 8000); // slot 0's position
            LiteDb::Result lr = LiteDb::Read(f);
            CHECK(lr.Ok());
            CHECK(lr.Collections["things"].size() == 30);
        }

        // Random damage anywhere in the file: never a crash.
        std::mt19937 rng(11); // NOLINT(bugprone-random-generator-seed): a fixed seed, so a failure can be repeated
        for (int i = 0; i < 2000; i++)
        {
            std::string f = bld.File;
            for (int k = 0; k < 4; k++) f[rng() % f.size()] = static_cast<char>(rng() & 0xFF);
            LiteDb::Read(f);
        }
    }

    // A real Chat Shorts file (its text replaced by filler, same layout as LiteDB wrote it).
    void TestSample()
    {
        const std::string f = ReadSample("tests\\samples\\chatshorts_macros.db");
        CHECK(f.size() == 8 * P);
        LiteDb::Result r = LiteDb::Read(f);
        CHECK(r.Ok());
        CHECK(r.Warnings.empty());
        CHECK(r.Collections["chat_macros"].size() == 11);
        CHECK(r.Collections["chat_lines"].size() == 48);
        if (r.Collections["chat_macros"].empty()) return;
        const json& first = r.Collections["chat_macros"][0];
        CHECK(first["_id"]["$oid"] == "6a637797c937df0352c60a94");
        CHECK(first["game_mode"] == "PvE, WvW, PvP");
        CHECK(first["key_binding"] == "{}");
        CHECK(first["lines"][0]["$ref"] == "chat_lines");

        std::mt19937 rng(3); // NOLINT(bugprone-random-generator-seed): a fixed seed, so a failure can be repeated
        for (int i = 0; i < 2000; i++)
        {
            std::string m = f;
            for (int k = 0; k < 3; k++) m[rng() % m.size()] = static_cast<char>(rng() & 0xFF);
            LiteDb::Read(m);
        }
    }
}

void RunLiteDbTests()
{
    TestBson();
    TestBuiltFile();
    TestSample();
}
