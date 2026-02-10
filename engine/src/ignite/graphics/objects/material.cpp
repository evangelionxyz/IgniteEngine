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

#include "ignite/asset/asset_manager.hpp"
#include "ignite/project/project.hpp"

#include "material.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/scene_renderer.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include <stb_image.h>

namespace ignite
{
    Material::Material()
    {
    }

    Material::~Material()
    {
        if (auto* device = Application::GetGraphicsDevice())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }
        
        // Clear binding set first (references other resources)
        m_BindingSet = nullptr;
        
        // Clear GPU data buffer
        m_GPUDataBuffer.reset();
        
        // Clear sampler
        sampler = nullptr;
    }

    void Material::UpdateBindingSet(SceneRenderer *sceneRenderer, MaterialTextures *textures)
    {
        EnsureGpuResources();
        auto device = Application::GetGraphicsDevice();

        nvrhi::BindingSetDesc desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_GPUDataBuffer->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, textures->baseColor->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, textures->emissive->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, textures->metallicRoughness->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, textures->normal->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, textures->occlusion->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, sceneRenderer->GetEnvironmentMapColorTexture()->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(6, sceneRenderer->GetCascadedShadowMapDepthTexture()->GetHandle()));

        // Sampler
        desc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));
        desc.addItem(nvrhi::BindingSetItem::Sampler(1, sceneRenderer->GetCascadedShadowMap()->GetDepthSampler()));
        
        auto newBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MATERIAL));
        LOG_ASSERT(newBindingSet, "Failed to create material binding set");

        if (newBindingSet)
        {
            m_BindingSet = newBindingSet;
        }
    }

    void Material::UploadToGpu(nvrhi::ICommandList* cmd)
    {
        EnsureGpuResources();
        m_GPUDataBuffer->SetData(cmd, Buffer(&gpuData, sizeof(Material_GPUData)));
    }

    void Material::SetSamplerDesc(const nvrhi::SamplerDesc &desc)
    {
        m_SamplerDesc = desc;
        m_HasSamplerDesc = true;
    }

	void Material::RetrieveTextures(AssetManager *assetManager, MaterialTextures *textures) const
	{
        textures->baseColor = RetrieveTexture(assetManager, baseColorTextureHandle, Renderer::GetWhiteTexture());
        textures->emissive = RetrieveTexture(assetManager, emissiveTextureHandle, Renderer::GetBlackTexture());
        textures->metallicRoughness = RetrieveTexture(assetManager, metallicRoughnessTextureHandle, Renderer::GetBlackTexture());
        textures->normal = RetrieveTexture(assetManager, normalTextureHandle, Renderer::GetWhiteTexture());
        textures->occlusion = RetrieveTexture(assetManager, occlusionTextureHandle, Renderer::GetWhiteTexture());
	}

	bool Material::IsNeedToInvalidate()
	{
        // Check if any texture handles have changed and are newly loaded
        auto* assetManager = &Project::GetInstance()->GetAssetManager();
        
        bool needsInvalidation = false;
        
        // Check each texture to see if it was recently loaded
        if (baseColorTextureHandle != 0 && assetManager->IsAssetLoaded(baseColorTextureHandle))
        {
            needsInvalidation = true;
        }
        if (emissiveTextureHandle != 0 && assetManager->IsAssetLoaded(emissiveTextureHandle))
        {
            needsInvalidation = true;
        }
        if (metallicRoughnessTextureHandle != 0 && assetManager->IsAssetLoaded(metallicRoughnessTextureHandle))
        {
            needsInvalidation = true;
        }
        if (normalTextureHandle != 0 && assetManager->IsAssetLoaded(normalTextureHandle))
        {
            needsInvalidation = true;
        }
        if (occlusionTextureHandle != 0 && assetManager->IsAssetLoaded(occlusionTextureHandle))
        {
            needsInvalidation = true;
        }

        return needsInvalidation && m_BindingSetDirty;
	}

	Ref<Texture> Material::RetrieveTexture(AssetManager *assetManager, AssetHandle handle, Ref<Texture> fallback)
	{
		if (handle == 0)
		{
			return fallback;
		}

		Ref<Texture> result = assetManager->GetProject()->GetAsset<Texture>(handle);
		if (result && result->IsReady())
		{
			return result;
		}
		return fallback;
	}

	nvrhi::BindingLayoutDesc Material::GetBindingLayoutDesc()
    {
        auto bindingLayoutDesc = nvrhi::BindingLayoutDesc()
            .setRegisterSpace(1) // set 1
            .setRegisterSpaceIsDescriptorSet(true)
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0)) // material
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)) // baseColorTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)) // emissiveTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(2)) // metallicRoughnessTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(3)) // normalMapTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(4)) // occlusionTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(5)) // environmentMapTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(6)) // csm
            .addItem(nvrhi::BindingLayoutItem::Sampler(0)) // sampler
            .addItem(nvrhi::BindingLayoutItem::Sampler(1)); // csm sampler
        return bindingLayoutDesc;
    }

    void Material::EnsureGpuResources()
    {
        if (!sampler)
        {
            auto device = Application::GetGraphicsDevice();
            nvrhi::SamplerDesc desc = m_HasSamplerDesc ? m_SamplerDesc : nvrhi::SamplerDesc();
            if (!m_HasSamplerDesc)
            {
                desc.setAllFilters(true);
                desc.setAllAddressModes(nvrhi::SamplerAddressMode::Repeat);
            }

            sampler = device->createSampler(desc);
            LOG_ASSERT(sampler, "Failed to create sampler");
        }

        if (!m_GPUDataBuffer)
        {
            m_GPUDataBuffer = ConstantBuffer::Create(sizeof(Material_GPUData), false, 1, "Material Constant Buffer");
        }
    }
}
