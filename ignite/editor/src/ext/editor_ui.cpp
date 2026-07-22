// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "editor_ui.hpp"

#include "ignite/asset/asset_manager.hpp"

namespace ignite::UI
{
    bool DrawAssetDropTarget(const std::string &title, const std::string &buttonLabel,
        const std::initializer_list<AssetType> &supportedTypes, AssetHandle *outHandle, AssetManager *assetManager, const char *dropSource)
    {
        ImGui::PushID(title.c_str());

        LOG_ASSERT(outHandle, "Out handle is null!, please provide a valid asset handle");

        bool edited = false;

        constexpr float closeButtonWidth = 24.0f;
        const float materialButtonWidth = ImGui::GetContentRegionAvail().x - closeButtonWidth - 4.0f;

        ImGui::TextUnformatted(title.c_str());
        ImGui::Button(buttonLabel.c_str(), ImVec2(materialButtonWidth, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(dropSource))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                    const AssetMetaData &metadata = assetManager->GetMetaData(droppedHandle);
                    for (const auto &type : supportedTypes)
                    {
                        if (metadata.type == type)
                        {
                            *outHandle = droppedHandle;
                            edited = true;
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (*outHandle != AssetHandle(0))
        {
            ImGui::SameLine();
            if (ImGui::Button("X", ImVec2(closeButtonWidth, 0.0f)))
            {
                *outHandle = AssetHandle(0);
                edited = true;
            }
        }
        ImGui::PopID();
        return edited;
    }

    void DrawCenteredText(const char *text, uint32_t color)
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();

        const auto &canvasPos = ImGui::GetCursorScreenPos();
        const auto &canvasSize = ImGui::GetContentRegionAvail();
        const auto textSize = ImGui::CalcTextSize(text);
        
        const ImVec2 center = { canvasPos.x + canvasSize.x / 2.0f, canvasPos.y + canvasSize.y / 2.0f };
        const ImVec2 textPos = { center.x - textSize.x / 2.0f, center.y - textSize.y / 2.0f };

        dl->AddText(textPos, color, text);
    }

}
