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

#include "mesh.hpp"
#include "ignite/core/time.hpp"
#include "environment.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/skeletal_animation.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <set>

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
			const std::filesystem::path &sourceDir, FBXMeshLoader::MaterialLoader &materialLoader, MeshScene::MaterialTextureMap &textureMap)
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

					std::filesystem::path texturePath = fbxTexture->GetFileName();
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
					if (!std::filesystem::exists(texturePath))
					{
						continue;
					}

					const std::string key = ToLowerCopy(texturePath.generic_string());
					if (materialLoader.textureLookup.contains(key))
					{
						const int index = materialLoader.textureLookup.at(key);
						textureMap = { index, materialLoader.loadedTextures[index] };
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
					textureMap = { index, texture };
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

        

        static void AddBoneInfluence(FBXMeshLoader::FBXBoneInfluence &influence, uint32_t boneId, float weight)
        {
            if (weight <= 0.0f)
            {
                return;
            }

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

        static void NormalizeBoneInfluence(FBXMeshLoader::FBXBoneInfluence &influence)
        {
            float total = 0.0f;
            for (float w : influence.weights)
            {
                total += w;
            }

            if (total <= 0.000001f)
            {
                return;
            }

            const float inv = 1.0f / total;
            for (float &w : influence.weights)
            {
                w *= inv;
            }
        }
    }


    MeshPrimitive::MeshPrimitive(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices)
        : vertices(vertices), indices(indices)
    {
    }

    MeshPrimitive::~MeshPrimitive()
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

    Ref<MeshPrimitive> MeshPrimitive::Create(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices)
    {
        return CreateRef<MeshPrimitive>(vertices, indices);
    }

    void MeshPrimitive::CreateBuffer(nvrhi::ICommandList *cmd)
    {
        vertexBuffer = VertexBuffer::Create(sizeof(VertexMesh_Anim) * vertices.size());
        indexBuffer = IndexBuffer::Create(sizeof(uint32_t) * indices.size());

        vertexBuffer->SetData(cmd, Buffer((void *)vertices.data(), sizeof(VertexMesh_Anim) * vertices.size()));
        indexBuffer->SetData(cmd, Buffer((void *)indices.data(), sizeof(uint32_t) * indices.size()));
    }

    void MeshPrimitive::ClearPrimitivesData()
    {
        vertices.clear();
        indices.clear();
    }

    // Mesh Instance class
    MeshInstance::MeshInstance(const std::string &name, const Ref<MeshPrimitive> &mesh)
        : m_Name(name), m_Primitive(mesh)
    {
    }

    MeshInstance::MeshInstance()
    {
        m_Primitive = CreateRef<MeshPrimitive>();
    }

	MeshInstance::MeshInstance(const MeshNode &node, const Ref<MeshPrimitive> &mesh)
        : m_Name(node.name), m_Primitive(mesh)
	{
        local = node.local;
        global = node.global;
	}

	MeshInstance::~MeshInstance()
    {
        // Wait for GPU to ensure resources are not in use
        if (auto *device = DeviceManager::GetInstance()->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }

        // Clear primitive (vertex/index buffers)
        m_Primitive.reset();
    }

    void MeshInstance::SetMaterial(AssetHandle assetHandle)
    {
        m_MaterialHandle = assetHandle;
    }

    Ref<MeshInstance> MeshInstance::Create(const std::string &name, const Ref<MeshPrimitive> &mesh)
    {
        return CreateRef<MeshInstance>(name, mesh);
    }

	Ref<MeshInstance> MeshInstance::Create(const MeshNode &node, const Ref<MeshPrimitive> &mesh)
	{
        return CreateRef<MeshInstance>(node, mesh);
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

    // ===================================
    // Skeletal Mesh
    // ===================================
    Ref<SkeletalMesh> SkeletalMesh::Create()
    {
        return CreateRef<SkeletalMesh>();
    }

    SkeletalMesh::~SkeletalMesh()
    {
        m_MeshInstances.clear();
    }

    // ===================================
    // Mesh Loader
    // ===================================
    Ref<Material> GLTFMeshLoader::LoadMaterial(const tinygltf::Primitive &primitive, const std::vector<tinygltf::Material> &gltfMaterials, const std::vector<Ref<Texture>> &loadedTextures, std::array<MeshScene::MaterialTextureMap, 5> &textureMap, const std::vector<nvrhi::SamplerDesc> &loadedSamplers, int *materialIndex)
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
                1.0f
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
                textureMap[0] = { baseColorIndex, loadedTextures[baseColorIndex] };
            }
            else
            {
                textureMap[0] = { -1, nullptr };
            }

            // emissive texture
            const int emissiveIndex = gltfMaterial.emissiveTexture.index;
            if (emissiveIndex >= 0 && emissiveIndex < loadedTextures.size())
            {
                textureMap[1] = { emissiveIndex, loadedTextures[emissiveIndex] };
            }
            else
            {
                textureMap[1] = { -1, nullptr };
            }

            // metallic roughness texture
            const int metallicRoughnessIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (metallicRoughnessIndex >= 0 && metallicRoughnessIndex < loadedTextures.size())
            {
                textureMap[2] = { metallicRoughnessIndex, loadedTextures[metallicRoughnessIndex] };
            }
            else
            {
                textureMap[2] = { -1, nullptr };
            }

            // normal texture
            const int normalIndex = gltfMaterial.normalTexture.index;
            if (normalIndex >= 0 && normalIndex < loadedTextures.size())
            {
                textureMap[3] = { normalIndex, loadedTextures[normalIndex] };
            }
            else
            {
                textureMap[3] = { -1, nullptr };
            }

            // occlusion texture
            const int occlusionIndex = gltfMaterial.occlusionTexture.index;
            if (occlusionIndex >= 0 && occlusionIndex < loadedTextures.size())
            {
                textureMap[4] = { occlusionIndex, loadedTextures[occlusionIndex] };
            }
            else
            {
                textureMap[4] = { -1, nullptr };
            }

            if (!loadedSamplers.empty())
            {
                material->SetSamplerDesc(loadedSamplers[0]);
            }
        }

        return material;
    }

    void GLTFMeshLoader::LoadVertexData(std::vector<VertexMesh_Anim> &vertices, const tinygltf::Primitive &primitive, const tinygltf::Model &model)
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

        // Build vertices
        vertices.reserve(positionCount);
        for (size_t i = 0; i < positionCount; ++i)
        {
            VertexMesh_Anim vertex{};
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

    void GLTFMeshLoader::LoadSceneGraphFromGLTF(const std::string &filename, MeshScene &outScene)
    {
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ok = false;
        if (filename.ends_with(".glb")) ok = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filename);
        else ok = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filename);

        if (!ok)
        {
            return;
        }

        // pre-load textures and samplers
        const auto textures = LoadTexturesFromGLTF(gltfModel);
        const auto samplers = GetSamplersFromGLTF(gltfModel);

        // preserve nodes
        outScene.nodes.resize(gltfModel.nodes.size());

        // build raw node relationships and local transforms
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node &node = gltfModel.nodes[i];

            MeshNode &meshNode = outScene.nodes[i];
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

        // load meshes referenced by nodes
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node &node = gltfModel.nodes[i];
            if (node.mesh < 0 || node.mesh >= (int)gltfModel.meshes.size())
                continue;

            const MeshNode &meshNode = outScene.nodes[i];

            const tinygltf::Mesh &gltfMesh = gltfModel.meshes[node.mesh];
            for (const auto &gltfPrim : gltfMesh.primitives)
            {
                std::vector<VertexMesh_Anim> vertices;
                std::vector<uint32_t> indices;

                // get vertices and indices
                LoadVertexData(vertices, gltfPrim, gltfModel);
                LoadIndicesData(indices, gltfPrim, gltfModel);
                Ref<MeshPrimitive> primitive = CreateRef<MeshPrimitive>(vertices, indices);

                // material
                std::array<MeshScene::MaterialTextureMap, 5> materialTextureMap{};

                int materialIndex = -1;
                Ref<Material> material = LoadMaterial(gltfPrim, gltfModel.materials, textures, materialTextureMap, samplers, &materialIndex);
                if (!material)
                {
                    material = CreateDefaultMaterial("DefaultMaterial");
                }

                Ref<MeshInstance> meshInstance = MeshInstance::Create(meshNode, primitive);

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
                MeshNode &node = outScene.nodes[nodeIndex];
                node.global = parentGlobal * node.local;
                for (const auto &m : node.meshes)
                {
                    m->local = node.local;
                    m->global = node.global;
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

    std::vector<Ref<Texture>> GLTFMeshLoader::LoadTexturesFromGLTF(const tinygltf::Model &model)
    {
        std::vector<Ref<Texture>> gltfTextures;
        LOG_TRACE("Loading {} textures from glTF", model.textures.size());

        for (size_t i = 0; i < model.textures.size(); ++i)
        {
            const tinygltf::Texture &gltfTexture = model.textures[i];

            if (gltfTexture.source >= 0 && gltfTexture.source < model.images.size())
            {
                const tinygltf::Image &image = model.images[gltfTexture.source];
                LOG_TRACE(" Texture {}: {} ({}x{})", i, image.name, image.width, image.height);

                TextureCreateInfo createInfo;
                createInfo.width = image.width;
                createInfo.height = image.height;
                createInfo.flip = false;
                createInfo.format = nvrhi::Format::RGBA8_UNORM;
                createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
                createInfo.keepInitialState = true;
                createInfo.keepCpuData = true;
                createInfo.deferGpuCreate = true;

                Ref<Texture> texture;
                if (!image.image.empty())
                {
                    texture = Texture::Create(Buffer((void *)image.image.data(), image.image.size() * sizeof(uint8_t)), createInfo, nullptr);
                    LOG_TRACE(" Loaded embedded texture");

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

                gltfTextures.push_back(texture);
            }
        }

        return gltfTextures;
    }

    std::vector<nvrhi::SamplerDesc> GLTFMeshLoader::GetSamplersFromGLTF(const tinygltf::Model &model)
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

    void FBXMeshLoader::LoadSceneGraphFromFBX(const std::string &filename, MeshScene &outScene, AssetManager *assetManager, bool importSkeletonAndAnimations)
    {
        FbxManager *sdkManager = assetManager->GetOrCreateFbxSdkManager();
        if (!sdkManager)
        {
            LOG_ASSERT(false, "[FBX Loader] Failed to create FBX SDK Manager");
            return;
        }

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

        const FbxAxisSystem targetAxisSystem = FbxAxisSystem::MayaYUp;
        const FbxAxisSystem sceneAxisSystem = fbxScene->GetGlobalSettings().GetAxisSystem();
        if (sceneAxisSystem != targetAxisSystem)
        {
            targetAxisSystem.ConvertScene(fbxScene);
        }

        const FbxSystemUnit targetUnit = FbxSystemUnit::cm;
        const FbxSystemUnit sceneUnit = fbxScene->GetGlobalSettings().GetSystemUnit();
        if (sceneUnit.GetScaleFactor() != targetUnit.GetScaleFactor())
        {
            targetUnit.ConvertScene(fbxScene);
        }

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
            outScene.skeleton = LoadSkeletonFBX(fbxScene, jointLoader);
            LoadAnimationsFBX(fbxScene, outScene.skeleton, jointLoader.jointNodes, outScene.animations);
        }
        else
        {
            outScene.skeleton = nullptr;
            outScene.animations.clear();
        }

        const std::filesystem::path sourceDir = std::filesystem::path(filename).parent_path();

        MaterialLoader materialLoader;
        FbxNode *rootNode = fbxScene->GetRootNode();
        if (rootNode)
        {
            for (int i = 0; i < rootNode->GetChildCount(); ++i)
            {
                BuildNode(rootNode->GetChild(i), fbxScene, outScene, materialLoader, jointLoader, sourceDir, -1, glm::mat4(1.0f), importSkeletonAndAnimations);
            }
        }

        fbxScene->Destroy();
    }

	void FBXMeshLoader::LoadSkeletonOnlyFromFBX(const std::string &filename, Ref<Skeleton> &skeleton, AssetManager *assetManager)
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

		JointLoader jointLoader;

		FbxNode *rootNode = fbxScene->GetRootNode();
		if (rootNode)
		{
			for (int i = 0; i < rootNode->GetChildCount(); ++i)
			{
				SkeletonBuildHierarchy(rootNode->GetChild(i), skeleton, jointLoader);
			}
		}

		if (skeleton->joints.empty())
		{
			skeleton.reset();
		}

		fbxScene->Destroy();
	}

	void FBXMeshLoader::LoadAnimationsOnlyFromFBX(const std::string &filename, Ref<Skeleton> skeleton, std::vector<Ref<SkeletalAnimation>> &outAnimations, AssetManager *assetManager)
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

		JointLoader jointLoader;

		FbxNode *rootNode = fbxScene->GetRootNode();
		if (rootNode)
		{
			for (int i = 0; i < rootNode->GetChildCount(); ++i)
			{
				SkeletonBuildHierarchy(rootNode->GetChild(i), skeleton, jointLoader);
			}
		}

		if (skeleton->joints.empty())
		{
			skeleton.reset();
		}

		LoadAnimationsFBX(fbxScene, skeleton, jointLoader.jointNodes, outAnimations);

        fbxScene->Destroy();
	}

    void FBXMeshLoader::BuildNode(FbxNode *node, FbxScene *fbxScene, MeshScene &outScene, MaterialLoader &materialLoader, JointLoader &jointLoader, const std::filesystem::path &sourceDir, int parentIdx, const glm::mat4 &parentGlobal, bool importSkinningData)
    {
        if (!node)
        {
            return;
        }

        const int nodeIndex = static_cast<int>(outScene.nodes.size());
        outScene.nodes.emplace_back();

        MeshNode &meshNode = outScene.nodes[nodeIndex];
        meshNode.parent = parentIdx;
        meshNode.name = node->GetName() ? node->GetName() : "";
        meshNode.local = ToGlmMatrix(node->EvaluateLocalTransform());
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
            std::vector<VertexMesh_Anim> vertices;
            std::vector<uint32_t> indices;

            const FbxVector4 *controlPoints = fbxMesh->GetControlPoints();
            FbxAMatrix meshGeom = BuildNodeGeometricMatrix(node);
            FbxStringList uvSetNames;
            fbxMesh->GetUVSetNames(uvSetNames);

            std::vector<FBXBoneInfluence> controlPointInfluence;
            controlPointInfluence.resize(static_cast<size_t>(fbxMesh->GetControlPointsCount()));

            bool isSkinned = importSkinningData && fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0;
            LOG_INFO("[FBX SKIN DEBUG] Node='{}' parent='{}' mesh='{}' skinned={} deformers={} cpCount={}",
                meshNode.name,
                (parentIdx >= 0 ? outScene.nodes[parentIdx].name : std::string("<root>")),
                (fbxMesh->GetName() ? std::string(fbxMesh->GetName()) : std::string("<unnamed>")),
                isSkinned,
                fbxMesh->GetDeformerCount(FbxDeformer::eSkin),
                fbxMesh->GetControlPointsCount());

            if (isSkinned)
            {
                FbxSkin *skin = static_cast<FbxSkin *>(fbxMesh->GetDeformer(0, FbxDeformer::eSkin));
                if (skin && skin->GetClusterCount() > 0)
                {
                    // Keep vertices in mesh-local space. Skinning matrices already include mesh bind transform.
                    LOG_INFO("[FBX SKIN DEBUG] Node='{}' firstSkinClusters={}", meshNode.name, skin->GetClusterCount());
                }
            }

            if (importSkinningData)
            {
                for (int deformerIndex = 0; deformerIndex < fbxMesh->GetDeformerCount(FbxDeformer::eSkin); ++deformerIndex)
                {
                    FbxSkin *skin = static_cast<FbxSkin *>(fbxMesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));
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

                        const int32_t jointId = SkeletonFindOrAddJoint(jointNode, outScene.skeleton, jointLoader);
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

                        // Cluster matrices (FBX bind pose)
                        cluster->GetTransformMatrix(meshBind);
                        cluster->GetTransformLinkMatrix(jointBind);

                        // Keep a joint-space inverse bind (shared safely across multiple skinned meshes).
                        // Mesh node placement is applied in object transform during rendering.
                        const FbxAMatrix invBind = jointBind.Inverse();
                        const glm::mat4 invBindGlm = ToGlmMatrix(invBind);
                        glm::mat4 &existingInvBind = outScene.skeleton->joints[jointId].inverseBindPose;
                        const bool hasExistingInvBind = !Mat4NearEqual(existingInvBind, glm::mat4(1.0f));
                        if (hasExistingInvBind && !Mat4NearEqual(existingInvBind, invBindGlm, 0.001f))
                        {
                            const glm::vec3 oldT = ExtractTranslation(existingInvBind);
                            const glm::vec3 newT = ExtractTranslation(invBindGlm);
                            LOG_WARN("[FBX SKIN DEBUG] InverseBind mismatch joint='{}' id={} meshNode='{}' oldT=({:.4f},{:.4f},{:.4f}) newT=({:.4f},{:.4f},{:.4f})",
                                outScene.skeleton->joints[jointId].name,
                                jointId,
                                meshNode.name,
                                oldT.x, oldT.y, oldT.z,
                                newT.x, newT.y, newT.z);
                        }

                        existingInvBind = invBindGlm;

                        if (clusterIndex < 6)
                        {
                            const glm::vec3 meshBindT = ExtractTranslation(ToGlmMatrix(meshBind));
                            const glm::vec3 jointBindT = ExtractTranslation(ToGlmMatrix(jointBind));
                            const glm::vec3 invBindT = ExtractTranslation(invBindGlm);
                            LOG_INFO("[FBX SKIN DEBUG] joint='{}' id={} cpInfluences={} meshBindT=({:.3f},{:.3f},{:.3f}) jointBindT=({:.3f},{:.3f},{:.3f}) invBindT=({:.3f},{:.3f},{:.3f})",
                                outScene.skeleton->joints[jointId].name,
                                jointId,
                                cluster->GetControlPointIndicesCount(),
                                meshBindT.x, meshBindT.y, meshBindT.z,
                                jointBindT.x, jointBindT.y, jointBindT.z,
                                invBindT.x, invBindT.y, invBindT.z);
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

                            AddBoneInfluence(controlPointInfluence[controlPointIndex], static_cast<uint32_t>(jointId), static_cast<float>(controlPointWeights[i]));
                        }
                    }
                }
            }

            if (importSkinningData)
            {
                for (FBXMeshLoader::FBXBoneInfluence &influence : controlPointInfluence)
                {
                    NormalizeBoneInfluence(influence);
                }
            }

            size_t influencedControlPoints = 0;
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

                VertexMesh_Anim vertex{};
                vertex.position =
                {
                    static_cast<float>(transformedPosition[0]),
                    static_cast<float>(transformedPosition[1]),
                    static_cast<float>(transformedPosition[2])
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

                vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                vertex.bitangent = glm::cross(vertex.normal, vertex.tangent);

                if (importSkinningData && controlPointIndex >= 0 && controlPointIndex < static_cast<int>(controlPointInfluence.size()))
                {
                    const FBXBoneInfluence &influence = controlPointInfluence[controlPointIndex];
                    for (size_t i = 0; i < VERTEX_MAX_BONES; ++i)
                    {
                        vertex.boneIDs[i] = influence.ids[i];
                        vertex.weights[i] = influence.weights[i];
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

            if (!vertices.empty() && !indices.empty())
            {
                Ref<MeshPrimitive> primitive = MeshPrimitive::Create(vertices, indices);
                Ref<MeshInstance> meshInstance = MeshInstance::Create(meshNode, primitive);

                outScene.nodes[nodeIndex].meshes.push_back(meshInstance);
                outScene.flatMeshes.push_back(meshInstance);

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

                    std::array<MeshScene::MaterialTextureMap, 5> textureMap{};
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

                outScene.materialMap[static_cast<int>(outScene.flatMeshes.size()) - 1] = sceneMaterialIndex;
            }
        }

        for (int i = 0; i < node->GetChildCount(); ++i)
        {
            BuildNode(node->GetChild(i), fbxScene, outScene, materialLoader, jointLoader, sourceDir, nodeIndex, meshNode.global, importSkinningData);
        }
    }

	Ref<Skeleton> FBXMeshLoader::LoadSkeletonFBX(fbxsdk::FbxScene *fbxScene, JointLoader &outJointResult)
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
				SkeletonBuildHierarchy(rootNode->GetChild(i), skeleton, outJointResult);
			}
		}

		if (skeleton->joints.empty())
		{
			skeleton.reset();
		}

        return skeleton;
	}

	void FBXMeshLoader::LoadAnimationsFBX(fbxsdk::FbxScene *fbxScene, const Ref<Skeleton> &skeleton, JointMap &jointNodes, std::vector<Ref<SkeletalAnimation>> &outAnimations)
	{
		if (!fbxScene || !skeleton)
		{
			return;
		}

		const double frameRate = FbxTime::GetFrameRate(fbxScene->GetGlobalSettings().GetTimeMode());
		const float ticksPerSecond = frameRate > 0.0 ? static_cast<float>(frameRate) : 30.0f;

		for (int stackIndex = 0; stackIndex < fbxScene->GetSrcObjectCount<FbxAnimStack>(); ++stackIndex)
		{
			FbxAnimStack *animStack = fbxScene->GetSrcObject<FbxAnimStack>(stackIndex);
			if (!animStack || animStack->GetMemberCount<FbxAnimLayer>() == 0)
			{
				continue;
			}

			FbxAnimLayer *layer = animStack->GetMember<FbxAnimLayer>(0);
			if (!layer)
			{
				continue;
			}

			FbxTimeSpan timeSpan = animStack->GetLocalTimeSpan();
			const double startSeconds = timeSpan.GetStart().GetSecondDouble();
			const double endSeconds = timeSpan.GetStop().GetSecondDouble();

			Ref<SkeletalAnimation> animation = CreateRef<SkeletalAnimation>();
			animation->name = animStack->GetName() ? animStack->GetName() : "FBXAnimation";
			animation->ticksPerSeconds = ticksPerSecond;
			animation->duration = std::max(0.0f, static_cast<float>((endSeconds - startSeconds) * ticksPerSecond));

			for (const Joint &joint : skeleton->joints)
			{
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

					glm::vec3 decomposedScale(1.0f);
					glm::quat decomposedRotation(1.0f, 0.0f, 0.0f, 0.0f);
					glm::vec3 decomposedTranslation(0.0f);
					glm::vec3 skew(0.0f);
					glm::vec4 perspective(0.0f);
					glm::decompose(localMatrix,
						decomposedScale,
						decomposedRotation,
						decomposedTranslation,
						skew,
						perspective);

					channel.translationKeys.AddFrame({ decomposedTranslation, timestamp });
					channel.rotationKeys.AddFrame({ decomposedRotation, timestamp });
					channel.scaleKeys.AddFrame({ decomposedScale, timestamp });
				}

				animation->channels.emplace(joint.name, std::move(channel));
			}

			if (!animation->channels.empty())
			{
				outAnimations.push_back(animation);
			}
		}
	}

	int32_t FBXMeshLoader::SkeletonFindOrAddJoint(fbxsdk::FbxNode *jointNode, const Ref<Skeleton> &skeleton, JointLoader &outJointResult)
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
			parentJointId = SkeletonFindOrAddJoint(parentNode, skeleton, outJointResult);
		}

		Joint joint{};
		joint.id = static_cast<int32_t>(skeleton->joints.size());
		joint.parentJointId = parentJointId;
		joint.name = jointName;
		joint.localTransform = ToGlmMatrix(jointNode->EvaluateLocalTransform());

		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(joint.localTransform, joint.defaultScale, joint.defaultRotation, joint.defaultTranslation, skew, perspective);

		joint.globalTransform = glm::mat4(1.0f);
		joint.inverseBindPose = glm::mat4(1.0f);

		skeleton->nameToJointMap[joint.name] = joint.id;
		skeleton->joints.push_back(joint);
		outJointResult.jointNameToIndex[joint.name] = joint.id;

		outJointResult.jointNodes[joint.name] = jointNode;

		return joint.id;
	}

	void FBXMeshLoader::SkeletonBuildHierarchy(fbxsdk::FbxNode *node, const Ref<Skeleton> &skeleton, JointLoader &outJointResult)
	{
		if (!node)
		{
			return;
		}

		FbxNodeAttribute *attribute = node->GetNodeAttribute();
		if (attribute && attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
            SkeletonFindOrAddJoint(node, skeleton, outJointResult);
		}

		for (int i = 0; i < node->GetChildCount(); ++i)
		{
            SkeletonBuildHierarchy(node->GetChild(i), skeleton, outJointResult);
		}
	}

	void MeshLoader::LoadSceneGraph(const std::string &filename, MeshScene &outScene, AssetManager *assetManager)
    {
        const std::string extension = ToLowerCopy(std::filesystem::path(filename).extension().string());
        if (extension == ".fbx")
        {
            FBXMeshLoader::LoadSceneGraphFromFBX(filename, outScene, assetManager);
            return;
        }

        GLTFMeshLoader::LoadSceneGraphFromGLTF(filename, outScene);
    }
}
