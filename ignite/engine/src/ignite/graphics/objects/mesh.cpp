// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "mesh.hpp"
#include "ignite/core/time.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/scene/scene.hpp"

#include <fbxsdk.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace ignite
{
    using namespace fbxsdk;

    namespace
    {
        static bool Mat4NearEqual(const glm::mat4 &a, const glm::mat4 &b, const float epsilon = 0.0001f)
        {
            for (int c = 0; c < 4; ++c)
            {
                for (int r = 0; r < 4; ++r)
                {
                    if (fabs(a[c][r] - b[c][r]) > epsilon)
                    {
                        return false;
                    }
                }

            }
            return true;
        }

        static glm::vec3 ExtractTranslation(const glm::mat4 &m)
        {
            return { m[3][0], m[3][1], m[3][2] };
        }

        // Scales only the translation column of a matrix by the given factor.
        // Used to convert FBX-native units (e.g. cm) to engine units (m) without
        // touching the rotation or scale components.
        static glm::mat4 ScaleTranslation(const glm::mat4 &m, float s)
        {
            glm::mat4 result = m;
            result[3][0] *= s;
            result[3][1] *= s;
            result[3][2] *= s;
            return result;
        }

        static glm::mat4 ToGlmMatrix(const FbxAMatrix &matrix)
        {
            glm::mat4 result(1.0f);
            for (int row = 0; row < 4; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    result[row][col] = static_cast<float>(matrix.Get(row, col));
                }
            }
            return result;
        }

        static std::string ToLowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        static FbxAMatrix BuildNodeGeometricMatrix(FbxNode *node)
        {
            FbxAMatrix geometric;
            geometric.SetIdentity();

            if (!node)
            {
                return geometric;
            }

            geometric.SetT(node->GetGeometricTranslation(FbxNode::eSourcePivot));
            geometric.SetR(node->GetGeometricRotation(FbxNode::eSourcePivot));
            geometric.SetS(node->GetGeometricScaling(FbxNode::eSourcePivot));

            return geometric;
        }

        static Ref<Material> CreateDefaultMaterial(const std::string &name)
        {
            Ref<Material> material = CreateRef<Material>();
            material->name = name.empty() ? "DefaultMaterial" : name;
            return material;
        }

        static bool TryGetMaterialPropertyVec3(FbxSurfaceMaterial *material, const std::initializer_list<const char *> &propertyNames, glm::vec3 &outValue)
        {
            if (!material)
            {
                return false;
            }

            for (const char *name : propertyNames)
            {
                FbxProperty property = material->FindProperty(name);
                if (!property.IsValid())
                {
                    property = material->RootProperty.Find(name);
                }

                if (!property.IsValid())
                {
                    continue;
                }

                if (property.GetPropertyDataType().GetType() == eFbxDouble3)
                {
                    const FbxDouble3 value = property.Get<FbxDouble3>();
                    outValue = glm::vec3((float)value[0], (float)value[1], (float)value[2]);
                    return true;
                }

                if (property.GetPropertyDataType().GetType() == eFbxDouble4)
                {
                    const FbxDouble4 value = property.Get<FbxDouble4>();
                    outValue = glm::vec3((float)value[0], (float)value[1], (float)value[2]);
                    return true;
                }
            }

            return false;
        }

        static bool TryGetMaterialPropertyFloat(FbxSurfaceMaterial *material, const std::initializer_list<const char *> &propertyNames, float &outValue)
        {
            if (!material)
            {
                return false;
            }

            for (const char *name : propertyNames)
            {
                FbxProperty property = material->FindProperty(name);
                if (!property.IsValid())
                {
                    property = material->RootProperty.Find(name);
                }

                if (!property.IsValid())
                {
                    continue;
                }

                const auto type = property.GetPropertyDataType().GetType();
                if (type == eFbxDouble)
                {
                    outValue = (float)property.Get<FbxDouble>();
                    return true;
                }

                if (type == eFbxFloat)
                {
                    outValue = property.Get<FbxFloat>();
                    return true;
                }
            }

            return false;
        }

        static bool TryLoadFBXTextureFromProperty(FbxSurfaceMaterial *material, const std::initializer_list<const char *> &propertyNames,
            const ignite::Path &sourceDir, FBXMeshLoader::MaterialLoader &materialLoader, MeshMaterialTextureMap &textureMap)
        {
            if (!material)
            {
                return false;
            }

            for (const char *name : propertyNames)
            {
                FbxProperty property = material->FindProperty(name);
                if (!property.IsValid())
                {
                    property = material->RootProperty.Find(name);
                }

                if (!property.IsValid())
                {
                    continue;
                }

                const int textureCount = property.GetSrcObjectCount<FbxFileTexture>();
                for (int i = 0; i < textureCount; ++i)
                {
                    FbxFileTexture *fbxTexture = property.GetSrcObject<FbxFileTexture>(i);
                    if (!fbxTexture)
                    {
                        continue;
                    }

                    ignite::Path texturePath = fbxTexture->GetFileName();
                    if (texturePath.empty())
                    {
                        texturePath = fbxTexture->GetRelativeFileName();
                    }

                    if (texturePath.empty())
                    {
                        continue;
                    }

                    if (texturePath.is_relative())
                    {
                        texturePath = sourceDir / texturePath;
                    }

                    texturePath = texturePath.lexically_normal();
                    if (!ignite::Path::exists(texturePath))
                    {
                        continue;
                    }

                    const std::string key = ToLowerCopy(texturePath.generic_string());
                    std::string textureName = texturePath.stem().string();
                    if (materialLoader.textureLookup.contains(key))
                    {
                        const int index = materialLoader.textureLookup.at(key);
                        textureMap = { index, textureName, materialLoader.loadedTextures[index] };
                        return true;
                    }

                    TextureCreateInfo createInfo;
                    createInfo.flip = true;
                    createInfo.format = ToLowerCopy(texturePath.extension().string()) == ".exr" ? nvrhi::Format::RGBA32_FLOAT : nvrhi::Format::RGBA8_UNORM;
                    createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
                    createInfo.keepInitialState = true;
                    createInfo.keepCpuData = true;
                    createInfo.deferGpuCreate = true;
                    createInfo.mipLevels = 4;

                    Ref<Texture> texture = Texture::Create(texturePath, createInfo, nullptr);
                    if (!texture)
                    {
                        continue;
                    }

                    texture->PrepareUploadData(4);
                    Application::SubmitToRenderThread([texture]()
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

                    const int index = (int)materialLoader.loadedTextures.size();
                    materialLoader.loadedTextures.push_back(texture);
                    materialLoader.textureLookup[key] = index;
                    textureMap = { index, textureName, texture };
                    return true;
                }
            }

            return false;
        }

        static Ref<Material> CreateMaterialFromFBX(FbxSurfaceMaterial *fbxMaterial)
        {
            if (!fbxMaterial)
            {
                return CreateDefaultMaterial("DefaultMaterial");
            }

            Ref<Material> material = CreateRef<Material>();
            material->name = fbxMaterial->GetName();

            glm::vec3 diffuse(1.0f);
            if (TryGetMaterialPropertyVec3(fbxMaterial, { "DiffuseColor", "Diffuse", "BaseColor" }, diffuse))
            {
                material->gpuData.baseColorFactor = glm::vec4(diffuse, 1.0f);
            }

            // Load transparency/opacity if available
            float opacity = 1.0f;
            if (TryGetMaterialPropertyFloat(fbxMaterial, { "Opacity" }, opacity))
            {
                material->gpuData.baseColorFactor.a = opacity;
            }
            else
            {
                float transparencyFactor = 0.0f;
                if (TryGetMaterialPropertyFloat(fbxMaterial, { "TransparencyFactor" }, transparencyFactor))
                {
                    material->gpuData.baseColorFactor.a = 1.0f - transparencyFactor;
                }
            }

            glm::vec3 emissive(0.0f);
            if (TryGetMaterialPropertyVec3(fbxMaterial, { "EmissiveColor", "Emissive" }, emissive))
            {
                material->gpuData.emissiveFactor = glm::vec4(emissive, 1.0f);
            }

            float shininess = 0.0f;
            if (TryGetMaterialPropertyFloat(fbxMaterial, { "Shininess", "SpecularExponent" }, shininess))
            {
                material->gpuData.roughnessFactor = glm::clamp(1.0f - shininess / 100.0f, 0.0f, 1.0f);
            }
            else
            {
                material->gpuData.roughnessFactor = 1.0f;
            }

            material->gpuData.metallicFactor = 0.0f;
            return material;
        }

        // Helper to build mat4 from glTF node TRS
        static glm::mat4 BuildNodeLocalMatrix(const tinygltf::Node &node)
        {
            if (!node.matrix.empty())
            {
                // glTF supplies 16 values column-major. Construct manually.
                return glm::mat4((float)node.matrix[0], (float)node.matrix[1], (float)node.matrix[2], (float)node.matrix[3],
                    (float)node.matrix[4], (float)node.matrix[5], (float)node.matrix[6], (float)node.matrix[7],
                    (float)node.matrix[8], (float)node.matrix[9], (float)node.matrix[10], (float)node.matrix[11],
                    (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]);
            }

            glm::vec3 translation(0.0f);
            glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            glm::vec3 scale(1.0f);

            if (!node.translation.empty())
            {
                translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }

            if (!node.rotation.empty())
            {
                rotation = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
            }

            if (!node.scale.empty())
            {
                scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }

            return glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
        }
    }

    // ===================================
    // Mesh Instance
    // ===================================
    MeshInstance::~MeshInstance()
    {
        // Wait for GPU to ensure resources are not in use
        if (auto *device = DeviceManager::GetInstance()->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }

        AssetManager::GetInstance()->RemoveAssetPin(m_MaterialHandle, std::format("meshinstance.material.{}.{}", (uint64_t)m_UUID, (uint64_t)m_MaterialHandle));

        m_MeshBindingSet = nullptr;
    }
    
    void MeshInstance::SetMaterial(const AssetHandle &assetHandle)
    {
        m_MaterialHandle = assetHandle;
        AssetManager::GetInstance()->AddAssetPin(m_MaterialHandle, std::format("meshinstance.material.{}.{}", (uint64_t)m_UUID, (uint64_t)m_MaterialHandle));
    }

    void MeshInstance::SetData(nvrhi::ICommandList *cmd, void *data, size_t size)
    {
        m_MeshConstantBuffer->SetData(cmd, Buffer(data, size));
    }

    // ===================================
    // Static Mesh Instance
    // ===================================
    StaticMeshInstance::StaticMeshInstance(const MeshNode<VertexMeshStatic> &node, const Ref<MeshPrimitive<VertexMeshStatic>> &primitive)
    {
        m_Name = node.name;
        m_Primitive = primitive;

        local = node.local;
        global = node.global;

        if (!m_MeshConstantBuffer)
        {
            constexpr uint32_t maxVersion = 1;
            m_MeshConstantBuffer = ConstantBuffer::Create(sizeof(Mesh_GPUData), false, 1, "Per-Entity Transform Buffer");
            LOG_INFO("[MeshInstance] Created per-draw object buffer '{}' as volatile", m_Name.empty() ? "<unnamed>" : m_Name);
        }
    }

    StaticMeshInstance::StaticMeshInstance(const std::string &name, const Ref<MeshPrimitive<VertexMeshStatic>> &primitive)
    {
        m_Name = name;
        m_Primitive = primitive;

        if (!m_MeshConstantBuffer)
        {
            constexpr uint32_t maxVersion = 1;
            m_MeshConstantBuffer = ConstantBuffer::Create(sizeof(Mesh_GPUData), false, 1, "Per-Entity Transform Buffer");
            LOG_INFO("[MeshInstance] Created per-draw object buffer '{}' as volatile", m_Name.empty() ? "<unnamed>" : m_Name);
        }
    }

    StaticMeshInstance::StaticMeshInstance()
    {
        m_Primitive = CreateRef<MeshPrimitive<VertexMeshStatic>>();
    }

    StaticMeshInstance::~StaticMeshInstance()
    {
        m_Primitive.reset();
        m_MeshBindingSetCache.clear();
    }

    bool StaticMeshInstance::UpdateBindingSet(const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &sceneBuffer, const Ref<ConstantBuffer> &csmBuffer, const Ref<ConstantBuffer> &pointLightBuffer /*= nullptr*/, const Ref<ConstantBuffer> &spotLightBuffer /*= nullptr */)
    {
        if (!m_MeshConstantBuffer)
        {
            return false;
        }

        BindingSetCacheKey cacheKey{};
        cacheKey.cameraBuffer = cameraBuffer->GetHandle();
        cacheKey.objectBuffer = m_MeshConstantBuffer->GetHandle();
        cacheKey.sceneBuffer = sceneBuffer ? sceneBuffer->GetHandle() : nullptr;
        cacheKey.csmBuffer = csmBuffer ? csmBuffer->GetHandle() : nullptr;
        cacheKey.pointLightBuffer = pointLightBuffer ? pointLightBuffer->GetHandle() : nullptr;
        cacheKey.spotLightBuffer = spotLightBuffer ? spotLightBuffer->GetHandle() : nullptr;

        if (auto it = m_MeshBindingSetCache.find(cacheKey); it != m_MeshBindingSetCache.end())
        {
            m_MeshBindingSet = it->second;
            return true;
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        auto desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cacheKey.cameraBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, cacheKey.objectBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, cacheKey.sceneBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, cacheKey.csmBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, cacheKey.pointLightBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, cacheKey.spotLightBuffer));

        m_MeshBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(EBindingLayout::MESH_STATIC));
        LOG_ASSERT(m_MeshBindingSet, "Failed to create mesh binding set");

        if (m_MeshBindingSet)
        {
            m_MeshBindingSetCache.emplace(cacheKey, m_MeshBindingSet);
            if (m_MeshBindingSetCache.size() > 512)
            {
                LOG_WARN("[MeshInstance] Binding cache for '{}' grew to {} entries. Verify instance buffer sharing strategy.", m_Name, m_MeshBindingSetCache.size());
            }
        }

        return true;
    }

    Ref<StaticMeshInstance> StaticMeshInstance::Create(const MeshNode<VertexMeshStatic> &node, const Ref<MeshPrimitive<VertexMeshStatic>> &primitive)
    {
        return CreateRef<StaticMeshInstance>(node, primitive);
    }

    Ref<StaticMeshInstance> StaticMeshInstance::Create(const std::string &name, const Ref<MeshPrimitive<VertexMeshStatic>> &primitive)
    {
        return CreateRef<StaticMeshInstance>(name, primitive);
    }

    // ===================================
    // Skeletal Mesh Instance
    // ===================================
    SkeletalMeshInstance::SkeletalMeshInstance(const std::string &name, const Ref<MeshPrimitive<VertexMeshAnim>> &primitive)
    {
        m_Name = name;
        m_Primitive = primitive;

        constexpr uint32_t maxVersion = 1;
        if (!m_MeshConstantBuffer)
        {
            m_MeshConstantBuffer = ConstantBuffer::Create(sizeof(Mesh_GPUData), false, maxVersion, "Per-Entity Transform Buffer");
            LOG_INFO("[MeshInstance] Created per-draw object buffer '{}' as volatile", m_Name.empty() ? "<unnamed>" : m_Name);
        }

        if (!m_SkeletonBuffer)
        {
            m_SkeletonBuffer = ConstantBuffer::Create(sizeof(glm::mat4) * MAX_BONES, false, maxVersion, "Default Skeleton Buffer");
            LOG_INFO("[MeshInstance] Created skeleton buffer '{}' as volatile", m_Name.empty() ? "<unnamed>" : m_Name);
        }

    }

    SkeletalMeshInstance::SkeletalMeshInstance(const MeshNode<VertexMeshAnim> &node, const Ref<MeshPrimitive<VertexMeshAnim>> &primitive)
    {
        m_Name = node.name;
        m_Primitive = primitive;

        local = node.local;
        global = node.global;

        constexpr uint32_t maxVersion = 1;

        if (!m_MeshConstantBuffer)
        {
            m_MeshConstantBuffer = ConstantBuffer::Create(sizeof(Mesh_GPUData), false, maxVersion, "Per-Entity Transform Buffer");
            LOG_INFO("[MeshInstance] Created per-draw object buffer '{}' as volatile", m_Name.empty() ? "<unnamed>" : m_Name);
        }

        if (!m_SkeletonBuffer)
        {
            m_SkeletonBuffer = ConstantBuffer::Create(sizeof(glm::mat4) * MAX_BONES, false, maxVersion, "Default Skeleton Buffer");
            LOG_INFO("[MeshInstance] Created skeleton buffer '{}' as volatile", m_Name.empty() ? "<unnamed>" : m_Name);
        }
    }

    SkeletalMeshInstance::SkeletalMeshInstance()
    {
        m_Primitive = CreateRef<MeshPrimitive<VertexMeshAnim>>();
    }

    SkeletalMeshInstance::~SkeletalMeshInstance()
    {
        m_Primitive.reset();
        m_MeshBindingSetCache.clear();
    }

    void SkeletalMeshInstance::SetSkeletonData(nvrhi::ICommandList *cmd, void *data, size_t size)
    {
        m_SkeletonBuffer->SetData(cmd, Buffer(data, size));
    }

    bool SkeletalMeshInstance::UpdateBindingSet(const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &sceneBuffer, const Ref<ConstantBuffer> &csmBuffer,
        const Ref<ConstantBuffer> &pointLightBuffer, const Ref<ConstantBuffer> &spotLightBuffer
    )
    {
        if (!m_SkeletonBuffer || !m_MeshConstantBuffer)
        {
            return false;
        }

        BindingSetCacheKey cacheKey {};
        cacheKey.cameraBuffer = cameraBuffer->GetHandle();
        cacheKey.objectBuffer = m_MeshConstantBuffer->GetHandle();
        cacheKey.skeletonBuffer = m_SkeletonBuffer->GetHandle();
        cacheKey.sceneBuffer = sceneBuffer ? sceneBuffer->GetHandle() : nullptr;
        cacheKey.csmBuffer = csmBuffer ? csmBuffer->GetHandle() : nullptr;
        cacheKey.pointLightBuffer = pointLightBuffer ? pointLightBuffer->GetHandle() : nullptr;
        cacheKey.spotLightBuffer = spotLightBuffer ? spotLightBuffer->GetHandle() : nullptr;

        if (auto it = m_MeshBindingSetCache.find(cacheKey); it != m_MeshBindingSetCache.end())
        {
            m_MeshBindingSet = it->second;
            return true;
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        auto desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cacheKey.cameraBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, cacheKey.objectBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, cacheKey.skeletonBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, cacheKey.sceneBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(4, cacheKey.csmBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(5, cacheKey.pointLightBuffer));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(6, cacheKey.spotLightBuffer));

        m_MeshBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(EBindingLayout::MESH_ANIM));
        LOG_ASSERT(m_MeshBindingSet, "Failed to create mesh binding set");

        if (m_MeshBindingSet)
        {
            m_MeshBindingSetCache.emplace(cacheKey, m_MeshBindingSet);
            if (m_MeshBindingSetCache.size() > 512)
            {
                LOG_WARN("[MeshInstance] Binding cache for '{}' grew to {} entries. Verify instance buffer sharing strategy.", m_Name, m_MeshBindingSetCache.size());
            }
        }

        return true;
    }

    Ref<SkeletalMeshInstance> SkeletalMeshInstance::Create(const std::string &name, const Ref<MeshPrimitive<VertexMeshAnim>> &primitive)
    {
        return CreateRef<SkeletalMeshInstance>(name, primitive);
    }

    Ref<SkeletalMeshInstance> SkeletalMeshInstance::Create(const MeshNode<VertexMeshAnim> &node, const Ref<MeshPrimitive<VertexMeshAnim>> &primitive)
    {
        return CreateRef<SkeletalMeshInstance>(node, primitive);
    }

    
    // ===================================
    // Static Mesh
    // ===================================
    Ref<StaticMesh> StaticMesh::Create()
    {
        return CreateRef<StaticMesh>();
    }

    StaticMesh::~StaticMesh()
    {
        m_MeshInstances.clear();
    }

    bool StaticMesh::Serialize(const ignite::Path &filepath)
    {
        BinarySerializer::SerializeMesh<StaticMesh, VertexMeshStatic>(this, filepath);
        SetDirtyFlag(false);
        return true;
    }

    Ref<StaticMesh> StaticMesh::Deserialize(const ignite::Path &filepath)
    {
        return BinarySerializer::DeserializeMesh<StaticMesh, VertexMeshStatic>(filepath);
    }

    const AABB &StaticMesh::CalculateLocalAABB()
    {
        for (const auto &mesh : m_MeshInstances)
        {
            if (!mesh)
                continue;

            auto &prim = mesh->GetPrimitive();
            if (!prim || prim->vertices.empty())
            {
                continue;
            }

            AABB meshBounds;
            // Vertices are already transformed when the mesh is loaded, so
            // use vertex positions directly and do not apply the mesh local transform.
            for (const auto &vertex : prim->vertices)
            {
                meshBounds.min = glm::min(meshBounds.min, vertex.position);
                meshBounds.max = glm::max(meshBounds.max, vertex.position);
            }

            localAABB.min = glm::min(localAABB.min, meshBounds.min);
            localAABB.max = glm::max(localAABB.max, meshBounds.max);
        }

        return this->localAABB;
    }

    // ===================================
    // Skeletal Mesh
    // ===================================
    Ref<SkeletalMesh> SkeletalMesh::Create()
    {
        return CreateRef<SkeletalMesh>();
    }

    void SkeletalMesh::SetSkeleton(AssetHandle skeletonHandle)
    {
        m_SkeletonHandle = handle;
        AssetManager::GetInstance()->AddAssetPin(skeletonHandle, std::format("skeletalmesh.skeleton.{}.{}", (uint64_t)this->handle, (uint64_t)skeletonHandle));
    }

    void SkeletalMesh::SetAnimator(AssetHandle animatorHandle)
    {
        m_AnimatorHandle = handle;
        AssetManager::GetInstance()->AddAssetPin(animatorHandle, std::format("skeletalmesh.animator.{}.{}", (uint64_t)this->handle, (uint64_t)animatorHandle));
    }

    bool SkeletalMesh::Serialize(const ignite::Path &filepath)
    {
        BinarySerializer::SerializeMesh<SkeletalMesh, VertexMeshAnim>(this, filepath);
        SetDirtyFlag(false);
        return true;
    }

    Ref<SkeletalMesh> SkeletalMesh::Deserialize(const ignite::Path &filepath)
    {
        return BinarySerializer::DeserializeMesh<SkeletalMesh, VertexMeshAnim>(filepath);
    }

    const AABB &SkeletalMesh::CalculateLocalAABB()
    {
        for (const auto &mesh : m_MeshInstances)
        {
            if (!mesh)
                continue;

            auto &prim = mesh->GetPrimitive();
            if (!prim || prim->vertices.empty())
            {
                continue;
            }

            AABB meshBounds;
            // Vertices are already transformed when the mesh is loaded, so
            // use vertex positions directly and do not apply the mesh local transform.
            for (const auto &vertex : prim->vertices)
            {
                meshBounds.min = glm::min(meshBounds.min, vertex.position);
                meshBounds.max = glm::max(meshBounds.max, vertex.position);
            }

            localAABB.min = glm::min(localAABB.min, meshBounds.min);
            localAABB.max = glm::max(localAABB.max, meshBounds.max);
        }

        return this->localAABB;
    }

    SkeletalMesh::~SkeletalMesh()
    {
        AssetManager::GetInstance()->RemoveAssetPin(m_SkeletonHandle, std::format("skeletalmesh.skeleton.{}.{}", (uint64_t)this->handle, (uint64_t)m_SkeletonHandle));
        AssetManager::GetInstance()->RemoveAssetPin(m_AnimatorHandle, std::format("skeletalmesh.animator.{}.{}", (uint64_t)this->handle, (uint64_t)m_AnimatorHandle));
        m_MeshInstances.clear();
    }

    // ===================================
    // Mesh Loader
    // ===================================
    Ref<Material> GLTFMeshLoader::LoadMaterial(const tinygltf::Primitive &primitive, const std::vector<tinygltf::Material> &gltfMaterials,
        const std::vector<std::pair<std::string, Ref<Texture>>> &loadedTextures,std::array<MeshMaterialTextureMap, 5> &textureMap,
        const std::vector<nvrhi::SamplerDesc> &loadedSamplers, int *materialIndex)
    {
        Ref<Material> material;
        *materialIndex = -1;

        if (primitive.material >= 0 && primitive.material < gltfMaterials.size())
        {
            *materialIndex = primitive.material;

            const tinygltf::Material &gltfMaterial = gltfMaterials[primitive.material];
            LOG_TRACE("Loading material: {}", gltfMaterial.name);

            material = CreateRef<Material>();
            material->name = gltfMaterial.name;

            material->gpuData.baseColorFactor =
            {
                gltfMaterial.pbrMetallicRoughness.baseColorFactor[0],
                gltfMaterial.pbrMetallicRoughness.baseColorFactor[1],
                gltfMaterial.pbrMetallicRoughness.baseColorFactor[2],
                gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() > 3 ? static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[3]) : 1.0f
            };

            material->gpuData.emissiveFactor =
            {
                gltfMaterial.emissiveFactor[0],
                gltfMaterial.emissiveFactor[1],
                gltfMaterial.emissiveFactor[2],
                1.0f
            };

            material->gpuData.metallicFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.metallicFactor);
            material->gpuData.roughnessFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.roughnessFactor);
            material->gpuData.occlusionStrength = static_cast<float>(gltfMaterial.occlusionTexture.strength);

            // base color texture
            const int baseColorIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
            if (baseColorIndex >= 0 && baseColorIndex < loadedTextures.size())
            {
                textureMap[0] = { baseColorIndex, loadedTextures[baseColorIndex].first, loadedTextures[baseColorIndex].second };
            }
            else
            {
                textureMap[0] = { -1, "", nullptr };
            }

            // emissive texture
            const int emissiveIndex = gltfMaterial.emissiveTexture.index;
            if (emissiveIndex >= 0 && emissiveIndex < loadedTextures.size())
            {
                textureMap[1] = { emissiveIndex, loadedTextures[emissiveIndex].first, loadedTextures[emissiveIndex].second };
            }
            else
            {
                textureMap[1] = { -1, "", nullptr };
            }

            // metallic roughness texture
            const int metallicRoughnessIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (metallicRoughnessIndex >= 0 && metallicRoughnessIndex < loadedTextures.size())
            {
                textureMap[2] = { metallicRoughnessIndex, loadedTextures[metallicRoughnessIndex].first, loadedTextures[metallicRoughnessIndex].second };
            }
            else
            {
                textureMap[2] = { -1, "", nullptr };
            }

            // normal texture
            const int normalIndex = gltfMaterial.normalTexture.index;
            if (normalIndex >= 0 && normalIndex < loadedTextures.size())
            {
                textureMap[3] = { normalIndex, loadedTextures[normalIndex].first, loadedTextures[normalIndex].second };
            }
            else
            {
                textureMap[3] = { -1, "", nullptr };
            }

            // occlusion texture
            const int occlusionIndex = gltfMaterial.occlusionTexture.index;
            if (occlusionIndex >= 0 && occlusionIndex < loadedTextures.size())
            {
                textureMap[4] = { occlusionIndex, loadedTextures[occlusionIndex].first, loadedTextures[occlusionIndex].second };
            }
            else
            {
                textureMap[4] = { -1, "", nullptr };
            }

            if (!loadedSamplers.empty())
            {
                material->SetSamplerDesc(loadedSamplers[0]);
            }
        }

        return material;
    }

    template<MeshVertex VertexType_T>
    void GLTFMeshLoader::LoadVertexData(std::vector<VertexType_T> &vertices, const tinygltf::Primitive &primitive, const tinygltf::Model &model)
    {
        // Get vertex positions
        glm::vec3 *positions = nullptr;
        size_t positionCount = 0;

        if (primitive.attributes.contains("POSITION"))
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("POSITION")];
            positions = (glm::vec3 *)GetBufferData(model, accessor);
            positionCount = accessor.count;
        }

        // Get vertex normals (optional)
        glm::vec3 *normals = nullptr;
        if (primitive.attributes.contains("NORMAL"))
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("NORMAL")];
            normals = (glm::vec3 *)GetBufferData(model, accessor);
        }

        // Get vertex tangents (optional)
        glm::vec4 *tangents = nullptr;
        if (primitive.attributes.contains("TANGENT"))
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("TANGENT")];
            tangents = (glm::vec4 *)GetBufferData(model, accessor);
        }

        // Get texture coordinates (optional)
        glm::vec2 *texCoords = nullptr;
        if (primitive.attributes.contains("TEXCOORD_0"))
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
            texCoords = (glm::vec2 *)GetBufferData(model, accessor);
        }

        // Get vertex colors (optional)
        const unsigned char* colorData = nullptr;
        int colorComponentType = 0;
        int colorType = 0;
        size_t colorStride = 0;
        size_t colorCount = 0;
        if (primitive.attributes.contains("COLOR_0"))
        {
            const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("COLOR_0")];
            colorData = GetBufferData(model, accessor);
            colorComponentType = accessor.componentType;
            colorType = accessor.type;
            colorStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
            colorCount = accessor.count;
        }

        // Get joint indices and weights for animated vertices (optional)
        const unsigned char *jointData = nullptr;
        int jointComponentType = 0;
        size_t jointStride = 0;
        size_t jointCount = 0;
        const unsigned char *weightData = nullptr;
        int weightComponentType = 0;
        size_t weightStride = 0;
        size_t weightCount = 0;
        if constexpr (AnimatedVertex<VertexType_T>)
        {
            // Get joint indices (optional)
            if (primitive.attributes.contains("JOINTS_0"))
            {
                const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("JOINTS_0")];
                jointData = GetBufferData(model, accessor);
                jointComponentType = accessor.componentType;
                jointStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                jointCount = accessor.count;
            }

            // Get joint weights (optional)
            if (primitive.attributes.contains("WEIGHTS_0"))
            {
                const tinygltf::Accessor &accessor = model.accessors[primitive.attributes.at("WEIGHTS_0")];
                weightData = GetBufferData(model, accessor);
                weightComponentType = accessor.componentType;
                weightStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                weightCount = accessor.count;
            }
        }
        
        // Build vertices
        vertices.reserve(positionCount);
        for (size_t i = 0; i < positionCount; ++i)
        {
            VertexType_T vertex{};
            vertex.position = positions[i];

            if (normals)
            {
                vertex.normal = normals[i];
            }

            if (tangents)
            {
                vertex.tangent = glm::vec3(tangents[i]);
                vertex.bitangent = glm::cross(vertex.normal, vertex.tangent) * tangents[i].w;
            }
            else if (normals)
            {
                // Generate tangent space if not provided
                // Simple approach: assume UV-aligned tangent
                vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                vertex.bitangent = glm::cross(vertex.normal, vertex.tangent);
            }

            if (texCoords)
            {
                vertex.uv = texCoords[i];
            }

            // Load vertex color
            vertex.color = glm::vec4(1.0f);
            if (colorData && i < colorCount)
            {
                const unsigned char* ptr = colorData + i * colorStride;
                if (colorType == TINYGLTF_TYPE_VEC4)
                {
                    if (colorComponentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                    {
                        vertex.color = *reinterpret_cast<const glm::vec4*>(ptr);
                    }
                    else if (colorComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const auto b = reinterpret_cast<const uint8_t*>(ptr);
                        vertex.color = glm::vec4(b[0], b[1], b[2], b[3]) / 255.0f;
                    }
                    else if (colorComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const auto s = reinterpret_cast<const uint16_t*>(ptr);
                        vertex.color = glm::vec4(s[0], s[1], s[2], s[3]) / 65535.0f;
                    }
                }
                else if (colorType == TINYGLTF_TYPE_VEC3)
                {
                    if (colorComponentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                    {
                        const auto v3 = reinterpret_cast<const glm::vec3*>(ptr);
                        vertex.color = glm::vec4(*v3, 1.0f);
                    }
                    else if (colorComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const auto b = reinterpret_cast<const uint8_t*>(ptr);
                        vertex.color = glm::vec4(b[0], b[1], b[2], 255.0f) / 255.0f;
                    }
                    else if (colorComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const auto s = reinterpret_cast<const uint16_t*>(ptr);
                        vertex.color = glm::vec4(s[0], s[1], s[2], 65535.0f) / 65535.0f;
                    }
                }
            }

            // Load joint indices and weights for animated vertices
            if constexpr (AnimatedVertex<VertexType_T>)
            {
                // Load bone IDs and weights
                if (jointData && i < jointCount)
                {
                    const unsigned char *ptr = jointData + i * jointStride;
                    if (jointComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const auto b = reinterpret_cast<const uint8_t *>(ptr);
                        vertex.boneIDs[0] = b[0];
                        vertex.boneIDs[1] = b[1];
                        vertex.boneIDs[2] = b[2];
                        vertex.boneIDs[3] = b[3];
                    }
                    else if (jointComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const auto s = reinterpret_cast<const uint16_t *>(ptr);
                        vertex.boneIDs[0] = s[0];
                        vertex.boneIDs[1] = s[1];
                        vertex.boneIDs[2] = s[2];
                        vertex.boneIDs[3] = s[3];
                    }
                }

                if (weightData && i < weightCount)
                {
                    const unsigned char *ptr = weightData + i * weightStride;
                    if (weightComponentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                    {
                        const auto f = reinterpret_cast<const float *>(ptr);
                        vertex.weights[0] = f[0];
                        vertex.weights[1] = f[1];
                        vertex.weights[2] = f[2];
                        vertex.weights[3] = f[3];
                    }
                    else if (weightComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                    {
                        const auto b = reinterpret_cast<const uint8_t *>(ptr);
                        vertex.weights[0] = b[0] / 255.0f;
                        vertex.weights[1] = b[1] / 255.0f;
                        vertex.weights[2] = b[2] / 255.0f;
                        vertex.weights[3] = b[3] / 255.0f;
                    }
                    else if (weightComponentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                    {
                        const auto s = reinterpret_cast<const uint16_t *>(ptr);
                        vertex.weights[0] = s[0] / 65535.0f;
                        vertex.weights[1] = s[1] / 65535.0f;
                        vertex.weights[2] = s[2] / 65535.0f;
                        vertex.weights[3] = s[3] / 65535.0f;
                    }

                    float sum = vertex.weights[0] + vertex.weights[1] + vertex.weights[2] + vertex.weights[3];
                    if (sum > 0.0f)
                    {
                        vertex.weights[0] /= sum;
                        vertex.weights[1] /= sum;
                        vertex.weights[2] /= sum;
                        vertex.weights[3] /= sum;
                    }
                }
            }

            vertices.push_back(vertex);
        }
    }

    void GLTFMeshLoader::LoadIndicesData(std::vector<uint32_t> &indices, const tinygltf::Primitive &primitive, const tinygltf::Model &model)
    {
        if (primitive.indices >= 0)
        {
            const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
            const unsigned char *indexData = GetBufferData(model, indexAccessor);

            LOG_INFO("Found {} indices", indexAccessor.count);

            // Handle different index types
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const auto indexPtr = (uint16_t *)indexData;
                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    indices.push_back(indexPtr[i]);
                }
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                const auto indexPtr = (uint32_t *)indexData;
                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    indices.push_back(indexPtr[i]);
                }
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            {
                const auto indexPtr = indexData;
                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    indices.push_back(indexPtr[i]);
                }
            }
        }
    }

    template<MeshVertex VertexType_T>
    void GLTFMeshLoader::LoadSceneGraph(const std::string &filename, MeshScene<VertexType_T> &outScene, AssetManager *assetManager)
    {
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ok = false;
        if (filename.ends_with(".glb")) ok = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filename);
        else ok = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filename);

        if (!ok)
        {
            LOG_ASSERT(false, "[GLTF Loader] error: {}", err);
            return;
        }

        // pre-load textures and samplers
        const auto textures = LoadTextures(gltfModel);
        const auto samplers = GetSamplers(gltfModel);

        // preserve nodes
        outScene.nodes.resize(gltfModel.nodes.size());

        // build raw node relationships and local transforms
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node &node = gltfModel.nodes[i];

            MeshNode<VertexType_T> &meshNode = outScene.nodes[i];
            meshNode.name = node.name;
            meshNode.local = BuildNodeLocalMatrix(node);
            for (int c : node.children)
            {
                meshNode.children.push_back(c);
                outScene.nodes[c].parent = static_cast<int>(i);
            }
        }

        // identify roots from default scene when available
        if (!gltfModel.scenes.empty())
        {
            int sceneIndex = gltfModel.defaultScene;
            if (sceneIndex < 0 || sceneIndex >= (int)gltfModel.scenes.size())
            {
                sceneIndex = 0;
            }

            const tinygltf::Scene &scene = gltfModel.scenes[sceneIndex];
            outScene.roots.reserve(scene.nodes.size());
            for (int nodeIndex : scene.nodes)
            {
                if (nodeIndex >= 0 && nodeIndex < (int)outScene.nodes.size())
                {
                    outScene.roots.push_back(nodeIndex);
                }
            }
        }

        // fallback for glTF assets without scene information
        if (outScene.roots.empty())
        {
            for (size_t i = 0; i < outScene.nodes.size(); ++i)
            {
                if (outScene.nodes[i].parent < 0)
                {
                    outScene.roots.push_back(static_cast<int>(i));
                }
            }
        }

        // Compute initial global transforms for all nodes via DFS so they are available when processing meshes
        std::function<void(int, const glm::mat4 &)> computeGlobals = [&](const int nodeIndex, const glm::mat4 &parentGlobal)
        {
            MeshNode<VertexType_T> &node = outScene.nodes[nodeIndex];
            node.global = parentGlobal * node.local;
            for (const int c : node.children)
            {
                computeGlobals(c, node.global);
            }
        };

        for (const int root : outScene.roots)
        {
            computeGlobals(root, glm::mat4(1.0f));
        }

        // Load GLTF Skeleton & Animations
        if constexpr (SkeletalMeshVertex<VertexType_T>)
        {
            if (!gltfModel.skins.empty())
            {
                const tinygltf::Skin &skin = gltfModel.skins[0];
                Ref<Skeleton> skeleton = CreateRef<Skeleton>();

                std::vector<glm::mat4> invBindPose;
                if (skin.inverseBindMatrices >= 0)
                {
                    const tinygltf::Accessor &accessor = gltfModel.accessors[skin.inverseBindMatrices];
                    const unsigned char *bufferData = GetBufferData(gltfModel, accessor);
                    if (bufferData)
                    {
                        invBindPose.resize(accessor.count);
                        std::memcpy(invBindPose.data(), bufferData, accessor.count * sizeof(glm::mat4));
                    }
                }

                skeleton->joints.resize(skin.joints.size());
                std::unordered_map<int, int32_t> nodeToJoint;
                for (int i = 0; i < (int)skin.joints.size(); ++i)
                {
                    nodeToJoint[skin.joints[i]] = i;
                }

                for (int i = 0; i < (int)skin.joints.size(); ++i)
                {
                    int nodeIdx = skin.joints[i];
                    const tinygltf::Node &gltfNode = gltfModel.nodes[nodeIdx];
                    Joint &joint = skeleton->joints[i];
                    joint.id = i;
                    joint.name = gltfNode.name.empty() ? ("Joint_" + std::to_string(i)) : gltfNode.name;

                    joint.parentJointId = -1;
                    int parentNodeIdx = outScene.nodes[nodeIdx].parent;
                    if (parentNodeIdx >= 0 && nodeToJoint.contains(parentNodeIdx))
                    {
                        joint.parentJointId = nodeToJoint[parentNodeIdx];
                    }

                    joint.localTransform = BuildNodeLocalMatrix(gltfNode);
                    Transform::Decompose(joint.localTransform, joint.defaultTransform);

                    joint.globalTransform = glm::mat4(1.0f);
                    if (i < (int)invBindPose.size())
                    {
                        joint.inverseBindPose = invBindPose[i];
                    }
                    else
                    {
                        joint.inverseBindPose = glm::mat4(1.0f);
                    }

                    skeleton->nameToJointMap[joint.name] = joint.id;
                }
                outScene.skeleton = skeleton;

                // Load GLTF Animations
                for (size_t animIdx = 0; animIdx < gltfModel.animations.size(); ++animIdx)
                {
                    const auto &gltfAnim = gltfModel.animations[animIdx];
                    Ref<SkeletalAnimation> animation = CreateRef<SkeletalAnimation>();
                    animation->name = gltfAnim.name.empty() ? ("GLTFAnimation_" + std::to_string(animIdx)) : gltfAnim.name;
                    animation->ticksPerSeconds = 30.0f;

                    float maxTimeSec = 0.0f;
                    for (const auto &sampler : gltfAnim.samplers)
                    {
                        const tinygltf::Accessor &inputAccessor = gltfModel.accessors[sampler.input];
                        const unsigned char *inputData = GetBufferData(gltfModel, inputAccessor);
                        if (inputData && inputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                        {
                            const auto times = reinterpret_cast<const float*>(inputData);
                            for (size_t k = 0; k < inputAccessor.count; ++k)
                            {
                                maxTimeSec = std::max(maxTimeSec, times[k]);
                            }
                        }
                    }
                    animation->duration = maxTimeSec * animation->ticksPerSeconds;

                    bool hasChannelData = false;
                    for (int i = 0; i < (int)skin.joints.size(); ++i)
                    {
                        int nodeIdx = skin.joints[i];
                        AnimationChannel channel{};
                        bool jointHasKeys = false;

                        for (const auto &gltfChannel : gltfAnim.channels)
                        {
                            if (gltfChannel.target_node != nodeIdx)
                                continue;

                            const auto &gltfSampler = gltfAnim.samplers[gltfChannel.sampler];

                            const tinygltf::Accessor &inputAccessor = gltfModel.accessors[gltfSampler.input];
                            const unsigned char *inputData = GetBufferData(gltfModel, inputAccessor);
                            if (!inputData || inputAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
                                continue;

                            const auto times = reinterpret_cast<const float*>(inputData);
                            size_t keyCount = inputAccessor.count;

                            const tinygltf::Accessor &outputAccessor = gltfModel.accessors[gltfSampler.output];
                            const unsigned char *outputData = GetBufferData(gltfModel, outputAccessor);
                            if (!outputData)
                                continue;

                            jointHasKeys = true;
                            hasChannelData = true;

                            if (gltfChannel.target_path == "translation")
                            {
                                if (outputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && outputAccessor.type == TINYGLTF_TYPE_VEC3)
                                {
                                    const auto vals = reinterpret_cast<const glm::vec3*>(outputData);
                                    channel.translationKeys.frames.reserve(keyCount);
                                    for (size_t k = 0; k < keyCount; ++k)
                                    {
                                        const float timestamp = times[k] * animation->ticksPerSeconds;
                                        channel.translationKeys.AddFrame({ vals[k], timestamp });
                                    }
                                }
                            }
                            else if (gltfChannel.target_path == "rotation")
                            {
                                if (outputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && outputAccessor.type == TINYGLTF_TYPE_VEC4)
                                {
                                    const auto vals = reinterpret_cast<const glm::vec4*>(outputData);
                                    channel.rotationKeys.frames.reserve(keyCount);
                                    for (size_t k = 0; k < keyCount; ++k)
                                    {
                                        const float timestamp = times[k] * animation->ticksPerSeconds;
                                        glm::quat q(vals[k].w, vals[k].x, vals[k].y, vals[k].z);
                                        channel.rotationKeys.AddFrame({ q, timestamp });
                                    }
                                }
                            }
                            else if (gltfChannel.target_path == "scale")
                            {
                                if (outputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && outputAccessor.type == TINYGLTF_TYPE_VEC3)
                                {
                                    const auto vals = reinterpret_cast<const glm::vec3*>(outputData);
                                    channel.scaleKeys.frames.reserve(keyCount);
                                    for (size_t k = 0; k < keyCount; ++k)
                                    {
                                        const float timestamp = times[k] * animation->ticksPerSeconds;
                                        channel.scaleKeys.AddFrame({ vals[k], timestamp });
                                    }
                                }
                            }
                        }

                        if (jointHasKeys)
                        {
                            animation->channels[i] = std::move(channel);
                        }
                    }

                    if (hasChannelData)
                    {
                        outScene.animations.push_back(animation);
                    }
                }
            }
        }

        // load meshes referenced by nodes
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node &node = gltfModel.nodes[i];
            if (node.mesh < 0 || node.mesh >= (int)gltfModel.meshes.size())
                continue;

            const MeshNode<VertexType_T> &meshNode = outScene.nodes[i];

            const tinygltf::Mesh &gltfMesh = gltfModel.meshes[node.mesh];
            for (const auto &gltfPrim : gltfMesh.primitives)
            {
                std::vector<VertexType_T> vertices;
                std::vector<uint32_t> indices;

                // get vertices and indices
                LoadVertexData(vertices, gltfPrim, gltfModel);
                LoadIndicesData(indices, gltfPrim, gltfModel);

                if (!vertices.empty() && !indices.empty())
                {
                    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(meshNode.global)));
                    for (auto &vertex : vertices)
                    {
                        vertex.position  = glm::vec3(meshNode.global * glm::vec4(vertex.position, 1.0f));
                        vertex.normal    = glm::normalize(normalMatrix * vertex.normal);
                        vertex.tangent   = glm::normalize(normalMatrix * vertex.tangent);
                        vertex.bitangent = glm::normalize(normalMatrix * vertex.bitangent);
                    }
                }

                Ref<MeshPrimitive<VertexType_T>> primitive = MeshPrimitive<VertexType_T>::Create(vertices, indices);

                // material
                std::array<MeshMaterialTextureMap, 5> materialTextureMap{};

                int materialIndex = -1;
                Ref<Material> material = LoadMaterial(gltfPrim, gltfModel.materials, textures, materialTextureMap, samplers, &materialIndex);
                if (!material)
                {
                    material = CreateDefaultMaterial("DefaultMaterial");
                }

                // Since vertices are baked to model space, the mesh instance node local/global transforms should be identity.
                MeshNode<VertexType_T> instanceNode = meshNode;
                instanceNode.local = glm::mat4(1.0f);
                instanceNode.global = glm::mat4(1.0f);

                Ref<MeshInstanceFor<VertexType_T>> meshInstance;
                if constexpr (SkeletalMeshVertex<VertexType_T>)
                {
                    meshInstance = MeshInstanceFor<VertexType_T>::Create(instanceNode, primitive);

                    // Link unskinned meshes to matching joint if skeletal scene
                    const bool isSkinned = gltfPrim.attributes.contains("JOINTS_0");
                    if (!isSkinned && outScene.skeleton && !outScene.skeleton->joints.empty())
                    {
                        int32_t linkedJointIdx = -1;
                        auto &nameMap = outScene.skeleton->nameToJointMap;
                        if (nameMap.contains(meshNode.name))
                        {
                            linkedJointIdx = nameMap.at(meshNode.name);
                        }
                        else
                        {
                            int parentWalkIdx = meshNode.parent;
                            while (parentWalkIdx >= 0)
                            {
                                const std::string &pName = outScene.nodes[parentWalkIdx].name;
                                if (nameMap.contains(pName))
                                {
                                    linkedJointIdx = nameMap.at(pName);
                                    break;
                                }
                                parentWalkIdx = outScene.nodes[parentWalkIdx].parent;
                            }
                            if (linkedJointIdx < 0)
                            {
                                linkedJointIdx = 0;
                            }
                        }
                        meshInstance->linkedJointIndex = linkedJointIdx;
                    }
                }
                else
                {
                    meshInstance = MeshInstanceFor<VertexType_T>::Create(instanceNode, primitive);
                }

                outScene.nodes[i].meshes.push_back(meshInstance);
                outScene.flatMeshes.push_back(meshInstance);
                const int sceneMaterialIndex = static_cast<int>(outScene.materials.size());
                outScene.materials.push_back(material);
                outScene.materialTextureMap.push_back(materialTextureMap);

                // Assign Mesh and Material Index
                outScene.materialMap[static_cast<int>(outScene.flatMeshes.size()) - 1] = sceneMaterialIndex;
            }
        }

        // compute global transform via DFS
        std::function<void(int, const glm::mat4 &)> recurse = [&](const int nodeIndex, const glm::mat4 &parentGlobal)
        {
            MeshNode<VertexType_T> &node = outScene.nodes[nodeIndex];
            node.global = parentGlobal * node.local;
            for (const auto &m : node.meshes)
            {
                m->local = glm::mat4(1.0f);
                m->global = glm::mat4(1.0f);
            }

            for (const int c : node.children)
            {
                recurse(c, node.global);
            }
        };

        for (const int root : outScene.roots)
        {
            recurse(root, glm::mat4(1.0f));
        }
    }

    std::vector<std::pair<std::string, Ref<Texture>>> GLTFMeshLoader::LoadTextures(const tinygltf::Model &model)
    {
        std::vector<std::pair<std::string, Ref<Texture>>> gltfTextures;
        gltfTextures.resize(model.textures.size(), { "", nullptr });

        LOG_TRACE("Loading {} textures from glTF", model.textures.size());

        std::unordered_map<int, Ref<Texture>> loadedImages;

        for (size_t i = 0; i < model.textures.size(); ++i)
        {
            const tinygltf::Texture &gltfTexture = model.textures[i];

            if (gltfTexture.source >= 0 && gltfTexture.source < model.images.size())
            {
                const tinygltf::Image &image = model.images[gltfTexture.source];
                const std::string imageName = image.name.empty() ? std::format("embedded_texture_{}", gltfTexture.source) : image.name;

                Ref<Texture> texture = nullptr;
                if (loadedImages.contains(gltfTexture.source))
                {
                    texture = loadedImages[gltfTexture.source];
                }
                else
                {
                    LOG_TRACE(" Texture {}: {} ({}x{})", i, imageName, image.width, image.height);

                    TextureCreateInfo createInfo;
                    createInfo.width = image.width;
                    createInfo.height = image.height;
                    createInfo.flip = false;
                    createInfo.format = nvrhi::Format::RGBA8_UNORM;
                    createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
                    createInfo.keepInitialState = true;
                    createInfo.keepCpuData = true;
                    createInfo.deferGpuCreate = true;

                    if (!image.image.empty())
                    {
                        std::vector<uint8_t> data;
                        data.resize(image.image.size() * sizeof(uint8_t));
                        std::memcpy(data.data(), image.image.data(), data.size());

                        texture = Texture::Create(data, createInfo, nullptr);
                        LOG_TRACE(" Loaded embedded texture");

                        texture->PrepareUploadData(4);
                        Application::SubmitToRenderThread([texture]()
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
                    }
                    else if (!image.uri.empty())
                    {
                        LOG_ASSERT(false, "Not implemented yet!");
                    }

                    loadedImages[gltfTexture.source] = texture;
                }

                gltfTextures[i] = { imageName, texture };
            }
        }

        return gltfTextures;
    }

    std::vector<nvrhi::SamplerDesc> GLTFMeshLoader::GetSamplers(const tinygltf::Model &model)
    {
        std::vector<nvrhi::SamplerDesc> samplers;
        for (size_t i = 0; i < model.textures.size(); ++i)
        {
            const tinygltf::Texture &gltfTexture = model.textures[i];
            if (gltfTexture.source >= 0 && gltfTexture.source < model.images.size())
            {
                tinygltf::Sampler gltfSampler = model.samplers[gltfTexture.sampler];

                gltfSampler.minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
                nvrhi::SamplerDesc desc;
                desc.borderColor = nvrhi::Color(1.0f);

                switch (gltfSampler.wrapS)
                {
                case TINYGLTF_TEXTURE_WRAP_REPEAT:
                    desc.addressU = nvrhi::SamplerAddressMode::Repeat;
                    desc.addressV = nvrhi::SamplerAddressMode::Repeat;
                    desc.addressW = nvrhi::SamplerAddressMode::Repeat;
                    break;
                case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                    desc.addressU = nvrhi::SamplerAddressMode::ClampToEdge;
                    desc.addressV = nvrhi::SamplerAddressMode::ClampToEdge;
                    desc.addressW = nvrhi::SamplerAddressMode::ClampToEdge;
                    break;
                case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                    desc.addressU = nvrhi::SamplerAddressMode::MirroredRepeat;
                    desc.addressV = nvrhi::SamplerAddressMode::MirroredRepeat;
                    desc.addressW = nvrhi::SamplerAddressMode::MirroredRepeat;
                    break;
                }

                samplers.push_back(desc);
            }
        }

        return samplers;
    }

    const unsigned char *GLTFMeshLoader::GetBufferData(const tinygltf::Model &model, const tinygltf::Accessor &accessor)
    {
        const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
        return &buffer.data[accessor.byteOffset + bufferView.byteOffset];
    }

    template<MeshVertex VertexType_T>
    void FBXMeshLoader::LoadSceneGraph(const std::string &filename, MeshScene<VertexType_T> &outScene, AssetManager *assetManager, bool importSkeletonAndAnimations)
    {
        FbxManager *sdkManager = assetManager->GetOrCreateFbxSdkManager();
        if (!sdkManager)
        {
            LOG_ASSERT(false, "[FBX Loader] Failed to create FBX SDK Manager");
            return;
        }

        FbxImporter *importer = nullptr;
        FbxScene *fbxScene = nullptr;

        {
            std::unique_lock lock(assetManager->GetFbxSdkMutex());

            // Create FBX Importer
            importer = FbxImporter::Create(sdkManager, "");
            if (!importer->Initialize(filename.c_str(), -1, sdkManager->GetIOSettings()))
            {
                LOG_ERROR("[FBX Loader] Failed to create FBX Importer");

                importer->Destroy();
                return;
            }

            // Create FBX Scene
            fbxScene = FbxScene::Create(sdkManager, "FBXScene");
            if (!importer->Import(fbxScene))
            {
                LOG_ERROR("[FBX Loader] Failed to import {}", filename);

                importer->Destroy();
                fbxScene->Destroy();
                return;
            }
            importer->Destroy();

            const FbxAxisSystem targetAxisSystem = FbxAxisSystem::MayaYUp;
            const FbxAxisSystem sceneAxisSystem = fbxScene->GetGlobalSettings().GetAxisSystem();
            if (sceneAxisSystem != targetAxisSystem)
            {
                targetAxisSystem.ConvertScene(fbxScene);
            }

            const FbxSystemUnit targetUnit = FbxSystemUnit::m;
            const FbxSystemUnit sceneUnit = fbxScene->GetGlobalSettings().GetSystemUnit();

            const auto scaleFactor = (float)sceneUnit.GetConversionFactorTo(targetUnit);
            LOG_INFO("[FBX Loader] Unit scale factor: {} (source unit scale: {})", scaleFactor, sceneUnit.GetScaleFactor());

            {
                Timer timer;
                LOG_WARN("[FBX Loader] Triangulate geometry...");
                FbxGeometryConverter geometryConverter(sdkManager);
                geometryConverter.Triangulate(fbxScene, true);

                LOG_WARN("[FBX Loader] Triangulate geometry completed for {}s", timer.Elapsed());
            }

            JointLoader jointLoader;
            if (importSkeletonAndAnimations)
            {
                outScene.skeleton = LoadSkeleton(fbxScene, jointLoader, scaleFactor);
                LoadAnimations(fbxScene, outScene.skeleton, jointLoader.jointNodes, outScene.animations, scaleFactor);
            }
            else
            {
                outScene.skeleton = nullptr;
                outScene.animations.clear();
            }

            const ignite::Path sourceDir = ignite::Path(filename).parent_path();

            MaterialLoader materialLoader;
            FbxNode *rootNode = fbxScene->GetRootNode();
            if (rootNode)
            {
                for (int i = 0; i < rootNode->GetChildCount(); ++i)
                {
                    BuildNode(rootNode->GetChild(i), fbxScene, outScene, materialLoader, jointLoader, sourceDir, -1, glm::mat4(1.0f), scaleFactor, importSkeletonAndAnimations);
                }
            }

            fbxScene->Destroy();
        }
    }

    void FBXMeshLoader::LoadSkeletonOnly(const std::string &filename, Ref<Skeleton> &skeleton, AssetManager *assetManager)
    {
        if (!skeleton)
        {
            LOG_ERROR("[FBX Loader] Failed to load animation only: Skeleton is required");
            return;
        }

        FbxManager *sdkManager = assetManager->GetOrCreateFbxSdkManager();
        if (!sdkManager)
        {
            LOG_ASSERT(false, "[FBX Loader] Failed to create FBX SDK Manager");
            return;
        }

        {
            std::unique_lock lock(assetManager->GetFbxSdkMutex());

            // Create FBX Importer
            FbxImporter *importer = FbxImporter::Create(sdkManager, "");
            if (!importer->Initialize(filename.c_str(), -1, sdkManager->GetIOSettings()))
            {
                LOG_ERROR("[FBX Loader] Failed to create FBX Importer");

                importer->Destroy();
                return;
            }

            // Create FBX Scene
            FbxScene *fbxScene = FbxScene::Create(sdkManager, "FBXScene");
            if (!importer->Import(fbxScene))
            {
                LOG_ERROR("[FBX Loader] Failed to import {}", filename);

                importer->Destroy();
                fbxScene->Destroy();
                return;
            }
            importer->Destroy();

            const FbxSystemUnit targetUnit = FbxSystemUnit::m;
            const FbxSystemUnit sceneUnit = fbxScene->GetGlobalSettings().GetSystemUnit();
            const float scaleFactor = (float)sceneUnit.GetConversionFactorTo(targetUnit);

            JointLoader jointLoader;

            FbxNode *rootNode = fbxScene->GetRootNode();
            if (rootNode)
            {
                for (int i = 0; i < rootNode->GetChildCount(); ++i)
                {
                    SkeletonBuildHierarchy(rootNode->GetChild(i), skeleton, jointLoader, scaleFactor);
                }
            }

            if (skeleton->joints.empty())
            {
                skeleton.reset();
            }

            fbxScene->Destroy();
        }
    }

    void FBXMeshLoader::LoadAnimationsOnly(const std::string &filename, Ref<Skeleton> skeleton, std::vector<Ref<SkeletalAnimation>> &outAnimations, AssetManager *assetManager)
    {
        if (!skeleton)
        {
            LOG_ERROR("[FBX Loader] Failed to load animation only: Skeleton is required");
            return;
        }

        FbxManager *sdkManager = assetManager->GetOrCreateFbxSdkManager();
        if (!sdkManager)
        {
            LOG_ASSERT(false, "[FBX Loader] Failed to create FBX SDK Manager");
            return;
        }

        {
            std::unique_lock lock(assetManager->GetFbxSdkMutex());

            // Create FBX Importer
            FbxImporter *importer = FbxImporter::Create(sdkManager, "");
            if (!importer->Initialize(filename.c_str(), -1, sdkManager->GetIOSettings()))
            {
                LOG_ERROR("[FBX Loader] Failed to create FBX Importer");

                importer->Destroy();
                return;
            }

            // Create FBX Scene
            FbxScene *fbxScene = FbxScene::Create(sdkManager, "FBXScene");
            if (!importer->Import(fbxScene))
            {
                LOG_ERROR("[FBX Loader] Failed to import {}", filename);

                importer->Destroy();
                fbxScene->Destroy();
                return;
            }
            importer->Destroy();

            const FbxSystemUnit targetUnit = FbxSystemUnit::m;
            const FbxSystemUnit sceneUnit = fbxScene->GetGlobalSettings().GetSystemUnit();
            const float scaleFactor = (float)sceneUnit.GetConversionFactorTo(targetUnit);

            JointLoader jointLoader;

            FbxNode *rootNode = fbxScene->GetRootNode();
            if (rootNode)
            {
                for (int i = 0; i < rootNode->GetChildCount(); ++i)
                {
                    SkeletonBuildHierarchy(rootNode->GetChild(i), skeleton, jointLoader, scaleFactor);
                }
            }

            if (skeleton->joints.empty())
            {
                skeleton.reset();
            }

            LoadAnimations(fbxScene, skeleton, jointLoader.jointNodes, outAnimations, scaleFactor);

            fbxScene->Destroy();
        }
    }

    void FBXMeshLoader::ExtractBonesAndWeights(fbxsdk::FbxMesh *fbxMesh, const MeshNode<VertexMeshAnim> &meshNode, std::vector<FBXBoneInfluence> &controlPointInfluence, MeshScene<VertexMeshAnim> &outScene, JointLoader &jointLoader, float scaleFactor, bool importSkinningData)
    {
        if (!importSkinningData)
        {
            return;
        }

        for (int deformerIndex = 0; deformerIndex < fbxMesh->GetDeformerCount(FbxDeformer::eSkin); ++deformerIndex)
        {
            auto skin = static_cast<FbxSkin *>(fbxMesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
            if (!skin)
            {
                continue;
            }

            if (!outScene.skeleton)
            {
                outScene.skeleton = CreateRef<Skeleton>();
            }

            for (int clusterIndex = 0; clusterIndex < skin->GetClusterCount(); ++clusterIndex)
            {
                FbxCluster *cluster = skin->GetCluster(clusterIndex);
                if (!cluster)
                {
                    continue;
                }

                FbxNode *jointNode = cluster->GetLink();
                if (!jointNode)
                {
                    continue;
                }

                const int32_t jointId = SkeletonFindOrAddJoint(jointNode, outScene.skeleton, jointLoader, scaleFactor);
                if (jointId < 0)
                {
                    continue;
                }

                if (jointId >= MAX_BONES)
                {
                    static bool s_MaxBonesWarningPrinted = false;
                    if (!s_MaxBonesWarningPrinted)
                    {
                        LOG_WARN("[FBX Loader] Joint count exceeds MAX_BONES ({}) - extra joints will be ignored for skinning", MAX_BONES);
                        s_MaxBonesWarningPrinted = true;
                    }
                    continue;
                }

                FbxAMatrix meshBind;
                FbxAMatrix jointBind;

                cluster->GetTransformMatrix(meshBind);
                cluster->GetTransformLinkMatrix(jointBind);

                const glm::mat4 jointBindGlm = ScaleTranslation(ToGlmMatrix(jointBind), scaleFactor);
                const glm::mat4 invBindGlm = glm::inverse(jointBindGlm);
                glm::mat4 &existingInvBind = outScene.skeleton->joints[jointId].inverseBindPose;
                const bool hasExistingInvBind = !Mat4NearEqual(existingInvBind, glm::mat4(1.0f));
                if (hasExistingInvBind && !Mat4NearEqual(existingInvBind, invBindGlm, 0.001f))
                {
                    const glm::vec3 oldT = ExtractTranslation(existingInvBind);
                    const glm::vec3 newT = ExtractTranslation(invBindGlm);
                    LOG_WARN("[FBX SKIN DEBUG] InverseBind mismatch joint='{}' id={} meshNode='{}' oldT=({:.4f},{:.4f},{:.4f}) newT=({:.4f},{:.4f},{:.4f}) (keeping first bind pose)",
                        outScene.skeleton->joints[jointId].name, jointId,
                        meshNode.name, oldT.x, oldT.y, oldT.z, newT.x, newT.y, newT.z);
                }
                else
                {
                    existingInvBind = invBindGlm;
                }

                if (clusterIndex < 6)
                {
                    const glm::vec3 meshBindT = ExtractTranslation(ToGlmMatrix(meshBind));
                    const glm::vec3 jointBindT = ExtractTranslation(ToGlmMatrix(jointBind));
                    const glm::vec3 invBindT = ExtractTranslation(invBindGlm);
                    LOG_INFO("[FBX SKIN DEBUG] joint='{}' id={} cpInfluences={} meshBindT=({:.3f},{:.3f},{:.3f}) jointBindT=({:.3f},{:.3f},{:.3f}) invBindT=({:.3f},{:.3f},{:.3f})",
                        outScene.skeleton->joints[jointId].name, jointId,
                        cluster->GetControlPointIndicesCount(), meshBindT.x, meshBindT.y, meshBindT.z,
                        jointBindT.x, jointBindT.y, jointBindT.z, invBindT.x, invBindT.y, invBindT.z);
                }

                const int *controlPointIndices = cluster->GetControlPointIndices();
                const double *controlPointWeights = cluster->GetControlPointWeights();
                const int controlPointIndexCount = cluster->GetControlPointIndicesCount();

                for (int i = 0; i < controlPointIndexCount; ++i)
                {
                    const int controlPointIndex = controlPointIndices[i];
                    if (controlPointIndex < 0 || controlPointIndex >= static_cast<int>(controlPointInfluence.size()))
                    {
                        continue;
                    }

                    // Adding bone influence
                    auto &influence = controlPointInfluence[controlPointIndex];
                    const auto &weight = static_cast<float>(controlPointWeights[i]);
                    const auto boneId = static_cast<uint32_t>(jointId);

                    if (weight > 0.0f)
                    {
                        size_t minIndex = 0;
                        for (size_t i = 1; i < VERTEX_MAX_BONES; ++i)
                        {
                            if (influence.weights[i] < influence.weights[minIndex])
                            {
                                minIndex = i;
                            }
                        }

                        if (weight > influence.weights[minIndex])
                        {
                            influence.weights[minIndex] = weight;
                            influence.ids[minIndex] = boneId;
                        }
                    }
                }
            }
        }

        const bool hasSkinDeformer = fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
        if (importSkinningData && hasSkinDeformer)
        {
            // Normalize bone influence
            for (FBXMeshLoader::FBXBoneInfluence &influence : controlPointInfluence)
            {
                float total = 0.0f;
                for (float w : influence.weights)
                {
                    total += w;
                }

                if (total <= 0.000001f)
                {
                    continue;
                }

                const float inv = 1.0f / total;
                for (float &w : influence.weights)
                {
                    w *= inv;
                }
            }
        }
    }

    template<MeshVertex VertexType_T>
    void FBXMeshLoader::ProcessMeshGeometry(fbxsdk::FbxMesh *fbxMesh, const fbxsdk::FbxAMatrix &meshGeom, float scaleFactor, const std::vector<FBXBoneInfluence> &controlPointInfluence, bool importSkinningData, std::vector<VertexType_T> &vertices, std::vector<uint32_t> &indices)
    {
        const FbxVector4 *controlPoints = fbxMesh->GetControlPoints();
        FbxStringList uvSetNames;
        fbxMesh->GetUVSetNames(uvSetNames);

        FbxGeometryElementVertexColor *elementVertexColor = fbxMesh->GetElementVertexColor();

        const int polygonCount = fbxMesh->GetPolygonCount();
        vertices.reserve(static_cast<size_t>(polygonCount) * 3);
        indices.reserve(static_cast<size_t>(polygonCount) * 3);

        auto emitVertex = [&](const int polygonIndex, const int polygonVertexIndex)
        {
            const int controlPointIndex = fbxMesh->GetPolygonVertex(polygonIndex, polygonVertexIndex);
            if (controlPointIndex < 0)
            {
                return;
            }

            const FbxVector4 cp = controlPoints[controlPointIndex];
            const FbxVector4 transformedPosition = meshGeom.MultT(cp);

            VertexType_T vertex{};
            vertex.position =
            {
                static_cast<float>(transformedPosition[0]) * scaleFactor,
                static_cast<float>(transformedPosition[1]) * scaleFactor,
                static_cast<float>(transformedPosition[2]) * scaleFactor
            };

            FbxVector4 normal(0.0, 1.0, 0.0, 0.0);
            fbxMesh->GetPolygonVertexNormal(polygonIndex, polygonVertexIndex, normal);
            normal = meshGeom.MultR(normal);
            normal.Normalize();
            vertex.normal = glm::vec3(static_cast<float>(normal[0]), static_cast<float>(normal[1]), static_cast<float>(normal[2]));

            if (uvSetNames.GetCount() > 0)
            {
                FbxVector2 uv(0.0, 0.0);
                bool unmapped = false;
                fbxMesh->GetPolygonVertexUV(polygonIndex, polygonVertexIndex, uvSetNames[0], uv, unmapped);
                if (!unmapped)
                {
                    vertex.uv = glm::vec2(static_cast<float>(uv[0]), static_cast<float>(uv[1]));
                }
            }

            // Load vertex color
            vertex.color = glm::vec4(1.0f);
            if (elementVertexColor)
            {
                FbxColor fbxCol(1.0, 1.0, 1.0, 1.0);
                const int vertexId = fbxMesh->GetPolygonVertexIndex(polygonIndex) + polygonVertexIndex;
                if (elementVertexColor->GetMappingMode() == FbxGeometryElement::eByControlPoint)
                {
                    if (elementVertexColor->GetReferenceMode() == FbxGeometryElement::eDirect)
                    {
                        fbxCol = elementVertexColor->GetDirectArray().GetAt(controlPointIndex);
                    }
                    else if (elementVertexColor->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
                    {
                        const int colorIndex = elementVertexColor->GetIndexArray().GetAt(controlPointIndex);
                        fbxCol = elementVertexColor->GetDirectArray().GetAt(colorIndex);
                    }
                }
                else if (elementVertexColor->GetMappingMode() == FbxGeometryElement::eByPolygonVertex)
                {
                    if (elementVertexColor->GetReferenceMode() == FbxGeometryElement::eDirect)
                    {
                        fbxCol = elementVertexColor->GetDirectArray().GetAt(vertexId);
                    }
                    else if (elementVertexColor->GetReferenceMode() == FbxGeometryElement::eIndexToDirect)
                    {
                        const int colorIndex = elementVertexColor->GetIndexArray().GetAt(vertexId);
                        fbxCol = elementVertexColor->GetDirectArray().GetAt(colorIndex);
                    }
                }
                vertex.color = glm::vec4(static_cast<float>(fbxCol.mRed), static_cast<float>(fbxCol.mGreen), static_cast<float>(fbxCol.mBlue), static_cast<float>(fbxCol.mAlpha));
            }

            vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            vertex.bitangent = glm::cross(vertex.normal, vertex.tangent);

            if constexpr (std::is_same_v<VertexType_T, VertexMeshAnim>)
            {
                if (importSkinningData && controlPointIndex >= 0 && controlPointIndex < static_cast<int>(controlPointInfluence.size()))
                {
                    const FBXBoneInfluence &influence = controlPointInfluence[controlPointIndex];
                    for (size_t i = 0; i < VERTEX_MAX_BONES; ++i)
                    {
                        vertex.boneIDs[i] = influence.ids[i];
                        vertex.weights[i] = influence.weights[i];
                    }
                }
            }

            vertices.push_back(vertex);
            indices.push_back(static_cast<uint32_t>(vertices.size() - 1));
        };

        for (int polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
        {
            const int polygonSize = fbxMesh->GetPolygonSize(polygonIndex);
            if (polygonSize < 3)
            {
                continue;
            }

            emitVertex(polygonIndex, 0);
            for (int v = 1; v < polygonSize - 1; ++v)
            {
                emitVertex(polygonIndex, v);
                emitVertex(polygonIndex, v + 1);
            }
        }
    }

    void FBXMeshLoader::LinkUnskinnedMeshToSkeleton(fbxsdk::FbxNode *node, const MeshNode<VertexMeshAnim> &meshNode, Ref<SkeletalMeshInstance> &meshInstance, MeshScene<VertexMeshAnim> &outScene, fbxsdk::FbxScene *fbxScene)
    {
        if (outScene.skeleton && !outScene.skeleton->joints.empty())
        {
            int32_t linkedJointIdx = -1;
            auto &nameMap = outScene.skeleton->nameToJointMap;

            // 1. Direct name match: node name == joint name
            if (nameMap.contains(meshNode.name))
            {
                linkedJointIdx = nameMap.at(meshNode.name);
                LOG_INFO("[FBX SKIN DEBUG] Non-skinned '{}' linked to same-name joint index {}", meshNode.name, linkedJointIdx);
            }
            else
            {
                // 2. Walk up the FBX node hierarchy looking for a joint-named ancestor
                FbxNode *parentWalk = node->GetParent();
                FbxNode *fbxRootN = fbxScene->GetRootNode();
                while (parentWalk && parentWalk != fbxRootN)
                {
                    const char *pNameC = parentWalk->GetName();
                    if (pNameC)
                    {
                        const std::string pName = pNameC;
                        if (nameMap.contains(pName))
                        {
                            linkedJointIdx = nameMap.at(pName);
                            LOG_INFO("[FBX SKIN DEBUG] Non-skinned '{}' linked to ancestor joint '{}' index {}",
                                meshNode.name, pName, linkedJointIdx);
                            break;
                        }
                    }
                    parentWalk = parentWalk->GetParent();
                }

                // 3. Root fallback: attach to skeleton root (joint 0)
                if (linkedJointIdx < 0)
                {
                    linkedJointIdx = 0;
                    LOG_WARN("[FBX SKIN DEBUG] Non-skinned '{}' has no matching joint ancestor — falling back to skeleton root (joint 0)", meshNode.name);
                }
            }

            meshInstance->linkedJointIndex = linkedJointIdx;
        }
    }

    template<MeshVertex VertexType_T>
    int FBXMeshLoader::ProcessMaterialAndTextures(fbxsdk::FbxNode *node, MeshScene<VertexType_T> &outScene, MaterialLoader &materialLoader, const ignite::Path &sourceDir)
    {
        FbxSurfaceMaterial *fbxMaterial = node->GetMaterialCount() > 0 ? node->GetMaterial(0) : nullptr;
        int sceneMaterialIndex = -1;

        if (materialLoader.materialIndices.contains(fbxMaterial))
        {
            sceneMaterialIndex = materialLoader.materialIndices[fbxMaterial];
        }
        else
        {
            Ref<Material> material = CreateMaterialFromFBX(fbxMaterial);
            sceneMaterialIndex = static_cast<int>(outScene.materials.size());

            std::array<MeshMaterialTextureMap, 5> textureMap{};
            if (fbxMaterial)
            {
                TryLoadFBXTextureFromProperty(fbxMaterial, { "DiffuseColor", "Diffuse", "BaseColor" }, sourceDir, materialLoader, textureMap[0]);
                TryLoadFBXTextureFromProperty(fbxMaterial, { "EmissiveColor", "Emissive" }, sourceDir, materialLoader, textureMap[1]);
                TryLoadFBXTextureFromProperty(fbxMaterial, { "SpecularColor", "Specular", "Metalness" }, sourceDir, materialLoader, textureMap[2]);
                TryLoadFBXTextureFromProperty(fbxMaterial, { "NormalMap", "Bump", "Maya|TEX_normal_map" }, sourceDir, materialLoader, textureMap[3]);
                TryLoadFBXTextureFromProperty(fbxMaterial, { "AmbientOcclusion", "AO", "Occlusion" }, sourceDir, materialLoader, textureMap[4]);
            }

            outScene.materials.push_back(material);
            outScene.materialTextureMap.push_back(textureMap);
            materialLoader.materialIndices[fbxMaterial] = sceneMaterialIndex;
        }

        return sceneMaterialIndex;
    }

    template<MeshVertex VertexType_T>
    void FBXMeshLoader::BuildNode(FbxNode *node, FbxScene *fbxScene, MeshScene<VertexType_T> &outScene, MaterialLoader &materialLoader, JointLoader &jointLoader, const ignite::Path &sourceDir, int parentIdx, const glm::mat4 &parentGlobal, float scaleFactor, bool importSkinningData)
    {
        if (!node)
        {
            return;
        }

        const auto nodeIndex = static_cast<int>(outScene.nodes.size());
        outScene.nodes.emplace_back();

        MeshNode<VertexType_T> &meshNode = outScene.nodes[nodeIndex];
        meshNode.parent = parentIdx;
        meshNode.name = node->GetName() ? node->GetName() : "";
        meshNode.local = ScaleTranslation(ToGlmMatrix(node->EvaluateLocalTransform()), scaleFactor);
        meshNode.global = parentGlobal * meshNode.local;

        if (parentIdx >= 0)
        {
            outScene.nodes[parentIdx].children.push_back(nodeIndex);
        }
        else
        {
            outScene.roots.push_back(nodeIndex);
        }

        if (FbxMesh *fbxMesh = node->GetMesh())
        {
            std::vector<VertexType_T> vertices;
            std::vector<uint32_t> indices;

            FbxAMatrix meshGeom = BuildNodeGeometricMatrix(node);

            // Extract bone influences for skinning
            size_t influencedControlPoints = 0;
            std::vector<FBXBoneInfluence> controlPointInfluence;

            if constexpr (std::is_same_v<VertexType_T, VertexMeshAnim>)
            {
                controlPointInfluence.resize(static_cast<size_t>(fbxMesh->GetControlPointsCount()));

                const bool hasSkinDeformer = fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
                bool isSkinned = importSkinningData && hasSkinDeformer;
                LOG_INFO("[FBX SKIN DEBUG] Node='{}' parent='{}' mesh='{}' skinned={} deformers={} cpCount={}",
                    meshNode.name,
                    (parentIdx >= 0 ? outScene.nodes[parentIdx].name : std::string("<root>")),
                    (fbxMesh->GetName() ? std::string(fbxMesh->GetName()) : std::string("<unnamed>")),
                    isSkinned, fbxMesh->GetDeformerCount(FbxDeformer::eSkin),
                    fbxMesh->GetControlPointsCount());

                if (isSkinned)
                {
                    auto skin = static_cast<FbxSkin *>(fbxMesh->GetDeformer(0, FbxDeformer::eSkin));
                    if (skin && skin->GetClusterCount() > 0)
                    {
                        LOG_INFO("[FBX SKIN DEBUG] Node='{}' firstSkinClusters={}", meshNode.name, skin->GetClusterCount());
                    }
                }

                ExtractBonesAndWeights(fbxMesh, meshNode, controlPointInfluence, outScene, jointLoader, scaleFactor, importSkinningData);

                for (const FBXMeshLoader::FBXBoneInfluence &influence : controlPointInfluence)
                {
                    float wsum = 0.0f;
                    for (float w : influence.weights)
                    {
                        wsum += w;
                    }

                    if (wsum > 0.00001f)
                    {
                        influencedControlPoints++;
                    }
                }

                if (isSkinned)
                {
                    LOG_INFO("[FBX SKIN DEBUG] Node='{}' influencedCP={}/{}", meshNode.name, influencedControlPoints, controlPointInfluence.size());
                }

                ProcessMeshGeometry(fbxMesh, meshGeom, scaleFactor, controlPointInfluence, importSkinningData, vertices, indices);
            }
            else
            {
                ProcessMeshGeometry(fbxMesh, meshGeom, scaleFactor, {}, false, vertices, indices);
            }

            if (!vertices.empty() && !indices.empty())
            {
                {
                    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(meshNode.global)));
                    for (auto &vertex : vertices)
                    {
                        vertex.position  = glm::vec3(meshNode.global * glm::vec4(vertex.position, 1.0f));
                        vertex.normal    = glm::normalize(normalMatrix * vertex.normal);
                        vertex.tangent   = glm::normalize(normalMatrix * vertex.tangent);
                        vertex.bitangent = glm::normalize(normalMatrix * vertex.bitangent);
                    }
                    LOG_INFO("[FBX SKIN DEBUG] Node='{}' vertices pre-baked to model space", meshNode.name);
                }

                MeshNode instanceNode = meshNode;
                instanceNode.global = glm::mat4(1.0f); // Vertices are now in model space

                Ref<MeshPrimitive<VertexType_T>> primitive = MeshPrimitive<VertexType_T>::Create(vertices, indices);
                Ref<MeshInstanceFor<VertexType_T>> meshInstance = MeshInstanceFor<VertexType_T>::Create(instanceNode, primitive);

                if constexpr (std::is_same_v<VertexType_T, VertexMeshAnim>)
                {
                    if (influencedControlPoints == 0)
                    {
                        LinkUnskinnedMeshToSkeleton(node, meshNode, meshInstance, outScene, fbxScene);
                    }
                }

                outScene.nodes[nodeIndex].meshes.push_back(meshInstance);
                outScene.flatMeshes.push_back(meshInstance);

                int sceneMaterialIndex = ProcessMaterialAndTextures(node, outScene, materialLoader, sourceDir);

                outScene.materialMap[static_cast<int>(outScene.flatMeshes.size()) - 1] = sceneMaterialIndex;
            }
        }

        for (int i = 0; i < node->GetChildCount(); ++i)
        {
            BuildNode(node->GetChild(i), fbxScene, outScene, materialLoader, jointLoader, sourceDir, nodeIndex, meshNode.global, scaleFactor, importSkinningData);
        }
    }

    Ref<Skeleton> FBXMeshLoader::LoadSkeleton(fbxsdk::FbxScene *fbxScene, JointLoader &outJointResult, float scaleFactor)
    {
        if (!fbxScene)
        {
            return nullptr;
        }

        Ref<Skeleton> skeleton = CreateRef<Skeleton>();

        FbxNode *rootNode = fbxScene->GetRootNode();
        if (rootNode)
        {
            for (int i = 0; i < rootNode->GetChildCount(); ++i)
            {
                SkeletonBuildHierarchy(rootNode->GetChild(i), skeleton, outJointResult, scaleFactor);
            }
        }

        if (skeleton->joints.empty())
        {
            skeleton.reset();
        }

        return skeleton;
    }

    void FBXMeshLoader::LoadAnimations(fbxsdk::FbxScene *fbxScene, const Ref<Skeleton> &skeleton, JointMap &jointNodes, std::vector<Ref<SkeletalAnimation>> &outAnimations, float scaleFactor)
    {
        if (!fbxScene || !skeleton)
        {
            return;
        }

        const double frameRate = FbxTime::GetFrameRate(fbxScene->GetGlobalSettings().GetTimeMode());
        const float ticksPerSecond = frameRate > 0.0 ? static_cast<float>(frameRate) : 30.0f;

        for (int stackIndex = 0; stackIndex < fbxScene->GetSrcObjectCount<FbxAnimStack>(); ++stackIndex)
        {
            auto animStack = fbxScene->GetSrcObject<FbxAnimStack>(stackIndex);
            if (!animStack || animStack->GetMemberCount<FbxAnimLayer>() == 0)
            {
                continue;
            }

            auto layer = animStack->GetMember<FbxAnimLayer>(0);
            if (!layer)
            {
                continue;
            }

            // Ensure evaluator reads transforms from the current animation stack.
            fbxScene->SetCurrentAnimationStack(animStack);

            FbxTimeSpan timeSpan = animStack->GetLocalTimeSpan();
            const double startSeconds = timeSpan.GetStart().GetSecondDouble();
            const double endSeconds = timeSpan.GetStop().GetSecondDouble();

            Ref<SkeletalAnimation> animation = CreateRef<SkeletalAnimation>();
            animation->name = animStack->GetName() ? animStack->GetName() : "FBXAnimation";
            animation->ticksPerSeconds = ticksPerSecond;
            animation->duration = std::max(0.0f, static_cast<float>((endSeconds - startSeconds) * ticksPerSecond));

            for (size_t jointIndex = 0; jointIndex < skeleton->joints.size(); ++jointIndex)
            {
                const Joint &joint = skeleton->joints[jointIndex];
                if (!jointNodes.contains(joint.name))
                {
                    continue;
                }

                FbxNode *node = jointNodes[joint.name];
                std::set<FbxLongLong> keyTicks;

                auto collectTicks = [&keyTicks](FbxAnimCurve *curve)
                {
                    if (!curve)
                    {
                        return;
                    }

                    for (int keyIndex = 0; keyIndex < curve->KeyGetCount(); ++keyIndex)
                    {
                        keyTicks.insert(curve->KeyGetTime(keyIndex).Get());
                    }
                };

                collectTicks(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
                collectTicks(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
                collectTicks(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));

                collectTicks(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
                collectTicks(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
                collectTicks(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));

                collectTicks(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
                collectTicks(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
                collectTicks(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));

                // Some FBX files store animation in evaluator-driven transforms with sparse/no explicit curve keys.
                // In that case, sample uniformly across the clip range so channels are still populated.
                if (keyTicks.size() <= 1 && endSeconds > startSeconds)
                {
                    const int sampleCount = std::max(2, static_cast<int>(std::ceil((endSeconds - startSeconds) * ticksPerSecond)) + 1);
                    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
                    {
                        const double alpha = sampleCount > 1 ? static_cast<double>(sampleIndex) / static_cast<double>(sampleCount - 1) : 0.0;
                        const double sampleSeconds = startSeconds + (endSeconds - startSeconds) * alpha;

                        FbxTime sampleTime;
                        sampleTime.SetSecondDouble(sampleSeconds);
                        keyTicks.insert(sampleTime.Get());
                    }

                    LOG_INFO("[FBX Loader] Fallback-sampled clip '{}' joint '{}' with {} samples (sparse keys)",
                        animation->name, joint.name, sampleCount);
                }

                if (keyTicks.empty())
                {
                    continue;
                }

                AnimationChannel channel{};
                channel.translationKeys.frames.reserve(keyTicks.size());
                channel.rotationKeys.frames.reserve(keyTicks.size());
                channel.scaleKeys.frames.reserve(keyTicks.size());

                for (const FbxLongLong tick : keyTicks)
                {
                    FbxTime sampleTime;
                    sampleTime.Set(tick);

                    const float timestamp = static_cast<float>((sampleTime.GetSecondDouble() - startSeconds) * ticksPerSecond);
                    const glm::mat4 localMatrix = ToGlmMatrix(node->EvaluateLocalTransform(sampleTime));
                    
                    Transform decomposed;
                    Transform::Decompose(localMatrix, decomposed);
                    
                    channel.translationKeys.AddFrame({ decomposed.translation * scaleFactor, timestamp });
                    channel.rotationKeys.AddFrame({ decomposed.rotation, timestamp });
                    channel.scaleKeys.AddFrame({ decomposed.scale, timestamp });
                }

                animation->channels[(int)jointIndex] = std::move(channel);
            }

            if (!animation->channels.empty())
            {
                outAnimations.push_back(animation);
            }
        }
    }

    int32_t FBXMeshLoader::SkeletonFindOrAddJoint(fbxsdk::FbxNode *jointNode, const Ref<Skeleton> &skeleton, JointLoader &outJointResult, float scaleFactor)
    {
        if (!jointNode || !skeleton)
        {
            return -1;
        }

        const std::string jointName = jointNode->GetName() ? jointNode->GetName() : "";
        if (jointName.empty())
        {
            return -1;
        }

        if (outJointResult.jointNameToIndex.contains(jointName))
        {
            return outJointResult.jointNameToIndex[jointName];
        }

        int32_t parentJointId = -1;
        FbxNode *parentNode = jointNode->GetParent();
        if (parentNode && jointNode->GetScene() && parentNode != jointNode->GetScene()->GetRootNode())
        {
            parentJointId = SkeletonFindOrAddJoint(parentNode, skeleton, outJointResult, scaleFactor);
        }

        Joint joint{};
        joint.id = static_cast<int32_t>(skeleton->joints.size());
        joint.parentJointId = parentJointId;
        joint.name = jointName;
        joint.localTransform = ScaleTranslation(ToGlmMatrix(jointNode->EvaluateLocalTransform()), scaleFactor);

        Transform::Decompose(joint.localTransform, joint.defaultTransform);

        joint.globalTransform = glm::mat4(1.0f);
        joint.inverseBindPose = glm::mat4(1.0f);

        skeleton->nameToJointMap[joint.name] = joint.id;
        skeleton->joints.push_back(joint);
        outJointResult.jointNameToIndex[joint.name] = joint.id;

        outJointResult.jointNodes[joint.name] = jointNode;

        return joint.id;
    }

    void FBXMeshLoader::SkeletonBuildHierarchy(fbxsdk::FbxNode *node, const Ref<Skeleton> &skeleton, JointLoader &outJointResult, float scaleFactor)
    {
        if (!node)
        {
            return;
        }

        FbxNodeAttribute *attribute = node->GetNodeAttribute();
        if (attribute && attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
        {
            SkeletonFindOrAddJoint(node, skeleton, outJointResult, scaleFactor);
        }

        for (int i = 0; i < node->GetChildCount(); ++i)
        {
            SkeletonBuildHierarchy(node->GetChild(i), skeleton, outJointResult, scaleFactor);
        }
    }

    template<MeshVertex VertexType_T>
    void MeshLoader::LoadSceneGraph(const std::string &filename, MeshScene<VertexType_T> &outScene, AssetManager *assetManager)
    {
        const std::string extension = ToLowerCopy(ignite::Path(filename).extension().string());
        if (extension == ".fbx")
        {
            FBXMeshLoader::LoadSceneGraph<VertexType_T>(filename, outScene, assetManager);
        }
        else if (extension == ".gltf" || extension == ".glb")
        {
            GLTFMeshLoader::LoadSceneGraph<VertexType_T>(filename, outScene, assetManager);
        }
        else
        {
            LOG_ASSERT(false, "[Mesh Loader] Unknown mesh type!");
        }
    }

    template void MeshLoader::LoadSceneGraph<VertexMeshStatic>(const std::string &filename, MeshScene<VertexMeshStatic> &outScene, AssetManager *assetManager);
    template void MeshLoader::LoadSceneGraph<VertexMeshAnim>(const std::string &filename, MeshScene<VertexMeshAnim> &outScene, AssetManager *assetManager);
}
