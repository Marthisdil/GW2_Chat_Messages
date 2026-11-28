#include "LiteDb.h"

#include <cstdint>
#include <cstring>
#include <set>

#include "Localization.h"

// The layout, from LiteDB 5's source (LiteDB/Engine/Pages/*.cs, Structures/DataBlock.cs):
// - The file is pages of 8192 bytes. Every page starts with a 32-byte header: page id (uint32 at
//   0), page type (byte at 4: 1 header, 2 collection, 3 index, 4 data), the collection's page id
//   (uint32 at 19), and the highest slot index used (byte at 30, 255 = none).
// - Page 0 is the header page: "** This is a LiteDB file **" at 32, the file version (8) at 59,
//   and at 192 a BSON document naming each collection and its collection page.
// - A data page's items are listed in a slot table at the page's end, 4 bytes per slot from the
//   last byte backwards: length (uint16), then position (uint16). A free slot is all zeros.
// - Each item is a data block: "extend" (byte: 0 = a document's first block, 1 = a later one),
//   the next block (uint32 page id + byte slot, page 0xFFFFFFFF = none), then the document's
//   bytes. A document is its blocks' bytes put together: one BSON document.

namespace
{
    using nlohmann::json;
    using LiteDb::kPageSize;

    constexpr char   kHeaderInfo[]   = "** This is a LiteDB file **";
    constexpr size_t kHeaderInfoPos  = 32;
    constexpr size_t kFileVersionPos = 59;
    constexpr unsigned char kFileVersion = 8;
    constexpr size_t kCollectionsPos = 192;
    constexpr size_t kCollectionsLen = 8000;
    constexpr size_t kPageHeaderSize = 32;
    constexpr size_t kSlotSize       = 4;
    constexpr size_t kBlockHeader    = 6; // extend + next block
    constexpr uint32_t kNoPage       = 0xFFFFFFFFu;

    constexpr unsigned char kTypeCollection = 2;
    constexpr unsigned char kTypeData       = 4;

    uint32_t U32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
    uint16_t U16(const unsigned char* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
    int32_t  I32(const unsigned char* p) { return static_cast<int32_t>(U32(p)); }
    int64_t  I64(const unsigned char* p)
    {
        uint64_t v = 0;
        for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
        return static_cast<int64_t>(v);
    }

    // --- BSON ---

    struct Bson
    {
        const unsigned char* Data;
        size_t               Size;

        // A document or array starting at aPos, ending by aLimit. aOut gets an object (or array).
        bool Doc(size_t aPos, size_t aLimit, bool aArray, int aDepth, json& aOut, size_t& aLen) const
        {
            if (aDepth > LiteDb::kMaxDepth) return false;
            if (aPos > aLimit || aLimit - aPos < 5) return false;
            const int32_t len = I32(Data + aPos);
            if (len < 5 || static_cast<size_t>(len) > aLimit - aPos) return false;
            const size_t end = aPos + static_cast<size_t>(len) - 1; // the closing 0
            if (Data[end] != 0) return false;
            aOut = aArray ? json::array() : json::object();
            size_t o = aPos + 4;
            while (o < end)
            {
                const unsigned char type = Data[o++];
                const void* zero = std::memchr(Data + o, 0, end - o);
                if (!zero) return false;
                const size_t nameEnd = static_cast<size_t>(static_cast<const unsigned char*>(zero) - Data);
                std::string name(reinterpret_cast<const char*>(Data + o), nameEnd - o);
                o = nameEnd + 1;
                json value;
                if (!Value(type, o, end, aDepth, value)) return false;
                if (aArray) aOut.push_back(std::move(value));
                else aOut[name] = std::move(value);
            }
            aLen = static_cast<size_t>(len);
            return true;
        }

        // One value of aType at o (moved past it), ending by aEnd.
        bool Value(unsigned char aType, size_t& o, size_t aEnd, int aDepth, json& aOut) const
        {
            auto need = [&](size_t n) { return o <= aEnd && aEnd - o >= n; };
            switch (aType)
            {
            case 0x01: // double
            {
                if (!need(8)) return false;
                double d;
                std::memcpy(&d, Data + o, 8);
                o += 8;
                aOut = d;
                return true;
            }
            case 0x02: // string: int32 length (with the closing 0), bytes, 0
            {
                if (!need(4)) return false;
                const int32_t sl = I32(Data + o);
                if (sl < 1 || !need(4 + static_cast<size_t>(sl))) return false;
                if (Data[o + 4 + sl - 1] != 0) return false;
                aOut = std::string(reinterpret_cast<const char*>(Data + o + 4), static_cast<size_t>(sl) - 1);
                o += 4 + static_cast<size_t>(sl);
                return true;
            }
            case 0x03: // document
            case 0x04: // array
            {
                size_t len = 0;
                if (!Doc(o, aEnd, aType == 0x04, aDepth + 1, aOut, len)) return false;
                o += len;
                return true;
            }
            case 0x05: // binary: int32 length, subtype, bytes
            {
                if (!need(5)) return false;
                const int32_t bl = I32(Data + o);
                if (bl < 0 || !need(5 + static_cast<size_t>(bl))) return false;
                o += 5 + static_cast<size_t>(bl);
                aOut = nullptr;
                return true;
            }
            case 0x07: // ObjectId
            {
                if (!need(12)) return false;
                static const char kHex[] = "0123456789abcdef";
                std::string hex;
                for (size_t i = 0; i < 12; i++)
                {
                    hex += kHex[Data[o + i] >> 4];
                    hex += kHex[Data[o + i] & 15];
                }
                o += 12;
                aOut = json{{"$oid", hex}};
                return true;
            }
            case 0x08: // bool
                if (!need(1)) return false;
                aOut = Data[o++] != 0;
                return true;
            case 0x09: // date (ms since 1970)
            case 0x12: // int64
                if (!need(8)) return false;
                aOut = I64(Data + o);
                o += 8;
                return true;
            case 0x0A: // null
            case 0x06: // undefined (old)
            case 0x7F: // max value
            case 0xFF: // min value
                aOut = nullptr;
                return true;
            case 0x10: // int32
                if (!need(4)) return false;
                aOut = I32(Data + o);
                o += 4;
                return true;
            case 0x13: // decimal128
                if (!need(16)) return false;
                o += 16;
                aOut = nullptr;
                return true;
            default:
                return false;
            }
        }
    };

    // A collection name from the file, for a warning: printable ASCII only, at most 40 characters.
    std::string ShownName(const std::string& aName)
    {
        std::string out;
        for (char c : aName)
        {
            if (out.size() >= 40) { out += "..."; break; }
            out += (c >= 0x20 && c < 0x7F) ? c : '?';
        }
        return out;
    }

    // --- Pages ---

    struct File
    {
        const unsigned char* Data;
        size_t               Pages;

        const unsigned char* Page(size_t i) const { return Data + i * kPageSize; }

        // The item in slot aSlot of data page aPage, or false if the slot is free or damaged.
        bool Item(size_t aPage, size_t aSlot, size_t& aPos, size_t& aLen) const
        {
            const unsigned char* p = Page(aPage);
            const unsigned char highest = p[30];
            if (highest == 255 || aSlot > highest) return false;
            const size_t footer = (static_cast<size_t>(highest) + 1) * kSlotSize;
            if (footer > kPageSize - kPageHeaderSize) return false;
            const size_t slotAt = kPageSize - (aSlot + 1) * kSlotSize;
            aLen = U16(p + slotAt);
            aPos = U16(p + slotAt + 2);
            if (aPos == 0 && aLen == 0) return false; // free
            const size_t contentEnd = kPageSize - footer;
            return aPos >= kPageHeaderSize && aLen >= kBlockHeader && aPos <= contentEnd && aLen <= contentEnd - aPos;
        }
    };
}

namespace LiteDb
{
    bool ParseBson(const unsigned char* aData, size_t aSize, json& aOut)
    {
        try
        {
            Bson b{aData, aSize};
            size_t len = 0;
            return b.Doc(0, aSize, false, 0, aOut, len) && len == aSize;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    Result Read(const std::string& aBytes)
    {
        Result r;
        try
        {
            const auto* data = reinterpret_cast<const unsigned char*>(aBytes.data());
            const size_t size = aBytes.size();
            if (size > kMaxFileBytes)
            {
                r.Error = Localization::Fmt(TR("It's too big (over %zu KB)."), static_cast<size_t>(kMaxFileBytes / 1024));
                return r;
            }
            const size_t infoLen = sizeof(kHeaderInfo) - 1;
            const bool hasInfo = size >= kPageSize && std::memcmp(data + kHeaderInfoPos, kHeaderInfo, infoLen) == 0;
            if (!hasInfo)
            {
                // An encrypted LiteDB 5 file starts with 1 and a salt instead of the header page.
                r.Error = size >= 1 && data[0] == 1 ? TR("It's protected with a password, so it can't be read.")
                                                    : TR("It isn't a Blish HUD Chat Shorts database (macros.db).");
                return r;
            }
            if (data[kFileVersionPos] != kFileVersion)
            {
                r.Error = TR("It was saved by a version of Blish HUD's database that this addon can't read.");
                return r;
            }
            if (size % kPageSize != 0)
            {
                r.Error = TR("It's damaged (it's cut short).");
                return r;
            }
            const File f{data, size / kPageSize};

            // The collections: name -> collection page id.
            json cols;
            {
                Bson b{data, size};
                size_t len = 0;
                if (!b.Doc(kCollectionsPos, kCollectionsPos + kCollectionsLen, false, 0, cols, len))
                {
                    r.Error = TR("It's damaged (its list of collections can't be read).");
                    return r;
                }
            }
            std::map<uint32_t, std::string> names; // collection page id -> name
            for (auto it = cols.begin(); it != cols.end(); ++it)
            {
                if (!it.value().is_number_integer()) continue;
                const long long id = it.value().get<long long>();
                if (id <= 0 || static_cast<size_t>(id) >= f.Pages) continue;
                if (f.Page(static_cast<size_t>(id))[4] != kTypeCollection) continue;
                names[static_cast<uint32_t>(id)] = it.key();
                r.Collections[it.key()]; // listed even when empty
            }

            std::map<std::string, size_t> damaged; // documents left out, per collection
            size_t documents = 0;
            bool full = false;
            for (size_t pg = 1; pg < f.Pages && !full; pg++)
            {
                const unsigned char* p = f.Page(pg);
                if (p[4] != kTypeData) continue;
                const uint32_t col = U32(p + 19);
                auto name = names.find(col);
                if (name == names.end()) continue;
                if (U32(p) != pg) { damaged[name->second]++; continue; } // a page that isn't where it says
                const unsigned char highest = p[30];
                if (highest == 255) continue;
                for (size_t slot = 0; slot <= highest; slot++)
                {
                    size_t pos = 0, len = 0;
                    if (!f.Item(pg, slot, pos, len)) continue;
                    const unsigned char* block = p + pos;
                    if (block[0] != 0) continue; // a later block of a longer document
                    std::string doc(reinterpret_cast<const char*>(block + kBlockHeader), len - kBlockHeader);
                    uint32_t nextPage = U32(block + 1);
                    size_t   nextSlot = block[5];
                    std::set<uint64_t> seen{(static_cast<uint64_t>(pg) << 8) | slot};
                    bool ok = true;
                    while (ok && nextPage != kNoPage)
                    {
                        const uint64_t key = (static_cast<uint64_t>(nextPage) << 8) | nextSlot;
                        size_t npos = 0, nlen = 0;
                        if (nextPage >= f.Pages || !seen.insert(key).second) { ok = false; break; }
                        const unsigned char* np = f.Page(nextPage);
                        if (np[4] != kTypeData || U32(np + 19) != col || !f.Item(nextPage, nextSlot, npos, nlen)) { ok = false; break; }
                        const unsigned char* nb = np + npos;
                        if (nb[0] != 1 || doc.size() + (nlen - kBlockHeader) > kMaxDocBytes) { ok = false; break; }
                        doc.append(reinterpret_cast<const char*>(nb + kBlockHeader), nlen - kBlockHeader);
                        nextPage = U32(nb + 1);
                        nextSlot = nb[5];
                    }
                    json value;
                    if (ok && doc.size() >= 4)
                    {
                        // The document's own length; any bytes after it are unused space in the block.
                        const int32_t declared = I32(reinterpret_cast<const unsigned char*>(doc.data()));
                        ok = declared >= 5 && static_cast<size_t>(declared) <= doc.size() &&
                             ParseBson(reinterpret_cast<const unsigned char*>(doc.data()), static_cast<size_t>(declared), value) &&
                             value.is_object();
                    }
                    else ok = false;
                    if (!ok) { damaged[name->second]++; continue; }
                    if (++documents > kMaxDocuments)
                    {
                        r.Warnings.push_back(Localization::Fmt(TR("It has more than %zu entries. Only the first %zu were read."),
                                                               kMaxDocuments, kMaxDocuments));
                        full = true;
                        break;
                    }
                    r.Collections[name->second].push_back(std::move(value));
                }
            }
            for (const auto& d : damaged)
                r.Warnings.push_back(Localization::Fmt(TR("%zu damaged entries in \"%s\" were left out."), d.second,
                                                       ShownName(d.first).c_str()));
        }
        catch (const std::exception&)
        {
            r = Result{};
            r.Error = TR("It couldn't be read.");
        }
        return r;
    }
}
