#include "TextSize.h"

#include "Localization.h"

#include <algorithm>
#include <cmath>

namespace TextSize
{
    const char* Key(EFont aFont)
    {
        switch (aFont)
        {
        case EFont::Nexus:    return "nexus";
        case EFont::NexusUi:  return "nexus_ui";
        case EFont::NexusBig: return "nexus_big";
        default:              return "default";
        }
    }

    bool FromKey(const std::string& aKey, EFont& aFont)
    {
        const EFont all[] = {EFont::Default, EFont::Nexus, EFont::NexusUi, EFont::NexusBig};
        for (EFont f : all)
            if (aKey == Key(f)) { aFont = f; return true; }
        return false;
    }

    bool Available(const Font& aFont) { return aFont.Id != nullptr && aFont.Size > 0.0f; }

    namespace
    {
        // The first available font in EFont order that is the same font as aFont (itself if none).
        const Font* FirstOfSame(const std::vector<Font>& aFonts, const Font& aFont)
        {
            const Font* best = nullptr;
            for (const Font& f : aFonts)
                if (Available(f) && f.Id == aFont.Id && (!best || f.Kind < best->Kind)) best = &f;
            return best;
        }
    }

    std::vector<Font> Choices(const std::vector<Font>& aFonts)
    {
        std::vector<Font> out;
        for (const Font& f : aFonts)
        {
            if (!Available(f)) continue;
            const Font* first = FirstOfSame(aFonts, f);
            if (first && first->Kind != f.Kind) continue; // listed under the earlier kind
            bool seen = false;
            for (const Font& o : out) seen = seen || o.Kind == f.Kind;
            if (!seen) out.push_back(f);
        }
        std::stable_sort(out.begin(), out.end(), [](const Font& a, const Font& b) {
            if (a.Size != b.Size) return a.Size < b.Size;
            return a.Kind < b.Kind;
        });
        return out;
    }

    Font Pick(const std::vector<Font>& aFonts, EFont aWanted)
    {
        const Font* fallback = nullptr;
        for (const Font& f : aFonts)
        {
            if (!Available(f)) continue;
            if (f.Kind == aWanted) return f;
            if (f.Kind == EFont::Default) fallback = &f;
        }
        return fallback ? *fallback : Font{};
    }

    bool Shows(const Font& aChoice, const std::vector<Font>& aFonts, EFont aWanted)
    {
        const Font picked = Pick(aFonts, aWanted);
        return Available(picked) && Available(aChoice) && picked.Id == aChoice.Id;
    }

    std::string Label(const Font& aFont)
    {
        const char* name = TX("Default");
        switch (aFont.Kind)
        {
        case EFont::Nexus:    name = TX("Nexus font"); break;
        case EFont::NexusUi:  name = TX("Nexus UI font"); break;
        case EFont::NexusBig: name = TX("Nexus large font"); break;
        default: break;
        }
        // Render thread only (Localization): the font name is translated.
        if (aFont.Size <= 0.0f) return Localization::LookupText(name);
        return Localization::LookupText(name) + " (" + std::to_string(static_cast<int>(std::lround(aFont.Size))) + " px)";
    }

    float Scale(float aDefaultSize, float aChosenSize)
    {
        if (!(aDefaultSize > 0.0f) || !(aChosenSize > 0.0f)) return 1.0f;
        return std::clamp(aChosenSize / aDefaultSize, 0.5f, 4.0f);
    }

    Box Rescale(const Box& aWindow, float aRatio, float aScreenW, float aScreenH)
    {
        Box b = aWindow;
        if (aRatio > 0.0f)
        {
            b.W *= aRatio;
            b.H *= aRatio;
        }
        if (aScreenW > 0.0f)
        {
            b.W = std::min(b.W, aScreenW);
            if (b.X + b.W > aScreenW) b.X = std::max(0.0f, aScreenW - b.W);
        }
        if (aScreenH > 0.0f)
        {
            b.H = std::min(b.H, aScreenH);
            if (b.Y + b.H > aScreenH) b.Y = std::max(0.0f, aScreenH - b.H);
        }
        return b;
    }
}
