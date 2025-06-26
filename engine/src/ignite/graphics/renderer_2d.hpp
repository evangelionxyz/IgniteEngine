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

#include "ignite/core/types.hpp"
#include "vertex_data.hpp"
#include "graphics_pipeline.hpp"
#include "renderer.hpp"
#include "shader.hpp"

#include "vertex_buffer.hpp"
#include "index_buffer.hpp"

#include <unordered_map>

namespace ignite
{
    class GraphicsPipeline;
    class DeviceManager;
    class Texture;

    template<typename VertexType>
    struct BatchRender
    {
        const size_t maxCount = 1024 * 3;
        const size_t maxVertices = maxCount * 4;
        const size_t maxIndices = maxCount * 6;
        const i32 maxTextureCount = 16;

        VertexType* vertexBufferBase = nullptr;
        VertexType*vertexBufferPtr = nullptr;
        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;
        nvrhi::SamplerHandle sampler = nullptr;
        std::vector<Ref<Texture>> textureSlots;
        u8 textureSlotIndex = 1; // 0 for white texture
        u32 indexCount = 0;
        u32 count = 0;

        ~BatchRender()
        {
            vertexBufferPtr = nullptr;
            if (vertexBufferBase)
                delete[] vertexBufferBase;

            indexCount = 0;
            count = 0;
        }
    };

    struct Renderer2DData
    {
        BatchRender<Vertex2DQuad> quadBatch;
        nvrhi::BindingSetHandle quadBindingSet;

        BatchRender<Vertex2DLine> lineBatch;
        nvrhi::BindingSetHandle lineBindingSet;

        glm::vec4 quadPositions[4];
    };

    class Renderer2D
    {
    public:
        static void Init();
        static void Shutdown();

        static void Begin(nvrhi::ICommandList *commandList, nvrhi::IFramebuffer* framebuffer);
        static void Flush(Ref<GraphicsPipeline> quadPipeline, Ref<GraphicsPipeline> linePipeline);
        static void End();

        static void DrawBox(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f), uint32_t entityID = 0);
        static void DrawRect(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f), uint32_t entityID = 0);
        static void DrawLine(const std::vector<glm::vec3>& positions, const glm::vec4& color = glm::vec4(1.0f), uint32_t entityID = 0);
        static void DrawLine(const glm::vec3 &pos0, const glm::vec3 &pos1, const glm::vec4& color = glm::vec4(1.0f), uint32_t entityID = 0);

        static void DrawQuad(const glm::vec3 &position, const glm::vec2 &size, f32 rotation, const glm::vec4 &color, Ref<Texture> texture = nullptr, const glm::vec2 &tilingFactor = glm::vec2(1.0f), uint32_t entityID = 0);
        static void DrawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, Ref<Texture> texture = nullptr, const glm::vec2 &tilingFactor = glm::vec2(1.0f), uint32_t entityID = 0);
        static void DrawQuad(const glm::mat4 &transform, const glm::vec4 &color, Ref<Texture> texture = nullptr, const glm::vec2 &tilingFactor = glm::vec2(1.0f), uint32_t entityID = 0);

        static void InitQuadData();
        static void InitLineData();

        static u32 GetOrInsertTexture(Ref<Texture> texture);
        static void UpdateTextureBindings();

    private:
        static nvrhi::ICommandList *renderCommandList;
        static nvrhi::IFramebuffer *renderFramebuffer;

        static Renderer2DData *s_Data;
    };
}
