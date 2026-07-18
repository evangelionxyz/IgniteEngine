// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_MESH_HPP
#define IGN_MESH_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/path.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "material.hpp"

#include <tinygltf.h>
#include <nvrhi/nvrhi.h>
#include <unordered_map>

namespace fbxsdk
{
    class FbxNode;
    class FbxScene;
    class FbxMesh;
    class FbxAMatrix;
    class FbxSurfaceMaterial;
}

namespace ignite
{
    class StaticMeshInstance;
    class SkeletalMeshInstance;

    // Create a mapping between MeshVertex types and their corresponding MeshInstance types
    template<MeshVertex VertexType_T>
    struct MeshInstanceTraits;

    // Specializations for each MeshVertex type
    template<>
    struct MeshInstanceTraits<VertexMeshStatic>
    {
        using Type = StaticMeshInstance;
    };

    template<>
    struct MeshInstanceTraits<VertexMeshAnim>
    {
        using Type = SkeletalMeshInstance;
    };

    // Helper alias template to get the corresponding MeshInstance type for a given MeshVertex type
    template<MeshVertex VertexType_T>
    using MeshInstanceFor = typename MeshInstanceTraits<VertexType_T>::Type;

    class Scene;
    class Shader;
    class Skeleton;
    class Environment;
    class SkeletalMeshInstance;
    class GraphicsPipeline;
    class SkeletalAnimation;
    class AssetManager;

    // scene graph structures
    struct MeshMaterialTextureMap
    {
        int textureIndex = -1;
        std::string name;
        Ref<Texture> texture;
    };

    template<MeshVertex VertexType_T>
    struct IGN_API MeshNode
    {
        int parent = -1;
        std::string name;
        std::vector<int> children;
        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
        std::vector<Ref<MeshInstanceFor<VertexType_T>>> meshes;
    };

    template<MeshVertex VertexType_T>
        struct IGN_API MeshScene
    {
        MeshScene() = default;

        std::vector<MeshNode<VertexType_T>> nodes;
        std::vector<int> roots;
        std::vector<Ref<MeshInstanceFor<VertexType_T>>> flatMeshes;
        std::vector<Ref<Material>> materials;

        std::vector<std::array<MeshMaterialTextureMap, 5>> materialTextureMap;

        // Mesh to Material
        std::unordered_map<int, int> materialMap;

        Ref<Skeleton> skeleton;
        std::vector<Ref<SkeletalAnimation>> animations;
    };

    // Primitive Mesh
    template<MeshVertex VertexType_T>
    struct IGN_API MeshPrimitive
    {
        MeshPrimitive() = default;
        ~MeshPrimitive()
        {
            // Wait for GPU to ensure buffers are not in use
            if (auto *device = DeviceManager::GetInstance()->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(device);
            }

            // Clear GPU buffers
            vertexBuffer.reset();
            indexBuffer.reset();

            // Clear CPU data
            vertices.clear();
            indices.clear();
        }

        MeshPrimitive(const std::vector<VertexType_T> &vertices, const std::vector<uint32_t> &indices)
            : vertices(vertices), indices(indices)
        {
        }

        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;

        std::vector<VertexType_T> vertices;
        std::vector<uint32_t> indices;

        static Ref<MeshPrimitive> Create(const std::vector<VertexType_T> &vertices, const std::vector<uint32_t> &indices)
        {
            return CreateRef<MeshPrimitive>(vertices, indices);
        }

        void WriteBuffer(nvrhi::ICommandList *cmd)
        {
            if (vertices.empty() || indices.empty())
            {
                LOG_ASSERT(false, "[MeshPrimitive] Please validate buffers data");
                return;
            }

            if (!vertexBuffer)
                vertexBuffer = VertexBuffer::Create(sizeof(VertexType_T) * vertices.size());

            if (!indexBuffer)
                indexBuffer = IndexBuffer::Create(sizeof(uint32_t) * indices.size());

            vertexBuffer->SetData(cmd, Buffer((void *)vertices.data(), sizeof(VertexType_T) * vertices.size()));
            indexBuffer->SetData(cmd, Buffer((void *)indices.data(), sizeof(uint32_t) * indices.size()));
        }

        void ClearPrimitivesData()
        {
            vertices.clear();
            indices.clear();
        }
    };

    class IGN_API MeshInstance
    {
    public:
        MeshInstance() = default;
        virtual ~MeshInstance();

        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
        int32_t linkedJointIndex = -1;
        AABB localAABB;

        void SetName(const std::string &name) { m_Name = name; }
        void SetMaterial(const AssetHandle &assetHandle);
        
        const std::string &GetName() { return m_Name; }
        const AssetHandle &GetMaterialAssetHandle() const { return m_MaterialHandle; }

    protected:
        UUID m_UUID;
        std::string m_Name;
        AssetHandle m_MaterialHandle = AssetHandle(0);
    };

    class IGN_API StaticMeshInstance : public MeshInstance
    {
    public:
        StaticMeshInstance();
        virtual ~StaticMeshInstance() override;

        StaticMeshInstance(const MeshNode<VertexMeshStatic> &node, const Ref<MeshPrimitive<VertexMeshStatic>> &primitive);
        StaticMeshInstance(const std::string &name, const Ref<MeshPrimitive<VertexMeshStatic>> &primitive);

        static Ref<StaticMeshInstance> Create(const MeshNode<VertexMeshStatic> &node, const Ref<MeshPrimitive<VertexMeshStatic>> &primitive);
        static Ref<StaticMeshInstance> Create(const std::string &name, const Ref<MeshPrimitive<VertexMeshStatic>> &primitive);

        Ref<MeshPrimitive<VertexMeshStatic>> &GetPrimitive() { return m_Primitive; }

    private:
        Ref<MeshPrimitive<VertexMeshStatic>> m_Primitive;
    };

    class IGN_API SkeletalMeshInstance : public MeshInstance
    {
    public:
        SkeletalMeshInstance();
        virtual ~SkeletalMeshInstance() override;

        SkeletalMeshInstance(const MeshNode<VertexMeshAnim> &node, const Ref<MeshPrimitive<VertexMeshAnim>> &primitive);
        SkeletalMeshInstance(const std::string &name, const Ref<MeshPrimitive<VertexMeshAnim>> &primitive);

        static Ref<SkeletalMeshInstance> Create(const MeshNode<VertexMeshAnim> &node, const Ref<MeshPrimitive<VertexMeshAnim>> &primitive);
        static Ref<SkeletalMeshInstance> Create(const std::string &name, const Ref<MeshPrimitive<VertexMeshAnim>> &primitive);

        Ref<MeshPrimitive<VertexMeshAnim>> &GetPrimitive() { return m_Primitive; }
    private:
        Ref<MeshPrimitive<VertexMeshAnim>> m_Primitive;
    };

    class IGN_API Mesh : public Asset
    {
    public:
        virtual ~Mesh() = default;
        
        virtual const AABB &CalculateLocalAABB() = 0;

        static AssetType GetStaticType() { return AssetType::Mesh; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

        AABB localAABB;
    };

    class IGN_API StaticMesh : public Mesh
    {
    public:
        StaticMesh() = default;
        virtual ~StaticMesh() override;

        static Ref<StaticMesh> Create();
        static AssetType GetStaticType() { return AssetType::StaticMesh; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

        const std::vector<Ref<StaticMeshInstance>> &GetMeshInstances() const { return m_MeshInstances; }
        void SetMeshInstances(const std::vector<Ref<StaticMeshInstance>> &meshInstances) { m_MeshInstances = meshInstances; }
        void AddMeshInstance(const Ref<StaticMeshInstance> &meshInstance) { m_MeshInstances.push_back(meshInstance); }

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<StaticMesh> Deserialize(const ignite::Path &filepath);

        virtual const AABB &CalculateLocalAABB() override;
    private:
        std::vector<Ref<StaticMeshInstance>> m_MeshInstances;
    };

    class IGN_API SkeletalMesh : public Mesh 
    {
    public:
        SkeletalMesh() = default;
        virtual ~SkeletalMesh() override;

        static Ref<SkeletalMesh> Create();
        static AssetType GetStaticType() { return AssetType::SkeletalMesh; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

        const std::vector<Ref<SkeletalMeshInstance>> &GetMeshInstances() const { return m_MeshInstances; }
        void SetMeshInstances(const std::vector<Ref<SkeletalMeshInstance>> &meshInstances) { m_MeshInstances = meshInstances; }
        void AddMeshInstance(const Ref<SkeletalMeshInstance> &meshInstance) { m_MeshInstances.push_back(meshInstance); }

        void SetSkeleton(AssetHandle skeletonHandle);
        AssetHandle GetSkeletonHandle() const { return m_SkeletonHandle; }

        void SetAnimator(AssetHandle animatorHandle);
        AssetHandle GetAnimatorHandle() const { return m_AnimatorHandle; }

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<SkeletalMesh> Deserialize(const ignite::Path &filepath);

        virtual const AABB &CalculateLocalAABB() override;
    private:
        AssetHandle m_AnimatorHandle = AssetHandle(0);
        AssetHandle m_SkeletonHandle = AssetHandle(0);
        std::vector<Ref<SkeletalMeshInstance>> m_MeshInstances;
    };

    // ==== LOADERS ====

    class IGN_API GLTFMeshLoader
    {
    public:
        static Ref<Material> LoadMaterial(const tinygltf::Primitive& primitive, const std::vector<tinygltf::Material>& materials,
            const std::vector<std::pair<std::string, Ref<Texture>>> &loadedTextures, std::array<MeshMaterialTextureMap, 5> &textureMap,
            const std::vector<nvrhi::SamplerDesc> &loadedSamplers, int *materialIndex);
        
        template<MeshVertex VertexType_T>
        static void LoadVertexData(std::vector<VertexType_T>& vertices, const tinygltf::Primitive& primitive, const tinygltf::Model& model);
        
        static void LoadIndicesData(std::vector<uint32_t>& indices, const tinygltf::Primitive& primitive, const tinygltf::Model& model);

        // Generic loader
        template<MeshVertex VertexType_T>
        static void LoadSceneGraph(const std::string& filename, MeshScene<VertexType_T> &outScene, AssetManager *assetManager);

    private:
        static std::vector<std::pair<std::string, Ref<Texture>>> LoadTextures(const tinygltf::Model& model);
        static std::vector<nvrhi::SamplerDesc> GetSamplers(const tinygltf::Model& model);
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

        // Generic loader
        template<MeshVertex VertexType_T>
        static void LoadSceneGraph(const std::string &filename, MeshScene<VertexType_T> &outScene, AssetManager *assetManager, bool importSkeletonAndAnimations = true);
        
        static void LoadSkeletonOnly(const std::string &filename, Ref<Skeleton> &skeleton, AssetManager *assetManager);
        static void LoadAnimationsOnly(const std::string &filename, Ref<Skeleton> skeleton, std::vector<Ref<SkeletalAnimation>> &outAnimations, AssetManager *assetManager);

        template<MeshVertex VertexType_T>
        static void BuildNode(fbxsdk::FbxNode *node, fbxsdk::FbxScene *fbxScene, MeshScene<VertexType_T> &outscene, MaterialLoader &materialLoader, JointLoader &jointLoader, const ignite::Path &sourceDir, int parentIdx, const glm::mat4 &parentGlobal, float scaleFactor, bool importSkinningData = true);

        static Ref<Skeleton> LoadSkeleton(fbxsdk::FbxScene *fbxScene, JointLoader &outJointResult, float scaleFactor);
        static void LoadAnimations(fbxsdk::FbxScene *fbxScene, const Ref<Skeleton> &skeleton, JointMap &jointNodes, std::vector<Ref<SkeletalAnimation>> &outAnimations, float scaleFactor);

    private:
        static void SkeletonBuildHierarchy(fbxsdk::FbxNode *node, const Ref<Skeleton> &skeleton, JointLoader &outJointResult, float scaleFactor);
        static int32_t SkeletonFindOrAddJoint(fbxsdk::FbxNode *jointNode, const Ref<Skeleton> &skeleton, JointLoader &outJointResult, float scaleFactor);

        static void ExtractBonesAndWeights(fbxsdk::FbxMesh *fbxMesh, const MeshNode<VertexMeshAnim> &meshNode, std::vector<FBXBoneInfluence> &controlPointInfluence, MeshScene<VertexMeshAnim> &outScene, JointLoader &jointLoader, float scaleFactor, bool importSkinningData);
        template<MeshVertex VertexType_T>
        static void ProcessMeshGeometry(fbxsdk::FbxMesh *fbxMesh, const fbxsdk::FbxAMatrix &meshGeom, float scaleFactor, const std::vector<FBXBoneInfluence> &controlPointInfluence, bool importSkinningData, std::vector<VertexType_T> &vertices, std::vector<uint32_t> &indices);
        static void LinkUnskinnedMeshToSkeleton(fbxsdk::FbxNode *node, const MeshNode<VertexMeshAnim> &meshNode, Ref<SkeletalMeshInstance> &meshInstance, MeshScene<VertexMeshAnim> &outScene, fbxsdk::FbxScene *fbxScene);
        
        template<MeshVertex VertexType_T>
        static int ProcessMaterialAndTextures(fbxsdk::FbxNode *node, MeshScene<VertexType_T> &outScene, MaterialLoader &materialLoader, const ignite::Path &sourceDir);
    };

    class IGN_API MeshLoader
    {
    public:
        template<MeshVertex VertexType_T>
        static void LoadSceneGraph(const std::string &filename, MeshScene<VertexType_T> &outScene, AssetManager *assetManager);
    };
}

#endif
