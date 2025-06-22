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
    static std::unordered_map<std::string, Ref<MaterialTextureResource>> s_TextureCache;

    Material::Material()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        auto paramsBufferDesc = nvrhi::BufferDesc();
        paramsBufferDesc.setIsConstantBuffer(true);
        paramsBufferDesc.setIsVolatile(true);
        paramsBufferDesc.setMaxVersions(128);
        paramsBufferDesc.setInitialState(nvrhi::ResourceStates::ConstantBuffer);
        paramsBufferDesc.setDebugName("MaterialConstantBuffer");
        paramsBufferDesc.setByteSize(sizeof(MaterialConstants));

        paramsBuffer = device->createBuffer(paramsBufferDesc);
        LOG_ASSERT(paramsBuffer, "[Material] Failed to create constant buffer");
    }

    Material::Material(const aiScene *aiScene, aiMaterial* aiMat, const std::filesystem::path& baseFilepath)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        auto paramsBufferDesc = nvrhi::BufferDesc();
        paramsBufferDesc.setIsConstantBuffer(true);
        paramsBufferDesc.setIsVolatile(true);
        paramsBufferDesc.setMaxVersions(128);
        paramsBufferDesc.setInitialState(nvrhi::ResourceStates::ConstantBuffer);
        paramsBufferDesc.setDebugName("MaterialConstantBuffer");
        paramsBufferDesc.setByteSize(sizeof(MaterialConstants));

        paramsBuffer = device->createBuffer(paramsBufferDesc);
        LOG_ASSERT(paramsBuffer, "[Material] Failed to create constant buffer");

        name = aiMat->GetName().data;

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

        // load textures
        LoadTexture(aiScene, aiMat, baseFilepath, MaterialTextureType::BaseColor);
        LoadTexture(aiScene, aiMat, baseFilepath, MaterialTextureType::Specular);
        LoadTexture(aiScene, aiMat, baseFilepath, MaterialTextureType::Emissive);
        LoadTexture(aiScene, aiMat, baseFilepath, MaterialTextureType::Roughness);
        LoadTexture(aiScene, aiMat, baseFilepath, MaterialTextureType::Normals);

        // set transparent and reflectivity
        // _transparent = false;
        // _reflective = reflectivity > 0.0f
    }

    void Material::LoadTexture(const aiScene* aiScene, const aiMaterial* aiMat, const std::filesystem::path& filepath, MaterialTextureType textureType)
    {
        const aiTextureType type = GetAssimpTextureType(textureType);

        // Create the material texture first (Ref counted object)
        textures[textureType] = CreateRef<MaterialTextureResource>();

        if (const uint32_t texCount = aiMat->GetTextureCount(type))
        {
            for (uint32_t i = 0; i < texCount; ++i)
            {
                aiString texFilename;
                aiMat->GetTexture(type, i, &texFilename);

                LOG_INFO("[Material Importer] Texture type {}", aiTextureTypeToString(type));

                // try to load from cache
                for (auto &[path, tex] : s_TextureCache)
                {
                    if (std::strcmp(path.c_str(), texFilename.C_Str()) == 0)
                    {
                        textures[textureType] = tex;
                        LOG_WARN("[Material Importer] {} Loaded from cache", path.c_str());
                        return;
                    }
                }

                stbi_set_flip_vertically_on_load(false);
                int width, height, channels;
                uint8_t *sourceData = nullptr;

                // create new texture
                // Load embedded texture
                const aiTexture *embeddedTexture = aiScene->GetEmbeddedTexture(texFilename.C_Str());
                if (embeddedTexture && textures[textureType]->handle == nullptr)
                {
                    // handle compressed textures
                    if (embeddedTexture->mHeight == 0)
                    {
                        LOG_INFO("[Material Importer] Loading embedded compressed format texture of size {} bytes", embeddedTexture->mWidth);
                        textures[textureType]->data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(embeddedTexture->pcData),
                            embeddedTexture->mWidth, &width, &height, &channels, 4);

                        textures[textureType]->rowPitch = width * 4u;
                    }
                    else
                    {
                        width = static_cast<int>(embeddedTexture->mWidth);
                        height = static_cast<int>(embeddedTexture->mHeight);

                        LOG_INFO("[Material Importer] Loading embedded uncompressed texture of size {}x{}", width, height);

                        // Allocate space for RGBA8 data
                        uint8_t *destinationData = new uint8_t[width * height * 4];

                        // Assimp embedded uncompressed texture data is usually in RGB format without alpha
                        // You can test with alpha channel (or assume RGB with alpha set to 255)
                        for (int p = 0; p < width * height; ++p)
                        {
                            destinationData[p * 4 + 0] = sourceData[p * 3 + 0]; // R
                            destinationData[p * 4 + 1] = sourceData[p * 3 + 1]; // G
                            destinationData[p * 4 + 2] = sourceData[p * 3 + 2]; // B
                            destinationData[p * 4 + 3] = 255;                   // A
                        }

                        textures[textureType]->data = destinationData;
                        textures[textureType]->rowPitch = width * 4u;

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
                        LOG_ERROR("[Material Importer] texture path is not found! \"{}\"", textureFilepath.generic_string());
                        return;
                    }

                    LOG_INFO("[Material Importer] Load texture from \"{}\"", textureFilepath.generic_string());
                    sourceData = stbi_load(textureFilepath.generic_string().c_str(), &width, &height, &channels, 4);
                    LOG_ASSERT(sourceData, "[Material Importer] Failed to load texture");
                }

                if (sourceData)
                {
                    textures[textureType]->data = sourceData;
                    LOG_ASSERT(textures[textureType]->data, "[Material Importer] Failed to load texture");
                }

                if (textures[textureType]->data)
                {
                    textures[textureType]->width = static_cast<uint32_t>(width);
                    textures[textureType]->height = static_cast<uint32_t>(height);
                    // textures[textureType]->buffer.Size = static_cast<uint64_t>(width * height) * 4u;
                    textures[textureType]->rowPitch = width * 4u;

                    s_TextureCache[texFilename.C_Str()] = textures[textureType];
                }
            }
        }
    }

    void Material::CreateTextures()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // Create loaded textures
        for (const auto &tex : textures | std::views::values)
        {
            // use white texture
            if (tex->data == nullptr)
            {
                tex->handle = Renderer::GetWhiteTexture()->GetHandle();
                continue;
            }

            // create texture
            auto textureDesc = nvrhi::TextureDesc();
            textureDesc.setDimension(nvrhi::TextureDimension::Texture2D);
            textureDesc.setWidth(tex->width);
            textureDesc.setHeight(tex->height);
            textureDesc.setFormat(nvrhi::Format::RGBA8_UNORM);
            textureDesc.setInitialState(nvrhi::ResourceStates::ShaderResource);
            textureDesc.setKeepInitialState(true);
            textureDesc.setMipLevels(mipLevels);
            textureDesc.setDebugName("Material embedded Texture");

            tex->handle = device->createTexture(textureDesc);
            LOG_ASSERT(tex->handle, "[Material Importer] Failed to create texture!");
        }

        nvrhi::CommandListHandle commandList = device->createCommandList();
        commandList->open();
        WriteTexture(commandList);
        commandList->close();
        device->executeCommandList(commandList);
    }

    void Material::UpdateBindingSet()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        auto desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, paramsBuffer));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, textures[MaterialTextureType::BaseColor]->handle));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, textures[MaterialTextureType::Specular]->handle));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, textures[MaterialTextureType::Emissive]->handle));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, textures[MaterialTextureType::Roughness]->handle));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, textures[MaterialTextureType::Normals]->handle));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, SceneRenderer::GetActive()->GetEnvironment()->GetHDRTexture()));
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

        textures[textureType]->handle = texture->GetHandle();

        UpdateBindingSet();
    }

    void Material::WriteTexture(nvrhi::ICommandList *commandList)
    {
        for (auto& texture : textures | std::views::values)
        {
            if (!texture->data)
                continue;

            Material::UploadTextureWithMips(commandList, texture->handle, texture->data,
                texture->width, texture->height, texture->rowPitch, nvrhi::Format::RGBA8_UNORM, mipLevels);
        }

        UpdateBindingSet();
    }

    void Material::WriteBuffer(nvrhi::ICommandList* commandList)
    {
        commandList->writeBuffer(paramsBuffer, &params, sizeof(params));
    }

    void Material::UploadTextureWithMips(nvrhi::ICommandList *commandList,
                                         const nvrhi::TextureHandle &handle, const void *baseData,
                                         uint32_t baseWidth, uint32_t baseHeight, uint32_t baseRowPitch,
                                         nvrhi::Format format, uint32_t mipLevels)
    {
        // Generate all mip levels on CPU
        auto mipChain = CPUMipGenerator::GenerateMipChain(baseData, baseWidth, baseHeight, baseRowPitch, format, mipLevels);

        // Upload all mip levels
        for (uint32_t mip = 0; mip < mipLevels && mip < mipChain.size(); ++mip)
        {
            const auto &mipData = mipChain[mip];
            commandList->writeTexture(handle, 0, mip, mipData.data.data(), mipData.rowPitch);
        }
    }
}
