#pragma once

#include "vertex_data.hpp"
#include "material.hpp"

#include "renderer.hpp"

#include "ignite/core/uuid.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/scene/entity.hpp"

#include <assimp/Importer.hpp>
#include <nvrhi/nvrhi.h>
#include <filesystem>

namespace ignite {

#define ASSIMP_IMPORTER_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices)

    class Shader;
    class Environment;
    class GraphicsPipeline;
    class Scene;

    struct NodeInfo
    {
        int id = -1;
        int parentID = -1;
        int materialIndex = -1;

        bool isJoint = false;
        
        UUID uuid = UUID(0);

        std::string name;
        glm::mat4 localTransform;
        glm::mat4 worldTransform;
        std::vector<int> childrenIDs;
        std::vector<int> meshIndices;  // Meshes owned by this node
    };

    struct BoneInfo
    {
        float weights[MAX_BONES] = { 0.0f };
        glm::mat4 offsetMatrix = glm::mat4(1.0f);
    };

    struct MeshData
    {
        std::vector<VertexMesh> vertices;
        std::vector<uint32_t> indices;
        int materialIndex = -1;
    };

    class Mesh
    {
    public:
        std::string name;
        MeshData data;

        // do not copy the buffer
        nvrhi::BufferHandle vertexBuffer;
        nvrhi::BufferHandle indexBuffer;

        uint32_t materialIndex = -1;
        int nodeParentID = -1;
        int nodeID = -1; // ID of the bone this mesh is attached to
        std::vector<BoneInfo> boneInfo; // Bone weights and indices
        std::unordered_map<std::string, uint32_t> boneMapping; // Maps bone name to indices

        AABB aabb;

        Mesh() = default;

        Mesh(const Mesh &other)
        {
            data = other.data;
            aabb = other.aabb;
            boneInfo = other.boneInfo;
            boneMapping = other.boneMapping;

            CreateBuffers();
        }

        void CreateBuffers();
        void WriteVertexBuffer(uint32_t entityID = -1);
    };
    
}