// Copyright (c) 2026 Evangelion Manuhutu

// Created by: Evangelion Manuhutu
// Date      : 19 April 2026

#include "widget_editor.hpp"
#include "states.hpp"
#include "ext/editor_ui.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <format>
#include <string>
#include <cstring>

namespace ignite
{
    namespace
    {
        static const char *GetWidgetTypeLabel(const IWidgetItem &item)
        {
            switch (item.GetWidgetType())
            {
                case WidgetType::Container: return "Container";
                case WidgetType::Button:    return "Button";
                case WidgetType::Label:     return "Label";
                default:                    return "Widget";
            }
        }

        static std::string GetWidgetTreeLabel(const Ref<IWidgetItem> &item, const WidgetCanvas *canvas)
        {
            if (!item)
                return "<null>";

            if (canvas && canvas->GetRoot() == item.get())
                return std::format("Canvas Root [{}]", item->id);

            const std::string displayName = item->name.empty() ? GetWidgetTypeLabel(*item) : item->name;
            return std::format("{} [{}]", displayName, item->id);
        }

        // Returns the screen-space anchor position for the given alignment on a parent rect.
        static ImVec2 GetAlignmentScreenPos(
            WidgetAlignment alignment,
            const Rect &rect,
            const ImVec2 &imagePos, const ImVec2 &imageSize,
            float canvasW, float canvasH)
        {
            const float minX  = rect.min.x;
            const float minY  = rect.min.y;
            const float midX  = (rect.min.x + rect.max.x) * 0.5f;
            const float midY  = (rect.min.y + rect.max.y) * 0.5f;
            const float maxX  = rect.max.x;
            const float maxY  = rect.max.y;

            float cx = minX, cy = minY;
            switch (alignment)
            {
                case WidgetAlignment::TopLeft:      cx = minX; cy = minY; break;
                case WidgetAlignment::TopCenter:    cx = midX; cy = minY; break;
                case WidgetAlignment::TopRight:     cx = maxX; cy = minY; break;
                case WidgetAlignment::CenterLeft:   cx = minX; cy = midY; break;
                case WidgetAlignment::Center:       cx = midX; cy = midY; break;
                case WidgetAlignment::CenterRight:  cx = maxX; cy = midY; break;
                case WidgetAlignment::BottomLeft:   cx = minX; cy = maxY; break;
                case WidgetAlignment::BottomCenter: cx = midX; cy = maxY; break;
                case WidgetAlignment::BottomRight:  cx = maxX; cy = maxY; break;
                default: break;
            }

            return WidgetEditor::CanvasToScreen(cx, cy, imagePos, imageSize, canvasW, canvasH);
        }
    }

    // =========================================================================
    // Public helpers
    // =========================================================================

    ImVec2 WidgetEditor::CanvasToScreen(float cx, float cy,
        const ImVec2 &imagePos, const ImVec2 &imageSize,
        float canvasW, float canvasH)
    {
        return ImVec2(
            imagePos.x + (cx / canvasW) * imageSize.x,
            imagePos.y + (cy / canvasH) * imageSize.y);
    }

    // =========================================================================
    // Tree
    // =========================================================================

    void WidgetEditor::DrawWidgetTreeRecursive(const Ref<IWidgetItem> &item, int &selectedItemId, const WidgetCanvas *canvas)
    {
        if (!item)
            return;

        const bool selected    = (selectedItemId == item->id);
        const bool hasChildren = !item->children.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth
                                 | ImGuiTreeNodeFlags_OpenOnArrow
                                 | ImGuiTreeNodeFlags_DefaultOpen;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
        if (selected)     flags |= ImGuiTreeNodeFlags_Selected;

        const bool opened = ImGui::TreeNodeEx(
            reinterpret_cast<void *>(static_cast<intptr_t>(item->id)),
            flags, "%s", GetWidgetTreeLabel(item, canvas).c_str());

        if (ImGui::IsItemClicked())
            selectedItemId = item->id;

        if (opened)
        {
            for (const Ref<IWidgetItem> &child : item->children)
                DrawWidgetTreeRecursive(child, selectedItemId, canvas);

            ImGui::TreePop();
        }
    }

    WidgetContainer *WidgetEditor::ResolveInsertionParent(const Ref<IWidgetItem> &selectedItem, const Ref<WidgetCanvas> &widget)
    {
        if (!widget)
            return nullptr;

        if (selectedItem && selectedItem->GetWidgetType() == WidgetType::Container)
            return selectedItem->As<WidgetContainer>().get();

        if (selectedItem && selectedItem->parent && selectedItem->parent->GetWidgetType() == WidgetType::Container)
            return dynamic_cast<WidgetContainer *>(selectedItem->parent);

        return widget->GetRoot();
    }

    // =========================================================================
    // Toolbox
    // =========================================================================

    void WidgetEditor::DrawToolbox(const Ref<WidgetCanvas> &widget, int &selectedItemId)
    {
        (void)widget;
        (void)selectedItemId;

        struct ToolboxEntry
        {
            const char  *label;
            const char  *icon;
            WidgetType   type;
            ImVec4       color;
        };

        static const ToolboxEntry entries[] =
        {
            { "Container", "[ ]", WidgetType::Container, { 0.22f, 0.52f, 0.82f, 1.0f } },
            { "Button",    "[B]", WidgetType::Button,    { 0.18f, 0.62f, 0.38f, 1.0f } },
            { "Label",     "[T]", WidgetType::Label,     { 0.72f, 0.52f, 0.12f, 1.0f } },
        };

        ImGui::Spacing();
        ImGui::TextDisabled("Drag items onto the preview to add:");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        for (const ToolboxEntry &entry : entries)
        {
            const ImVec4 hovered = { entry.color.x + 0.12f, entry.color.y + 0.12f, entry.color.z + 0.12f, 1.0f };
            const ImVec4 active  = { entry.color.x - 0.08f, entry.color.y - 0.08f, entry.color.z - 0.08f, 1.0f };

            ImGui::PushStyleColor(ImGuiCol_Button,        entry.color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  active);
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

            const std::string btnLabel = std::format("  {}  {}", entry.icon, entry.label);
            ImGui::Button(btnLabel.c_str(), ImVec2(-1.0f, 34.0f));

            ImGui::PopStyleColor(4);

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const int typeInt = static_cast<int>(entry.type);
                ImGui::SetDragDropPayload(DND_WIDGET_TOOLBOX_ITEM, &typeInt, sizeof(int));

                ImGui::PushStyleColor(ImGuiCol_Text, entry.color);
                ImGui::Text("  %s  %s", entry.icon, entry.label);
                ImGui::PopStyleColor();

                ImGui::EndDragDropSource();
            }

            ImGui::Spacing();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Tip: Right-click the preview\nto add a widget at that position.");
    }

    // =========================================================================
    // Preview overlay  (bounds + corner handles)
    // =========================================================================

    void WidgetEditor::DrawPreviewOverlay(ImDrawList *drawList, const Ref<WidgetCanvas> &widget,
        int selectedItemId, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
    {
        if (!drawList || !widget || canvasW <= 0.0f || canvasH <= 0.0f)
            return;

        // Colour tokens
        constexpr ImU32 kDimOutline    = IM_COL32(100, 190, 255,  70);
        constexpr ImU32 kSelRect       = IM_COL32(  0, 200, 255, 230);
        constexpr ImU32 kHandleFill    = IM_COL32(255, 255, 255, 235);
        constexpr ImU32 kHandleBorder  = IM_COL32(  0, 160, 220, 255);
        constexpr float kHandleHalf    = 4.0f;

        for (const auto &[id, item] : widget->GetItems())
        {
            if (!item || !item->IsVisible())
                continue;

            const Rect   &wr = item->GetAlignedRect();
            const ImVec2  tl = CanvasToScreen(wr.min.x, wr.min.y, imagePos, imageSize, canvasW, canvasH);
            const ImVec2  br = CanvasToScreen(wr.max.x, wr.max.y, imagePos, imageSize, canvasW, canvasH);

            if (id == selectedItemId)
            {
                // Bright selection rectangle
                drawList->AddRect(tl, br, kSelRect, 0.0f, 0, 1.5f);

                // Eight resize handles: 4 corners + 4 edge midpoints
                const ImVec2 tr = { br.x, tl.y };
                const ImVec2 bl = { tl.x, br.y };
                const ImVec2 tc = { (tl.x + br.x) * 0.5f, tl.y };
                const ImVec2 bc = { (tl.x + br.x) * 0.5f, br.y };
                const ImVec2 ml = { tl.x, (tl.y + br.y) * 0.5f };
                const ImVec2 mr = { br.x, (tl.y + br.y) * 0.5f };

                const ImVec2 handles[] = { tl, tr, bl, br, tc, bc, ml, mr };
                for (const ImVec2 &h : handles)
                {
                    drawList->AddRectFilled(
                        { h.x - kHandleHalf, h.y - kHandleHalf },
                        { h.x + kHandleHalf, h.y + kHandleHalf },
                        kHandleFill);
                    drawList->AddRect(
                        { h.x - kHandleHalf, h.y - kHandleHalf },
                        { h.x + kHandleHalf, h.y + kHandleHalf },
                        kHandleBorder, 0.0f, 0, 1.0f);
                }
            }
            else
            {
                // Dim outline for non-selected items
                drawList->AddRect(tl, br, kDimOutline, 0.0f, 0, 1.0f);
            }
        }
    }

    // =========================================================================
    // Anchor-point visualization
    // =========================================================================

    void WidgetEditor::DrawAnchorPoints(ImDrawList *drawList, const Ref<IWidgetItem> &item, const Ref<WidgetCanvas> &widget,
        const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
    {
        if (!drawList || !item || !widget || canvasW <= 0.0f || canvasH <= 0.0f)
            return;

        // Find the parent rect we annotate anchors on
        Rect parentRect;
        if (item->parent)
        {
            parentRect = item->parent->GetAlignedRect();
        }
        else
        {
            // Root-level items — canvas itself is the "parent"
            parentRect = Rect(0.0f, 0.0f, canvasW, canvasH);
        }

        const WidgetAlignment activeAlign = item->alignment;

        constexpr ImU32  kInactive    = IM_COL32(200, 200, 200, 140);
        constexpr ImU32  kActive      = IM_COL32(255, 200,  50, 255);
        constexpr ImU32  kActiveLine  = IM_COL32(255, 200,  50, 100);
        constexpr ImU32  kParentTint  = IM_COL32( 80, 160, 255,  35);
        constexpr float  kDiamond     = 5.0f;

        // Faint tint on parent rect to show context
        {
            const ImVec2 ptl = CanvasToScreen(parentRect.min.x, parentRect.min.y, imagePos, imageSize, canvasW, canvasH);
            const ImVec2 pbr = CanvasToScreen(parentRect.max.x, parentRect.max.y, imagePos, imageSize, canvasW, canvasH);
            drawList->AddRectFilled(ptl, pbr, kParentTint);
            drawList->AddRect(ptl, pbr, IM_COL32(80, 160, 255, 90), 0.0f, 0, 1.0f);
        }

        // Draw all 9 anchor diamonds
        ImVec2 activeScreenPos = {};
        for (int i = 0; i < static_cast<int>(WidgetAlignment::COUNT); ++i)
        {
            const WidgetAlignment al = static_cast<WidgetAlignment>(i);
            const ImVec2  sp    = GetAlignmentScreenPos(al, parentRect, imagePos, imageSize, canvasW, canvasH);
            const bool    isAct = (al == activeAlign);
            const ImU32   color = isAct ? kActive : kInactive;
            const float   sz    = isAct ? kDiamond + 2.0f : kDiamond;

            // Diamond shape (rotated square)
            drawList->AddQuadFilled(
                { sp.x,      sp.y - sz },
                { sp.x + sz, sp.y      },
                { sp.x,      sp.y + sz },
                { sp.x - sz, sp.y      },
                color);

            // Thin outline on inactive diamonds for contrast
            if (!isAct)
            {
                drawList->AddQuad(
                    { sp.x,      sp.y - sz },
                    { sp.x + sz, sp.y      },
                    { sp.x,      sp.y + sz },
                    { sp.x - sz, sp.y      },
                    IM_COL32(255, 255, 255, 60), 1.0f);
            }

            if (isAct)
                activeScreenPos = sp;
        }

        // Draw a line from the active anchor to the item's centre
        const glm::vec2 itemCenter = item->GetAlignedRect().GetCenter();
        const ImVec2    itemScreenCenter = CanvasToScreen(itemCenter.x, itemCenter.y, imagePos, imageSize, canvasW, canvasH);
        drawList->AddLine(activeScreenPos, itemScreenCenter, kActiveLine, 1.2f);
    }

    // =========================================================================
    // Details panel
    // =========================================================================

    void WidgetEditor::DrawWidgetDetails(const Ref<WidgetCanvas> &widget, int &selectedItemId, AssetManager *assetManager)
    {
        if (!widget)
            return;

        Ref<IWidgetItem> selectedItem = nullptr;
        if (selectedItemId != 0 && widget->GetItems().contains(selectedItemId))
        {
            selectedItem = widget->GetItems().at(selectedItemId);
        }
        else
        {
            if (widget->GetRoot())
                selectedItem = widget->GetRoot()->As<IWidgetItem>();
            if (selectedItem)
                selectedItemId = selectedItem->id;
        }

        if (!selectedItem)
        {
            ImGui::TextDisabled("No widget selected.");
            return;
        }

        ImGui::PushID(selectedItem.get());

        bool dirty = false;

        // --- Name ---
        char nameBuffer[256] {};
        std::strncpy(nameBuffer, selectedItem->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            selectedItem->name = nameBuffer;
            dirty = true;
        }

        // --- Visibility ---
        bool visible = selectedItem->IsVisible();
        if (ImGui::Checkbox("Visible", &visible))
        {
            selectedItem->SetVisible(visible);
            dirty = true;
        }

        // --- Z-Index ---
        dirty |= ImGui::DragInt("Z-Index", &selectedItem->zIndex, 1.0f, 0, 9999,
            "%d", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Higher Z-Index renders on top of lower values.");

        ImGui::Separator();

        // --- Alignment ---
        int alignment = static_cast<int>(selectedItem->alignment);
        const char *alignmentNames[] =
        {
            "Top Left",    "Top Center",    "Top Right",
            "Center Left", "Center",        "Center Right",
            "Bottom Left", "Bottom Center", "Bottom Right"
        };
        if (ImGui::Combo("Alignment", &alignment, alignmentNames, IM_ARRAYSIZE(alignmentNames)))
        {
            selectedItem->alignment = static_cast<WidgetAlignment>(alignment);
            dirty = true;
        }

        // --- Sizing ---
        int sizingMode = static_cast<int>(selectedItem->sizingMode);
        const char *sizingModeNames[] = { "Default", "Expand To Parent" };
        if (ImGui::Combo("Sizing Mode", &sizingMode, sizingModeNames, IM_ARRAYSIZE(sizingModeNames)))
        {
            selectedItem->sizingMode = static_cast<SizingMode>(sizingMode);
            dirty = true;
        }

        // --- Transform ---
        // Position is controlled by the parent container's layout system.
        // Only allow editing position for items inside an Absolute layout container.
        WidgetContainer *parentContainer = nullptr;
        if (selectedItem->parent && selectedItem->parent->GetWidgetType() == WidgetType::Container)
            parentContainer = dynamic_cast<WidgetContainer *>(selectedItem->parent);

        const bool canEditPosition = parentContainer && parentContainer->layout == LayoutMode::Absolute;
        if (!canEditPosition)
        {
            ImGui::BeginDisabled();
            UI::DrawVec2Control("Position", selectedItem->position, 1.0f);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Position is controlled by the parent's layout. Switch parent to Absolute to edit.");
            ImGui::EndDisabled();
        }
        else
        {
            dirty |= UI::DrawVec2Control("Position", selectedItem->position, 1.0f);
        }
        if (selectedItem->sizingMode == SizingMode::ExpandToParent)
        {
            ImGui::BeginDisabled();
            UI::DrawVec2Control("Size", selectedItem->size, 1.0f, 1.0f);
            ImGui::EndDisabled();
        }
        else
        {
            dirty |= UI::DrawVec2Control("Size", selectedItem->size, 1.0f, 1.0f);
        }

        // --- Container-specific ---
        if (Ref<WidgetContainer> container = selectedItem->As<WidgetContainer>())
        {
            ImGui::PushID(container.get());
            ImGui::SeparatorText("Container");

            int layout = static_cast<int>(container->layout);
            const char *layoutNames[] = { "Horizontal", "Vertical", "Grid", "Absolute" };
            if (ImGui::Combo("Layout", &layout, layoutNames, IM_ARRAYSIZE(layoutNames)))
            {
                const LayoutMode newLayout = static_cast<LayoutMode>(layout);
                const LayoutMode oldLayout = container->layout;
                container->layout = newLayout;
                dirty = true;

                // If switching away from Absolute layout, clear manual positions on children
                if (oldLayout == LayoutMode::Absolute && newLayout != LayoutMode::Absolute)
                {
                    for (auto &child : container->children)
                    {
                        if (child)
                            child->position = glm::vec2(0.0f);
                    }
                }
            }

            dirty |= UI::DrawFloatControl("Padding", &container->padding, 0.5f, 0.0f, 512.0f);
            dirty |= UI::DrawFloatControl("Margin",  &container->margin,  0.5f, 0.0f, 512.0f);
            dirty |= UI::DrawFloatControl("Gap",     &container->gap,     0.5f, 0.0f, 512.0f);

            // Quick-add children from the details panel too
            if (ImGui::Button("+ Container##c"))
            {
                selectedItemId = widget->AddContainer(container.get());
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Button##c"))
            {
                selectedItemId = widget->AddButton(container.get(), "Button");
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Label##c"))
            {
                selectedItemId = widget->AddLabel(container.get(), "Label");
                dirty = true;
            }

            ImGui::PopID();
        }

        // --- Button-specific ---
        if (Ref<WidgetButton> button = selectedItem->As<WidgetButton>())
        {
            ImGui::SeparatorText("Button");

            AssetHandle fontHandle = button->GetFontHandle();
            const std::string fontName = (fontHandle == AssetHandle(0))
                ? "Drop Font Here"
                : assetManager->GetAssetDisplayName(fontHandle);
            ImGui::Button(std::format("Font: {}", fontName).c_str(), ImVec2(-1.0f, 0.0f));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                    {
                        const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                        if (assetManager->GetMetaData(dropped).type == AssetType::Font)
                        {
                            button->SetFontHandle(dropped);
                            dirty = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            char textBuffer[256] {};
            std::strncpy(textBuffer, button->GetText().c_str(), sizeof(textBuffer) - 1);
            if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer)))
            {
                button->SetText(textBuffer);
                dirty = true;
            }

            float textSize    = button->GetFontSize();
            float kerning     = button->GetKerning();
            float lineSpacing = button->GetLineSpacing();
            if (UI::DrawFloatControl("Text Size",    &textSize,    0.025f, 0.0f, 120.0f))  { button->SetFontSize(textSize);       dirty = true; }
            if (UI::DrawFloatControl("Kerning",      &kerning,     0.01f,  0.0f, FLT_MAX)) { button->SetKerning(kerning);         dirty = true; }
            if (UI::DrawFloatControl("Line Spacing", &lineSpacing, 0.01f, -FLT_MAX, FLT_MAX)) { button->SetLineSpacing(lineSpacing); dirty = true; }

            dirty |= ImGui::ColorEdit4("Normal Color",  &button->normalColor.x);
            dirty |= ImGui::ColorEdit4("Hover Color",   &button->hoverColor.x);
            dirty |= ImGui::ColorEdit4("Pressed Color", &button->pressedColor.x);
            dirty |= ImGui::ColorEdit4("Border Color",  &button->borderColor.x);

            AssetHandle imageHandle = button->GetImageHandle();
            const std::string imageName = (imageHandle == AssetHandle(0))
                ? "Drop Texture Here"
                : assetManager->GetAssetDisplayName(imageHandle);
            ImGui::Button(std::format("Image: {}", imageName).c_str(), ImVec2(-1.0f, 0.0f));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                    {
                        const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                        if (assetManager->GetMetaData(dropped).type == AssetType::Texture)
                        {
                            button->SetImageHandle(dropped);
                            dirty = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
        else if (Ref<WidgetLabel> label = selectedItem->As<WidgetLabel>())
        {
            ImGui::SeparatorText("Label");

            char textBuffer[256] {};
            std::strncpy(textBuffer, label->GetText().c_str(), sizeof(textBuffer) - 1);
            if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer)))
            {
                label->SetText(textBuffer);
                dirty = true;
            }

            glm::vec4 textColor = label->GetColor();
            if (ImGui::ColorEdit4("Color", &textColor.x))
            {
                label->SetColor(textColor);
                dirty = true;
            }

            float kerning     = label->GetKerning();
            float lineSpacing = label->GetLineSpacing();
            if (ImGui::DragFloat("Kerning",      &kerning,     0.01f)) { label->SetKerning(kerning);         dirty = true; }
            if (ImGui::DragFloat("Line Spacing", &lineSpacing, 0.01f)) { label->SetLineSpacing(lineSpacing); dirty = true; }

            float fontSize = label->GetFontSize();
            if (UI::DrawFloatControl("Font Size", &fontSize, 0.5f, 1.0f, 512.0f))
            {
                label->SetFontSize(fontSize);
                dirty = true;
            }

            AssetHandle fontHandle = label->GetFontHandle();
            const std::string fontName = (fontHandle == AssetHandle(0))
                ? "Drop Font Here"
                : assetManager->GetAssetDisplayName(fontHandle);
            ImGui::Button(std::format("Font: {}", fontName).c_str(), ImVec2(-1.0f, 0.0f));
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                    {
                        const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                        if (assetManager->GetMetaData(dropped).type == AssetType::Font)
                        {
                            label->SetFontHandle(dropped);
                            dirty = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        // --- Add Child / Remove ---
        WidgetContainer *insertParent = ResolveInsertionParent(selectedItem, widget);
        if (insertParent)
        {
            ImGui::SeparatorText("Add Child");
            if (ImGui::Button("+ Container##add")) { selectedItemId = widget->AddContainer(insertParent); dirty = true; }
            ImGui::SameLine();
            if (ImGui::Button("+ Button##add"))    { selectedItemId = widget->AddButton(insertParent, "Button"); dirty = true; }
            ImGui::SameLine();
            if (ImGui::Button("+ Label##add"))     { selectedItemId = widget->AddLabel(insertParent, "Label");  dirty = true; }
        }

        if (widget->GetRoot() && selectedItem->id != widget->GetRoot()->id)
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
            if (ImGui::Button("Remove Selected Item", ImVec2(-1.0f, 0.0f)))
            {
                if (widget->RemoveItem(selectedItem->id))
                {
                    selectedItemId = widget->GetRoot() ? widget->GetRoot()->id : 0;
                    dirty = true;
                }
            }
            ImGui::PopStyleColor();
        }

        if (dirty)
            widget->SetDirtyFlag(true);

        ImGui::PopID();
    }
}
