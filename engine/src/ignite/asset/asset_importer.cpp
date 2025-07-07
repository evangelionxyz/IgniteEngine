/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "asset_importer.hpp"

#include <stb_image.h>

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
        { AssetType::Skeleton, MeshImporter::ImportSkeleton },
        { AssetType::MeshSource, MeshImporter::ImportMeshSource },
        { AssetType::SkeletalMesh, MeshImporter::ImportSkeletalMesh },
        { AssetType::Material, MeshImporter::ImportMaterial },
    };

    void AssetImporter::SyncMainThread()
    {
        EnvironmentImporter::SyncMainThread();
    }

    Ref<Asset> AssetImporter::Import(AssetHandle handle, const AssetMetaData &metadata)
    {
        // should be always importing with full filepath
        AssetMetaData metadataCopy = metadata;
        metadataCopy.filepath = Project::GetActive()->GetAssetFilepath(metadata.filepath);

        if (s_ImportFunctions.contains(metadataCopy.type))
            return s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy);

        return nullptr;
    }

    void AssetImporter::ImportAsync(AssetHandle handle, const AssetMetaData &metadata, std::function<void(Ref<Asset>, AssetHandle)> callback)
    {
        Project::GetActive()->GetAssetManager().SubmitJob([handle, metadata, callback]()
        {
            // should be always importing with full filepath
            AssetMetaData metadataCopy = metadata;
            metadataCopy.filepath = Project::GetActive()->GetAssetFilepath(metadata.filepath);

            Ref<Asset> asset;
            if (s_ImportFunctions.contains(metadataCopy.type))
                asset = s_ImportFunctions.at(metadataCopy.type)(handle, metadataCopy);
            
            if (asset)
                callback(asset, handle);
        });
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
        createInfo.mipLevels = 4;

        Ref<Texture> texture = Texture::Create(metadata.filepath, createInfo);
        if (texture)
        {
            // asset handle
            texture->handle = handle;
            Renderer::Submit([tex = texture](nvrhi::ICommandList *commandList)
            {
                tex->Write(commandList);
            });
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

    Ref<Asset> MeshImporter::ImportMeshSource(AssetHandle handle, const AssetMetaData &metadata)
    {
        Assimp::Importer importer;
        const aiScene *assimpScene = importer.ReadFile(metadata.filepath.generic_string(), ASSIMP_IMPORTER_FLAGS);

        LOG_ASSERT(assimpScene == nullptr || assimpScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || assimpScene->mRootNode,
            "[Asset Importer] Failed to load {}: {}", metadata.filepath, importer.GetErrorString());

        if (!assimpScene)
            return nullptr;

        std::filesystem::path containerDirectory = metadata.filepath.parent_path() / metadata.filepath.stem().generic_string();
        if (!std::filesystem::exists(containerDirectory))
            std::filesystem::create_directory(containerDirectory);

        // Generated directory
        std::filesystem::path meshBinaryFilepath = containerDirectory / (metadata.filepath.stem().generic_string() + ".ixmesh");
        Ref<MeshAsset> meshAsset = CreateRef<MeshAsset>();
        meshAsset->handle = handle;

        if (std::filesystem::exists(meshBinaryFilepath))
        {
            meshAsset = BinarySerializer::DeserializeMeshAsset(meshBinaryFilepath);
        }
        else
        {
            // Load Meshes
            std::vector<Ref<Mesh>> meshes;
            meshes.resize(assimpScene->mNumMeshes);
            meshAsset->meshes.resize(assimpScene->mNumMeshes);

            for (auto &mesh : meshes)
            {
                mesh = CreateRef<Mesh>();
            }

            MeshLoader::ProcessNode(assimpScene, assimpScene->mRootNode, metadata.filepath, meshes,
                meshAsset->nodes, nullptr, -1);

            MeshLoader::CalculateWorldTransforms(meshAsset->nodes);

            for (size_t meshIdx = 0; meshIdx < meshes.size(); ++meshIdx)
            {
                Ref<Mesh> mesh = meshes[meshIdx];
                meshAsset->meshes[meshIdx].meshIndex = mesh->data.meshIndex;
                meshAsset->meshes[meshIdx].materialIndex = mesh->data.materialIndex;
                meshAsset->meshes[meshIdx].nodeParentID = mesh->data.nodeParentID;
                meshAsset->meshes[meshIdx].nodeID = mesh->data.nodeID;
                meshAsset->meshes[meshIdx].name = mesh->data.name;
                meshAsset->meshes[meshIdx].vertices = mesh->data.vertices;
                meshAsset->meshes[meshIdx].indices = mesh->data.indices;
            }

            // Save to binary
            BinarySerializer::SerializeMeshAsset(meshAsset, meshBinaryFilepath);
        }

        // Load Materials
        meshAsset->materials.reserve(assimpScene->mNumMaterials);
        for (uint32_t matIndex = 0; matIndex < assimpScene->mNumMaterials; ++matIndex)
        {
            aiMaterial *aiMat = assimpScene->mMaterials[matIndex];
            std::string materialName = aiMat->GetName().data;

            if (materialName.empty())
                continue;

            Ref<Material> mat;
            std::filesystem::path materialBinaryFilepath = containerDirectory / (materialName + ".ixmat");
            if (std::filesystem::exists(materialBinaryFilepath))
            {
                mat = BinarySerializer::DeserializeMaterial(materialBinaryFilepath);
            }
            else
            {
                mat = CreateRef<Material>(assimpScene, aiMat, metadata.filepath);
                BinarySerializer::SerializeMaterial(mat, materialBinaryFilepath);
            }

            if (mat)
            {
                mat->CreateTextures();
                meshAsset->materials.push_back(mat);
            }
        }

        return meshAsset;
    }

#ifdef OLD
    void AssetImporter::LoadSkinnedMesh(Scene *scene, Entity outEntity, const std::filesystem::path &filepath)
    {
        LOG_ASSERT(std::filesystem::exists(filepath), "[Mesh Loader] File does not exists!");

        SkeletalMesh &skinnedMesh = outEntity.GetComponent<SkeletalMesh>();

        Assimp::Importer importer;
        const aiScene *assimpScene = importer.ReadFile(filepath.generic_string(), ASSIMP_IMPORTER_FLAGS);

        LOG_ASSERT(assimpScene == nullptr || assimpScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || assimpScene->mRootNode,
            "[Asset Importer] Failed to load {}: {}", filepath, importer.GetErrorString());

        if (!assimpScene)
            return;

        // Load Animations
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
            std::filesystem::path skeletonFilepath = containerDirectory / (filepath.stem().generic_string() + ".skel");
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

        // Load Materials
        std::vector<Ref<Material>> materials = MaterialImporter::Load(assimpScene, filepath);

        // Load Meshes
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

                ID &idComp = nodeEntity.GetComponent<ID>();
                node.uuid = idComp.uuid;

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
            ID &idComp = nodeEntity.GetComponent<ID>();
            idComp.type = EntityType_Node | EntityType_Prefab;

            if (node.parentID == -1)
            {
                // Attach the node to root node
                ID &rootIDComp = outEntity.GetComponent<ID>();
                rootIDComp.name = filepath.stem().string();
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
                const Ref<Mesh> &mesh = meshes[meshIdx];
                bool isSkinnedMesh = true;
                MeshRenderer &mr = nodeEntity.AddComponent<MeshRenderer>();

                mr.Create(isSkinnedMesh);

                mr.material = materials[mesh->materialIndex] ? materials[mesh->materialIndex] : CreateRef<Material>(); // set material
                mr.root = outEntity.GetUUID();
                mr.mesh = mesh;
                mr.mesh->CreateBuffers();
                mr.mesh->WriteVertexBuffer(nodeEntity);
            }

            // Extract skeleton joints into entity
            for (size_t i = 0; i < skinnedMesh.skeleton->joints.size(); ++i)
            {
                const std::string &name = skinnedMesh.skeleton->joints[i].name;
                for (auto [uuid, e] : scene->entities)
                {
                    Entity jointEntity = { e, scene };
                    if (jointEntity.GetName() == name)
                    {
                        skinnedMesh.skeleton->jointEntityMap[static_cast<i32>(i)] = uuid;
                        jointEntity.GetComponent<ID>().type = EntityType_Joint | EntityType_Prefab;
                        break;
                    }
                }
            }
        }
    }
#endif

    Ref<Asset> MeshImporter::ImportSkeletalMesh(AssetHandle handle, const AssetMetaData& metadata)
    {
        // Generated directory
        std::filesystem::path containerDirectory = metadata.filepath.parent_path() / metadata.filepath.stem().generic_string();

        std::filesystem::path meshBinaryFilepath = containerDirectory / (metadata.filepath.stem().generic_string() + ".ixmesh");
        Ref<MeshAsset> skeletalMeshAsset = CreateRef<MeshAsset>();

        Assimp::Importer importer;
        const aiScene *assimpScene = importer.ReadFile(metadata.filepath.generic_string(), ASSIMP_IMPORTER_FLAGS);

        LOG_ASSERT(assimpScene == nullptr || assimpScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || assimpScene->mRootNode,
            "[Asset Importer] Failed to load {}: {}", metadata.filepath, importer.GetErrorString());

        if (!assimpScene)
            return nullptr;

        Ref<Skeleton> skeleton;
        std::vector<Ref<SkeletalAnimation>> animations;

        // Load Animations
        if (assimpScene->HasAnimations())
        {
            if (!std::filesystem::exists(containerDirectory))
                std::filesystem::create_directory(containerDirectory);

            // Load animation
            // MeshLoader::LoadAnimation(assimpScene, skinnedMesh.animations);
            animations.resize(assimpScene->mNumAnimations);
            for (uint32_t i = 0; i < assimpScene->mNumAnimations; ++i)
            {
                aiAnimation *anim = assimpScene->mAnimations[i];
                std::string animName = anim->mName.data;

                std::filesystem::path animationFilepath = containerDirectory / (metadata.filepath.stem().generic_string() + "_" + animName + ".anim");
                if (std::filesystem::exists(animationFilepath))
                {
                    animations[i] = BinarySerializer::DeserializeAnimation(animationFilepath);
                }
                else
                {
                    animations[i] = CreateRef<SkeletalAnimation>(anim);
                    BinarySerializer::SerializeAnimation(animations[i], animationFilepath);
                }
            }

            // Process Skeleton
            std::filesystem::path skeletonFilepath = containerDirectory / (metadata.filepath.stem().generic_string() + ".skel");
            if (std::filesystem::exists(skeletonFilepath))
            {
                skeleton = BinarySerializer::DeserializeSkeleton(skeletonFilepath);
            }
            else
            {
                MeshLoader::ExtractSkeleton(assimpScene, skeleton);
                MeshLoader::SortJointsHierarchically(skeleton);
                BinarySerializer::SerializeSkeleton(skeleton, skeletonFilepath);
            }
        }

        // Load Materials
        // std::vector<Ref<Material>> materials = MaterialImporter::Load(assimpScene, metadata.filepath);

        // Load Meshes
        std::vector<Ref<Mesh>> meshes;
        meshes.resize(assimpScene->mNumMeshes);
        for (auto &mesh : meshes)
        {
            mesh = CreateRef<Mesh>();
        }

        MeshLoader::ProcessNode(assimpScene, assimpScene->mRootNode, metadata.filepath, meshes, skeletalMeshAsset->nodes, skeleton, -1);
        MeshLoader::CalculateWorldTransforms(skeletalMeshAsset->nodes);

        // Save to binary
        BinarySerializer::SerializeMeshAsset(skeletalMeshAsset, meshBinaryFilepath);

        return skeletalMeshAsset;
    }

    Ref<Asset> MeshImporter::ImportSkeleton(AssetHandle handle, const AssetMetaData& metadata)
    {
        Ref<Skeleton> skeleton = BinarySerializer::DeserializeSkeleton(metadata.filepath);

        if (skeleton)
            skeleton->handle = handle;

        return skeleton;
    }

    Ref<Asset> MeshImporter::ImportAnimation(AssetHandle handle, const AssetMetaData& metadata)
    {
        /*Ref<SkeletalAnimation> animation = BinarySerializer::DeserializeAnimation(metadata.filepath);

        if (animation)
            animation->handle = handle;*/

        return nullptr;
    }

    Ref<Asset> MeshImporter::ImportMaterial(AssetHandle handle, const AssetMetaData& metadata)
    {
        Ref<Material> mat;
        return mat;
    }
    

    // Environment Importer Class
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
