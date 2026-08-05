// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "widget_editor.hpp"
#include "states.hpp"
#include "ext/editor_ui.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "panels/asset_editor_data.hpp"

#include "editor_layer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/texture.hpp"

#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_container.hpp"
#include "ignite/graphics/ui/widget_button.hpp"
#include "ignite/graphics/ui/widget_label.hpp"
#include "ignite/graphics/ui/widget_image.hpp"

#include "ignite/core/string_utils.hpp"

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
                case WidgetType::Image:     return "Image";
                case WidgetType::BoxSizing: return "BoxSizing";
                case WidgetType::Overlay:   return "Overlay";
                default:                    return "Widget";
            }
        }

        static std::string GetWidgetTreeLabel(const Ref<IWidgetItem> &item, const WidgetCanvas *canvas)
        {
            if (!item)
                return "<null>";

            if (canvas && canvas->GetRoot() == item.get())
                return "[Canvas Root]";

            const std::string displayName = item->name.empty() ? GetWidgetTypeLabel(*item) : item->name;
            return std::format("[{}]", displayName);
        }

        // Returns the screen-space anchor position for the given alignment on a parent rect.
        static ImVec2 GetAlignmentScreenPos(VerticalAlignment VAlignment, HorizontalAlignment HAlignment,
            const Rect &rect, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
        {
            const float minX  = rect.min.x;
            const float minY  = rect.min.y;
            const float midX  = (rect.min.x + rect.max.x) * 0.5f;
            const float midY  = (rect.min.y + rect.max.y) * 0.5f;
            const float maxX  = rect.max.x;
            const float maxY  = rect.max.y;
            float cx = minX, cy = minY;

            switch (HAlignment)
            {
                case HorizontalAlignment::Left: cx = minX; break;
                case HorizontalAlignment::Center: cx = midX; break;
                case HorizontalAlignment::Right: cx = maxX; break;
                case HorizontalAlignment::ExpandHorizontally: cx = minX; break;
                default: break;
            }
        
            switch (VAlignment)
            {
                case VerticalAlignment::Top: cy = minY; break;
                case VerticalAlignment::Middle: cy = midY; break;
                case VerticalAlignment::Bottom: cy = maxY; break;
                case VerticalAlignment::ExpandVertically: cy = minY; break;
                default: break;
            }

            return WidgetEditor::CanvasToScreen(cx, cy, imagePos, imageSize, canvasW, canvasH);
        }
    }

    // =========================================================================
    // Public helpers
    // =========================================================================

    ImVec2 WidgetEditor::CanvasToScreen(float cx, float cy, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
    {
        if (canvasW <= 0.0f || canvasH <= 0.0f)
            return imagePos;

        return { imagePos.x + (cx / canvasW) * imageSize.x, imagePos.y + (cy / canvasH) * imageSize.y };
    }

    ImVec2 WidgetEditor::ScreenToCanvas(const ImVec2 &screenPos, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
    {
        if (imageSize.x <= 0.0f || imageSize.y <= 0.0f || canvasW <= 0.0f || canvasH <= 0.0f)
            return { 0.0f, 0.0f };

        const float u = (screenPos.x - imagePos.x) / imageSize.x;
        const float v = (screenPos.y - imagePos.y) / imageSize.y;
        return { u * canvasW, v * canvasH };
    }

    void WidgetEditor::DrawWidgetTreeRecursive(const Ref<IWidgetItem> &item, int &selectedItemId, const WidgetCanvas *canvas)
    {
        if (!item)
            return;

        const bool selected = (selectedItemId == item->id);
        const bool hasChildren = !item->children.empty();
        const bool isRoot = (canvas && canvas->GetRoot() && item.get() == canvas->GetRoot());
        bool reparent = false;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
        if (selected) flags |= ImGuiTreeNodeFlags_Selected;

        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<intptr_t>(item->id)), flags, "%s", GetWidgetTreeLabel(item, canvas).c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            selectedItemId = item->id;

        // Context Menu for right-click in hierarchy tree
        if (ImGui::BeginPopupContextItem())
        {
            selectedItemId = item->id;
            WidgetContainer *container = dynamic_cast<WidgetContainer *>(item.get());
            if (!container && item->parent)
                container = dynamic_cast<WidgetContainer *>(item->parent);

            if (ImGui::BeginMenu("Add Child"))
            {
                if (ImGui::MenuItem("Container"))
                {
                    selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddContainer(container);
                }
                if (ImGui::MenuItem("Button"))
                {
                    selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddButton(container, "Button");
                }
                if (ImGui::MenuItem("Label"))
                {
                    selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddLabel(container, "Label");
                }
                if (ImGui::MenuItem("Image"))
                {
                    selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddImage(container);
                }
                if (ImGui::MenuItem("BoxSizing"))
                {
                    selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddBoxSizing(container);
                }
                if (ImGui::MenuItem("Overlay"))
                {
                    selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddOverlay(container);
                }
                ImGui::EndMenu();
            }

            if (!isRoot)
            {
                ImGui::Separator();
                if (ImGui::MenuItem("Move Up"))
                {
                    const_cast<WidgetCanvas *>(canvas)->MoveItemUp(item->id);
                }
                if (ImGui::MenuItem("Move Down"))
                {
                    const_cast<WidgetCanvas *>(canvas)->MoveItemDown(item->id);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete"))
                {
                    if (const_cast<WidgetCanvas *>(canvas)->RemoveItem(item->id))
                    {
                        selectedItemId = canvas->GetRoot() ? canvas->GetRoot()->id : 0;
                    }
                }
            }

            ImGui::EndPopup();
        }

        // --- Drag source: allow drag-reparent / drag-reorder ---
        if (!isRoot)
        {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const int dragId = item->id;
                ImGui::SetDragDropPayload(DND_WIDGET_ITEM_REPARENT, &dragId, sizeof(int));
                ImGui::TextUnformatted(GetWidgetTreeLabel(item, canvas).c_str());
                ImGui::EndDragDropSource();
            }
        }

        // --- Drop target: accept toolbox items AND reorder/reparent payloads ---
        if (ImGui::BeginDragDropTarget())
        {
            auto ResolveContainer = [&](IWidgetItem *target) -> WidgetContainer *
            {
                if (auto *c = dynamic_cast<WidgetContainer *>(target))
                    return c;
                IWidgetItem *p = target ? target->parent : nullptr;
                while (p)
                {
                    if (auto *c = dynamic_cast<WidgetContainer *>(p))
                        return c;
                    p = p->parent;
                }
                return canvas ? canvas->GetRoot() : nullptr;
            };

            // Toolbox → tree
            if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(DND_WIDGET_TOOLBOX_ITEM))
            {
                if (p->Data && p->DataSize == sizeof(int))
                {
                    const WidgetType droppedType = static_cast<WidgetType>(*static_cast<const int *>(p->Data));
                    const bool sameType = (item->GetWidgetType() == droppedType);

                    WidgetContainer *container = nullptr;
                    if (!sameType)
                        container = dynamic_cast<WidgetContainer *>(item.get());
                    if (!container)
                        container = ResolveContainer(item->parent);

                    if (container)
                    {
                        switch (droppedType)
                        {
                            case WidgetType::Container:
                                selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddContainer(container); break;
                            case WidgetType::Button:
                                selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddButton(container, "Button"); break;
                            case WidgetType::Label:
                                selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddLabel(container, "Label"); break;
                            case WidgetType::Image:
                                selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddImage(container); break;
                            case WidgetType::BoxSizing:
                                selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddBoxSizing(container); break;
                            case WidgetType::Overlay:
                                selectedItemId = const_cast<WidgetCanvas *>(canvas)->AddOverlay(container); break;
                            default: break;
                        }
                        const_cast<WidgetCanvas *>(canvas)->SetDirtyFlag(true);
                    }

                    reparent = true;
                }
            }

            // Reorder / Reparent tree item
            if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(DND_WIDGET_ITEM_REPARENT))
            {
                if (p->Data && p->DataSize == sizeof(int))
                {
                    const int draggedId = *static_cast<const int *>(p->Data);
                    if (draggedId != item->id)
                    {
                        const ImVec2 itemMin = ImGui::GetItemRectMin();
                        const ImVec2 itemMax = ImGui::GetItemRectMax();
                        const float mouseY = ImGui::GetMousePos().y;
                        const float midY = (itemMin.y + itemMax.y) * 0.5f;

                        if (!isRoot && item->parent)
                        {
                            // Reorder relative to target item
                            const bool insertAfter = (mouseY > midY);
                            const_cast<WidgetCanvas *>(canvas)->ReorderItem(draggedId, item->id, insertAfter);
                        }
                        else
                        {
                            // Target is root container: reparent inside root
                            WidgetContainer *newParent = dynamic_cast<WidgetContainer *>(item.get());
                            if (newParent)
                            {
                                const_cast<WidgetCanvas *>(canvas)->ReparentItem(draggedId, newParent);
                            }
                        }
                        const_cast<WidgetCanvas *>(canvas)->SetDirtyFlag(true);
                        reparent = true;
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        bool isDeleting = false;
        if (opened)
        {
            if (!isDeleting && !reparent)
            {
                const std::vector<Ref<IWidgetItem>> childrenSnapshot = item->children;
                for (const Ref<IWidgetItem> &child : childrenSnapshot)
                    DrawWidgetTreeRecursive(child, selectedItemId, canvas);
            }

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
            { "Image",     "[I]", WidgetType::Image,     { 0.18f, 0.82f, 0.12f, 1.0f } },
            { "BoxSizing", "[X]", WidgetType::BoxSizing, { 0.60f, 0.20f, 0.80f, 1.0f } },
            { "Overlay",   "[O]", WidgetType::Overlay,   { 0.80f, 0.40f, 0.10f, 1.0f } },
        };

        ImGui::Spacing();

        // ----- Search filter -----
        static char s_ToolboxSearch[128] = {};
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##toolbox_search", "Search...", s_ToolboxSearch, sizeof(s_ToolboxSearch));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const bool hasFilter = s_ToolboxSearch[0] != '\0';

        for (const ToolboxEntry &entry : entries)
        {
            if (hasFilter && !strstr(stringutils::ToLower(entry.label).c_str(), stringutils::ToLower(s_ToolboxSearch).c_str()))
                continue;

            const ImVec4 hovered = { entry.color.x + 0.12f, entry.color.y + 0.12f, entry.color.z + 0.12f, 1.0f };
            const ImVec4 active  = { entry.color.x - 0.08f, entry.color.y - 0.08f, entry.color.z - 0.08f, 1.0f };

            ImGui::PushStyleColor(ImGuiCol_Button,        entry.color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  active);
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

            const std::string btnLabel = std::format("  {}  {}", entry.icon, entry.label);
            ImGui::Button(btnLabel.c_str(), ImVec2(-1.0f, 24.0f));

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
        ImGui::TextDisabled("Tip: Drag onto the tree or preview\nto add a widget at that position.");
    }

    // =========================================================================
    // Preview overlay  (bounds + corner handles)
    // =========================================================================
    void WidgetEditor::DrawPreviewOverlay(ImDrawList *drawList, const Ref<WidgetCanvas> &widget, int selectedItemId, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
    {
        if (!drawList || !widget || canvasW <= 0.0f || canvasH <= 0.0f)
            return;

        // Color tokens
        constexpr ImU32 kDimOutline = IM_COL32(100, 190, 255, 70);
        constexpr ImU32 kSelRect = IM_COL32(0, 200, 255, 230);
        constexpr ImU32 kHandleFill = IM_COL32(255, 255, 255, 235);
        constexpr ImU32 kHandleBorder = IM_COL32(0, 160, 220, 255);
        constexpr float kHandleHalf = 4.0f;

        for (const auto &[id, item] : widget->GetItems())
        {
            if (!item || !item->IsVisible())
                continue;

            const Rect &wr = item->GetAlignedRect();
            const ImVec2 tl = CanvasToScreen(wr.min.x, wr.min.y, imagePos, imageSize, canvasW, canvasH);
            const ImVec2 br = CanvasToScreen(wr.max.x, wr.max.y, imagePos, imageSize, canvasW, canvasH);

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
                    drawList->AddRectFilled({ h.x - kHandleHalf, h.y - kHandleHalf }, { h.x + kHandleHalf, h.y + kHandleHalf }, kHandleFill);
                    drawList->AddRect({ h.x - kHandleHalf, h.y - kHandleHalf }, { h.x + kHandleHalf, h.y + kHandleHalf }, kHandleBorder, 0.0f, 0, 1.0f);
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
    void WidgetEditor::DrawAnchorPoints(ImDrawList *drawList, const Ref<IWidgetItem> &item, const Ref<WidgetCanvas> &widget, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
    {
        if (!drawList || !item || !widget || canvasW <= 0.0f || canvasH <= 0.0f)
            return;

        Rect parentRect;
        if (item->parent)
        {
            parentRect = item->parent->GetAlignedRect();
        }
        else
        {
            parentRect = Rect(0.0f, 0.0f, canvasW, canvasH);
        }

        const VerticalAlignment activeVAlign = item->layout.verticalAlignment;
        const HorizontalAlignment activeHAlign = item->layout.horizontalAlignment;

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

        // Draw all anchor diamonds across vertical and horizontal alignment grid
        ImVec2 activeScreenPos = {};
        bool foundActive = false;
        for (int v = 0; v < static_cast<int>(VerticalAlignment::COUNT); ++v)
        {
            for (int h = 0; h < static_cast<int>(HorizontalAlignment::COUNT); ++h)
            {
                const VerticalAlignment VAlign = static_cast<VerticalAlignment>(v);
                const HorizontalAlignment HAlign = static_cast<HorizontalAlignment>(h);

                const ImVec2 screenPos = GetAlignmentScreenPos(VAlign, HAlign, parentRect, imagePos, imageSize, canvasW, canvasH);
                const bool isActive = (VAlign == activeVAlign && HAlign == activeHAlign);
                const ImU32 color = isActive ? kActive : kInactive;
                const float sz = isActive ? kDiamond + 2.0f : kDiamond;

                // Diamond shape (rotated square)
                drawList->AddQuadFilled(
                    { screenPos.x,      screenPos.y - sz },
                    { screenPos.x + sz, screenPos.y      },
                    { screenPos.x,      screenPos.y + sz },
                    { screenPos.x - sz, screenPos.y      },
                    color
                );

                // Thin outline on inactive diamonds for contrast
                if (!isActive)
                {
                    drawList->AddQuad(
                        { screenPos.x,      screenPos.y - sz },
                        { screenPos.x + sz, screenPos.y      },
                        { screenPos.x,      screenPos.y + sz },
                        { screenPos.x - sz, screenPos.y      },
                        IM_COL32(255, 255, 255, 60), 1.0f
                    );
                }

                if (isActive)
                {
                    activeScreenPos = screenPos;
                    foundActive = true;
                }
            }
        }

        // Draw a line from the active anchor to the item's center
        if (foundActive)
        {
            const glm::vec2 itemCenter = item->GetAlignedRect().GetCenter();
            const ImVec2 itemScreenCenter = CanvasToScreen(itemCenter.x, itemCenter.y, imagePos, imageSize, canvasW, canvasH);
            drawList->AddLine(activeScreenPos, itemScreenCenter, kActiveLine, 1.2f);
        }
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

        char nameBuffer[256] {};
        std::strncpy(nameBuffer, selectedItem->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            selectedItem->name = nameBuffer;
            dirty = true;
        }

        // --- Layout & Box Model ---
        if (ImGui::TreeNodeEx("##widget_layout", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Layout"))
        {
            if (UI::DrawVec2Control("Position", selectedItem->layout.position, 1.0f))
            {
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            int wMode = static_cast<int>(selectedItem->layout.widthMode);
            static const char *sizeModeNames[] = { "Auto", "Fixed", "Fill", "Percent" };
            if (UI::DrawComboBox("Width Mode", sizeModeNames, 4, &wMode))
            {
                selectedItem->layout.widthMode = static_cast<SizeMode>(wMode);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            if (selectedItem->layout.widthMode == SizeMode::Fixed || selectedItem->layout.widthMode == SizeMode::Percent)
            {
                if (UI::DrawFloatControl("Width", &selectedItem->layout.width, 1.0f, 0.0f, 10000.0f))
                {
                    selectedItem->MarkLayoutDirty();
                    dirty = true;
                }
            }

            int hMode = static_cast<int>(selectedItem->layout.heightMode);
            if (UI::DrawComboBox("Height Mode", sizeModeNames, 4, &hMode))
            {
                selectedItem->layout.heightMode = static_cast<SizeMode>(hMode);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            if (selectedItem->layout.heightMode == SizeMode::Fixed || selectedItem->layout.heightMode == SizeMode::Percent)
            {
                if (UI::DrawFloatControl("Height", &selectedItem->layout.height, 1.0f, 0.0f, 10000.0f))
                {
                    selectedItem->MarkLayoutDirty();
                    dirty = true;
                }
            }

            glm::vec2 minSz(selectedItem->layout.minWidth, selectedItem->layout.minHeight);
            if (UI::DrawVec2Control("Min Size", minSz, 1.0f))
            {
                selectedItem->layout.minWidth = minSz.x;
                selectedItem->layout.minHeight = minSz.y;
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            glm::vec2 maxSz(selectedItem->layout.maxWidth, selectedItem->layout.maxHeight);
            if (UI::DrawVec2Control("Max Size", maxSz, 1.0f))
            {
                selectedItem->layout.maxWidth = maxSz.x;
                selectedItem->layout.maxHeight = maxSz.y;
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            if (UI::DrawVec4Control("Margin (T/R/B/L)", selectedItem->layout.margin, 1.0f))
            {
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            if (UI::DrawVec4Control("Padding (T/R/B/L)", selectedItem->layout.padding, 1.0f))
            {
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            int posType = static_cast<int>(selectedItem->layout.positionType);
            static const char *posTypeNames[] = { "Relative", "Absolute", "Fixed" };
            if (UI::DrawComboBox("Position Type", posTypeNames, 3, &posType))
            {
                selectedItem->layout.positionType = static_cast<PositionType>(posType);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            int overflow = static_cast<int>(selectedItem->layout.overflow);
            static const char *overflowNames[] = { "Visible", "Hidden", "Clip", "Scroll" };
            if (UI::DrawComboBox("Overflow", overflowNames, 4, &overflow))
            {
                selectedItem->layout.overflow = static_cast<OverflowMode>(overflow);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            int visibility = static_cast<int>(selectedItem->layout.visibility);
            static const char *visNames[] = { "Visible", "Hidden", "Collapsed" };
            if (UI::DrawComboBox("Visibility", visNames, 3, &visibility))
            {
                selectedItem->layout.visibility = static_cast<VisibilityMode>(visibility);
                selectedItem->MarkLayoutDirty();
                selectedItem->MarkPaintDirty();
                dirty = true;
            }

            int VAlignment = static_cast<int>(selectedItem->layout.verticalAlignment);
            static std::array<const char *, 4> VAlignmentNames = { "Top", "Middle", "Bottom", "Expand Vertically" };
            if (UI::DrawComboBox("V-Align", VAlignmentNames.data(), static_cast<int>(VAlignmentNames.size()), &VAlignment))
            {
                selectedItem->layout.verticalAlignment = static_cast<VerticalAlignment>(VAlignment);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            int HAlignment = static_cast<int>(selectedItem->layout.horizontalAlignment);
            static std::array<const char *, 4> HAlignmentNames = { "Left", "Center", "Right", "Expand Horizontally" };
            if (UI::DrawComboBox("H-Align", HAlignmentNames.data(), static_cast<int>(HAlignmentNames.size()), &HAlignment))
            {
                selectedItem->layout.horizontalAlignment = static_cast<HorizontalAlignment>(HAlignment);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            ImGui::TreePop();
        }

        // --- Flex Properties ---
        const bool isContainer = (selectedItem->GetWidgetType() == WidgetType::Container || selectedItem->GetWidgetType() == WidgetType::BoxSizing || selectedItem->GetWidgetType() == WidgetType::Overlay);
        const bool parentIsContainer = (selectedItem->parent && selectedItem->parent->GetWidgetType() == WidgetType::Container);

        if (isContainer || parentIsContainer)
        {
            if (ImGui::TreeNodeEx("##widget_flex", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Flex Properties"))
            {
                if (isContainer)
                {
                    int dir = static_cast<int>(selectedItem->layout.flex.direction);
                    static const char *flexDirNames[] = { "Row", "Column" };
                    if (UI::DrawComboBox("Direction", flexDirNames, 2, &dir))
                    {
                        selectedItem->layout.flex.direction = static_cast<FlexDirection>(dir);
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }

                    int justify = static_cast<int>(selectedItem->layout.flex.justifyContent);
                    static const char *justifyNames[] = { "Start", "Center", "End", "Space Between", "Space Around" };
                    if (UI::DrawComboBox("Justify Content", justifyNames, 5, &justify))
                    {
                        selectedItem->layout.flex.justifyContent = static_cast<JustifyContent>(justify);
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }

                    int align = static_cast<int>(selectedItem->layout.flex.alignItems);
                    static const char *alignItemsNames[] = { "Start", "Center", "End", "Stretch" };
                    if (UI::DrawComboBox("Align Items", alignItemsNames, 4, &align))
                    {
                        selectedItem->layout.flex.alignItems = static_cast<AlignItems>(align);
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }

                    int wrap = static_cast<int>(selectedItem->layout.flex.wrap);
                    static const char *wrapNames[] = { "No Wrap", "Wrap" };
                    if (UI::DrawComboBox("Wrap", wrapNames, 2, &wrap))
                    {
                        selectedItem->layout.flex.wrap = static_cast<FlexWrap>(wrap);
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }

                    if (UI::DrawFloatControl("Gap", &selectedItem->layout.flex.gap, 0.5f, 0.0f, 512.0f))
                    {
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }
                }

                int selfAlign = static_cast<int>(selectedItem->layout.flex.alignSelf);
                static const char *alignSelfNames[] = { "Auto", "Start", "Center", "End", "Stretch" };
                if (UI::DrawComboBox("Align Self", alignSelfNames, 5, &selfAlign))
                {
                    selectedItem->layout.flex.alignSelf = static_cast<AlignSelf>(selfAlign);
                    selectedItem->MarkLayoutDirty();
                    dirty = true;
                }

                if (UI::DrawFloatControl("Grow", &selectedItem->layout.flex.grow, 0.1f, 0.0f, 100.0f))
                {
                    selectedItem->MarkLayoutDirty();
                    dirty = true;
                }

                if (UI::DrawFloatControl("Shrink", &selectedItem->layout.flex.shrink, 0.1f, 0.0f, 100.0f))
                {
                    selectedItem->MarkLayoutDirty();
                    dirty = true;
                }

                if (UI::DrawFloatControl("Basis (-1 Auto)", &selectedItem->layout.flex.basis, 1.0f, -1.0f, 10000.0f))
                {
                    selectedItem->MarkLayoutDirty();
                    dirty = true;
                }

                ImGui::TreePop();
            }
        }

        // --- UI Style ---
        if (ImGui::TreeNodeEx("##widget_uistyle", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "UI Style"))
        {
            if (UI::DrawColorVec4("Background Color", selectedItem->baseStyle.backgroundColor))
            {
                selectedItem->MarkPaintDirty();
                dirty = true;
                if (Ref<WidgetButton> btn = selectedItem->As<WidgetButton>())
                {
                    btn->style.color = selectedItem->baseStyle.backgroundColor;
                }
            }

            if (UI::DrawColorVec4("Border Color", selectedItem->baseStyle.borderColor))
            {
                selectedItem->MarkPaintDirty();
                dirty = true;
            }

            if (UI::DrawFloatControl("Border Width", &selectedItem->baseStyle.borderWidth, 0.5f, 0.0f, 100.0f))
            {
                selectedItem->MarkLayoutDirty();
                selectedItem->MarkPaintDirty();
                dirty = true;
            }

            if (UI::DrawFloatControl("Corner Radius", &selectedItem->baseStyle.cornerRadius, 0.5f, 0.0f, 100.0f))
            {
                selectedItem->MarkPaintDirty();
                dirty = true;
            }

            if (UI::DrawFloatControl("Opacity", &selectedItem->baseStyle.opacity, 0.02f, 0.0f, 1.0f))
            {
                selectedItem->MarkPaintDirty();
                dirty = true;
            }

            ImGui::TreePop();
        }

        // Z-Index ordering
        if (ImGui::TreeNodeEx("##widget_order", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Order"))
        {
            dirty |= UI::DrawIntControl("Z-Index", &selectedItem->zIndex, 1.0f, 0, 9999, 0);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Higher Z-Index renders on top of lower values.");
            }
            ImGui::TreePop();
        }

        // --- Button-specific ---
        if (Ref<WidgetButton> button = selectedItem->As<WidgetButton>())
        {
            if (ImGui::TreeNodeEx("##widget_button_appearance", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Button Style"))
            {
                if (UI::DrawColorVec4("Color", button->style.color))
                {
                    button->baseStyle.backgroundColor = button->style.color;
                    button->MarkPaintDirty();
                    dirty = true;
                }
                dirty |= UI::DrawColorVec4("Hover Color", button->style.hoverColor);
                dirty |= UI::DrawColorVec4("Pressed Color", button->style.pressedColor);
                dirty |= UI::DrawColorVec4("Border Color", button->style.borderColor);
                dirty |= UI::DrawFloatControl("Border Width", &button->style.borderWidth, 0.5f, 0.0f, 100.0f);
                dirty |= UI::DrawFloatControl("Corner Radius", &button->style.cornerRadius, 0.5f, 0.0f, 100.0f);

                bool isImageLoaded = button->imageHandle != AssetHandle(0);
                const std::string imageName = isImageLoaded ? assetManager->GetAssetDisplayName(button->imageHandle) : "Drop Texture Here";
                UI::DrawButtonWithColumn("Image", std::format("Image: {}", imageName).c_str(), nullptr, [assetManager, button, &dirty, &isImageLoaded]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                            {
                                const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                                if (assetManager->GetMetaData(dropped).type == AssetType::Texture)
                                {
                                    button->imageHandle = dropped;
                                    dirty = true;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isImageLoaded)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("X##ClearImage"))
                        {
                            button->imageHandle = AssetHandle(0);
                            button->image = nullptr;
                        }
                    }
                });

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("##widget_button_label", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Label"))
            {
                dirty |= DrawWidgetLabel(button->label.get(), assetManager);
                ImGui::TreePop();
            }
        }
        else if (Ref<WidgetLabel> label = selectedItem->As<WidgetLabel>())
        {
            if (ImGui::TreeNodeEx("##widget_label_props", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Label"))
            {
                dirty |= DrawWidgetLabel(label.get(), assetManager);
                ImGui::TreePop();
            }
        }
        else if (Ref<WidgetImage> img = selectedItem->As<WidgetImage>())
        {
            if (ImGui::TreeNodeEx("##widget_image_props", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Image"))
            {
                dirty |= DrawWidgetImage(img.get(), assetManager);
                ImGui::TreePop();
            }
        }

        WidgetContainer *insertParent = ResolveInsertionParent(selectedItem, widget);

        if (dirty)
        {
            widget->SetDirtyFlag(true);
        }

        ImGui::PopID();
    }

    // Draw Funcs
    bool WidgetEditor::DrawWidgetLabel(WidgetLabel *label, AssetManager *assetManager)
    {
        bool dirty = false;

        AssetHandle fontHandle = label->fontHandle;
        const bool isFontLoaded = fontHandle != AssetHandle(0);
        const std::string fontName = isFontLoaded ? assetManager->GetAssetDisplayName(fontHandle) : "Drop Font Here";
        UI::DrawButtonWithColumn("Font", std::format("Font: {}", fontName).c_str(), nullptr, [assetManager, label, &dirty, isFontLoaded]()
        {
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                    {
                        const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                        if (assetManager->GetMetaData(dropped).type == AssetType::Font)
                        {
                            label->fontHandle = dropped;
                            dirty = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (isFontLoaded)
            {
                ImGui::SameLine();
                if (ImGui::Button("X##ClearFont"))
                {
                    label->fontHandle = AssetHandle(0);
                }
            }
        });

        if (isFontLoaded)
        {
            char textBuffer[256] {};
            std::strncpy(textBuffer, label->text.c_str(), sizeof(textBuffer) - 1);
            if (ImGui::InputText("Text", textBuffer, sizeof(textBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                label->text = textBuffer;
                dirty = true;
            }

            dirty |= UI::DrawFloatControl("Text Size", &label->style.fontSize, 0.025f, 0.0f, 120.0f);
            dirty |= UI::DrawFloatControl("Kerning", &label->style.kerning, 0.01f, 0.0f, FLT_MAX);
            dirty |= UI::DrawFloatControl("Line Spacing", &label->style.lineSpacing, 0.01f, -FLT_MAX, FLT_MAX);
        }

        return dirty;
    }

    void WidgetEditor::UIWidgetEditor(UI::AssetEditorData &assetData, EditorLayer *editorLayer)
    {
        if (!assetData.asset || !assetData.asset->IsReady())
        {
            auto assetManager = AssetManager::GetInstance();
            assetData.asset = assetManager ? assetManager->GetAsset(assetData.handle) : nullptr;
            if (!assetData.asset || (assetData.asset && !assetData.asset->IsReady()))
            {
                UI::DrawCenteredText("Loading asset...");
                return;
            }
        }

        if (Ref<WidgetCanvas> widget = assetData.asset->As<WidgetCanvas>())
        {
			auto assetManager = AssetManager::GetInstance();

            UI::EditorSceneData &sceneData = assetData.sceneData;

            widget->CreateRoot(sceneData.viewportWidth > 0 ? sceneData.viewportWidth : 1280, sceneData.viewportHeight > 0 ? sceneData.viewportHeight : 720);
            if (widget->GetRoot() && widget->GetRoot()->name.empty())
                widget->GetRoot()->name = "Canvas Root";

            // ---- Per-asset persistent state ----
            static std::unordered_map<uint64_t, int>       s_SelectedWidgetItem;
            static std::unordered_map<uint64_t, float>     s_WidgetPreviewZoom;
            static std::unordered_map<uint64_t, glm::vec2> s_WidgetPreviewPan;
            static std::unordered_map<uint64_t, int>       s_WidgetPreviewAspect;
            static std::unordered_map<uint64_t, glm::vec2> s_WidgetPreviewViewportSize;

            const auto stateKey = static_cast<uint64_t>(assetData.handle);

            int &selectedItemId = s_SelectedWidgetItem[stateKey];
            if (selectedItemId == 0 || !widget->GetItems().contains(selectedItemId))
                selectedItemId = widget->GetRoot() ? widget->GetRoot()->id : 0;

            float &widgetPreviewZoom = s_WidgetPreviewZoom[stateKey];
            if (widgetPreviewZoom <= 0.0f) widgetPreviewZoom = 1.0f;
            glm::vec2 &widgetPreviewPan = s_WidgetPreviewPan[stateKey];
            int &widgetPreviewAspect = s_WidgetPreviewAspect[stateKey];
            glm::vec2 &widgetPreviewViewportSize = s_WidgetPreviewViewportSize[stateKey];
            if (widgetPreviewViewportSize.x <= 0.0f || widgetPreviewViewportSize.y <= 0.0f)
                widgetPreviewViewportSize = { 1920.0f, 1080.0f };

            if (sceneData.sceneRenderer)
                sceneData.sceneRenderer->SetPreviewWidget(widget);

            // Resolve selected item once (used by multiple sections)
            Ref<IWidgetItem> selectedItem = nullptr;
            if (selectedItemId != 0 && widget->GetItems().contains(selectedItemId))
                selectedItem = widget->GetItems().at(selectedItemId);

            // LEFT PANEL — Toolbox + Layout Tree
            if (ImGui::BeginChild("##widget_layout_toolbox_tree", { 250.0f, 0.0f }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
            {
                ImGui::BeginChild("##widget_layout_toolbox", { 0.0f, 250.0f }, ImGuiChildFlags_ResizeY);
                {
                    if (ImGui::BeginTabBar("##layout_toolbox_tab_bar"))
                    {
                        if (ImGui::BeginTabItem("Toolbox"))
                        {
                            WidgetEditor::DrawToolbox(widget, selectedItemId);
                            ImGui::EndTabItem();
                        }

                        ImGui::EndTabBar();
                    }
                }
                ImGui::EndChild(); // !Toolbox

                // Tree - bottom
                ImGui::BeginChild("##widget_layout_tree", { 0.0f, 0.0f });
                {
                    if (ImGui::BeginTabBar("##layout_tree_tab_bar"))
                    {
                        if (ImGui::BeginTabItem("Layout Tree"))
                        {
                            if (widget->GetRoot())
                                WidgetEditor::DrawWidgetTreeRecursive(widget->GetRoot()->As<IWidgetItem>(), selectedItemId, widget.get());
                            else
                                ImGui::TextDisabled("No root container.");
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                }
                ImGui::EndChild(); // !Tree
            }
            ImGui::EndChild(); // !Left panel

            ImGui::SameLine();

            // MIDDLE PANEL — Scene Preview
            ImGui::BeginChild("##widget_scene_preview", { 0.0f, 0.0f }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX, ImGuiWindowFlags_NoScrollbar);
            {
                static const char *aspectRatioLabels[] = { "Free", "16:9", "16:10", "4:3", "21:9", "1:1" };
                static const float aspectRatioValues[] = { 0.0f, 16.0f / 9.0f, 16.0f / 10.0f, 4.0f / 3.0f, 21.0f / 9.0f, 1.0f };

                widgetPreviewAspect = std::clamp(widgetPreviewAspect, 0, static_cast<int>(IM_ARRAYSIZE(aspectRatioLabels)) - 1);
                ImGui::SetNextItemWidth(140.0f);
                if (ImGui::Combo("Aspect Ratio", &widgetPreviewAspect, aspectRatioLabels, IM_ARRAYSIZE(aspectRatioLabels)))
                {
                    if (widgetPreviewAspect > 0)
                    {
                        const float aspect = aspectRatioValues[widgetPreviewAspect];
                        widgetPreviewViewportSize.y = std::max(widgetPreviewViewportSize.x / aspect, 1.0f);
                    }
                }

                ImGui::SetNextItemWidth(180.0f);
                ImGui::SameLine();
                if (ImGui::InputFloat2("Viewport Size", &widgetPreviewViewportSize.x, "%.0f"))
                {
                    widgetPreviewViewportSize.x = std::max(widgetPreviewViewportSize.x, 1.0f);
                    widgetPreviewViewportSize.y = std::max(widgetPreviewViewportSize.y, 1.0f);
                    widgetPreviewAspect = 0;
                }
                widgetPreviewViewportSize.x = std::max(widgetPreviewViewportSize.x, 1.0f);
                widgetPreviewViewportSize.y = std::max(widgetPreviewViewportSize.y, 1.0f);

                const ImVec2 previewRegionSize = ImGui::GetContentRegionAvail();
                const ImVec2 previewRegionPos = ImGui::GetCursorScreenPos();
                const float selectedAspect = aspectRatioValues[widgetPreviewAspect];

                ImVec2 viewportSize = { widgetPreviewViewportSize.x, widgetPreviewViewportSize.y };
                if (selectedAspect > 0.0f)
                {
                    viewportSize.y = std::max(viewportSize.x / selectedAspect, 1.0f);
                }

                viewportSize.x = std::max(viewportSize.x, 1.0f);
                viewportSize.y = std::max(viewportSize.y, 1.0f);

                const ImVec2 viewportPos =
                {
                    previewRegionPos.x + std::max((previewRegionSize.x - viewportSize.x) * 0.5f, 0.0f),
                    previewRegionPos.y + std::max((previewRegionSize.y - viewportSize.y) * 0.5f, 0.0f)
                };

                sceneData.viewportWidth  = std::max(1u, static_cast<uint32_t>(viewportSize.x + 0.5f));
                sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y + 0.5f));

                const float canvasW = static_cast<float>(sceneData.viewportWidth);
                const float canvasH = static_cast<float>(sceneData.viewportHeight);

                if (sceneData.compositeRT)
                {
                    Ref<Texture> previewTexture = sceneData.compositeRT->GetColorAttachment(0);

                    const std::string previewBtnId = std::format("##widget_preview_{}", stateKey);
                    ImGui::SetCursorScreenPos(previewRegionPos);
                    ImGui::InvisibleButton(previewBtnId.c_str(), previewRegionSize);
                    sceneData.viewportHovered = ImGui::IsItemHovered();
                    const ImVec2 mousePos = ImGui::GetMousePos();

                    // Zoom/pan calculations
                    const float previousZoom = widgetPreviewZoom;
                    if (sceneData.viewportHovered && ImGui::GetIO().MouseWheel != 0.0f)
                    {
                        widgetPreviewZoom = std::clamp(widgetPreviewZoom + ImGui::GetIO().MouseWheel * 0.1f, 0.25f, 5.0f);
                        if (widgetPreviewZoom != previousZoom)
                        {
                            const ImVec2 prevImagePos  = { viewportPos.x + widgetPreviewPan.x, viewportPos.y + widgetPreviewPan.y };
                            const ImVec2 prevImageSize = { viewportSize.x * previousZoom, viewportSize.y * previousZoom };
                            const ImVec2 uvAtMouse =
                            {
                                (mousePos.x - prevImagePos.x) / std::max(prevImageSize.x, 1.0f),
                                (mousePos.y - prevImagePos.y) / std::max(prevImageSize.y, 1.0f)
                            };
                            const ImVec2 newImageSize = { viewportSize.x * widgetPreviewZoom, viewportSize.y * widgetPreviewZoom };
                            widgetPreviewPan.x = mousePos.x - viewportPos.x - uvAtMouse.x * newImageSize.x;
                            widgetPreviewPan.y = mousePos.y - viewportPos.y - uvAtMouse.y * newImageSize.y;
                        }
                    }

                    if (sceneData.viewportHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                    {
                        widgetPreviewPan.x += ImGui::GetIO().MouseDelta.x;
                        widgetPreviewPan.y += ImGui::GetIO().MouseDelta.y;
                    }

                    const ImVec2 imagePos  = { viewportPos.x + widgetPreviewPan.x, viewportPos.y + widgetPreviewPan.y };
                    const ImVec2 scaledImageSize = { viewportSize.x * widgetPreviewZoom, viewportSize.y * widgetPreviewZoom };

                    // Convert screen mouse position to exact canvas space taking zoom & pan into account
                    const ImVec2 canvasMouse = ScreenToCanvas(mousePos, imagePos, scaledImageSize, canvasW, canvasH);
                    const bool isMouseOverCanvas = (mousePos.x >= imagePos.x && mousePos.x <= imagePos.x + scaledImageSize.x &&
                                                    mousePos.y >= imagePos.y && mousePos.y <= imagePos.y + scaledImageSize.y);

                    if (sceneData.sceneRenderer)
                    {
                        const float localMouseX = std::clamp(canvasMouse.x, 0.0f, std::max(canvasW - 1.0f, 0.0f));
                        const float localMouseY = std::clamp(canvasMouse.y, 0.0f, std::max(canvasH - 1.0f, 0.0f));
                        sceneData.sceneRenderer->SetPreviewMouseState(
                            static_cast<uint32_t>(localMouseX),
                            static_cast<uint32_t>(localMouseY),
                            sceneData.viewportHovered && isMouseOverCanvas);
                    }

                    // Click in scene preview to select widget
                    if (sceneData.viewportHovered && isMouseOverCanvas && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemActive() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    {
                        int hitId = 0;
                        float smallestArea = FLT_MAX;

                        for (const auto &[id, item] : widget->GetItems())
                        {
                            if (!item || !item->IsVisible())
                                continue;

                            const Rect &r = item->GetAlignedRect();
                            if (r.Contains(glm::vec2(canvasMouse.x, canvasMouse.y)))
                            {
                                const float area = r.GetSize().x * r.GetSize().y;
                                if (area < smallestArea)
                                {
                                    smallestArea = area;
                                    hitId = id;
                                }
                            }
                        }

                        if (hitId != 0)
                        {
                            selectedItemId = hitId;
                        }
                    }

                    // Drag & drop onto preview
                    if (ImGui::BeginDragDropTarget())
                    {
                        // Drop Widget asset from Content Browser
                        if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (p->Data && p->DataSize == sizeof(AssetHandle) && assetManager)
                            {
                                const AssetHandle dropped = *static_cast<const AssetHandle *>(p->Data);
                                if (assetManager->GetMetaData(dropped).type == AssetType::Widget)
                                {
                                    WidgetChildEntry entry;
                                    entry.handle = dropped;
                                    entry.enabled = true;
                                    entry.blockWidgetsBelow = false;
                                    widget->GetChildWidgets().push_back(entry);
                                    widget->SetDirtyFlag(true);
                                }
                            }
                        }

                        // Drop item from Toolbox directly onto preview canvas
                        if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(DND_WIDGET_TOOLBOX_ITEM))
                        {
                            if (p->Data && p->DataSize == sizeof(int))
                            {
                                const WidgetType droppedType = static_cast<WidgetType>(*static_cast<const int *>(p->Data));
                                WidgetContainer *container = ResolveInsertionParent(selectedItem, widget);
                                if (container)
                                {
                                    int newId = -1;
                                    switch (droppedType)
                                    {
                                        case WidgetType::Container:
                                            newId = widget->AddContainer(container); break;
                                        case WidgetType::Button:
                                            newId = widget->AddButton(container, "Button"); break;
                                        case WidgetType::Label:
                                            newId = widget->AddLabel(container, "Label"); break;
                                        case WidgetType::Image:
                                            newId = widget->AddImage(container); break;
                                        case WidgetType::BoxSizing:
                                            newId = widget->AddBoxSizing(container); break;
                                        case WidgetType::Overlay:
                                            newId = widget->AddOverlay(container); break;
                                        default: break;
                                    }
                                    if (newId != -1)
                                    {
                                        selectedItemId = newId;
                                        if (widget->GetItems().contains(newId))
                                        {
                                            auto newItem = widget->GetItems().at(newId);
                                            if (newItem)
                                            {
                                                newItem->SetPosition(glm::vec2(canvasMouse.x, canvasMouse.y));
                                            }
                                        }
                                        widget->SetDirtyFlag(true);
                                    }
                                }
                            }
                        }

                        ImGui::EndDragDropTarget();
                    }

                    ImDrawList *drawList = ImGui::GetWindowDrawList();
                    const ImVec2 clipMax = { previewRegionPos.x + previewRegionSize.x, previewRegionPos.y + previewRegionSize.y };
                    drawList->PushClipRect(previewRegionPos, clipMax, true);

                    drawList->AddImage(reinterpret_cast<ImTextureID>(previewTexture->GetHandle().Get()), imagePos, { imagePos.x + scaledImageSize.x, imagePos.y + scaledImageSize.y }, { 0.0f, 0.0f }, { 1.0f, 1.0f });
                    WidgetEditor::DrawPreviewOverlay(drawList, widget, selectedItemId, imagePos, scaledImageSize, canvasW, canvasH);

                    if (selectedItem && widget->GetRoot() && selectedItem->id != widget->GetRoot()->id)
                    {
                        WidgetEditor::DrawAnchorPoints(drawList, selectedItem, widget, imagePos, scaledImageSize, canvasW, canvasH);
                    }

                    drawList->PopClipRect();
                }
                else
                {
                    ImGui::Dummy(previewRegionSize);
                }
            }
            ImGui::EndChild();

            ImGui::SameLine(0.0f, 0.0f);

            // RIGHT PANEL — Details
            ImGui::BeginChild("##widget_details", { 0.0f, 0.0f }, ImGuiChildFlags_Borders);
            {
                if (ImGui::BeginTabBar("##widget_details_tab_bar"))
                {
                    if (ImGui::BeginTabItem("Details"))
                    {
                        WidgetEditor::DrawWidgetDetails(widget, selectedItemId, assetManager);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndChild();
        }
        else
        {
            ImGui::Text("Invalid asset!");
        }
    }

    bool WidgetEditor::DrawWidgetImage(WidgetImage *image, AssetManager *assetManager)
    {
        if (!image || !assetManager)
            return false;

        bool dirty = false;

        const bool isLoaded = image->imageHandle != AssetHandle(0);
        const std::string imageName = isLoaded
            ? assetManager->GetAssetDisplayName(image->imageHandle)
            : "Drop Texture Here";

        UI::DrawButtonWithColumn("Image", std::format("Image: {}", imageName).c_str(), nullptr,
            [assetManager, image, &dirty, &isLoaded]()
        {
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                    {
                        const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                        if (assetManager->GetMetaData(dropped).type == AssetType::Texture)
                        {
                            image->imageHandle = dropped;
                            image->image = nullptr; // will be resolved next frame
                            dirty = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (isLoaded)
            {
                ImGui::SameLine();
                if (ImGui::Button("X##ClearWidgetImage"))
                {
                    image->imageHandle = AssetHandle(0);
                    image->image = nullptr;
                    dirty = true;
                }
            }
        });

        return dirty;
    }

}
