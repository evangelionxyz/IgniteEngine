// Copyright (c) 2026 Evangelion Manuhutu

#include "widget_editor.hpp"
#include "states.hpp"
#include "ext/editor_ui.hpp"
#include "ignite/asset/asset_manager.hpp"

namespace ignite
{
    namespace
    {
        static const char *GetWidgetTypeLabel(const IWidgetItem &item)
        {
            switch (item.GetWidgetType())
            {
                case WidgetType::Container: return "Container";
                case WidgetType::Button: return "Button";
                case WidgetType::Label: return "Label";
                default: return "Widget";
            }
        }

        static std::string GetWidgetTreeLabel(const Ref<IWidgetItem> &item, const WidgetCanvas *canvas)
        {
            if (!item)
            {
                return "<null>";
            }

            if (canvas && canvas->GetRoot() == item.get())
            {
                return std::format("Canvas Root [{}]##widget_node_{}", item->id, item->id);
            }

            const std::string displayName = item->name.empty() ? GetWidgetTypeLabel(*item) : item->name;
            return std::format("{} [{}]##widget_node_{}", displayName, item->id, item->id);
        }
    }

    void WidgetEditor::DrawWidgetTreeRecursive(const Ref<IWidgetItem> &item, int &selectedItemId, const WidgetCanvas *canvas)
    {
        if (!item)
        {
            return;
        }

        const bool selected = selectedItemId == item->id;
        const bool hasChildren = !item->children.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
        if (!hasChildren)
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (selected)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<intptr_t>(item->id)), flags, "%s", GetWidgetTreeLabel(item, canvas).c_str());
        if (ImGui::IsItemClicked())
        {
            selectedItemId = item->id;
        }

        if (opened)
        {
            for (const Ref<IWidgetItem> &child : item->children)
            {
                DrawWidgetTreeRecursive(child, selectedItemId, canvas);
            }

            ImGui::TreePop();
        }
    }

    WidgetContainer *WidgetEditor::ResolveInsertionParent(const Ref<IWidgetItem> &selectedItem, const Ref<WidgetCanvas> &widget)
    {
        if (!widget)
        {
            return nullptr;
        }

        if (selectedItem && selectedItem->GetWidgetType() == WidgetType::Container)
        {
            return selectedItem->As<WidgetContainer>().get();
        }

        if (selectedItem && selectedItem->parent && selectedItem->parent->GetWidgetType() == WidgetType::Container)
        {
            return dynamic_cast<WidgetContainer *>(selectedItem->parent);
        }

        return widget->GetRoot();
    }

    void WidgetEditor::DrawWidgetDetails(const Ref<WidgetCanvas> &widget, int &selectedItemId, AssetManager *assetManager)
    {
        if (!widget)
        {
            return;
        }

        Ref<IWidgetItem> selectedItem = nullptr;
        if (selectedItemId != 0 && widget->GetItems().contains(selectedItemId))
        {
            selectedItem = widget->GetItems().at(selectedItemId);
        }
        else
        {
            if (widget->GetRoot())
            {
                selectedItem = widget->GetRoot()->As<IWidgetItem>();
            }
            if (selectedItem)
            {
                selectedItemId = selectedItem->id;
            }
        }

        if (!selectedItem)
        {
            ImGui::TextDisabled("No widget selected.");
            return;
        }

        ImGui::PushID(selectedItem.get());

        bool dirty = false;

        ImGui::SeparatorText("Selected Item");

        char nameBuffer[256] {};
        std::strncpy(nameBuffer, selectedItem->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
        {
            selectedItem->name = nameBuffer;
            dirty = true;
        }

        bool visible = selectedItem->IsVisible();
        if (ImGui::Checkbox("Visible", &visible))
        {
            selectedItem->SetVisible(visible);
            dirty = true;
        }

        int alignment = static_cast<int>(selectedItem->alignment);
        const char *alignmentNames[] =
        {
            "Top Left", "Top Center", "Top Right",
            "Center Left", "Center", "Center Right",
            "Bottom Left", "Bottom Center", "Bottom Right"
        };
        if (ImGui::Combo("Alignment", &alignment, alignmentNames, IM_ARRAYSIZE(alignmentNames)))
        {
            selectedItem->alignment = static_cast<WidgetAlignment>(alignment);
            dirty = true;
        }

        int sizingMode = static_cast<int>(selectedItem->sizingMode);
        const char *sizingModeNames[] = { "Default", "Expand To Parent" };
        if (ImGui::Combo("Sizing Mode", &sizingMode, sizingModeNames, IM_ARRAYSIZE(sizingModeNames)))
        {
            selectedItem->sizingMode = static_cast<SizingMode>(sizingMode);
            dirty = true;
        }

        dirty |= UI::DrawVec2Control("Position", selectedItem->position, 1.0f);
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

        if (Ref<WidgetContainer> container = selectedItem->As<WidgetContainer>())
        {
            ImGui::PushID(container.get());

            ImGui::SeparatorText("Container");

            int layout = static_cast<int>(container->layout);
            const char *layoutNames[] = { "Horizontal", "Vertical", "Grid", "Absolute" };
            if (ImGui::Combo("Layout", &layout, layoutNames, IM_ARRAYSIZE(layoutNames)))
            {
                container->layout = static_cast<LayoutMode>(layout);
                dirty = true;
            }

            dirty |= UI::DrawFloatControl("Padding", &container->padding, 0.5f, 0.0f, 512.0f);
            dirty |= UI::DrawFloatControl("Margin", &container->margin, 0.5f, 0.0f, 512.0f);
            dirty |= UI::DrawFloatControl("Gap", &container->gap, 0.5f, 0.0f, 512.0f);

            if (ImGui::Button("+ Container##container"))
            {
                selectedItemId = widget->AddContainer(container.get());
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Button"))
            {
                selectedItemId = widget->AddButton(container.get(), "Button");
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Label"))
            {
                selectedItemId = widget->AddLabel(container.get(), "Label");
                dirty = true;
            }

            ImGui::PopID();
        }

        if (Ref<WidgetButton> button = selectedItem->As<WidgetButton>())
        {
            ImGui::SeparatorText("Button");

            AssetHandle fontHandle = button->GetFontHandle();
            const std::string fontName = fontHandle == AssetHandle(0) ? "Drop Font Here" : assetManager->GetAssetDisplayName(fontHandle);
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

            float textSize = button->GetFontSize();
            float kerning = button->GetKerning();
            float lineSpacing = button->GetLineSpacing();
            if (UI::DrawFloatControl("Text Size", &textSize, 0.025f, 0.0f, 120.0f))
            {
                button->SetFontSize(textSize);
                dirty = true;
            }
            if (UI::DrawFloatControl("Kerning", &kerning, 0.01f, 0.0f, FLT_MAX))
            {
                button->SetKerning(kerning);
                dirty = true;
            }
            if (UI::DrawFloatControl("Line Spacing", &lineSpacing, 0.01f, 0.0f, FLT_MAX))
            {
                button->SetLineSpacing(lineSpacing);
                dirty = true;
            }

            dirty |= ImGui::ColorEdit4("Normal Color", &button->normalColor.x);
            dirty |= ImGui::ColorEdit4("Hover Color", &button->hoverColor.x);
            dirty |= ImGui::ColorEdit4("Pressed Color", &button->pressedColor.x);
            dirty |= ImGui::ColorEdit4("Border Color", &button->borderColor.x);

            AssetHandle imageHandle = button->GetImageHandle();
            std::string imageName = imageHandle == AssetHandle(0) ? "Drop Texture Here" : assetManager->GetAssetDisplayName(imageHandle);
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

            float kerning = label->GetKerning();
            float lineSpacing = label->GetLineSpacing();
            if (ImGui::DragFloat("Kerning", &kerning, 0.01f) || ImGui::DragFloat("Line Spacing", &lineSpacing, 0.01f))
            {
                label->SetKerning(kerning);
                label->SetLineSpacing(lineSpacing);
                dirty = true;
            }

            AssetHandle fontHandle = label->GetFontHandle();
            const std::string fontName = fontHandle == AssetHandle(0) ? "Drop Font Here" : assetManager->GetAssetDisplayName(fontHandle);
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

        WidgetContainer *insertionParent = ResolveInsertionParent(selectedItem, widget);
        if (insertionParent)
        {
            ImGui::SeparatorText("Add Child");
            if (ImGui::Button("+ Container##container"))
            {
                selectedItemId = widget->AddContainer(insertionParent);
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Button##button"))
            {
                selectedItemId = widget->AddButton(insertionParent, "Button");
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Label##label"))
            {
                selectedItemId = widget->AddLabel(insertionParent, "Label");
                dirty = true;
            }
        }

        if (selectedItem->id != widget->GetRoot()->id)
        {
            if (ImGui::Button("Remove Selected Item"))
            {
                if (widget->RemoveItem(selectedItem->id))
                {
                    selectedItemId = widget->GetRoot() ? widget->GetRoot()->id : 0;
                    dirty = true;
                }
            }
        }

        if (dirty)
        {
            widget->SetDirtyFlag(true);
        }

        ImGui::PopID();
    }
}
