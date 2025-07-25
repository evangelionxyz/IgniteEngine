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

#include "lighting.hpp"
#include "texture.hpp"
#include "ignite/asset/asset.hpp"

#include <string>
#include <filesystem>

#include <vector>
#include <nvrhi/nvrhi.h>

namespace ignite {

    class GraphicsPipeline;

    struct EnvironmentParams
    {
        float exposure = 1.0f;
        float gamma = 2.2f;
        float ambient = 0.5f;
    };

    class Environment : public Asset
    {
    public:
        Environment();

        void Render(nvrhi::ICommandList *commandList, nvrhi::IFramebuffer *framebuffer, const Ref<GraphicsPipeline> &pipeline);
        void LoadTexture(const std::string &filepath);
        void WriteBuffer(nvrhi::ICommandList *commandList);
        void SetSunDirection(float pitch, float yaw);

        static Ref<Environment> Create();

        static nvrhi::VertexAttributeDesc GetAttribute();
        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

        EnvironmentParams params;
        DirLight dirLight;

        nvrhi::TextureHandle GetHDRTexture() { return m_HDRTexture->GetHandle(); }
        nvrhi::BufferHandle GetParamsBuffer() { return m_ParamsConstantBuffer; }
        nvrhi::BufferHandle GetDirLightBuffer() { return m_DirLightConstantBuffer; }

        bool isUpdatingTexture = false;
    private:

        nvrhi::BufferHandle m_VertexBuffer;
        nvrhi::BufferHandle m_IndexBuffer;
        nvrhi::BufferHandle m_ParamsConstantBuffer;
        nvrhi::BufferHandle m_DirLightConstantBuffer;

        nvrhi::BindingSetHandle m_BindingSet;

        Ref<Texture> m_HDRTexture;
    };
}
