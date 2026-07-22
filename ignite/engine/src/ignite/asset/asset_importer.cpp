// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "asset_importer.hpp"

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
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/graphics/font.hpp"
#include "ignite/scripting/scriptable_object.hpp"

namespace ignite
{
    static std::unordered_map<AssetType, std::function<Ref<Asset>(AssetHandle, const AssetMetaData &, AssetManager *)>> s_ImportFunctions =
    {
        { AssetType::Audio, AssetImporter::ImportAudio },
        { AssetType::Scene, AssetImporter::ImportScene },
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
        if (!ignite::Path::exists(metadata.filepath))
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

        ignite::Path fontFilepath = metadata.filepath;
        if (!ignite::Path::exists(fontFilepath))
        {
            fontFilepath = project->GetProjectFilepath(metadata.filepath);
        }

        if (!ignite::Path::exists(fontFilepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<Font> font = Font::Create(fontFilepath);
        if (font)
        {
            font->handle = handle;
            font->SetDirtyFlag(false);
            font->SetReadyFlag(true);
        }

        return font;
    }

    Ref<WidgetCanvas> AssetImporter::ImportWidget(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        if (!ignite::Path::exists(metadata.filepath))
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
        if (!ignite::Path::exists(metadata.filepath))
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

        const ignite::Path projectAssetPath = project->GetAssetDirectory();
        const ignite::Path outputRootDirectory = payload.targetDirectory.empty() ? projectAssetPath : payload.targetDirectory;
        const ignite::Path filename = metadata.filepath.stem();
        const ignite::Path meshBinaryPath = outputRootDirectory / (filename.string() + skeletalMeshBinExt);
        const ignite::Path skeletonPath = outputRootDirectory / (filename.string() + skeletonBinExt);
        const ignite::Path meshRelativePath = project->GetProjectFilepath(meshBinaryPath);

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
                prepareMeshGpuAndMaterials(asset);
            }
            asset->CalculateLocalAABB();
            return asset;
        }

        // Fast path: use cached binaries generated in dedicated folders.
        if (!payload.forceRebuild && ignite::Path::exists(meshBinaryPath) && ignite::Path::exists(skeletonPath))
        {
            asset = SkeletalMesh::Deserialize(meshBinaryPath);
            if (!asset)
            {
                return nullptr;
            }

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
            std::vector<ignite::Path> animationFiles;
            if (ignite::Path::exists(outputRootDirectory))
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

        // Load from .FBX
        MeshScene<VertexMeshAnim> meshScene;
        MeshLoader::LoadSceneGraph(metadata.filepath.generic_string(), meshScene, assetManager);

        if (!ignite::Path::exists(outputRootDirectory)) ignite::Path::create_directory(outputRootDirectory);

        // Import textures and materials from the same FBX parse used for skeleton/animation,
        // so bone IDs and skeleton mapping stay consistent.
        std::vector<std::array<AssetHandle, 5>> materialTextureHandles;
        if (payload.importMaterials)
        {
            LOG_INFO("[Asset Importer] Importing materials and textures for SkeletalMesh: {} materials to process", meshScene.materials.size());
            materialTextureHandles.resize(meshScene.materials.size());

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

                    const bool writeEXR = texture->GetFormat() == nvrhi::Format::RGBA32_FLOAT;
                    const std::string textureExtension = writeEXR ? ".exr" : ".png";
                    const ignite::Path textureOutputFullPath = outputRootDirectory / (name + textureExtension);
                    const auto relativeTexturePath = project->GetProjectFilepath(textureOutputFullPath);

                    AssetHandle textureHandle = assetManager->GetAssetHandle(relativeTexturePath);
                    if (textureHandle == AssetHandle(0))
                    {
                        LOG_INFO("[Asset Importer] Serializing new texture asset: {} -> {}", name, relativeTexturePath.generic_string());
                        if (writeEXR)
                        {
                            BinarySerializer::SerializeTextureToEXR(texture, textureOutputFullPath);
                        }
                        else
                        {
                            BinarySerializer::SerializeTextureToPNG(texture, textureOutputFullPath);
                        }

                        textureHandle = AssetHandle();

                        AssetMetaData textureMD;
                        textureMD.filepath = relativeTexturePath;
                        textureMD.type = AssetType::Texture;

                        assetManager->AssignAsset(textureHandle, texture);
                        assetManager->AssignMetaData(textureHandle, textureMD);
                    }
                    else
                    {
                        LOG_INFO("[Asset Importer] Overwriting existing texture asset: {} -> {} (Handle: {})", name, relativeTexturePath.generic_string(), static_cast<uint64_t>(textureHandle));
                        if (writeEXR)
                        {
                            BinarySerializer::SerializeTextureToEXR(texture, textureOutputFullPath);
                        }
                        else
                        {
                            BinarySerializer::SerializeTextureToPNG(texture, textureOutputFullPath);
                        }
                        assetManager->AssignAsset(textureHandle, texture);
                    }

                    texture->handle = textureHandle;
                    serializedTextures[texture] = textureHandle;
                    textureHandles[j] = textureHandle;
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

                const std::string materialFilename = mat->name + materialBinExt;
                const ignite::Path materialBinFullPath = outputRootDirectory / materialFilename;

                // Serialize Material
                mat->Serialize(materialBinFullPath);

                const auto relativeMaterialPath = project->GetProjectFilepath(materialBinFullPath);

                AssetHandle materialHandle = assetManager->GetAssetHandle(relativeMaterialPath);
                if (materialHandle == AssetHandle(0))
                {
                    materialHandle = AssetHandle();
                }
                mat->handle = materialHandle;

                AssetMetaData materialMD;
                materialMD.filepath = relativeMaterialPath;
                materialMD.type = AssetType::Material;

                assetManager->AssignAsset(materialHandle, mat);
                assetManager->AssignMetaData(materialHandle, materialMD);
            }
        }

        asset = SkeletalMesh::Create();
        if (payload.importMesh)
        {
            for (size_t meshIdx = 0; meshIdx < meshScene.flatMeshes.size(); ++meshIdx)
            {
                Ref<SkeletalMeshInstance> m = meshScene.flatMeshes[meshIdx];
                m->global = glm::mat4(1.0f);

                if (payload.importMaterials)
                {
                    const int matIdx = meshScene.materialMap[(int)meshIdx];
                    Ref<Material> mat = meshScene.materials[matIdx];
                    m->SetMaterial(mat->handle);
                }
                asset->AddMeshInstance(m);
            }
        }

        // Skeleton
        if (payload.importSkeleton && meshScene.skeleton)
        {
            // Serialize skeleton
            meshScene.skeleton->Serialize(skeletonPath);

            AssetHandle skeletonHandle = AssetHandle();
            meshScene.skeleton->handle = skeletonHandle;

            AssetMetaData skeletonMD;
            skeletonMD.filepath = project->GetProjectFilepath(skeletonPath);
            skeletonMD.type = AssetType::Skeleton;

            assetManager->AssignAsset(skeletonHandle, meshScene.skeleton);
            assetManager->AssignMetaData(skeletonHandle, skeletonMD);

            asset->SetSkeleton(skeletonHandle);
        }

        // Animations
        if (payload.importAnimations)
        {
            for (size_t i = 0; i < meshScene.animations.size(); ++i)
            {
                Ref<SkeletalAnimation> animation = meshScene.animations[i];
                if (!animation)
                {
                    continue;
                }

                animation->SetSkeletonHandle(meshScene.skeleton ? meshScene.skeleton->handle : AssetHandle(0));

                ignite::Path animationPath = outputRootDirectory / (animation->name + animationBinExt);
                animation->Serialize(animationPath);

                AssetHandle animationHandle = AssetHandle();
                animation->handle = animationHandle;

                AssetMetaData animationMD;
                animationMD.filepath = project->GetProjectFilepath(animationPath);
                animationMD.type = AssetType::SkeletalAnimation;

                assetManager->AssignAsset(animationHandle, animation);
                assetManager->AssignMetaData(animationHandle, animationMD);
            }
        }

        asset->handle = handle;

        if (payload.importMesh)
        {
            prepareMeshGpuAndMaterials(asset);
            asset->Serialize(meshBinaryPath);
            asset->CalculateLocalAABB();

            assetManager->AssignAsset(handle, asset);
            assetManager->AssignMetaData(handle, { meshRelativePath, AssetType::SkeletalMesh });
        }

        return asset;
    }

    Ref<StaticMesh> AssetImporter::ImportStaticMesh(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, const StaticMeshImportPayload &payload)
    {
        if (!ignite::Path::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        const auto staticMeshBinExt = GetAssetExtensionFromType(AssetType::StaticMesh);
        const auto materialBinExt = GetAssetExtensionFromType(AssetType::Material);

		auto project = assetManager->LockActiveProject();
        if (!project)
            return nullptr;

        const ignite::Path projectAssetPath = project->GetAssetDirectory();
        const ignite::Path outputRootDirectory = payload.targetDirectory.empty() ? projectAssetPath : payload.targetDirectory;
        const ignite::Path filename = metadata.filepath.stem();
        const ignite::Path meshBinaryPath = outputRootDirectory / (filename.string() + staticMeshBinExt);
        const ignite::Path meshRelativePath = project->GetProjectFilepath(meshBinaryPath);

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
                prepareMeshGpuAndMaterials(asset);
            }
            asset->CalculateLocalAABB();
            return asset;
        }

        // Fast path: use cached binaries generated in dedicated folders.
        if (!payload.forceRebuild && ignite::Path::exists(meshBinaryPath))
        {
            asset = StaticMesh::Deserialize(meshBinaryPath);
            if (!asset)
            {
                return nullptr;
            }

            asset->handle = handle;
            prepareMeshGpuAndMaterials(asset);
            asset->CalculateLocalAABB();
            return asset;
        }

        // Load from .fbx/.gltf/.glb
        MeshScene<VertexMeshStatic> meshScene;
        MeshLoader::LoadSceneGraph(metadata.filepath.generic_string(), meshScene, assetManager);

        if (!ignite::Path::exists(outputRootDirectory)) ignite::Path::create_directory(outputRootDirectory);

        // Import textures and materials from the same FBX parse used for skeleton/animation,
        // so bone IDs and skeleton mapping stay consistent.
        std::vector<std::array<AssetHandle, 5>> materialTextureHandles;
        if (payload.importMaterials)
        {
            LOG_INFO("[Asset Importer] Importing materials and textures for StaticMesh: {} materials to process", meshScene.materials.size());
            materialTextureHandles.resize(meshScene.materials.size());

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

                    const bool writeEXR = texture->GetFormat() == nvrhi::Format::RGBA32_FLOAT;
                    const std::string textureExtension = writeEXR ? ".exr" : ".png";
                    const ignite::Path textureOutputFullPath = outputRootDirectory / (name + textureExtension);
                    const auto relativeTexturePath = project->GetProjectFilepath(textureOutputFullPath);

                    AssetHandle textureHandle = assetManager->GetAssetHandle(relativeTexturePath);
                    if (textureHandle == AssetHandle(0))
                    {
                        LOG_INFO("[Asset Importer] Serializing new texture asset: {} -> {}", name, relativeTexturePath.generic_string());
                        if (writeEXR)
                        {
                            BinarySerializer::SerializeTextureToEXR(texture, textureOutputFullPath);
                        }
                        else
                        {
                            BinarySerializer::SerializeTextureToPNG(texture, textureOutputFullPath);
                        }

                        textureHandle = AssetHandle();

                        AssetMetaData textureMD;
                        textureMD.filepath = relativeTexturePath;
                        textureMD.type = AssetType::Texture;

                        assetManager->AssignAsset(textureHandle, texture);
                        assetManager->AssignMetaData(textureHandle, textureMD);
                    }
                    else
                    {
                        LOG_INFO("[Asset Importer] Overwriting existing texture asset: {} -> {} (Handle: {})", name, relativeTexturePath.generic_string(), static_cast<uint64_t>(textureHandle));
                        if (writeEXR)
                        {
                            BinarySerializer::SerializeTextureToEXR(texture, textureOutputFullPath);
                        }
                        else
                        {
                            BinarySerializer::SerializeTextureToPNG(texture, textureOutputFullPath);
                        }
                        assetManager->AssignAsset(textureHandle, texture);
                    }

                    texture->handle = textureHandle;
                    serializedTextures[texture] = textureHandle;
                    textureHandles[j] = textureHandle;
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

                const std::string materialFilename = mat->name + materialBinExt;
                const ignite::Path materialBinFullPath = outputRootDirectory / materialFilename;

                // Serialize Material
                mat->Serialize(materialBinFullPath);

                const auto relativeMaterialPath = project->GetProjectFilepath(materialBinFullPath);

                AssetHandle materialHandle = assetManager->GetAssetHandle(relativeMaterialPath);
                if (materialHandle == AssetHandle(0))
                {
                    materialHandle = AssetHandle();
                }
                mat->handle = materialHandle;

                AssetMetaData materialMD;
                materialMD.filepath = relativeMaterialPath;
                materialMD.type = AssetType::Material;

                assetManager->AssignAsset(materialHandle, mat);
                assetManager->AssignMetaData(materialHandle, materialMD);
            }
        }

        asset = StaticMesh::Create();
        for (size_t meshIdx = 0; meshIdx < meshScene.flatMeshes.size(); ++meshIdx)
        {
            Ref<StaticMeshInstance> m = meshScene.flatMeshes[meshIdx];
            m->global = glm::mat4(1.0f);

            if (payload.importMaterials)
            {
                const int matIdx = meshScene.materialMap[(int)meshIdx];
                Ref<Material> mat = meshScene.materials[matIdx];
                m->SetMaterial(mat->handle);
            }
            asset->AddMeshInstance(m);
        }

        prepareMeshGpuAndMaterials(asset);
        asset->Serialize(meshBinaryPath);
        asset->CalculateLocalAABB();

        assetManager->AssignAsset(handle, asset);
        assetManager->AssignMetaData(handle, { meshRelativePath, AssetType::StaticMesh });

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
        if (!ignite::Path::exists(metadata.filepath))
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
        if (!ignite::Path::exists(metadata.filepath))
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
        if (!ignite::Path::exists(metadata.filepath))
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
        if (!ignite::Path::exists(metadata.filepath))
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

        createInfo.format = isHDR ? nvrhi::Format::RGBA32_FLOAT : nvrhi::Format::RGBA8_UNORM;
        createInfo.mipLevels = isHDR ? 1 : 4;
        createInfo.flip = isHDR;
        createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfo.keepInitialState = true; // should keep initial state
        createInfo.deferGpuCreate = true;

        return ImportTexture(handle, metadata, createInfo, assetManager);
    }

    Ref<Texture> AssetImporter::ImportTexture(AssetHandle handle, const AssetMetaData &metadata, const TextureCreateInfo &createInfo, AssetManager *assetManager)
    {
        TextureCreateInfo importCreateInfo = createInfo;

        if (importCreateInfo.format == nvrhi::Format::UNKNOWN)
        {
            importCreateInfo.format = nvrhi::Format::RGBA8_UNORM;
        }

        if (importCreateInfo.mipLevels == 0)
        {
            importCreateInfo.mipLevels = 1;
        }

        if (importCreateInfo.initialState == nvrhi::ResourceStates::Unknown)
        {
            importCreateInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        }

        // Load texture pixel data on worker thread (no GPU operations)
        Ref<Texture> result = Texture::Create(metadata.filepath, importCreateInfo, nullptr);
        if (!result)
        {
            return nullptr;
        }

        result->handle = handle;
        result->PrepareUploadData(4);

        // Submit GPU upload to main thread with proper synchronization
        Application::SubmitToRenderThread([texture = result]()
        {
            nvrhi::CommandListHandle cmd = DeviceManager::GetInstance()->GetDevice()->createCommandList();
            cmd->open();
            texture->SetData(cmd, 4);
            texture->SetReadyFlag(false);
            cmd->close();

            Application::SubmitWorkerCommandList(cmd, [texture]()
            {
                texture->SetReadyFlag(true);
                texture->NotifyChange();
            });
        });

        return result;
    }

    Ref<FmodSound> AssetImporter::ImportAudio(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<FmodSound> sound = FmodSound::Create(metadata.filepath.filename().string(), metadata.filepath.generic_string(), FMOD_DEFAULT);
        if (sound)
        {
            sound->handle = handle;
        }
        return sound;
    }
}
