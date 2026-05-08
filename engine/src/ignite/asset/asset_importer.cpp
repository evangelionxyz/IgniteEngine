#include "asset_importer.hpp"

#include "ignite/core/device/device_manager.hpp"

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

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <limits>

namespace ignite
{
    static std::unordered_map<AssetType, std::function<Ref<Asset>(AssetHandle, const AssetMetaData &, AssetManager *)>> s_ImportFunctions =
    {
        { AssetType::Scene, AssetImporter::ImportScene },
        { AssetType::Texture, [](AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager) { return AssetImporter::ImportTexture(handle, metadata, assetManager); } },
        { AssetType::Audio, AssetImporter::ImportAudio },
        { AssetType::Mesh, [](AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager) { return AssetImporter::ImportMesh(handle, metadata, assetManager); } },
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
    };

    Ref<Asset> AssetImporter::Import(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        // should be always importing with full filepath
        AssetMetaData metadataCopy = metadata;
        metadataCopy.filepath = assetManager->GetProject()->GetProjectFilepath(metadata.filepath);

        if (s_ImportFunctions.contains(metadataCopy.type))
        {
            return s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy, assetManager);
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
        if (!assetManager || !assetManager->GetProject())
        {
            return nullptr;
        }

        std::filesystem::path fontFilepath = metadata.filepath;
        if (!std::filesystem::exists(fontFilepath))
        {
            fontFilepath = assetManager->GetProject()->GetProjectFilepath(metadata.filepath);
        }

        if (!std::filesystem::exists(fontFilepath))
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
        AssetWorker::SubmitJob([handle, metadata, assetManager, callback]()
        {
            // should be always importing with full filepath
            AssetMetaData metadataCopy = metadata;
            metadataCopy.filepath = assetManager->GetProject()->GetProjectFilepath(metadata.filepath);

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

    Ref<Mesh> AssetImporter::ImportMesh(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, const MeshImportOptions &options)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        const auto skeletalMeshBinExt = GetAssetExtensionFromType(AssetType::Mesh);
        const auto skeletonBinExt = GetAssetExtensionFromType(AssetType::Skeleton);
        const auto animationBinExt = GetAssetExtensionFromType(AssetType::SkeletalAnimation);
        const auto materialBinExt = GetAssetExtensionFromType(AssetType::Material);

        const std::filesystem::path projectAssetPath = assetManager->GetProject()->GetAssetDirectory();
        const std::filesystem::path outputRootDirectory = options.targetDirectory.empty() ? projectAssetPath : options.targetDirectory;
        const std::filesystem::path filename = metadata.filepath.stem();
        const std::filesystem::path meshBinaryPath = outputRootDirectory / (filename.string() + skeletalMeshBinExt);
        const std::filesystem::path skeletonPath = outputRootDirectory / (filename.string() + skeletonBinExt);

        Ref<Mesh> asset;

        auto prepareMeshGpuAndMaterials = [assetManager](const Ref<Mesh> &mesh)
        {
            if (!mesh)
            {
                return;
            }

            for (auto &mesh : mesh->GetMeshInstances())
            {
                AssetHandle materialHandle = mesh->GetMaterialHandle();
                AssetMetaData materialMetadata = assetManager->GetMetaData(materialHandle);
                if (materialMetadata.type == AssetType::Material)
                {
                    const auto &materialFilepath = assetManager->GetProject()->GetProjectFilepath(materialMetadata.filepath);
                    Ref<Material> material = Material::Deserialize(materialFilepath);
                    assetManager->AssignAsset(materialHandle, material);
                }
            }
        };

        if (metadata.filepath.extension() == skeletalMeshBinExt)
        {
            asset = Mesh::Deserialize(metadata.filepath);
            if (asset)
            {
                asset->handle = handle;
                prepareMeshGpuAndMaterials(asset);
                asset->aabb = AABB::CalculateMeshAABB(asset->GetMeshInstances());
            }
            return asset;
        }

        // Fast path: use cached binaries generated in dedicated folders.
        if (!options.forceRebuild && std::filesystem::exists(meshBinaryPath) && std::filesystem::exists(skeletonPath))
        {
            asset = Mesh::Deserialize(meshBinaryPath);
            if (!asset)
            {
                return nullptr;
            }

            // skeleton
            AssetMetaData skeletonMD;
            skeletonMD.filepath = assetManager->GetProject()->GetProjectFilepath(skeletonPath);
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
                for (const auto &entry : std::filesystem::directory_iterator(outputRootDirectory))
                {
                    if (entry.is_regular_file() && entry.path().extension() == animationBinExt)
                    {
                        animationFiles.push_back(entry.path());
                    }
                }
            }

            std::sort(animationFiles.begin(), animationFiles.end());

            for (const auto &animationPath : animationFiles)
            {
                AssetMetaData animationMD;
                animationMD.filepath = assetManager->GetProject()->GetProjectFilepath(animationPath);
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
            return asset;
        }

        // Load from .FBX
        MeshScene meshScene;
        MeshLoader::LoadSceneGraph(metadata.filepath.generic_string(), meshScene, assetManager);

        if (!std::filesystem::exists(outputRootDirectory)) std::filesystem::create_directory(outputRootDirectory);

        // Import textures and materials from the same FBX parse used for skeleton/animation,
        // so bone IDs and skeleton mapping stay consistent.
        std::vector<std::array<AssetHandle, 5>> materialTextureHandles;
        if (options.importMaterials)
        {
            materialTextureHandles.resize(meshScene.materials.size());

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

                    const bool writeEXR = texture->GetFormat() == nvrhi::Format::RGBA32_FLOAT;
                    const std::string textureExtension = writeEXR ? ".exr" : ".png";
                    const std::filesystem::path textureOutputFullPath = outputRootDirectory / (name + textureExtension);

                    if (writeEXR)
                    {
                        BinarySerializer::SerializeTextureToEXR(texture, textureOutputFullPath);
                    }
                    else
                    {
                        BinarySerializer::SerializeTextureToPNG(texture, textureOutputFullPath);
                    }

                    const auto relativeTexturePath = assetManager->GetProject()->GetProjectFilepath(textureOutputFullPath);

                    AssetHandle textureHandle = assetManager->GetAssetHandle(relativeTexturePath);
                    if (textureHandle == AssetHandle(0))
                    {
                        textureHandle = AssetHandle();
                    }
                    texture->handle = textureHandle;

                    AssetMetaData textureMD;
                    textureMD.filepath = relativeTexturePath;
                    textureMD.type = AssetType::Texture;

                    assetManager->AssignAsset(textureHandle, texture);
                    assetManager->AssignMetaData(textureHandle, textureMD);
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
                const std::filesystem::path materialBinFullPath = outputRootDirectory / materialFilename;

                // Serialize Material
                mat->Serialize(materialBinFullPath);

                const auto relativeMaterialPath = assetManager->GetProject()->GetProjectFilepath(materialBinFullPath);

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

        asset = Mesh::Create();
        if (options.importMesh)
        {
            for (size_t meshIdx = 0; meshIdx < meshScene.flatMeshes.size(); ++meshIdx)
            {
                Ref<MeshInstance> m = meshScene.flatMeshes[meshIdx];
                m->global = glm::mat4(1.0f);

                if (options.importMaterials)
                {
                    const int matIdx = meshScene.materialMap[(int)meshIdx];
                    Ref<Material> mat = meshScene.materials[matIdx];
                    m->SetMaterial(mat->handle);
                }
                asset->AddMeshInstance(m);
            }
            asset->aabb = meshScene.aabb;
        }

        // Skeleton
        if (options.importSkeleton && meshScene.skeleton)
        {
            // Serialize skeleton
            meshScene.skeleton->Serialize(skeletonPath);

            AssetHandle skeletonHandle = AssetHandle();
            meshScene.skeleton->handle = skeletonHandle;

            AssetMetaData skeletonMD;
            skeletonMD.filepath = assetManager->GetProject()->GetProjectFilepath(skeletonPath);
            skeletonMD.type = AssetType::Skeleton;

            assetManager->AssignAsset(skeletonHandle, meshScene.skeleton);
            assetManager->AssignMetaData(skeletonHandle, skeletonMD);

            asset->SetSkeleton(skeletonHandle);
        }

        // Animations
        if (options.importAnimations)
        {
            for (size_t i = 0; i < meshScene.animations.size(); ++i)
            {
                Ref<SkeletalAnimation> animation = meshScene.animations[i];
                if (!animation)
                {
                    continue;
                }

                animation->SetSkeletonHandle(meshScene.skeleton ? meshScene.skeleton->handle : AssetHandle(0));

                std::filesystem::path animationPath = outputRootDirectory / (animation->name + animationBinExt);
                animation->Serialize(animationPath);

                AssetHandle animationHandle = AssetHandle();
                animation->handle = animationHandle;

                AssetMetaData animationMD;
                animationMD.filepath = assetManager->GetProject()->GetProjectFilepath(animationPath);
                animationMD.type = AssetType::SkeletalAnimation;

                assetManager->AssignAsset(animationHandle, animation);
                assetManager->AssignMetaData(animationHandle, animationMD);
            }
        }

        if (options.importMesh)
        {
            asset->Serialize(meshBinaryPath);
        }

        asset->handle = handle;
        if (options.importMesh)
        {
            prepareMeshGpuAndMaterials(asset);
        }
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
			asset->SetReadyFlag(true);
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

    Ref<Scene> AssetImporter::ImportScene(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager)
    {
        Ref<Scene> scene = SceneSerializer::Deserialize(metadata.filepath, assetManager->GetProject());
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
