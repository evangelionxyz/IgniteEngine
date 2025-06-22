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

#include "renderer.hpp"
#include "scene_renderer.hpp"

namespace ignite
{
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
