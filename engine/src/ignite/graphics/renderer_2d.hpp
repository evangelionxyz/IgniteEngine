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

#ifndef RENDERER_2D_HPP
#define RENDERER_2D_HPP

#include "ignite/core/types.hpp"
#include "vertex_data.hpp"
#include "graphics_pipeline.hpp"
#include "renderer.hpp"
#include "shader.hpp"
#include "ignite/math/math.hpp"

#include "ignite/math/aabb.hpp"

#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"

namespace ignite
{
    class GraphicsPipeline;
    class DeviceManager;
    class Texture;
    class RenderTarget;

    template<typename VertexType>
    struct BatchRender
    {
        const uint32_t maxCount = 1024 * 3;
        const uint32_t maxVertices = maxCount * 4;
        const uint32_t maxIndices = maxCount * 6;
        uint8_t textureSlotIndex = 1; // 0 for white texture
        uint32_t indexCount = 0;
        uint32_t count = 0;

        VertexType* vertexBufferBase = nullptr;
        VertexType*vertexBufferPtr = nullptr;
        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;
        std::vector<Ref<Texture>> textureSlots;

        ~BatchRender()
        {
            vertexBufferPtr = nullptr;
            delete[] vertexBufferBase;
        }
    };

    class Renderer2D
    {
    public:
        Renderer2D();
        ~Renderer2D();

        void Begin(nvrhi::ICommandList *cmd);
        void Flush(nvrhi::IFramebuffer *framebuffer);
        void End();

        void SetFillMode(nvrhi::RasterFillMode mode) { m_FillMode = mode; ClearPipelineCache(); }

        void DrawBox(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));
        void DrawRect(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));
        void DrawLine(const std::vector<glm::vec3>& positions, const glm::vec4& color = glm::vec4(1.0f));
        void DrawLine(const glm::vec3 &pos0, const glm::vec3 &pos1, const glm::vec4& color = glm::vec4(1.0f));
        void DrawAABB(const AABB& aabb, const glm::vec4& color = glm::vec4(1.0f));

        void DrawCircle(const glm::vec3& position, const glm::vec3 &scale, const glm::vec4& color = glm::vec4(1.0f), float thickness = -1.0f, float fade = 0.005f);
        void DrawCircle(const glm::mat4 &transform, const glm::vec4 &color = glm::vec4(1.0f), float thickness = -1.0f, float fade = 0.005f);

        void DrawQuad(const Rect &rect, float rotation, const glm::vec4 &color, const Ref<Texture> &texture = nullptr, const glm::vec2 &tilingFactor = glm::vec2(1.0f));
        void DrawQuad(const glm::vec3 &position, const glm::vec2 &size, f32 rotation, const glm::vec4 &color, const Ref<Texture>& texture = nullptr, const glm::vec2 &tilingFactor = glm::vec2(1.0f));
        void DrawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, const Ref<Texture>& texture = nullptr, const glm::vec2 &tilingFactor = glm::vec2(1.0f));
        void DrawQuad(const glm::mat4 &transform, const glm::vec4 &color, const Ref<Texture>& texture = nullptr, const glm::vec2 &tilingFactor = glm::vec2(1.0f));

        void InitQuadData();
        void InitLineData();
        void InitCircleData();

        void ClearPipelineCache();
        
        u32 GetOrInsertTexture(const Ref<Texture>& texture);
        
        static Ref<Renderer2D> Create();
        
    private:
        nvrhi::ICommandList *m_Cmd;
        BatchRender<Vertex2DQuad> m_QuadBatch;
        BatchRender<Vertex2DLine> m_LineBatch;
        BatchRender<Vertex2DCircle> m_CircleBatch;
        nvrhi::RasterFillMode m_FillMode = nvrhi::RasterFillMode::Solid;
    };
}

#endif
