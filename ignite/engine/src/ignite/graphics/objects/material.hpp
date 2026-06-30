/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
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

#ifndef IGN_MATERIAL_HPP
#define IGN_MATERIAL_HPP

#include "ignite/core/base.hpp"
#include "ignite/asset/asset.hpp"

#include "ignite/core/application.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>
#include <map>

namespace ignite
{
    class ISceneRenderer;
    class AssetManager;
    class Project;

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
		MaterialBufferData gpuData;

        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetAssetType() override { return GetStaticType(); }


        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<Material> Deserialize(const ignite::Path &filepath);

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
