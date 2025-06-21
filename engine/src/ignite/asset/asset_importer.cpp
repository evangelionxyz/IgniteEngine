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
                idComp.isPrefab = true;
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
                const Ref<Mesh> &mesh = meshes[meshIdx];
                bool isSkinnedMesh = true;
                MeshRenderer &mr = nodeEntity.AddComponent<MeshRenderer>();

                mr.Create(isSkinnedMesh);
                mr.meshIndex = meshIdx;
                mr.material = materials[mesh->materialIndex]; // set material

                if (!mr.material)
                    mr.material = CreateRef<Material>(); // default material

                mr.root = outEntity.GetUUID();
                mr.mesh = mesh;
                mr.mesh->CreateBuffers();
                mr.mesh->WriteVertexBuffer(nodeEntity);
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
    }


    // Material Importer Class
    std::unordered_map<std::string, Ref<MaterialTextureResource>> MaterialImporter::s_TextureCache;

    std::vector<Ref<Material>> MaterialImporter::Load(const aiScene* aiScene, const std::filesystem::path &filepath)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        nvrhi::CommandListHandle commandList = device->createCommandList();
        commandList->open();

        // Create material buckets and reserve space
        std::vector<Ref<Material>> materials;
        materials.reserve(aiScene->mNumMaterials);

        for (uint32_t materialIndex = 0; materialIndex < aiScene->mNumMaterials; ++materialIndex)
        {
            aiMaterial *aiMat = aiScene->mMaterials[materialIndex];
            Ref<Material> material = CreateRef<Material>();
            material->name = aiMat->GetName().data;

            aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
            aiColor4D diffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
            aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 0.0f);
            f32 reflectivity = 0.0f;

            aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor);
            aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
            aiMat->Get(AI_MATKEY_METALLIC_FACTOR, material->params.metallicFactor);
            aiMat->Get(AI_MATKEY_SPECULAR_FACTOR, material->params.specularFactor);
            aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, material->params.roughnessFactor);
            aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
            aiMat->Get(AI_MATKEY_REFLECTIVITY, reflectivity);

            material->params.baseColor = { baseColor.r, baseColor.g, baseColor.b, 1.0f };

            if (diffuseColor.r > 0.0f)
            {
                material->params.emissiveFactor = emissiveColor.r / diffuseColor.r;
            }

            // load textures
            LoadTexture(aiScene, materialIndex, filepath, material, MaterialTextureType::BaseColor);
            LoadTexture(aiScene, materialIndex, filepath, material, MaterialTextureType::Specular);
            LoadTexture(aiScene, materialIndex, filepath, material, MaterialTextureType::Emissive);
            LoadTexture(aiScene, materialIndex, filepath, material, MaterialTextureType::Roughness);
            LoadTexture(aiScene, materialIndex, filepath, material, MaterialTextureType::Normals);

            // set transparent and reflectivity
            // material->_transparent = false;
            // material->_reflective = reflectivity > 0.0f;

            // Create loaded textures
            for (const auto& tex : material->textures | std::views::values)
            {
                // use white texture
                if (tex->data == nullptr)
                {
                    tex->handle = Renderer::GetWhiteTexture()->GetHandle();
                    continue;
                }

                // create texture
                auto textureDesc = nvrhi::TextureDesc();
                textureDesc.setDimension(nvrhi::TextureDimension::Texture2D);
                textureDesc.setWidth(tex->width);
                textureDesc.setHeight(tex->height);
                textureDesc.setFormat(nvrhi::Format::RGBA8_UNORM);
                textureDesc.setInitialState(nvrhi::ResourceStates::ShaderResource);
                textureDesc.setKeepInitialState(true);
                textureDesc.setMipLevels(material->mipLevels);
                textureDesc.setDebugName("Material embedded Texture");

                tex->handle = device->createTexture(textureDesc);
                LOG_ASSERT(tex->handle, "[Material Importer] Failed to create texture!");
            }
           
            material->WriteTexture(commandList);

            materials.push_back(material);
        }

        commandList->close();
        device->executeCommandList(commandList);

        s_TextureCache.clear();
        return materials;
    }

    void MaterialImporter::LoadTexture(const aiScene* aiScene, uint32_t materialIndex, const std::filesystem::path& filepath,
        const Ref<Material>& material, MaterialTextureType textureType)
    {
        const aiMaterial *mat = aiScene->mMaterials[materialIndex];
        const aiTextureType type = GetAssimpTextureType(textureType);

        // Create the material texture first (Ref counted object)
        material->textures[textureType] = CreateRef<MaterialTextureResource>();

        if (const uint32_t texCount = mat->GetTextureCount(type))
        {
            for (uint32_t i = 0; i < texCount; ++i)
            {
                aiString texFilename;
                mat->GetTexture(type, i, &texFilename);

                LOG_INFO("[Material Importer] Texture type {}", aiTextureTypeToString(type));

                // try to load from cache
                for (auto &[path, tex] : s_TextureCache)
                {
                    if (std::strcmp(path.c_str(), texFilename.C_Str()) == 0)
                    {
                        material->textures[textureType] = tex;
                        LOG_WARN("[Material Importer] {} Loaded from cache", path.c_str());
                        return;
                    }
                }

                stbi_set_flip_vertically_on_load(false);
                int width, height, channels;
                uint8_t *sourceData = nullptr;

                // create new texture
                // Load embedded texture
                const aiTexture *embeddedTexture = aiScene->GetEmbeddedTexture(texFilename.C_Str());
                if (embeddedTexture && material->textures[textureType]->handle == nullptr)
                {
                    // handle compressed textures
                    if (embeddedTexture->mHeight == 0)
                    {
                        LOG_INFO("[Material Importer] Loading embedded compressed format texture of size {} bytes", embeddedTexture->mWidth);
                        material->textures[textureType]->data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(embeddedTexture->pcData),
                            embeddedTexture->mWidth, &width, &height, &channels, 4);
                    }
                    else
                    {
                        width = static_cast<int>(embeddedTexture->mWidth);
                        height = static_cast<int>(embeddedTexture->mHeight);

                        LOG_INFO("[Material Importer] Loading embedded uncompressed texture of size {}x{}", width, height);

                        // Allocate space for RGBA8 data
                        uint8_t *destinationData = new uint8_t[width * height * 4];

                        // Assimp embedded uncompressed texture data is usually in RGB format without alpha
                        // You can test with alpha channel (or assume RGB with alpha set to 255)
                        for (int p = 0; p < width * height; ++p)
                        {
                            destinationData[p * 4 + 0] = sourceData[p * 3 + 0]; // R
                            destinationData[p * 4 + 1] = sourceData[p * 3 + 1]; // G
                            destinationData[p * 4 + 2] = sourceData[p * 3 + 2]; // B
                            destinationData[p * 4 + 3] = 255;                   // A
                        }

                        material->textures[textureType]->data = destinationData;

                        sourceData = reinterpret_cast<uint8_t *>(embeddedTexture->pcData);
                        LOG_ASSERT(sourceData, "[Material Importer] Failed to load texture");
                    }
                }
                else
                {
                    // Texture from file
                    std::filesystem::path textureFilepath = filepath.parent_path() / std::string(texFilename.C_Str());
                    if (!std::filesystem::exists(textureFilepath))
                    {
                        LOG_ERROR("[Material Importer] texture path is not found! \"{}\"", textureFilepath.generic_string());
                        return;
                    }

                    LOG_INFO("[Material Importer] Load texture from \"{}\"", textureFilepath.generic_string());
                    sourceData = stbi_load(textureFilepath.generic_string().c_str(), &width, &height, &channels, 4);
                    LOG_ASSERT(sourceData, "[Material Importer] Failed to load texture");
                }

                if (sourceData)
                {
                    material->textures[textureType]->data = sourceData;
                    LOG_ASSERT(material->textures[textureType]->data, "[Material Importer] Failed to load texture");
                }

                if (material->textures[textureType]->data)
                {
                    material->textures[textureType]->width = static_cast<uint32_t>(width);
                    material->textures[textureType]->height = static_cast<uint32_t>(height);
                    // material->textures[textureType]->buffer.Size = static_cast<uint64_t>(width * height) * 4u;
                    material->textures[textureType]->rowPitch = width * 4u;

                    s_TextureCache[texFilename.C_Str()] = material->textures[textureType];
                }
            }
        }
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
