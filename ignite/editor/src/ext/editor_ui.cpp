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

	bool DrawButton(const char *label, const ImVec2 &size)
	{
		ImDrawList *dl = ImGui::GetWindowDrawList();

        ImGui::PushID(label);

        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 textSize = ImGui::CalcTextSize(label);

		const ImVec2 buttonMin = { canvasPos.x, canvasPos.y };
		const ImVec2 buttonMax = { buttonMin.x + size.x, buttonMin.y + size.y };

		// Interactive region via InvisibleButton
		ImGui::SetCursorScreenPos(buttonMin);
		ImGui::InvisibleButton("##custom_button", size);

		const bool isHovered = ImGui::IsItemHovered();
		const bool isActive = ImGui::IsItemActive();

        const bool isClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

		// Dynamic colors based on interaction state
		const ImU32 bgColor = isActive ? IM_COL32(25, 25, 25, 255) : (isHovered ? IM_COL32(82, 63, 25, 255) : IM_COL32(15, 15, 15, 180));
		const ImU32 borderColor = isHovered ? IM_COL32(255, 200, 128, 240) : IM_COL32(255, 128, 0, 220);

		// Subtle outer shadow
		dl->AddRectFilled({ buttonMin.x - 1.0f, buttonMin.y - 1.0f }, { buttonMax.x + 1.0f, buttonMax.y + 1.0f }, IM_COL32(0, 0, 0, isHovered ? 90 : 60), 7.0f);

		// Rounded banner background
		dl->AddRectFilled(buttonMin, buttonMax, bgColor, 6.0f);

		// Border outline
		dl->AddRect(buttonMin, buttonMax, borderColor, 6.0f, 0, 1.5f);

		// Centered text inside the banner
		const ImVec2 textPos = { buttonMin.x + size.x * 0.5f - textSize.x * 0.5f, buttonMin.y + size.y * 0.5f - textSize.y * 0.5f};
		dl->AddText(textPos, IM_COL32(255, 255, 255, 255), label);

        ImGui::PopID();

        return isClicked;
	}

	bool DrawImageButton(const char *strId, ImTextureID texID, const ImVec2 &size, const ImVec2 &uv0, const ImVec2 &uv1)
	{
		const ImVec2 &canvasPos = ImGui::GetCursorScreenPos();

		auto dl = ImGui::GetWindowDrawList();

		ImGui::PushID(strId);

		const ImVec2 buttonMin = { canvasPos.x, canvasPos.y };
		const ImVec2 buttonMax = { buttonMin.x + size.x, buttonMin.y + size.y };

		ImGui::SetCursorScreenPos(buttonMin);
		ImGui::InvisibleButton((const char *)texID, size);

		const bool isHovered = ImGui::IsItemHovered();
		const bool isClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

		const ImU32 bgColor = isClicked ? IM_COL32(82, 63, 25, 255) : (isHovered ? IM_COL32(82, 63, 25, 255) : IM_COL32(0, 0, 0, 0));
		const ImU32 borderColor = isClicked ? IM_COL32(255, 128, 0, 220) : (isHovered ? IM_COL32(255, 200, 128, 240) : IM_COL32(0, 0, 0, 0));

		// Rounded background
		dl->AddRectFilled(buttonMin, buttonMax, bgColor, 6.0f);
		dl->AddRect(buttonMin, buttonMax, borderColor, 3.0f);

		dl->AddImage(texID, buttonMin, buttonMax, uv0, uv1);
		ImGui::PopID();

		return isClicked;
	}

	bool DrawSelectImageButton(const char *strId, ImTextureID texID, const ImVec2 &size, bool active, const ImVec2 &uv0, const ImVec2 &uv1)
	{
		const ImVec2 &canvasPos = ImGui::GetCursorScreenPos();

		auto dl = ImGui::GetWindowDrawList();

		ImGui::PushID(strId);

		const ImVec2 buttonMin = { canvasPos.x, canvasPos.y };
		const ImVec2 buttonMax = { buttonMin.x + size.x, buttonMin.y + size.y };

		ImGui::SetCursorScreenPos(buttonMin);
		ImGui::InvisibleButton((const char *)texID, size);

		const bool isHovered = ImGui::IsItemHovered();
		const bool isClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

		const ImU32 bgColor = (active || isClicked) ? IM_COL32(82, 63, 25, 255) : (isHovered ? IM_COL32(82, 63, 25, 255) : IM_COL32(0, 0, 0, 0));
		const ImU32 borderColor = (active || isClicked) ? IM_COL32(255, 128, 0, 220) : (isHovered ? IM_COL32(255, 200, 128, 240) : IM_COL32(0, 0, 0, 0));

		// Rounded background
		dl->AddRectFilled(buttonMin, buttonMax, bgColor, 6.0f);
		dl->AddRect(buttonMin, buttonMax, borderColor, 3.0f);

		dl->AddImage(texID, buttonMin, buttonMax, uv0, uv1);
		ImGui::PopID();

		return isClicked;
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

	void DrawBannerText(const char *text, const ImVec2 &size)
	{
		ImDrawList *dl = ImGui::GetWindowDrawList();

		const auto &canvasPos = ImGui::GetCursorScreenPos();
		const auto textSize = ImGui::CalcTextSize(text);

		const ImVec2 bannerMax = { canvasPos.x + size.x, canvasPos.y + size.y };

		const ImU32 bgColor = IM_COL32(82, 63, 25, 255);
		const ImU32 borderColor = IM_COL32(255, 128, 0, 220);

		// Rounded background
		dl->AddRectFilled(canvasPos, bannerMax, bgColor, 6.0f);
		dl->AddRect(canvasPos, bannerMax, borderColor, 3.0f);

		// Draw text centered inside the banner
		const ImVec2 textPos = { canvasPos.x + size.x * 0.5f - textSize.x * 0.5f, canvasPos.y + size.y * 0.5f - textSize.y * 0.5f };
		dl->AddText(textPos, IM_COL32(255, 255, 255, 255), text);
	}

}
