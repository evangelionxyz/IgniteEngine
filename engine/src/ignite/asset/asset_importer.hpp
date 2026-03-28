// Copyright (c) 2026 Evangelion Manuhutu

#pragma once

#include "asset.hpp"
#include <future>
#include <nvrhi/nvrhi.h>

#include "ignite/scene/entity.hpp"

namespace ignite {

    struct TextureCreateInfo;

    struct FmodSound;
    class Environment;
    class GraphicsPipeline;
    class StaticMesh;
    class SkeletalMesh;
    class Skeleton;
    class SkeletalAnimation;
    class Material2D;
    class Scene;

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

    class AssetImporter
    {
    public:
        static Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata);
        static void ImportAsync(AssetHandle handle, const AssetMetaData &metadata, std::function<void(Ref<Asset>, AssetHandle)> callback);

        static Ref<StaticMesh> ImportStaticMesh(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<SkeletalMesh> ImportSkeletalMesh(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Material> ImportMaterial(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Material2D> ImportMaterial2D(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Skeleton> ImportSkeleton(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<SkeletalAnimation> ImportSkeletalAnimation(AssetHandle handle, const AssetMetaData &metadata);


        static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata, const TextureCreateInfo &createInfo);
        static Ref<FmodSound> ImportAudio(AssetHandle handle, const AssetMetaData &metadata);
    };
}
