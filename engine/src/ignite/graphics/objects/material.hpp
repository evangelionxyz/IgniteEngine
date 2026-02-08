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

#pragma once

#include "ignite/core/application.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/gpu_data.hpp"

#include <glm/glm.hpp>
#include <nvrhi/nvrhi.h>

namespace ignite
{
    enum class MaterialType
    {
        Opaque = 0,
        Transparent,
        Masked
    };

    class Material : public Asset
    {
    public:
        Material();
    	~Material();

        std::string name;

        // TODO: Use asset handle instead of actual resource
        Ref<Texture> baseColorTexture;
        Ref<Texture> emissiveTexture;
        Ref<Texture> metallicRoughnessTexture;
        Ref<Texture> normalTexture;
        Ref<Texture> occlusionTexture;

        nvrhi::SamplerHandle sampler;

        void UpdateBindingSet();
        void UploadToGpu(nvrhi::ICommandList *cmd);
        void SetTextureData(nvrhi::ICommandList *cmd);

        void SetType(MaterialType type) { m_Type = type; }
        
        nvrhi::BindingSetHandle GetBindingSet() { return m_BindingSet; }
        Ref<ConstantBuffer> GetGPUDataBuffer() { return m_GPUDataBuffer; }
        MaterialType GetType() const { return m_Type; }
		Material_GPUData gpuData;

        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

    private:
        MaterialType m_Type = MaterialType::Opaque;
        Ref<ConstantBuffer> m_GPUDataBuffer;
        nvrhi::BindingSetHandle m_BindingSet;
    };
}
