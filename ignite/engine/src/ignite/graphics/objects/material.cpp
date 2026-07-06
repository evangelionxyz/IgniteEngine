// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "material.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/application.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/iscene_renderer.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/serializer/serializer.hpp"

#include <stb_image.h>

namespace ignite
{
    Material::Material()
    {
    }

    Material::~Material()
    {
        if (auto* device = DeviceManager::GetInstance()->GetDevice())
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

    bool Material::UpdateBindingSet(Ref<Texture> envMap, Ref<Texture> shadowMap)
    {
        Texture* envPtr = envMap ? envMap.get() : nullptr;
        Texture* shadowPtr = shadowMap ? shadowMap.get() : nullptr;
        auto key = std::make_pair(envPtr, shadowPtr);

        if (m_BindingSetDirty)
        {
            m_BindingSets.clear();
        }
        else
        {
            auto it = m_BindingSets.find(key);
            if (it != m_BindingSets.end())
            {
                m_BindingSet = it->second;
                return true;
            }
        }

        auto isTextureReady = [](AssetHandle textureHandle, Ref<Texture> &outTexture, Ref<Texture> fallback) -> bool
        {
            if (textureHandle == AssetHandle(0))
            {
                outTexture = fallback;
                return false;
            }

			Ref<Texture> result = AssetManager::GetInstance()->GetAsset<Texture>(textureHandle);
            const bool ready = result && result->IsReady();
			if (result && result->IsReady())
			{
                AssetManager::GetInstance()->AddAssetPin(textureHandle, std::format("material_{}", static_cast<uint64_t>(textureHandle)));
				outTexture = result;
			}
            return ready;
        };

        Ref<Texture> baseColor, emissive, metallic, roughness, normal, occlusion;
		bool allTexturesReady = isTextureReady(baseColorTextureHandle, baseColor, Renderer::GetWhiteTexture());
        allTexturesReady &= isTextureReady(emissiveTextureHandle, emissive, Renderer::GetBlackTexture());
        allTexturesReady &= isTextureReady(metallicTextureHandle, metallic, Renderer::GetBlackTexture());
        allTexturesReady &= isTextureReady(roughnessTextureHandle, roughness, Renderer::GetBlackTexture());
        allTexturesReady &= isTextureReady(normalTextureHandle, normal, Renderer::GetWhiteTexture());
        allTexturesReady &= isTextureReady(occlusionTextureHandle, occlusion, Renderer::GetWhiteTexture());

        // Waiting for all textures loaded
        // binding set is still not created
        if (!allTexturesReady && !(baseColor && emissive && metallic && roughness && normal && occlusion))
        {
            m_BindingSet = nullptr;
            m_BindingSetDirty = true;
            return false;
        }

        EnsureGpuResources();
        auto device = DeviceManager::GetInstance()->GetDevice();
        GPUUploadSync::DeviceWaitIdle(device);

        nvrhi::BindingSetDesc desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_GPUDataBuffer->GetHandle()));
		desc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, baseColor->GetHandle()));
		desc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, emissive->GetHandle()));
		desc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, metallic->GetHandle()));
		desc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, roughness->GetHandle()));
		desc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, normal->GetHandle()));
		desc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, occlusion->GetHandle()));

        Ref<Texture> environmentTexture = envMap;
        if (!environmentTexture)
            environmentTexture = Renderer::GetBlackTexture();

        Ref<Texture> shadowTexture = shadowMap;
        if (!shadowTexture)
            shadowTexture = Renderer::GetWhiteTexture();

        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(6, environmentTexture->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::Texture_SRV(7, shadowTexture->GetHandle()));

        // Sampler
        desc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));
        desc.addItem(nvrhi::BindingSetItem::Sampler(1, shadowMap ? shadowMap->GetSampler() : sampler));

        auto newBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(EBindingLayout::MATERIAL));
        LOG_ASSERT(newBindingSet, "Failed to create material binding set");

        // All created
        if (newBindingSet)
        {
            m_BindingSet = newBindingSet;
            m_BindingSets[key] = newBindingSet;
            m_BindingSetDirty = false;

            return true;
        }

        return false;
    }

    void Material::UploadToGpu(nvrhi::ICommandList *cmd)
    {
        EnsureGpuResources();
        // Sync blend mode from material type so the shader always has the correct value
        gpuData.blendMode = static_cast<int>(m_Type);
        m_GPUDataBuffer->SetData(cmd, Buffer(&gpuData, sizeof(Material_GPUData)));
    }

    void Material::SetSamplerDesc(const nvrhi::SamplerDesc &desc)
    {
        m_SamplerDesc = desc;
        m_HasSamplerDesc = true;
    }

    bool Material::IsNeedToInvalidate() const
    {
        return m_BindingSetDirty;
    }

    bool Material::Serialize(const ignite::Path &filepath)
    {
        Serializer sr(filepath);

        sr.BeginMap();
        {
            sr.BeginMap("Material"); // MATERIAL START
            sr.AddKeyValue("Version", Application::GetVersion());
            sr.AddKeyValue("Name", name);
            sr.AddKeyValue("Type", static_cast<int>(GetType()));
            sr.AddKeyValue("BaseColorTextureHandle", static_cast<uint64_t>(baseColorTextureHandle));
            sr.AddKeyValue("EmissiveTextureHandle", static_cast<uint64_t>(emissiveTextureHandle));
            sr.AddKeyValue("MetallicTextureHandle", static_cast<uint64_t>(metallicTextureHandle));
            sr.AddKeyValue("RoughnessTextureHandle", static_cast<uint64_t>(roughnessTextureHandle));
            sr.AddKeyValue("NormalTextureHandle", static_cast<uint64_t>(normalTextureHandle));
            sr.AddKeyValue("OcclusionTextureHandle", static_cast<uint64_t>(occlusionTextureHandle));

            sr.BeginMap("GPUData");
            sr.AddKeyValue("BaseColorFactor", gpuData.baseColorFactor);
            sr.AddKeyValue("EmissiveFactor", gpuData.emissiveFactor);
            sr.AddKeyValue("MetallicFactor", gpuData.metallicFactor);
            sr.AddKeyValue("RoughnessFactor", gpuData.roughnessFactor);
            sr.AddKeyValue("OcclusionStrength", gpuData.occlusionStrength);
            sr.AddKeyValue("MetallicChannel", gpuData.metallicChannel);
            sr.AddKeyValue("RoughnessChannel", gpuData.roughnessChannel);
            sr.AddKeyValue("TilingFactorX", gpuData.tilingFactor.x);
            sr.AddKeyValue("TilingFactorY", gpuData.tilingFactor.y);
            sr.EndMap();

            sr.EndMap(); // MATERIAL END
        }
        sr.EndMap();

        SetReadyFlag(true);
        SetDirtyFlag(false);

        sr.Serialize(filepath);
        return true;
    }

    Ref<Material> Material::Deserialize(const ignite::Path &filepath)
    {
        YAML::Node fileNode = Serializer::Deserialize(filepath);
        YAML::Node materialNode = fileNode["Material"];
        if (!materialNode)
        {
            return nullptr;
        }

        Ref<Material> material = CreateRef<Material>();
        if (materialNode["Name"]) material->name = materialNode["Name"].as<std::string>();
        if (materialNode["Type"]) material->SetType(static_cast<MaterialType>(materialNode["Type"].as<int>()));
        if (materialNode["BaseColorTextureHandle"]) material->baseColorTextureHandle = AssetHandle(materialNode["BaseColorTextureHandle"].as<uint64_t>());
        if (materialNode["EmissiveTextureHandle"]) material->emissiveTextureHandle = AssetHandle(materialNode["EmissiveTextureHandle"].as<uint64_t>());
        if (materialNode["MetallicTextureHandle"]) material->metallicTextureHandle = AssetHandle(materialNode["MetallicTextureHandle"].as<uint64_t>());
        if (materialNode["RoughnessTextureHandle"]) material->roughnessTextureHandle = AssetHandle(materialNode["RoughnessTextureHandle"].as<uint64_t>());
        if (materialNode["MetallicRoughnessTextureHandle"])
        {
            const AssetHandle legacyHandle = AssetHandle(materialNode["MetallicRoughnessTextureHandle"].as<uint64_t>());
            if (material->metallicTextureHandle == AssetHandle(0)) material->metallicTextureHandle = legacyHandle;
            if (material->roughnessTextureHandle == AssetHandle(0)) material->roughnessTextureHandle = legacyHandle;
        }
        if (materialNode["NormalTextureHandle"]) material->normalTextureHandle = AssetHandle(materialNode["NormalTextureHandle"].as<uint64_t>());
        if (materialNode["OcclusionTextureHandle"]) material->occlusionTextureHandle = AssetHandle(materialNode["OcclusionTextureHandle"].as<uint64_t>());

        if (YAML::Node gpuDataNode = materialNode["GPUData"])
        {
            if (gpuDataNode["BaseColorFactor"]) material->gpuData.baseColorFactor = gpuDataNode["BaseColorFactor"].as<glm::vec4>();
            if (gpuDataNode["EmissiveFactor"]) material->gpuData.emissiveFactor = gpuDataNode["EmissiveFactor"].as<glm::vec4>();
            if (gpuDataNode["MetallicFactor"]) material->gpuData.metallicFactor = gpuDataNode["MetallicFactor"].as<float>();
            if (gpuDataNode["RoughnessFactor"]) material->gpuData.roughnessFactor = gpuDataNode["RoughnessFactor"].as<float>();
            if (gpuDataNode["OcclusionStrength"]) material->gpuData.occlusionStrength = gpuDataNode["OcclusionStrength"].as<float>();
            if (gpuDataNode["MetallicChannel"]) material->gpuData.metallicChannel = gpuDataNode["MetallicChannel"].as<int>();
            if (gpuDataNode["RoughnessChannel"]) material->gpuData.roughnessChannel = gpuDataNode["RoughnessChannel"].as<int>();
            if (gpuDataNode["TilingFactorX"]) material->gpuData.tilingFactor.x = gpuDataNode["TilingFactorX"].as<float>();
            if (gpuDataNode["TilingFactorY"]) material->gpuData.tilingFactor.y = gpuDataNode["TilingFactorY"].as<float>();
        }

        material->SetReadyFlag(true);
        material->SetDirtyFlag(false);

        return material;
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
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(2)) // metallicTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(3)) // roughnessTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(4)) // normalMapTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(5)) // occlusionTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(6)) // environmentMapTexture
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(7)) // csm
            .addItem(nvrhi::BindingLayoutItem::Sampler(0)) // sampler
            .addItem(nvrhi::BindingLayoutItem::Sampler(1)); // csm sampler
        return bindingLayoutDesc;
    }

    void Material::EnsureGpuResources()
    {
        if (!sampler)
        {
            auto device = DeviceManager::GetInstance()->GetDevice();
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
