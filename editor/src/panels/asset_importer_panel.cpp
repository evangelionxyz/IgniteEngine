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
#include <cctype>
#include <condition_variable>
#include <format>
#include <mutex>

namespace ignite
{
    namespace
    {
        template<typename T>
        void PublishImportedAsset(AssetManager *assetManager, const AssetMetaData &metadata, const Ref<T> &asset)
        {
            if (!assetManager || !asset)
            {
                return;
            }

            Application::SubmitToMainThread([assetManager, metadata, asset]()
            {
                AssetHandle handle = assetManager->GetAssetHandle(metadata.filepath);
                if (handle == AssetHandle(0))
                {
                    handle = AssetHandle();
                }

                asset->handle = handle;
                asset->SetReadyFlag(true);
                assetManager->AssignMetaData(handle, metadata);
                assetManager->AssignAsset(handle, asset);
            }, "AssetImporterPanel::PublishImportedAsset");
        }

        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        bool IsFbxFile(const std::filesystem::path &filepath)
        {
            return ToLower(filepath.extension().string()) == ".fbx";
        }

        bool IsMeshImportDialogFile(const std::filesystem::path &filepath)
        {
            return GetAssetTypeFromExtension(ToLower(filepath.extension().string())) == AssetType::Mesh;
        }

        bool IsTextureImportDialogFile(const std::filesystem::path &filepath)
        {
            return GetAssetTypeFromExtension(ToLower(filepath.extension().string())) == AssetType::Texture;
        }

        bool IsFontImportDialogFile(const std::filesystem::path &filepath)
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
        ResetImportState();

        m_TargetDirectory = event.GetTargetDirectory();
        if (m_TargetDirectory.empty())
        {
            m_TargetDirectory = m_EditorLayer->GetActiveProject() ? m_EditorLayer->GetActiveProject()->GetAssetDirectory() : std::filesystem::path();
        }

        m_MeshOptions = {};
        m_MeshOptions.targetDirectory = m_TargetDirectory;
        for (auto &p : event.GetFilepaths())
        {
            const AssetType assetType = GetAssetTypeFromExtension(ToLower(p.extension().string()));
            if (assetType == AssetType::Invalid)
            {
                LOG_WARN("[Asset Importer] Skipped unsupported file '{}'", p.generic_string());
                continue;
            }

            if (!m_ImportQueues.contains(assetType))
            {
                m_ImportTypeOrder.push_back(assetType);
            }

            AssetImportData importData;
            importData.filepath = p;
            importData.assetType = assetType;
            importData.meshOptions = m_MeshOptions;
            m_ImportQueues[assetType].push(importData);
        }

        if (!AdvanceToNextAsset())
        {
            return false;
        }

        m_ShowImporterWindow = true;
        m_OpenImporterPopup = true;
        return true;
    }

    void AssetImporterPanel::OnUpdate(float deltaTime)
    {
        IGN_PROFILE_FUNCTION();
        (void)deltaTime;
    }

    void AssetImporterPanel::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();
        if (!m_ShowImporterWindow)
        {
            return;
        }

        if (m_OpenImporterPopup)
        {
            ImGui::OpenPopup("Asset Importer");
            m_OpenImporterPopup = false;
        }

        if (ImGui::BeginPopup("Asset Importer", ImGuiWindowFlags_AlwaysAutoResize))
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

            const AssetImportData currentImportData = *m_CurrentAsset;
            ImGui::Text("File: %s", currentImportData.filepath.generic_string().c_str());
            ImGui::Text("Type: %s", AssetTypeToString(currentImportData.assetType).c_str());
            ImGui::Separator();

            if (IsMeshImportDialogFile(currentImportData.filepath))
            {
                DrawMeshImportOptions();
            }
            else if (IsTextureImportDialogFile(currentImportData.filepath))
            {
                DrawTextureImportOptions();
            }
            else if (IsFontImportDialogFile(currentImportData.filepath))
            {
                DrawFontImportOptions();
            }
            else
            {
                DrawGenericImportOptions();
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

            ImGui::EndPopup();
        }
    }

    void AssetImporterPanel::DrawMeshImportOptions()
    {
        IGN_PROFILE_FUNCTION();
        if (ImGui::TreeNodeEx("Mesh (FBX/GLTF) Import", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Import Mesh", &m_MeshOptions.importMesh);
            ImGui::Checkbox("Import Materials & Textures", &m_MeshOptions.importMaterials);
            ImGui::Checkbox("Import Skeleton", &m_MeshOptions.importSkeleton);
            ImGui::Checkbox("Import Animations", &m_MeshOptions.importAnimations);
            ImGui::Checkbox("Force Rebuild ?", &m_MeshOptions.forceRebuild);

            if (m_MeshOptions.importAnimations)
            {
                ImGui::SeparatorText("Animation Options");
                ImGui::Checkbox("Use Existing Skeleton", &m_MeshOptions.useExistingSkeletonForAnimations);

                if (m_MeshOptions.useExistingSkeletonForAnimations)
                {
                    const bool hasSkeleton = m_MeshOptions.existingSkeletonHandle != AssetHandle(0);
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
                                    const AssetMetaData &metadata = project->GetAssetManager()->GetMetaData(droppedHandle);
                                    if (metadata.type == AssetType::Skeleton)
                                    {
                                        m_MeshOptions.existingSkeletonHandle = droppedHandle;
                                    }
                                }

                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Clear Skeleton"))
                    {
                        m_MeshOptions.existingSkeletonHandle = AssetHandle(0);
                    }

                    ImGui::TextDisabled("Handle: %llu", static_cast<uint64_t>(m_MeshOptions.existingSkeletonHandle));
                }
            }

            ImGui::TreePop();
        }
    }

    void AssetImporterPanel::DrawTextureImportOptions()
    {
        if (ImGui::TreeNodeEx("Texture Import", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("No texture import options yet.");
            ImGui::TreePop();
        }
    }

    void AssetImporterPanel::DrawFontImportOptions()
    {
        if (ImGui::TreeNodeEx("Font Import", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("No font import options yet.");
            ImGui::TreePop();
        }
    }

    void AssetImporterPanel::DrawGenericImportOptions()
    {
        if (ImGui::TreeNodeEx("Import##import_tree_node", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TextDisabled("No additional import options for this asset type.");
            ImGui::TreePop();
        }
    }

    bool AssetImporterPanel::ProcessImportRequest(const AssetImportData &asset)
    {
        IGN_PROFILE_FUNCTION();
        auto project = m_EditorLayer->GetActiveProject();
        if (!project)
        {
            return false;
        }

        auto assetManager = project->GetAssetManager();
        if (!assetManager)
        {
            return false;
        }

        // Submit the import work to the AssetManager worker threads so the main thread
        // is not blocked by file IO or heavy processing. Capture copies of the
        // relevant data so the lambda is independent of the caller's stack.
        AssetImportData jobData = asset;
        auto jobProject = project;
        auto jobAssetManager = assetManager;

        jobAssetManager->SubmitJob([this, jobProject, jobAssetManager, jobData]() mutable
        {
            IGN_PROFILE_SCOPE("AssetImporterPanel::ProcessImportRequest::SubmitJob");

            // FBX handling (mesh or skeleton/animations) — these do their own heavy work
            // and register everything internally, so we leave them unchanged.
            if (IsFbxFile(jobData.filepath))
            {
                if (jobData.meshOptions.importMesh || jobData.meshOptions.importMaterials)
                {
                    ImportFbxMesh(jobData.filepath, jobData.meshOptions);
                }
                else
                {
                    ImportFbxSkeletonAndAnimations(jobData.filepath, jobData.meshOptions);
                }
                return;
            }

            // Non-FBX generic asset import:
            // 1. Copy the file into the project asset directory (filesystem work on worker thread).
            // 2. Register only the metadata — do NOT load the asset into memory.
            //    Assets are loaded lazily the first time they are actually used
            //    (opened in the editor, referenced in a scene, dropped into a slot, etc.).
            const std::filesystem::path importedPath = PrepareAssetForImport(jobData);
            if (importedPath.empty())
            {
                return;
            }

            const std::filesystem::path relativePath = jobProject->GetProjectFilepath(importedPath);
            const AssetType assetType = jobData.assetType;

            // Commit the registry entry on the main thread to prevent TOCTOU races
            // between concurrent workers importing different assets to the same path.
            Application::SubmitToMainThread([jobAssetManager, relativePath, assetType]()
            {
                // Re-check: another concurrent import job may have already registered this path.
                AssetHandle finalHandle = jobAssetManager->GetAssetHandle(relativePath);
                if (finalHandle == AssetHandle(0))
                {
                    // Not yet registered — create a new entry in the registry only.
                    finalHandle = AssetHandle();

                    AssetMetaData metadata;
                    metadata.filepath = relativePath;
                    metadata.type = assetType;

                    jobAssetManager->AssignMetaData(finalHandle, metadata);

                    LOG_TRACE("[Asset Importer] Registered asset (not loaded): {} ({})",
                        static_cast<uint64_t>(finalHandle), relativePath.generic_string());
                }
                // The asset is intentionally NOT loaded here.
                // It will be loaded on demand via GetAsset() / GetAssetImmediate()
                // the first time it is referenced in a scene or opened in the editor.
            }, "AssetImporterPanel::RegisterMetaData");
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
            m_MeshOptions = m_CurrentAsset->meshOptions;
            m_MeshOptions.targetDirectory = m_TargetDirectory;
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
        m_OpenImporterPopup = false;
        m_ShowImporterWindow = false;
    }

    AssetImporterPanel::AssetImportData AssetImporterPanel::BuildCurrentImportData() const
    {
        AssetImportData data = m_CurrentAsset.value();
        if (IsMeshImportDialogFile(data.filepath))
        {
            data.meshOptions = m_MeshOptions;
            data.meshOptions.targetDirectory = m_TargetDirectory;
        }

        return data;
    }

    void AssetImporterPanel::ImportCurrentAsset()
    {
        if (!m_CurrentAsset.has_value())
        {
            return;
        }

        bool importedAny = false;
        const AssetType currentType = m_CurrentAsset->assetType;
        importedAny |= ProcessImportRequest(BuildCurrentImportData());

        if (m_SkipDialogForSameType)
        {
            auto queueIt = m_ImportQueues.find(currentType);
            if (queueIt != m_ImportQueues.end())
            {
                while (!queueIt->second.empty())
                {
                    AssetImportData data = queueIt->second.front();
                    queueIt->second.pop();
                    data.meshOptions = m_MeshOptions;
                    data.meshOptions.targetDirectory = m_TargetDirectory;
                    importedAny |= ProcessImportRequest(data);
                }
            }
        }

        if (importedAny)
        {
            m_EditorLayer->SaveProject();
            m_EditorLayer->RefreshContentBrowsers();
        }

        if (!AdvanceToNextAsset())
        {
            ResetImportState();
        }
    }

    void AssetImporterPanel::SkipCurrentAsset()
    {
        AdvanceToNextAsset();
        if (!m_CurrentAsset.has_value())
        {
            ResetImportState();
        }
    }

    std::filesystem::path AssetImporterPanel::PrepareAssetForImport(const AssetImportData &asset) const
    {
        auto project = m_EditorLayer->GetActiveProject();
        if (!project)
        {
            return {};
        }

        std::error_code ec;
        const std::filesystem::path sourcePath = std::filesystem::weakly_canonical(asset.filepath, ec);
        if (ec || !std::filesystem::exists(sourcePath))
        {
            LOG_ERROR("[Asset Importer] File does not exist '{}'", asset.filepath.generic_string());
            return {};
        }

        const std::filesystem::path assetDirectory = std::filesystem::weakly_canonical(project->GetAssetDirectory(), ec);
        if (ec)
        {
            return {};
        }

        if (IsPathWithin(sourcePath, assetDirectory))
        {
            return sourcePath;
        }

        std::filesystem::path targetDirectory = m_TargetDirectory;
        if (targetDirectory.empty())
        {
            targetDirectory = project->GetAssetDirectory();
        }

        std::filesystem::create_directories(targetDirectory, ec);
        if (ec)
        {
            LOG_ERROR("[Asset Importer] Failed to create target directory '{}'", targetDirectory.generic_string());
            return {};
        }

        std::filesystem::path destinationPath = targetDirectory / sourcePath.filename();
        if (std::filesystem::exists(destinationPath))
        {
            destinationPath = BuildUniquePath(targetDirectory, sourcePath.stem().string(), sourcePath.extension().string());
        }

        std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            LOG_ERROR("[Asset Importer] Failed to copy '{}' to '{}': {}", sourcePath.generic_string(), destinationPath.generic_string(), ec.message());
            return {};
        }

        return destinationPath;
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

    void AssetImporterPanel::ImportFbxMesh(const std::filesystem::path &filepath, const MeshImportOptions &options)
    {
        IGN_PROFILE_FUNCTION();
        auto project = m_EditorLayer->GetActiveProject();
        if (!project)
        {
            return;
        }

        auto assetManager = project->GetAssetManager();
        const std::filesystem::path filename = filepath.stem();
        const std::filesystem::path outputRootDirectory = options.targetDirectory.empty() ? project->GetAssetDirectory() : options.targetDirectory;
        const std::filesystem::path skmBinaryPath = outputRootDirectory / filename / "Mesh" / (filename.string() + GetAssetExtensionFromType(AssetType::Mesh));
        const std::filesystem::path skmRelativePath = project->GetProjectFilepath(skmBinaryPath);

        AssetHandle handle = assetManager->GetAssetHandle(skmRelativePath);
        if (handle == AssetHandle(0))
        {
            handle = AssetHandle();
        }

        AssetMetaData sourceMetadata;
        sourceMetadata.filepath = filepath;
        sourceMetadata.type = AssetType::Mesh;

        Ref<Mesh> importedAsset = AssetImporter::ImportMesh(handle, sourceMetadata, assetManager, options);
        if (!importedAsset)
        {
            LOG_ERROR("[Asset Importer] Failed to import mesh from {}", filepath.generic_string());
            return;
        }

        if (options.importMesh)
        {
            AssetMetaData registryMetadata;
            registryMetadata.filepath = skmRelativePath;
            registryMetadata.type = AssetType::Mesh;

            PublishImportedAsset(assetManager, registryMetadata, importedAsset);
        }
    }

    void AssetImporterPanel::ImportFbxSkeletonAndAnimations(const std::filesystem::path &filepath, const MeshImportOptions &options)
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
        const std::filesystem::path assetDirectory = options.targetDirectory.empty() ? project->GetAssetDirectory() : options.targetDirectory;
        const std::filesystem::path filename = filepath.stem();

        const std::filesystem::path outputDirectory = assetDirectory / filename;
        const std::filesystem::path skeletalMeshDirectory = outputDirectory / "Mesh";
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

        Ref<Skeleton> skeleton = nullptr;
        if (options.useExistingSkeletonForAnimations && options.existingSkeletonHandle != AssetHandle(0))
        {
            std::mutex waitMutex;
            std::condition_variable waitCv;
            bool resolved = false;

            Application::SubmitToMainThread([project, existingHandle = options.existingSkeletonHandle, &skeleton, &waitMutex, &waitCv, &resolved]()
            {
                skeleton = project->GetAsset<Skeleton>(existingHandle);
                if (!skeleton)
                {
                    skeleton = project->GetAssetImmediate<Skeleton>(existingHandle);
                }

                {
                    std::lock_guard<std::mutex> guard(waitMutex);
                    resolved = true;
                }
                waitCv.notify_one();
            }, "AssetImporterPanel::ResolveExistingSkeleton");

            std::unique_lock<std::mutex> waitLock(waitMutex);
            waitCv.wait(waitLock, [&resolved]() { return resolved; });
        }

        if (!skeleton)
        {
            skeleton = CreateRef<Skeleton>();
            FBXMeshLoader::LoadSkeletonOnlyFromFBX(filepath.generic_string(), skeleton, assetManager);
        }

        if (!skeleton)
        {
            LOG_WARN("[Asset Importer] FBX has no valid skeleton: {}", filepath.generic_string());
            return;
        }

        const std::string skeletonExt = GetAssetExtensionFromType(AssetType::Skeleton);
        const std::string animationExt = GetAssetExtensionFromType(AssetType::SkeletalAnimation);
        std::filesystem::path importedSkeletonRelativePath;

        if (options.importSkeleton)
        {
            const std::filesystem::path skeletonPath = skeletalMeshDirectory / (filename.string() + skeletonExt);
            skeleton->Serialize(skeletonPath);

            AssetMetaData skeletonMD;
            skeletonMD.filepath = project->GetProjectFilepath(skeletonPath);
            skeletonMD.type = AssetType::Skeleton;
            importedSkeletonRelativePath = skeletonMD.filepath;

            skeleton->handle = assetManager->GetAssetHandle(skeletonMD.filepath);
            if (skeleton->handle == AssetHandle(0))
            {
                skeleton->handle = AssetHandle();
            }

            PublishImportedAsset(assetManager, skeletonMD, skeleton);
        }
        else if (options.existingSkeletonHandle != AssetHandle(0))
        {
            skeleton->handle = options.existingSkeletonHandle;
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
                animationMD.filepath = project->GetProjectFilepath(animationPath);
                animationMD.type = AssetType::SkeletalAnimation;

                const AssetHandle fallbackSkeletonHandle = skeleton ? skeleton->handle : AssetHandle(0);
                Application::SubmitToMainThread([assetManager, animationMD, animation, importedSkeletonRelativePath, fallbackSkeletonHandle]()
                {
                    AssetHandle skeletonHandle = fallbackSkeletonHandle;
                    if (!importedSkeletonRelativePath.empty())
                    {
                        const AssetHandle importedSkeletonHandle = assetManager->GetAssetHandle(importedSkeletonRelativePath);
                        if (importedSkeletonHandle != AssetHandle(0))
                        {
                            skeletonHandle = importedSkeletonHandle;
                        }
                    }

                    AssetHandle animationHandle = assetManager->GetAssetHandle(animationMD.filepath);
                    if (animationHandle == AssetHandle(0))
                    {
                        animationHandle = AssetHandle();
                    }

                    animation->handle = animationHandle;
                    animation->SetSkeletonHandle(skeletonHandle);
                    animation->SetReadyFlag(true);
                    assetManager->AssignMetaData(animationHandle, animationMD);
                    assetManager->AssignAsset(animationHandle, animation);
                }, "AssetImporterPanel::PublishImportedAnimation");
            }
        }
    }

}
