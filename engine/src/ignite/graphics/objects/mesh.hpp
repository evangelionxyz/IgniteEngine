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

#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/math/aabb.hpp"
#include "material.hpp"

#include "../../thirdparty/TINYGLTF/include/tinygltf.h"

#include <nvrhi/nvrhi.h>
#include <filesystem>

namespace ignite {

    class Shader;
    class Environment;
    class GraphicsPipeline;
    class Scene;

    class Mesh
    {
    public:
        Mesh() = default;
        Mesh(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices);
    
        void UpdateBindingSet(Scene *scene);

        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;
        Ref<ConstantBuffer> skinnedBuffer;

        std::string name;
        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
        Ref<Material> material;
        int materialIndex = -1;

        nvrhi::BindingSetHandle GetBindingSet() { return m_BindingSet; }

    private:
        nvrhi::BindingSetHandle m_BindingSet;
    };

    // scene graph structures
    struct MeshNode
    {
        int parent = -1;
        std::string name;
        std::vector<int> children;
        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
        std::vector<Ref<Mesh>> meshes;
    };

    struct MeshScene
    {
        std::vector<MeshNode> nodes;
        std::vector<int> roots;
        std::vector<Ref<Mesh>> flatMeshes;
    };

    class MeshLoader
    {
    public:
        static void LoadMaterial(const Ref<Mesh>& mesh, const tinygltf::Primitive& primitive, const std::vector<tinygltf::Material>& materials, const std::vector<Ref<Texture>> &loadedTextures);
        static void LoadVertexData(std::vector<VertexMesh_Anim>& vertices, const tinygltf::Primitive& primitive, const tinygltf::Model& model);
        static void LoadIndicesData(std::vector<uint32_t>& indices, const tinygltf::Primitive& primitive, const tinygltf::Model& model);

        static MeshScene LoadSceneGraphFromGLTF(const std::string& filename);

    private:
        static std::vector<Ref<Texture>> LoadTexturesFromGLTF(const tinygltf::Model& model);
        static const unsigned char* GetBufferData(const tinygltf::Model& model, const tinygltf::Accessor& accessor);
    };
}
