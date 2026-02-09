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
#include "environment.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/scene_renderer.hpp"

#include <mutex>

namespace ignite
{
    namespace
    {
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


    MeshPrimitive::MeshPrimitive(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices)
        : vertices(vertices), indices(indices)
    {
    }

    MeshPrimitive::~MeshPrimitive()
    {
        LOG_TRACE("MeshPrimitive::~MeshPrimitive() - Destroying mesh primitive");
        
        // Wait for GPU to ensure buffers are not in use
        if (auto* device = Application::GetGraphicsDevice())
        {
            device->waitForIdle();
        }
        
        // Clear GPU buffers
        vertexBuffer.reset();
        indexBuffer.reset();
        
        // Clear CPU data
        vertices.clear();
        indices.clear();
        
        LOG_TRACE("MeshPrimitive::~MeshPrimitive() - Mesh primitive destroyed");
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

    MeshInstance::~MeshInstance()
    {
        LOG_TRACE("MeshInstance::~MeshInstance() - Destroying mesh instance: {}", m_Name);
        
        // Wait for GPU to ensure resources are not in use
        if (auto* device = Application::GetGraphicsDevice())
        {
            device->waitForIdle();
        }
        
        // Clear GPU data buffer
        m_SkinnedMeshGPUDataBuffer.reset();
        
        // Clear primitive (vertex/index buffers)
        m_Primitive.reset();
        
        LOG_TRACE("MeshInstance::~MeshInstance() - Mesh instance destroyed: {}", m_Name);
    }

    void MeshInstance::SetMaterial(AssetHandle assetHandle)
    {
        m_MaterialHandle = assetHandle;
    }

    Ref<MeshInstance> MeshInstance::Create(const std::string &name, const Ref<MeshPrimitive> &mesh)
    {
        return CreateRef<MeshInstance>(name, mesh);
    }

    // 
    // ==== Static Mesh ====
    // 
    Ref<StaticMesh> StaticMesh::Create()
    {
        return CreateRef<StaticMesh>();
    }

    StaticMesh::~StaticMesh()
    {
        LOG_TRACE("StaticMesh::~StaticMesh() - Destroying static mesh (MeshInstances: {})", m_MeshInstances.size());
        
        // Wait for GPU to ensure meshes are not in use
        if (auto* device = Application::GetGraphicsDevice())
        {
            device->waitForIdle();
        }
        
        // Clear all mesh instances
        m_MeshInstances.clear();
        
        LOG_TRACE("StaticMesh::~StaticMesh() - Static mesh destroyed");
    }

    // 
    // ==== Mesh Loader ====
    // 
    Ref<Material> MeshLoader::LoadMaterial(const tinygltf::Primitive &primitive, const std::vector<tinygltf::Material> &gltfMaterials,
        const std::vector<Ref<Texture>> &loadedTextures, const std::vector<nvrhi::SamplerHandle> &loadedSamplers, int *materialIndex)
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
            material->gpuData.baseColorFactor = { gltfMaterial.pbrMetallicRoughness.baseColorFactor[0], gltfMaterial.pbrMetallicRoughness.baseColorFactor[1], gltfMaterial.pbrMetallicRoughness.baseColorFactor[2], 1.0f };
            material->gpuData.emissiveFactor = { gltfMaterial.emissiveFactor[0], gltfMaterial.emissiveFactor[1], gltfMaterial.emissiveFactor[2], 1.0f };
            material->gpuData.metallicFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.metallicFactor);
            material->gpuData.roughnessFactor = static_cast<float>(gltfMaterial.pbrMetallicRoughness.roughnessFactor);
            material->gpuData.occlusionStrength = static_cast<float>(gltfMaterial.occlusionTexture.strength);

            // base color texture
            const int baseColorIndex = gltfMaterial.pbrMetallicRoughness.baseColorTexture.index;
            if (baseColorIndex >= 0 && baseColorIndex < loadedTextures.size())
            {
                material->baseColorTexture = loadedTextures[baseColorIndex];
            }

            // emissive texture
            const int emissiveIndex = gltfMaterial.emissiveTexture.index;
            if (emissiveIndex >= 0 && emissiveIndex < loadedTextures.size())
            {
                material->emissiveTexture = loadedTextures[emissiveIndex];
            }

            // metallic roughness texture
            const int metallicRoughnessIndex = gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (metallicRoughnessIndex >= 0 && metallicRoughnessIndex < loadedTextures.size())
            {
                material->metallicRoughnessTexture = loadedTextures[metallicRoughnessIndex];
            }

            // normal texture
            const int normalIndex = gltfMaterial.normalTexture.index;
            if (normalIndex >= 0 && normalIndex < loadedTextures.size())
            {
                material->normalTexture = loadedTextures[normalIndex];
            }

            // occlusion texture
            const int occlusionIndex = gltfMaterial.occlusionTexture.index;
            if (occlusionIndex >= 0 && occlusionIndex < loadedTextures.size())
            {
                material->occlusionTexture = loadedTextures[occlusionIndex];
            }

            if (!loadedSamplers.empty())
            {
                material->sampler = loadedSamplers[0];
            }
        }

        if (material)
        {
            material->UpdateBindingSet();
        }

        return material;
    }

    void MeshLoader::LoadVertexData(std::vector<VertexMesh_Anim> &vertices, const tinygltf::Primitive &primitive, const tinygltf::Model &model)
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
                // vertex.uv = { texCoords[i].x, 1.0f - texCoords[i].y };
                vertex.uv = texCoords[i]; //{ texCoords[i].x, 1.0f - texCoords[i].y };
            }

            vertices.push_back(vertex);
        }
    }

    void MeshLoader::LoadIndicesData(std::vector<uint32_t> &indices, const tinygltf::Primitive &primitive, const tinygltf::Model &model)
    {
        if (primitive.indices >= 0)
        {
            const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
            const unsigned char *indexData = GetBufferData(model, indexAccessor);

            LOG_INFO("Found {} indices", indexAccessor.count);

            // Handle different index types
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const auto indexPtr = reinterpret_cast<const uint16_t *>(indexData);
                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    indices.push_back(indexPtr[i]);
                }
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                const auto indexPtr = reinterpret_cast<const uint32_t *>(indexData);
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

    void MeshLoader::LoadSceneGraphFromGLTF(const std::string &filename, MeshScene &outScene)
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

        // identify roots
        for (size_t i = 0; i < outScene.nodes.size(); ++i)
        {
            if (outScene.nodes[i].parent < 0)
            {
                outScene.roots.push_back(static_cast<int>(i));
            }
        }

        // load meshes referenced by nodes
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node &node = gltfModel.nodes[i];
            if (node.mesh < 0 || node.mesh >= (int)gltfModel.meshes.size())
                continue;

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
                int materialIndex = -1;
                Ref<Material> material = LoadMaterial(gltfPrim, gltfModel.materials, textures, samplers, &materialIndex);

                Ref<MeshInstance> meshInstance = MeshInstance::Create(gltfMesh.name, primitive);

                outScene.nodes[i].meshes.push_back(meshInstance);
                outScene.flatMeshes.push_back(meshInstance);
                outScene.materials.push_back(material);

                // Assign Mesh and Material Index
                outScene.materialMap[node.mesh] = materialIndex;
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
                    recurse(c, node.global);
            };

        for (const int root : outScene.roots)
            recurse(root, glm::mat4(1.0f));
    }

    std::vector<Ref<Texture>> MeshLoader::LoadTexturesFromGLTF(const tinygltf::Model &model)
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

                Ref<Texture> texture;
                if (!image.image.empty())
                {
                    texture = Texture::Create(Buffer((void *)image.image.data(), image.image.size() * sizeof(uint8_t)), createInfo, nullptr);
                    LOG_TRACE(" Loaded embedded texture");
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

    std::vector<nvrhi::SamplerHandle> MeshLoader::GetSamplersFromGLTF(const tinygltf::Model &model)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        std::vector<nvrhi::SamplerHandle> samplers;
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

                nvrhi::SamplerHandle sampler = device->createSampler(desc);
                LOG_ASSERT(sampler, "Failed to create sampler");
                samplers.push_back(sampler);
            }
        }

        return samplers;
    }

    const unsigned char *MeshLoader::GetBufferData(const tinygltf::Model &model, const tinygltf::Accessor &accessor)
    {
        const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
        return &buffer.data[accessor.byteOffset + bufferView.byteOffset];
    }
}
