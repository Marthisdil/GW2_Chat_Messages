#pragma once

// A small read-only reader for LiteDB 5 data files (file version 8), the database Blish HUD's
// Chat Shorts module keeps its macros in (docs/design.md, "Import / export", "From Blish HUD").
// No Nexus or ImGui dependency (tested by tests/LiteDbTests.cpp).
//
// The file is untrusted input (docs/security-review.md): every offset, length and link in it is
// checked before it's used, a document's blocks can't loop, and the sizes are capped. Only the
// documents are read (the data pages, followed block by block); indexes, the free lists and the
// separate "-log.db" file are not. Never writes anything.

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace LiteDb
{
    constexpr size_t kPageSize     = 8192;
    constexpr size_t kMaxFileBytes = 16 * 1024 * 1024; // 2048 pages; Chat Shorts files are a few pages
    constexpr size_t kMaxDocBytes  = 1024 * 1024;      // one document, all its blocks together
    constexpr size_t kMaxDocuments = 20000;            // in the whole file
    constexpr int    kMaxDepth     = 16;               // nested documents and arrays

    // The documents of each collection, by collection name, in file order (page, then slot).
    // BSON values become JSON: an ObjectId is {"$oid": "<24 hex digits>"}, a date the number of
    // milliseconds, and binary, decimal, min and max values null.
    struct Result // NOLINT(bugprone-exception-escape): SEC-030, accepted (MSVC map move allocates)
    {
        std::map<std::string, std::vector<nlohmann::json>> Collections;
        std::string              Error;    // set if the file can't be read at all
        std::vector<std::string> Warnings; // documents that were damaged and left out

        bool Ok() const { return Error.empty(); }
    };

    // aBytes: the whole file. Never throws.
    Result Read(const std::string& aBytes);

    // One BSON document (exposed for the tests). False if it's damaged: a length or string that
    // runs past aSize, an unknown type, or nesting deeper than kMaxDepth.
    bool ParseBson(const unsigned char* aData, size_t aSize, nlohmann::json& aOut);
}
