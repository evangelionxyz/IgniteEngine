//Copyright (c) 2026 Evangelion Manuhutu

#include "asset_editor_panel.hpp"

#include "../editor_layer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/texture.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <iterator>
#include <ranges>
#include <unordered_map>

namespace ignite
{
	namespace
	{
		struct TextureEditorState
		{
			TextureCreateInfo createInfo;
			bool initialized = false;
		};

		static std::unordered_map<uint64_t, TextureEditorState> s_TextureEditorState;

		static const char *TextureFormatToString(nvrhi::Format format)
		{
			switch (format)
			{
				case nvrhi::Format::RGBA8_UNORM: return "RGBA8_UNORM";
				case nvrhi::Format::RGBA32_FLOAT: return "RGBA32_FLOAT";
				default: return "UNKNOWN";
			}
		}

		static const char *SamplerAddressModeToString(nvrhi::SamplerAddressMode mode)
		{
			switch (mode)
			{
				case nvrhi::SamplerAddressMode::Repeat: return "Repeat";
				case nvrhi::SamplerAddressMode::ClampToEdge: return "ClampToEdge";
				case nvrhi::SamplerAddressMode::ClampToBorder: return "ClampToBorder";
				default: return "Other";
			}
		}
	}


	AssetEditorPanel::AssetEditorPanel(const char *windowTitle, EditorLayer *editor)
		: IPanel(windowTitle, editor)
	{
	}

	std::filesystem::path AssetEditorPanel::BuildUniqueAssetPath(const std::filesystem::path &baseDirectory, const std::string &baseName, const std::string &extension) const
	{
		std::filesystem::path candidate = baseDirectory / (baseName + extension);
		if (!std::filesystem::exists(candidate))
		{
			return candidate;
		}

		uint32_t suffix = 1;
		while (true)
		{
			candidate = baseDirectory / std::format("{}_{}{}", baseName, suffix, extension);
			if (!std::filesystem::exists(candidate))
			{
				return candidate;
			}
			++suffix;
		}
	}

	void AssetEditorPanel::RenderCreateAssetPopup()
	{
		if (!m_CreateRequest.open || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
		{
			return;
		}

		Project *project = m_EditorLayer->GetActiveProject().get();
		auto &assetManager = project->GetAssetManager();

		if (m_CreateRequest.type == AssetType::Material2D && !m_CreateRequest.asset)
		{
			m_CreateRequest.asset = CreateRef<Material2D>();
		}

		ImGui::OpenPopup("Create Asset");
		if (ImGui::BeginPopupModal("Create Asset", &m_CreateRequest.open, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Asset Type: %s", AssetTypeToString(m_CreateRequest.type).c_str());
			ImGui::InputText("Name", m_CreateRequest.nameBuffer, sizeof(m_CreateRequest.nameBuffer));

			if (m_CreateRequest.type == AssetType::Material2D)
			{
				Ref<Material2D> material2D = std::dynamic_pointer_cast<Material2D>(m_CreateRequest.asset);
				if (!material2D)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for Material2D creation.");
					ImGui::EndPopup();
					return;
				}

				ImGui::Separator();
				RenderMaterial2DEditor(material2D);
			}

			ImGui::Separator();
			if (ImGui::Button("Create"))
			{
				std::string assetName = m_CreateRequest.nameBuffer;
				if (assetName.empty())
				{
					assetName = "NewAsset";
				}

				std::filesystem::path targetDirectory = m_CreateRequest.targetDirectory.empty() ? project->GetAssetDirectory() : m_CreateRequest.targetDirectory;
				if (!std::filesystem::exists(targetDirectory))
				{
					std::filesystem::create_directories(targetDirectory);
				}

				const std::string extension = GetAssetExtensionFromType(m_CreateRequest.type);
				const std::filesystem::path fullAssetPath = BuildUniqueAssetPath(targetDirectory, assetName, extension);

				bool created = false;
				Ref<Asset> createdAsset = nullptr;
				if (m_CreateRequest.type == AssetType::Material2D)
				{
					Ref<Material2D> material2D = std::dynamic_pointer_cast<Material2D>(m_CreateRequest.asset);
					if (material2D)
					{
						material2D->name = assetName;
						Material2DSerializer serializer(material2D);
						created = serializer.Serialize(fullAssetPath);
						if (created)
						{
							material2D->SetDirtyFlag(false);
							material2D->SetReadyFlag(true);
							createdAsset = material2D;
						}
					}
				}

				if (created && createdAsset)
				{
					AssetHandle handle = AssetHandle();
					AssetMetaData metadata;
					metadata.type = m_CreateRequest.type;
					metadata.filepath = project->GetAssetRelativeFilepath(fullAssetPath);

					createdAsset->handle = handle;
					assetManager.AssignMetaData(handle, metadata);
					assetManager.AssignAsset(handle, createdAsset);

					AssetEditorData data;
					data.asset = createdAsset;
					data.metadata = metadata;
					data.handle = handle;
					data.isOpen = true;
					data.requestFocus = true;
					data.windowTitle = std::format("{} - {}###asset_editor_{}", AssetTypeToString(metadata.type), fullAssetPath.filename().string(), static_cast<uint64_t>(handle));
					m_Assets.push_back(std::move(data));

					m_CreateRequest = {};
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				m_CreateRequest = {};
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void AssetEditorPanel::RenderMaterial2DEditor(const Ref<Material2D> &material2D)
	{
		if (!material2D || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
		{
			return;
		}

		auto &assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();

		const char *materialTypeLabel = material2D->data.type == MATERIAL_2D_TYPE_LIT ? "Lit" : "Unlit";
		if (ImGui::BeginCombo("Material Type", materialTypeLabel))
		{
			if (ImGui::Selectable("Unlit", material2D->data.type == MATERIAL_2D_TYPE_UNLIT))
			{
				material2D->data.type = MATERIAL_2D_TYPE_UNLIT;
				material2D->SetDirtyFlag(true);
			}

			if (ImGui::Selectable("Lit", material2D->data.type == MATERIAL_2D_TYPE_LIT))
			{
				material2D->data.type = MATERIAL_2D_TYPE_LIT;
				material2D->SetDirtyFlag(true);
			}

			ImGui::EndCombo();
		}

		if (ImGui::ColorEdit4("Base Color", &material2D->data.baseColor.x))
		{
			material2D->SetDirtyFlag(true);
		}

		if (ImGui::ColorEdit4("Additive Color", &material2D->data.additiveColor.x))
		{
			material2D->SetDirtyFlag(true);
		}

		if (ImGui::DragFloat2("Tiling", &material2D->data.tilingFactor.x, 0.025f))
		{
			material2D->SetDirtyFlag(true);
		}

		std::string textureLabel = material2D->textureHandle == AssetHandle(0) ? "Drop Texture Here" : "Texture Loaded";
		ImGui::Button(textureLabel.c_str(), ImVec2(220.0f, 0.0f));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
			{
				if (payload->Data && payload->DataSize == sizeof(AssetHandle))
				{
					const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
					const AssetMetaData &droppedMetadata = assetManager.GetMetaData(droppedHandle);
					if (droppedMetadata.type == AssetType::Texture)
					{
						material2D->textureHandle = droppedHandle;
						material2D->SetDirtyFlag(true);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (material2D->textureHandle != AssetHandle(0))
		{
			ImGui::SameLine();
			if (ImGui::Button("Clear Texture"))
			{
				material2D->textureHandle = AssetHandle(0);
				material2D->SetDirtyFlag(true);
			}
		}

		ImGui::Text("Texture Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(material2D->textureHandle)));
	}

	void AssetEditorPanel::RenderTextureEditor(AssetEditorData &assetData, const Ref<Texture> &texture)
	{
		if (!texture || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
		{
			return;
		}

		Project *project = m_EditorLayer->GetActiveProject().get();
		auto &assetManager = project->GetAssetManager();

		const uint64_t stateKey = static_cast<uint64_t>(assetData.handle);
		TextureEditorState &state = s_TextureEditorState[stateKey];
		if (!state.initialized)
		{
			state.createInfo = texture->GetCreateInfo();
			state.initialized = true;
		}

		ImGui::Text("Path: %s", assetData.metadata.filepath.generic_string().c_str());
		ImGui::Text("Resolution: %d x %d", texture->GetWidth(), texture->GetHeight());
		ImGui::Text("Channels: %d", texture->GetChannels());
		ImGui::Text("Current Format: %s", TextureFormatToString(texture->GetFormat()));

		const float previewMaxWidth = std::min(320.0f, ImGui::GetContentRegionAvail().x);
		if (previewMaxWidth > 0.0f && texture->GetWidth() > 0 && texture->GetHeight() > 0)
		{
			const float aspectRatio = static_cast<float>(texture->GetWidth()) / static_cast<float>(texture->GetHeight());
			ImVec2 previewSize(previewMaxWidth, previewMaxWidth);
			if (aspectRatio > 1.0f)
			{
				previewSize.y = previewMaxWidth / aspectRatio;
			}
			else
			{
				previewSize.x = previewMaxWidth * aspectRatio;
			}

			ImGui::Text("Preview");
			ImTextureID textureId = reinterpret_cast<ImTextureID>(texture->GetHandle().Get());
			ImGui::Image(textureId, previewSize);
		}

		ImGui::Separator();

		int mipLevels = static_cast<int>(state.createInfo.mipLevels);
		if (ImGui::DragInt("Mip Levels", &mipLevels, 1.0f, 1, 16))
		{
			state.createInfo.mipLevels = static_cast<uint32_t>(std::max(mipLevels, 1));
		}

		int arraySize = static_cast<int>(state.createInfo.arraySize);
		if (ImGui::DragInt("Array Size", &arraySize, 1.0f, 1, 64))
		{
			state.createInfo.arraySize = static_cast<uint32_t>(std::max(arraySize, 1));
		}

		int sampleCount = static_cast<int>(state.createInfo.sampleCount);
		if (ImGui::DragInt("Sample Count", &sampleCount, 1.0f, 1, 16))
		{
			state.createInfo.sampleCount = static_cast<uint32_t>(std::max(sampleCount, 1));
		}

		int sampleQuality = static_cast<int>(state.createInfo.sampleQuality);
		if (ImGui::DragInt("Sample Quality", &sampleQuality, 1.0f, 0, 16))
		{
			state.createInfo.sampleQuality = static_cast<uint32_t>(std::max(sampleQuality, 0));
		}

		const nvrhi::Format formatOptions[] = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::RGBA32_FLOAT };
		int currentFormatIndex = 0;
		for (int i = 0; i < static_cast<int>(std::size(formatOptions)); ++i)
		{
			if (state.createInfo.format == formatOptions[i])
			{
				currentFormatIndex = i;
				break;
			}
		}

		if (ImGui::BeginCombo("Import Format", TextureFormatToString(formatOptions[currentFormatIndex])))
		{
			for (int i = 0; i < static_cast<int>(std::size(formatOptions)); ++i)
			{
				const bool selected = i == currentFormatIndex;
				if (ImGui::Selectable(TextureFormatToString(formatOptions[i]), selected))
				{
					state.createInfo.format = formatOptions[i];
				}

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		const nvrhi::SamplerAddressMode addressModeOptions[] =
		{
			nvrhi::SamplerAddressMode::Repeat,
			nvrhi::SamplerAddressMode::ClampToEdge,
			nvrhi::SamplerAddressMode::ClampToBorder
		};

		auto drawAddressModeCombo = [&addressModeOptions](const char *label, nvrhi::SamplerAddressMode &mode)
		{
			if (ImGui::BeginCombo(label, SamplerAddressModeToString(mode)))
			{
				for (const auto option : addressModeOptions)
				{
					const bool selected = option == mode;
					if (ImGui::Selectable(SamplerAddressModeToString(option), selected))
					{
						mode = option;
					}

					if (selected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		};

		drawAddressModeCombo("Wrap U", state.createInfo.samplerAddressU);
		drawAddressModeCombo("Wrap V", state.createInfo.samplerAddressV);
		drawAddressModeCombo("Wrap W", state.createInfo.samplerAddressW);

		ImGui::Checkbox("Linear Filtering", &state.createInfo.samplerLinearFiltering);
		ImGui::Checkbox("Flip Vertically", &state.createInfo.flip);
		ImGui::Checkbox("Keep CPU Data", &state.createInfo.keepCpuData);
		ImGui::Checkbox("Keep Initial State", &state.createInfo.keepInitialState);

		ImGui::Separator();
		if (ImGui::Button("ReImport"))
		{
			AssetMetaData importMetadata = assetData.metadata;
			importMetadata.filepath = project->GetAssetFilepath(assetData.metadata.filepath);

			Ref<Texture> reimportedTexture = AssetImporter::ImportTexture(assetData.handle, importMetadata, state.createInfo);
			if (reimportedTexture)
			{
				reimportedTexture->handle = assetData.handle;
				assetManager.AssignAsset(assetData.handle, reimportedTexture);
				assetData.asset = reimportedTexture;
				state.createInfo = reimportedTexture->GetCreateInfo();
			}
		}
	}

	void AssetEditorPanel::OnGuiRender()
	{
        RenderCreateAssetPopup();

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
			case AssetType::Texture:
			{
				Ref<Texture> texture = std::dynamic_pointer_cast<Texture>(assetData.asset);
				if (!texture)
					break;

				RenderTextureEditor(assetData, texture);
			}
			break;

			case AssetType::Material2D:
			{
				Ref<Material2D> material2D = std::dynamic_pointer_cast<Material2D>(assetData.asset);
				if (!material2D)
					break;

				RenderMaterial2DEditor(material2D);
			}
			break;

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
		case AssetType::Material2D:
		{
			Ref<Material2D> material2D = std::dynamic_pointer_cast<Material2D>(assetData.asset);
			if (!material2D)
			{
				return false;
			}

			Material2DSerializer serializer(material2D);
			if (!serializer.Serialize(savePath))
			{
				return false;
			}

			material2D->SetDirtyFlag(false);
			return true;
		}

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
		dispatcher.Dispatch<AssetEditorCreateEvent>(BIND_CLASS_EVENT_FN(AssetEditorPanel::OnAssetEditorCreateEvent));
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

	bool AssetEditorPanel::OnAssetEditorCreateEvent(AssetEditorCreateEvent &event)
	{
       if (!m_EditorLayer || !m_EditorLayer->GetActiveProject())
			return false;

		if (event.GetAssetType() == AssetType::Invalid)
			return false;

		m_CreateRequest = {};
		m_CreateRequest.type = event.GetAssetType();
		m_CreateRequest.targetDirectory = event.GetTargetDirectory();
		m_CreateRequest.open = true;

		if (m_CreateRequest.type == AssetType::Material2D)
		{
           m_CreateRequest.asset = CreateRef<Material2D>();
			std::memcpy(m_CreateRequest.nameBuffer, "NewMaterial2D", sizeof("NewMaterial2D"));
		}

		return true;
	}

}
