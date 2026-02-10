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

namespace ignite
{

    class Shader;
    class Environment;
    class GraphicsPipeline;
    class Scene;

    // Primitive Mesh
    struct MeshPrimitive
    {
        MeshPrimitive() = default;
        ~MeshPrimitive();

        MeshPrimitive(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices);
        
        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;

        std::vector<VertexMesh_Anim> vertices;
        std::vector<uint32_t> indices;

        static Ref<MeshPrimitive> Create(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices);

        void CreateBuffer(nvrhi::ICommandList *cmd);
        void ClearPrimitivesData();
    };

    class MeshInstance
    {
    public:
        MeshInstance();
        ~MeshInstance();

        MeshInstance(const std::string &name, const Ref<MeshPrimitive> &mesh);

        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);

        void SetName(const std::string &name) { m_Name = name; }
        void SetMaterial(AssetHandle assetHandle);

        static Ref<MeshInstance> Create(const std::string &name, const Ref<MeshPrimitive> &mesh);

        Ref<MeshPrimitive> &GetPrimitive() { return m_Primitive; }
        std::string &GetName() { return m_Name; }
    
        AssetHandle GetMaterialHandle() const { return m_MaterialHandle; }

    private:
        std::string m_Name;
        Ref<MeshPrimitive> m_Primitive;
        Ref<ConstantBuffer> m_SkinnedMeshGPUDataBuffer;
        AssetHandle m_MaterialHandle = AssetHandle(0);
    };

    // scene graph structures
    struct MeshNode
    {
        int parent = -1;
        std::string name;
        std::vector<int> children;
        glm::mat4 local = glm::mat4(1.0f);
        glm::mat4 global = glm::mat4(1.0f);
        std::vector<Ref<MeshInstance>> meshes;
    };

    struct MeshScene
    {
        MeshScene() = default;

        std::vector<MeshNode> nodes;
        std::vector<int> roots;
        std::vector<Ref<MeshInstance>> flatMeshes;
        std::vector<Ref<Material>> materials;

        struct MaterialTextureMap
        {
            int textureIndex = -1;
            Ref<Texture> texture;
        };

        std::vector<std::array<MaterialTextureMap, 5>> materialTextureMap;

        // Mesh to Material
        std::unordered_map<int, int> materialMap;

    };

	class StaticMesh : public Asset
	{
	public:
		StaticMesh() = default;
		virtual ~StaticMesh();

		static Ref<StaticMesh> Create();
		static AssetType GetStaticType() { return AssetType::StaticMesh; }
		virtual AssetType GetAssetType() const { return GetStaticType(); }

		const std::vector<Ref<MeshInstance>> &GetMeshInstances() const { return m_MeshInstances; }
		void SetMeshInstance(const std::vector<Ref<MeshInstance>> &meshInstances) { m_MeshInstances = meshInstances; }
		void AddMeshInstance(const Ref<MeshInstance> &meshInstance) { m_MeshInstances.push_back(meshInstance); }

	private:
		std::vector<Ref<MeshInstance>> m_MeshInstances;
	};

    class MeshLoader
    {
    public:
        static Ref<Material> LoadMaterial(const tinygltf::Primitive& primitive, const std::vector<tinygltf::Material>& materials,
            const std::vector<Ref<Texture>> &loadedTextures, std::array<MeshScene::MaterialTextureMap, 5> &textureMap, const std::vector<nvrhi::SamplerDesc> &loadedSamplers, int *materialIndex);
        static void LoadVertexData(std::vector<VertexMesh_Anim>& vertices, const tinygltf::Primitive& primitive, const tinygltf::Model& model);
        static void LoadIndicesData(std::vector<uint32_t>& indices, const tinygltf::Primitive& primitive, const tinygltf::Model& model);

        static void LoadSceneGraphFromGLTF(const std::string& filename, MeshScene &outScene);

    private:
        static std::vector<Ref<Texture>> LoadTexturesFromGLTF(const tinygltf::Model& model);
        static std::vector<nvrhi::SamplerDesc> GetSamplersFromGLTF(const tinygltf::Model& model);
        static const unsigned char* GetBufferData(const tinygltf::Model& model, const tinygltf::Accessor& accessor);
    };
}
