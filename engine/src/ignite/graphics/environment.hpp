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

#include "index_buffer.hpp"
#include "vertex_buffer.hpp"
#include "constant_buffer.hpp"

#include <string>
#include <filesystem>

#include <vector>
#include <nvrhi/nvrhi.h>

namespace ignite {

    class GraphicsPipeline;
    class ICamera;

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

        void Begin(nvrhi::ICommandList *commandList, ICamera *camera, nvrhi::IFramebuffer *framebuffer, const Ref<GraphicsPipeline> &pipeline);
        void End();

        void LoadTexture(const std::string &filepath);
        void WriteBuffer(nvrhi::ICommandList *commandList);
        void SetSunDirection(float pitch, float yaw);

        static Ref<Environment> Create();

        static nvrhi::VertexAttributeDesc GetAttribute();
        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

        EnvironmentParams params;
        DirLight dirLight;

        Ref<Texture> GetHDRTexture() { return m_HDRTexture; }
        Ref<ConstantBuffer> GetParamsBuffer() { return m_ParamsConstantBuffer; }
        Ref<ConstantBuffer> GetDirLightBuffer() { return m_DirLightConstantBuffer; }

        bool IsInvalidating() const { return m_Invalidating; }

    private:
        bool m_Invalidating = false;

        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<ConstantBuffer> m_ParamsConstantBuffer;
        Ref<ConstantBuffer> m_DirLightConstantBuffer;
        Ref<Texture> m_HDRTexture;

        nvrhi::BindingSetHandle m_BindingSet;
    };
}
