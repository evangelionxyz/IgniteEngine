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

#include "ignite/graphics/texture.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include <string>
#include <filesystem>
#include <nvrhi/nvrhi.h>

namespace ignite {

    class GraphicsPipeline;
    class Scene;
    class ICamera;

    class Environment : public Asset
    {
    public:
        Environment(Scene *scene);
    	~Environment();

        void Begin(nvrhi::ICommandList *commandList, ICamera *camera, nvrhi::IFramebuffer *framebuffer, const Ref<GraphicsPipeline> &pipeline);
        void End();

        void UpdateBindingSet();

        void LoadTexture(const std::string &filepath);
        void WriteBuffer(nvrhi::ICommandList *commandList);

        static Ref<Environment> Create(Scene *scene);
        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc();

        Ref<Texture> GetHDRTexture() { return m_HDRTexture; }

        bool IsInvalidating() const { return m_Invalidating; }

    private:
        bool m_Invalidating = false;

        Ref<VertexBuffer> m_VertexBuffer;
        Ref<IndexBuffer> m_IndexBuffer;
        Ref<Texture> m_HDRTexture;
    	nvrhi::SamplerHandle m_Sampler;
        Scene* m_Scene;

        nvrhi::BindingSetHandle m_BindingSet;
    };
}
