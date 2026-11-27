#include "UiFont.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Globals.h"
#include "Storage.h"
#include "TextSize.h"

namespace
{
    constexpr const char* kSettingKey = "text_size";

    NexusLinkData_t*  s_Nexus   = nullptr;
    TextSize::EFont   s_Choice  = TextSize::EFont::Default;
    nlohmann::json    s_Settings = nlohmann::json::object(); // the whole file, so other keys survive a save

    float s_Scale = 1.0f; // chosen size / default size (what the layout was designed for), this frame
    std::vector<bool> s_PushStack; // per Push: whether it pushed a font (so Pop knows)

    // The fonts as Nexus has them right now. Read fresh every time: never keep an ImFont*. The
    // list is refilled in place (render thread only), so it doesn't allocate every frame; copy it
    // if something else may call this while you use it.
    const std::vector<TextSize::Font>& CurrentFonts()
    {
        using TextSize::EFont;
        static std::vector<TextSize::Font> fonts;
        fonts.clear();
        auto add = [&](EFont aKind, void* aFont) {
            const ImFont* f = static_cast<const ImFont*>(aFont);
            fonts.push_back({aKind, f, f && f->IsLoaded() ? f->FontSize : 0.0f});
        };
        // The default is what ImGui draws with when nothing is pushed (as Nexus set it up): the
        // same rule as ImGui's own GetDefaultFont(), so it's right even while another font is pushed.
        const ImGuiIO& io = ImGui::GetIO();
        ImFont* def = io.FontDefault ? io.FontDefault : (io.Fonts && io.Fonts->Fonts.Size > 0 ? io.Fonts->Fonts[0] : nullptr);
        add(EFont::Default, def);
        if (s_Nexus)
        {
            add(EFont::Nexus, s_Nexus->Font);
            add(EFont::NexusUi, s_Nexus->FontUI);
            add(EFont::NexusBig, s_Nexus->FontBig);
        }
        return fonts;
    }

    ImFont* ChosenFont()
    {
        const TextSize::Font f = TextSize::Pick(CurrentFonts(), s_Choice);
        return TextSize::Available(f) ? const_cast<ImFont*>(static_cast<const ImFont*>(f.Id)) : nullptr;
    }

    // Wide enough for the longest entry, at whatever font is drawing the combo.
    float ComboBoxWidth(const std::vector<TextSize::Font>& aChoices, const std::string& aPreview)
    {
        float widest = UiFont::TextWidth(aPreview.c_str());
        for (const TextSize::Font& c : aChoices) widest = std::max(widest, UiFont::TextWidth(TextSize::Label(c).c_str()));
        return widest + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
    }

    void Save()
    {
        s_Settings[kSettingKey] = TextSize::Key(s_Choice);
        if (!Storage::SaveSetting(kSettingKey, s_Settings[kSettingKey])) LogF(LOGL_WARNING, "The text size couldn't be saved.");
    }
}

void UiFont::Init()
{
    s_Nexus    = static_cast<NexusLinkData_t*>(g_Api->DataLink_Get(DL_NEXUS_LINK));
    s_Settings = Storage::LoadSettings();
    s_Choice   = TextSize::EFont::Default;
    const auto it = s_Settings.find(kSettingKey);
    if (it != s_Settings.end() && it->is_string()) TextSize::FromKey(it->get<std::string>(), s_Choice);
    s_PushStack.clear();
}

void UiFont::NewFrame()
{
    const std::vector<TextSize::Font>& fonts = CurrentFonts();
    const TextSize::Font def    = TextSize::Pick(fonts, TextSize::EFont::Default);
    const TextSize::Font chosen = TextSize::Pick(fonts, s_Choice);
    // While Nexus rebuilds its fonts the chosen one is briefly missing: keep the last scale, or
    // every window would shrink for those frames and grow back after.
    if (!TextSize::Available(def) || chosen.Kind != s_Choice) return;
    s_Scale = TextSize::Scale(def.Size, chosen.Size);
}

void UiFont::Push()
{
    ImFont* f = s_Choice == TextSize::EFont::Default ? nullptr : ChosenFont();
    if (f) ImGui::PushFont(f);
    s_PushStack.push_back(f != nullptr);
}

void UiFont::Pop()
{
    if (s_PushStack.empty()) return;
    if (s_PushStack.back()) ImGui::PopFont();
    s_PushStack.pop_back();
}

float UiFont::Px(float aDesignPx) { return aDesignPx * s_Scale; }
ImVec2 UiFont::Px(float aDesignW, float aDesignH) { return ImVec2(aDesignW * s_Scale, aDesignH * s_Scale); }

void UiFont::DrawCombo(const char* aLabel)
{
    const std::vector<TextSize::Font> fonts   = CurrentFonts(); // a copy: Save() and drawing may read it again
    const std::vector<TextSize::Font> choices = TextSize::Choices(fonts);
    const TextSize::Font picked = TextSize::Pick(fonts, s_Choice);
    const std::string preview = TextSize::Label(picked);

    ImGui::SetNextItemWidth(ComboBoxWidth(choices, preview));
    if (!ImGui::BeginCombo(aLabel, preview.c_str())) return;
    for (const TextSize::Font& c : choices)
    {
        const bool selected = TextSize::Shows(c, fonts, s_Choice);
        if (ImGui::Selectable(TextSize::Label(c).c_str(), selected) && !selected)
        {
            s_Choice = c.Kind;
            Save();
        }
        if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

float UiFont::ComboWidth(const char* aLabel)
{
    const std::vector<TextSize::Font>& fonts = CurrentFonts();
    const float box = ComboBoxWidth(TextSize::Choices(fonts), TextSize::Label(TextSize::Pick(fonts, s_Choice)));
    return box + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(aLabel, nullptr, true).x;
}

void UiFont::BeforeBegin(WindowFit& aFit, ImVec2 aDesignSize, ImVec2 aDesignMinSize)
{
    const ImVec2 screen = ImGui::GetIO().DisplaySize;
    ImVec2 minSize = Px(aDesignMinSize.x, aDesignMinSize.y);
    if (screen.x > 0.0f) minSize.x = std::min(minSize.x, screen.x);
    if (screen.y > 0.0f) minSize.y = std::min(minSize.y, screen.y);

    ImGui::SetNextWindowSize(Px(aDesignSize.x, aDesignSize.y), ImGuiCond_FirstUseEver);
    if (aFit.Seen && aFit.Scale > 0.0f && aFit.Scale != s_Scale)
    {
        const TextSize::Box b = TextSize::Rescale({aFit.Pos.x, aFit.Pos.y, aFit.Size.x, aFit.Size.y},
                                                  s_Scale / aFit.Scale, screen.x, screen.y);
        ImGui::SetNextWindowPos(ImVec2(b.X, b.Y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(std::max(b.W, minSize.x), std::max(b.H, minSize.y)), ImGuiCond_Always);
    }
    ImGui::SetNextWindowSizeConstraints(minSize, ImVec2(FLT_MAX, FLT_MAX));
}

void UiFont::AfterBegin(WindowFit& aFit)
{
    aFit.Seen  = true;
    aFit.Scale = s_Scale;
    aFit.Pos   = ImGui::GetWindowPos();
    aFit.Size  = ImGui::GetWindowSize();
}

UiFont::Row::Row() : m_Right(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x) {}

void UiFont::Row::Next(float aItemWidth)
{
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    if (ImGui::GetItemRectMax().x + spacing + aItemWidth <= m_Right) ImGui::SameLine();
}

float UiFont::ButtonWidth(const char* aLabel)
{
    return ImGui::CalcTextSize(aLabel, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

float UiFont::CheckboxWidth(const char* aLabel)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(aLabel, nullptr, true).x;
}

float UiFont::TextWidth(const char* aText) { return ImGui::CalcTextSize(aText).x; }

void UiFont::PushTooltipWrap() { ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f); }
