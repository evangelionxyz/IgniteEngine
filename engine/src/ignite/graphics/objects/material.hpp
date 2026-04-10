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

#ifndef MATERIAL_HPP
#define MATERIAL_HPP

#include "ignite/asset/asset.hpp"

#include "ignite/core/application.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>

namespace ignite
{
    class SceneRenderer;
    class AssetManager;
    class Project;

    enum class MaterialType
    {
        Opaque = 0,
        Transparent,
        Masked
    };

    struct MaterialTextures
    {
		Ref<Texture> baseColor;
	    Ref<Texture> emissive;
        Ref<Texture> metallic;
        Ref<Texture> roughness;
	    Ref<Texture> normal;
        Ref<Texture> occlusion;
    };

    class Material : public Asset
    {
    public:
        Material();
    	~Material();

        std::string name;

        AssetHandle baseColorTextureHandle = AssetHandle(0);
        AssetHandle emissiveTextureHandle = AssetHandle(0);
        AssetHandle metallicTextureHandle = AssetHandle(0);
        AssetHandle roughnessTextureHandle = AssetHandle(0);
        AssetHandle normalTextureHandle = AssetHandle(0);
		AssetHandle occlusionTextureHandle = AssetHandle(0);

        nvrhi::SamplerHandle sampler;
        void SetSamplerDesc(const nvrhi::SamplerDesc &desc);

        void UpdateBindingSet(SceneRenderer *sceneRenderer, MaterialTextures *textures, AssetManager *assetManager);
        void UploadToGpu(nvrhi::ICommandList *cmd);
        void SetType(MaterialType type) { m_Type = type; }
		void RetrieveTextures(AssetManager *assetManager, MaterialTextures *textures) const;

        bool IsNeedToInvalidate();
        void InvalidateBindingSet() { m_BindingSetDirty = true; m_BindingSet = nullptr; }
        bool IsBindingSetDirty() const { return m_BindingSetDirty; }
        void SetBindingSetClean() { m_BindingSetDirty = false; }
        
        nvrhi::BindingSetHandle GetBindingSet() { return m_BindingSet; }
        Ref<ConstantBuffer> GetGPUDataBuffer() { return m_GPUDataBuffer; }
        MaterialType GetType() const { return m_Type; }
		Material_GPUData gpuData;

        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetAssetType() override { return GetStaticType(); }

        static Ref<Texture> RetrieveTexture(AssetManager *assetManager, AssetHandle handle, Ref<Texture> fallback);

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<Material> Deserialize(const std::filesystem::path &filepath);

    private:
        void EnsureGpuResources();

        MaterialType m_Type = MaterialType::Opaque;
        Ref<ConstantBuffer> m_GPUDataBuffer;
        nvrhi::BindingSetHandle m_BindingSet;
        nvrhi::SamplerDesc m_SamplerDesc{};
        bool m_BindingSetDirty = true;
        bool m_HasSamplerDesc = false;
    };
}

#endif
