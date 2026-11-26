// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "ui_showcase.hpp"
#include "ui_theme.hpp"
#include "ui_button.hpp"
#include "ui_toggle.hpp"
#include "ui_input.hpp"
#include "ui_slider.hpp"
#include "ui_dropdown.hpp"
#include "ui_feedback.hpp"
#include "ui_properties.hpp"
#include "icons_lucide.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

namespace ignite::UI
{
    namespace
    {
        bool CaseInsensitiveContains(std::string_view text, std::string_view query)
        {
            if (query.empty())
                return true;
            auto it = std::search(
                text.begin(), text.end(),
                query.begin(), query.end(),
                [](char a, char b)
                {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
            return it != text.end();
        }
    }

    void ShowShowcaseWindow(bool *pOpen)
    {
        const ThemeColors &t = GetTheme();

        ImGui::SetNextWindowSize(ImVec2(820.0f, 640.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(ICON_LC_SPARKLES " Ignite UI Kit — Shadcn Dark Monochrome", pOpen))
        {
            ImGui::End();
            return;
        }

        static int s_ActiveTab = 0;
        static constexpr std::array<const char *, 6> kTabs = {
            "Overview", "Buttons & Toggles", "Inputs & Sliders", "Menus & Dropdowns", "Loading & Shaders", "Inspector & Icons"
        };

        // Header bar
        {
            ScopedFont boldFont(FontVariant::Bold);
            ImGui::TextUnformatted("Ignite Design System");
        }
        ImGui::SameLine();
        Badge("shadcn/zinc", ButtonVariant::Secondary);
        ImGui::SameLine();
        Badge("0px flat", ButtonVariant::Outline);
        ImGui::SameLine();
        Badge("Geist + Lucide", ButtonVariant::Outline, ICON_LC_SPARKLES);

        {
            ScopedFont smallFont(FontVariant::Small);
            ImGui::TextColored(ToImVec4(t.mutedForeground), "Dark monochrome zinc palette, crisp 1px borders, 11-16px Geist typography, 24-32px interactive controls.");
        }

        ImGui::Spacing();
        ButtonGroup("##showcase_tabs", kTabs, &s_ActiveTab, ButtonSize::Medium, ImGui::GetContentRegionAvail().x);
        ImGui::Spacing();

        if (s_ActiveTab == 0)
        {
            // Overview: Typography + Palette + Quick Sandbox
            if (BeginCard("##overview_typo", ImVec2(0, 0), true))
            {
                CardHeader("Typography Scale (11px - 16px)", "Geist Sans & Geist Mono merged with Lucide icon glyphs");

                {
                    ScopedFont f(FontVariant::Bold);
                    ImGui::TextUnformatted("16px Bold — Scene & Material Inspector Header");
                }
                {
                    ScopedFont f(FontVariant::SemiBold);
                    ImGui::TextUnformatted("15px SemiBold — Section Title & Card Heading");
                }
                {
                    ScopedFont f(FontVariant::Medium);
                    ImGui::TextUnformatted("14px Medium — Primary Button & Active Navigation");
                }
                {
                    ScopedFont f(FontVariant::Regular);
                    ImGui::TextUnformatted("14px Regular — Default UI body text, labels, and tooltips.");
                }
                {
                    ScopedFont f(FontVariant::Mono);
                    ImGui::TextColored(ToImVec4(t.mutedForeground), "13px Mono — vec3(1.000, 0.500, 0.000) | 16.67 ms | 0x4A2F");
                }
                {
                    ScopedFont f(FontVariant::Small);
                    ImGui::TextColored(ToImVec4(t.mutedForeground), "12px Small — Helper descriptions, shortcuts (Ctrl+S), and badges.");
                }
                {
                    ScopedFont f(FontVariant::Tiny);
                    ImGui::TextColored(ToImVec4(t.disabledForeground), "11px Tiny — Micro metadata & status bar timestamps.");
                }
                EndCard();
            }

            ImGui::Spacing();

            if (BeginCard("##overview_quick"))
            {
                CardHeader("Interactive Quick Preview", "Live test bench combining core widgets in a flat monochrome card");
                static bool s_AutoSave = true;
                static bool s_Confirm = false;
                static float s_Opacity = 0.85f;
                static std::string s_SceneName = "MainViewport";
                static int s_Preset = 1;
                static constexpr const char *kPresets[] = { "Minimal", "Balanced", "Ultra" };

                TextField("##quick_name", s_SceneName, "Scene name...", ImVec2(0.0f, 28.0f), ICON_LC_LAYERS, true);
                ImGui::Spacing();
                Dropdown("##quick_preset", kPresets, 3, &s_Preset, "Select preset...");
                ImGui::Spacing();
                SliderFloat("Render Opacity", &s_Opacity, 0.0f, 1.0f, "%.2f");
                ImGui::Spacing();
                Switch("Auto-Sync Shaders", &s_AutoSave, "Watch HLSL source edits and hot-reload PSOs");
                ImGui::Spacing();
                Checkbox("Bake Ambient Occlusion", &s_Confirm);
                ImGui::Spacing();

                if (Button("Apply Changes", ButtonVariant::Primary, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_CHECK))
                {
                    s_Confirm = !s_Confirm;
                }
                ImGui::SameLine();
                Button("Discard", ButtonVariant::Outline, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_ROTATE_CCW);
                ImGui::SameLine();
                Button("Delete", ButtonVariant::Destructive, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_TRASH_2);
                EndCard();
            }
        }
        else if (s_ActiveTab == 1)
        {
            if (BeginCard("##card_buttons"))
            {
                CardHeader("Buttons & Variants", "Flat 0px corners with Primary, Secondary, Outline, Ghost, Destructive, and Link variants");
                static int s_ClickCount = 0;
                static bool s_SimulateLoading = false;
                static bool s_ToggleBtn = true;
                static int s_Segmented = 0;
                static constexpr const char *kSegLabels[] = { "Translate", "Rotate", "Scale", "Bounds" };

                if (Button("Primary", ButtonVariant::Primary, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_SPARKLES)) ++s_ClickCount;
                ImGui::SameLine();
                if (Button("Secondary", ButtonVariant::Secondary)) ++s_ClickCount;
                ImGui::SameLine();
                if (Button("Outline", ButtonVariant::Outline, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_COPY)) ++s_ClickCount;
                ImGui::SameLine();
                if (Button("Ghost", ButtonVariant::Ghost, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_SETTINGS)) ++s_ClickCount;
                ImGui::SameLine();
                if (Button("Destructive", ButtonVariant::Destructive, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_TRASH_2)) ++s_ClickCount;
                ImGui::SameLine();
                if (Button("Documentation", ButtonVariant::Link)) ++s_ClickCount;

                ImGui::Spacing();
                Button("Small 24px", ButtonVariant::Secondary, ButtonSize::Small);
                ImGui::SameLine();
                Button("Medium 28px", ButtonVariant::Secondary, ButtonSize::Medium);
                ImGui::SameLine();
                Button("Large 32px", ButtonVariant::Primary, ButtonSize::Large);
                ImGui::SameLine();
                Button("Compiling", ButtonVariant::Outline, ButtonSize::Medium, ImVec2(0, 0), nullptr, s_SimulateLoading);
                ImGui::SameLine();
                Button("Disabled", ButtonVariant::Secondary, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_LOCK, false, true);

                ImGui::Spacing();
                IconButton("##ic_play", ICON_LC_PLAY, ButtonVariant::Outline, ButtonSize::IconSm, "Play (24px)");
                ImGui::SameLine();
                IconButton("##ic_pause", ICON_LC_PAUSE, ButtonVariant::Outline, ButtonSize::IconMd, "Pause (28px)");
                ImGui::SameLine();
                IconButton("##ic_stop", ICON_LC_SQUARE, ButtonVariant::Outline, ButtonSize::IconLg, "Stop (32px)");
                ImGui::SameLine();
                ToggleButton("Snapping", &s_ToggleBtn, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_ZAP);
                ImGui::SameLine();
                Switch("Simulate Loading Button", &s_SimulateLoading);

                ImGui::Spacing();
                ButtonGroup("##gizmo_group", kSegLabels, &s_Segmented, ButtonSize::Medium);

                {
                    ScopedFont mono(FontVariant::Mono);
                    ImGui::TextColored(ToImVec4(t.mutedForeground), "Button clicks: %d | Active tool: %s", s_ClickCount, kSegLabels[s_Segmented]);
                }
                EndCard();
            }

            ImGui::Spacing();

            if (BeginCard("##card_toggles"))
            {
                CardHeader("Switchers, Checkboxes & Radio Groups", "Animated monochrome state transitions and indeterminate checks");
                static bool s_Sw1 = true;
                static bool s_Sw2 = false;
                static bool s_Sw3 = true;
                static bool s_Chk1 = true;
                static bool s_Chk2 = false;
                static bool s_Chk3 = false;
                static int s_Radio = 1;
                static constexpr const char *kRadioOpts[] = { "Local Space", "World Space", "Screen Space" };

                Switch("Enable GTAO Prepass", &s_Sw1, "Half-resolution ground-truth ambient occlusion");
                ImGui::Spacing();
                Switch("VSync", &s_Sw2, "Synchronize swapchain presentation to monitor refresh");
                ImGui::Spacing();
                Switch("Locked Option", &s_Sw3, "Disabled switcher state preview", true);

                ImGui::Spacing();
                Checkbox("Cast Dynamic Shadows", &s_Chk1);
                ImGui::Spacing();
                Checkbox("Show Wireframe Overlay", &s_Chk2, "Renders edge overlay over lit geometry");
                ImGui::Spacing();
                Checkbox("Indeterminate Selection", &s_Chk3, nullptr, !s_Chk3);

                ImGui::Spacing();
                RadioGroup("##space_radio", kRadioOpts, &s_Radio, true);
                EndCard();
            }
        }
        else if (s_ActiveTab == 2)
        {
            if (BeginCard("##card_inputs"))
            {
                CardHeader("Text Fields, Search & Password", "Crisp 1px borders with shadcn focus ring and clear/reveal actions");
                static std::string s_Text = "Directional Light";
                static std::string s_Search;
                static std::string s_Password = "super-secret-token";
                static std::string s_Notes = "Flat monochrome UI system\nZero corner rounding, high contrast zinc palette.";

                TextField("##tf_entity", s_Text, "Entity name...", ImVec2(0.0f, 28.0f), ICON_LC_BOX, true);
                ImGui::Spacing();
                SearchField("##tf_search", s_Search, "Search assets, components, or shaders...");
                ImGui::Spacing();
                PasswordField("##tf_pass", s_Password, "API Key...");
                ImGui::Spacing();
                TextArea("##tf_notes", s_Notes, "Notes...", ImVec2(0.0f, 84.0f));
                EndCard();
            }

            ImGui::Spacing();

            if (BeginCard("##card_sliders"))
            {
                CardHeader("Sliders & Range Controls", "Drag smoothly or Ctrl+Click / Double-Click to type exact numeric values");
                static float s_Exposure = 1.15f;
                static int s_Samples = 16;
                static float s_RangeMin = 0.25f;
                static float s_RangeMax = 0.80f;

                SliderFloat("Tone Mapping Exposure", &s_Exposure, 0.1f, 4.0f, "%.2f");
                ImGui::Spacing();
                SliderInt("GTAO Sample Count", &s_Samples, 2, 64, "%d samples");
                ImGui::Spacing();
                RangeSliderFloat("Shadow Cascade Split Range", &s_RangeMin, &s_RangeMax, 0.0f, 1.0f, "%.2f");
                EndCard();
            }
        }
        else if (s_ActiveTab == 3)
        {
            if (BeginCard("##card_menus"))
            {
                CardHeader("Dropdowns & Context Menus", "Searchable popovers and right-click / trigger context menus");
                static int s_Backend = 0;
                static int s_ShaderIdx = 2;
                static bool s_CtxGrid = true;
                static bool s_CtxSnap = false;
                static std::string s_LastAction = "None";

                static constexpr const char *kBackends[] = { "Vulkan 1.3 (NRI)", "DirectX 12 (NRI)" };
                static constexpr const char *kShaders[] = {
                    "PBR Standard", "PBR ClearCoat", "PBR Subsurface", "Unlit Wireframe",
                    "Sky Atmosphere", "Volumetric Fog", "GTAO Half-Res", "GI Probe Debug",
                    "UI Monochrome", "UI Shimmer"
                };

                Dropdown("##dd_backend", kBackends, 2, &s_Backend, "Choose backend...");
                ImGui::Spacing();
                Dropdown("##dd_shader", kShaders, IM_ARRAYSIZE(kShaders), &s_ShaderIdx, "Searchable shader list...", 0.0f, true);
                ImGui::Spacing();

                // Right-click target area + explicit button trigger
                const ImVec2 areaPos = ImGui::GetCursorScreenPos();
                const ImVec2 areaSize(ImGui::GetContentRegionAvail().x, 96.0f);
                ImGui::InvisibleButton("##ctx_zone", areaSize);
                const bool zoneHovered = ImGui::IsItemHovered();
                ImDrawList *dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(areaPos, ImVec2(areaPos.x + areaSize.x, areaPos.y + areaSize.y), ToU32(zoneHovered ? t.secondary : t.inputBg), 0.0f);
                dl->AddRect(areaPos, ImVec2(areaPos.x + areaSize.x, areaPos.y + areaSize.y), ToU32(zoneHovered ? t.borderStrong : t.border), 0.0f, 0, 1.0f);
                const char *hint = "Right-click anywhere in this box to open Shadcn Context Menu";
                const ImVec2 hs = ImGui::CalcTextSize(hint);
                dl->AddText(ImVec2(areaPos.x + (areaSize.x - hs.x) * 0.5f, areaPos.y + (areaSize.y - hs.y) * 0.5f), ToU32(t.mutedForeground), hint);

                if (BeginContextMenu("##zone_ctx"))
                {
                    ContextMenuLabel("Viewport Actions");
                    ContextMenuSeparator();
                    if (ContextMenuItem("Duplicate Entity", "Ctrl+D", ICON_LC_COPY)) s_LastAction = "Duplicate";
                    if (ContextMenuItem("Focus Selection", "F", ICON_LC_SEARCH)) s_LastAction = "Focus";
                    ContextMenuCheckboxItem("Show Grid", &s_CtxGrid, "G");
                    ContextMenuCheckboxItem("Grid Snap", &s_CtxSnap, "Ctrl+G");
                    if (ContextMenuBeginSubMenu("Shading Mode", ICON_LC_PALETTE))
                    {
                        if (ContextMenuItem("Lit PBR")) s_LastAction = "Lit PBR";
                        if (ContextMenuItem("Normals")) s_LastAction = "Normals";
                        if (ContextMenuItem("Ambient Occlusion")) s_LastAction = "AO";
                        ContextMenuEndSubMenu();
                    }
                    ContextMenuSeparator();
                    if (ContextMenuItem("Delete Entity", "Del", ICON_LC_TRASH_2, true)) s_LastAction = "Delete";
                    EndContextMenu();
                }

                ImGui::Spacing();
                if (Button("Open Menu via Button", ButtonVariant::Outline, ButtonSize::Medium, ImVec2(0, 0), ICON_LC_CHEVRON_DOWN))
                {
                    OpenPopupMenu("##btn_popup_menu");
                }
                if (BeginPopupMenu("##btn_popup_menu"))
                {
                    ContextMenuLabel("Quick Commands");
                    ContextMenuSeparator();
                    if (ContextMenuItem("Build Solution", "Ctrl+B", ICON_LC_CPU)) s_LastAction = "Build Solution";
                    if (ContextMenuItem("Reload Shaders", "F5", ICON_LC_REFRESH_CW)) s_LastAction = "Reload Shaders";
                    ContextMenuSeparator();
                    if (ContextMenuItem("Reset Layout", nullptr, ICON_LC_ROTATE_CCW, true)) s_LastAction = "Reset Layout";
                    EndContextMenu();
                }

                ImGui::SameLine();
                {
                    ScopedFont mono(FontVariant::Mono);
                    ImGui::TextColored(ToImVec4(t.mutedForeground), "Last menu action: %s", s_LastAction.c_str());
                }
                EndCard();
            }
        }
        else if (s_ActiveTab == 4)
        {
            if (BeginCard("##card_loading", ImVec2(0, 0), true))
            {
                CardHeader("Loading Spinners, Progress & GPU Shaders", "Custom HLSL shimmer & dot-grid effects rendered via ImGui_NRI");
                static float s_Progress = 0.64f;
                static bool s_AutoAnimate = true;

                LoadingSpinner("##spin_arc", 11.0f, 2.2f);
                ImGui::SameLine(0.0f, 16.0f);
                LoadingSpinnerTicks("##spin_ticks", 11.0f, 10);
                ImGui::SameLine(0.0f, 16.0f);
                Switch("Animate Progress", &s_AutoAnimate);

                if (s_AutoAnimate)
                {
                    s_Progress = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 1.1f);
                }

                ImGui::Spacing();
                ImGui::TextUnformatted("Determinate Progress");
                ProgressBar(s_Progress, ImVec2(-1.0f, 16.0f), "64%");
                ImGui::Spacing();
                ImGui::TextUnformatted("Indeterminate Progress (GPU Shimmer Shader)");
                ProgressBar(0.0f, ImVec2(-1.0f, 10.0f), nullptr, true);

                ImGui::Spacing();
                ImGui::TextUnformatted("Skeleton Placeholders (GPU Shimmer Shader)");
                SkeletonBox(ImVec2(240.0f, 18.0f));
                ImGui::Spacing();
                SkeletonBox(ImVec2(-1.0f, 14.0f));
                ImGui::Spacing();
                SkeletonBox(ImVec2(320.0f, 14.0f));

                ImGui::Spacing();
                SliderFloat("Manual Progress", &s_Progress, 0.0f, 1.0f, "%.2f");
                EndCard();
            }
        }
        else if (s_ActiveTab == 5)
        {
            if (BeginCard("##card_inspector"))
            {
                CardHeader("Engine Property Controls (UI::Draw*)", "Unified inspector row widgets residing in Ignite.Engine");
                static glm::vec3 s_Pos(0.0f, 1.5f, -3.0f);
                static glm::vec3 s_Color(0.95f, 0.95f, 0.98f);
                static float s_Intensity = 2.5f;
                static int s_Bounces = 3;
                static bool s_CastShadows = true;
                static bool s_Visible = true;
                static int s_Mobility = 0;
                static constexpr const char *kMobility[] = { "Static", "Stationary", "Movable" };

                DrawVec3Control("Translation", s_Pos, 0.05f, 0.0f);
                DrawComboBox("Mobility", kMobility, 3, &s_Mobility);
                DrawSliderFloat("Intensity", &s_Intensity, 0.0f, 10.0f);
                DrawIntControl("GI Bounces", &s_Bounces, 1.0f, 1, 8, 2);
                DrawColorVec3("Tint Color", s_Color);
                DrawCheckbox("Cast Shadows", &s_CastShadows);
                DrawSwitch("Visible", &s_Visible);
                EndCard();
            }

            ImGui::Spacing();

            if (BeginCard("##card_icons"))
            {
                CardHeader("Lucide Icon Set Preview", "Merged directly into Geist fonts for crisp monochrome inline icons");
                static std::string s_IconFilter;
                SearchField("##icon_filter", s_IconFilter, "Filter preview icons...");
                ImGui::Spacing();

                struct IconSample { const char *name; const char *glyph; };
                static constexpr IconSample kSamples[] = {
                    { "sparkles", ICON_LC_SPARKLES }, { "search", ICON_LC_SEARCH }, { "settings", ICON_LC_SETTINGS },
                    { "sliders", ICON_LC_SLIDERS_HORIZONTAL }, { "layers", ICON_LC_LAYERS }, { "box", ICON_LC_BOX },
                    { "play", ICON_LC_PLAY }, { "pause", ICON_LC_PAUSE }, { "square", ICON_LC_SQUARE },
                    { "plus", ICON_LC_PLUS }, { "minus", ICON_LC_MINUS }, { "x", ICON_LC_X },
                    { "check", ICON_LC_CHECK }, { "trash-2", ICON_LC_TRASH_2 }, { "copy", ICON_LC_COPY },
                    { "save", ICON_LC_SAVE }, { "folder", ICON_LC_FOLDER }, { "file", ICON_LC_FILE },
                    { "eye", ICON_LC_EYE }, { "eye-off", ICON_LC_EYE_OFF }, { "lock", ICON_LC_LOCK },
                    { "palette", ICON_LC_PALETTE }, { "terminal", ICON_LC_TERMINAL }, { "cpu", ICON_LC_CPU },
                    { "globe", ICON_LC_GLOBE }, { "refresh", ICON_LC_REFRESH_CW }, { "rotate-ccw", ICON_LC_ROTATE_CCW },
                    { "sun", ICON_LC_SUN }, { "moon", ICON_LC_MOON }, { "bell", ICON_LC_BELL },
                    { "zap", ICON_LC_ZAP }, { "alert", ICON_LC_TRIANGLE_ALERT }
                };

                int col = 0;
                const int cols = 4;
                for (const auto &s : kSamples)
                {
                    if (!CaseInsensitiveContains(s.name, s_IconFilter))
                        continue;

                    if (col > 0 && (col % cols) != 0)
                        ImGui::SameLine();

                    Button(s.name, ButtonVariant::Outline, ButtonSize::Small, ImVec2(170.0f, 28.0f), s.glyph);
                    ++col;
                }
                EndCard();
            }
        }

        ImGui::End();
    }
}

