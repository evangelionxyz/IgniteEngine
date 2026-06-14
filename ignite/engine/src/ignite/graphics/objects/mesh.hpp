// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_MESH_HPP
#define IGN_MESH_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/path.hpp"
#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/scene/scene.hpp"
#include "material.hpp"

#include <tinygltf.h>
#include <nvrhi/nvrhi.h>
#include <unordered_map>

namespace fbxsdk
{
    class FbxNode;
    class FbxScene;
    class FbxSurfaceMaterial;
}

namespace ignite
{
    class Scene;
    class Shader;
    class Skeleton;
    class Environment;
    class MeshInstance;
    class GraphicsPipeline;
    class SkeletalAnimation;

    // scene graph structures
    struct IGN_API MeshNode
    {
        int parent = -1;
        std::string name;
        std::vector<int> children;
        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
        std::vector<Ref<MeshInstance>> meshes;
    };

    struct IGN_API MeshScene
    {
        MeshScene() = default;

        std::vector<MeshNode> nodes;
        std::vector<int> roots;
        std::vector<Ref<MeshInstance>> flatMeshes;
        std::vector<Ref<Material>> materials;
        AABB aabb;

        struct MaterialTextureMap
        {
            int textureIndex = -1;
            std::string name;
            Ref<Texture> texture;
        };

        std::vector<std::array<MaterialTextureMap, 5>> materialTextureMap;

        // Mesh to Material
        std::unordered_map<int, int> materialMap;

        Ref<Skeleton> skeleton;
        std::vector<Ref<SkeletalAnimation>> animations;
    };

    // Primitive Mesh
    struct IGN_API MeshPrimitive
    {
        MeshPrimitive() = default;
        ~MeshPrimitive();

        MeshPrimitive(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices);

        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;

        std::vector<VertexMesh_Anim> vertices;
        std::vector<uint32_t> indices;

        static Ref<MeshPrimitive> Create(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices);

        void WriteBuffer(nvrhi::ICommandList *cmd);
        void ClearPrimitivesData();
    };

    class IGN_API MeshInstance
    {
    public:
        MeshInstance();
        ~MeshInstance();

        MeshInstance(const MeshNode &node, const Ref<MeshPrimitive> &mesh);
        MeshInstance(const std::string &name, const Ref<MeshPrimitive> &mesh);

        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);

        int32_t linkedJointIndex = -1;

        void SetName(const std::string &name) { m_Name = name; }
        std::string &GetName() { return m_Name; }

        void SetMaterial(AssetHandle assetHandle);
        AssetHandle GetMaterialHandle() const { return m_MaterialHandle; }

        static Ref<MeshInstance> Create(const MeshNode &node, const Ref<MeshPrimitive> &mesh);
        static Ref<MeshInstance> Create(const std::string &name, const Ref<MeshPrimitive> &mesh);
        static void ReleaseGlobalResources();

        Ref<MeshPrimitive> &GetPrimitive() { return m_Primitive; }

        void SetData(nvrhi::ICommandList *cmd, void *data, size_t size);
        void EnsureBuffer(nvrhi::ICommandList *cmd, const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &sceneBuffer, const Ref<ConstantBuffer> &csmBuffer, const Ref<ConstantBuffer> &skeletonBuffer);

        nvrhi::BindingSetHandle GetBindingSet() const { return m_MeshBindingSet; }
        Ref<ConstantBuffer> GetConstantBuffer() { return m_MeshConstantBuffer; }

    private:
        struct BindingSetCacheKey
        {
            nvrhi::IBuffer *cameraBuffer = nullptr;
            nvrhi::IBuffer *objectBuffer = nullptr;
            nvrhi::IBuffer *skeletonBuffer = nullptr;
            nvrhi::IBuffer *sceneBuffer = nullptr;
            nvrhi::IBuffer *csmBuffer = nullptr;

            bool operator==(const BindingSetCacheKey &other) const noexcept
            {
                return cameraBuffer == other.cameraBuffer
                    && objectBuffer == other.objectBuffer
                    && skeletonBuffer == other.skeletonBuffer
                    && sceneBuffer == other.sceneBuffer
                    && csmBuffer == other.csmBuffer;
            }
        };

        struct BindingSetCacheKeyHash
        {
            size_t operator()(const BindingSetCacheKey &k) const noexcept
            {
                size_t h = std::hash<const void *>{}(k.cameraBuffer);
                h ^= (std::hash<const void *>{}(k.objectBuffer) + 0x9e3779b9 + (h << 6) + (h >> 2));
                h ^= (std::hash<const void *>{}(k.skeletonBuffer) + 0x9e3779b9 + (h << 6) + (h >> 2));
                h ^= (std::hash<const void *>{}(k.sceneBuffer) + 0x9e3779b9 + (h << 6) + (h >> 2));
                h ^= (std::hash<const void *>{}(k.csmBuffer) + 0x9e3779b9 + (h << 6) + (h >> 2));
                return h;
            }
        };

        Ref<ConstantBuffer> m_MeshConstantBuffer; // SkinnedMesh_GPUData
        nvrhi::BindingSetHandle m_MeshBindingSet;
        std::unordered_map<BindingSetCacheKey, nvrhi::BindingSetHandle, BindingSetCacheKeyHash> m_MeshBindingSetCache;
        std::string m_Name;
        Ref<MeshPrimitive> m_Primitive;
        AssetHandle m_MaterialHandle = AssetHandle(0);
    };

    class IGN_API Mesh : public Asset
    {
    public:
        Mesh() = default;
        virtual ~Mesh();

        static Ref<Mesh> Create();
        static AssetType GetStaticType() { return AssetType::Mesh; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

        const std::vector<Ref<MeshInstance>> &GetMeshInstances() const { return m_MeshInstances; }
        void SetMeshInstance(const std::vector<Ref<MeshInstance>> &meshInstances) { m_MeshInstances = meshInstances; }
        void AddMeshInstance(const Ref<MeshInstance> &meshInstance) { m_MeshInstances.push_back(meshInstance); }

        void SetSkeleton(AssetHandle handle) { m_SkeletonHandle = handle; }
        AssetHandle GetSkeletonHandle() const { return m_SkeletonHandle; }

        void SetAnimator(AssetHandle handle) { m_AnimatorHandle = handle; }
        AssetHandle GetAnimatorHandle() const { return m_AnimatorHandle; }

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<Mesh> Deserialize(const ignite::Path &filepath);

        AABB aabb;
    private:
        AssetHandle m_AnimatorHandle = AssetHandle(0);
        AssetHandle m_SkeletonHandle = AssetHandle(0);
        std::vector<Ref<MeshInstance>> m_MeshInstances;
    };

    class IGN_API GLTFMeshLoader
    {
    public:
        static Ref<Material> LoadMaterial(const tinygltf::Primitive& primitive, const std::vector<tinygltf::Material>& materials,
            const std::vector<std::pair<std::string, Ref<Texture>>> &loadedTextures, std::array<MeshScene::MaterialTextureMap, 5> &textureMap, const std::vector<nvrhi::SamplerDesc> &loadedSamplers, int *materialIndex);
        static void LoadVertexData(std::vector<VertexMesh_Anim>& vertices, const tinygltf::Primitive& primitive, const tinygltf::Model& model);
        static void LoadIndicesData(std::vector<uint32_t>& indices, const tinygltf::Primitive& primitive, const tinygltf::Model& model);

        static void LoadSceneGraphFromGLTF(const std::string& filename, MeshScene &outScene);

    private:
        static std::vector<std::pair<std::string, Ref<Texture>>> LoadTexturesFromGLTF(const tinygltf::Model& model);
        static std::vector<nvrhi::SamplerDesc> GetSamplersFromGLTF(const tinygltf::Model& model);
        static const unsigned char* GetBufferData(const tinygltf::Model& model, const tinygltf::Accessor& accessor);
    };

    class IGN_API FBXMeshLoader
    {
    public:

        using JointMap = std::unordered_map<std::string, fbxsdk::FbxNode *>;
        using JointIdxMap = std::unordered_map<std::string, int32_t>;

        struct JointLoader
        {
            JointMap jointNodes;
            JointIdxMap jointNameToIndex;
        };

        struct MaterialLoader
        {
            std::vector<Ref<Texture>> loadedTextures;
            std::unordered_map<fbxsdk::FbxSurfaceMaterial *, int> materialIndices;
            std::unordered_map<std::string, int> textureLookup;
        };

		struct FBXBoneInfluence
		{
			std::array<uint32_t, VERTEX_MAX_BONES> ids = { 0, 0, 0, 0 };
			std::array<float, VERTEX_MAX_BONES> weights = { 0.0f, 0.0f, 0.0f, 0.0f };
		};

        static void LoadSceneGraphFromFBX(const std::string &filename, MeshScene &outScene, AssetManager *assetManager, bool importSkeletonAndAnimations = true);
        static void LoadSkeletonOnlyFromFBX(const std::string &filename, Ref<Skeleton> &skeleton, AssetManager *assetManager);
        static void LoadAnimationsOnlyFromFBX(const std::string &filename, Ref<Skeleton> skeleton, std::vector<Ref<SkeletalAnimation>> &outAnimations, AssetManager *assetManager);

        static void BuildNode(fbxsdk::FbxNode *node, fbxsdk::FbxScene *fbxScene, MeshScene &outscene, MaterialLoader &materialLoader, JointLoader &jointLoader, const ignite::Path &sourceDir, int parentIdx, const glm::mat4 &parentGlobal, float scaleFactor, bool importSkinningData = true);

        static Ref<Skeleton> LoadSkeletonFBX(fbxsdk::FbxScene *fbxScene, JointLoader &outJointResult, float scaleFactor);
        static void LoadAnimationsFBX(fbxsdk::FbxScene *fbxScene, const Ref<Skeleton> &skeleton, JointMap &jointNodes, std::vector<Ref<SkeletalAnimation>> &outAnimations, float scaleFactor);

    private:
        static void SkeletonBuildHierarchy(fbxsdk::FbxNode *node, const Ref<Skeleton> &skeleton, JointLoader &outJointResult, float scaleFactor);
        static int32_t SkeletonFindOrAddJoint(fbxsdk::FbxNode *jointNode, const Ref<Skeleton> &skeleton, JointLoader &outJointResult, float scaleFactor);
    };

    class IGN_API MeshLoader
    {
    public:
        static void LoadSceneGraph(const std::string &filename, MeshScene &outScene, AssetManager *assetManager);
    };
}

#endif
