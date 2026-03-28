//Copyright (c) 2026 Evangelion Manuhutu

#include "asset_editor_panel.hpp"

#include "../editor_layer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/animation/skeletal_animation.hpp"

#include <algorithm>
#include <format>
#include <ranges>

namespace ignite
{
    
	AssetEditorPanel::AssetEditorPanel(const char *windowTitle, EditorLayer *editor)
		: IPanel(windowTitle, editor)
	{
	}

	void AssetEditorPanel::OnGuiRender()
	{
		for (auto &assetData : m_Assets)
		{
			if (!assetData.isOpen)
				continue;

			if (assetData.requestFocus)
			{
				ImGui::SetNextWindowFocus();
			}

			bool isOpen = assetData.isOpen;
			if (!ImGui::Begin(assetData.windowTitle.c_str(), &isOpen))
			{
				assetData.isOpen = isOpen;
				assetData.requestFocus = false;
				ImGui::End();
				continue;
			}

			assetData.requestFocus = false;

			if (!DrawAssetEditorHeader(assetData))
			{
				ImGui::End();
				assetData.isOpen = isOpen;
				continue;
			}

			if (!assetData.asset || !assetData.asset->IsReady())
			{
				ImGui::Text("Loading asset...");
				ImGui::End();
				assetData.isOpen = isOpen;
				continue;
			}

			switch (assetData.metadata.type)
			{
			case AssetType::SkeletalAnimation:
			{
				Ref<SkeletalAnimation> anim = std::dynamic_pointer_cast<SkeletalAnimation>(assetData.asset);
				if (!anim)
					break;

				RenderSkeletalAnimationEditor(anim);
			}
			break;

			default:
				ImGui::Text("Asset type '%s' editor is not implemented yet.", AssetTypeToString(assetData.metadata.type).c_str());
				break;
			}

			ImGui::End();
			assetData.isOpen = isOpen;
		}

		std::erase_if(m_Assets, [](const AssetEditorData &assetData)
			{
				return !assetData.isOpen;
			});
	}

	bool AssetEditorPanel::DrawAssetEditorHeader(AssetEditorData &assetData)
	{
		ImGui::Text("Asset: %s", assetData.metadata.filepath.filename().string().c_str());
		ImGui::Text("Type: %s", AssetTypeToString(assetData.metadata.type).c_str());
		ImGui::Separator();

		const bool isDirty = assetData.asset && assetData.asset->IsDirty();
		ImGui::Text("Status: %s", isDirty ? "Modified" : "Saved");
		ImGui::SameLine();

		if (ImGui::Button("Save"))
		{
			if (!SaveAsset(assetData))
			{
				LOG_ERROR("[Asset Editor] Failed to save asset: {}", assetData.metadata.filepath.generic_string());
			}
		}

		ImGui::Separator();
		return true;
	}

   void AssetEditorPanel::RenderSkeletalAnimationEditor(const Ref<SkeletalAnimation> &animation)
	{
		ImGui::Text("Name: %s", animation->name.c_str());
		ImGui::Text("Duration: %.3f", animation->duration);
		ImGui::Text("Ticks Per Second: %.3f", animation->ticksPerSeconds);
		ImGui::Text("Channels: %zu", animation->channels.size());

		Project *project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
        if (!project)
		{
			return;
		}

		auto &assetManager = project->GetAssetManager();

		const AssetHandle skeletonHandle = AssetHandle(animation->GetSkeletonHandle());
		if (skeletonHandle != AssetHandle(0))
		{
			const AssetMetaData &skeletonMetadata = assetManager.GetMetaData(skeletonHandle);
			if (skeletonMetadata.type == AssetType::Skeleton)
			{
				ImGui::Text("Skeleton: %s", skeletonMetadata.filepath.generic_string().c_str());
			}
			else
			{
				ImGui::Text("Skeleton Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(skeletonHandle)));
			}
		}
		else
		{
			ImGui::Text("Skeleton: <none>");
		}

		ImGui::Button("Drop Skeleton Here", ImVec2(220.0f, 0.0f));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
			{
				if (payload->Data && payload->DataSize == sizeof(AssetHandle))
				{
					const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
					const AssetMetaData &droppedMetadata = assetManager.GetMetaData(droppedHandle);
					if (droppedMetadata.type == AssetType::Skeleton)
					{
						animation->SetSkeletonHandle(UUID(static_cast<uint64_t>(droppedHandle)));
						animation->SetDirtyFlag(true);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	bool AssetEditorPanel::SaveAsset(AssetEditorData &assetData)
	{
		if (!m_EditorLayer || !m_EditorLayer->GetActiveProject() || !assetData.asset)
		{
			return false;
		}

		Project *project = m_EditorLayer->GetActiveProject().get();
		const std::filesystem::path savePath = project->GetAssetFilepath(assetData.metadata.filepath);

		switch (assetData.metadata.type)
		{
		case AssetType::SkeletalAnimation:
		{
			Ref<SkeletalAnimation> animation = std::dynamic_pointer_cast<SkeletalAnimation>(assetData.asset);
			if (!animation)
			{
				return false;
			}

			BinarySerializer::SerializeAnimation(animation, savePath);
			animation->SetDirtyFlag(false);
			return true;
		}

		default:
			return false;
		}
	}

	void AssetEditorPanel::OnEvent(Event &event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<AssetEditorOpenEvent>(BIND_CLASS_EVENT_FN(AssetEditorPanel::OnAssetEditorOpenEvent));
	}

	bool AssetEditorPanel::OnAssetEditorOpenEvent(AssetEditorOpenEvent &event)
	{
		auto handle = event.GetAssetHandle();
		auto &metadata = event.GetAssetMetaData();
		if (metadata.type == AssetType::Invalid || handle == AssetHandle(0) || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
			return false;

		// Check if the asset window is already open.
		auto it = std::ranges::find(m_Assets, handle, &AssetEditorData::handle);
		if (it != m_Assets.end())
		{
			it->isOpen = true;
            it->requestFocus = true;
			return true;
		}

		auto &assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
		Ref<Asset> asset = assetManager.GetAsset(handle);
		if (!asset)
		{
			asset = assetManager.GetAssetImmediate(handle);
		}

		if (asset)
		{
			std::string assetName = metadata.filepath.filename().string();
			if (assetName.empty())
			{
				assetName = metadata.filepath.generic_string();
			}

			AssetEditorData data;
			data.asset = asset;
			data.metadata = metadata;
			data.handle = handle;
			data.isOpen = true;
            data.requestFocus = true;
			data.windowTitle = std::format("{} - {}###asset_editor_{}", AssetTypeToString(metadata.type), assetName, static_cast<uint64_t>(handle));

			m_Assets.push_back(std::move(data));
			return true;
		}

		return false;
   }
}
