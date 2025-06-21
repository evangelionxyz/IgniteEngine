#pragma once

#include "asset.hpp"
#include <future>
#include <nvrhi/nvrhi.h>

#include "ignite/scene/entity.hpp"

namespace ignite {

    struct FmodSound;
    class Environment;
    class GraphicsPipeline;
    class Scene;

    class AssetImporter
    {
    public:
        static void SyncMainThread();
        static Ref<Asset> Import(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<Texture> ImportTexture(AssetHandle handle, const AssetMetaData &metadata);
        static Ref<FmodSound> ImportAudio(AssetHandle handle, const AssetMetaData &metadata);

        static void LoadSkinnedMesh(Scene *scene, Entity outEntity, const std::filesystem::path& filepath);
    };

    class MaterialImporter : public AssetImporter
    {
    public:
        static std::vector<Ref<Material>> Load(const aiScene *aiScene, const std::filesystem::path &filepath);
        static void LoadTexture(const aiScene *aiScene, uint32_t materialIndex, const std::filesystem::path &filepath, const Ref<Material>& material, MaterialTextureType textureType);

    private:
        static std::unordered_map<std::string, Ref<MaterialTextureResource>> s_TextureCache;
    };

    class EnvironmentImporter : public AssetImporter
    {
    public:
        static void Import(Ref<Environment> *outEnvironment, const std::string &filepath);
        static void UpdateTexture(Ref<Environment> *outEnvironment, const std::string &filepath);
        static void SyncMainThread();

    private:
        static Ref<Environment> ImportAsync(Ref<Environment> *outEnvironment, const std::string &filepath);
        static Ref<Environment> LoadTextureAsync(Ref<Environment> *outEnvironment, const std::string &filepath);
        static std::future<Ref<Environment>> m_Future;
    };
}
