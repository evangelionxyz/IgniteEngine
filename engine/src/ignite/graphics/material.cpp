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

#include "material.hpp"

#include <stb_image.h>

#include "renderer.hpp"
#include "scene_renderer.hpp"

namespace ignite
{
    static std::unordered_map<std::string, Ref<Texture>> s_TextureCache;

    Material::Material()
    {
        m_ConstantBuffer = ConstantBuffer::Create(sizeof(MaterialConstants), true, 256, "[Material] Constant Buffer");
    }

    Material::Material(const aiScene *aiScene, aiMaterial* aiMat, const std::filesystem::path& baseFilepath)
    {
        m_ConstantBuffer = ConstantBuffer::Create(sizeof(MaterialConstants), true, 256, "[Material] Constant Buffer");
        name = aiMat->GetName().data;

        // Load Materal's params
        aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
        aiColor4D diffuseColor(1.0f, 1.0f, 1.0f, 1.0f);
        aiColor4D emissiveColor(0.0f, 0.0f, 0.0f, 0.0f);
        f32 reflectivity = 0.0f;
        aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor);
        aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor);
        aiMat->Get(AI_MATKEY_METALLIC_FACTOR, params.metallicFactor);
        aiMat->Get(AI_MATKEY_SPECULAR_FACTOR, params.specularFactor);
        aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, params.roughnessFactor);
        aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);
        aiMat->Get(AI_MATKEY_REFLECTIVITY, reflectivity);

        params.baseColor = { baseColor.r, baseColor.g, baseColor.b, 1.0f };

        if (diffuseColor.r > 0.0f)
        {
            params.emissiveFactor = emissiveColor.r / diffuseColor.r;
        }

        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();

        // load textures
        LoadTexture(cmd, aiScene, aiMat, baseFilepath, MaterialTextureType::BaseColor);
        LoadTexture(cmd, aiScene, aiMat, baseFilepath, MaterialTextureType::Specular);
        LoadTexture(cmd, aiScene, aiMat, baseFilepath, MaterialTextureType::Emissive);
        LoadTexture(cmd, aiScene, aiMat, baseFilepath, MaterialTextureType::Roughness);
        LoadTexture(cmd, aiScene, aiMat, baseFilepath, MaterialTextureType::Normals);

        cmd->close();
        device->executeCommandList(cmd);

        UpdateBindingSet();

        // s_TextureCache.clear();

        // set transparent and reflectivity
        // _transparent = false;
        // _reflective = reflectivity > 0.0f
    }

    Material::~Material()
    {
    }

    void Material::LoadTexture(nvrhi::ICommandList *cmd, const aiScene* aiScene, const aiMaterial* aiMat, const std::filesystem::path& filepath, MaterialTextureType textureType)
    {
        const aiTextureType type = GetAssimpTextureType(textureType);

        // Create the material texture first (Ref counted object)
        TextureCreateInfo createInfo = {};
        createInfo.dimension = nvrhi::TextureDimension::Texture2D;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;
        createInfo.mipLevels = 4;
        createInfo.flip = false;

        Buffer buffer;
        int channels;
        uint8_t *sourceData = nullptr;

        if (const uint32_t texCount = aiMat->GetTextureCount(type))
        {
            for (uint32_t i = 0; i < texCount; ++i)
            {
                aiString texFilename;
                aiMat->GetTexture(type, i, &texFilename);

                // try to load from cache
                for (auto &[path, tex] : s_TextureCache)
                {
                    if (std::strcmp(path.c_str(), texFilename.C_Str()) == 0)
                    {
                        textures[textureType] = tex;
                        LOG_WARN("[Material Importer] {}: {} Texture Loaded from cache", aiTextureTypeToString(type), path.c_str());
                        return;
                    }
                }
                
                stbi_set_flip_vertically_on_load(createInfo.flip);

                // create new texture
                // Load embedded texture
                const aiTexture *embeddedTexture = aiScene->GetEmbeddedTexture(texFilename.C_Str());
                if (embeddedTexture)
                {
                    // handle compressed textures
                    if (embeddedTexture->mHeight == 0)
                    {
                        LOG_INFO("[Material Importer] {}: Loading embedded compressed format texture of size {} bytes", aiTextureTypeToString(type), embeddedTexture->mWidth);
                        buffer.data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(embeddedTexture->pcData), embeddedTexture->mWidth, &createInfo.width, &createInfo.height, &channels, 4);
                        buffer.size = createInfo.width * createInfo.height * 4;
                    }
                    else
                    {
                        createInfo.width = static_cast<int>(embeddedTexture->mWidth);
                        createInfo.height = static_cast<int>(embeddedTexture->mHeight);

                        LOG_INFO("[Material Importer] {}: Loading embedded uncompressed texture of size {}x{}", aiTextureTypeToString(type), createInfo.width, createInfo.height);

                        // Allocate space for RGBA8 data
                        buffer = Buffer(createInfo.width * createInfo.height * 4);

                        // Assimp embedded uncompressed texture data is usually in RGB format without alpha
                        // You can test with alpha channel (or assume RGB with alpha set to 255)
                        for (int p = 0; p < createInfo.width * createInfo.height; ++p)
                        {
                            buffer.data[p * 4 + 0] = sourceData[p * 3 + 0]; // R
                            buffer.data[p * 4 + 1] = sourceData[p * 3 + 1]; // G
                            buffer.data[p * 4 + 2] = sourceData[p * 3 + 2]; // B
                            buffer.data[p * 4 + 3] = 255;                   // A
                        }

                        buffer.size = createInfo.width * createInfo.height * 4;
                        sourceData = reinterpret_cast<uint8_t *>(embeddedTexture->pcData);
                        LOG_ASSERT(sourceData, "[Material Importer] Failed to load texture");
                    }
                }
                else
                {
                    // Texture from file
                    std::filesystem::path textureFilepath = filepath.parent_path() / std::string(texFilename.C_Str());
                    if (!std::filesystem::exists(textureFilepath))
                    {
                        LOG_ERROR("[Material Importer] {}: Texture path is not found! \"{}\"", aiTextureTypeToString(type), textureFilepath.generic_string());
                        return;
                    }

                    LOG_INFO("[Material Importer] {}: Load texture from \"{}\"", aiTextureTypeToString(type), textureFilepath.generic_string());
                    sourceData = stbi_load(textureFilepath.generic_string().c_str(), &createInfo.width, &createInfo.height, &channels, 4);
                    LOG_ASSERT(sourceData, "[Material Importer] Failed to load texture");
                }

                if (sourceData)
                {
                    buffer.data = sourceData;
                    LOG_ASSERT(buffer.data, "[Material Importer] Failed to load texture");
                }

                Ref<Texture> texture = Texture::Create(createInfo);
                int rowPitch = createInfo.width * 4;
                texture->SetData(cmd, buffer, rowPitch, 0);

                textures[textureType] = texture;
                s_TextureCache[texFilename.C_Str()] = texture;
            }
        }
        else
        {
            textures[textureType] = Renderer::GetWhiteTexture();
        }
    }

    void Material::CreateDefaultTextures()
    {
        textures[MaterialTextureType::BaseColor] = Renderer::GetWhiteTexture();
        textures[MaterialTextureType::Specular] = Renderer::GetWhiteTexture();
        textures[MaterialTextureType::Emissive] = Renderer::GetWhiteTexture();
        textures[MaterialTextureType::Roughness] = Renderer::GetWhiteTexture();
        textures[MaterialTextureType::Normals] = Renderer::GetWhiteTexture();

        UpdateBindingSet();
    }

    void Material::UpdateBindingSet()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        const Ref<Environment> &env = SceneRenderer::GetActive()->GetEnvironment();
        
        auto desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_ConstantBuffer->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, textures[MaterialTextureType::BaseColor]->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, textures[MaterialTextureType::Specular]->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, textures[MaterialTextureType::Emissive]->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, textures[MaterialTextureType::Roughness]->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, textures[MaterialTextureType::Normals]->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, env->GetHDRTexture()->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Sampler(0, Renderer::GetWhiteTexture()->GetSampler()));

        auto newBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MATERIAL));
        LOG_ASSERT(newBindingSet, "Failed to create binding set");

        if (newBindingSet)
        {
            bindingSet = newBindingSet;
        }
    }

    void Material::UpdateTexture(const Ref<Texture> &texture, MaterialTextureType textureType)
    {
        if (!texture)
            return;

        textures[textureType] = texture;
        UpdateBindingSet();
    }

    void Material::WriteBuffer(nvrhi::ICommandList* cmd)
    {
        m_ConstantBuffer->SetData(cmd, Buffer(&params, sizeof(MaterialConstants)));
    }
    
    Ref<Material> Material::Create(const aiScene *aiScene, aiMaterial *aiMat, const std::filesystem::path &baseFilepath)
    {
        return CreateRef<Material>(aiScene, aiMat, baseFilepath);
    }
}
