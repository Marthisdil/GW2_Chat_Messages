#include "PathText.h"

namespace
{
    constexpr char32_t kReplacement = 0xFFFD;

    void PutUtf8(std::string& aOut, char32_t aCp)
    {
        if (aCp < 0x80) aOut += static_cast<char>(aCp);
        else if (aCp < 0x800)
        {
            aOut += static_cast<char>(0xC0 | (aCp >> 6));
            aOut += static_cast<char>(0x80 | (aCp & 0x3F));
        }
        else if (aCp < 0x10000)
        {
            aOut += static_cast<char>(0xE0 | (aCp >> 12));
            aOut += static_cast<char>(0x80 | ((aCp >> 6) & 0x3F));
            aOut += static_cast<char>(0x80 | (aCp & 0x3F));
        }
        else
        {
            aOut += static_cast<char>(0xF0 | (aCp >> 18));
            aOut += static_cast<char>(0x80 | ((aCp >> 12) & 0x3F));
            aOut += static_cast<char>(0x80 | ((aCp >> 6) & 0x3F));
            aOut += static_cast<char>(0x80 | (aCp & 0x3F));
        }
    }

    void PutWide(std::wstring& aOut, char32_t aCp)
    {
        if (aCp < 0x10000) aOut += static_cast<wchar_t>(aCp);
        else
        {
            aCp -= 0x10000;
            aOut += static_cast<wchar_t>(0xD800 + (aCp >> 10));
            aOut += static_cast<wchar_t>(0xDC00 + (aCp & 0x3FF));
        }
    }
}

std::string PathText::FromWide(const std::wstring& aText)
{
    std::string out;
    out.reserve(aText.size());
    for (size_t i = 0; i < aText.size(); i++)
    {
        const char32_t c = static_cast<char16_t>(aText[i]);
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < aText.size())
        {
            const char32_t d = static_cast<char16_t>(aText[i + 1]);
            if (d >= 0xDC00 && d <= 0xDFFF)
            {
                PutUtf8(out, 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00));
                i++;
                continue;
            }
        }
        PutUtf8(out, c >= 0xD800 && c <= 0xDFFF ? kReplacement : c);
    }
    return out;
}

std::wstring PathText::ToWide(const std::string& aText)
{
    std::wstring out;
    out.reserve(aText.size());
    const size_t n = aText.size();
    size_t i = 0;
    while (i < n)
    {
        const unsigned char b = static_cast<unsigned char>(aText[i]);
        if (b < 0x80) { out += static_cast<wchar_t>(b); i++; continue; }
        int len = 0;
        char32_t cp = 0, min = 0;
        if (b >= 0xC2 && b <= 0xDF) { len = 2; cp = b & 0x1F; min = 0x80; }
        else if (b >= 0xE0 && b <= 0xEF) { len = 3; cp = b & 0x0F; min = 0x800; }
        else if (b >= 0xF0 && b <= 0xF4) { len = 4; cp = b & 0x07; min = 0x10000; }
        int k = 1;
        for (; len && k < len && i + k < n; k++)
        {
            const unsigned char cb = static_cast<unsigned char>(aText[i + k]);
            if ((cb & 0xC0) != 0x80) break;
            cp = (cp << 6) | (cb & 0x3F);
        }
        if (len == 0 || k < len || cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        {
            // One U+FFFD for the bad lead byte and the continuation bytes read with it.
            out += static_cast<wchar_t>(kReplacement);
            i += len == 0 ? 1 : static_cast<size_t>(k);
            continue;
        }
        PutWide(out, cp);
        i += static_cast<size_t>(len);
    }
    return out;
}
