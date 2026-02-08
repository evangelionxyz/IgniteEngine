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
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/scene_renderer.hpp"

#include "ignite/graphics/objects/shadow_map.hpp"

#include <stb_image.h>

namespace ignite
{
    Material::Material()
    {
        // Neutral defaults per glTF PBR spec when a texture is absent
        baseColorTexture = Renderer::GetMagentaTexture();         // baseColorFactor will tint
        emissiveTexture = Renderer::GetWhiteTexture();            // no emissive
        metallicRoughnessTexture = Renderer::GetBlackTexture();   // will be overridden if texture present; factors supply values
        normalTexture = Renderer::GetWhiteTexture();              // flat normal
        occlusionTexture = Renderer::GetWhiteTexture();           // full occlusion (no darkening)

        auto device = Application::GetGraphicsDevice();
        auto desc = nvrhi::SamplerDesc();
        desc.setAllFilters(true);
        desc.setAllAddressModes(nvrhi::SamplerAddressMode::Repeat);
        sampler = device->createSampler(desc);
        LOG_ASSERT(sampler, "Failed to create sampler");

        m_GPUDataBuffer = ConstantBuffer::Create(sizeof(Material_GPUData), false, 1, "Material Constant Buffer");
    }

    Material::~Material()
    {
		sampler = nullptr;
    }

    void Material::UpdateBindingSet()
    {
        auto device = Application::GetGraphicsDevice();

        if (!m_GPUDataBuffer)
        {
            m_GPUDataBuffer = ConstantBuffer::Create(sizeof(Material_GPUData), false, 1, "Material Constant Buffer");
        }

        nvrhi::BindingSetDesc desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_GPUDataBuffer->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, baseColorTexture->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, emissiveTexture->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, metallicRoughnessTexture->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, normalTexture->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, occlusionTexture->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, SceneRenderer::GetActive()->GetEnvironmentMapColorTexture()->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(6, SceneRenderer::GetActive()->GetCascadedShadowMapDepthTexture()->GetHandle()));

        // Sampler
        desc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));
        desc.addItem(nvrhi::BindingSetItem::Sampler(1, SceneRenderer::GetActive()->GetCascadedShadowMap()->GetDepthSampler()));
        
        auto newBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MATERIAL));
        LOG_ASSERT(newBindingSet, "Failed to create material binding set");

        if (newBindingSet)
        {
            m_BindingSet = newBindingSet;
        }
    }

    void Material::UploadToGpu(nvrhi::ICommandList* cmd)
    {
        m_GPUDataBuffer->SetData(cmd, Buffer(&gpuData, sizeof(Material_GPUData)));
    }

	void Material::SetTextureData(nvrhi::ICommandList *cmd)
	{
        const uint32_t channelCount = 4;
		baseColorTexture->SetData(cmd, channelCount);
        emissiveTexture->SetData(cmd, channelCount);
		metallicRoughnessTexture->SetData(cmd, channelCount);
        normalTexture->SetData(cmd, channelCount);
        occlusionTexture->SetData(cmd, channelCount);
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
}
