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
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/scene_renderer.hpp"

namespace ignite
{
    // Helper to build mat4 from glTF node TRS
    static glm::mat4 BuildNodeLocalMatrix(const tinygltf::Node& node)
    {
        if (!node.matrix.empty())
        {
            // glTF supplies 16 values column-major. Construct manually.
            return
                glm::mat4(
                    (float)node.matrix[0], (float)node.matrix[1], (float)node.matrix[2], (float)node.matrix[3],
                    (float)node.matrix[4], (float)node.matrix[5], (float)node.matrix[6], (float)node.matrix[7],
                    (float)node.matrix[8], (float)node.matrix[9], (float)node.matrix[10], (float)node.matrix[11],
                    (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]
                );
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

    Mesh::Mesh(const std::vector<VertexMesh_Anim> &vertices, const std::vector<uint32_t> &indices)
    {
        vertexBuffer = VertexBuffer::Create(sizeof(VertexMesh_Anim) * vertices.size());
        indexBuffer = IndexBuffer::Create(sizeof(uint32_t) * indices.size());

        vertexBuffer->SetData(Buffer((void *)vertices.data(), sizeof(VertexMesh_Anim) * vertices.size()));
        indexBuffer->SetData(Buffer((void *)indices.data(), sizeof(uint32_t) * indices.size()));

        skinnedBuffer = ConstantBuffer::Create(sizeof(SkinnedMeshBuffer), true, 16, "[Mesh] Constant Buffer");
    }

    void Mesh::UpdateBindingSet(Scene *scene)
    {
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        // Create binding set
        const Ref<Environment> &env = SceneRenderer::GetActive()->GetEnvironment();
        auto desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraConstantBuffer()->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, skinnedBuffer->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, scene->GetConstantBuffer()->GetHandle()));

        const auto newBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM));
        LOG_ASSERT(newBindingSet, "Failed to create binding set");

        if (newBindingSet)
        {
            m_BindingSet = newBindingSet;
        }
    }

    void MeshLoader::LoadMaterial(const Ref<Mesh>& mesh, const tinygltf::Primitive& primitive, const std::vector<tinygltf::Material>& materials, const std::vector<Ref<Texture>>& loadedTextures)
    {
        mesh->materialIndex = primitive.material;
        if (primitive.material >= 0 && primitive.material < materials.size())
        {
            const tinygltf::Material& material = materials[primitive.material];
            LOG_TRACE("Loading material: {}", material.name);

            mesh->material = CreateRef<Material>();
            mesh->material->name = material.name;
            mesh->material->params.baseColorFactor = { material.pbrMetallicRoughness.baseColorFactor[0], material.pbrMetallicRoughness.baseColorFactor[1], material.pbrMetallicRoughness.baseColorFactor[2], 1.0f };
            mesh->material->params.emissiveFactor = { material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2], 1.0f };
            mesh->material->params.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
            mesh->material->params.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
            mesh->material->params.occlusionStrength = static_cast<float>(material.occlusionTexture.strength);

            // base color texture
            const int baseColorIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            if (baseColorIndex >= 0 && baseColorIndex < loadedTextures.size())
            {
                mesh->material->baseColorTexture = loadedTextures[baseColorIndex];
            }

            // emissive texture
            const int emissiveIndex = material.emissiveTexture.index;
            if (emissiveIndex >= 0 && emissiveIndex < loadedTextures.size())
            {
                mesh->material->emissiveTexture = loadedTextures[emissiveIndex];
            }

            // metallic roughness texture
            const int metallicRoughnessIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (metallicRoughnessIndex >= 0 && metallicRoughnessIndex < loadedTextures.size())
            {
                mesh->material->metallicRoughnessTexture = loadedTextures[metallicRoughnessIndex];
            }

            // normal texture
            const int normalIndex = material.normalTexture.index;
            if (normalIndex >= 0 && normalIndex < loadedTextures.size())
            {
                mesh->material->normalTexture = loadedTextures[normalIndex];
            }

            // occlusion texture
            const int occlusionIndex = material.occlusionTexture.index;
            if (occlusionIndex >= 0 && occlusionIndex < loadedTextures.size())
            {
                mesh->material->occlusionTexture = loadedTextures[occlusionIndex];
            }

            // update binding set
            mesh->material->UpdateBindingSet();

            auto device = Application::GetGraphicsDevice();
            auto cmd = device->createCommandList();
            cmd->open();
            mesh->material->UploadToGpu(cmd);
            cmd->close();
            device->executeCommandList(cmd);
        }
    }

    void MeshLoader::LoadVertexData(std::vector<VertexMesh_Anim>& vertices, const tinygltf::Primitive& primitive, const tinygltf::Model& model)
    {
        // Get vertex positions
        glm::vec3* positions = nullptr;
        size_t positionCount = 0;

        if (primitive.attributes.contains("POSITION"))
        {
            const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
            positions = (glm::vec3*)GetBufferData(model, accessor);
            positionCount = accessor.count;
        }

        // Get vertex normals (optional)
        glm::vec3* normals = nullptr;
        if (primitive.attributes.contains("NORMAL"))
        {
            const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
            normals = (glm::vec3*)GetBufferData(model, accessor);
        }

        // Get vertex tangents (optional)
        glm::vec4* tangents = nullptr;
        if (primitive.attributes.contains("TANGENT"))
        {
            const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TANGENT")];
            tangents = (glm::vec4*)GetBufferData(model, accessor);
        }

        // Get texture coordinates (optional)
        glm::vec2* texCoords = nullptr;
        if (primitive.attributes.contains("TEXCOORD_0"))
        {
            const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
            texCoords = (glm::vec2*)GetBufferData(model, accessor);
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

    void MeshLoader::LoadIndicesData(std::vector<uint32_t>& indices, const tinygltf::Primitive& primitive, const tinygltf::Model& model)
    {
        if (primitive.indices >= 0)
        {
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const unsigned char* indexData = GetBufferData(model, indexAccessor);

            LOG_INFO("Found {} indices", indexAccessor.count);

            // Handle different index types
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const auto indexPtr = reinterpret_cast<const uint16_t*>(indexData);
                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    indices.push_back(indexPtr[i]);
                }
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                const auto indexPtr = reinterpret_cast<const uint32_t*>(indexData);
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

    MeshScene MeshLoader::LoadSceneGraphFromGLTF(const std::string& filename)
    {
        MeshScene scene;
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ok = false;
        if (filename.ends_with(".glb"))
            ok = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filename);
        else
            ok = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filename);

        if (!ok)
            return scene;

        // preload textures
        const auto textures = LoadTexturesFromGLTF(gltfModel);

        // preserve nodes
        scene.nodes.resize(gltfModel.nodes.size());

        // build raw node relationships and local transforms
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node& node = gltfModel.nodes[i];

            MeshNode& meshNode = scene.nodes[i];
            meshNode.name = node.name;
            meshNode.local = BuildNodeLocalMatrix(node);
            for (int c : node.children)
            {
                meshNode.children.push_back(c);
                scene.nodes[c].parent = static_cast<int>(i);
            }
        }

        // identify roots
        for (size_t i = 0; i < scene.nodes.size(); ++i)
        {
            if (scene.nodes[i].parent < 0)
            {
                scene.roots.push_back(static_cast<int>(i));
            }
        }

        // load meshes referenced by nodes
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node& node = gltfModel.nodes[i];
            if (node.mesh < 0 || node.mesh >= (int)gltfModel.meshes.size())
                continue;

            const tinygltf::Mesh& gltfMesh = gltfModel.meshes[node.mesh];
            for (const auto& primitive : gltfMesh.primitives)
            {
                std::vector<VertexMesh_Anim> vertices;
                std::vector<uint32_t> indices;

                // get vertices and indices
                LoadVertexData(vertices, primitive, gltfModel);
                LoadIndicesData(indices, primitive, gltfModel);
                Ref<Mesh> mesh = CreateRef<Mesh>(vertices, indices);
                mesh->name = gltfMesh.name;

                // material
                LoadMaterial(mesh, primitive, gltfModel.materials, textures);

                scene.nodes[i].meshes.push_back(mesh);
                scene.flatMeshes.push_back(mesh);
            }
        }

        // compute global transform via DFS
        std::function<void(int, const glm::mat4&)> recurse = [&](const int nodeIndex, const glm::mat4& parentGlobal)
        {
            MeshNode& node = scene.nodes[nodeIndex];
            node.global = parentGlobal * node.local;
            for (const auto& m : node.meshes)
            {
                m->local = node.local;
                m->global = node.global;
            }

            for (const int c : node.children)
                recurse(c, node.global);
        };

        for (const int root : scene.roots)
            recurse(root, glm::mat4(1.0f));

        return scene;
    }

    std::vector<Ref<Texture>> MeshLoader::LoadTexturesFromGLTF(const tinygltf::Model& model)
    {
        std::vector<Ref<Texture>> gltfTextures;
        LOG_TRACE("Loading {} textures from glTF", model.textures.size());

        for (size_t i = 0; i < model.textures.size(); ++i)
        {
            const tinygltf::Texture& gltfTexture = model.textures[i];

            if (gltfTexture.source >= 0 && gltfTexture.source < model.images.size())
            {
                const tinygltf::Image& image = model.images[gltfTexture.source];
                LOG_TRACE(" Texture {}: {} ({}x{})", i, image.name, image.width, image.height);

                TextureCreateInfo createInfo;
                createInfo.width = image.width;
                createInfo.height = image.height;
                createInfo.flip = false;
                createInfo.format = nvrhi::Format::RGBA8_UNORM;

                tinygltf::Sampler sampler = model.samplers[gltfTexture.sampler];
                switch (sampler.wrapS)
                {
                case TINYGLTF_TEXTURE_WRAP_REPEAT:
                    createInfo.samplerMode = nvrhi::SamplerAddressMode::Repeat;
                    break;
                case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                    createInfo.samplerMode = nvrhi::SamplerAddressMode::ClampToEdge;
                    break;
                case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
                    createInfo.samplerMode = nvrhi::SamplerAddressMode::MirroredRepeat;
                    break;
                }

                Ref<Texture> texture;

                if (!image.image.empty())
                {
                    texture = Texture::Create(Buffer((void*)image.image.data(), image.image.size() * sizeof(uint8_t)), createInfo);
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

    const unsigned char* MeshLoader::GetBufferData(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
    {
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        return &buffer.data[accessor.byteOffset + bufferView.byteOffset];
    }
}
