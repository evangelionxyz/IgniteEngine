// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "asset_importer.hpp"
#include "ignite/asset/asset_worker.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/application.hpp"

#include "ignite/audio/fmod_audio.hpp"
#include "ignite/audio/fmod_sound.hpp"

#include "ignite/project/project.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/serializer/scene_serializer.hpp"
#include "ignite/serializer/binary_serializer.hpp"

#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/animation_montage.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/locomotion.hpp"
#include "ignite/animation/blend_space.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/animation/animator/animator_controller_2d.hpp"

#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/prefab.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/scripting/scriptable_object.hpp"
#include "ignite/scripting/lua/lua_script.hpp"

namespace ignite
{
    namespace
    {
        std::mutex s_UniqueAssetPathMutex;

        std::string ToLowerAsciiCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        template<MeshVertex VertexType_T>
        void ImportMeshMaterialsAndTextures(
            MeshScene<VertexType_T> &meshScene,
            const std::filesystem::path &outputRootDirectory,
            const std::string &materialBinExt,
            bool allowOverwrite,
            const StaticMeshImportPayload &payload,
            AssetManager *assetManager,
            Project *project,
            std::unordered_set<std::string> &reservedPaths)
        {
            std::vector<std::array<AssetHandle, 5>> materialTextureHandles(meshScene.materials.size());
            std::unordered_map<Ref<Texture>, AssetHandle> serializedTextures;

            for (size_t i = 0; i < meshScene.materialTextureMap.size(); ++i)
            {
                auto &textureHandles = materialTextureHandles[i];
                std::fill(textureHandles.begin(), textureHandles.end(), AssetHandle(0));

                for (size_t j = 0; j < meshScene.materialTextureMap[i].size(); ++j)
                {
                    auto &[idx, name, texture] = meshScene.materialTextureMap[i][j];
                    if (idx < 0 || !texture)
                    {
                        continue;
                    }

                    if (serializedTextures.contains(texture))
                    {
                        LOG_TRACE("[Asset Importer] Texture '{}' already serialized in this run, reusing handle {}", name, static_cast<uint64_t>(serializedTextures[texture]));
                        textureHandles[j] = serializedTextures[texture];
                        continue;
                    }

                    const bool writeEXR = texture->GetFormat() == nri::Format::RGBA32_SFLOAT;
                    const std::string textureExtension = writeEXR ? ".exr" : ".png";
                    const std::string cleanTexName = AssetImporter::SanitizeAssetName(name, "Texture");
                    const std::filesystem::path textureOutputFullPath = AssetImporter::BuildUniqueAssetPath(
                        outputRootDirectory,
                        cleanTexName,
                        textureExtension,
                        allowOverwrite ? nullptr : assetManager,
                        &reservedPaths,
                        !allowOverwrite);
                    const auto relativeTexturePath = project->GetProjectFilepath(textureOutputFullPath);

                    AssetHandle textureHandle = allowOverwrite ? assetManager->GetAssetHandle(relativeTexturePath) : AssetHandle(0);
                    assetManager->SuppressWatchPath(textureOutputFullPath, 1500);
                    if (textureHandle == AssetHandle(0))
                    {
                        LOG_INFO("[Asset Importer] Serializing new texture asset: {} -> {}", cleanTexName, relativeTexturePath.generic_string());
                        if (writeEXR)
                        {
                            BinarySerializer::SerializeTextureToEXR(texture, textureOutputFullPath);
                        }
                        else
                        {
                            BinarySerializer::SerializeTextureToPNG(texture, textureOutputFullPath);
                        }
                        assetManager->SuppressWatchPath(textureOutputFullPath, 1500);

                        textureHandle = AssetHandle();

                        AssetMetaData textureMD;
                        textureMD.filepath = relativeTexturePath;
                        textureMD.type = AssetType::Texture;

                        assetManager->AssignAsset(textureHandle, texture);
                        assetManager->AssignMetaData(textureHandle, textureMD);
                    }
                    else
                    {
                        LOG_INFO("[Asset Importer] Overwriting existing texture asset: {} -> {} (Handle: {})", cleanTexName, relativeTexturePath.generic_string(), static_cast<uint64_t>(textureHandle));
                        if (writeEXR)
                        {
                            BinarySerializer::SerializeTextureToEXR(texture, textureOutputFullPath);
                        }
                        else
                        {
                            BinarySerializer::SerializeTextureToPNG(texture, textureOutputFullPath);
                        }
                        assetManager->SuppressWatchPath(textureOutputFullPath, 1500);
                        if (Ref<Asset> prevTex = assetManager->TryGetAsset(textureHandle))
                        {
                            assetManager->DeferReleaseResource(prevTex);
                        }
                        assetManager->AssignAsset(textureHandle, texture);
                    }

                    texture->handle = textureHandle;
                    serializedTextures[texture] = textureHandle;
                    textureHandles[j] = textureHandle;
                    if (payload.outUpdatedTextures)
                    {
                        payload.outUpdatedTextures->push_back(textureHandle);
                    }
                }
            }

            for (size_t i = 0; i < meshScene.materials.size(); ++i)
            {
                Ref<Material> &mat = meshScene.materials[i];
                mat->baseColorTextureHandle = materialTextureHandles[i][0];
                mat->emissiveTextureHandle = materialTextureHandles[i][1];
                mat->metallicTextureHandle = materialTextureHandles[i][2];
                mat->roughnessTextureHandle = materialTextureHandles[i][2];
                mat->normalTextureHandle = materialTextureHandles[i][3];
                mat->occlusionTextureHandle = materialTextureHandles[i][4];

                const std::string cleanMaterialName = AssetImporter::SanitizeAssetName(mat->name, "Material");
                const std::filesystem::path materialBinFullPath = AssetImporter::BuildUniqueAssetPath(
                    outputRootDirectory,
                    cleanMaterialName,
                    materialBinExt,
                    allowOverwrite ? nullptr : assetManager,
                    &reservedPaths,
                    !allowOverwrite);

                if (!allowOverwrite)
                {
                    mat->name = materialBinFullPath.stem().string();
                }

                // Serialize Material
                assetManager->SuppressWatchPath(materialBinFullPath);
                mat->Serialize(materialBinFullPath);

                const auto relativeMaterialPath = project->GetProjectFilepath(materialBinFullPath);

                AssetHandle materialHandle = allowOverwrite ? assetManager->GetAssetHandle(relativeMaterialPath) : AssetHandle(0);
                if (materialHandle == AssetHandle(0))
                {
                    materialHandle = AssetHandle();
                }
                mat->handle = materialHandle;

                AssetMetaData materialMD;
                materialMD.filepath = relativeMaterialPath;
                materialMD.type = AssetType::Material;

                if (Ref<Asset> existingAsset = assetManager->TryGetAsset(materialHandle))
                {
                    if (Ref<Material> existingMat = existingAsset->As<Material>())
                    {
                        auto updatedMat = mat;
                        Application::SubmitToRenderThread([existingMat, updatedMat]()
                        {
                            existingMat->name = updatedMat->name;
                            existingMat->SetType(updatedMat->GetType());
                            existingMat->SetCullMode(updatedMat->GetCullMode());
                            existingMat->baseColorTextureHandle = updatedMat->baseColorTextureHandle;
                            existingMat->emissiveTextureHandle = updatedMat->emissiveTextureHandle;
                            existingMat->metallicTextureHandle = updatedMat->metallicTextureHandle;
                            existingMat->roughnessTextureHandle = updatedMat->roughnessTextureHandle;
                            existingMat->normalTextureHandle = updatedMat->normalTextureHandle;
                            existingMat->occlusionTextureHandle = updatedMat->occlusionTextureHandle;
                            existingMat->gpuData = updatedMat->gpuData;
                            existingMat->InvalidateBindingSet();
                            existingMat->SetDirtyFlag(false);
                            existingMat->SetReadyFlag(true);
                        }, "AssetImporter::UpdateMeshMaterial");
                    }
                    else
                    {
                        assetManager->AssignAsset(materialHandle, mat);
                    }
                }
                else
                {
                    assetManager->AssignAsset(materialHandle, mat);
                }

                assetManager->AssignMetaData(materialHandle, materialMD);
                if (payload.outUpdatedMaterials)
                {
                    payload.outUpdatedMaterials->push_back(materialHandle);
                }
            }
        }
    }

    std::string AssetImporter::SanitizeAssetName(const std::string &name, const std::string &fallback)
    {
        std::string result = name;
        for (char &ch : result)
        {
            const unsigned char uch = static_cast<unsigned char>(ch);
            if (uch < 32 || ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|')
            {
                ch = '_';
            }
        }

        const size_t first = result.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
        {
            return fallback;
        }
        const size_t last = result.find_last_not_of(" \t\r\n");
        result = result.substr(first, last - first + 1);
        return result.empty() ? fallback : result;
    }

    std::filesystem::path AssetImporter::BuildUniqueAssetPath(
        const std::filesystem::path &directory,
        const std::string &baseName,
        const std::string &extension,
        AssetManager *assetManager,
        std::unordered_set<std::string> *reservedLowerPaths,
        bool checkExisting)
    {
        std::lock_guard<std::mutex> lock(s_UniqueAssetPathMutex);

        const std::string cleanBase = SanitizeAssetName(baseName, "Asset");
        std::string cleanExt = extension;
        if (!cleanExt.empty() && cleanExt.front() != '.')
        {
            cleanExt.insert(cleanExt.begin(), '.');
        }

        auto isAvailable = [&](const std::filesystem::path &candidate) -> bool
        {
            if (checkExisting)
            {
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec))
                {
                    return false;
                }
                if (assetManager && assetManager->GetAssetHandle(candidate) != AssetHandle(0))
                {
                    return false;
                }
            }
            if (reservedLowerPaths)
            {
                const std::string key = ToLowerAsciiCopy(candidate.lexically_normal().generic_string());
                if (reservedLowerPaths->contains(key))
                {
                    return false;
                }
            }
            return true;
        };

        std::filesystem::path candidate = directory.empty()
            ? std::filesystem::path(cleanBase + cleanExt)
            : (directory / (cleanBase + cleanExt));

        if (!isAvailable(candidate))
        {
            uint32_t suffix = 1;
            while (true)
            {
                const std::string suffixedName = std::format("{}_{}{}", cleanBase, suffix, cleanExt);
                candidate = directory.empty() ? std::filesystem::path(suffixedName) : (directory / suffixedName);
                if (isAvailable(candidate))
                {
                    break;
                }
                ++suffix;
            }
        }

        if (reservedLowerPaths)
        {
            reservedLowerPaths->insert(ToLowerAsciiCopy(candidate.lexically_normal().generic_string()));
        }

        return candidate;
    }

    static std::unordered_map<AssetType, std::function<Ref<Asset>(AssetHandle, const AssetMetaData &, AssetManager *)>> s_ImportFunctions =
    {
        { AssetType::Audio, AssetImporter::ImportAudio },
        { AssetType::Scene, AssetImporter::ImportScene },
        { AssetType::Prefab, AssetImporter::ImportPrefab },
        { AssetType::Texture, [](AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager) { 
                    return AssetImporter::ImportTexture(handle, metadata, assetManager); 
                }},
        { AssetType::StaticMesh, [](AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager) { 
                    return AssetImporter::ImportStaticMesh(handle, metadata, assetManager); 
                }},
        { AssetType::SkeletalMesh, [](AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager) { 
                    return AssetImporter::ImportSkeletalMesh(handle, metadata, assetManager); 
                }},
        { AssetType::Material, AssetImporter::ImportMaterial },
        { AssetType::Material2D, AssetImporter::ImportMaterial2D },
        { AssetType::SpriteSheet, AssetImporter::ImportSpriteSheet },
        { AssetType::Font, AssetImporter::ImportFont },
        { AssetType::Widget, AssetImporter::ImportWidget },
        { AssetType::Skeleton, AssetImporter::ImportSkeleton },
        { AssetType::SkeletalAnimation, AssetImporter::ImportSkeletalAnimation },
        { AssetType::AnimationMontage, AssetImporter::ImportAnimationMontage },
        { AssetType::BlendSpace, AssetImporter::ImportBlendSpace },
        { AssetType::LocomotionController, AssetImporter::ImportLocomotionController },
        { AssetType::AnimatorController, AssetImporter::ImportAnimatorController },
        { AssetType::Animation2D, AssetImporter::ImportAnimation2D },
        { AssetType::AnimatorController2D, AssetImporter::ImportAnimatorController2D },
        { AssetType::ScriptableObject, AssetImporter::ImportScriptableObject },
        { AssetType::LuaScript, AssetImporter::ImportLuaScript },
        { AssetType::Terrain, AssetImporter::ImportTerrain },
    };

    Ref<Asset> AssetImporter::Import(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        // should be always importing with full filepath
        if (auto project = assetManager->LockActiveProject())
        {
            AssetMetaData metadataCopy = metadata;
            metadataCopy.filepath = project->GetProjectFilepath(metadata.filepath);
            if (s_ImportFunctions.contains(metadataCopy.type))
            {
                return s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy, assetManager);
            }
        }
        
        return nullptr;
    }

    Ref<SpriteSheet> AssetImporter::ImportSpriteSheet(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<SpriteSheet> spriteSheet = SpriteSheet::Deserialize(metadata.filepath);
        if (spriteSheet)
        {
            spriteSheet->handle = handle;
            spriteSheet->SetReadyFlag(true);
            spriteSheet->SetDirtyFlag(false);
        }

        return spriteSheet;
    }

    Ref<Font> AssetImporter::ImportFont(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!assetManager)
            return nullptr;

        auto project = assetManager->LockActiveProject();
        if (!project)
            return nullptr;

        std::filesystem::path fontFilepath = metadata.filepath;
        if (!std::filesystem::exists(fontFilepath))
        {
            fontFilepath = project->GetProjectFilepath(metadata.filepath);
        }

        if (!std::filesystem::exists(fontFilepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<Font> font = Font::Create(fontFilepath, false);
        if (font)
        {
            font->handle = handle;
            font->SetDirtyFlag(false);
        }

        return font;
    }

    Ref<WidgetCanvas> AssetImporter::ImportWidget(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<WidgetCanvas> widget = WidgetCanvas::Deserialize(metadata.filepath);
        if (widget)
        {
            widget->handle = handle;
            widget->SetDirtyFlag(false);
            widget->SetReadyFlag(true);
        }

        return widget;
    }

    void AssetImporter::ImportAsync(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, std::function<void(Ref<Asset>, AssetHandle)> callback)
    {
        if (!assetManager)
            return;
        
        AssetWorker::SubmitJob([handle, metadata, assetManager, callback]()
        {
            // should be always importing with full filepath
            AssetMetaData metadataCopy = metadata;
        
            auto project = assetManager->LockActiveProject();
            if (!project)
                return;

            metadataCopy.filepath = project->GetProjectFilepath(metadata.filepath);

            Ref<Asset> asset;
            if (s_ImportFunctions.contains(metadataCopy.type))
            {
                asset = s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy, assetManager);
            }

            if (asset)
            {
                callback(asset, handle);
            }
        });
    }

    Ref<AnimationMontage> AssetImporter::ImportAnimationMontage(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<AnimationMontage> asset = AnimationMontage::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<BlendSpace> AssetImporter::ImportBlendSpace(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<BlendSpace> asset = BlendSpace::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetSkeletonAssetHandle(asset->GetSkeletonAssetHandle());
            asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<LocomotionController> AssetImporter::ImportLocomotionController(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<LocomotionController> asset = LocomotionController::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<SkeletalMesh> AssetImporter::ImportSkeletalMesh(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, const SkeletalMeshImportPayload &payload)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        const auto skeletalMeshBinExt = GetAssetExtensionFromType(AssetType::SkeletalMesh);
        const auto skeletonBinExt = GetAssetExtensionFromType(AssetType::Skeleton);
        const auto animationBinExt = GetAssetExtensionFromType(AssetType::SkeletalAnimation);
        const auto materialBinExt = GetAssetExtensionFromType(AssetType::Material);

        auto project = assetManager->LockActiveProject();
        if (!project)
            return nullptr;

        const std::filesystem::path projectAssetPath = project->GetAssetDirectory();
        const std::filesystem::path outputRootDirectory = payload.targetDirectory.empty() ? projectAssetPath : payload.targetDirectory;
        const std::filesystem::path filename = metadata.filepath.stem();

        auto resolveTrackedSourcePath = [&project](const std::filesystem::path &sourceFile) -> std::filesystem::path
        {
            std::error_code ec;
            std::filesystem::path absSource = std::filesystem::absolute(sourceFile, ec).lexically_normal();
            if (ec)
                absSource = sourceFile.lexically_normal();
            return project->GetProjectRelativeFilepath(absSource);
        };

        Ref<SkeletalMesh> asset;

        auto prepareMeshGpuAndMaterials = [assetManager, project](const Ref<SkeletalMesh> &mesh)
        {
            if (!mesh)
                return;

            for (const auto &mesh : mesh->GetMeshInstances())
            {
                AssetHandle materialHandle = mesh->GetMaterialAssetHandle();
                AssetMetaData materialMetadata = assetManager->GetMetaData(materialHandle);
                if (materialMetadata.type == AssetType::Material)
                {
                    // Avoid overwriting an already-loaded material from a background mesh import,
                    // which could destroy live GPU resources while they are referenced by active command buffers.
                    if (assetManager->TryGetAsset(materialHandle))
                        continue;

                    const auto &materialFilepath = project->GetProjectFilepath(materialMetadata.filepath);
                    Ref<Material> material = Material::Deserialize(materialFilepath);
                    assetManager->AssignAsset(materialHandle, material);
                }
            }
        };

        if (metadata.filepath.extension() == skeletalMeshBinExt)
        {
            asset = SkeletalMesh::Deserialize(metadata.filepath);
            if (asset)
            {
                asset->handle = handle;
                asset->SetSourcePath(Asset::ReadSourcePathFromMetaFile(metadata.filepath));
                prepareMeshGpuAndMaterials(asset);
                asset->CalculateLocalAABB();
            }
            return asset;
        }

        const bool allowOverwrite = payload.overwriteExisting || !payload.outputMeshPath.empty();
        if (!allowOverwrite && (handle == AssetHandle(0) || assetManager->IsAssetHandleValid(handle)))
        {
            handle = AssetHandle();
        }

        std::unordered_set<std::string> reservedPaths;
        const std::string cleanMeshStem = SanitizeAssetName(filename.string(), "SkeletalMesh");
        std::filesystem::path meshBinaryPath;
        if (!payload.outputMeshPath.empty())
        {
            meshBinaryPath = payload.outputMeshPath;
            reservedPaths.insert(ToLowerAsciiCopy(meshBinaryPath.lexically_normal().generic_string()));
        }
        else if (allowOverwrite)
        {
            meshBinaryPath = BuildUniqueAssetPath(outputRootDirectory, cleanMeshStem, skeletalMeshBinExt, nullptr, &reservedPaths, false);
        }
        else
        {
            meshBinaryPath = BuildUniqueAssetPath(outputRootDirectory, cleanMeshStem, skeletalMeshBinExt, assetManager, &reservedPaths, true);
        }

        const std::filesystem::path skeletonPath = BuildUniqueAssetPath(
            outputRootDirectory,
            cleanMeshStem,
            skeletonBinExt,
            allowOverwrite ? nullptr : assetManager,
            &reservedPaths,
            !allowOverwrite);
        const std::filesystem::path meshRelativePath = project->GetProjectFilepath(meshBinaryPath);

        // Fast path: use cached binaries generated in dedicated folders when overwriting/reusing in-place.
        if (!payload.forceRebuild && allowOverwrite && std::filesystem::exists(meshBinaryPath) && std::filesystem::exists(skeletonPath))
        {
            asset = SkeletalMesh::Deserialize(meshBinaryPath);
            if (!asset)
            {
                return nullptr;
            }
            asset->SetSourcePath(Asset::ReadSourcePathFromMetaFile(meshBinaryPath));

            // skeleton
            AssetMetaData skeletonMD;
            skeletonMD.filepath = project->GetProjectFilepath(skeletonPath);
            skeletonMD.type = AssetType::Skeleton;

            AssetHandle skeletonHandle = assetManager->GetAssetHandle(skeletonMD.filepath);
            if (skeletonHandle == AssetHandle(0))
            {
                skeletonHandle = AssetHandle();
            }

            Ref<Skeleton> skeletonAsset = Skeleton::Deserialize(skeletonPath);
            if (skeletonAsset)
            {
                skeletonAsset->handle = skeletonHandle;
                assetManager->AssignMetaData(skeletonHandle, skeletonMD);
                assetManager->AssignAsset(skeletonHandle, skeletonAsset);
            }

            // animations
            std::vector<std::filesystem::path> animationFiles;
            if (std::filesystem::exists(outputRootDirectory))
            {
                for (const auto &entry : std::filesystem::directory_iterator(outputRootDirectory.string()))
                {
                    if (entry.is_regular_file() && entry.path().extension() == animationBinExt)
                    {
                        animationFiles.push_back(entry.path().string());
                    }
                }
            }

            std::sort(animationFiles.begin(), animationFiles.end());

            for (const auto &animationPath : animationFiles)
            {
                AssetMetaData animationMD;
                animationMD.filepath = project->GetProjectFilepath(animationPath);
                animationMD.type = AssetType::SkeletalAnimation;

                AssetHandle animationHandle = assetManager->GetAssetHandle(animationMD.filepath);
                if (animationHandle == AssetHandle(0))
                {
                    animationHandle = AssetHandle();
                }

                Ref<SkeletalAnimation> animationAsset = SkeletalAnimation::Deserialize(animationPath);
                if (!animationAsset)
                {
                    continue;
                }

                animationAsset->SetSkeletonHandle(skeletonHandle ? skeletonHandle : AssetHandle(0));

                animationAsset->handle = animationHandle;
                assetManager->AssignMetaData(animationHandle, animationMD);
                assetManager->AssignAsset(animationHandle, animationAsset);
            }

            asset->handle = handle;
            prepareMeshGpuAndMaterials(asset);
            asset->CalculateLocalAABB();
            return asset;
        }

        // Load from .FBX/.GLTF/.GLB
        MeshScene<VertexMeshAnim> meshScene;
        MeshLoader::LoadSceneGraph(metadata.filepath.generic_string(), meshScene, assetManager);

        std::error_code dirEc;
        if (!std::filesystem::exists(outputRootDirectory, dirEc))
            std::filesystem::create_directories(outputRootDirectory, dirEc);

        // Import textures and materials from the same parse used for skeleton/animation,
        // so bone IDs and skeleton mapping stay consistent.
        if (payload.importMaterials)
        {
            LOG_INFO("[Asset Importer] Importing materials and textures for SkeletalMesh: {} materials to process", meshScene.materials.size());
            ImportMeshMaterialsAndTextures(meshScene, outputRootDirectory, materialBinExt, allowOverwrite, payload, assetManager, project.get(), reservedPaths);
        }

        asset = SkeletalMesh::Create();
        if (payload.importMesh)
        {
            for (size_t meshIdx = 0; meshIdx < meshScene.flatMeshes.size(); ++meshIdx)
            {
                Ref<SkeletalMeshInstance> m = meshScene.flatMeshes[meshIdx];
                m->global = glm::mat4(1.0f);

                if (payload.importMaterials && meshScene.materialMap.contains(static_cast<int>(meshIdx)))
                {
                    const int matIdx = meshScene.materialMap[static_cast<int>(meshIdx)];
                    if (matIdx >= 0 && matIdx < static_cast<int>(meshScene.materials.size()))
                    {
                        Ref<Material> mat = meshScene.materials[matIdx];
                        m->SetMaterial(mat->handle);
                    }
                }
                asset->AddMeshInstance(m);
            }
        }

        // Skeleton
        if (payload.importSkeleton && meshScene.skeleton)
        {
            // Serialize skeleton
            assetManager->SuppressWatchPath(skeletonPath);
            meshScene.skeleton->Serialize(skeletonPath);

            const std::filesystem::path skeletonRelPath = project->GetProjectFilepath(skeletonPath);
            AssetHandle skeletonHandle = allowOverwrite ? assetManager->GetAssetHandle(skeletonRelPath) : AssetHandle(0);
            if (skeletonHandle == AssetHandle(0))
            {
                skeletonHandle = AssetHandle();
            }
            meshScene.skeleton->handle = skeletonHandle;

            AssetMetaData skeletonMD;
            skeletonMD.filepath = skeletonRelPath;
            skeletonMD.type = AssetType::Skeleton;

            assetManager->AssignAsset(skeletonHandle, meshScene.skeleton);
            assetManager->AssignMetaData(skeletonHandle, skeletonMD);

            asset->SetSkeleton(skeletonHandle);
        }

        // Animations
        if (payload.importAnimations)
        {
            const AssetHandle targetSkeletonHandle = (payload.useExistingSkeletonForAnimations && payload.existingSkeletonHandle != AssetHandle(0))
                ? payload.existingSkeletonHandle
                : (meshScene.skeleton ? meshScene.skeleton->handle : AssetHandle(0));

            for (size_t i = 0; i < meshScene.animations.size(); ++i)
            {
                Ref<SkeletalAnimation> animation = meshScene.animations[i];
                if (!animation)
                {
                    continue;
                }

                animation->SetSkeletonHandle(targetSkeletonHandle);

                const std::string cleanAnimName = SanitizeAssetName(animation->name, "Animation");
                const std::filesystem::path animationPath = BuildUniqueAssetPath(
                    outputRootDirectory,
                    cleanAnimName,
                    animationBinExt,
                    allowOverwrite ? nullptr : assetManager,
                    &reservedPaths,
                    !allowOverwrite);

                assetManager->SuppressWatchPath(animationPath);
                animation->Serialize(animationPath);

                const std::filesystem::path animRelPath = project->GetProjectFilepath(animationPath);
                AssetHandle animationHandle = allowOverwrite ? assetManager->GetAssetHandle(animRelPath) : AssetHandle(0);
                if (animationHandle == AssetHandle(0))
                {
                    animationHandle = AssetHandle();
                }
                animation->handle = animationHandle;

                AssetMetaData animationMD;
                animationMD.filepath = animRelPath;
                animationMD.type = AssetType::SkeletalAnimation;

                assetManager->AssignAsset(animationHandle, animation);
                assetManager->AssignMetaData(animationHandle, animationMD);
            }
        }

        asset->handle = handle;
        asset->SetSourcePath(resolveTrackedSourcePath(metadata.filepath));

        if (payload.importMesh)
        {
            prepareMeshGpuAndMaterials(asset);
            assetManager->SuppressWatchPath(meshBinaryPath);
            asset->Serialize(meshBinaryPath);
            asset->CalculateLocalAABB();

            const std::filesystem::path metaPath = Asset::GetMetaFilePath(meshBinaryPath);
            assetManager->SuppressWatchPath(metaPath);
            asset->SerializeMetaFile(metaPath, [&payload](Serializer &sr)
            {
                sr.AddKeyValue("ImportMaterials", payload.importMaterials);
                sr.AddKeyValue("ImportSkeleton", payload.importSkeleton);
                sr.AddKeyValue("ImportAnimations", payload.importAnimations);
            });

            if (Ref<Asset> existingAsset = assetManager->TryGetAsset(handle))
            {
                if (Ref<SkeletalMesh> existingMesh = existingAsset->As<SkeletalMesh>())
                {
                    auto newInstances = asset->GetMeshInstances();
                    const AssetHandle newSkelHandle = asset->GetSkeletonHandle();
                    const AssetHandle newAnimHandle = asset->GetAnimatorHandle();
                    const std::filesystem::path newSourcePath = asset->GetSourcePath();
                    Application::SubmitToRenderThread([assetManager, existingMesh, newInstances = std::move(newInstances), newSkelHandle, newAnimHandle, newSourcePath]() mutable
                    {
                        auto oldInstances = std::make_shared<std::vector<Ref<SkeletalMeshInstance>>>(existingMesh->GetMeshInstances());
                        existingMesh->SetMeshInstances(newInstances);
                        if (newSkelHandle != AssetHandle(0))
                            existingMesh->SetSkeleton(newSkelHandle);
                        if (newAnimHandle != AssetHandle(0))
                            existingMesh->SetAnimator(newAnimHandle);
                        existingMesh->SetSourcePath(newSourcePath);
                        existingMesh->CalculateLocalAABB();
                        existingMesh->SetReadyFlag(true);
                        assetManager->DeferReleaseResource(std::move(oldInstances));
                    }, "AssetImporter::UpdateSkeletalMesh");
                    asset = existingMesh;
                }
                else
                {
                    assetManager->AssignAsset(handle, asset);
                }
            }
            else
            {
                assetManager->AssignAsset(handle, asset);
            }

            assetManager->AssignMetaData(handle, { meshRelativePath, AssetType::SkeletalMesh });
            assetManager->TrackMeshSourceAsset(handle);
        }

        return asset;
    }

    Ref<StaticMesh> AssetImporter::ImportStaticMesh(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, const StaticMeshImportPayload &payload)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        const auto staticMeshBinExt = GetAssetExtensionFromType(AssetType::StaticMesh);
        const auto materialBinExt = GetAssetExtensionFromType(AssetType::Material);

        auto project = assetManager->LockActiveProject();
        if (!project)
            return nullptr;

        const std::filesystem::path projectAssetPath = project->GetAssetDirectory();
        const std::filesystem::path outputRootDirectory = payload.targetDirectory.empty() ? projectAssetPath : payload.targetDirectory;
        const std::filesystem::path filename = metadata.filepath.stem();

        auto resolveTrackedSourcePath = [&project](const std::filesystem::path &sourceFile) -> std::filesystem::path
        {
            std::error_code ec;
            std::filesystem::path absSource = std::filesystem::absolute(sourceFile, ec).lexically_normal();
            if (ec)
                absSource = sourceFile.lexically_normal();
            return project->GetProjectRelativeFilepath(absSource);
        };

        Ref<StaticMesh> asset;

        auto prepareMeshGpuAndMaterials = [assetManager, project](const Ref<StaticMesh> &mesh)
        {
            if (!mesh)
                return;

            for (const auto &mesh : mesh->GetMeshInstances())
            {
                AssetHandle materialHandle = mesh->GetMaterialAssetHandle();
                AssetMetaData materialMetadata = assetManager->GetMetaData(materialHandle);
                if (materialMetadata.type == AssetType::Material)
                {
                    // Avoid overwriting an already-loaded material from a background mesh import,
                    // which could destroy live GPU resources while they are referenced by active command buffers.
                    if (assetManager->TryGetAsset(materialHandle))
                        continue;

                    const auto &materialFilepath = project->GetProjectFilepath(materialMetadata.filepath);
                    Ref<Material> material = Material::Deserialize(materialFilepath);
                    assetManager->AssignAsset(materialHandle, material);
                }
            }
        };

        if (metadata.filepath.extension() == staticMeshBinExt)
        {
            asset = StaticMesh::Deserialize(metadata.filepath);
            if (asset)
            {
                asset->handle = handle;
                asset->SetSourcePath(Asset::ReadSourcePathFromMetaFile(metadata.filepath));
                prepareMeshGpuAndMaterials(asset);
                asset->CalculateLocalAABB();
            }
            return asset;
        }

        const bool allowOverwrite = payload.overwriteExisting || !payload.outputMeshPath.empty();
        if (!allowOverwrite && (handle == AssetHandle(0) || assetManager->IsAssetHandleValid(handle)))
        {
            handle = AssetHandle();
        }

        std::unordered_set<std::string> reservedPaths;
        const std::string cleanMeshStem = SanitizeAssetName(filename.string(), "StaticMesh");
        std::filesystem::path meshBinaryPath;
        if (!payload.outputMeshPath.empty())
        {
            meshBinaryPath = payload.outputMeshPath;
            reservedPaths.insert(ToLowerAsciiCopy(meshBinaryPath.lexically_normal().generic_string()));
        }
        else if (allowOverwrite)
        {
            meshBinaryPath = BuildUniqueAssetPath(outputRootDirectory, cleanMeshStem, staticMeshBinExt, nullptr, &reservedPaths, false);
        }
        else
        {
            meshBinaryPath = BuildUniqueAssetPath(outputRootDirectory, cleanMeshStem, staticMeshBinExt, assetManager, &reservedPaths, true);
        }
        const std::filesystem::path meshRelativePath = project->GetProjectFilepath(meshBinaryPath);

        // Fast path: use cached binaries generated in dedicated folders when overwriting/reusing in-place.
        if (!payload.forceRebuild && allowOverwrite && std::filesystem::exists(meshBinaryPath))
        {
            asset = StaticMesh::Deserialize(meshBinaryPath);
            if (!asset)
            {
                return nullptr;
            }

            asset->handle = handle;
            asset->SetSourcePath(Asset::ReadSourcePathFromMetaFile(meshBinaryPath));
            prepareMeshGpuAndMaterials(asset);
            asset->CalculateLocalAABB();
            return asset;
        }

        // Load from .fbx/.gltf/.glb
        MeshScene<VertexMeshStatic> meshScene;
        MeshLoader::LoadSceneGraph(metadata.filepath.generic_string(), meshScene, assetManager);

        std::error_code dirEc;
        if (!std::filesystem::exists(outputRootDirectory, dirEc))
            std::filesystem::create_directories(outputRootDirectory, dirEc);

        // Import textures and materials from the same parse used for geometry,
        // so material handles and mesh references stay consistent.
        if (payload.importMaterials)
        {
            LOG_INFO("[Asset Importer] Importing materials and textures for StaticMesh: {} materials to process", meshScene.materials.size());
            ImportMeshMaterialsAndTextures(meshScene, outputRootDirectory, materialBinExt, allowOverwrite, payload, assetManager, project.get(), reservedPaths);
        }

        asset = StaticMesh::Create();
        for (size_t meshIdx = 0; meshIdx < meshScene.flatMeshes.size(); ++meshIdx)
        {
            Ref<StaticMeshInstance> m = meshScene.flatMeshes[meshIdx];
            m->global = glm::mat4(1.0f);

            if (payload.importMaterials && meshScene.materialMap.contains(static_cast<int>(meshIdx)))
            {
                const int matIdx = meshScene.materialMap[static_cast<int>(meshIdx)];
                if (matIdx >= 0 && matIdx < static_cast<int>(meshScene.materials.size()))
                {
                    Ref<Material> mat = meshScene.materials[matIdx];
                    m->SetMaterial(mat->handle);
                }
            }
            asset->AddMeshInstance(m);
        }

        asset->handle = handle;
        asset->SetSourcePath(resolveTrackedSourcePath(metadata.filepath));

        prepareMeshGpuAndMaterials(asset);
        assetManager->SuppressWatchPath(meshBinaryPath);
        asset->Serialize(meshBinaryPath);
        asset->CalculateLocalAABB();

        const std::filesystem::path metaPath = Asset::GetMetaFilePath(meshBinaryPath);
        assetManager->SuppressWatchPath(metaPath);
        asset->SerializeMetaFile(metaPath, [&payload](Serializer &sr)
        {
            sr.AddKeyValue("ImportMaterials", payload.importMaterials);
        });

        if (Ref<Asset> existingAsset = assetManager->TryGetAsset(handle))
        {
            if (Ref<StaticMesh> existingMesh = existingAsset->As<StaticMesh>())
            {
                auto newInstances = asset->GetMeshInstances();
                const std::filesystem::path newSourcePath = asset->GetSourcePath();
                Application::SubmitToRenderThread([assetManager, existingMesh, newInstances = std::move(newInstances), newSourcePath]() mutable
                {
                    auto oldInstances = std::make_shared<std::vector<Ref<StaticMeshInstance>>>(existingMesh->GetMeshInstances());
                    existingMesh->SetMeshInstances(newInstances);
                    existingMesh->SetSourcePath(newSourcePath);
                    existingMesh->CalculateLocalAABB();
                    existingMesh->SetReadyFlag(true);
                    assetManager->DeferReleaseResource(std::move(oldInstances));
                }, "AssetImporter::UpdateStaticMesh");
                asset = existingMesh;
            }
            else
            {
                assetManager->AssignAsset(handle, asset);
            }
        }
        else
        {
            assetManager->AssignAsset(handle, asset);
        }

        assetManager->AssignMetaData(handle, { meshRelativePath, AssetType::StaticMesh });
        assetManager->TrackMeshSourceAsset(handle);

        return asset;
    }

    Ref<Skeleton> AssetImporter::ImportSkeleton(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<Skeleton> asset = Skeleton::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<SkeletalAnimation> AssetImporter::ImportSkeletalAnimation(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<SkeletalAnimation> asset = SkeletalAnimation::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetSkeletonHandle(asset->GetSkeletonHandle());
        }
        return asset;
    }

    Ref<Animation2D> AssetImporter::ImportAnimation2D(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<Animation2D> asset = Animation2D::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
            asset->SetDirtyFlag(false);
        }
        return asset;
    }

    Ref<AnimatorController2D> AssetImporter::ImportAnimatorController2D(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<AnimatorController2D> asset = AnimatorController2D::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
            asset->SetDirtyFlag(false);
        }
        return asset;
    }

    Ref<AnimatorController> AssetImporter::ImportAnimatorController(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<AnimatorController> asset = AnimatorController::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetSkeletonHandle(asset->GetSkeletonHandle());
            asset->SetReadyFlag(true);
            asset->SetDirtyFlag(false);
        }
        return asset;
    }

    Ref<Material> AssetImporter::ImportMaterial(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<Material> asset = Material::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<Material2D> AssetImporter::ImportMaterial2D(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<Material2D> asset = Material2D::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<ScriptableObject> AssetImporter::ImportScriptableObject(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("[AssetImporter] ScriptableObject file does not exist: {}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<ScriptableObject> so = ScriptableObject::Deserialize(metadata.filepath);
        if (so)
        {
            so->handle = handle;
            so->SetReadyFlag(true);
            so->SetDirtyFlag(false);
        }
        return so;
    }

    Ref<lua::Script> AssetImporter::ImportLuaScript(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("[AssetImporter] LuaScript file does not exist: {}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<lua::Script> script = CreateRef<lua::Script>(metadata.filepath);
        if (script)
        {
            script->handle = handle;
            script->SetReadyFlag(true);
            script->SetDirtyFlag(false);
        }
        return script;
    }

    Ref<Scene> AssetImporter::ImportScene(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<Scene> scene;
        if (auto project = assetManager->LockActiveProject())
        {
            scene = SceneSerializer::Deserialize(metadata.filepath, project.get());
        }

        if (scene)
        {
            scene->handle = handle;
            scene->SetReadyFlag(true);
        }
        return scene;
    }

    Ref<Texture> AssetImporter::ImportTexture(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        TextureCreateInfo createInfo;
        const std::string extension = metadata.filepath.extension().string();
        const bool isHDR = extension == ".hdr" || extension == ".exr";

        createInfo.format = isHDR ? nri::Format::RGBA32_SFLOAT : nri::Format::RGBA8_UNORM;
        createInfo.mipLevels = isHDR ? 1 : 4;
        createInfo.flip = isHDR;
        createInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfo.keepInitialState = true; // should keep initial state
        createInfo.deferGpuCreate = true;

        return ImportTexture(handle, metadata, createInfo, assetManager);
    }

    Ref<Texture> AssetImporter::ImportTexture(AssetHandle handle, const AssetMetaData &metadata, const TextureCreateInfo &createInfo, AssetManager *assetManager)
    {
        TextureCreateInfo importCreateInfo = createInfo;

        if (importCreateInfo.format == nri::Format::UNKNOWN)
        {
            importCreateInfo.format = nri::Format::RGBA8_UNORM;
        }

        if (importCreateInfo.mipLevels == 0)
        {
            importCreateInfo.mipLevels = 1;
        }

        if (importCreateInfo.initialLayout == nri::Layout::UNDEFINED)
        {
            importCreateInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
        }

        // Load texture pixel data on worker thread (no GPU operations)
        Ref<Texture> result = Texture::Create(metadata.filepath, importCreateInfo, nullptr);
        if (!result)
        {
            return nullptr;
        }

        result->handle = handle;
        result->PrepareUploadData(4);

        Texture::SubmitAsyncUpload(result, [texture = result]()
        {
            texture->NotifyChange();
        });

        return result;
    }

    Ref<FmodSound> AssetImporter::ImportAudio(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<FmodSound> sound = FmodSound::Create(metadata.filepath.filename().string(), metadata.filepath.generic_string(), FMOD_DEFAULT);
        if (sound)
        {
            sound->handle = handle;
            sound->SetReadyFlag(true);
            sound->SetDirtyFlag(false);
        }
        return sound;
    }

    Ref<Prefab> AssetImporter::ImportPrefab(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<Project> project = assetManager ? assetManager->LockActiveProject() : nullptr;
        Ref<Prefab> prefab = Prefab::Deserialize(metadata.filepath, project.get());
        if (prefab)
        {
            prefab->handle = handle;
            prefab->SetReadyFlag(true);
        }
        return prefab;
    }

    Ref<TerrainData> AssetImporter::ImportTerrain(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<TerrainData> terrain = TerrainData::Deserialize(metadata.filepath);
        if (terrain)
        {
            terrain->handle = handle;
            terrain->SetReadyFlag(true);
        }
        return terrain;
    }
}
