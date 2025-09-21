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

    class Material
    {
    public:
        Material();

        std::string name;

        struct Params
        {
            glm::vec4 baseColorFactor = glm::vec4(1.0f);
            glm::vec4 emissiveFactor = glm::vec4(0.0f);
            float metallicFactor = 1.0f;
            float roughnessFactor = 1.0f;
            float occlusionStrength = 0.0f;
        };

        Ref<Texture> baseColorTexture;
        Ref<Texture> emissiveTexture;
        Ref<Texture> metallicRoughnessTexture;
        Ref<Texture> normalTexture;
        Ref<Texture> occlusionTexture;

        void UpdateBindingSet();
        void UploadToGpu(nvrhi::ICommandList *cmd);
        
        nvrhi::BindingSetHandle GetBindingSet() { return m_BindingSet; }
        Ref<ConstantBuffer> GetConstantBuffer() { return m_ConstantBuffer; }
        MaterialType GetType() const { return m_Type; }
		Params params;

        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

    private:
        MaterialType m_Type = MaterialType::Opaque;
        Ref<ConstantBuffer> m_ConstantBuffer;
        nvrhi::BindingSetHandle m_BindingSet;
    };
}
