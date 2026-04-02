#include "asset_importer.hpp"

#include "ignite/audio/fmod_audio.hpp"
#include "ignite/audio/fmod_sound.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animator_controller_2d.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/graphics/font.hpp"

#include <mutex>
#include <condition_variable>
#include <chrono>

namespace ignite {

    static std::unordered_map<AssetType, std::function<Ref<Asset>(AssetHandle, const AssetMetaData &)>> s_ImportFunctions =
    {
        { AssetType::Scene, AssetImporter::ImportScene },
        { AssetType::Texture, [](AssetHandle handle, const AssetMetaData &metadata) { return AssetImporter::ImportTexture(handle, metadata); } },
        { AssetType::Audio, AssetImporter::ImportAudio },
        { AssetType::StaticMesh, AssetImporter::ImportStaticMesh },
        { AssetType::SkeletalMesh, AssetImporter::ImportSkeletalMesh },
        { AssetType::Material, AssetImporter::ImportMaterial },
        { AssetType::Material2D, AssetImporter::ImportMaterial2D },
        { AssetType::SpriteSheet, AssetImporter::ImportSpriteSheet },
        { AssetType::Font, AssetImporter::ImportFont },
        { AssetType::Skeleton, AssetImporter::ImportSkeleton },
        { AssetType::SkeletalAnimation, AssetImporter::ImportSkeletalAnimation },
        { AssetType::Animation2D, AssetImporter::ImportAnimation2D },
        { AssetType::AnimatorController2D, AssetImporter::ImportAnimatorController2D },
    };

    Ref<Asset> AssetImporter::Import(AssetHandle handle, const AssetMetaData &metadata)
    {
        // should be always importing with full filepath
        AssetMetaData metadataCopy = metadata;
        metadataCopy.filepath = AssetManager::GetProject()->GetAssetFilepath(metadata.filepath);

        if (s_ImportFunctions.contains(metadataCopy.type))
        {
            return s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy);
        }
        return nullptr;
	}


    Ref<SpriteSheet> AssetImporter::ImportSpriteSheet(AssetHandle handle, const AssetMetaData &metadata)
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

    Ref<Font> AssetImporter::ImportFont(AssetHandle handle, const AssetMetaData &metadata)
    {
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

        Ref<Font> font = Font::Create(metadata.filepath);
        if (font)
        {
            font->handle = handle;
            font->SetDirtyFlag(false);
            font->SetReadyFlag(true);
        }

        return font;
    }

    void AssetImporter::ImportAsync(AssetHandle handle, const AssetMetaData &metadata, std::function<void(Ref<Asset>, AssetHandle)> callback)
    {
        AssetManager::GetProject()->GetAssetManager().SubmitJob([handle, metadata, callback]()
        {
            // should be always importing with full filepath
            AssetMetaData metadataCopy = metadata;
            metadataCopy.filepath = AssetManager::GetProject()->GetAssetFilepath(metadata.filepath);

            Ref<Asset> asset;
            if (s_ImportFunctions.contains(metadataCopy.type))
            {
                asset = s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy);
            }
            
            if (asset)
            {
                callback(asset, handle);
            }
        });
    }

	Ref<StaticMesh> AssetImporter::ImportStaticMesh(AssetHandle handle, const AssetMetaData &metadata)
	{
        if (!std::filesystem::exists(metadata.filepath))
        {
            LOG_ERROR("File does not exists {0}", metadata.filepath.generic_string());
            return nullptr;
        }

		static auto staticMeshBinExt = GetAssetExtensionFromType(AssetType::StaticMesh);
		static auto materialExt = GetAssetExtensionFromType(AssetType::Material);

		Ref<StaticMesh> asset;

		// Load the mesh from .ixsm
        if (metadata.filepath.extension() == staticMeshBinExt && false)
        {
		    asset = BinarySerializer::DeserializeStaticMesh(metadata.filepath);
        }

        if (asset && false)
        {
			for (auto &mesh : asset->GetMeshInstances())
			{
			    // Load materials
				AssetHandle materialHandle = mesh->GetMaterialHandle();
				AssetMetaData metadata = Project::GetInstance()->GetAssetManager().GetMetaData(materialHandle);
				if (metadata.type == AssetType::Material)
				{
					const auto &materialFilepath = Project::GetInstance()->GetAssetFilepath(metadata.filepath);
					Ref<Material> material = BinarySerializer::DeserializeMaterial(materialFilepath);
                    Project::GetInstance()->GetAssetManager().AssignAsset(materialHandle, material);

                    // Submit GPU upload command list to render thread (thread-safe)
                    Application::SubmitToRenderThread([m = mesh]()
                    {
                        nvrhi::IDevice* device = DeviceManager::GetInstance()->GetDevice();
                        nvrhi::CommandListHandle cmd = device->createCommandList();
                        cmd->open();
                        m->GetPrimitive()->CreateBuffer(cmd);
                        cmd->close();

                        Application::SubmitWorkerCommandList(cmd);
                    });
                }
            }

            return asset;
        }

        const std::filesystem::path parentPath = metadata.filepath.parent_path();

        // Get project asset directory
        const std::filesystem::path projectAssetPath = Project::GetInstance()->GetAssetDirectory();

        const std::filesystem::path filename = metadata.filepath.stem();
        const std::filesystem::path outputDirectory = projectAssetPath / filename; // inside project asset directory
        const std::filesystem::path meshDirectory = outputDirectory / "StaticMesh";
        const std::filesystem::path materialDirectory = outputDirectory / "Material";
        const std::filesystem::path textureDirectory = outputDirectory / "Textures";

        if (!std::filesystem::exists(outputDirectory))
            std::filesystem::create_directory(outputDirectory);

        if (!std::filesystem::exists(meshDirectory))
            std::filesystem::create_directory(meshDirectory);

        if (!std::filesystem::exists(materialDirectory))
            std::filesystem::create_directory(materialDirectory);

        if (!std::filesystem::exists(textureDirectory)) 
            std::filesystem::create_directory(textureDirectory);

        std::filesystem::path meshBinaryFilename = filename;
        meshBinaryFilename = meshBinaryFilename.replace_extension(staticMeshBinExt);
        std::filesystem::path meshBinaryFullpath = meshDirectory / meshBinaryFilename;

        if (!asset)
        {
            // Generate folders
            MeshScene meshScene;
            MeshLoader::LoadSceneGraph(metadata.filepath.generic_string(), meshScene);

            // Prepare AssetHandle map for texture material textures
            // we need 5 textures
            std::vector<std::array<AssetHandle, 5>> materialTextureHandles;
            materialTextureHandles.resize(meshScene.materials.size());

            // Import and store textures
            for (size_t i = 0; i < meshScene.materialTextureMap.size(); ++i)
            {
                auto &textureHandles = materialTextureHandles[i];

                // Set the default value to be 0
                std::fill(textureHandles.begin(), textureHandles.end(), AssetHandle(0));

                for (size_t j = 0; j < meshScene.materialTextureMap[i].size(); ++j)
                {
                    auto &[idx, texture] = meshScene.materialTextureMap[i][j];

                    // no texture
                    if (idx < 0)
                        continue;

                    const std::string textureFilename = filename.stem().string() + std::format("_{0}_{1}", idx, ".png");
                    std::filesystem::path texturePNGFullPath = textureDirectory / textureFilename;
                    BinarySerializer::SerializeTextureToPNG(texture, texturePNGFullPath);

                    auto &assetManager = Project::GetInstance()->GetAssetManager();
                    const auto relativeTexturePath = Project::GetInstance()->GetAssetRelativeFilepath(texturePNGFullPath);

                    AssetHandle textureHandle = assetManager.GetAssetHandle(relativeTexturePath);
                    if (textureHandle == AssetHandle(0))
                    {
                        textureHandle = AssetHandle();
                    }
                    texture->handle = textureHandle;

                    AssetMetaData textureMD;
                    textureMD.filepath = relativeTexturePath;
                    textureMD.type = AssetType::Texture;

                    assetManager.AssignAsset(textureHandle, texture);
                    assetManager.AssignMetaData(textureHandle, textureMD);

                    // If the texture has been stored, then assign AssetHandle
                    textureHandles[j] = textureHandle;
                }
            }

            // Import and store material
            for (size_t i = 0; i < meshScene.materials.size(); ++i)
            {
                Ref<Material> &mat = meshScene.materials[i];

                // First store the texture handles
                mat->baseColorTextureHandle = materialTextureHandles[i][0];
                mat->emissiveTextureHandle = materialTextureHandles[i][1];
                mat->metallicRoughnessTextureHandle = materialTextureHandles[i][2];
                mat->normalTextureHandle = materialTextureHandles[i][3];
                mat->occlusionTextureHandle = materialTextureHandles[i][4];

                std::filesystem::path materialFullPath = materialDirectory / (mat->name + materialExt);

                // 
                mat->Serialize(materialFullPath);

                auto &assetManager = Project::GetInstance()->GetAssetManager();
                const auto relativeMaterialPath = Project::GetInstance()->GetAssetRelativeFilepath(materialFullPath);

                AssetHandle materialHandle = assetManager.GetAssetHandle(relativeMaterialPath);
                if (materialHandle == AssetHandle(0))
                {
                    materialHandle = AssetHandle();
                }
                mat->handle = materialHandle; // assign material handle

                AssetMetaData materialMD;
                materialMD.filepath = relativeMaterialPath;
                materialMD.type = AssetType::Material;

                assetManager.AssignAsset(materialHandle, mat);
                assetManager.AssignMetaData(materialHandle, materialMD);
            }

            asset = CreateRef<StaticMesh>();
            for (size_t meshIdx = 0; meshIdx < meshScene.flatMeshes.size(); ++meshIdx)
            {
                // Resolve material handle
                const int matIdx = meshScene.materialMap[(int)meshIdx];
                Ref<Material> mat = meshScene.materials[matIdx];

                Ref<MeshInstance> m = meshScene.flatMeshes[meshIdx];
                m->SetMaterial(mat->handle);

                asset->AddMeshInstance(m);
            }

            // Serialize the mesh
            BinarySerializer::SerializeStaticMesh(asset, meshBinaryFullpath);
        }

        if (asset)
        {
            asset->handle = handle;

            for (auto &mesh : asset->GetMeshInstances())
            {
                // Load materials
                AssetHandle materialHandle = mesh->GetMaterialHandle();
                AssetMetaData metadata = Project::GetInstance()->GetAssetManager().GetMetaData(materialHandle);
                if (metadata.type == AssetType::Material)
                {
                    const auto &materialFilepath = Project::GetInstance()->GetAssetFilepath(metadata.filepath);
                    Ref<Material> material = Material::Deserialize(materialFilepath);
                    Project::GetInstance()->GetAssetManager().AssignAsset(materialHandle, material);

                    // Submit GPU upload command list to render thread (thread-safe)
                    Application::SubmitToRenderThread([m = mesh]()
                    {
                        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
                        nvrhi::CommandListHandle cmd = device->createCommandList();
                        cmd->open();
                        m->GetPrimitive()->CreateBuffer(cmd);
                        cmd->close();

                        Application::SubmitWorkerCommandList(cmd);
                    });
                }
            }

            auto relativePath = Project::GetInstance()->GetAssetRelativeFilepath(meshBinaryFullpath);
        }

        return asset;
    }

    Ref<SkeletalMesh> AssetImporter::ImportSkeletalMesh(AssetHandle handle, const AssetMetaData &metadata)
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

        const std::filesystem::path projectAssetPath = Project::GetInstance()->GetAssetDirectory();
        const std::filesystem::path filename = metadata.filepath.stem();
        const std::filesystem::path outputDirectory = projectAssetPath / filename;
        const std::filesystem::path skeletalMeshDirectory = outputDirectory / "SkeletalMesh";
        const std::filesystem::path animationDirectory = outputDirectory / "Animation";
        const std::filesystem::path skmBinaryPath = skeletalMeshDirectory / (filename.string() + skeletalMeshBinExt);
        const std::filesystem::path skeletonPath = skeletalMeshDirectory / (filename.string() + skeletonBinExt);

        Ref<SkeletalMesh> asset;

        auto prepareMeshGpuAndMaterials = [](const Ref<SkeletalMesh> &skeletalMesh)
        {
            if (!skeletalMesh)
            {
                return;
            }

            for (auto &mesh : skeletalMesh->GetMeshInstances())
            {
                AssetHandle materialHandle = mesh->GetMaterialHandle();
                AssetMetaData materialMetadata = Project::GetInstance()->GetAssetManager().GetMetaData(materialHandle);
                if (materialMetadata.type == AssetType::Material)
                {
                    const auto &materialFilepath = Project::GetInstance()->GetAssetFilepath(materialMetadata.filepath);
                    Ref<Material> material = Material::Deserialize(materialFilepath);
                    Project::GetInstance()->GetAssetManager().AssignAsset(materialHandle, material);
                }

                Application::SubmitToRenderThread([m = mesh]()
                {
                    nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
                    nvrhi::CommandListHandle cmd = device->createCommandList();
                    cmd->open();
                    m->GetPrimitive()->CreateBuffer(cmd);
                    cmd->close();

                    Application::SubmitWorkerCommandList(cmd);
                });
            }
        };

        if (metadata.filepath.extension() == skeletalMeshBinExt)
        {
            asset = BinarySerializer::DeserializeSkeletalMesh(metadata.filepath);
            if (asset)
            {
                asset->handle = handle;
                prepareMeshGpuAndMaterials(asset);
            }
            return asset;
        }

        // Fast path: use cached binaries generated in dedicated folders.
        if (std::filesystem::exists(skmBinaryPath) && std::filesystem::exists(skeletonPath))
        {
            asset = BinarySerializer::DeserializeSkeletalMesh(skmBinaryPath);
            if (!asset)
            {
                return nullptr;
            }

            auto &assetManager = Project::GetInstance()->GetAssetManager();

            // skeleton
            AssetMetaData skeletonMD;
            skeletonMD.filepath = Project::GetInstance()->GetAssetRelativeFilepath(skeletonPath);
            skeletonMD.type = AssetType::Skeleton;

            AssetHandle skeletonHandle = assetManager.GetAssetHandle(skeletonMD.filepath);
            if (skeletonHandle == AssetHandle(0))
            {
                skeletonHandle = AssetHandle();
            }

            Ref<Skeleton> skeletonAsset = Skeleton::Deserialize(skeletonPath);
            if (skeletonAsset)
            {
                skeletonAsset->handle = skeletonHandle;
                assetManager.AssignMetaData(skeletonHandle, skeletonMD);
                assetManager.AssignAsset(skeletonHandle, skeletonAsset);
                asset->boneTransforms.resize(skeletonAsset->joints.size(), glm::mat4(1.0f));
            }

            // animations
            asset->animationHandles.clear();
            std::vector<std::filesystem::path> animationFiles;
            if (std::filesystem::exists(animationDirectory))
            {
                for (const auto &entry : std::filesystem::directory_iterator(animationDirectory))
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
                animationMD.filepath = Project::GetInstance()->GetAssetRelativeFilepath(animationPath);
                animationMD.type = AssetType::SkeletalAnimation;

                AssetHandle animationHandle = assetManager.GetAssetHandle(animationMD.filepath);
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
                assetManager.AssignMetaData(animationHandle, animationMD);
                assetManager.AssignAsset(animationHandle, animationAsset);
                asset->animationHandles.push_back(animationHandle);
            }

            asset->handle = handle;
            prepareMeshGpuAndMaterials(asset);
            return asset;
        }

        // Load from .FBX
        MeshScene meshScene;
        MeshLoader::LoadSceneGraph(metadata.filepath.generic_string(), meshScene);

        if (!std::filesystem::exists(outputDirectory)) std::filesystem::create_directory(outputDirectory);
        if (!std::filesystem::exists(skeletalMeshDirectory)) std::filesystem::create_directory(skeletalMeshDirectory);
        if (!std::filesystem::exists(animationDirectory)) std::filesystem::create_directory(animationDirectory);

        const std::filesystem::path materialDirectory = outputDirectory / "Material";
        const std::filesystem::path textureDirectory = outputDirectory / "Textures";
        if (!std::filesystem::exists(materialDirectory)) std::filesystem::create_directory(materialDirectory);
        if (!std::filesystem::exists(textureDirectory)) std::filesystem::create_directory(textureDirectory);

        // Import textures and materials from the same FBX parse used for skeleton/animation,
        // so bone IDs and skeleton mapping stay consistent.
        std::vector<std::array<AssetHandle, 5>> materialTextureHandles;
        materialTextureHandles.resize(meshScene.materials.size());

        for (size_t i = 0; i < meshScene.materialTextureMap.size(); ++i)
        {
            auto &textureHandles = materialTextureHandles[i];
            std::fill(textureHandles.begin(), textureHandles.end(), AssetHandle(0));

            for (size_t j = 0; j < meshScene.materialTextureMap[i].size(); ++j)
            {
                auto &[idx, texture] = meshScene.materialTextureMap[i][j];
                if (idx < 0 || !texture)
                {
                    continue;
                }

                const std::string textureFilename = filename.stem().string() + std::format("_{0}_{1}", idx, ".png");
                const std::filesystem::path texturePNGFullPath = textureDirectory / textureFilename;
                BinarySerializer::SerializeTextureToPNG(texture, texturePNGFullPath);

                auto &assetManager = Project::GetInstance()->GetAssetManager();
                const auto relativeTexturePath = Project::GetInstance()->GetAssetRelativeFilepath(texturePNGFullPath);

                AssetHandle textureHandle = assetManager.GetAssetHandle(relativeTexturePath);
                if (textureHandle == AssetHandle(0))
                {
                    textureHandle = AssetHandle();
                }
                texture->handle = textureHandle;

                AssetMetaData textureMD;
                textureMD.filepath = relativeTexturePath;
                textureMD.type = AssetType::Texture;

                assetManager.AssignAsset(textureHandle, texture);
                assetManager.AssignMetaData(textureHandle, textureMD);
                textureHandles[j] = textureHandle;
            }
        }

        for (size_t i = 0; i < meshScene.materials.size(); ++i)
        {
            Ref<Material> &mat = meshScene.materials[i];
            mat->baseColorTextureHandle = materialTextureHandles[i][0];
            mat->emissiveTextureHandle = materialTextureHandles[i][1];
            mat->metallicRoughnessTextureHandle = materialTextureHandles[i][2];
            mat->normalTextureHandle = materialTextureHandles[i][3];
            mat->occlusionTextureHandle = materialTextureHandles[i][4];

            const std::string materialFilename = mat->name + materialBinExt;
            const std::filesystem::path materialBinFullPath = materialDirectory / materialFilename;
            
            // Serialize Material
            mat->Serialize(materialBinFullPath);

            auto &assetManager = Project::GetInstance()->GetAssetManager();
            const auto relativeMaterialPath = Project::GetInstance()->GetAssetRelativeFilepath(materialBinFullPath);

            AssetHandle materialHandle = assetManager.GetAssetHandle(relativeMaterialPath);
            if (materialHandle == AssetHandle(0))
            {
                materialHandle = AssetHandle();
            }
            mat->handle = materialHandle;

            AssetMetaData materialMD;
            materialMD.filepath = relativeMaterialPath;
            materialMD.type = AssetType::Material;

            assetManager.AssignAsset(materialHandle, mat);
            assetManager.AssignMetaData(materialHandle, materialMD);
        }

        asset = SkeletalMesh::Create();
        for (size_t meshIdx = 0; meshIdx < meshScene.flatMeshes.size(); ++meshIdx)
        {
            const int matIdx = meshScene.materialMap[(int)meshIdx];
            Ref<Material> mat = meshScene.materials[matIdx];

            Ref<MeshInstance> m = meshScene.flatMeshes[meshIdx];
            m->global = glm::mat4(1.0f);
            m->SetMaterial(mat->handle);
            asset->AddMeshInstance(m);
        }

        // Skeleton
        if (meshScene.skeleton)
        {
            // Serialize skeleton
            meshScene.skeleton->Serialize(skeletonPath);

            AssetHandle skeletonHandle = AssetHandle();
            meshScene.skeleton->handle = skeletonHandle;

            AssetMetaData skeletonMD;
            skeletonMD.filepath = Project::GetInstance()->GetAssetRelativeFilepath(skeletonPath);
            skeletonMD.type = AssetType::Skeleton;

            Project::GetInstance()->GetAssetManager().AssignAsset(skeletonHandle, meshScene.skeleton);
            Project::GetInstance()->GetAssetManager().AssignMetaData(skeletonHandle, skeletonMD);
            asset->boneTransforms.resize(meshScene.skeleton->joints.size(), glm::mat4(1.0f));
        }

        // Animations
        for (size_t i = 0; i < meshScene.animations.size(); ++i)
        {
            Ref<SkeletalAnimation> animation = meshScene.animations[i];
            if (!animation)
            {
                continue;
            }

            animation->SetSkeletonHandle(meshScene.skeleton ? meshScene.skeleton->handle : AssetHandle(0));

            std::filesystem::path animationPath = animationDirectory / (std::format("{}_{}", filename.string(), i) + animationBinExt);
            animation->Serialize(animationPath);

            AssetHandle animationHandle = AssetHandle();
            animation->handle = animationHandle;

            AssetMetaData animationMD;
            animationMD.filepath = Project::GetInstance()->GetAssetRelativeFilepath(animationPath);
            animationMD.type = AssetType::SkeletalAnimation;

            Project::GetInstance()->GetAssetManager().AssignAsset(animationHandle, animation);
            Project::GetInstance()->GetAssetManager().AssignMetaData(animationHandle, animationMD);
            asset->animationHandles.push_back(animationHandle);
        }

        BinarySerializer::SerializeSkeletalMesh(asset, skmBinaryPath);

        asset->handle = handle;
        prepareMeshGpuAndMaterials(asset);
        return asset;
    }

    Ref<Skeleton> AssetImporter::ImportSkeleton(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<Skeleton> asset = Skeleton::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
			asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<SkeletalAnimation> AssetImporter::ImportSkeletalAnimation(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<SkeletalAnimation> asset = SkeletalAnimation::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
			asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<Animation2D> AssetImporter::ImportAnimation2D(AssetHandle handle, const AssetMetaData &metadata)
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

    Ref<AnimatorController2D> AssetImporter::ImportAnimatorController2D(AssetHandle handle, const AssetMetaData &metadata)
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

    Ref<Material> AssetImporter::ImportMaterial(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<Material> asset = Material::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<Material2D> AssetImporter::ImportMaterial2D(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<Material2D> asset = Material2D::Deserialize(metadata.filepath);
        if (asset)
        {
            asset->handle = handle;
            asset->SetReadyFlag(true);
        }
        return asset;
    }

    Ref<Scene> AssetImporter::ImportScene(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<Scene> scene = SceneSerializer::Deserialize(metadata.filepath, AssetManager::GetProject());
        if (scene)
        {
            scene->handle = handle;
			scene->SetReadyFlag(true);

        }
        return scene;
    }

    Ref<Texture> AssetImporter::ImportTexture(AssetHandle handle, const AssetMetaData &metadata)
    {
        TextureCreateInfo createInfo;
        const std::string extension = metadata.filepath.extension().string();
        const bool isHDR = extension == ".hdr";

        createInfo.format = isHDR ? nvrhi::Format::RGBA32_FLOAT : nvrhi::Format::RGBA8_UNORM;
        createInfo.mipLevels = isHDR ? 1 : 4;
        createInfo.flip = isHDR;
        createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfo.keepInitialState = true; // should keep initial state
        createInfo.deferGpuCreate = true;

        return ImportTexture(handle, metadata, createInfo);
    }

    Ref<Texture> AssetImporter::ImportTexture(AssetHandle handle, const AssetMetaData &metadata, const TextureCreateInfo &createInfo)
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

    Ref<FmodSound> AssetImporter::ImportAudio(AssetHandle handle, const AssetMetaData &metadata)
    {
        Ref<FmodSound> sound = FmodSound::Create(metadata.filepath.filename().string(), metadata.filepath.generic_string(), FMOD_DEFAULT);
        if (sound)
        {
            sound->handle = handle;
        }
        return sound;
    }
}
