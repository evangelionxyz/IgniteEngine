// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_importer_panel.hpp"

#include "ignite/project/project.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/serializer/binary_serializer.hpp"

#include <algorithm>
#include <format>

namespace ignite
{
   namespace
	{
		bool IsFbxFile(const std::filesystem::path &filepath)
		{
			std::string extension = filepath.extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
			return extension == ".fbx";
		}
	}

	AssetImporterPanel::AssetImporterPanel(const char *name, EditorLayer *editor)
		: IPanel(name, editor)
	{
	}

	void AssetImporterPanel::OnEvent(Event &event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<AssetImportEvent>(BIND_CLASS_EVENT_FN(AssetImporterPanel::OnAssetImportEvent));
	}

	bool AssetImporterPanel::OnAssetImportEvent(AssetImportEvent &event)
	{
        m_SelectedFilepaths = event.GetFilepaths();
		m_SelectedAssetType = event.GetAssetType();
		m_SkeletalMeshOptions = {};
		m_ShowImporterWindow = !m_SelectedFilepaths.empty();

       return m_ShowImporterWindow;
	}

	void AssetImporterPanel::OnUpdate(float deltaTime)
	{
		while (!m_ImportRequests.empty())
		{
			ImportRequest request = m_ImportRequests.front();
			m_ImportRequests.pop();
			ProcessImportRequest(request);
		}
	}

	void AssetImporterPanel::OnGuiRender()
	{
       if (!m_ShowImporterWindow)
		{
			return;
		}

		if (!ImGui::Begin("Asset Importer", &m_ShowImporterWindow))
		{
			ImGui::End();
			return;
		}

		ImGui::Text("Files: %zu", m_SelectedFilepaths.size());
		ImGui::Text("Detected Type: %s", AssetTypeToString(m_SelectedAssetType).c_str());

		if (ImGui::TreeNodeEx("Selected Files", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const auto &filepath : m_SelectedFilepaths)
			{
				ImGui::BulletText("%s", filepath.generic_string().c_str());
			}
			ImGui::TreePop();
		}

		const bool hasFbx = std::ranges::any_of(m_SelectedFilepaths, [](const std::filesystem::path &filepath)
		{
			return IsFbxFile(filepath);
		});

		if (hasFbx)
		{
			DrawSkeletalMeshImportOptions();
		}

		ImGui::Separator();

		if (ImGui::Button("Import"))
		{
			QueueImportRequest();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			m_ShowImporterWindow = false;
		}

		ImGui::End();
	}

	void AssetImporterPanel::QueueImportRequest()
	{
		if (m_SelectedFilepaths.empty())
		{
			m_ShowImporterWindow = false;
			return;
		}

		if (!Project::GetInstance())
		{
			LOG_ERROR("[Asset Importer] No active project to import assets");
			m_ShowImporterWindow = false;
			return;
		}

		if (!m_SkeletalMeshOptions.importSkeletalMesh
			&& !m_SkeletalMeshOptions.importSkeleton
			&& !m_SkeletalMeshOptions.importAnimations)
		{
			LOG_WARN("[Asset Importer] Nothing selected for FBX import");
			return;
		}

		ImportRequest request;
		request.filepaths = m_SelectedFilepaths;
		request.assetType = m_SelectedAssetType;
		request.skeletalMeshOptions = m_SkeletalMeshOptions;
		m_ImportRequests.push(std::move(request));
		m_ShowImporterWindow = false;
	}

	void AssetImporterPanel::DrawSkeletalMeshImportOptions()
	{
		if (ImGui::TreeNodeEx("Skeletal Mesh (FBX) Import", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Import Skeletal Mesh", &m_SkeletalMeshOptions.importSkeletalMesh);

			if (m_SkeletalMeshOptions.importSkeletalMesh)
			{
				m_SkeletalMeshOptions.importSkeleton = true;
				m_SkeletalMeshOptions.importAnimations = true;
			}

			ImGui::BeginDisabled(m_SkeletalMeshOptions.importSkeletalMesh);
			ImGui::Checkbox("Import Skeleton", &m_SkeletalMeshOptions.importSkeleton);
			ImGui::Checkbox("Import Animations", &m_SkeletalMeshOptions.importAnimations);
			ImGui::EndDisabled();

			if (m_SkeletalMeshOptions.importSkeletalMesh)
			{
				ImGui::TextDisabled("Skeletal mesh import currently requires skeleton and animations.");
			}

			ImGui::TreePop();
		}
	}

	void AssetImporterPanel::ProcessImportRequest(const ImportRequest &request)
	{
		Project *project = Project::GetInstance();
		if (!project)
		{
			return;
		}

		auto &assetManager = project->GetAssetManager();

		for (const auto &filepath : request.filepaths)
		{
			if (IsFbxFile(filepath))
			{
				if (request.skeletalMeshOptions.importSkeletalMesh)
				{
					ImportFbxAsSkeletalMesh(filepath);
				}
				else
				{
					ImportFbxSkeletonAndAnimations(filepath, request.skeletalMeshOptions);
				}
				continue;
			}

			assetManager.ImportAsset(filepath);
		}
	}

	void AssetImporterPanel::ImportFbxAsSkeletalMesh(const std::filesystem::path &filepath)
	{
		Project *project = Project::GetInstance();
		if (!project)
		{
			return;
		}

		auto &assetManager = project->GetAssetManager();
		const std::filesystem::path filename = filepath.stem();
		const std::filesystem::path skmBinaryPath = project->GetAssetDirectory() / filename / "SkeletalMesh" / (filename.string() + GetAssetExtensionFromType(AssetType::SkeletalMesh));
		const std::filesystem::path skmRelativePath = project->GetAssetRelativeFilepath(skmBinaryPath);

		AssetHandle handle = assetManager.GetAssetHandle(skmRelativePath);
		if (handle == AssetHandle(0))
		{
			handle = AssetHandle();
		}

		AssetMetaData sourceMetadata;
		sourceMetadata.filepath = filepath;
		sourceMetadata.type = AssetType::SkeletalMesh;

		Ref<SkeletalMesh> importedAsset = AssetImporter::ImportSkeletalMesh(handle, sourceMetadata);
		if (!importedAsset)
		{
			LOG_ERROR("[Asset Importer] Failed to import skeletal mesh from {}", filepath.generic_string());
			return;
		}

		AssetMetaData registryMetadata;
		registryMetadata.filepath = skmRelativePath;
		registryMetadata.type = AssetType::SkeletalMesh;

		assetManager.AssignMetaData(handle, registryMetadata);
		assetManager.AssignAsset(handle, importedAsset);
	}

	void AssetImporterPanel::ImportFbxSkeletonAndAnimations(const std::filesystem::path &filepath, const SkeletalMeshImportOptions &options)
	{
		if (!options.importSkeleton && !options.importAnimations)
		{
			return;
		}

		Project *project = Project::GetInstance();
		if (!project)
		{
			return;
		}

		auto &assetManager = project->GetAssetManager();
		const std::filesystem::path assetDirectory = project->GetAssetDirectory();
		const std::filesystem::path filename = filepath.stem();

		const std::filesystem::path outputDirectory = assetDirectory / filename;
		const std::filesystem::path skeletalMeshDirectory = outputDirectory / "SkeletalMesh";
		const std::filesystem::path animationDirectory = outputDirectory / "Animation";

		if (!std::filesystem::exists(outputDirectory))
		{
			std::filesystem::create_directory(outputDirectory);
		}
		if (!std::filesystem::exists(skeletalMeshDirectory))
		{
			std::filesystem::create_directory(skeletalMeshDirectory);
		}
		if (!std::filesystem::exists(animationDirectory))
		{
			std::filesystem::create_directory(animationDirectory);
		}

		Ref<Skeleton> skeleton = CreateRef<Skeleton>();
		FBXMeshLoader::LoadSkeletonOnlyFromFBX(filepath.generic_string(), skeleton);

		if (!skeleton)
		{
			LOG_WARN("[Asset Importer] FBX has no valid skeleton: {}", filepath.generic_string());
			return;
		}

		const std::string skeletonExt = GetAssetExtensionFromType(AssetType::Skeleton);
		const std::string animationExt = GetAssetExtensionFromType(AssetType::SkeletalAnimation);

		if (options.importSkeleton)
		{
			const std::filesystem::path skeletonPath = skeletalMeshDirectory / (filename.string() + skeletonExt);
			BinarySerializer::SerializeSkeleton(skeleton, skeletonPath);

			AssetMetaData skeletonMD;
			skeletonMD.filepath = project->GetAssetRelativeFilepath(skeletonPath);
			skeletonMD.type = AssetType::Skeleton;

			AssetHandle skeletonHandle = assetManager.GetAssetHandle(skeletonMD.filepath);
			if (skeletonHandle == AssetHandle(0))
			{
				skeletonHandle = AssetHandle();
			}

			skeleton->handle = skeletonHandle;
			skeleton->SetReadyFlag(true);
			assetManager.AssignMetaData(skeletonHandle, skeletonMD);
			assetManager.AssignAsset(skeletonHandle, skeleton);
		}

		if (options.importAnimations)
		{
			std::vector<Ref<SkeletalAnimation>> animations;
			FBXMeshLoader::LoadAnimationsOnlyFromFBX(filepath.generic_string(), skeleton, animations);

			if (animations.empty())
			{
				LOG_WARN("[Asset Importer] FBX has no animation clips: {}", filepath.generic_string());
				return;
			}

			for (size_t i = 0; i < animations.size(); ++i)
			{
				Ref<SkeletalAnimation> animation = animations[i];
				if (!animation)
				{
					continue;
				}

				const std::filesystem::path animationPath = animationDirectory / (std::format("{}_{}", filename.string(), i) + animationExt);
				BinarySerializer::SerializeAnimation(animation, animationPath);

				AssetMetaData animationMD;
				animationMD.filepath = project->GetAssetRelativeFilepath(animationPath);
				animationMD.type = AssetType::SkeletalAnimation;

				AssetHandle animationHandle = assetManager.GetAssetHandle(animationMD.filepath);
				if (animationHandle == AssetHandle(0))
				{
					animationHandle = AssetHandle();
				}

				animation->handle = animationHandle;
				animation->SetReadyFlag(true);
				assetManager.AssignMetaData(animationHandle, animationMD);
				assetManager.AssignAsset(animationHandle, animation);
			}
		}
	}

}
