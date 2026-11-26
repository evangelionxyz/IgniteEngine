// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_theme.hpp"
#include "icons_lucide.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/vfs/vfs.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/imgui/imgui_nri.hpp"

#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ignite::UI
{
    namespace
    {
        constexpr ImWchar kLucideGlyphRanges[] = { ICON_MIN_LC, ICON_MAX_16_LC, 0 };

        std::unordered_map<ImGuiID, float> &GetAnimMap()
        {
            static std::unordered_map<ImGuiID, float> s_Animations;
            return s_Animations;
        }

        ImGuiCol ToImGuiCol(EColorStyle style)
        {
            switch (style)
            {
                case EColorStyle::Text:                      return ImGuiCol_Text;
                case EColorStyle::TextDisabled:              return ImGuiCol_TextDisabled;
                case EColorStyle::WindowBg:                  return ImGuiCol_WindowBg;
                case EColorStyle::ChildBg:                   return ImGuiCol_ChildBg;
                case EColorStyle::PopupBg:                   return ImGuiCol_PopupBg;
                case EColorStyle::Border:                    return ImGuiCol_Border;
                case EColorStyle::BorderShadow:              return ImGuiCol_BorderShadow;
                case EColorStyle::FrameBg:                   return ImGuiCol_FrameBg;
                case EColorStyle::FrameBgHovered:            return ImGuiCol_FrameBgHovered;
                case EColorStyle::FrameBgActive:             return ImGuiCol_FrameBgActive;
                case EColorStyle::TitleBg:                   return ImGuiCol_TitleBg;
                case EColorStyle::TitleBgActive:             return ImGuiCol_TitleBgActive;
                case EColorStyle::TitleBgCollapsed:          return ImGuiCol_TitleBgCollapsed;
                case EColorStyle::MenuBarBg:                 return ImGuiCol_MenuBarBg;
                case EColorStyle::ScrollbarBg:               return ImGuiCol_ScrollbarBg;
                case EColorStyle::ScrollbarGrab:             return ImGuiCol_ScrollbarGrab;
                case EColorStyle::ScrollbarGrabHovered:      return ImGuiCol_ScrollbarGrabHovered;
                case EColorStyle::ScrollbarGrabActive:       return ImGuiCol_ScrollbarGrabActive;
                case EColorStyle::CheckMark:                 return ImGuiCol_CheckMark;
                case EColorStyle::SliderGrab:                return ImGuiCol_SliderGrab;
                case EColorStyle::SliderGrabActive:          return ImGuiCol_SliderGrabActive;
                case EColorStyle::Button:                    return ImGuiCol_Button;
                case EColorStyle::ButtonHovered:             return ImGuiCol_ButtonHovered;
                case EColorStyle::ButtonActive:              return ImGuiCol_ButtonActive;
                case EColorStyle::Header:                    return ImGuiCol_Header;
                case EColorStyle::HeaderHovered:             return ImGuiCol_HeaderHovered;
                case EColorStyle::HeaderActive:              return ImGuiCol_HeaderActive;
                case EColorStyle::Separator:                 return ImGuiCol_Separator;
                case EColorStyle::SeparatorHovered:          return ImGuiCol_SeparatorHovered;
                case EColorStyle::SeparatorActive:           return ImGuiCol_SeparatorActive;
                case EColorStyle::ResizeGrip:                return ImGuiCol_ResizeGrip;
                case EColorStyle::ResizeGripHovered:         return ImGuiCol_ResizeGripHovered;
                case EColorStyle::ResizeGripActive:          return ImGuiCol_ResizeGripActive;
                case EColorStyle::TabHovered:                return ImGuiCol_TabHovered;
                case EColorStyle::Tab:                       return ImGuiCol_Tab;
                case EColorStyle::TabSelected:               return ImGuiCol_TabSelected;
                case EColorStyle::TabSelectedOverline:       return ImGuiCol_TabSelectedOverline;
                case EColorStyle::TabDimmed:                 return ImGuiCol_TabDimmed;
                case EColorStyle::TabDimmedSelected:         return ImGuiCol_TabDimmedSelected;
                case EColorStyle::TabDimmedSelectedOverline: return ImGuiCol_TabDimmedSelectedOverline;
                case EColorStyle::DockingPreview:            return ImGuiCol_DockingPreview;
                case EColorStyle::DockingEmptyBg:            return ImGuiCol_DockingEmptyBg;
                case EColorStyle::PlotLines:                 return ImGuiCol_PlotLines;
                case EColorStyle::PlotLinesHovered:          return ImGuiCol_PlotLinesHovered;
                case EColorStyle::PlotHistogram:             return ImGuiCol_PlotHistogram;
                case EColorStyle::PlotHistogramHovered:      return ImGuiCol_PlotHistogramHovered;
                case EColorStyle::TableHeaderBg:             return ImGuiCol_TableHeaderBg;
                case EColorStyle::TableBorderStrong:         return ImGuiCol_TableBorderStrong;
                case EColorStyle::TableBorderLight:          return ImGuiCol_TableBorderLight;
                case EColorStyle::TableRowBg:                return ImGuiCol_TableRowBg;
                case EColorStyle::TableRowBgAlt:             return ImGuiCol_TableRowBgAlt;
                case EColorStyle::TextSelectedBg:            return ImGuiCol_TextSelectedBg;
                case EColorStyle::DragDropTarget:            return ImGuiCol_DragDropTarget;
                case EColorStyle::NavHighlight:              return ImGuiCol_NavCursor;
                case EColorStyle::NavWindowingHighlight:     return ImGuiCol_NavWindowingHighlight;
                case EColorStyle::NavWindowingDimBg:         return ImGuiCol_NavWindowingDimBg;
                case EColorStyle::ModalWindowDimBg:          return ImGuiCol_ModalWindowDimBg;
                default:                                     return ImGuiCol_Text;
            }
        }

        ImGuiStyleVar ToImGuiStyleVar(EStyle var)
        {
            switch (var)
            {
                case EStyle::Alpha:                       return ImGuiStyleVar_Alpha;
                case EStyle::DisabledAlpha:               return ImGuiStyleVar_DisabledAlpha;
                case EStyle::WindowPadding:               return ImGuiStyleVar_WindowPadding;
                case EStyle::WindowRounding:              return ImGuiStyleVar_WindowRounding;
                case EStyle::WindowBorderSize:            return ImGuiStyleVar_WindowBorderSize;
                case EStyle::WindowMinSize:               return ImGuiStyleVar_WindowMinSize;
                case EStyle::WindowTitleAlign:            return ImGuiStyleVar_WindowTitleAlign;
                case EStyle::ChildRounding:               return ImGuiStyleVar_ChildRounding;
                case EStyle::ChildBorderSize:             return ImGuiStyleVar_ChildBorderSize;
                case EStyle::PopupRounding:               return ImGuiStyleVar_PopupRounding;
                case EStyle::PopupBorderSize:             return ImGuiStyleVar_PopupBorderSize;
                case EStyle::FramePadding:                return ImGuiStyleVar_FramePadding;
                case EStyle::FrameRounding:               return ImGuiStyleVar_FrameRounding;
                case EStyle::FrameBorderSize:             return ImGuiStyleVar_FrameBorderSize;
                case EStyle::ItemSpacing:                 return ImGuiStyleVar_ItemSpacing;
                case EStyle::ItemInnerSpacing:            return ImGuiStyleVar_ItemInnerSpacing;
                case EStyle::IndentSpacing:               return ImGuiStyleVar_IndentSpacing;
                case EStyle::CellPadding:                 return ImGuiStyleVar_CellPadding;
                case EStyle::ScrollbarSize:               return ImGuiStyleVar_ScrollbarSize;
                case EStyle::ScrollbarRounding:           return ImGuiStyleVar_ScrollbarRounding;
                case EStyle::GrabMinSize:                 return ImGuiStyleVar_GrabMinSize;
                case EStyle::GrabRounding:                return ImGuiStyleVar_GrabRounding;
                case EStyle::TabRounding:                 return ImGuiStyleVar_TabRounding;
                case EStyle::TabBorderSize:               return ImGuiStyleVar_TabBorderSize;
                case EStyle::TabBarBorderSize:            return ImGuiStyleVar_TabBarBorderSize;
                case EStyle::TabBarOverlineSize:          return ImGuiStyleVar_TabBarOverlineSize;
                case EStyle::TableAngledHeadersAngle:     return ImGuiStyleVar_TableAngledHeadersAngle;
                case EStyle::TableAngledHeadersTextAlign: return ImGuiStyleVar_TableAngledHeadersTextAlign;
                case EStyle::ButtonTextAlign:             return ImGuiStyleVar_ButtonTextAlign;
                case EStyle::SelectableTextAlign:         return ImGuiStyleVar_SelectableTextAlign;
                case EStyle::SeparatorTextBorderSize:     return ImGuiStyleVar_SeparatorTextBorderSize;
                case EStyle::SeparatorTextAlign:          return ImGuiStyleVar_SeparatorTextAlign;
                case EStyle::SeparatorTextPadding:        return ImGuiStyleVar_SeparatorTextPadding;
                case EStyle::DockingSeparatorSize:        return ImGuiStyleVar_DockingSeparatorSize;
                default:                                  return ImGuiStyleVar_Alpha;
            }
        }

        Buffer LoadFontBuffer(const std::filesystem::path &relPath)
        {
            if (auto rfs = Application::GeRelativeFileSystem(); rfs && rfs->FileExists(relPath))
            {
                return rfs->ReadFile(relPath);
            }

            std::error_code ec;
            if (std::filesystem::exists(relPath, ec))
            {
                std::ifstream file(relPath, std::ios::binary | std::ios::ate);
                if (file)
                {
                    const auto size = static_cast<size_t>(file.tellg());
                    file.seekg(0, std::ios::beg);
                    Buffer buf(size);
                    file.read(const_cast<char *>(reinterpret_cast<const char *>(buf.Data())), static_cast<std::streamsize>(size));
                    return buf;
                }
            }
            return {};
        }
    }

    const ThemeColors &GetTheme()
    {
        static const ThemeColors s_Theme{};
        return s_Theme;
    }

    const Metrics &GetMetrics()
    {
        static const Metrics s_Metrics{};
        return s_Metrics;
    }

    ImVec4 ToImVec4(const glm::vec4 &c)
    {
        return ImVec4(c.r, c.g, c.b, c.a);
    }

    ImU32 ToU32(const glm::vec4 &c, float alphaMul)
    {
        ImVec4 v(c.r, c.g, c.b, c.a * alphaMul);
        return ImGui::GetCurrentContext() ? ImGui::GetColorU32(v) : ImGui::ColorConvertFloat4ToU32(v);
    }

    glm::vec4 WithAlpha(const glm::vec4 &c, float alpha)
    {
        return glm::vec4(c.r, c.g, c.b, alpha);
    }

    glm::vec4 LerpColor(const glm::vec4 &a, const glm::vec4 &b, float t)
    {
        const float clamped = std::clamp(t, 0.0f, 1.0f);
        return a + (b - a) * clamped;
    }

    ScopedColorStyle::ScopedColorStyle(std::initializer_list<std::pair<EColorStyle, glm::vec4>> styles)
        : m_Count(static_cast<int>(styles.size()))
    {
        for (const auto &s : styles)
        {
            ImGui::PushStyleColor(ToImGuiCol(s.first), ToImVec4(s.second));
        }
    }

    ScopedColorStyle::~ScopedColorStyle()
    {
        if (m_Count > 0)
        {
            ImGui::PopStyleColor(m_Count);
        }
    }

    ScopedStyleVar::ScopedStyleVar(EStyle var, float val)
    {
        ImGui::PushStyleVar(ToImGuiStyleVar(var), val);
    }

    ScopedStyleVar::ScopedStyleVar(EStyle var, const ImVec2 &val)
    {
        ImGui::PushStyleVar(ToImGuiStyleVar(var), val);
    }

    ScopedStyleVar::~ScopedStyleVar()
    {
        ImGui::PopStyleVar();
    }

    ScopedFont::ScopedFont(FontVariant variant)
    {
        ImFont *font = FontRegistry::Get().GetFont(variant);
        if (font != nullptr)
        {
            ImGui::PushFont(font, font->LegacySize);
            m_Pushed = true;
        }
    }

    ScopedFont::~ScopedFont()
    {
        if (m_Pushed)
        {
            ImGui::PopFont();
        }
    }

    void PushFont(FontVariant variant)
    {
        ImFont *font = FontRegistry::Get().GetFont(variant);
        if (font != nullptr)
        {
            ImGui::PushFont(font, font->LegacySize);
        }
        else
        {
            ImGui::PushFont(nullptr, 0.0f);
        }
    }

    void PopFont()
    {
        ImGui::PopFont();
    }

    FontRegistry &FontRegistry::Get()
    {
        static FontRegistry s_Instance;
        return s_Instance;
    }

    void FontRegistry::InitFromVFS()
    {
        if (m_Initialized)
            return;

        m_GeistRegular  = LoadFontBuffer("resources/fonts/Geist-Regular.ttf");
        m_GeistMedium   = LoadFontBuffer("resources/fonts/Geist-Medium.ttf");
        m_GeistSemiBold = LoadFontBuffer("resources/fonts/Geist-SemiBold.ttf");
        m_GeistBold     = LoadFontBuffer("resources/fonts/Geist-Bold.ttf");
        m_GeistMono     = LoadFontBuffer("resources/fonts/GeistMono-Regular.ttf");
        m_LucideIcons   = LoadFontBuffer("resources/fonts/lucide.ttf");
        m_FallbackSegoe = LoadFontBuffer("resources/fonts/segoeui.ttf");

        m_Initialized = true;
    }

    float FontRegistry::GetBasePixelSize(FontVariant variant) const
    {
        switch (variant)
        {
            case FontVariant::Tiny:     return 11.0f;
            case FontVariant::Small:    return 12.0f;
            case FontVariant::Mono:     return 13.0f;
            case FontVariant::Regular:  return 14.0f;
            case FontVariant::Medium:   return 14.0f;
            case FontVariant::SemiBold: return 15.0f;
            case FontVariant::Bold:     return 16.0f;
            case FontVariant::Large:    return 16.0f;
            default:                    return 14.0f;
        }
    }

    void FontRegistry::RebuildAtlas(float dpiScale)
    {
        if (!m_Initialized)
            InitFromVFS();

        ImGuiContext *ctx = ImGui::GetCurrentContext();
        if (!ctx)
            return;

        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->Clear();
        io.Fonts->TexRef = ImTextureRef((ImTextureID)0);
        m_Fonts.fill(nullptr);

        const float clampedScale = std::max(dpiScale, 0.5f);
        m_CurrentDPIScale = clampedScale;

        auto pickBuffer = [&](FontVariant v) -> const Buffer *
        {
            const Buffer *buf = nullptr;
            switch (v)
            {
                case FontVariant::Regular:
                case FontVariant::Small:
                case FontVariant::Tiny:
                case FontVariant::Large:
                    buf = &m_GeistRegular;
                    break;
                case FontVariant::Medium:
                    buf = !m_GeistMedium.IsEmpty() ? &m_GeistMedium : &m_GeistRegular;
                    break;
                case FontVariant::SemiBold:
                    buf = !m_GeistSemiBold.IsEmpty() ? &m_GeistSemiBold : &m_GeistMedium;
                    break;
                case FontVariant::Bold:
                    buf = !m_GeistBold.IsEmpty() ? &m_GeistBold : &m_GeistSemiBold;
                    break;
                case FontVariant::Mono:
                    buf = !m_GeistMono.IsEmpty() ? &m_GeistMono : &m_GeistRegular;
                    break;
                default:
                    buf = &m_GeistRegular;
                    break;
            }
            if (!buf || buf->IsEmpty())
                buf = &m_FallbackSegoe;
            return (buf && !buf->IsEmpty()) ? buf : nullptr;
        };

        for (size_t i = 0; i < static_cast<size_t>(FontVariant::Count); ++i)
        {
            const auto variant = static_cast<FontVariant>(i);
            const float px = std::round(GetBasePixelSize(variant) * clampedScale);

            ImFontConfig cfg;
            cfg.SizePixels = px;
            cfg.OversampleH = 2;
            cfg.OversampleV = 1;
            cfg.PixelSnapH = false;

            const Buffer *buf = pickBuffer(variant);
            if (buf)
            {
                cfg.FontDataOwnedByAtlas = false;
                m_Fonts[i] = io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t *>(buf->Data()), static_cast<int>(buf->Size()), px, &cfg);
            }
            else
            {
                m_Fonts[i] = io.Fonts->AddFontDefaultVector(&cfg);
            }

            if (m_Fonts[i] && !m_LucideIcons.IsEmpty())
            {
                ImFontConfig iconCfg;
                iconCfg.MergeMode = true;
                iconCfg.PixelSnapH = true;
                iconCfg.FontDataOwnedByAtlas = false;
                iconCfg.OversampleH = 1;
                iconCfg.OversampleV = 1;
                const float iconPx = std::round(GetBasePixelSize(variant) * clampedScale);
                iconCfg.GlyphMinAdvanceX = iconPx;
                iconCfg.GlyphOffset = ImVec2(0.0f, std::round(1.5f * clampedScale));
                io.Fonts->AddFontFromMemoryTTF(
                    const_cast<uint8_t *>(m_LucideIcons.Data()),
                    static_cast<int>(m_LucideIcons.Size()),
                    iconPx,
                    &iconCfg,
                    kLucideGlyphRanges);
            }
        }

        if (m_Fonts[static_cast<size_t>(FontVariant::Regular)])
        {
            io.FontDefault = m_Fonts[static_cast<size_t>(FontVariant::Regular)];
        }
        io.Fonts->TexRef = ImTextureRef((ImTextureID)0);
    }

    void FontRegistry::ReleaseScaledFonts()
    {
        m_Fonts.fill(nullptr);
    }

    void FontRegistry::Shutdown()
    {
        ReleaseScaledFonts();
        m_GeistRegular.Release();
        m_GeistMedium.Release();
        m_GeistSemiBold.Release();
        m_GeistBold.Release();
        m_GeistMono.Release();
        m_LucideIcons.Release();
        m_FallbackSegoe.Release();
        m_Initialized = false;
    }

    ImFont *FontRegistry::GetFont(FontVariant variant) const
    {
        const auto idx = static_cast<size_t>(variant);
        if (idx < m_Fonts.size() && m_Fonts[idx] != nullptr)
        {
            return m_Fonts[idx];
        }
        return m_Fonts[static_cast<size_t>(FontVariant::Regular)];
    }

    void ApplyShadcnDarkTheme(ImGuiStyle &style)
    {
        const ThemeColors &t = GetTheme();
        const Metrics &m = GetMetrics();
        ImVec4 *colors = style.Colors;

        colors[ImGuiCol_Text]                      = ToImVec4(t.foreground);
        colors[ImGuiCol_TextDisabled]              = ToImVec4(t.disabledForeground);
        colors[ImGuiCol_WindowBg]                  = ToImVec4(t.background);
        colors[ImGuiCol_ChildBg]                   = ToImVec4(t.card);
        colors[ImGuiCol_PopupBg]                   = ToImVec4(t.popover);
        colors[ImGuiCol_Border]                    = ToImVec4(t.border);
        colors[ImGuiCol_BorderShadow]              = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_FrameBg]                   = ToImVec4(t.inputBg);
        colors[ImGuiCol_FrameBgHovered]            = ToImVec4(t.secondaryHover);
        colors[ImGuiCol_FrameBgActive]             = ToImVec4(t.secondaryActive);
        colors[ImGuiCol_TitleBg]                   = ToImVec4(t.background);
        colors[ImGuiCol_TitleBgActive]             = ToImVec4(t.card);
        colors[ImGuiCol_TitleBgCollapsed]          = ToImVec4(t.background);
        colors[ImGuiCol_MenuBarBg]                 = ToImVec4(t.background);
        colors[ImGuiCol_ScrollbarBg]               = ToImVec4(t.background);
        colors[ImGuiCol_ScrollbarGrab]             = ToImVec4(t.secondaryHover);
        colors[ImGuiCol_ScrollbarGrabHovered]      = ToImVec4(t.secondaryActive);
        colors[ImGuiCol_ScrollbarGrabActive]       = ToImVec4(t.mutedForeground);
        colors[ImGuiCol_CheckMark]                 = ToImVec4(t.primary);
        colors[ImGuiCol_CheckboxSelectedBg]        = ToImVec4(t.secondary);
        colors[ImGuiCol_SliderGrab]                = ToImVec4(t.primary);
        colors[ImGuiCol_SliderGrabActive]          = ToImVec4(t.primaryHover);
        colors[ImGuiCol_Button]                    = ToImVec4(t.secondary);
        colors[ImGuiCol_ButtonHovered]             = ToImVec4(t.secondaryHover);
        colors[ImGuiCol_ButtonActive]              = ToImVec4(t.secondaryActive);
        colors[ImGuiCol_Header]                    = ToImVec4(t.secondary);
        colors[ImGuiCol_HeaderHovered]             = ToImVec4(t.secondaryHover);
        colors[ImGuiCol_HeaderActive]              = ToImVec4(t.secondaryActive);
        colors[ImGuiCol_Separator]                 = ToImVec4(t.border);
        colors[ImGuiCol_SeparatorHovered]          = ToImVec4(t.borderStrong);
        colors[ImGuiCol_SeparatorActive]           = ToImVec4(t.primary);
        colors[ImGuiCol_ResizeGrip]                = ToImVec4(WithAlpha(t.mutedForeground, 0.25f));
        colors[ImGuiCol_ResizeGripHovered]         = ToImVec4(WithAlpha(t.primary, 0.65f));
        colors[ImGuiCol_ResizeGripActive]          = ToImVec4(t.primary);
        colors[ImGuiCol_InputTextCursor]           = ToImVec4(t.primary);
        colors[ImGuiCol_TabHovered]                = ToImVec4(t.secondaryHover);
        colors[ImGuiCol_Tab]                       = ToImVec4(t.background);
        colors[ImGuiCol_TabSelected]               = ToImVec4(t.secondary);
        colors[ImGuiCol_TabSelectedOverline]       = ToImVec4(t.primary);
        colors[ImGuiCol_TabDimmed]                 = ToImVec4(t.background);
        colors[ImGuiCol_TabDimmedSelected]         = ToImVec4(t.card);
        colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_DockingPreview]            = ToImVec4(WithAlpha(t.primary, 0.18f));
        colors[ImGuiCol_DockingEmptyBg]            = ToImVec4(t.background);
        colors[ImGuiCol_PlotLines]                 = ToImVec4(t.mutedForeground);
        colors[ImGuiCol_PlotLinesHovered]          = ToImVec4(t.primary);
        colors[ImGuiCol_PlotHistogram]             = ToImVec4(t.mutedForeground);
        colors[ImGuiCol_PlotHistogramHovered]      = ToImVec4(t.primary);
        colors[ImGuiCol_TableHeaderBg]             = ToImVec4(t.secondary);
        colors[ImGuiCol_TableBorderStrong]         = ToImVec4(t.border);
        colors[ImGuiCol_TableBorderLight]          = ToImVec4(t.borderSubtle);
        colors[ImGuiCol_TableRowBg]                = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_TableRowBgAlt]             = ImVec4(1.0f, 1.0f, 1.0f, 0.015f);
        colors[ImGuiCol_TextLink]                  = ToImVec4(t.primary);
        colors[ImGuiCol_TextSelectedBg]            = ImVec4(1.0f, 1.0f, 1.0f, 0.16f);
        colors[ImGuiCol_TreeLines]                 = ToImVec4(t.border);
        colors[ImGuiCol_DragDropTarget]            = ToImVec4(t.primary);
        colors[ImGuiCol_DragDropTargetBg]          = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
        colors[ImGuiCol_UnsavedMarker]             = ToImVec4(t.primary);
        colors[ImGuiCol_NavCursor]                 = ToImVec4(t.ring);
        colors[ImGuiCol_NavWindowingHighlight]     = ToImVec4(WithAlpha(t.primary, 0.70f));
        colors[ImGuiCol_NavWindowingDimBg]         = ImVec4(0.0f, 0.0f, 0.0f, 0.45f);
        colors[ImGuiCol_ModalWindowDimBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);

        // Generous shadcn spacing & flat 0px corners
        style.WindowPadding            = ImVec2(12.0f, 12.0f);
        style.FramePadding             = ImVec2(10.0f, 6.0f);
        style.ItemSpacing              = ImVec2(8.0f, 8.0f);
        style.ItemInnerSpacing         = ImVec2(6.0f, 6.0f);
        style.CellPadding              = ImVec2(8.0f, 5.0f);
        style.TouchExtraPadding        = ImVec2(0.0f, 0.0f);
        style.IndentSpacing            = 16.0f;
        style.ColumnsMinSpacing        = 8.0f;

        style.WindowBorderSize         = m.borderWidth;
        style.ChildBorderSize          = m.borderWidth;
        style.PopupBorderSize          = m.borderWidth;
        style.FrameBorderSize          = m.borderWidth;
        style.TabBorderSize            = 0.0f;
        style.TabBarBorderSize         = m.borderWidth;
        style.TabBarOverlineSize       = 2.0f;
        style.DockingSeparatorSize     = 1.0f;
        style.SeparatorSize            = 1.0f;
        style.SeparatorTextBorderSize  = 1.0f;
        style.SeparatorTextPadding     = ImVec2(12.0f, 4.0f);
        style.SeparatorTextAlign       = ImVec2(0.0f, 0.5f);

        style.WindowRounding           = m.cornerRadius;
        style.ChildRounding            = m.cornerRadius;
        style.PopupRounding            = m.cornerRadius;
        style.FrameRounding            = m.cornerRadius;
        style.ScrollbarRounding        = m.cornerRadius;
        style.GrabRounding             = m.cornerRadius;
        style.TabRounding              = m.cornerRadius;
        style.ImageRounding            = m.cornerRadius;
        style.ImageBorderSize          = 0.0f;
        style.MenuItemRounding         = m.cornerRadius;
        style.SelectableRounding       = m.cornerRadius;
        style.DragDropTargetRounding   = m.cornerRadius;

        style.ScrollbarSize            = 10.0f;
        style.ScrollbarPadding         = 2.0f;
        style.GrabMinSize              = 10.0f;

        style.WindowTitleAlign         = ImVec2(0.0f, 0.5f);
        style.WindowBorderHoverPadding = 6.0f;
        style.WindowMenuButtonPosition = ImGuiDir_Right;
        style.ColorButtonPosition      = ImGuiDir_Right;
        style.ButtonTextAlign          = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign      = ImVec2(0.0f, 0.5f);
        style.TabCloseButtonMinWidthSelected = 0.0f;
        style.TabCloseButtonMinWidthUnselected = 0.0f;

        style.AntiAliasedFill          = true;
        style.AntiAliasedLines         = true;
        style.AntiAliasedLinesUseTex   = true;
    }

    float AnimateFloat(ImGuiID id, float target, float speed)
    {
        auto &map = GetAnimMap();
        auto it = map.find(id);
        if (it == map.end())
        {
            map.emplace(id, target);
            return target;
        }

        const float dt = ImGui::GetCurrentContext() ? std::clamp(ImGui::GetIO().DeltaTime, 0.0001f, 0.1f) : (1.0f / 60.0f);
        const float factor = 1.0f - std::exp(-speed * dt);
        it->second += (target - it->second) * factor;
        if (std::abs(it->second - target) < 0.001f)
            it->second = target;
        return it->second;
    }

    void ClearAnimationState()
    {
        GetAnimMap().clear();
    }

    void PushShaderEffect(ImDrawList *drawList, ShaderEffect effect, float param0, float param1)
    {
        if (!drawList)
            return;

        const uint8_t p0 = static_cast<uint8_t>(std::clamp(std::round(param0 * 16.0f), 0.0f, 255.0f));
        const uint8_t p1 = static_cast<uint8_t>(std::clamp(std::round(param1), 0.0f, 255.0f));
        drawList->AddCallback(&ImGui_NRI::SetShaderParamsCallback, ImGui_NRI::PackShaderCallbackData(static_cast<uint32_t>(effect), p0, p1));
    }

    void PopShaderEffect(ImDrawList *drawList)
    {
        if (!drawList)
            return;

        drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    }

    float GetDpiScale()
    {
        return FontRegistry::Get().GetCurrentDPIScale();
    }

    void BindContext(ImGuiContext *ctx)
    {
        ImGui::SetCurrentContext(ctx);
    }
}

