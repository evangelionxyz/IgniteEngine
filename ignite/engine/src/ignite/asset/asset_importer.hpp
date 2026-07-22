// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_IMPORTER_HPP
#define IGN_ASSET_IMPORTER_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/scene/component.hpp"

#include <functional>

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
    class Prefab;
    class Font;
    class WidgetCanvas;
    class ScriptableObject;

    enum class FileStatus : uint8_t
    {
        Failure = 0,
        Success = 1,
        Pending = 2,

        Unknown,
    };

    enum class ImportType : uint8_t
    {
        None = 0,
        Open,
        Save,
        Import,
    };

    struct FileImportPayload
    {
        ImportType type = ImportType::None;
        FileStatus status = FileStatus::Unknown;
        AssetMetaData metadata;
        ignite::Path targetDirectory;
        void *userData = nullptr;
    };

    struct StaticMeshImportPayload : public FileImportPayload
    {
		bool importMaterials = true;
		bool forceRebuild = false;
    };

	struct SkeletalMeshImportPayload : public StaticMeshImportPayload
    {
        bool importMesh = true;
        bool importSkeleton = true;
        bool importAnimations = true;
        bool useExistingSkeletonForAnimations = false;

        AssetHandle existingSkeletonHandle = AssetHandle(0);
    };

    class IGN_API AssetImporter
    {
    public:
        static Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static void ImportAsync(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, std::function<void(Ref<Asset>, AssetHandle)> callback);

		static Ref<SkeletalMesh> ImportSkeletalMesh(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, const SkeletalMeshImportPayload &payload = SkeletalMeshImportPayload());
		static Ref<StaticMesh> ImportStaticMesh(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager, const StaticMeshImportPayload &payload = StaticMeshImportPayload());

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

        static Ref<ScriptableObject> ImportScriptableObject(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Prefab> ImportPrefab(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata, const TextureCreateInfo &createInfo, AssetManager *assetManager);
        static Ref<FmodSound> ImportAudio(AssetHandle handle, const AssetMetaData &metadata, AssetManager *assetManager);
    };
}

#endif
