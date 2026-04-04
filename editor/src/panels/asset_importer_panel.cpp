// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_importer_panel.hpp"

#include "ignite/project/project.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/serializer/serializer.hpp"

#include "editor_layer.hpp"

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
		IGN_PROFILE_FUNCTION();
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<AssetImportEvent>(BIND_CLASS_EVENT_FN(AssetImporterPanel::OnAssetImportEvent));
	}

	bool AssetImporterPanel::OnAssetImportEvent(AssetImportEvent &event)
	{
		IGN_PROFILE_FUNCTION();
		m_SelectedFilepaths = event.GetFilepaths();
		m_TargetDirectory = event.GetTargetDirectory();
		if (m_TargetDirectory.empty())
		{
            m_TargetDirectory = m_EditorLayer->GetActiveProject() ? m_EditorLayer->GetActiveProject()->GetAssetDirectory() : std::filesystem::path();
		}
		m_SelectedAssetType = event.GetAssetType();
		m_SkeletalMeshOptions = {};
		m_FontPreview = {};

		if (m_SelectedAssetType == AssetType::Font && !m_SelectedFilepaths.empty())
		{
			m_FontPreview.sourceFilepath = m_SelectedFilepaths.front();
			m_FontPreview.font = Font::Create(m_FontPreview.sourceFilepath);
		}
		m_ShowImporterWindow = !m_SelectedFilepaths.empty();

		return m_ShowImporterWindow;
	}

	void AssetImporterPanel::OnUpdate(float deltaTime)
	{
		IGN_PROFILE_FUNCTION();
		while (!m_ImportRequests.empty())
		{
			IGN_PROFILE_SCOPE("AssetImporterPanel::OnUpdate::ProcessRequest");
			ImportRequest request = m_ImportRequests.front();
			m_ImportRequests.pop();
			ProcessImportRequest(request);
		}
	}

	void AssetImporterPanel::OnGuiRender()
	{
		IGN_PROFILE_FUNCTION();
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
			if (m_SelectedAssetType == AssetType::SkeletalMesh)
			{
				DrawSkeletalMeshImportOptions();
			}
			else if (m_SelectedAssetType == AssetType::StaticMesh)
			{
				DrawStaticMeshImportOptions();
			}
		}

		if (m_SelectedAssetType == AssetType::Font)
		{
			DrawFontImportPreview();
		}

		ImGui::Separator();

		bool canImport = true;
		if (m_SelectedAssetType == AssetType::Font)
		{
			Ref<Texture> atlasTexture = m_FontPreview.font ? m_FontPreview.font->GetAtlasTexture() : nullptr;
			canImport = atlasTexture && atlasTexture->IsReady();
		}

        if (!canImport)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Import") && canImport)
		{
			QueueImportRequest();
		}

		if (!canImport)
		{
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::TextDisabled("Waiting for atlas upload...");
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			m_FontPreview = {};
			m_ShowImporterWindow = false;
		}

		ImGui::End();
	}

	void AssetImporterPanel::QueueImportRequest()
	{
        IGN_PROFILE_FUNCTION();
		if (m_SelectedFilepaths.empty())
		{
			m_ShowImporterWindow = false;
			return;
		}

		if (!m_EditorLayer->GetActiveProject())
		{
			LOG_ERROR("[Asset Importer] No active project to import assets");
			m_ShowImporterWindow = false;
			return;
		}

       const bool hasFbx = std::ranges::any_of(m_SelectedFilepaths, [](const std::filesystem::path &filepath)
			{
				return IsFbxFile(filepath);
			});

		if (hasFbx
			&& m_SelectedAssetType != AssetType::StaticMesh
			&& !m_SkeletalMeshOptions.importSkeletalMesh
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
		m_FontPreview = {};
		m_ShowImporterWindow = false;
	}

	void AssetImporterPanel::DrawFontImportPreview()
	{
		IGN_PROFILE_FUNCTION();
		ImGui::SeparatorText("Font MSDF Preview");
		if (!m_FontPreview.font)
		{
			ImGui::TextDisabled("Failed to generate font preview.");
			return;
		}

		Ref<Texture> atlasTexture = m_FontPreview.font->GetAtlasTexture();
        if (!atlasTexture || !atlasTexture->IsReady())
		{
			ImGui::TextDisabled("Generating atlas...");
			return;
		}

		ImGui::Text("Source: %s", m_FontPreview.sourceFilepath.filename().generic_string().c_str());
		ImGui::Text("Atlas: %d x %d", atlasTexture->GetWidth(), atlasTexture->GetHeight());

		const float maxWidth = std::min(420.0f, ImGui::GetContentRegionAvail().x);
		const float aspect = atlasTexture->GetHeight() > 0 ? static_cast<float>(atlasTexture->GetWidth()) / static_cast<float>(atlasTexture->GetHeight()) : 1.0f;
		ImVec2 previewSize(maxWidth, maxWidth / std::max(aspect, 0.001f));
		ImGui::Image(reinterpret_cast<ImTextureID>(atlasTexture->GetHandle().Get()), previewSize);
	}

	void AssetImporterPanel::DrawSkeletalMeshImportOptions()
	{
        IGN_PROFILE_FUNCTION();
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

	void AssetImporterPanel::DrawStaticMeshImportOptions()
	{
		IGN_PROFILE_FUNCTION();

		if (ImGui::TreeNodeEx("Static Mesh (FBX) Import", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const uint32_t fbxCount = static_cast<uint32_t>(std::count_if(m_SelectedFilepaths.begin(), m_SelectedFilepaths.end(), [](const std::filesystem::path &filepath)
			{
				return IsFbxFile(filepath);
			}));

			ImGui::Text("FBX files selected: %u", fbxCount);
			ImGui::TextWrapped("This import path creates a Static Mesh asset only. Skeleton and animation tracks are ignored.");

			ImGui::Spacing();
			ImGui::SeparatorText("Pipeline");
			ImGui::BulletText("Triangulate geometry");
			ImGui::BulletText("Extract materials and texture maps");
			ImGui::BulletText("Create static mesh binary (.ixsm)");
			ImGui::BulletText("Queue GPU buffer upload for mesh primitives");

			auto project = m_EditorLayer->GetActiveProject();
			if (project)
			{
				ImGui::Spacing();
				ImGui::SeparatorText("Output Preview");

				uint32_t previewCount = 0;
				for (const auto &filepath : m_SelectedFilepaths)
				{
					if (!IsFbxFile(filepath))
					{
						continue;
					}

					const std::filesystem::path filename = filepath.stem();
					const std::filesystem::path rootOutput = project->GetAssetDirectory() / filename;
					const std::filesystem::path meshOutput = rootOutput / "StaticMesh" / (filename.string() + GetAssetExtensionFromType(AssetType::StaticMesh));
					const std::filesystem::path materialOutput = rootOutput / "Material";
					const std::filesystem::path textureOutput = rootOutput / "Textures";

					ImGui::PushID(filepath.generic_string().c_str());
					if (ImGui::TreeNodeEx(filepath.filename().generic_string().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Text("Source: %s", filepath.generic_string().c_str());
						ImGui::Text("Mesh: %s", meshOutput.generic_string().c_str());
						ImGui::Text("Materials: %s", materialOutput.generic_string().c_str());
						ImGui::Text("Textures: %s", textureOutput.generic_string().c_str());

						if (std::filesystem::exists(meshOutput))
						{
							ImGui::TextColored(ImVec4(0.80f, 0.95f, 0.80f, 1.0f), "Existing cached static mesh binary found.");
						}
						else
						{
							ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.65f, 1.0f), "No cached binary found. A new one will be generated.");
						}

						ImGui::TreePop();
					}
					ImGui::PopID();

					++previewCount;
					if (previewCount >= 3 && fbxCount > previewCount)
					{
						ImGui::TextDisabled("...and %u more file(s)", fbxCount - previewCount);
						break;
					}
				}

				ImGui::Spacing();
				ImGui::SeparatorText("Notes");
				ImGui::TextWrapped("Material and texture assets are generated from FBX material slots. Reimporting can overwrite generated source files in target folders.");
			}
			else
			{
				ImGui::TextDisabled("No active project. Output preview is unavailable.");
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Compatibility", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BulletText("Best with FBX meshes using standard PBR-like material naming");
			ImGui::BulletText("Non-triangulated meshes are automatically triangulated");
			ImGui::BulletText("Skeletal data in FBX is ignored for static mesh import");
			ImGui::TreePop();
		}
	}

    void AssetImporterPanel::ProcessImportRequest(const ImportRequest &request)
	{
		IGN_PROFILE_FUNCTION();
		auto project = m_EditorLayer->GetActiveProject();
		if (!project)
		{
			return;
		}

		auto assetManager = project->GetAssetManager();
		bool importedAny = false;

		for (const auto &filepath : request.filepaths)
		{
			IGN_PROFILE_SCOPE("AssetImporterPanel::ProcessImportRequest::File");
			if (request.assetType == AssetType::StaticMesh && IsFbxFile(filepath))
			{
				IGN_PROFILE_SCOPE("AssetImporterPanel::ProcessImportRequest::FBXStaticMesh");
				ImportFbxAsStaticMesh(filepath);
				importedAny = true;
				continue;
			}

			if (request.assetType == AssetType::Font)
			{
				IGN_PROFILE_SCOPE("AssetImporterPanel::ProcessImportRequest::Font");
				ImportFontAsset(filepath);
				importedAny = true;
				continue;
			}

			if (IsFbxFile(filepath))
			{
				IGN_PROFILE_SCOPE("AssetImporterPanel::ProcessImportRequest::FBX");
				if (request.skeletalMeshOptions.importSkeletalMesh)
				{
					ImportFbxAsSkeletalMesh(filepath);
				}
				else
				{
					ImportFbxSkeletonAndAnimations(filepath, request.skeletalMeshOptions);
				}
				importedAny = true;
				continue;
			}

			assetManager->ImportAsset(filepath);
			importedAny = true;
		}

		if (importedAny)
		{
			project->Serialize(project->GetFilepath());
		}
	}

	void AssetImporterPanel::ImportFontAsset(const std::filesystem::path &filepath)
	{
		IGN_PROFILE_FUNCTION();
		auto project = m_EditorLayer->GetActiveProject();
		if (!project || !std::filesystem::exists(filepath))
		{
			return;
		}

		auto assetManager = project->GetAssetManager();
		const std::filesystem::path assetDirectory = m_TargetDirectory.empty() ? project->GetAssetDirectory() : m_TargetDirectory;
		if (!std::filesystem::exists(assetDirectory))
		{
			std::filesystem::create_directories(assetDirectory);
		}
		const std::filesystem::path sourceExtension = filepath.extension();
		const std::filesystem::path targetFontPath = BuildUniquePath(assetDirectory, filepath.stem().string(), sourceExtension.string());

		std::error_code ec;
		if (std::filesystem::absolute(filepath) != std::filesystem::absolute(targetFontPath))
		{
			std::filesystem::copy_file(filepath, targetFontPath, std::filesystem::copy_options::overwrite_existing, ec);
			if (ec)
			{
				LOG_ERROR("[Asset Importer] Failed to copy font '{}' -> '{}': {}", filepath.generic_string(), targetFontPath.generic_string(), ec.message());
				return;
			}
		}

		AssetMetaData importMetadata;
		importMetadata.filepath = targetFontPath;
		importMetadata.type = AssetType::Font;

		const std::filesystem::path relativeFontPath = project->GetAssetRelativeFilepath(targetFontPath);
		AssetHandle fontHandle = assetManager->GetAssetHandle(relativeFontPath);
		if (fontHandle == AssetHandle(0))
		{
			fontHandle = AssetHandle();
		}

		Ref<Font> importedFont = AssetImporter::ImportFont(fontHandle, importMetadata, assetManager);
		if (!importedFont)
		{
			LOG_ERROR("[Asset Importer] Failed to import font {}", filepath.generic_string());
			return;
		}

		AssetMetaData fontRegistryMetadata;
		fontRegistryMetadata.filepath = relativeFontPath;
		fontRegistryMetadata.type = AssetType::Font;
		assetManager->AssignMetaData(fontHandle, fontRegistryMetadata);
		assetManager->AssignAsset(fontHandle, importedFont);

        if (Ref<Texture> atlasTexture = importedFont->GetAtlasTexture())
		{
            if (!atlasTexture->IsReady())
			{
				LOG_WARN("[Asset Importer] Font atlas is not ready yet for {}", filepath.generic_string());
				return;
			}

			const std::filesystem::path atlasPath = BuildUniquePath(assetDirectory, filepath.stem().string() + "_msdf", ".png");
			if (BinarySerializer::SerializeTextureToPNG(atlasTexture, atlasPath))
			{
				const std::filesystem::path relativeAtlasPath = project->GetAssetRelativeFilepath(atlasPath);
				AssetHandle atlasHandle = assetManager->GetAssetHandle(relativeAtlasPath);
				if (atlasHandle == AssetHandle(0))
				{
					atlasHandle = AssetHandle();
				}

				atlasTexture->handle = atlasHandle;
				atlasTexture->SetDirtyFlag(false);
				atlasTexture->SetReadyFlag(true);

				AssetMetaData atlasMetadata;
				atlasMetadata.filepath = relativeAtlasPath;
				atlasMetadata.type = AssetType::Texture;
				assetManager->AssignMetaData(atlasHandle, atlasMetadata);
				assetManager->AssignAsset(atlasHandle, atlasTexture);
			}
		}
	}

	std::filesystem::path AssetImporterPanel::BuildUniquePath(const std::filesystem::path &directory, const std::string &baseName, const std::string &extension) const
	{
		IGN_PROFILE_FUNCTION();
		std::filesystem::path candidate = directory / (baseName + extension);
		if (!std::filesystem::exists(candidate))
		{
			return candidate;
		}

		uint32_t suffix = 1;
		while (true)
		{
			candidate = directory / std::format("{}_{}{}", baseName, suffix, extension);
			if (!std::filesystem::exists(candidate))
			{
				return candidate;
			}
			++suffix;
		}
	}

	void AssetImporterPanel::ImportFbxAsStaticMesh(const std::filesystem::path &filepath)
	{
		IGN_PROFILE_FUNCTION();
		auto project = m_EditorLayer->GetActiveProject();
		if (!project)
		{
			return;
		}

		auto assetManager = project->GetAssetManager();
		const std::filesystem::path filename = filepath.stem();
		const std::filesystem::path smBinaryPath = project->GetAssetDirectory() / filename / "StaticMesh" / (filename.string() + GetAssetExtensionFromType(AssetType::StaticMesh));
		const std::filesystem::path smRelativePath = project->GetAssetRelativeFilepath(smBinaryPath);

		AssetHandle handle = assetManager->GetAssetHandle(smRelativePath);
		if (handle == AssetHandle(0))
		{
			handle = AssetHandle();
		}

		AssetMetaData sourceMetadata;
		sourceMetadata.filepath = filepath;
		sourceMetadata.type = AssetType::StaticMesh;

		Ref<StaticMesh> importedAsset = AssetImporter::ImportStaticMesh(handle, sourceMetadata, assetManager);
		if (!importedAsset)
		{
			LOG_ERROR("[Asset Importer] Failed to import static mesh from {}", filepath.generic_string());
			return;
		}

		AssetMetaData registryMetadata;
		registryMetadata.filepath = smRelativePath;
		registryMetadata.type = AssetType::StaticMesh;

		assetManager->AssignMetaData(handle, registryMetadata);
		assetManager->AssignAsset(handle, importedAsset);
	}

	void AssetImporterPanel::ImportFbxAsSkeletalMesh(const std::filesystem::path &filepath)
	{
		IGN_PROFILE_FUNCTION();
		auto project = m_EditorLayer->GetActiveProject();
		if (!project)
		{
			return;
		}

		auto assetManager = project->GetAssetManager();
		const std::filesystem::path filename = filepath.stem();
		const std::filesystem::path skmBinaryPath = project->GetAssetDirectory() / filename / "SkeletalMesh" / (filename.string() + GetAssetExtensionFromType(AssetType::SkeletalMesh));
		const std::filesystem::path skmRelativePath = project->GetAssetRelativeFilepath(skmBinaryPath);

		AssetHandle handle = assetManager->GetAssetHandle(skmRelativePath);
		if (handle == AssetHandle(0))
		{
			handle = AssetHandle();
		}

		AssetMetaData sourceMetadata;
		sourceMetadata.filepath = filepath;
		sourceMetadata.type = AssetType::SkeletalMesh;

		Ref<SkeletalMesh> importedAsset = AssetImporter::ImportSkeletalMesh(handle, sourceMetadata, assetManager);
		if (!importedAsset)
		{
			LOG_ERROR("[Asset Importer] Failed to import skeletal mesh from {}", filepath.generic_string());
			return;
		}

		AssetMetaData registryMetadata;
		registryMetadata.filepath = skmRelativePath;
		registryMetadata.type = AssetType::SkeletalMesh;

		assetManager->AssignMetaData(handle, registryMetadata);
		assetManager->AssignAsset(handle, importedAsset);
	}

	void AssetImporterPanel::ImportFbxSkeletonAndAnimations(const std::filesystem::path &filepath, const SkeletalMeshImportOptions &options)
	{
		IGN_PROFILE_FUNCTION();
		if (!options.importSkeleton && !options.importAnimations)
		{
			return;
		}

		auto project = m_EditorLayer->GetActiveProject();
		if (!project)
		{
			return;
		}

		auto assetManager = project->GetAssetManager();
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
		FBXMeshLoader::LoadSkeletonOnlyFromFBX(filepath.generic_string(), skeleton, assetManager);

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
			skeleton->Serialize(skeletonPath);

			AssetMetaData skeletonMD;
			skeletonMD.filepath = project->GetAssetRelativeFilepath(skeletonPath);
			skeletonMD.type = AssetType::Skeleton;

			AssetHandle skeletonHandle = assetManager->GetAssetHandle(skeletonMD.filepath);
			if (skeletonHandle == AssetHandle(0))
			{
				skeletonHandle = AssetHandle();
			}

			skeleton->handle = skeletonHandle;
			skeleton->SetReadyFlag(true);
			assetManager->AssignMetaData(skeletonHandle, skeletonMD);
			assetManager->AssignAsset(skeletonHandle, skeleton);
		}

		if (options.importAnimations)
		{
			std::vector<Ref<SkeletalAnimation>> animations;
			FBXMeshLoader::LoadAnimationsOnlyFromFBX(filepath.generic_string(), skeleton, animations, assetManager);

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
				animation->Serialize(animationPath);

				AssetMetaData animationMD;
				animationMD.filepath = project->GetAssetRelativeFilepath(animationPath);
				animationMD.type = AssetType::SkeletalAnimation;

				AssetHandle animationHandle = assetManager->GetAssetHandle(animationMD.filepath);
				if (animationHandle == AssetHandle(0))
				{
					animationHandle = AssetHandle();
				}

				animation->handle = animationHandle;
				animation->SetReadyFlag(true);
				assetManager->AssignMetaData(animationHandle, animationMD);
				assetManager->AssignAsset(animationHandle, animation);
			}
		}
	}

}
