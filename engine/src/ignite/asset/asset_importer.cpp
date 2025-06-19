#include "asset_importer.hpp"

#include "ignite/audio/fmod_audio.hpp"
#include "ignite/audio/fmod_sound.hpp"

#include "ignite/core/application.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/environment.hpp"
#include "ignite/graphics/mesh_loader.hpp"
#include "ignite/graphics/mesh.hpp"
#include "ignite/animation/skeleton.hpp"

#include "ignite/serializer/binary_serializer.hpp"

#include "ignite/scene/scene.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/scene_manager.hpp"

namespace ignite {

    static std::unordered_map<AssetType, std::function<Ref<Asset>(AssetHandle, const AssetMetaData &)>> s_ImportFunctions =
    {
        { AssetType::Scene, AssetImporter::ImportScene },
        { AssetType::Texture, AssetImporter::ImportTexture },
        { AssetType::Audio, AssetImporter::ImportAudio },
    };

    void AssetImporter::SyncMainThread()
    {
        // ModelImporter::SyncMainThread(commandList, device);
        EnvironmentImporter::SyncMainThread();
    }

    Ref<Asset> AssetImporter::Import(AssetHandle handle, const AssetMetaData &metadata)
    {
        Project *activeProject = Project::GetActive();

        // should be always importing with full filepath
        AssetMetaData metadataCopy = metadata;
        metadataCopy.filepath = activeProject->GetAssetFilepath(metadata.filepath);

        if (s_ImportFunctions.contains(metadataCopy.type))
            return s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy);

        return nullptr;
    }

    Ref<Scene> AssetImporter::ImportScene(AssetHandle handle, const AssetMetaData& metadata)
    {
        Ref<Scene> scene = SceneSerializer::Deserialize(metadata.filepath);
        if (scene)
        {
            scene->handle = handle;
        }
        return scene;
    }

    Ref<Texture> AssetImporter::ImportTexture(AssetHandle handle, const AssetMetaData &metadata)
    {
        TextureCreateInfo createInfo;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;

        Ref<Texture> texture = Texture::Create(metadata.filepath, createInfo);
        if (texture)
        {
            texture->handle = handle;
        }

        return texture;
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

    void AssetImporter::LoadSkinnedMesh(Scene *scene, Entity outEntity, const std::filesystem::path &filepath)
    {
        LOG_ASSERT(std::filesystem::exists(filepath), "[Mesh Loader] File does not exists!");

        SkinnedMesh &skinnedMesh = outEntity.GetComponent<SkinnedMesh>();
        skinnedMesh.filepath = Project::GetActive()->GetAssetRelativeFilepath(filepath);

        Assimp::Importer importer;
        const aiScene *assimpScene = importer.ReadFile(filepath.generic_string(), ASSIMP_IMPORTER_FLAGS);

        LOG_ASSERT(assimpScene == nullptr || assimpScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || assimpScene->mRootNode,
            "[Asset Importer] Failed to load {}: {}",
            filepath,
            importer.GetErrorString());

        if (!assimpScene)
        {
            return;
        }

        if (assimpScene->HasAnimations())
        {
            std::filesystem::path containerDirectory = filepath.parent_path() / filepath.stem().generic_string();
            if (!std::filesystem::exists(containerDirectory))
                std::filesystem::create_directory(containerDirectory);

            // Load animation
            // MeshLoader::LoadAnimation(assimpScene, skinnedMesh.animations);
            skinnedMesh.animations.resize(assimpScene->mNumAnimations);
            for (uint32_t i = 0; i < assimpScene->mNumAnimations; ++i)
            {
                aiAnimation *anim = assimpScene->mAnimations[i];
                std::string animName = anim->mName.data;

                std::filesystem::path animationFilepath = containerDirectory / (filepath.stem().generic_string() + "_" + animName + ".anim");
                if (std::filesystem::exists(animationFilepath))
                {
                    skinnedMesh.animations[i] = BinarySerializer::DeserializeAnimation(animationFilepath);
                }
                else
                {
                    skinnedMesh.animations[i] = SkeletalAnimation(anim);
                    std::vector<std::byte> bytes = BinarySerializer::SerializeAnimation(anim);
                    std::ofstream of(animationFilepath, std::ios::binary);
                    of.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
                }
            }

            // Process Skeleton
            std::filesystem::path skeletonFilepath = containerDirectory / (filepath.stem().generic_string() + ".skeleton");
            if (std::filesystem::exists(skeletonFilepath))
            {
                skinnedMesh.skeleton = BinarySerializer::DeserializeSkeleton(skeletonFilepath);
            }
            else
            {
                MeshLoader::ExtractSkeleton(assimpScene, skinnedMesh.skeleton);
                MeshLoader::SortJointsHierarchically(skinnedMesh.skeleton);

                std::vector<std::byte> bytes = BinarySerializer::SerializeSkeleton(skinnedMesh.skeleton);
                std::ofstream of(skeletonFilepath, std::ios::binary);
                of.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            }
        }

        std::vector<Ref<Mesh>> meshes;
        meshes.resize(assimpScene->mNumMeshes);
        for (auto &mesh : meshes)
        {
            mesh = CreateRef<Mesh>();
        }

        std::vector<NodeInfo> nodes;

        MeshLoader::ProcessNode(assimpScene, assimpScene->mRootNode, filepath, meshes, nodes, skinnedMesh.skeleton, -1);
        MeshLoader::CalculateWorldTransforms(nodes);

        // First pass: create all node entities
        for (auto &node : nodes)
        {
            if (node.uuid == UUID(0) || node.parentID != -1) // not yet created
            {
                Entity nodeEntity = SceneManager::CreateEntity(scene, node.name, EntityType_Node);
                node.uuid = nodeEntity.GetUUID();

                Transform &tr = nodeEntity.GetComponent<Transform>();

                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(node.localTransform, tr.localScale, tr.localRotation, tr.localTranslation, skew, perspective);

                tr.dirty = true;
            }
        }

        // Second pass: establish hierarchy and add meshes
        for (auto &node : nodes)
        {
            Entity nodeEntity = SceneManager::GetEntity(scene, node.uuid);

            if (node.parentID == -1)
            {
                // Attach the node to root node
                ID &id = outEntity.GetComponent<ID>();
                id.name = filepath.stem().string();

                SceneManager::AddChild(scene, outEntity, nodeEntity);
            }
            else
            {
                // Attach to parent if not root
                const auto &parentNode = nodes[node.parentID];
                Entity parentEntity = SceneManager::GetEntity(scene, parentNode.uuid);
                SceneManager::AddChild(scene, parentEntity, nodeEntity);
            }

            // Attach mesh entities to this node
            for (i32 meshIdx : node.meshIndices)
            {
                const auto &mesh = meshes[meshIdx];

                MeshRenderer &meshRenderer = nodeEntity.AddOrReplaceComponent<MeshRenderer>();
                meshRenderer.meshIndex = meshIdx;
                meshRenderer.root = outEntity.GetUUID();
                meshRenderer.mesh = mesh;
                meshRenderer.mesh->environment = scene->sceneRenderer->GetEnvironment();
                meshRenderer.mesh->CreateBuffers();
                meshRenderer.mesh->UpdateBindingSet();
                meshRenderer.mesh->WriteBuffers(nodeEntity);
            }

            // Extract skeleton joints into entity
            for (size_t i = 0; i < skinnedMesh.skeleton.joints.size(); ++i)
            {
                const std::string &name = skinnedMesh.skeleton.joints[i].name;
                for (auto [uuid, e] : scene->entities)
                {
                    Entity jointEntity = { e, scene };
                    if (jointEntity.GetName() == name)
                    {
                        skinnedMesh.skeleton.jointEntityMap[static_cast<i32>(i)] = uuid;
                        jointEntity.GetComponent<ID>().type = EntityType_Joint;
                        break;
                    }
                }
            }
        }

        MeshLoader::ClearTextureCache();
    }

    void EnvironmentImporter::Import(Ref<Environment> *outEnvironment, const std::string &filepath)
    {
        m_Future = std::async(std::launch::async, ImportAsync, outEnvironment, filepath);
    }

    void EnvironmentImporter::UpdateTexture(Ref<Environment> *outEnvironment, const std::string &filepath)
    {
        m_Future = std::async(std::launch::async, LoadTextureAsync, outEnvironment, filepath);
    }

    void EnvironmentImporter::SyncMainThread()
    {
        if (m_Future.valid() && m_Future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready)
        {
            nvrhi::IDevice *device = Application::GetGraphicsDevice();

            nvrhi::CommandListHandle commandList = device->createCommandList(); 
            
            Ref<Environment> env = m_Future.get();

            commandList->open();
            env->WriteBuffer(commandList);
            commandList->close();
            device->executeCommandList(commandList);

            env->isUpdatingTexture = true;
        }
    }

    Ref<Environment> EnvironmentImporter::ImportAsync(Ref<Environment> *outEnvironment, const std::string &filepath)
    {
        (*outEnvironment) = Environment::Create();
        (*outEnvironment)->LoadTexture(filepath);
        return *outEnvironment;
    }

    Ref<Environment> EnvironmentImporter::LoadTextureAsync(Ref<Environment> *outEnvironment, const std::string &filepath)
    {
        (*outEnvironment)->LoadTexture(filepath);
        return *outEnvironment;
    }

    std::future<Ref<Environment>> EnvironmentImporter::m_Future;

}
