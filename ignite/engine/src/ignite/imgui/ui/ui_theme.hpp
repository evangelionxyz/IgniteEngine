// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_UI_THEME_HPP
#define IGN_UI_THEME_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/buffer.hpp"
#include "ignite/math/math.hpp"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
    #define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>

#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

struct ImGuiContext;

namespace ignite::UI
{
    struct ThemeColors
    {
        // Shadcn Dark Monochrome (Zinc)
        glm::vec4 background{ 0.035f, 0.035f, 0.043f, 1.0f };        // #09090B zinc-950
        glm::vec4 card{ 0.047f, 0.047f, 0.055f, 1.0f };              // #0C0C0E
        glm::vec4 popover{ 0.059f, 0.059f, 0.071f, 0.99f };          // #0F0F12
        glm::vec4 inputBg{ 0.071f, 0.071f, 0.082f, 1.0f };           // #121215
        glm::vec4 inputBgHover{ 0.094f, 0.094f, 0.106f, 1.0f };      // #18181B
        glm::vec4 secondary{ 0.094f, 0.094f, 0.106f, 1.0f };         // #18181B zinc-900
        glm::vec4 secondaryHover{ 0.153f, 0.153f, 0.165f, 1.0f };    // #27272A zinc-800
        glm::vec4 secondaryActive{ 0.247f, 0.247f, 0.275f, 1.0f };   // #3F3F46 zinc-700

        glm::vec4 primary{ 0.980f, 0.980f, 0.980f, 1.0f };           // #FAFAFA zinc-50
        glm::vec4 primaryHover{ 0.894f, 0.894f, 0.906f, 1.0f };      // #E4E4E7 zinc-200
        glm::vec4 primaryActive{ 0.831f, 0.831f, 0.847f, 1.0f };     // #D4D4D8 zinc-300
        glm::vec4 primaryForeground{ 0.035f, 0.035f, 0.043f, 1.0f }; // #09090B zinc-950

        glm::vec4 border{ 0.153f, 0.153f, 0.165f, 1.0f };            // #27272A zinc-800
        glm::vec4 borderSubtle{ 0.122f, 0.122f, 0.137f, 1.0f };      // #1F1F23
        glm::vec4 borderStrong{ 0.247f, 0.247f, 0.275f, 1.0f };      // #3F3F46 zinc-700
        glm::vec4 ring{ 0.831f, 0.831f, 0.847f, 1.0f };              // #D4D4D8 zinc-300

        glm::vec4 foreground{ 0.980f, 0.980f, 0.980f, 1.0f };        // #FAFAFA zinc-50
        glm::vec4 mutedForeground{ 0.631f, 0.631f, 0.667f, 1.0f };   // #A1A1AA zinc-400
        glm::vec4 disabledForeground{ 0.322f, 0.322f, 0.357f, 1.0f };// #52525B zinc-600

        glm::vec4 destructive{ 0.498f, 0.114f, 0.114f, 1.0f };       // #7F1D1D
        glm::vec4 destructiveHover{ 0.600f, 0.106f, 0.106f, 1.0f };  // #991B1B
        glm::vec4 destructiveBorder{ 0.863f, 0.149f, 0.149f, 1.0f }; // #DC2626
        glm::vec4 destructiveForeground{ 0.996f, 0.949f, 0.949f, 1.0f }; // #FEF2F2
    };

    namespace Accent
    {
        inline constexpr glm::vec4 Folder{ 0.96f, 0.75f, 0.26f, 1.0f };      // #F5BF42 Amber
        inline constexpr glm::vec4 Mesh{ 0.20f, 0.83f, 0.60f, 1.0f };        // #34D399 Emerald
        inline constexpr glm::vec4 Camera{ 0.22f, 0.74f, 0.97f, 1.0f };      // #38BDF8 Sky
        inline constexpr glm::vec4 ScriptCS{ 0.38f, 0.65f, 0.98f, 1.0f };    // #60A5FA Blue
        inline constexpr glm::vec4 ScriptLua{ 0.51f, 0.55f, 0.97f, 1.0f };   // #818CF8 Indigo
        inline constexpr glm::vec4 Material{ 0.96f, 0.45f, 0.71f, 1.0f };    // #F472B6 Rose
        inline constexpr glm::vec4 Animation{ 0.75f, 0.52f, 0.99f, 1.0f };   // #C084FC Violet
        inline constexpr glm::vec4 Physics{ 0.52f, 0.88f, 0.38f, 1.0f };     // #84E161 Lime
        inline constexpr glm::vec4 Audio{ 0.98f, 0.57f, 0.24f, 1.0f };       // #FB923C Orange
        inline constexpr glm::vec4 Environment{ 0.13f, 0.83f, 0.93f, 1.0f }; // #22D3EE Cyan
        inline constexpr glm::vec4 Widget{ 0.91f, 0.47f, 0.98f, 1.0f };      // #E879F9 Fuchsia
        inline constexpr glm::vec4 Network{ 0.18f, 0.83f, 0.75f, 1.0f };     // #2DD4BF Teal
    }

    struct Metrics
    {
        float cornerRadius = 0.0f;
        float borderWidth = 1.0f;
        float buttonSm = 24.0f;
        float buttonMd = 28.0f;
        float buttonLg = 32.0f;
        float inputHeight = 28.0f;
        float switchWidth = 36.0f;
        float switchHeight = 20.0f;
        float checkboxSize = 16.0f;
        float sliderTrackHeight = 6.0f;
        float sliderThumbWidth = 10.0f;
        float sliderThumbHeight = 18.0f;
        float menuItemHeight = 26.0f;
    };

    enum class FontVariant : uint8_t
    {
        Regular = 0, // 14px Geist Regular + Lucide
        Small,       // 12px Geist Regular + Lucide
        Tiny,        // 11px Geist Regular + Lucide
        Large,       // 16px Geist Regular + Lucide
        Medium,      // 14px Geist Medium  + Lucide
        SemiBold,    // 15px Geist SemiBold + Lucide
        Bold,        // 16px Geist Bold + Lucide
        Mono,        // 13px Geist Mono + Lucide
        Count
    };

    enum class ShaderEffect : uint32_t
    {
        None = 0,
        Shimmer = 1,
        DotGrid = 2
    };

    class IGN_API FontRegistry
    {
    public:
        static FontRegistry &Get();

        void InitFromVFS();
        void RebuildAtlas(float dpiScale);
        void ReleaseScaledFonts();
        void Shutdown();

        [[nodiscard]] ImFont *GetFont(FontVariant variant) const;
        [[nodiscard]] float GetBasePixelSize(FontVariant variant) const;
        [[nodiscard]] float GetCurrentDPIScale() const { return m_CurrentDPIScale; }
        [[nodiscard]] bool IsInitialized() const { return m_Initialized; }

    private:
        FontRegistry() = default;

        Buffer m_GeistRegular;
        Buffer m_GeistMedium;
        Buffer m_GeistSemiBold;
        Buffer m_GeistBold;
        Buffer m_GeistMono;
        Buffer m_LucideIcons;
        Buffer m_FallbackSegoe;

        std::array<ImFont *, static_cast<size_t>(FontVariant::Count)> m_Fonts{};
        float m_CurrentDPIScale = 1.0f;
        bool m_Initialized = false;
    };

    enum class EWindowFlags
    {
        None = 0,
        NoTitleBar = 1 << 0,
        NoResize = 1 << 1,
        NoMove = 1 << 2,
        NoScrollbar = 1 << 3,
        NoScrollWithMouse = 1 << 4,
        NoCollapse = 1 << 5,
        AlwaysAutoResize = 1 << 6,
        NoBackground = 1 << 7,
        NoSavedSettings = 1 << 8,
        NoMouseInputs = 1 << 9,
        MenuBar = 1 << 10,
        HorizontalScrollbar = 1 << 11,
        NoFocusOnAppearing = 1 << 12,
        NoBringToFrontOnFocus = 1 << 13,
        AlwaysVerticalScrollbar = 1 << 14,
        AlwaysHorizontalScrollbar = 1 << 15,
        NoNavInputs = 1 << 16,
        NoNavFocus = 1 << 17,
        UnsavedDocument = 1 << 18,
        NoDocking = 1 << 19,
        NoNav = NoNavInputs | NoNavFocus,
        NoDecoration = NoTitleBar | NoResize | NoScrollbar | NoCollapse,
        NoInputs = NoMouseInputs | NoNavInputs | NoNavFocus,
        ChildWindow = 1 << 24,
        Tooltip = 1 << 25,
        Popup = 1 << 26,
        Modal = 1 << 27,
        ChildMenu = 1 << 28,
        DockNodeHost = 1 << 29,
    };

    enum class EStyle
    {
        Alpha,
        DisabledAlpha,
        WindowPadding,
        WindowRounding,
        WindowBorderSize,
        WindowMinSize,
        WindowTitleAlign,
        ChildRounding,
        ChildBorderSize,
        PopupRounding,
        PopupBorderSize,
        FramePadding,
        FrameRounding,
        FrameBorderSize,
        ItemSpacing,
        ItemInnerSpacing,
        IndentSpacing,
        CellPadding,
        ScrollbarSize,
        ScrollbarRounding,
        GrabMinSize,
        GrabRounding,
        TabRounding,
        TabBorderSize,
        TabBarBorderSize,
        TabBarOverlineSize,
        TableAngledHeadersAngle,
        TableAngledHeadersTextAlign,
        ButtonTextAlign,
        SelectableTextAlign,
        SeparatorTextBorderSize,
        SeparatorTextAlign,
        SeparatorTextPadding,
        DockingSeparatorSize,
    };

    enum class EColorStyle
    {
        Text,
        TextDisabled,
        WindowBg,
        ChildBg,
        PopupBg,
        Border,
        BorderShadow,
        FrameBg,
        FrameBgHovered,
        FrameBgActive,
        TitleBg,
        TitleBgActive,
        TitleBgCollapsed,
        MenuBarBg,
        ScrollbarBg,
        ScrollbarGrab,
        ScrollbarGrabHovered,
        ScrollbarGrabActive,
        CheckMark,
        SliderGrab,
        SliderGrabActive,
        Button,
        ButtonHovered,
        ButtonActive,
        Header,
        HeaderHovered,
        HeaderActive,
        Separator,
        SeparatorHovered,
        SeparatorActive,
        ResizeGrip,
        ResizeGripHovered,
        ResizeGripActive,
        TabHovered,
        Tab,
        TabSelected,
        TabSelectedOverline,
        TabDimmed,
        TabDimmedSelected,
        TabDimmedSelectedOverline,
        DockingPreview,
        DockingEmptyBg,
        PlotLines,
        PlotLinesHovered,
        PlotHistogram,
        PlotHistogramHovered,
        TableHeaderBg,
        TableBorderStrong,
        TableBorderLight,
        TableRowBg,
        TableRowBgAlt,
        TextSelectedBg,
        DragDropTarget,
        NavHighlight,
        NavWindowingHighlight,
        NavWindowingDimBg,
        ModalWindowDimBg,
    };

    class IGN_API ScopedColorStyle
    {
    public:
        ScopedColorStyle(std::initializer_list<std::pair<EColorStyle, glm::vec4>> styles);
        ~ScopedColorStyle();

        ScopedColorStyle(const ScopedColorStyle &) = delete;
        ScopedColorStyle &operator=(const ScopedColorStyle &) = delete;

    private:
        int m_Count = 0;
    };

    class IGN_API ScopedStyleVar
    {
    public:
        ScopedStyleVar(EStyle var, float val);
        ScopedStyleVar(EStyle var, const ImVec2 &val);
        ~ScopedStyleVar();

        ScopedStyleVar(const ScopedStyleVar &) = delete;
        ScopedStyleVar &operator=(const ScopedStyleVar &) = delete;
    };

    class IGN_API ScopedFont
    {
    public:
        explicit ScopedFont(FontVariant variant);
        ~ScopedFont();

        ScopedFont(const ScopedFont &) = delete;
        ScopedFont &operator=(const ScopedFont &) = delete;

    private:
        bool m_Pushed = false;
    };

    IGN_API const ThemeColors &GetTheme();
    IGN_API const Metrics &GetMetrics();
    IGN_API void ApplyShadcnDarkTheme(ImGuiStyle &style);

    IGN_API ImVec4 ToImVec4(const glm::vec4 &c);
    IGN_API ImU32 ToU32(const glm::vec4 &c, float alphaMul = 1.0f);
    IGN_API glm::vec4 WithAlpha(const glm::vec4 &c, float alpha);
    IGN_API glm::vec4 LerpColor(const glm::vec4 &a, const glm::vec4 &b, float t);

    IGN_API void PushFont(FontVariant variant);
    IGN_API void PopFont();

    IGN_API float AnimateFloat(ImGuiID id, float target, float speed = 16.0f);
    IGN_API void ClearAnimationState();

    IGN_API void PushShaderEffect(ImDrawList *drawList, ShaderEffect effect, float param0 = 1.0f, float param1 = 16.0f);
    IGN_API void PopShaderEffect(ImDrawList *drawList);

    IGN_API float GetDpiScale();
    IGN_API void BindContext(ImGuiContext *ctx);
}

#endif

