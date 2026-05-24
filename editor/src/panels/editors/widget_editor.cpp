// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "widget_editor.hpp"
#include "states.hpp"
#include "ext/editor_ui.hpp"
#include "ignite/asset/asset_manager.hpp"

#include "editor_layer.hpp"
#include "panels/asset_editor_panel.hpp"
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
        return { imagePos.x + (cx / canvasW) * imageSize.x, imagePos.y + (cy / canvasH) * imageSize.y };
    }

    void WidgetEditor::DrawWidgetTreeRecursive(const Ref<IWidgetItem> &item, int &selectedItemId, const WidgetCanvas *canvas)
    {
        if (!item)
            return;

        const bool selected = (selectedItemId == item->id);
        const bool hasChildren = !item->children.empty();
        bool reparent = false;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
        if (selected) flags |= ImGuiTreeNodeFlags_Selected;

        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<intptr_t>(item->id)), flags, "%s", GetWidgetTreeLabel(item, canvas).c_str());

        if (ImGui::IsItemClicked())
            selectedItemId = item->id;

        // --- Drag source: allow reparenting by dragging a tree node ---
        if (!canvas || !canvas->GetRoot() || item.get() != canvas->GetRoot())
        {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const int dragId = item->id;
                ImGui::SetDragDropPayload(DND_WIDGET_ITEM_REPARENT, &dragId, sizeof(int));
                ImGui::TextUnformatted(GetWidgetTreeLabel(item, canvas).c_str());
                ImGui::EndDragDropSource();
            }
        }

        // --- Drop target: accept toolbox items AND reparenting payloads ---
        if (ImGui::BeginDragDropTarget())
        {
            // Helper: walk up to the nearest WidgetContainer ancestor (including self).
            // Returns nullptr if none found.
            // NOTE: we must store the Ref<> to keep the object alive while we hold the raw ptr.
            auto ResolveContainer = [&](IWidgetItem *target) -> WidgetContainer *
            {
                // Is target itself a container?
                if (auto *c = dynamic_cast<WidgetContainer *>(target))
                    return c;
                // Walk up parents
                IWidgetItem *p = target ? target->parent : nullptr;
                while (p)
                {
                    if (auto *c = dynamic_cast<WidgetContainer *>(p))
                        return c;
                    p = p->parent;
                }
                // Fallback to canvas root
                return canvas ? canvas->GetRoot() : nullptr;
            };

            // Toolbox → tree
            if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(DND_WIDGET_TOOLBOX_ITEM))
            {
                if (p->Data && p->DataSize == sizeof(int))
                {
                    const WidgetType droppedType = static_cast<WidgetType>(*static_cast<const int *>(p->Data));

                    // Rule 1: do not add the same type onto the same type
                    const bool sameType = (item->GetWidgetType() == droppedType);

                    // Resolve container: if target is a container of a DIFFERENT type, add inside it;
                    // otherwise add to its nearest container ancestor.
                    WidgetContainer *container = nullptr;
                    if (!sameType)
                    {
                        // If target is a container (and not the same type), use it directly
                        container = dynamic_cast<WidgetContainer *>(item.get());
                    }
                    // If target is not a container, or same-type, climb to nearest container ancestor
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

            // Reparent: move a dragged tree item to a new parent
            if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(DND_WIDGET_ITEM_REPARENT))
            {
                if (p->Data && p->DataSize == sizeof(int))
                {
                    const int draggedId = *static_cast<const int *>(p->Data);

                    // Look up the dragged item to check its type
                    const auto &items = canvas->GetItems();
                    const auto draggedIt = items.find(draggedId);
                    const WidgetType draggedType = (draggedIt != items.end() && draggedIt->second)
                        ? draggedIt->second->GetWidgetType()
                        : WidgetType::COUNT;

                    // Rule 1: do not reparent onto a node of the same type
                    const bool sameType = (item->GetWidgetType() == draggedType);

                    // Resolve new parent container (same logic as toolbox drop)
                    WidgetContainer *newParent = nullptr;
                    if (!sameType)
                        newParent = dynamic_cast<WidgetContainer *>(item.get());
                    if (!newParent)
                        newParent = ResolveContainer(item->parent);

                    if (newParent && const_cast<WidgetCanvas *>(canvas)->ReparentItem(draggedId, newParent))
                        const_cast<WidgetCanvas *>(canvas)->SetDirtyFlag(true);

                    reparent = true;
                }
            }

            ImGui::EndDragDropTarget();
        }

        bool isDeleting = false;
        if (opened)
        {
            if (!isDeleting && !reparent)
            {
                // Snapshot children BEFORE recursing: a DnD drop inside any recursive call
                // may push_back into item->children (or an ancestor's children vector),
                // which would invalidate the range-based-for iterator and cause a crash.
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

        const VerticalAlignment activeVAlign = item->VAlignment;
        const HorizontalAlignment activeHAlign = item->HAlignment;

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
        for (int i = 0; i < static_cast<int>(VerticalAlignment::COUNT); ++i)
        {
            const VerticalAlignment VAlign = static_cast<VerticalAlignment>(i);
            const HorizontalAlignment HAlign = static_cast<HorizontalAlignment>(i);

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
            }
        }

        // Draw a line from the active anchor to the item's center
        const glm::vec2 itemCenter = item->GetAlignedRect().GetCenter();
        const ImVec2 itemScreenCenter = CanvasToScreen(itemCenter.x, itemCenter.y, imagePos, imageSize, canvasW, canvasH);
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

        ImGui::PushID(selectedItem.get()); // SelectedItem ID

        bool dirty = false;

        char nameBuffer[256] {};
        std::strncpy(nameBuffer, selectedItem->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            selectedItem->name = nameBuffer;
            dirty = true;
        }

        dirty |= UI::DrawVec2Control("Position", selectedItem->position, 1.0f);
        dirty |= UI::DrawVec2Control("Size", selectedItem->size, 1.0f, 1.0f);

        // --- Visibility ---
        if (ImGui::TreeNodeEx("##widget_visibility", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Visibility"))
        {
            bool visible = selectedItem->IsVisible();
            if (ImGui::Checkbox("Visible", &visible))
            {
                selectedItem->SetVisible(visible);
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

        // Alignment
        if (ImGui::TreeNodeEx("##widget_alignment", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Alignment"))
        {
            int VAlignment = static_cast<int>(selectedItem->VAlignment);
            static std::array<const char *, 4> VAlignmentNames = { "Top", "Middle", "Bottom", "Expand Vertically" };

            int HAlignment = static_cast<int>(selectedItem->HAlignment);
            static std::array<const char *, 4> HAlignmentNames = { "Left", "Center", "Right", "Expand Horizontally" };

            if (UI::DrawComboBox("Vertical", VAlignmentNames.data(), static_cast<int>(VAlignmentNames.size()), &VAlignment))
            {
                selectedItem->VAlignment = static_cast<VerticalAlignment>(VAlignment);
                dirty = true;
            }

            if (UI::DrawComboBox("Horizontal", HAlignmentNames.data(), static_cast<int>(HAlignmentNames.size()), &HAlignment))
            {
                selectedItem->HAlignment = static_cast<HorizontalAlignment>(HAlignment);
                dirty = true;
            }

            ImGui::TreePop();
        }

        // Transform
        WidgetContainer *parentContainer = nullptr;
        if (selectedItem->parent && selectedItem->parent->GetWidgetType() == WidgetType::Container)
            parentContainer = dynamic_cast<WidgetContainer *>(selectedItem->parent);

        // --- Button-specific ---
        if (Ref<WidgetButton> button = selectedItem->As<WidgetButton>())
        {
            // Appearance
            if (ImGui::TreeNodeEx("##widget_button_appearance", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Appearance"))
            {
                dirty |= UI::DrawColorVec4("Color", button->style.color);
                dirty |= UI::DrawColorVec4("Hover Color", button->style.hoverColor);
                dirty |= UI::DrawColorVec4("Pressed Color", button->style.pressedColor);
                dirty |= UI::DrawColorVec4("Border Color", button->style.borderColor);
                dirty |= UI::DrawFloatControl("Border Width", &button->style.borderWidth, 0.5f, 0.0f, 100.0f);
                dirty |= UI::DrawFloatControl("Corner Radius", &button->style.cornerRadius, 0.5f, 0.0f, 100.0f);

                // Background
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

            // Label
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
        
        // Container-specific
        else if (Ref<WidgetContainer> container = selectedItem->As<WidgetContainer>())
        {
            ImGui::PushID(container.get()); // Container ID

            if (ImGui::TreeNodeEx("##widget_container_props", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Container"))
            {
                int layout = static_cast<int>(container->layout);
                const char *layoutNames[] = { "Horizontal", "Vertical", "Grid", "Absolute" };
                if (UI::DrawComboBox("Layout", layoutNames, IM_ARRAYSIZE(layoutNames), &layout))
                {
                    const LayoutMode newLayout = static_cast<LayoutMode>(layout);
                    const LayoutMode oldLayout = container->layout;
                    container->layout = newLayout;
                    dirty = true;

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
                dirty |= UI::DrawFloatControl("Margin", &container->margin, 0.5f, 0.0f, 512.0f);
                dirty |= UI::DrawFloatControl("Gap", &container->gap, 0.5f, 0.0f, 512.0f);

                ImGui::TreePop();
            }

            ImGui::PopID(); // !Container ID
        }

        WidgetContainer *insertParent = ResolveInsertionParent(selectedItem, widget);
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
        {
            widget->SetDirtyFlag(true);
        }

        ImGui::PopID(); // !SelectedItem ID
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

    void WidgetEditor::UIWidgetEditor(AssetEditorData &assetData, EditorLayer *editorLayer)
    {
        if (!assetData.asset || !assetData.asset->IsReady())
        {
            auto assetManager = editorLayer && editorLayer->GetActiveProject() ? editorLayer->GetActiveProject()->GetAssetManager() : nullptr;
            assetData.asset = assetManager ? assetManager->GetAsset(assetData.handle) : nullptr;
            if (!assetData.asset || (assetData.asset && !assetData.asset->IsReady()))
            {
                ImGui::Text("Loading asset...");
                return;
            }
        }

        if (Ref<WidgetCanvas> widget = assetData.asset->As<WidgetCanvas>())
        {
            Project *project = editorLayer ? editorLayer->GetActiveProject().get() : nullptr;
            AssetManager *assetManager = project ? project->GetAssetManager() : nullptr;

            EditorSceneData &sceneData = assetData.sceneData;

            widget->CreateRoot(sceneData.viewportWidth > 0 ? sceneData.viewportWidth : 1280, sceneData.viewportHeight > 0 ? sceneData.viewportHeight : 720);
            if (widget->GetRoot() && widget->GetRoot()->name.empty())
                widget->GetRoot()->name = "Canvas Root";

            // ---- Per-asset persistent state ----
            static std::unordered_map<uint64_t, int>       s_SelectedWidgetItem;
            static std::unordered_map<uint64_t, float>     s_WidgetPreviewZoom;
            static std::unordered_map<uint64_t, glm::vec2> s_WidgetPreviewPan;
            static std::unordered_map<uint64_t, int>       s_WidgetPreviewAspect;

            const auto stateKey = static_cast<uint64_t>(assetData.handle);

            int &selectedItemId = s_SelectedWidgetItem[stateKey];
            if (selectedItemId == 0 || !widget->GetItems().contains(selectedItemId))
                selectedItemId = widget->GetRoot() ? widget->GetRoot()->id : 0;

            float &widgetPreviewZoom = s_WidgetPreviewZoom[stateKey];
            if (widgetPreviewZoom <= 0.0f) widgetPreviewZoom = 1.0f;
            glm::vec2 &widgetPreviewPan = s_WidgetPreviewPan[stateKey];
            int &widgetPreviewAspect = s_WidgetPreviewAspect[stateKey];

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
            ImGui::BeginChild("##widget_scene_preview", { 0.0f, 0.0f }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
            {
                static const char *aspectRatioLabels[] = { "Free", "16:9", "16:10", "4:3", "21:9", "1:1" };
                static const float aspectRatioValues[] = { 0.0f, 16.0f / 9.0f, 16.0f / 10.0f, 4.0f / 3.0f, 21.0f / 9.0f, 1.0f };

                widgetPreviewAspect = std::clamp(widgetPreviewAspect, 0, static_cast<int>(IM_ARRAYSIZE(aspectRatioLabels)) - 1);
                ImGui::SetNextItemWidth(140.0f);
                ImGui::Combo("Aspect Ratio", &widgetPreviewAspect, aspectRatioLabels, IM_ARRAYSIZE(aspectRatioLabels));

                const ImVec2 previewRegionSize = ImGui::GetContentRegionAvail();
                const ImVec2 previewRegionPos = ImGui::GetCursorScreenPos();
                const float selectedAspect = aspectRatioValues[widgetPreviewAspect];

                ImVec2 viewportSize = previewRegionSize;
                if (selectedAspect > 0.0f && viewportSize.x > 0.0f && viewportSize.y > 0.0f)
                {
                    const float regionAspect = viewportSize.x / std::max(viewportSize.y, 1.0f);
                    if (regionAspect > selectedAspect)
                    {
                        viewportSize.x = viewportSize.y * selectedAspect;
                    }
                    else
                    {
                        viewportSize.y = viewportSize.x / selectedAspect;
                    }
                }

                viewportSize.x = std::max(viewportSize.x, 1.0f);
                viewportSize.y = std::max(viewportSize.y, 1.0f);

                const ImVec2 viewportOffset =
                {
                    std::max((previewRegionSize.x - viewportSize.x) * 0.5f, 0.0f),
                    std::max((previewRegionSize.y - viewportSize.y) * 0.5f, 0.0f)
                };
                const ImVec2 viewportPos = { previewRegionPos.x + viewportOffset.x, previewRegionPos.y + viewportOffset.y };

                sceneData.viewportWidth  = std::max(1u, static_cast<uint32_t>(viewportSize.x + 0.5f));
                sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y + 0.5f));

                const float canvasW = static_cast<float>(sceneData.viewportWidth);
                const float canvasH = static_cast<float>(sceneData.viewportHeight);

                if (sceneData.compositeRT)
                {
                    Ref<Texture> previewTexture = sceneData.compositeRT->GetColorAttachment(0);

                    const std::string previewBtnId = std::format("##widget_preview_{}", stateKey);
                    ImGui::SetCursorScreenPos(viewportPos);
                    ImGui::InvisibleButton(previewBtnId.c_str(), viewportSize);
                    sceneData.viewportHovered = ImGui::IsItemHovered();

                    // Drop a WidgetCanvas from the Content Browser as a child canvas
                    if (ImGui::BeginDragDropTarget())
                    {
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
                        ImGui::EndDragDropTarget();
                    }

                    const ImVec2 mousePos = ImGui::GetMousePos();
                    if (sceneData.sceneRenderer)
                    {
                        const float localMouseX = std::clamp(mousePos.x - viewportPos.x, 0.0f, std::max(viewportSize.x - 1.0f, 0.0f));
                        const float localMouseY = std::clamp(mousePos.y - viewportPos.y, 0.0f, std::max(viewportSize.y - 1.0f, 0.0f));
                        sceneData.sceneRenderer->SetPreviewMouseState(
                            static_cast<uint32_t>(localMouseX),
                            static_cast<uint32_t>(localMouseY),
                            sceneData.viewportHovered);
                    }

                    // Zoom/pan
                    const float previousZoom = widgetPreviewZoom;
                    if (sceneData.viewportHovered && ImGui::GetIO().MouseWheel != 0.0f)
                    {
                        widgetPreviewZoom = std::clamp(widgetPreviewZoom + ImGui::GetIO().MouseWheel * 0.1f, 0.25f, 5.0f);
                        if (widgetPreviewZoom != previousZoom)
                        {
                            const ImVec2 prevImagePos  = { viewportPos.x + widgetPreviewPan.x, viewportPos.y + widgetPreviewPan.y };
                            const ImVec2 prevImageSize = { viewportSize.x * previousZoom,      viewportSize.y * previousZoom };
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
                    const ImVec2 imageSize = { viewportSize.x * widgetPreviewZoom,  viewportSize.y * widgetPreviewZoom };

                    ImDrawList *drawList = ImGui::GetWindowDrawList();
                    const ImVec2 clipMax = { viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y };
                    drawList->PushClipRect(viewportPos, clipMax, true);

                    drawList->AddImage(reinterpret_cast<ImTextureID>(previewTexture->GetHandle().Get()), imagePos, { imagePos.x + imageSize.x, imagePos.y + imageSize.y }, { 0.0f, 0.0f }, { 1.0f, 1.0f });
                    WidgetEditor::DrawPreviewOverlay(drawList, widget, selectedItemId, imagePos, imageSize, canvasW, canvasH);

                    if (selectedItem && widget->GetRoot() && selectedItem->id != widget->GetRoot()->id)
                    {
                        WidgetEditor::DrawAnchorPoints(drawList, selectedItem, widget, imagePos, imageSize, canvasW, canvasH);
                    }

                    drawList->PopClipRect();
                }
                else
                {
                    ImGui::Dummy(viewportSize);
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
