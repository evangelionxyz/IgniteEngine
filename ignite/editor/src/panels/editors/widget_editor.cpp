// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "widget_editor.hpp"
#include "states.hpp"
#include "ext/editor_ui.hpp"
#include <imgui_internal.h>
#include "ignite/asset/asset_manager.hpp"
#include "panels/asset_editor_data.hpp"

#include "editor_layer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/font.hpp"

#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_container.hpp"
#include "ignite/graphics/ui/widget_button.hpp"
#include "ignite/graphics/ui/widget_label.hpp"
#include "ignite/graphics/ui/widget_image.hpp"

#include "ignite/core/string_utils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <string>
#include <unordered_map>
#include <vector>

namespace ignite
{
    namespace
    {
        enum class WidgetResizeHandle : int
        {
            None        = -1,
            TopLeft     = 0,
            TopRight    = 1,
            BottomLeft  = 2,
            BottomRight = 3,
            TopCenter   = 4,
            BottomCenter= 5,
            MiddleLeft  = 6,
            MiddleRight = 7,
            Move        = 8
        };

        struct WidgetPreviewInteractionState
        {
            int selectedItemId = 0;
            float zoom = 1.0f;
            glm::vec2 pan = { 0.0f, 0.0f };
            int aspectIndex = 1; // 16:9 default
            glm::vec2 canvasSize = { 1920.0f, 1080.0f };
            bool initializedView = false;

            WidgetResizeHandle activeHandle = WidgetResizeHandle::None;
            int activeWidgetId = 0;
            glm::vec2 dragStartMouseCanvas = { 0.0f, 0.0f };
            glm::vec2 dragStartPos = { 0.0f, 0.0f };
            glm::vec2 dragStartSize = { 0.0f, 0.0f };
            Rect dragStartRect = {};
        };

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
            if (!item->IsVisible())
                return std::format("[{}] (Hidden)", displayName);
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
                case HorizontalAlignment::ExpandHorizontally: cx = midX; break;
                default: break;
            }
        
            switch (VAlignment)
            {
                case VerticalAlignment::Top: cy = minY; break;
                case VerticalAlignment::Middle: cy = midY; break;
                case VerticalAlignment::Bottom: cy = maxY; break;
                case VerticalAlignment::ExpandVertically: cy = midY; break;
                default: break;
            }

            return WidgetEditor::CanvasToScreen(cx, cy, imagePos, imageSize, canvasW, canvasH);
        }

        static std::array<ImVec2, 8> GetWidgetScreenHandles(const Rect &wr, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
        {
            const ImVec2 tl = WidgetEditor::CanvasToScreen(wr.min.x, wr.min.y, imagePos, imageSize, canvasW, canvasH);
            const ImVec2 br = WidgetEditor::CanvasToScreen(wr.max.x, wr.max.y, imagePos, imageSize, canvasW, canvasH);
            const ImVec2 tr = { br.x, tl.y };
            const ImVec2 bl = { tl.x, br.y };
            const ImVec2 tc = { (tl.x + br.x) * 0.5f, tl.y };
            const ImVec2 bc = { (tl.x + br.x) * 0.5f, br.y };
            const ImVec2 ml = { tl.x, (tl.y + br.y) * 0.5f };
            const ImVec2 mr = { br.x, (tl.y + br.y) * 0.5f };
            return { tl, tr, bl, br, tc, bc, ml, mr };
        }

        static WidgetResizeHandle HitTestResizeHandles(const Ref<IWidgetItem> &item, const ImVec2 &mouseScreenPos,
            const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH, float hitRadius = 7.5f)
        {
            if (!item || !item->IsEffectivelyVisible())
                return WidgetResizeHandle::None;

            const auto handles = GetWidgetScreenHandles(item->GetAlignedRect(), imagePos, imageSize, canvasW, canvasH);
            const float r2 = hitRadius * hitRadius;
            for (int i = 0; i < 8; ++i)
            {
                const float dx = mouseScreenPos.x - handles[i].x;
                const float dy = mouseScreenPos.y - handles[i].y;
                if (dx * dx + dy * dy <= r2)
                {
                    return static_cast<WidgetResizeHandle>(i);
                }
            }
            return WidgetResizeHandle::None;
        }

        static void ApplyHandleMouseCursor(WidgetResizeHandle handle)
        {
            switch (handle)
            {
                case WidgetResizeHandle::TopLeft:
                case WidgetResizeHandle::BottomRight:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                    break;
                case WidgetResizeHandle::TopRight:
                case WidgetResizeHandle::BottomLeft:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
                    break;
                case WidgetResizeHandle::TopCenter:
                case WidgetResizeHandle::BottomCenter:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    break;
                case WidgetResizeHandle::MiddleLeft:
                case WidgetResizeHandle::MiddleRight:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                    break;
                case WidgetResizeHandle::Move:
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                    break;
                default:
                    break;
            }
        }

        // Traverse widget hierarchy in reverse draw order (topmost child first) to pick the best widget at canvasPt.
        static Ref<IWidgetItem> HitTestWidgetTreeRecursive(const Ref<IWidgetItem> &node, const glm::vec2 &canvasPt, const WidgetContainer *root)
        {
            if (!node || !node->IsVisible())
                return nullptr;

            std::vector<Ref<IWidgetItem>> sortedChildren;
            sortedChildren.reserve(node->children.size());
            for (const Ref<IWidgetItem> &child : node->children)
            {
                if (child && child->IsVisible())
                    sortedChildren.push_back(child);
            }

            std::stable_sort(sortedChildren.begin(), sortedChildren.end(), [](const Ref<IWidgetItem> &a, const Ref<IWidgetItem> &b)
            {
                return a->zIndex < b->zIndex;
            });

            for (auto it = sortedChildren.rbegin(); it != sortedChildren.rend(); ++it)
            {
                if (Ref<IWidgetItem> hitChild = HitTestWidgetTreeRecursive(*it, canvasPt, root))
                    return hitChild;
            }

            if (node.get() != root && node->GetAlignedRect().Contains(canvasPt))
                return node;

            return nullptr;
        }

        static bool DrawAssetHandlePickerPopup(const char *popupId, AssetType expectedType, AssetHandle &inOutHandle, AssetManager *assetManager, const char *clearLabel)
        {
            bool changed = false;
            if (ImGui::BeginPopup(popupId))
            {
                if (ImGui::Selectable(clearLabel, inOutHandle == AssetHandle(0)))
                {
                    inOutHandle = AssetHandle(0);
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }

                if (assetManager)
                {
                    ImGui::Separator();
                    for (const auto &[handle, meta] : assetManager->GetAssetAssetRegistry())
                    {
                        if (meta.type != expectedType)
                            continue;

                        const std::string display = assetManager->GetAssetDisplayName(handle);
                        const bool isSelected = (inOutHandle == handle);
                        if (ImGui::Selectable(std::format("{}##{}", display, static_cast<uint64_t>(handle)).c_str(), isSelected))
                        {
                            inOutHandle = handle;
                            changed = true;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }

                ImGui::EndPopup();
            }
            return changed;
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

        const bool isEffectivelyVisible = item->IsEffectivelyVisible();
        if (!isEffectivelyVisible)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.62f, 0.85f));

        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<intptr_t>(item->id)), flags, "%s", GetWidgetTreeLabel(item, canvas).c_str());

        if (!isEffectivelyVisible)
            ImGui::PopStyleColor();

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            selectedItemId = item->id;

        // Context Menu for right-click in hierarchy tree
        if (ImGui::BeginPopupContextItem())
        {
            selectedItemId = item->id;
            auto container = dynamic_cast<WidgetContainer *>(item.get());
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
                if (ImGui::MenuItem(item->IsVisible() ? "Hide Widget" : "Show Widget"))
                {
                    item->SetVisible(!item->IsVisible());
                    const_cast<WidgetCanvas *>(canvas)->SetDirtyFlag(true);
                }
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
                            auto newParent = dynamic_cast<WidgetContainer *>(item.get());
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

        if (opened)
        {
            if (!reparent)
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

        if (selectedItem)
        {
            if (auto *container = dynamic_cast<WidgetContainer *>(selectedItem.get()))
                return container;

            if (selectedItem->parent)
            {
                if (auto *parentContainer = dynamic_cast<WidgetContainer *>(selectedItem->parent))
                    return parentContainer;
            }
        }

        return widget->GetRoot();
    }

    void WidgetEditor::DrawToolbox(const Ref<WidgetCanvas> &widget, int &selectedItemId)
    {
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
            { "Image",     "[I]", WidgetType::Image,     { 0.18f, 0.72f, 0.22f, 1.0f } },
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

        Ref<IWidgetItem> selectedItem = nullptr;
        if (widget && selectedItemId != 0 && widget->GetItems().contains(selectedItemId))
            selectedItem = widget->GetItems().at(selectedItemId);

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
            if (ImGui::Button(btnLabel.c_str(), ImVec2(-1.0f, 24.0f)) && widget)
            {
                if (WidgetContainer *parentContainer = ResolveInsertionParent(selectedItem, widget))
                {
                    switch (entry.type)
                    {
                        case WidgetType::Container: selectedItemId = widget->AddContainer(parentContainer); break;
                        case WidgetType::Button:    selectedItemId = widget->AddButton(parentContainer, "Button"); break;
                        case WidgetType::Label:     selectedItemId = widget->AddLabel(parentContainer, "Label"); break;
                        case WidgetType::Image:     selectedItemId = widget->AddImage(parentContainer); break;
                        case WidgetType::BoxSizing: selectedItemId = widget->AddBoxSizing(parentContainer); break;
                        case WidgetType::Overlay:   selectedItemId = widget->AddOverlay(parentContainer); break;
                        default: break;
                    }
                }
            }

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
        ImGui::TextDisabled("Tip: Click or drag onto the tree/preview\nto add a widget.");
    }

    // =========================================================================
    // Preview overlay (bounds + corner/edge resize handles)
    // =========================================================================
    void WidgetEditor::DrawPreviewOverlay(ImDrawList *drawList, const Ref<WidgetCanvas> &widget, int selectedItemId, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
    {
        if (!drawList || !widget || canvasW <= 0.0f || canvasH <= 0.0f)
            return;

        constexpr ImU32 kDimOutline   = IM_COL32(100, 190, 255, 55);
        constexpr ImU32 kSelRect      = IM_COL32(0, 210, 255, 240);
        constexpr ImU32 kHandleFill   = IM_COL32(255, 255, 255, 245);
        constexpr ImU32 kHandleBorder = IM_COL32(0, 150, 215, 255);
        constexpr float kHandleHalf   = 4.5f;

        const WidgetContainer *root = widget->GetRoot();

        // Draw non-selected outlines first
        for (const auto &[id, item] : widget->GetItems())
        {
            if (!item || !item->IsEffectivelyVisible())
                continue;

            if (id == selectedItemId || item.get() == root)
                continue;

            const Rect &wr = item->GetAlignedRect();
            const ImVec2 tl = CanvasToScreen(wr.min.x, wr.min.y, imagePos, imageSize, canvasW, canvasH);
            const ImVec2 br = CanvasToScreen(wr.max.x, wr.max.y, imagePos, imageSize, canvasW, canvasH);
            drawList->AddRect(tl, br, kDimOutline, 0.0f, 0, 1.0f);
        }

        // Draw selected item highlight + 8 resize handles on top
        if (selectedItemId != 0 && widget->GetItems().contains(selectedItemId))
        {
            const Ref<IWidgetItem> &selItem = widget->GetItems().at(selectedItemId);
            if (selItem && selItem->IsEffectivelyVisible())
            {
                const Rect &wr = selItem->GetAlignedRect();
                const ImVec2 tl = CanvasToScreen(wr.min.x, wr.min.y, imagePos, imageSize, canvasW, canvasH);
                const ImVec2 br = CanvasToScreen(wr.max.x, wr.max.y, imagePos, imageSize, canvasW, canvasH);

                drawList->AddRect(tl, br, kSelRect, 0.0f, 0, 1.8f);

                if (selItem.get() != root)
                {
                    const auto handles = GetWidgetScreenHandles(wr, imagePos, imageSize, canvasW, canvasH);
                    for (const ImVec2 &h : handles)
                    {
                        drawList->AddRectFilled({ h.x - kHandleHalf, h.y - kHandleHalf }, { h.x + kHandleHalf, h.y + kHandleHalf }, kHandleFill, 1.0f);
                        drawList->AddRect({ h.x - kHandleHalf, h.y - kHandleHalf }, { h.x + kHandleHalf, h.y + kHandleHalf }, kHandleBorder, 1.0f, 0, 1.2f);
                    }

                    // Draw compact size/name label above selection box
                    const glm::vec2 sz = wr.GetSize();
                    const std::string badgeText = std::format("{} ({:.0f} x {:.0f})",
                        selItem->name.empty() ? GetWidgetTypeLabel(*selItem) : selItem->name,
                        sz.x, sz.y);
                    const ImVec2 textSz = ImGui::CalcTextSize(badgeText.c_str());
                    const ImVec2 badgeMin = { tl.x, tl.y - textSz.y - 6.0f };
                    const ImVec2 badgeMax = { tl.x + textSz.x + 8.0f, tl.y - 2.0f };
                    drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(12, 24, 38, 215), 3.0f);
                    drawList->AddRect(badgeMin, badgeMax, IM_COL32(0, 190, 240, 180), 3.0f, 0, 1.0f);
                    drawList->AddText({ badgeMin.x + 4.0f, badgeMin.y + 2.0f }, IM_COL32(230, 248, 255, 255), badgeText.c_str());
                }
            }
        }
    }

    // =========================================================================
    // Anchor-point visualization
    // =========================================================================
    void WidgetEditor::DrawAnchorPoints(ImDrawList *drawList, const Ref<IWidgetItem> &item, const Ref<WidgetCanvas> &widget, const ImVec2 &imagePos, const ImVec2 &imageSize, float canvasW, float canvasH)
    {
        if (!drawList || !item || !widget || !item->IsEffectivelyVisible() || canvasW <= 0.0f || canvasH <= 0.0f)
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

        constexpr ImU32 kInactive   = IM_COL32(200, 200, 200, 140);
        constexpr ImU32 kActive     = IM_COL32(255, 200,  50, 255);
        constexpr ImU32 kActiveLine = IM_COL32(255, 200,  50, 120);
        constexpr ImU32 kParentTint = IM_COL32( 80, 160, 255,  25);
        constexpr float kDiamond    = 5.0f;

        // Faint tint on parent rect to show context
        {
            const ImVec2 ptl = CanvasToScreen(parentRect.min.x, parentRect.min.y, imagePos, imageSize, canvasW, canvasH);
            const ImVec2 pbr = CanvasToScreen(parentRect.max.x, parentRect.max.y, imagePos, imageSize, canvasW, canvasH);
            drawList->AddRectFilled(ptl, pbr, kParentTint);
            drawList->AddRect(ptl, pbr, IM_COL32(80, 160, 255, 90), 0.0f, 0, 1.0f);
        }

        // Draw 3x3 anchor diamonds across Top/Middle/Bottom x Left/Center/Right
        const VerticalAlignment effectiveV = (activeVAlign == VerticalAlignment::ExpandVertically) ? VerticalAlignment::Middle : activeVAlign;
        const HorizontalAlignment effectiveH = (activeHAlign == HorizontalAlignment::ExpandHorizontally) ? HorizontalAlignment::Center : activeHAlign;

        ImVec2 activeScreenPos = {};
        bool foundActive = false;
        for (int v = 0; v < 3; ++v)
        {
            for (int h = 0; h < 3; ++h)
            {
                const VerticalAlignment VAlign = static_cast<VerticalAlignment>(v);
                const HorizontalAlignment HAlign = static_cast<HorizontalAlignment>(h);

                const ImVec2 screenPos = GetAlignmentScreenPos(VAlign, HAlign, parentRect, imagePos, imageSize, canvasW, canvasH);
                const bool isActive = (VAlign == effectiveV && HAlign == effectiveH);
                const ImU32 color = isActive ? kActive : kInactive;
                const float sz = isActive ? kDiamond + 2.0f : kDiamond;

                drawList->AddQuadFilled(
                    { screenPos.x,      screenPos.y - sz },
                    { screenPos.x + sz, screenPos.y      },
                    { screenPos.x,      screenPos.y + sz },
                    { screenPos.x - sz, screenPos.y      },
                    color
                );

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
        const bool isRoot = (widget->GetRoot() && selectedItem.get() == widget->GetRoot());

        // =====================================================================
        // 1. Canvas Settings (shown at top when Root is selected, or collapsible)
        // =====================================================================
        const ImGuiTreeNodeFlags canvasFlags = ImGuiTreeNodeFlags_Framed | (isRoot ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        if (ImGui::TreeNodeEx("##widget_canvas_settings", canvasFlags, "Canvas Settings"))
        {
            char canvasNameBuf[256] {};
            std::strncpy(canvasNameBuf, widget->GetName().c_str(), sizeof(canvasNameBuf) - 1);
            if (ImGui::InputText("Canvas Name", canvasNameBuf, sizeof(canvasNameBuf)))
            {
                widget->SetName(canvasNameBuf);
                dirty = true;
            }

            bool canvasEnabled = widget->IsEnabled();
            if (UI::DrawCheckbox("Canvas Enabled", &canvasEnabled))
            {
                widget->SetEnabled(canvasEnabled);
                dirty = true;
            }

            bool blocksBelow = widget->BlocksWidgetsBelow();
            if (UI::DrawCheckbox("Blocks Below", &blocksBelow))
            {
                widget->SetBlocksWidgetsBelow(blocksBelow);
                dirty = true;
            }

            // Child widgets list
            if (ImGui::TreeNodeEx("##canvas_child_widgets", ImGuiTreeNodeFlags_DefaultOpen, "Child Canvases (%zu)", widget->GetChildWidgets().size()))
            {
                auto &childWidgets = widget->GetChildWidgets();
                int removeIdx = -1;
                for (size_t i = 0; i < childWidgets.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    WidgetChildEntry &entry = childWidgets[i];
                    const std::string childName = (assetManager && entry.handle != AssetHandle(0))
                        ? assetManager->GetAssetDisplayName(entry.handle)
                        : "None (Drop .wdgt)";

                    if (ImGui::Checkbox("##enabled", &entry.enabled))
                        dirty = true;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Enabled");

                    ImGui::SameLine();
                    if (ImGui::Checkbox("Block##block", &entry.blockWidgetsBelow))
                        dirty = true;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Block Widgets Below");

                    ImGui::SameLine();
                    if (ImGui::Button(childName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 28.0f, 0.0f)))
                    {
                        ImGui::OpenPopup("##pick_child_widget_popup");
                    }
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (p->Data && p->DataSize == sizeof(AssetHandle) && assetManager)
                            {
                                const AssetHandle dropped = *static_cast<const AssetHandle *>(p->Data);
                                if (assetManager->GetMetaData(dropped).type == AssetType::Widget && dropped != widget->handle)
                                {
                                    entry.handle = dropped;
                                    dirty = true;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (DrawAssetHandlePickerPopup("##pick_child_widget_popup", AssetType::Widget, entry.handle, assetManager, "None"))
                        dirty = true;

                    ImGui::SameLine();
                    if (ImGui::Button("X", ImVec2(22.0f, 0.0f)))
                        removeIdx = static_cast<int>(i);

                    ImGui::PopID();
                }

                if (removeIdx >= 0 && removeIdx < static_cast<int>(childWidgets.size()))
                {
                    childWidgets.erase(childWidgets.begin() + removeIdx);
                    dirty = true;
                }

                if (ImGui::Button("+ Add Child Canvas", ImVec2(-1.0f, 0.0f)))
                {
                    childWidgets.push_back(WidgetChildEntry{});
                    dirty = true;
                }

                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

        // =====================================================================
        // 2. Selected Widget Identity & General Properties
        // =====================================================================
        if (ImGui::TreeNodeEx("##widget_general", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed,
            "Widget: %s (ID: %d)", GetWidgetTypeLabel(*selectedItem), selectedItem->id))
        {
            char nameBuffer[256] {};
            std::strncpy(nameBuffer, selectedItem->name.c_str(), sizeof(nameBuffer) - 1);
            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            {
                selectedItem->name = nameBuffer;
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

            if (UI::DrawIntControl("Z-Index", &selectedItem->zIndex, 1.0f, -9999, 9999, 0))
            {
                selectedItem->MarkPaintDirty();
                dirty = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Higher Z-Index renders on top of lower values among siblings.");
            }

            if (UI::DrawCheckbox("Interactable", &selectedItem->interactable))
            {
                selectedItem->MarkPaintDirty();
                dirty = true;
            }

            ImGui::TreePop();
        }

        // =====================================================================
        // 3. Widget-Specific Content (Label / Button / Image / BoxSizing)
        //    Placed near the top so text, colors, and textures are immediately accessible!
        // =====================================================================
        if (Ref<WidgetButton> button = selectedItem->As<WidgetButton>())
        {
            if (ImGui::TreeNodeEx("##widget_button_appearance", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Button Style"))
            {
                if (UI::DrawColorVec4("Normal Color", button->style.color))
                {
                    button->baseStyle.backgroundColor = button->style.color;
                    button->MarkPaintDirty();
                    dirty = true;
                }
                if (UI::DrawColorVec4("Hover Color", button->style.hoverColor))
                {
                    button->MarkPaintDirty();
                    dirty = true;
                }
                if (UI::DrawColorVec4("Pressed Color", button->style.pressedColor))
                {
                    button->MarkPaintDirty();
                    dirty = true;
                }
                if (UI::DrawColorVec4("Disabled Color", button->style.disabledColor))
                {
                    button->MarkPaintDirty();
                    dirty = true;
                }
                if (UI::DrawColorVec4("Border Color", button->style.borderColor))
                {
                    button->baseStyle.borderColor = button->style.borderColor;
                    button->MarkPaintDirty();
                    dirty = true;
                }
                if (UI::DrawFloatControl("Border Width", &button->style.borderWidth, 0.25f, 0.0f, 100.0f, 1.0f))
                {
                    button->baseStyle.borderWidth = button->style.borderWidth;
                    button->MarkLayoutDirty();
                    button->MarkPaintDirty();
                    dirty = true;
                }
                if (UI::DrawFloatControl("Corner Radius", &button->style.cornerRadius, 0.5f, 0.0f, 100.0f, 0.0f))
                {
                    button->baseStyle.cornerRadius = button->style.cornerRadius;
                    button->MarkPaintDirty();
                    dirty = true;
                }
                if (UI::DrawFloatControl("Opacity", &button->baseStyle.opacity, 0.02f, 0.0f, 1.0f, 1.0f))
                {
                    button->MarkPaintDirty();
                    dirty = true;
                }

                bool isImageLoaded = button->imageHandle != AssetHandle(0);
                const std::string imageName = (isImageLoaded && assetManager)
                    ? assetManager->GetAssetDisplayName(button->imageHandle)
                    : "None (Click or Drop Texture)";
                bool openTexPicker = false;
                UI::DrawButtonWithColumn("Texture", imageName.c_str(), &openTexPicker, [assetManager, button, &dirty, isImageLoaded]()
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->Data && payload->DataSize == sizeof(AssetHandle) && assetManager)
                            {
                                const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                                if (assetManager->GetMetaData(dropped).type == AssetType::Texture)
                                {
                                    button->imageHandle = dropped;
                                    button->image = nullptr;
                                    button->MarkPaintDirty();
                                    dirty = true;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (isImageLoaded)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("X##ClearButtonImage"))
                        {
                            button->imageHandle = AssetHandle(0);
                            button->image = nullptr;
                            button->MarkPaintDirty();
                            dirty = true;
                        }
                    }
                });
                if (openTexPicker)
                    ImGui::OpenPopup("##button_tex_picker");
                if (DrawAssetHandlePickerPopup("##button_tex_picker", AssetType::Texture, button->imageHandle, assetManager, "None"))
                {
                    button->image = nullptr;
                    button->MarkPaintDirty();
                    dirty = true;
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("##widget_button_label", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Button Label"))
            {
                if (DrawWidgetLabel(button->label.get(), assetManager))
                {
                    button->MarkLayoutDirty();
                    button->MarkPaintDirty();
                    dirty = true;
                }
                ImGui::TreePop();
            }
        }
        else if (Ref<WidgetLabel> label = selectedItem->As<WidgetLabel>())
        {
            if (ImGui::TreeNodeEx("##widget_label_props", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Text & Font"))
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

        // =====================================================================
        // 4. Layout & Box Model
        // =====================================================================
        if (ImGui::TreeNodeEx("##widget_layout", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Layout & Transform"))
        {
            int posType = static_cast<int>(selectedItem->layout.positionType);
            static const char *posTypeNames[] = { "Relative (Flex)", "Absolute", "Fixed" };
            if (UI::DrawComboBox("Position Type", posTypeNames, 3, &posType))
            {
                selectedItem->layout.positionType = static_cast<PositionType>(posType);
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

            int VAlignment = static_cast<int>(selectedItem->layout.verticalAlignment);
            static std::array<const char *, 4> VAlignmentNames = { "Top", "Middle", "Bottom", "Expand Vertically" };
            if (UI::DrawComboBox("V-Align", VAlignmentNames.data(), static_cast<int>(VAlignmentNames.size()), &VAlignment))
            {
                selectedItem->layout.verticalAlignment = static_cast<VerticalAlignment>(VAlignment);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            if (UI::DrawVec2Control("Position", selectedItem->layout.position, 1.0f))
            {
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static const char *sizeModeNames[] = { "Auto", "Fixed", "Fill", "Percent" };

            int wMode = static_cast<int>(selectedItem->layout.widthMode);
            if (UI::DrawComboBox("Width Mode", sizeModeNames, 4, &wMode))
            {
                selectedItem->layout.widthMode = static_cast<SizeMode>(wMode);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            const float maxWLimit = (selectedItem->layout.widthMode == SizeMode::Percent) ? 100.0f : 10000.0f;
            if (UI::DrawFloatControl("Width", &selectedItem->layout.width, 1.0f, 0.0f, maxWLimit, 150.0f))
            {
                if (selectedItem->layout.widthMode == SizeMode::Auto || selectedItem->layout.widthMode == SizeMode::Fill)
                    selectedItem->layout.widthMode = SizeMode::Fixed;
                if (selectedItem->layout.horizontalAlignment == HorizontalAlignment::ExpandHorizontally)
                    selectedItem->layout.horizontalAlignment = HorizontalAlignment::Left;
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            int hMode = static_cast<int>(selectedItem->layout.heightMode);
            if (UI::DrawComboBox("Height Mode", sizeModeNames, 4, &hMode))
            {
                selectedItem->layout.heightMode = static_cast<SizeMode>(hMode);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            const float maxHLimit = (selectedItem->layout.heightMode == SizeMode::Percent) ? 100.0f : 10000.0f;
            if (UI::DrawFloatControl("Height", &selectedItem->layout.height, 1.0f, 0.0f, maxHLimit, 50.0f))
            {
                if (selectedItem->layout.heightMode == SizeMode::Auto || selectedItem->layout.heightMode == SizeMode::Fill)
                    selectedItem->layout.heightMode = SizeMode::Fixed;
                if (selectedItem->layout.verticalAlignment == VerticalAlignment::ExpandVertically)
                    selectedItem->layout.verticalAlignment = VerticalAlignment::Top;
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            if (Ref<WidgetBoxSizing> boxSizing = selectedItem->As<WidgetBoxSizing>())
            {
                if (UI::DrawVec2Control("Clamp Min", boxSizing->minSize, 1.0f, 0.0f))
                {
                    selectedItem->MarkLayoutDirty();
                    dirty = true;
                }
                if (UI::DrawVec2Control("Clamp Max", boxSizing->maxSize, 1.0f, 10000.0f))
                {
                    selectedItem->MarkLayoutDirty();
                    dirty = true;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            glm::vec2 minSz(selectedItem->layout.minWidth, selectedItem->layout.minHeight);
            if (UI::DrawVec2Control("Min Size", minSz, 1.0f, 0.0f))
            {
                selectedItem->layout.minWidth = std::max(0.0f, minSz.x);
                selectedItem->layout.minHeight = std::max(0.0f, minSz.y);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            glm::vec2 maxSz(selectedItem->layout.maxWidth, selectedItem->layout.maxHeight);
            if (UI::DrawVec2Control("Max Size", maxSz, 1.0f, 10000.0f))
            {
                selectedItem->layout.maxWidth = std::max(0.0f, maxSz.x);
                selectedItem->layout.maxHeight = std::max(0.0f, maxSz.y);
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            if (UI::DrawVec4Control("Margin (T/R/B/L)", selectedItem->layout.margin, 0.5f, 0.0f))
            {
                selectedItem->MarkLayoutDirty();
                dirty = true;
            }

            if (UI::DrawVec4Control("Padding (T/R/B/L)", selectedItem->layout.padding, 0.5f, 0.0f))
            {
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

            ImGui::TreePop();
        }

        // =====================================================================
        // 5. Flex Properties
        // =====================================================================
        const bool isContainer = (selectedItem->GetWidgetType() == WidgetType::Container
            || selectedItem->GetWidgetType() == WidgetType::BoxSizing
            || selectedItem->GetWidgetType() == WidgetType::Overlay);
        const bool parentIsContainer = (selectedItem->parent && dynamic_cast<WidgetContainer *>(selectedItem->parent) != nullptr);

        if (isContainer || parentIsContainer)
        {
            const ImGuiTreeNodeFlags flexFlags = ImGuiTreeNodeFlags_Framed
                | ((isContainer || selectedItem->layout.positionType == PositionType::Relative) ? ImGuiTreeNodeFlags_DefaultOpen : 0);
            if (ImGui::TreeNodeEx("##widget_flex", flexFlags, "Flex Layout"))
            {
                if (isContainer)
                {
                    int dir = static_cast<int>(selectedItem->layout.flex.direction);
                    static const char *flexDirNames[] = { "Row (Horizontal)", "Column (Vertical)" };
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

                    if (UI::DrawFloatControl("Gap", &selectedItem->layout.flex.gap, 0.5f, 0.0f, 512.0f, 0.0f))
                    {
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }
                }

                if (parentIsContainer)
                {
                    if (isContainer)
                        ImGui::Separator();

                    int selfAlign = static_cast<int>(selectedItem->layout.flex.alignSelf);
                    static const char *alignSelfNames[] = { "Auto", "Start", "Center", "End", "Stretch" };
                    if (UI::DrawComboBox("Align Self", alignSelfNames, 5, &selfAlign))
                    {
                        selectedItem->layout.flex.alignSelf = static_cast<AlignSelf>(selfAlign);
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }

                    if (UI::DrawFloatControl("Grow", &selectedItem->layout.flex.grow, 0.1f, 0.0f, 100.0f, 0.0f))
                    {
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }

                    if (UI::DrawFloatControl("Shrink", &selectedItem->layout.flex.shrink, 0.1f, 0.0f, 100.0f, 1.0f))
                    {
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }

                    if (UI::DrawFloatControl("Basis (-1 Auto)", &selectedItem->layout.flex.basis, 1.0f, -1.0f, 10000.0f, -1.0f))
                    {
                        selectedItem->MarkLayoutDirty();
                        dirty = true;
                    }
                }

                ImGui::TreePop();
            }
        }

        // =====================================================================
        // 6. UI Style & Drop Shadow
        // =====================================================================
        if (selectedItem->GetWidgetType() != WidgetType::Button)
        {
            if (ImGui::TreeNodeEx("##widget_uistyle", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "UI Style"))
            {
                if (UI::DrawColorVec4("Background Color", selectedItem->baseStyle.backgroundColor))
                {
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }

                if (UI::DrawColorVec4("Border Color", selectedItem->baseStyle.borderColor))
                {
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }

                if (UI::DrawFloatControl("Border Width", &selectedItem->baseStyle.borderWidth, 0.25f, 0.0f, 100.0f, 0.0f))
                {
                    selectedItem->MarkLayoutDirty();
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }

                if (UI::DrawFloatControl("Corner Radius", &selectedItem->baseStyle.cornerRadius, 0.5f, 0.0f, 100.0f, 0.0f))
                {
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }

                if (UI::DrawFloatControl("Opacity", &selectedItem->baseStyle.opacity, 0.02f, 0.0f, 1.0f, 1.0f))
                {
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }

                ImGui::TreePop();
            }
        }

        if (ImGui::TreeNodeEx("##widget_shadow_style", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "Drop Shadow"))
        {
            if (UI::DrawCheckbox("Enable Shadow", &selectedItem->baseStyle.shadowEnabled))
            {
                selectedItem->MarkPaintDirty();
                dirty = true;
            }

            if (selectedItem->baseStyle.shadowEnabled)
            {
                if (UI::DrawColorVec4("Shadow Color", selectedItem->baseStyle.shadowColor))
                {
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }

                if (UI::DrawVec2Control("Shadow Offset", selectedItem->baseStyle.shadowOffset, 0.5f, 2.0f))
                {
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }

                if (UI::DrawFloatControl("Shadow Size", &selectedItem->baseStyle.shadowSize, 0.5f, 0.0f, 128.0f, 7.0f))
                {
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }

                if (UI::DrawFloatControl("Shadow Softness", &selectedItem->baseStyle.shadowSoftness, 0.01f, 0.08f, 0.85f, 0.32f))
                {
                    selectedItem->MarkPaintDirty();
                    dirty = true;
                }
            }

            ImGui::TreePop();
        }

        if (dirty)
        {
            widget->SetDirtyFlag(true);
        }

        ImGui::PopID();
    }

    // =========================================================================
    // Label Inspector (used by both WidgetLabel and WidgetButton::label)
    // =========================================================================
    bool WidgetEditor::DrawWidgetLabel(WidgetLabel *label, AssetManager *assetManager)
    {
        if (!label)
            return false;

        bool dirty = false;

        // Text content (updates live on every keystroke)
        char textBuffer[1024] {};
        std::strncpy(textBuffer, label->text.c_str(), sizeof(textBuffer) - 1);
        ImGui::BeginColumns("##label_text_col", 2, ImGuiOldColumnFlags_GrowParentContentsSize);
        ImGui::SetColumnWidth(0, UI::defColWidth);
        ImGui::Text("Text");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##label_text_input", textBuffer, sizeof(textBuffer)))
        {
            label->text = textBuffer;
            label->MarkLayoutDirty();
            label->MarkPaintDirty();
            dirty = true;
        }
        ImGui::EndColumns();

        // Text Color (always available!)
        if (UI::DrawColorVec4("Text Color", label->style.color))
        {
            label->MarkPaintDirty();
            dirty = true;
        }

        // Font asset selector + drag-and-drop (defaults to engine default font when 0)
        const bool hasCustomFont = label->fontHandle != AssetHandle(0);
        std::string fontName = "Default Font (Click or Drop)";
        if (hasCustomFont && assetManager)
        {
            const std::string resolvedName = assetManager->GetAssetDisplayName(label->fontHandle);
            fontName = (!resolvedName.empty() && resolvedName != "Invalid Asset")
                ? resolvedName
                : std::format("Font #{}", static_cast<uint64_t>(label->fontHandle));
        }

        bool openFontPicker = false;
        UI::DrawButtonWithColumn("Font", fontName.c_str(), &openFontPicker, [assetManager, label, &dirty, hasCustomFont]()
        {
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle) && assetManager)
                    {
                        const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                        if (assetManager->GetMetaData(dropped).type == AssetType::Font)
                        {
                            label->fontHandle = dropped;
                            label->font = nullptr;
                            label->MarkLayoutDirty();
                            label->MarkPaintDirty();
                            dirty = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (hasCustomFont)
            {
                ImGui::SameLine();
                if (ImGui::Button("X##ClearFont"))
                {
                    label->fontHandle = AssetHandle(0);
                    label->font = nullptr;
                    label->MarkLayoutDirty();
                    label->MarkPaintDirty();
                    dirty = true;
                }
            }
        });
        if (openFontPicker)
            ImGui::OpenPopup("##label_font_picker");
        if (DrawAssetHandlePickerPopup("##label_font_picker", AssetType::Font, label->fontHandle, assetManager, "Default Font"))
        {
            label->font = nullptr;
            label->MarkLayoutDirty();
            label->MarkPaintDirty();
            dirty = true;
        }

        // Typography controls (always available whether using default font or custom font!)
        if (UI::DrawFloatControl("Font Size", &label->style.fontSize, 0.25f, 1.0f, 256.0f, 16.0f))
        {
            label->MarkLayoutDirty();
            label->MarkPaintDirty();
            dirty = true;
        }

        if (UI::DrawFloatControl("Kerning", &label->style.kerning, 0.01f, -20.0f, 100.0f, 0.0f))
        {
            label->MarkLayoutDirty();
            label->MarkPaintDirty();
            dirty = true;
        }

        if (UI::DrawFloatControl("Line Spacing", &label->style.lineSpacing, 0.01f, -10.0f, 50.0f, 0.025f))
        {
            label->MarkLayoutDirty();
            label->MarkPaintDirty();
            dirty = true;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Text Shadow controls
        if (UI::DrawCheckbox("Text Shadow", &label->style.shadowEnabled))
        {
            label->MarkPaintDirty();
            dirty = true;
        }

        if (label->style.shadowEnabled)
        {
            if (UI::DrawColorVec4("Text Shadow Color", label->style.shadowColor))
            {
                label->MarkPaintDirty();
                dirty = true;
            }

            if (UI::DrawVec2Control("Text Shadow Offset", label->style.shadowOffset, 0.25f, 1.5f))
            {
                label->MarkPaintDirty();
                dirty = true;
            }
        }

        return dirty;
    }

    // =========================================================================
    // Image Inspector
    // =========================================================================
    bool WidgetEditor::DrawWidgetImage(WidgetImage *image, AssetManager *assetManager)
    {
        if (!image)
            return false;

        bool dirty = false;

        const bool isLoaded = image->imageHandle != AssetHandle(0);
        const std::string imageName = (isLoaded && assetManager)
            ? assetManager->GetAssetDisplayName(image->imageHandle)
            : "None (Click or Drop Texture)";

        bool openTexPicker = false;
        UI::DrawButtonWithColumn("Texture", imageName.c_str(), &openTexPicker,
            [assetManager, image, &dirty, isLoaded]()
        {
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle) && assetManager)
                    {
                        const AssetHandle dropped = *static_cast<const AssetHandle *>(payload->Data);
                        if (assetManager->GetMetaData(dropped).type == AssetType::Texture)
                        {
                            image->imageHandle = dropped;
                            image->image = nullptr;
                            image->MarkPaintDirty();
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
                    image->MarkPaintDirty();
                    dirty = true;
                }
            }
        });
        if (openTexPicker)
            ImGui::OpenPopup("##widget_image_tex_picker");
        if (DrawAssetHandlePickerPopup("##widget_image_tex_picker", AssetType::Texture, image->imageHandle, assetManager, "None"))
        {
            image->image = nullptr;
            image->MarkPaintDirty();
            dirty = true;
        }

        if (UI::DrawColorVec4("Tint Color", image->tintColor))
        {
            image->MarkPaintDirty();
            dirty = true;
        }

        if (UI::DrawVec2Control("UV Min", image->uv0, 0.01f, 0.0f))
        {
            image->MarkPaintDirty();
            dirty = true;
        }

        if (UI::DrawVec2Control("UV Max", image->uv1, 0.01f, 1.0f))
        {
            image->MarkPaintDirty();
            dirty = true;
        }

        return dirty;
    }

    // =========================================================================
    // Main Widget Editor Window
    // =========================================================================
    void WidgetEditor::UIWidgetEditor(UI::AssetEditorData &assetData, EditorLayer *editorLayer)
    {
        (void)editorLayer;

        if (!assetData.asset || !assetData.asset->IsReady())
        {
            auto assetManager = AssetManager::GetInstance();
            assetData.asset = assetManager ? assetManager->GetAsset(assetData.handle) : nullptr;
            if (!assetData.asset || !assetData.asset->IsReady())
            {
                UI::DrawCenteredText("Loading asset...");
                return;
            }
        }

        if (Ref<WidgetCanvas> widget = assetData.asset->As<WidgetCanvas>())
        {
            auto assetManager = AssetManager::GetInstance();
            UI::EditorSceneData &sceneData = assetData.sceneData;

            static std::unordered_map<uint64_t, WidgetPreviewInteractionState> s_EditorState;
            const auto stateKey = static_cast<uint64_t>(assetData.handle);
            WidgetPreviewInteractionState &state = s_EditorState[stateKey];

            if (!state.initializedView)
            {
                const glm::uvec2 initialVp = widget->GetViewportSize();
                if (initialVp.x > 0 && initialVp.y > 0 && (initialVp.x != 1280 || initialVp.y != 720))
                {
                    state.canvasSize = { static_cast<float>(initialVp.x), static_cast<float>(initialVp.y) };
                }
                else
                {
                    state.canvasSize = { 1920.0f, 1080.0f };
                }
            }

            widget->CreateRoot(
                static_cast<uint32_t>(std::max(state.canvasSize.x, 1.0f)),
                static_cast<uint32_t>(std::max(state.canvasSize.y, 1.0f)));
            if (widget->GetRoot() && widget->GetRoot()->name.empty())
                widget->GetRoot()->name = "Canvas Root";

            int &selectedItemId = state.selectedItemId;
            if (selectedItemId == 0 || !widget->GetItems().contains(selectedItemId))
                selectedItemId = widget->GetRoot() ? widget->GetRoot()->id : 0;

            if (sceneData.sceneRenderer)
                sceneData.sceneRenderer->SetPreviewWidget(widget);

            // Resolve selected item
            Ref<IWidgetItem> selectedItem = nullptr;
            if (selectedItemId != 0 && widget->GetItems().contains(selectedItemId))
                selectedItem = widget->GetItems().at(selectedItemId);

            // LEFT PANEL — Toolbox + Layout Tree
            if (ImGui::BeginChild("##widget_layout_toolbox_tree", { 250.0f, 0.0f }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
            {
                ImGui::BeginChild("##widget_layout_toolbox", { 0.0f, 260.0f }, ImGuiChildFlags_ResizeY);
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
                ImGui::EndChild();

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
                ImGui::EndChild();
            }
            ImGui::EndChild();

            // Re-resolve selected item in case tree selection changed
            selectedItem = (selectedItemId != 0 && widget->GetItems().contains(selectedItemId))
                ? widget->GetItems().at(selectedItemId)
                : nullptr;

            ImGui::SameLine();

            // MIDDLE PANEL — Scene Preview
            ImGui::BeginChild("##widget_scene_preview", { 0.0f, 0.0f }, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                static const char *aspectRatioLabels[] = { "Free", "16:9", "16:10", "4:3", "21:9", "1:1" };
                static const float aspectRatioValues[] = { 0.0f, 16.0f / 9.0f, 16.0f / 10.0f, 4.0f / 3.0f, 21.0f / 9.0f, 1.0f };

                state.aspectIndex = std::clamp(state.aspectIndex, 0, static_cast<int>(IM_ARRAYSIZE(aspectRatioLabels)) - 1);
                ImGui::SetNextItemWidth(110.0f);
                if (ImGui::Combo("Aspect", &state.aspectIndex, aspectRatioLabels, IM_ARRAYSIZE(aspectRatioLabels)))
                {
                    if (state.aspectIndex > 0)
                    {
                        const float aspect = aspectRatioValues[state.aspectIndex];
                        state.canvasSize.y = std::round(std::max(state.canvasSize.x / aspect, 1.0f));
                    }
                }

                ImGui::SameLine();
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::InputFloat2("Canvas Size", &state.canvasSize.x, "%.0f"))
                {
                    state.canvasSize.x = std::max(state.canvasSize.x, 1.0f);
                    state.canvasSize.y = std::max(state.canvasSize.y, 1.0f);
                    state.aspectIndex = 0;
                }
                state.canvasSize.x = std::max(state.canvasSize.x, 1.0f);
                state.canvasSize.y = std::max(state.canvasSize.y, 1.0f);

                const float selectedAspect = aspectRatioValues[state.aspectIndex];
                ImVec2 logicalCanvasSize = { state.canvasSize.x, state.canvasSize.y };
                if (selectedAspect > 0.0f)
                {
                    logicalCanvasSize.y = std::round(std::max(logicalCanvasSize.x / selectedAspect, 1.0f));
                }

                const float canvasW = std::max(logicalCanvasSize.x, 1.0f);
                const float canvasH = std::max(logicalCanvasSize.y, 1.0f);

                // Ensure layout is up-to-date for current logical canvas dimensions
                widget->UpdateLayout(static_cast<uint32_t>(canvasW), static_cast<uint32_t>(canvasH));

                // Toolbar zoom/fit buttons
                bool requestFitView = false;
                bool requestOneToOne = false;
                ImGui::SameLine();
                if (ImGui::Button("Fit"))
                    requestFitView = true;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Fit canvas to preview panel (F)");

                ImGui::SameLine();
                if (ImGui::Button("1:1"))
                    requestOneToOne = true;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Reset zoom to 100%%");

                ImGui::SameLine();
                ImGui::TextDisabled("Zoom: %.0f%%", state.zoom * 100.0f);

                const ImVec2 previewRegionSize = ImGui::GetContentRegionAvail();
                const ImVec2 previewRegionPos = ImGui::GetCursorScreenPos();

                if (previewRegionSize.x > 32.0f && previewRegionSize.y > 32.0f)
                {
                    if (!state.initializedView || requestFitView)
                    {
                        const float fitX = std::max(previewRegionSize.x - 40.0f, 16.0f) / canvasW;
                        const float fitY = std::max(previewRegionSize.y - 40.0f, 16.0f) / canvasH;
                        state.zoom = std::clamp(std::min(fitX, fitY), 0.1f, 8.0f);
                        state.pan.x = std::round((previewRegionSize.x - canvasW * state.zoom) * 0.5f);
                        state.pan.y = std::round((previewRegionSize.y - canvasH * state.zoom) * 0.5f);
                        state.initializedView = true;
                    }
                    else if (requestOneToOne)
                    {
                        state.zoom = 1.0f;
                        state.pan.x = std::round((previewRegionSize.x - canvasW) * 0.5f);
                        state.pan.y = std::round((previewRegionSize.y - canvasH) * 0.5f);
                    }
                }

                // Set render target size to match the preview region (1:1 screen pixels for crisp zoomed MSDF text!)
                sceneData.viewportWidth  = std::max(1u, static_cast<uint32_t>( std::max(previewRegionSize.x, 1.0f) + 0.5f ));
                sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>( std::max(previewRegionSize.y, 1.0f) + 0.5f ));

                if (sceneData.compositeRT && previewRegionSize.x > 1.0f && previewRegionSize.y > 1.0f)
                {
                    Ref<Texture> previewTexture = sceneData.compositeRT->GetColorAttachment(0);

                    const std::string previewBtnId = std::format("##widget_preview_{}", stateKey);
                    ImGui::SetCursorScreenPos(previewRegionPos);
                    ImGui::InvisibleButton(previewBtnId.c_str(), previewRegionSize,
                        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);

                    const bool isViewportHovered = ImGui::IsItemHovered();
                    const bool isViewportActive  = ImGui::IsItemActive();
                    sceneData.viewportHovered = isViewportHovered;

                    const ImVec2 mousePos = ImGui::GetMousePos();

                    // 1. Zoom with Mouse Wheel (anchored at mouse cursor position)
                    if (isViewportHovered && std::abs(ImGui::GetIO().MouseWheel) > 0.0001f)
                    {
                        const float previousZoom = state.zoom;
                        const float zoomFactor = std::pow(1.12f, ImGui::GetIO().MouseWheel);
                        state.zoom = std::clamp(state.zoom * zoomFactor, 0.1f, 8.0f);
                        if (state.zoom != previousZoom)
                        {
                            const ImVec2 prevImagePos  = { previewRegionPos.x + state.pan.x, previewRegionPos.y + state.pan.y };
                            const ImVec2 prevImageSize = { canvasW * previousZoom, canvasH * previousZoom };
                            const ImVec2 uvAtMouse =
                            {
                                (mousePos.x - prevImagePos.x) / std::max(prevImageSize.x, 1.0f),
                                (mousePos.y - prevImagePos.y) / std::max(prevImageSize.y, 1.0f)
                            };
                            const ImVec2 newImageSize = { canvasW * state.zoom, canvasH * state.zoom };
                            state.pan.x = (mousePos.x - previewRegionPos.x) - uvAtMouse.x * newImageSize.x;
                            state.pan.y = (mousePos.y - previewRegionPos.y) - uvAtMouse.y * newImageSize.y;
                        }
                    }

                    // 2. Pan with Middle Mouse, Right Mouse, or Space + Left Mouse
                    const bool isSpacePanning = ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
                    const bool isPanning = (isViewportHovered || isViewportActive) &&
                        (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right) || isSpacePanning);

                    if (isPanning)
                    {
                        state.pan.x += ImGui::GetIO().MouseDelta.x;
                        state.pan.y += ImGui::GetIO().MouseDelta.y;
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }

                    // Shortcut F to fit view when hovered
                    if (isViewportHovered && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F))
                    {
                        const float fitX = std::max(previewRegionSize.x - 40.0f, 16.0f) / canvasW;
                        const float fitY = std::max(previewRegionSize.y - 40.0f, 16.0f) / canvasH;
                        state.zoom = std::clamp(std::min(fitX, fitY), 0.1f, 8.0f);
                        state.pan.x = std::round((previewRegionSize.x - canvasW * state.zoom) * 0.5f);
                        state.pan.y = std::round((previewRegionSize.y - canvasH * state.zoom) * 0.5f);
                    }

                    const ImVec2 imagePos = { previewRegionPos.x + state.pan.x, previewRegionPos.y + state.pan.y };
                    const ImVec2 scaledImageSize = { canvasW * state.zoom, canvasH * state.zoom };

                    // Convert screen mouse position to exact canvas space taking zoom & pan into account
                    const ImVec2 canvasMouse = ScreenToCanvas(mousePos, imagePos, scaledImageSize, canvasW, canvasH);
                    const bool isMouseOverCanvas = (mousePos.x >= imagePos.x && mousePos.x <= imagePos.x + scaledImageSize.x &&
                                                    mousePos.y >= imagePos.y && mousePos.y <= imagePos.y + scaledImageSize.y);

                    if (sceneData.sceneRenderer)
                    {
                        sceneData.sceneRenderer->SetPreviewCanvasTransform(
                            static_cast<uint32_t>(canvasW),
                            static_cast<uint32_t>(canvasH),
                            state.pan,
                            state.zoom);

                        const float localMouseX = std::clamp(canvasMouse.x, 0.0f, std::max(canvasW - 1.0f, 0.0f));
                        const float localMouseY = std::clamp(canvasMouse.y, 0.0f, std::max(canvasH - 1.0f, 0.0f));
                        sceneData.sceneRenderer->SetPreviewMouseState(
                            static_cast<uint32_t>(localMouseX),
                            static_cast<uint32_t>(localMouseY),
                            isViewportHovered && isMouseOverCanvas);
                    }

                    // 3. Hover cursor feedback & Left-Click to Resize / Select / Move
                    const bool canSelectOrManipulate = !ImGui::IsKeyDown(ImGuiKey_Space)
                        && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)
                        && !ImGui::IsMouseDown(ImGuiMouseButton_Right);

                    if (state.activeHandle == WidgetResizeHandle::None && isViewportHovered && canSelectOrManipulate)
                    {
                        WidgetResizeHandle hoveredHandle = WidgetResizeHandle::None;
                        if (selectedItem && selectedItem.get() != widget->GetRoot())
                        {
                            hoveredHandle = HitTestResizeHandles(selectedItem, mousePos, imagePos, scaledImageSize, canvasW, canvasH);
                            if (hoveredHandle == WidgetResizeHandle::None &&
                                selectedItem->IsEffectivelyVisible() &&
                                selectedItem->GetAlignedRect().Contains(glm::vec2(canvasMouse.x, canvasMouse.y)))
                            {
                                hoveredHandle = WidgetResizeHandle::Move;
                            }
                        }
                        ApplyHandleMouseCursor(hoveredHandle);

                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            // Priority 1: Resize handles of currently selected widget
                            WidgetResizeHandle clickedResize = WidgetResizeHandle::None;
                            if (selectedItem && selectedItem.get() != widget->GetRoot())
                            {
                                clickedResize = HitTestResizeHandles(selectedItem, mousePos, imagePos, scaledImageSize, canvasW, canvasH);
                            }

                            if (clickedResize != WidgetResizeHandle::None)
                            {
                                state.activeHandle = clickedResize;
                                state.activeWidgetId = selectedItem->id;
                                state.dragStartMouseCanvas = { canvasMouse.x, canvasMouse.y };
                                state.dragStartPos = selectedItem->layout.position;
                                state.dragStartRect = selectedItem->GetAlignedRect();
                                state.dragStartSize = state.dragStartRect.GetSize();
                            }
                            else
                            {
                                // Priority 2: Hit-test widget hierarchy in reverse draw order
                                Ref<IWidgetItem> hitItem = nullptr;
                                if (widget->GetRoot())
                                {
                                    hitItem = HitTestWidgetTreeRecursive(
                                        widget->GetRoot()->As<IWidgetItem>(),
                                        glm::vec2(canvasMouse.x, canvasMouse.y),
                                        widget->GetRoot());
                                }

                                if (hitItem)
                                {
                                    selectedItemId = hitItem->id;
                                    selectedItem = hitItem;

                                    state.activeHandle = WidgetResizeHandle::Move;
                                    state.activeWidgetId = hitItem->id;
                                    state.dragStartMouseCanvas = { canvasMouse.x, canvasMouse.y };
                                    state.dragStartPos = hitItem->layout.position;
                                    state.dragStartRect = hitItem->GetAlignedRect();
                                    state.dragStartSize = state.dragStartRect.GetSize();
                                }
                                else if (isMouseOverCanvas && widget->GetRoot())
                                {
                                    selectedItemId = widget->GetRoot()->id;
                                    selectedItem = widget->GetRoot()->As<IWidgetItem>();
                                }
                            }
                        }
                    }

                    // 4. Active Dragging (Move or Resize)
                    if (state.activeHandle != WidgetResizeHandle::None)
                    {
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && widget->GetItems().contains(state.activeWidgetId))
                        {
                            ApplyHandleMouseCursor(state.activeHandle);
                            Ref<IWidgetItem> activeItem = widget->GetItems().at(state.activeWidgetId);
                            if (activeItem && activeItem.get() != widget->GetRoot())
                            {
                                const glm::vec2 delta = glm::vec2(canvasMouse.x, canvasMouse.y) - state.dragStartMouseCanvas;

                                if (state.activeHandle == WidgetResizeHandle::Move)
                                {
                                    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.5f))
                                    {
                                        activeItem->layout.position = glm::round(state.dragStartPos + delta);
                                        activeItem->MarkLayoutDirty();
                                        activeItem->MarkPaintDirty();
                                        widget->SetDirtyFlag(true);
                                        widget->UpdateLayout(static_cast<uint32_t>(canvasW), static_cast<uint32_t>(canvasH));
                                    }
                                }
                                else
                                {
                                    const int hIdx = static_cast<int>(state.activeHandle);
                                    const bool resizeLeft   = (hIdx == 0 || hIdx == 2 || hIdx == 6);
                                    const bool resizeRight  = (hIdx == 1 || hIdx == 3 || hIdx == 7);
                                    const bool resizeTop    = (hIdx == 0 || hIdx == 1 || hIdx == 4);
                                    const bool resizeBottom = (hIdx == 2 || hIdx == 3 || hIdx == 5);

                                    if (resizeLeft || resizeRight)
                                    {
                                        activeItem->layout.widthMode = SizeMode::Fixed;
                                        if (activeItem->layout.horizontalAlignment == HorizontalAlignment::ExpandHorizontally)
                                            activeItem->layout.horizontalAlignment = HorizontalAlignment::Left;
                                        if (activeItem->layout.flex.basis >= 0.0f)
                                            activeItem->layout.flex.basis = -1.0f;

                                        const float minW = std::max(activeItem->layout.minWidth, 4.0f);
                                        const float maxW = std::max(activeItem->layout.maxWidth, minW);

                                        float newMinX = state.dragStartRect.min.x;
                                        float newMaxX = state.dragStartRect.max.x;
                                        if (resizeLeft)
                                        {
                                            newMinX = std::clamp(std::round(state.dragStartRect.min.x + delta.x),
                                                state.dragStartRect.max.x - maxW,
                                                state.dragStartRect.max.x - minW);
                                        }
                                        if (resizeRight)
                                        {
                                            newMaxX = std::clamp(std::round(state.dragStartRect.max.x + delta.x),
                                                state.dragStartRect.min.x + minW,
                                                state.dragStartRect.min.x + maxW);
                                        }

                                        const float newWidth = newMaxX - newMinX;
                                        activeItem->layout.width = newWidth;

                                        const float dMinX = newMinX - state.dragStartRect.min.x;
                                        const float dWidth = newWidth - state.dragStartSize.x;
                                        switch (activeItem->layout.horizontalAlignment)
                                        {
                                            case HorizontalAlignment::Center:
                                                activeItem->layout.position.x = std::round(state.dragStartPos.x + dMinX + 0.5f * dWidth);
                                                break;
                                            case HorizontalAlignment::Right:
                                                activeItem->layout.position.x = std::round(state.dragStartPos.x + dMinX + dWidth);
                                                break;
                                            default:
                                                activeItem->layout.position.x = std::round(state.dragStartPos.x + dMinX);
                                                break;
                                        }
                                    }

                                    if (resizeTop || resizeBottom)
                                    {
                                        activeItem->layout.heightMode = SizeMode::Fixed;
                                        if (activeItem->layout.verticalAlignment == VerticalAlignment::ExpandVertically)
                                            activeItem->layout.verticalAlignment = VerticalAlignment::Top;
                                        if (activeItem->layout.flex.basis >= 0.0f)
                                            activeItem->layout.flex.basis = -1.0f;

                                        const float minH = std::max(activeItem->layout.minHeight, 4.0f);
                                        const float maxH = std::max(activeItem->layout.maxHeight, minH);

                                        float newMinY = state.dragStartRect.min.y;
                                        float newMaxY = state.dragStartRect.max.y;
                                        if (resizeTop)
                                        {
                                            newMinY = std::clamp(std::round(state.dragStartRect.min.y + delta.y),
                                                state.dragStartRect.max.y - maxH,
                                                state.dragStartRect.max.y - minH);
                                        }
                                        if (resizeBottom)
                                        {
                                            newMaxY = std::clamp(std::round(state.dragStartRect.max.y + delta.y),
                                                state.dragStartRect.min.y + minH,
                                                state.dragStartRect.min.y + maxH);
                                        }

                                        const float newHeight = newMaxY - newMinY;
                                        activeItem->layout.height = newHeight;

                                        const float dMinY = newMinY - state.dragStartRect.min.y;
                                        const float dHeight = newHeight - state.dragStartSize.y;
                                        switch (activeItem->layout.verticalAlignment)
                                        {
                                            case VerticalAlignment::Middle:
                                                activeItem->layout.position.y = std::round(state.dragStartPos.y + dMinY + 0.5f * dHeight);
                                                break;
                                            case VerticalAlignment::Bottom:
                                                activeItem->layout.position.y = std::round(state.dragStartPos.y + dMinY + dHeight);
                                                break;
                                            default:
                                                activeItem->layout.position.y = std::round(state.dragStartPos.y + dMinY);
                                                break;
                                        }
                                    }

                                    if (Ref<WidgetBoxSizing> boxSizing = activeItem->As<WidgetBoxSizing>())
                                    {
                                        boxSizing->minSize = glm::min(boxSizing->minSize, glm::vec2(activeItem->layout.width, activeItem->layout.height));
                                        boxSizing->maxSize = glm::max(boxSizing->maxSize, glm::vec2(activeItem->layout.width, activeItem->layout.height));
                                    }

                                    activeItem->MarkLayoutDirty();
                                    activeItem->MarkPaintDirty();
                                    widget->SetDirtyFlag(true);
                                    widget->UpdateLayout(static_cast<uint32_t>(canvasW), static_cast<uint32_t>(canvasH));
                                }
                            }
                        }
                        else
                        {
                            state.activeHandle = WidgetResizeHandle::None;
                            state.activeWidgetId = 0;
                        }
                    }

                    // 5. Keyboard nudging / delete when preview is hovered
                    if (isViewportHovered && !ImGui::GetIO().WantTextInput && selectedItem && selectedItem.get() != widget->GetRoot())
                    {
                        const float nudgeStep = ImGui::GetIO().KeyShift ? 10.0f : 1.0f;
                        glm::vec2 nudge(0.0f);
                        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  nudge.x -= nudgeStep;
                        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nudge.x += nudgeStep;
                        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    nudge.y -= nudgeStep;
                        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))  nudge.y += nudgeStep;

                        if (nudge.x != 0.0f || nudge.y != 0.0f)
                        {
                            selectedItem->layout.position += nudge;
                            selectedItem->MarkLayoutDirty();
                            selectedItem->MarkPaintDirty();
                            widget->SetDirtyFlag(true);
                            widget->UpdateLayout(static_cast<uint32_t>(canvasW), static_cast<uint32_t>(canvasH));
                        }

                        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
                        {
                            if (widget->RemoveItem(selectedItem->id))
                            {
                                selectedItemId = widget->GetRoot() ? widget->GetRoot()->id : 0;
                                selectedItem = widget->GetRoot() ? widget->GetRoot()->As<IWidgetItem>() : nullptr;
                                widget->UpdateLayout(static_cast<uint32_t>(canvasW), static_cast<uint32_t>(canvasH));
                            }
                        }
                    }

                    // 6. Drag & drop onto preview
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *p = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (p->Data && p->DataSize == sizeof(AssetHandle) && assetManager)
                            {
                                const AssetHandle dropped = *static_cast<const AssetHandle *>(p->Data);
                                if (assetManager->GetMetaData(dropped).type == AssetType::Widget && dropped != widget->handle)
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
                                        case WidgetType::Container: newId = widget->AddContainer(container); break;
                                        case WidgetType::Button:    newId = widget->AddButton(container, "Button"); break;
                                        case WidgetType::Label:     newId = widget->AddLabel(container, "Label"); break;
                                        case WidgetType::Image:     newId = widget->AddImage(container); break;
                                        case WidgetType::BoxSizing: newId = widget->AddBoxSizing(container); break;
                                        case WidgetType::Overlay:   newId = widget->AddOverlay(container); break;
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
                                                const Rect parentRect = container->GetAlignedRect();
                                                newItem->layout.positionType = PositionType::Absolute;
                                                newItem->SetPosition(glm::round(glm::vec2(canvasMouse.x - parentRect.min.x, canvasMouse.y - parentRect.min.y)));
                                                selectedItem = newItem;
                                            }
                                        }
                                        widget->SetDirtyFlag(true);
                                        widget->UpdateLayout(static_cast<uint32_t>(canvasW), static_cast<uint32_t>(canvasH));
                                    }
                                }
                            }
                        }

                        ImGui::EndDragDropTarget();
                    }

                    // 7. Draw workspace, canvas image, and editor overlays
                    ImDrawList *drawList = ImGui::GetWindowDrawList();
                    const ImVec2 clipMax = { previewRegionPos.x + previewRegionSize.x, previewRegionPos.y + previewRegionSize.y };
                    drawList->PushClipRect(previewRegionPos, clipMax, true);

                    // Dark outer workspace background
                    drawList->AddRectFilled(previewRegionPos, clipMax, IM_COL32(20, 22, 26, 255));

                    const ImVec2 canvasScreenMin = imagePos;
                    const ImVec2 canvasScreenMax = { imagePos.x + scaledImageSize.x, imagePos.y + scaledImageSize.y };
                    const ImVec2 visMin = { std::max(canvasScreenMin.x, previewRegionPos.x), std::max(canvasScreenMin.y, previewRegionPos.y) };
                    const ImVec2 visMax = { std::min(canvasScreenMax.x, clipMax.x), std::min(canvasScreenMax.y, clipMax.y) };

                    if (previewTexture && previewTexture->GetShaderResourceView()
                        && previewTexture->GetCurrentLayout() != nri::Layout::UNDEFINED
                        && visMax.x > visMin.x && visMax.y > visMin.y)
                    {
                        const float rtW = std::max(1.0f, static_cast<float>(previewTexture->GetWidth()));
                        const float rtH = std::max(1.0f, static_cast<float>(previewTexture->GetHeight()));
                        const ImVec2 uv0 = { (visMin.x - previewRegionPos.x) / rtW, (visMin.y - previewRegionPos.y) / rtH };
                        const ImVec2 uv1 = { (visMax.x - previewRegionPos.x) / rtW, (visMax.y - previewRegionPos.y) / rtH };
                        drawList->AddImage(UI::GetTextureID(previewTexture), visMin, visMax, uv0, uv1);
                    }

                    // Subtle canvas frame border
                    drawList->AddRect(canvasScreenMin, canvasScreenMax, IM_COL32(95, 105, 120, 180), 0.0f, 0, 1.0f);

                    WidgetEditor::DrawPreviewOverlay(drawList, widget, selectedItemId, imagePos, scaledImageSize, canvasW, canvasH);

                    if (selectedItem && widget->GetRoot() && selectedItem->id != widget->GetRoot()->id)
                    {
                        WidgetEditor::DrawAnchorPoints(drawList, selectedItem, widget, imagePos, scaledImageSize, canvasW, canvasH);
                    }

                    // Bottom-left status overlay inside preview
                    if (isViewportHovered)
                    {
                        const std::string cursorInfo = std::format("Canvas: ({:.0f}, {:.0f})", canvasMouse.x, canvasMouse.y);
                        drawList->AddText({ previewRegionPos.x + 8.0f, clipMax.y - 22.0f }, IM_COL32(180, 195, 210, 200), cursorInfo.c_str());
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
}
