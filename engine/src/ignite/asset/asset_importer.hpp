// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ASSET_IMPORTER_HPP
#define ASSET_IMPORTER_HPP

#include "asset.hpp"
#include <future>
#include <nvrhi/nvrhi.h>

#include "ignite/scene/entity.hpp"

namespace ignite
{
    struct TextureCreateInfo;
    struct FmodSound;
    class Environment;
    class Mesh;
    class Skeleton;
    class Material2D;
    class SpriteSheet;
    class Animation2D;
    class AssetManager;
    class GraphicsPipeline;
    class SkeletalAnimation;
    class AnimationMontage;
    class BlendSpace;
    class LocomotionController;
    class AnimatorController;
    class AnimatorController2D;
    class Scene;
    class Font;
    class WidgetCanvas;

    struct PendingFileLoading
    {
        enum Type : uint8_t
        {
            None = 0,
            Open,
            Save,
            ImportAssets,
        };

        Type type = None;
        AssetMetaData metadata;
        void *userData = nullptr;
    };

    struct AssetImporterPayload
    {
        std::filesystem::path targetDirectory;
        AssetType assetType = AssetType::Invalid;
    };

    struct MeshImportOptions
    {
        bool importMesh = true;
        bool importSkeleton = true;
        bool importAnimations = true;
        bool importMaterials = true;
        bool forceRebuild = false;
        std::filesystem::path targetDirectory;
        bool useExistingSkeletonForAnimations = false;
        AssetHandle existingSkeletonHandle = AssetHandle(0);
    };

    struct StaticMeshImportOptions
    {
        std::filesystem::path targetDirectory;
    };

    class AssetImporter
    {
    public:
        static Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static void ImportAsync(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, std::function<void(Ref<Asset>, AssetHandle)> callback);

        static Ref<Mesh> ImportMesh(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, const MeshImportOptions &options = MeshImportOptions());
        static Ref<Material> ImportMaterial(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Material2D> ImportMaterial2D(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<SpriteSheet> ImportSpriteSheet(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Font> ImportFont(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<WidgetCanvas> ImportWidget(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Skeleton> ImportSkeleton(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<SkeletalAnimation> ImportSkeletalAnimation(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<AnimationMontage> ImportAnimationMontage(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<BlendSpace> ImportBlendSpace(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<LocomotionController> ImportLocomotionController(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<AnimatorController> ImportAnimatorController(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);

        static Ref<Animation2D> ImportAnimation2D(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<AnimatorController2D> ImportAnimatorController2D(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);

        static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata, const TextureCreateInfo &createInfo, AssetManager *assetManager);
        static Ref<FmodSound> ImportAudio(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
    };
}

#endif
