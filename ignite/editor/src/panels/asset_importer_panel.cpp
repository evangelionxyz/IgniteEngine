// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "asset_importer_panel.hpp"

#include "ignite/project/project.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/serializer/serializer.hpp"

#include "editor_layer.hpp"

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <format>
#include <mutex>
#include <filesystem>

namespace ignite
{
    namespace
    {
        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool IsMeshImportDialogFile(const ignite::Path &filepath)
        {
            return GetAssetTypeFromExtension(ToLower(filepath.extension().string())) == AssetType::Mesh;
        }

        bool IsTextureImportDialogFile(const ignite::Path &filepath)
        {
            return GetAssetTypeFromExtension(ToLower(filepath.extension().string())) == AssetType::Texture;
        }

        bool IsFontImportDialogFile(const ignite::Path &filepath)
        {
            return GetAssetTypeFromExtension(ToLower(filepath.extension().string())) == AssetType::Font;
        }

        bool IsPathWithin(const std::filesystem::path &path, const std::filesystem::path &base)
        {
            auto baseIt = base.begin();
            auto pathIt = path.begin();

            for (; baseIt != base.end(); ++baseIt, ++pathIt)
            {
                if (pathIt == path.end() || *baseIt != *pathIt)
                {
                    return false;
                }
            }

            return true;
        }
    }

    AssetImporterPanel::AssetImporterPanel(const char *name, EditorLayer *editor)
        : IPanel(name, editor)
    {
        m_ImportSignalToken = SignalBus::Subscribe<AssetImportSignal>([this](const AssetImportSignal& signal)
        {
            OnAssetImportSignal(signal);
        });
    }

    AssetImporterPanel::~AssetImporterPanel()
    {
        if (m_EditorLayer)
        {
            m_EditorLayer->m_AssetImporterPanel = nullptr;
        }
        SignalBus::Unsubscribe<AssetImportSignal>(m_ImportSignalToken);
        m_ImportSignalToken = kInvalidSignalToken;
    }

    bool AssetImporterPanel::OnAssetImportSignal(const AssetImportSignal &signal)
    {
        IGN_PROFILE_FUNCTION();
        ResetImportState();

        m_TargetDirectory = signal.targetDirectory;
        if (m_TargetDirectory.empty())
        {
            m_TargetDirectory = m_EditorLayer->GetActiveProject()->GetAssetDirectory();
        }

        for (auto &payload : signal.payloads)
        {
			if (payload.metadata.type == AssetType::Invalid)
			{
				LOG_WARN("[Asset Importer] Skipped invalid asset type for file: {0}", payload.metadata.filepath.generic_string());
				continue;
			}

			if (!m_ImportQueues.contains(payload.metadata.type))
			{
				m_ImportTypeOrder.push_back(payload.metadata.type);
			}

			m_ImportQueues[payload.metadata.type].push(payload);
        }

        m_TotalImportItems = 0;
        for (const auto &[type, queue] : m_ImportQueues)
        {
            m_TotalImportItems += (int)queue.size();
        }
        m_ImportedItems = 0;

        if (!AdvanceToNextAsset())
        {
            return false;
        }

        m_ShowImporterWindow = true;
        return true;
    }

    void AssetImporterPanel::OnUpdate(float deltaTime)
    {
        IGN_PROFILE_FUNCTION();
        (void)deltaTime;

        // Poll importer
        if (m_IsImporting)
        {
            if (m_TotalImportItems > 0)
            {
                float progress = (float)m_ImportedItems / (float)m_TotalImportItems;
                m_EditorLayer->SetLoadingProgress(progress);
            }

            if (m_ActiveImportJobs <= 0)
            {
                m_EditorLayer->SaveProject();
                m_EditorLayer->RefreshContentBrowsers();

                if (auto project = m_EditorLayer->GetActiveProject())
                {
                    if (auto assetManager = AssetManager::GetInstance())
                    {
                        assetManager->ResumeUnloadAssets();
                    }
                }

                if (AdvanceToNextAsset())
                {
                    m_ShowImporterWindow = true;
                }
                else
                {
                    ResetImportState();
                    m_EditorLayer->SetStatusText("Ready");
                    m_EditorLayer->SetLoadingProgress(0.0f);
                }

                m_IsImporting = false;
            }
        }
    }

    void AssetImporterPanel::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();

        if (m_ShowImporterWindow)
        {
			if (ImGui::Begin("Asset Importer", &m_ShowImporterWindow, ImGuiWindowFlags_AlwaysAutoResize))
			{
				if (!m_CurrentAsset.has_value())
				{
					ImGui::TextDisabled("No pending assets.");
					if (ImGui::Button("Close"))
					{
						ResetImportState();
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
					return;
				}

				const auto &currentImportData = m_CurrentAsset.value();
				ImGui::Text("File: %s", currentImportData.metadata.filepath.generic_string().c_str());
				ImGui::Text("Type: %s", AssetTypeToString(currentImportData.metadata.type).c_str());
				ImGui::Separator();

				switch (currentImportData.metadata.type)
				{
				case AssetType::Mesh:
				case AssetType::StaticMesh:
				case AssetType::SkeletalMesh:
					DrawMeshImportOptions(m_CurrentAsset.value());
					break;
				case AssetType::Texture:
					DrawTextureImportOptions(m_CurrentAsset.value());
					break;
				case AssetType::Font:
					DrawFontImportOptions(m_CurrentAsset.value());
					break;
				default:
					DrawGenericImportOptions(m_CurrentAsset.value());
					break;
				}

				ImGui::Separator();
				ImGui::Checkbox("Skip dialog for same asset type as this file", &m_SkipDialogForSameType);
				ImGui::Separator();

				if (ImGui::Button("Import##import_button"))
				{
					ImportCurrentAsset();
					if (!m_CurrentAsset.has_value())
					{
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Skip"))
				{
					SkipCurrentAsset();
					if (!m_CurrentAsset.has_value())
					{
						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					ResetImportState();
					ImGui::CloseCurrentPopup();
				}

				ImGui::End();
			}
        }
    }

    void AssetImporterPanel::DrawMeshImportOptions(const FileImportPayload &payload)
    {
        IGN_PROFILE_FUNCTION();
        if (ImGui::BeginTabBar("##mesh_importer_tab_bar"))
        {
			if (ImGui::BeginTabItem("Static Mesh"))
			{
				m_MeshImportType = AssetType::StaticMesh;

				ImGui::Checkbox("Import Materials & Textures", &m_StaticMeshImportPayload.importMaterials);
				ImGui::Checkbox("Force Rebuild ?", &m_StaticMeshImportPayload.forceRebuild);
				ImGui::EndTabItem();
			}
            
			if (ImGui::BeginTabItem("Skeletal Mesh"))
			{
				m_MeshImportType = AssetType::SkeletalMesh;

				ImGui::Checkbox("Import Mesh", &m_SkeletalMeshImportPayload.importMesh);
				ImGui::Checkbox("Import Materials & Textures", &m_SkeletalMeshImportPayload.importMaterials);
				ImGui::Checkbox("Import Skeleton", &m_SkeletalMeshImportPayload.importSkeleton);
				ImGui::Checkbox("Import Animations", &m_SkeletalMeshImportPayload.importAnimations);
				ImGui::Checkbox("Force Rebuild ?", &m_SkeletalMeshImportPayload.forceRebuild);

				if (m_SkeletalMeshImportPayload.importAnimations)
				{
					if (ImGui::TreeNodeEx("Animation Options", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Checkbox("Use Existing Skeleton", &m_SkeletalMeshImportPayload.useExistingSkeletonForAnimations);

						if (m_SkeletalMeshImportPayload.useExistingSkeletonForAnimations)
						{
							const bool hasSkeleton = m_SkeletalMeshImportPayload.existingSkeletonHandle != AssetHandle(0);
							ImGui::Button(hasSkeleton ? "Skeleton Selected" : "Drop Skeleton Here", ImVec2(220.0f, 0.0f));

							if (ImGui::BeginDragDropTarget())
							{
								if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
								{
									if (payload->Data && payload->DataSize == sizeof(AssetHandle))
									{
										const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
										auto project = m_EditorLayer->GetActiveProject();
										if (project)
										{
											const AssetMetaData &metadata = AssetManager::GetInstance()->GetMetaData(droppedHandle);
											if (metadata.type == AssetType::Skeleton)
											{
												m_SkeletalMeshImportPayload.existingSkeletonHandle = droppedHandle;
											}
										}

									}
								}
								ImGui::EndDragDropTarget();
							}

							ImGui::SameLine();
							if (ImGui::Button("Clear Skeleton"))
							{
								m_SkeletalMeshImportPayload.existingSkeletonHandle = AssetHandle(0);
							}

							ImGui::TextDisabled("Handle: %llu", static_cast<uint64_t>(m_SkeletalMeshImportPayload.existingSkeletonHandle));
						}
						ImGui::TreePop();
					}
				}
				ImGui::EndTabItem();
			}
            ImGui::EndTabBar();
        }

        
    }

    void AssetImporterPanel::DrawTextureImportOptions(const FileImportPayload &payload)
    {
        if (ImGui::TreeNodeEx("Texture Import", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("No texture import options yet.");
            ImGui::TreePop();
        }
    }

    void AssetImporterPanel::DrawFontImportOptions(const FileImportPayload &payload)
    {
        if (ImGui::TreeNodeEx("Font Import", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("No font import options yet.");
            ImGui::TreePop();
        }
    }

    void AssetImporterPanel::DrawGenericImportOptions(const FileImportPayload &payload)
    {
        if (ImGui::TreeNodeEx("Import##import_tree_node", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("No additional import options for this asset type.");
            ImGui::TreePop();
        }
    }

    bool AssetImporterPanel::ProcessImportRequest(const FileImportPayload &payload)
    {
        IGN_PROFILE_FUNCTION();
        auto project = m_EditorLayer->GetActiveProject();
        if (!project)
            return false;

        auto assetManager = AssetManager::GetInstance();
        if (!assetManager)
            return false;

        m_ActiveImportJobs++;

        FileImportPayload jobData = payload;
        auto jobAssetManager = assetManager;

        AssetWorker::SubmitJob([this, project, jobAssetManager, jobData]() mutable
            {
                IGN_PROFILE_SCOPE("AssetImporterPanel::ProcessImportRequest::SubmitJob");

				auto &metadata = jobData.metadata;
                m_EditorLayer->SetStatusText(std::format("Importing {}...", metadata.filepath.filename().string()));

                switch (metadata.type)
                {
					case AssetType::Mesh:
					{
						if (m_MeshImportType == AssetType::StaticMesh)
						{
							ImportStaticMesh(jobData.metadata.filepath);
						}
						else if (m_MeshImportType == AssetType::SkeletalMesh)
						{
							ImportSkeletalMesh(jobData.metadata.filepath);
						}
						else
						{
							LOG_ERROR("[Asset Importer] Invalid mesh import type for file: {}", jobData.metadata.filepath.generic_string());
						}
						break;
					}
				    case AssetType::StaticMesh:
					    ImportStaticMesh(jobData.metadata.filepath);
					    break;
				    case AssetType::SkeletalMesh:
					    ImportSkeletalMesh(jobData.metadata.filepath);
					    break;
                    default:
                    {
					    const ignite::Path importedPath = PrepareAssetForImport(jobData);

					    if (importedPath.empty())
					    {
						    m_ActiveImportJobs--;
						    return;
					    }

					    const ignite::Path relativePath = project->GetProjectFilepath(importedPath);
					    AssetHandle finalHandle = jobAssetManager->GetAssetHandle(relativePath);
					    if (finalHandle == AssetHandle(0))
					    {
						    finalHandle = AssetHandle();
						    metadata.filepath = relativePath;
						    jobAssetManager->AssignMetaData(finalHandle, metadata);
						    LOG_TRACE("[Asset Importer] Registered asset (not loaded): {} ({})", static_cast<uint64_t>(finalHandle), relativePath.generic_string());
					    }
                        break;
                    }
                }

                m_ActiveImportJobs--;
                m_ImportedItems++;
            });

        return true;
    }

    bool AssetImporterPanel::AdvanceToNextAsset()
    {
        while (m_CurrentTypeQueueIndex < m_ImportTypeOrder.size())
        {
            const AssetType type = m_ImportTypeOrder[m_CurrentTypeQueueIndex];
            auto queueIt = m_ImportQueues.find(type);
            if (queueIt == m_ImportQueues.end() || queueIt->second.empty())
            {
                ++m_CurrentTypeQueueIndex;
                continue;
            }

            m_CurrentAsset = queueIt->second.front();
            queueIt->second.pop();
            m_SkipDialogForSameType = false;
            return true;
        }

        m_CurrentAsset.reset();
        return false;
    }

    void AssetImporterPanel::ResetImportState()
    {
        m_ImportQueues.clear();
        m_ImportTypeOrder.clear();
        m_CurrentTypeQueueIndex = 0;
        m_CurrentAsset.reset();
        m_SkipDialogForSameType = false;
        m_ShowImporterWindow = false;
        m_TotalImportItems = 0;
        m_ImportedItems = 0;
    }

    void AssetImporterPanel::ImportCurrentAsset()
    {
        if (!m_CurrentAsset.has_value())
            return;

        auto project = m_EditorLayer->GetActiveProject();
        auto assetManager = AssetManager::GetInstance();

        if (assetManager)
        {
            assetManager->PauseUnloadAssets();
        }

        m_IsImporting = true;
        m_EditorLayer->SetStatusText("Starting import...");

        AssetWorker::SubmitJob([this]()
        {
			// Process the current asset import request
            const AssetType currentType = m_CurrentAsset->metadata.type;
			ProcessImportRequest(*m_CurrentAsset);

			// Process any remaining assets of the same type if the user chose to skip the dialog for same type
            if (m_SkipDialogForSameType)
            {
                auto queueIt = m_ImportQueues.find(currentType);
                if (queueIt != m_ImportQueues.end())
                {
                    while (!queueIt->second.empty())
                    {
                        FileImportPayload payload = queueIt->second.front();
                        queueIt->second.pop();
                        ProcessImportRequest(payload);
                    }
                }
            }

            m_ShowImporterWindow = false;
        });
    }

    void AssetImporterPanel::SkipCurrentAsset()
    {
        m_ImportedItems++;
        AdvanceToNextAsset();
        if (!m_CurrentAsset.has_value())
        {
            ResetImportState();
        }
    }

    ignite::Path AssetImporterPanel::PrepareAssetForImport(const FileImportPayload &payload) const
    {
        auto project = m_EditorLayer->GetActiveProject();
        if (!project)
            return {};

        std::error_code ec;
        const ignite::Path sourcePath = std::filesystem::weakly_canonical(payload.metadata.filepath.string(), ec).string();
        if (ec || !ignite::Path::exists(sourcePath))
        {
            LOG_ERROR("[Asset Importer] File does not exist '{}'", payload.metadata.filepath.generic_string());
            return {};
        }

        const ignite::Path assetDirectory = std::filesystem::weakly_canonical(project->GetAssetDirectory().string(), ec).string();
        if (ec)
            return {};

        if (IsPathWithin(sourcePath.string(), assetDirectory.string()))
            return sourcePath;

        ignite::Path targetDirectory = m_TargetDirectory;
        if (targetDirectory.empty())
            targetDirectory = project->GetAssetDirectory();

        std::filesystem::create_directories(targetDirectory.string(), ec);
        if (ec)
        {
            LOG_ERROR("[Asset Importer] Failed to create target directory '{}'", targetDirectory.generic_string());
            return {};
        }

        ignite::Path destinationPath = targetDirectory / sourcePath.filename();
        if (ignite::Path::exists(destinationPath))
        {
            destinationPath = BuildUniquePath(targetDirectory, sourcePath.stem().string(), sourcePath.extension().string());
        }

        std::filesystem::copy_file(sourcePath.string(), destinationPath.string(), std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            LOG_ERROR("[Asset Importer] Failed to copy '{}' to '{}': {}", sourcePath.generic_string(), destinationPath.generic_string(), ec.message());
            return {};
        }

        return destinationPath;
    }

	bool AssetImporterPanel::ImportStaticMesh(const ignite::Path &filepath)
	{
		IGN_PROFILE_FUNCTION();
		auto project = m_EditorLayer->GetActiveProject();
		if (!project)
		{
			return false;
		}

		auto assetManager = AssetManager::GetInstance();
		const ignite::Path meshRelativePath = project->GetProjectFilepath(filepath);

		AssetHandle handle = assetManager->GetAssetHandle(meshRelativePath);
		if (handle == AssetHandle(0))
		{
			handle = AssetHandle();
		}

		AssetMetaData sourceMetadata;
		sourceMetadata.filepath = filepath;
		sourceMetadata.type = AssetType::StaticMesh;

        m_StaticMeshImportPayload.targetDirectory = m_TargetDirectory;
		Ref<StaticMesh> importedAsset = AssetImporter::ImportStaticMesh(handle, sourceMetadata, assetManager, m_StaticMeshImportPayload);
		if (!importedAsset)
		{
			LOG_ERROR("[Asset Importer] Failed to import mesh from {}", filepath.generic_string());
			return false;
		}
        return true;
	}

	bool AssetImporterPanel::ImportSkeletalMesh(const ignite::Path &filepath)
	{
		IGN_PROFILE_FUNCTION();
		auto project = m_EditorLayer->GetActiveProject();
		if (!project)
		{
			return false;
		}

		auto assetManager = AssetManager::GetInstance();
		const ignite::Path skmRelativePath = project->GetProjectFilepath(filepath);

		AssetHandle handle = assetManager->GetAssetHandle(skmRelativePath);
		if (handle == AssetHandle(0))
		{
			handle = AssetHandle();
		}

		AssetMetaData sourceMetadata;
		sourceMetadata.filepath = filepath;
		sourceMetadata.type = AssetType::SkeletalMesh;

        m_SkeletalMeshImportPayload.targetDirectory = m_TargetDirectory;
		Ref<SkeletalMesh> importedAsset = AssetImporter::ImportSkeletalMesh(handle, sourceMetadata, assetManager, m_SkeletalMeshImportPayload);
		if (!importedAsset && m_SkeletalMeshImportPayload.importMesh)
		{
			LOG_ERROR("[Asset Importer] Failed to import mesh from {}", filepath.generic_string());
			return false;
		}
        return true;
	}

	ignite::Path AssetImporterPanel::BuildUniquePath(const ignite::Path &directory, const std::string &baseName, const std::string &extension) const
    {
        IGN_PROFILE_FUNCTION();
        ignite::Path candidate = directory / (baseName + extension);
        if (!ignite::Path::exists(candidate))
        {
            return candidate;
        }

        uint32_t suffix = 1;
        while (true)
        {
            candidate = directory / std::format("{}_{}{}", baseName, suffix, extension);
            if (!ignite::Path::exists(candidate))
            {
                return candidate;
            }
            ++suffix;
        }
    }
}
