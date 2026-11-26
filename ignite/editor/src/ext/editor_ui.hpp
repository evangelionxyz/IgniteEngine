// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_EDITOR_UI_HPP
#define IGN_EDITOR_UI_HPP

#include "states.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/imgui/ui/ui.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>

namespace ignite::UI
{
    template<typename TOnChanged>
    inline void DrawTexturePreviewDropTarget(const char *label, AssetHandle &textureHandle, TOnChanged &&onChanged)
    {
        auto assetManager = AssetManager::GetInstance();
        const ThemeColors &t = GetTheme();

        ImGui::PushID(label);
        ImGui::TextUnformatted(label);

        if (ImGui::BeginTable("##texture_preview_table", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 96.0f);
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            const ImVec2 previewSize(96.0f, 96.0f);
            const ImVec2 previewMin = ImGui::GetCursorScreenPos();
            const ImVec2 previewMax(previewMin.x + previewSize.x, previewMin.y + previewSize.y);

            ImGui::InvisibleButton("##texture_preview", previewSize);

            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImU32 dark = IM_COL32(24, 24, 27, 255);
            const ImU32 light = IM_COL32(39, 39, 42, 255);
            constexpr float tileSize = 16.0f;
            for (float y = previewMin.y; y < previewMax.y; y += tileSize)
            {
                for (float x = previewMin.x; x < previewMax.x; x += tileSize)
                {
                    const auto ix = static_cast<int>((x - previewMin.x) / tileSize);
                    const auto iy = static_cast<int>((y - previewMin.y) / tileSize);
                    const ImU32 color = ((ix + iy) % 2 == 0) ? dark : light;
                    drawList->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + tileSize, previewMax.x), std::min(y + tileSize, previewMax.y)), color);
                }
            }

            Ref<Texture> texture = nullptr;
            if (textureHandle != AssetHandle(0))
            {
                texture = assetManager->GetAsset<Texture>(textureHandle);
            }

            if (texture && texture->GetShaderResourceView())
            {
                drawList->AddImage(GetTextureID(texture), previewMin, previewMax, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            }

            drawList->AddRect(previewMin, previewMax, ToU32(t.border), 0.0f, 0, 1.0f);

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                {
                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                    {
                        const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                        const AssetMetaData &droppedMetadata = assetManager->GetMetaData(droppedHandle);
                        if (droppedMetadata.type == AssetType::Texture)
                        {
                            textureHandle = droppedHandle;
                            onChanged();
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::TableNextColumn();

            std::string textureDisplayName = assetManager->GetAssetDisplayName(textureHandle);
            ImGui::TextWrapped("%s", textureDisplayName.c_str());
            {
                ScopedFont mono(FontVariant::Mono);
                ImGui::TextColored(ToImVec4(t.mutedForeground), "ID: %llu", static_cast<unsigned long long>(textureHandle));
            }

            if (textureHandle != AssetHandle(0))
            {
                ImGui::Spacing();
                if (Button("Clear", ButtonVariant::Outline, ButtonSize::Small, ImVec2(0.0f, 0.0f), ICON_LC_X))
                {
                    textureHandle = AssetHandle(0);
                    onChanged();
                }
            }

            ImGui::EndTable();
        }

        ImGui::PopID();
    }

    bool DrawAssetDropTarget(
        const std::string &title,
        const std::string &buttonLabel,
        const std::initializer_list<AssetType> &supportedTypes,
        AssetHandle *outHandle,
        AssetManager *assetManager,
        const char *dropSource = DND_PAYLOAD_CONTENT_BROWSER_ITEM);
}

#endif
