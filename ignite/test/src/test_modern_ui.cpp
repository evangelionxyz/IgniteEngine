// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite/imgui/ui/ui.hpp"

#include <gtest/gtest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <cstring>
#include <string>

namespace ignite::tests
{
    namespace
    {
        struct ScopedHeadlessUIContext
        {
            ImGuiContext *prevCtx = nullptr;
            ImGuiContext *ctx = nullptr;

            ScopedHeadlessUIContext()
            {
                prevCtx = ImGui::GetCurrentContext();
                ctx = ImGui::CreateContext();
                ImGui::SetCurrentContext(ctx);
                UI::BindContext(ctx);

                ImGuiIO &io = ImGui::GetIO();
                io.IniFilename = nullptr;
                io.DisplaySize = ImVec2(1280.0f, 800.0f);
                io.DeltaTime = 1.0f / 60.0f;

                UI::ApplyShadcnDarkTheme(ImGui::GetStyle());
                UI::FontRegistry::Get().InitFromVFS();
                UI::FontRegistry::Get().RebuildAtlas(1.0f);

                unsigned char *pixels = nullptr;
                int w = 0, h = 0;
                io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
                io.Fonts->TexID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(1));
            }

            ~ScopedHeadlessUIContext()
            {
                UI::ClearAnimationState();
                UI::FontRegistry::Get().ReleaseScaledFonts();
                UI::BindContext(prevCtx);
                ImGui::SetCurrentContext(prevCtx);
                if (ctx)
                {
                    ImGui::DestroyContext(ctx);
                    ctx = nullptr;
                }
            }
        };

        void BeginHeadlessFrame(const ImVec2 &mousePos = ImVec2(-1.0f, -1.0f), bool mouseDown = false)
        {
            ImGuiIO &io = ImGui::GetIO();
            io.AddMousePosEvent(mousePos.x, mousePos.y);
            io.AddMouseButtonEvent(0, mouseDown);
            ImGui::NewFrame();
        }

        void EndHeadlessFrame()
        {
            ImGui::Render();
        }
    }

    TEST(ModernUI, ShadcnFlatMonochromeStyleAndTypography)
    {
        ScopedHeadlessUIContext scoped;

        const ImGuiStyle &style = ImGui::GetStyle();
        EXPECT_FLOAT_EQ(style.WindowRounding, 0.0f);
        EXPECT_FLOAT_EQ(style.ChildRounding, 0.0f);
        EXPECT_FLOAT_EQ(style.PopupRounding, 0.0f);
        EXPECT_FLOAT_EQ(style.FrameRounding, 0.0f);
        EXPECT_FLOAT_EQ(style.GrabRounding, 0.0f);
        EXPECT_FLOAT_EQ(style.TabRounding, 0.0f);
        EXPECT_FLOAT_EQ(style.ScrollbarRounding, 0.0f);
        EXPECT_FLOAT_EQ(style.WindowBorderSize, 1.0f);
        EXPECT_FLOAT_EQ(style.FrameBorderSize, 1.0f);

        const UI::ThemeColors &theme = UI::GetTheme();
        EXPECT_NEAR(theme.background.r, theme.background.g, 0.01f);
        EXPECT_NEAR(theme.primary.r, theme.primary.g, 0.01f);
        EXPECT_NEAR(theme.primary.g, theme.primary.b, 0.01f);
        EXPECT_GT(theme.primary.r, 0.90f);
        EXPECT_LT(theme.background.r, 0.06f);

        auto &fonts = UI::FontRegistry::Get();
        ASSERT_NE(fonts.GetFont(UI::FontVariant::Tiny), nullptr);
        ASSERT_NE(fonts.GetFont(UI::FontVariant::Small), nullptr);
        ASSERT_NE(fonts.GetFont(UI::FontVariant::Regular), nullptr);
        ASSERT_NE(fonts.GetFont(UI::FontVariant::Medium), nullptr);
        ASSERT_NE(fonts.GetFont(UI::FontVariant::SemiBold), nullptr);
        ASSERT_NE(fonts.GetFont(UI::FontVariant::Bold), nullptr);
        ASSERT_NE(fonts.GetFont(UI::FontVariant::Mono), nullptr);

        EXPECT_GE(fonts.GetBasePixelSize(UI::FontVariant::Tiny), 11.0f);
        EXPECT_LE(fonts.GetBasePixelSize(UI::FontVariant::Bold), 16.0f);
    }

    TEST(ModernUI, ShowcaseAndAllWidgetsRenderCleanly)
    {
        ScopedHeadlessUIContext scoped;

        bool showcaseOpen = true;
        bool sw = false;
        bool chk = false;
        float sliderF = 0.25f;
        int sliderI = 10;
        float rangeLo = 0.2f;
        float rangeHi = 0.8f;
        int ddIdx = 0;
        int btnGrpIdx = 0;
        std::string textValue = "Hello";
        char buf[64] = "Search";
        glm::vec3 vec3Val(1.0f, 2.0f, 3.0f);

        static constexpr const char *kItems[] = { "Alpha", "Beta", "Gamma" };

        for (int frame = 0; frame < 4; ++frame)
        {
            BeginHeadlessFrame();
            UI::ShowShowcaseWindow(&showcaseOpen);

            ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(420.0f, 540.0f), ImGuiCond_Always);
            if (ImGui::Begin("WidgetHarness"))
            {
                if (UI::BeginCard("##card", ImVec2(0, 0), true))
                {
                    UI::CardHeader("Harness", "Verifies all split engine UI primitives");
                    UI::Button("Primary", UI::ButtonVariant::Primary, UI::ButtonSize::Medium, ImVec2(0, 0), ICON_LC_SPARKLES);
                    UI::Switch("Enable Feature", &sw);
                    UI::Checkbox("Agree", &chk);
                    UI::SliderFloat("Float", &sliderF, 0.0f, 1.0f);
                    UI::SliderInt("Int", &sliderI, 0, 100);
                    UI::RangeSliderFloat("Range", &rangeLo, &rangeHi, 0.0f, 1.0f);
                    UI::Dropdown("##dd", kItems, 3, &ddIdx, "Pick...", 0.0f, true);
                    UI::ButtonGroup("##bg", kItems, &btnGrpIdx);
                    UI::TextField("##tf", textValue, "Enter...", ImVec2(0, 28.0f), ICON_LC_LAYERS, true);
                    UI::SearchField("##sf", buf, sizeof(buf));
                    UI::LoadingSpinner("##spin");
                    UI::ProgressBar(0.5f, ImVec2(-1.0f, 10.0f), nullptr, true);
                    UI::SkeletonBox(ImVec2(120.0f, 16.0f));
                    UI::Badge("v2.0", UI::ButtonVariant::Outline);
                    UI::DrawVec3Control("Position", vec3Val);
                    UI::EndCard();
                }
            }
            ImGui::End();
            EndHeadlessFrame();

            ImDrawData *drawData = ImGui::GetDrawData();
            ASSERT_NE(drawData, nullptr);
            EXPECT_GT(drawData->TotalVtxCount, 0);
            EXPECT_GT(drawData->TotalIdxCount, 0);
        }
    }

    TEST(ModernUI, InteractiveClickTogglesButtonSwitchAndCheckbox)
    {
        ScopedHeadlessUIContext scoped;

        bool buttonClicked = false;
        bool switchVal = false;
        bool checkboxVal = false;

        ImVec2 btnCenter(0.0f, 0.0f);
        ImVec2 switchCenter(0.0f, 0.0f);
        ImVec2 checkboxCenter(0.0f, 0.0f);

        auto renderWidgets = [&]()
        {
            ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(320.0f, 240.0f), ImGuiCond_Always);
            if (ImGui::Begin("InteractiveTest", nullptr, ImGuiWindowFlags_NoDecoration))
            {
                if (UI::Button("ClickMe", UI::ButtonVariant::Primary, UI::ButtonSize::Medium, ImVec2(160.0f, 28.0f)))
                    buttonClicked = true;
                ImVec2 bMin = ImGui::GetItemRectMin();
                ImVec2 bMax = ImGui::GetItemRectMax();
                btnCenter = ImVec2((bMin.x + bMax.x) * 0.5f, (bMin.y + bMax.y) * 0.5f);

                UI::Switch("ToggleMe", &switchVal);
                ImVec2 sMin = ImGui::GetItemRectMin();
                ImVec2 sMax = ImGui::GetItemRectMax();
                switchCenter = ImVec2((sMin.x + sMax.x) * 0.5f, (sMin.y + sMax.y) * 0.5f);

                UI::Checkbox("CheckMe", &checkboxVal);
                ImVec2 cMin = ImGui::GetItemRectMin();
                ImVec2 cMax = ImGui::GetItemRectMax();
                checkboxCenter = ImVec2((cMin.x + cMax.x) * 0.5f, (cMin.y + cMax.y) * 0.5f);
            }
            ImGui::End();
        };

        // Frame 1: measure layout rects
        BeginHeadlessFrame();
        renderWidgets();
        EndHeadlessFrame();

        // Click button (hover -> press -> release)
        BeginHeadlessFrame(btnCenter, false);
        renderWidgets();
        EndHeadlessFrame();
        BeginHeadlessFrame(btnCenter, true);
        renderWidgets();
        EndHeadlessFrame();
        BeginHeadlessFrame(btnCenter, false);
        renderWidgets();
        EndHeadlessFrame();
        EXPECT_TRUE(buttonClicked);

        // Click switch
        BeginHeadlessFrame(switchCenter, false);
        renderWidgets();
        EndHeadlessFrame();
        BeginHeadlessFrame(switchCenter, true);
        renderWidgets();
        EndHeadlessFrame();
        BeginHeadlessFrame(switchCenter, false);
        renderWidgets();
        EndHeadlessFrame();
        EXPECT_TRUE(switchVal);

        // Click checkbox
        BeginHeadlessFrame(checkboxCenter, false);
        renderWidgets();
        EndHeadlessFrame();
        BeginHeadlessFrame(checkboxCenter, true);
        renderWidgets();
        EndHeadlessFrame();
        BeginHeadlessFrame(checkboxCenter, false);
        renderWidgets();
        EndHeadlessFrame();
        EXPECT_TRUE(checkboxVal);
    }

    TEST(ModernUI, ColoredContextMenusAndCompactProperties)
    {
        ScopedHeadlessUIContext scoped;

        ImGuiContext *ctx = ImGui::GetCurrentContext();
        ASSERT_NE(ctx, nullptr);

        float valF = 0.5f;
        int valI = 25;
        int comboIdx = 1;
        bool chk = true;
        std::string textVal = "PlayerEntity";
        glm::vec3 pos(1.0f, 2.0f, 3.0f);
        static constexpr const char *kComboLabels[] = { "Static", "Movable" };

        BeginHeadlessFrame();
        const int fontStackBefore = ctx->FontStack.Size;
        const int colorStackBefore = ctx->ColorStack.Size;
        const int styleVarStackBefore = ctx->StyleVarStack.Size;

        ImVec2 itemSize(0.0f, 0.0f);
        ImVec2 primitivesSize(0.0f, 0.0f);
        ImVec2 primitivesMin(0.0f, 0.0f);
        ImVec2 primitivesMax(0.0f, 0.0f);
        ImVec2 effectsMin(0.0f, 0.0f);
        ImVec2 effectsSize(0.0f, 0.0f);
        ImVec2 nestedSubmenuSize(0.0f, 0.0f);
        bool primitivesOpened = false;
        bool openMenuOnce = true;

        auto renderTestFrame = [&]()
        {
            ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(320.0f, 400.0f), ImGuiCond_Always);
            if (ImGui::Begin("CompactInspectorTest"))
            {
                UI::ScopedCompactInspector compact;

                const float startY = ImGui::GetCursorScreenPos().y;
                UI::DrawVec3Control("Translation", pos);
                const float vec3Height = ImGui::GetCursorScreenPos().y - startY;
                EXPECT_LE(vec3Height, 30.0f);

                const float rowStartY = ImGui::GetCursorScreenPos().y;
                UI::DrawTextControl("Prefab", textVal);
                UI::DrawSliderFloat("Intensity", &valF, 0.0f, 1.0f);
                UI::DrawSliderInt("Samples", &valI, 1, 64);
                UI::DrawComboBox("Mobility", kComboLabels, 2, &comboIdx);
                UI::DrawCheckbox("Cast Shadow", &chk);
                UI::DrawButtonWithColumn("Mesh", "Drag Here");
                const float avgRowH = (ImGui::GetCursorScreenPos().y - rowStartY) / 6.0f;
                EXPECT_LE(avgRowH, 28.0f);

                UI::IconButton("##disabled_btn", ICON_LC_ARROW_LEFT, UI::ButtonVariant::Ghost, UI::ButtonSize::IconSm, "Back", false, true);

                if (openMenuOnce)
                {
                    UI::OpenPopupMenu("##test_colored_menu");
                    openMenuOnce = false;
                }

                if (UI::BeginPopupMenu("##test_colored_menu"))
                {
                    UI::ContextMenuLabel("Actions");
                    UI::ContextMenuSeparator();

                    UI::ContextMenuItem("Create Folder", "Ctrl+Shift+N", ICON_LC_FOLDER_PLUS, false, true, UI::Accent::Folder);
                    itemSize = ImGui::GetItemRectSize();

                    const bool pOpen = UI::ContextMenuBeginSubMenu("Primitives", ICON_LC_BOX, true, UI::Accent::Mesh);
                    primitivesSize = ImGui::GetItemRectSize();
                    primitivesMin = ImGui::GetItemRectMin();
                    primitivesMax = ImGui::GetItemRectMax();
                    if (pOpen)
                    {
                        primitivesOpened = true;
                        if (UI::ContextMenuBeginSubMenu("Basic Shapes", ICON_LC_SPARKLES, true, UI::Accent::Mesh))
                        {
                            nestedSubmenuSize = ImGui::GetItemRectSize();
                            UI::ContextMenuItem("Sphere", nullptr, ICON_LC_BOX, false, true, UI::Accent::Mesh);
                            UI::ContextMenuEndSubMenu();
                        }
                        else
                        {
                            nestedSubmenuSize = ImGui::GetItemRectSize();
                        }
                        UI::ContextMenuItem("Cube", nullptr, ICON_LC_BOX, false, true, UI::Accent::Mesh);
                        UI::ContextMenuEndSubMenu();
                    }

                    const bool eOpen = UI::ContextMenuBeginSubMenu("Effects", ICON_LC_SPARKLES, true, UI::Accent::Audio);
                    effectsMin = ImGui::GetItemRectMin();
                    effectsSize = ImGui::GetItemRectSize();
                    if (eOpen)
                    {
                        UI::ContextMenuItem("Smoke", nullptr, ICON_LC_SPARKLES, false, true, UI::Accent::Audio);
                        UI::ContextMenuEndSubMenu();
                    }

                    UI::ContextMenuItem("Delete", "Del", ICON_LC_TRASH_2, true);
                    UI::EndContextMenu();
                }
            }
            ImGui::End();
        };

        renderTestFrame();

        EXPECT_EQ(ctx->FontStack.Size, fontStackBefore);
        EXPECT_EQ(ctx->ColorStack.Size, colorStackBefore);
        EXPECT_EQ(ctx->StyleVarStack.Size, styleVarStackBefore);
        EndHeadlessFrame();

        // Second frame so popup auto-resize settles before computing hover point
        BeginHeadlessFrame();
        renderTestFrame();
        EndHeadlessFrame();

        EXPECT_FLOAT_EQ(itemSize.y, 26.0f);
        EXPECT_FLOAT_EQ(primitivesSize.y, itemSize.y);
        EXPECT_FLOAT_EQ(effectsSize.y, itemSize.y);
        EXPECT_GE(effectsMin.y, primitivesMax.y + 1.5f);

        // Hover the Primitives submenu row and verify hover-to-open and 2-level submenu layout + stack balance
        const ImVec2 primitivesCenter((primitivesMin.x + primitivesMax.x) * 0.5f, (primitivesMin.y + primitivesMax.y) * 0.5f);
        BeginHeadlessFrame(primitivesCenter, false);
        renderTestFrame();
        EXPECT_EQ(ctx->FontStack.Size, fontStackBefore);
        EXPECT_EQ(ctx->ColorStack.Size, colorStackBefore);
        EXPECT_EQ(ctx->StyleVarStack.Size, styleVarStackBefore);
        EndHeadlessFrame();

        BeginHeadlessFrame(primitivesCenter, false);
        renderTestFrame();
        EXPECT_EQ(ctx->FontStack.Size, fontStackBefore);
        EXPECT_EQ(ctx->ColorStack.Size, colorStackBefore);
        EXPECT_EQ(ctx->StyleVarStack.Size, styleVarStackBefore);
        EndHeadlessFrame();

        EXPECT_TRUE(primitivesOpened);
        EXPECT_FLOAT_EQ(nestedSubmenuSize.y, 26.0f);

        ImDrawData *drawData = ImGui::GetDrawData();
        ASSERT_NE(drawData, nullptr);
        EXPECT_GT(drawData->TotalVtxCount, 0);
    }
}

