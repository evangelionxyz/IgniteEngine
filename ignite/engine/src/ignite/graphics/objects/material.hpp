// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_MATERIAL_HPP
#define IGN_MATERIAL_HPP

#include "ignite/core/base.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>
#include <map>

namespace ignite
{
    class Texture;

    enum class MaterialType
    {
        Opaque = 0,
        Transparent,
        Masked
    };

    class IGN_API Material : public Asset
    {
    public:
        Material();
    	virtual ~Material() override;

        std::string name;

        AssetHandle baseColorTextureHandle = AssetHandle(0);
        AssetHandle emissiveTextureHandle = AssetHandle(0);
        AssetHandle metallicTextureHandle = AssetHandle(0);
        AssetHandle roughnessTextureHandle = AssetHandle(0);
        AssetHandle normalTextureHandle = AssetHandle(0);
		AssetHandle occlusionTextureHandle = AssetHandle(0);

        nvrhi::SamplerHandle sampler;
        void SetSamplerDesc(const nvrhi::SamplerDesc &desc);

        bool UpdateBindingSet(Ref<Texture> envMap = nullptr, Ref<Texture> shadowMap = nullptr);
        void UploadToGpu(nvrhi::ICommandList *cmd);
        void SetType(MaterialType type) { m_Type = type; }

        bool IsNeedToInvalidate() const;
        void InvalidateBindingSet() { m_BindingSetDirty = true; m_BindingSet = nullptr; m_BindingSets.clear(); }
        bool IsBindingSetDirty() const { return m_BindingSetDirty; }
        void SetBindingSetClean() { m_BindingSetDirty = false; }

        nvrhi::BindingSetHandle GetBindingSet() { return m_BindingSet; }
        Ref<ConstantBuffer> GetGPUDataBuffer() { return m_GPUDataBuffer; }
        MaterialType GetType() const { return m_Type; }
		Material_GPUData gpuData;

        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetAssetType() override { return GetStaticType(); }

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<Material> Deserialize(const std::filesystem::path &filepath);

    private:
        void EnsureGpuResources();

        MaterialType m_Type = MaterialType::Opaque;
        Ref<ConstantBuffer> m_GPUDataBuffer;
        nvrhi::BindingSetHandle m_BindingSet;
        std::map<std::pair<Texture*, Texture*>, nvrhi::BindingSetHandle> m_BindingSets;
        nvrhi::SamplerDesc m_SamplerDesc{};
        bool m_BindingSetDirty = true;
        bool m_HasSamplerDesc = false;
    };
}

#endif
