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

#pragma once

#include "vertex_buffer.hpp"
#include "index_buffer.hpp"
#include "vertex_data.hpp"
#include "material.hpp"
#include "renderer.hpp"
#include "constant_buffer.hpp"

#include "ignite/core/uuid.hpp"
#include "ignite/math/aabb.hpp"

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
        int materialIndex = -1; // maybe should not be serialized

        bool isJoint = false;
        
        UUID uuid = UUID(0);

        std::string name;
        glm::mat4 localTransform;
        glm::mat4 worldTransform; // should not be serialized
        std::vector<int> childrenIDs;
        std::vector<int> meshIndices;  // Meshes owned by this node
    };

    struct MeshData
    {
        int meshIndex = -1;
        int materialIndex = -1;
        int nodeParentID = -1;
        int nodeID = -1; // ID of the bone this mesh is attached to

        std::string name;
        std::vector<VertexMesh_Anim> vertices;
        std::vector<uint32_t> indices;
        AABB aabb;
    };

    class Mesh
    {
    public:
        Mesh() = default;

        Mesh(const Mesh &other)
        {
            data = other.data;
            CreateBuffers();
        }

        void CreateBuffers();

        Ref<VertexBuffer> GetVertexBuffer() { return m_VertexBuffer; }
        Ref<IndexBuffer> GetIndexBuffer() { return m_IndexBuffer; }

        uint32_t GetIndicesCount() const
        {
            return static_cast<uint32_t>(data.indices.size());
        }
        
        MeshData data;

    private:
        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
    };

    struct MeshInstance
    {
        Mesh mesh;
        Ref<Material> material;
        SkinnedMeshBuffer skinBuffer;
        Ref<ConstantBuffer> constantBuffer;
        nvrhi::BindingSetHandle bindingSet = nullptr;

        MeshInstance() = default;

        void UpdateBindingSet();
        void SetMaterial(const Ref<Material> &mat);
    };

    // Skeletal Mesh Asset
    class MeshAsset : public Asset
    {
    public:
        std::vector<NodeInfo> nodes;
        std::vector<MeshData> meshesData;
        std::vector<Ref<Material>> materials;

        std::vector<Ref<MeshInstance>> Create();

        static AssetType GetStaticType() { return AssetType::SkeletalMesh; }
        virtual AssetType GetType() override { return GetStaticType(); }
    };
}
