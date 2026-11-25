// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "editor_ui.hpp"

#include "ignite/asset/asset_manager.hpp"

namespace ignite::UI
{
    bool DrawAssetDropTarget(
        const std::string &title,
        const std::string &buttonLabel,
        const std::initializer_list<AssetType> &supportedTypes,
        AssetHandle *outHandle,
        AssetManager *assetManager,
        const char *dropSource)
    {
        ImGui::PushID(title.c_str());

        LOG_ASSERT(outHandle, "Out handle is null!, please provide a valid asset handle");

        bool edited = false;

        constexpr float closeButtonWidth = 26.0f;
        const float dropButtonWidth = (*outHandle != AssetHandle(0))
            ? std::max(40.0f, ImGui::GetContentRegionAvail().x - closeButtonWidth - 6.0f)
            : std::max(40.0f, ImGui::GetContentRegionAvail().x);

        ImGui::TextUnformatted(title.c_str());
        Button(buttonLabel.c_str(), ButtonVariant::Outline, ButtonSize::Small, ImVec2(dropButtonWidth, 26.0f));
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
            ImGui::SameLine(0.0f, 6.0f);
            if (IconButton("##clear_asset", ICON_LC_X, ButtonVariant::Outline, ButtonSize::IconSm, "Clear asset"))
            {
                *outHandle = AssetHandle(0);
                edited = true;
            }
        }

        ImGui::PopID();
        return edited;
    }
}
