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

#include "renderer_2d.hpp"
#include "render_target.hpp"
#include "framebuffer_key.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "graphics_pipeline.hpp"

#include "texture.hpp"

#include <stb_image.h>
#include <unordered_map>

namespace ignite
{
    static glm::vec4 QUAD_POSITIONS[4];
    const uint8_t MAX_TEXTURE_BATCH_COUNT = 32;
    static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_LinePSOCache;
    static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_QuadPSOCache;

    static std::unordered_map<nvrhi::IBindingLayout *, nvrhi::BindingSetHandle> s_QuadBindingSetCache;
    static std::unordered_map<nvrhi::IBindingLayout *, nvrhi::BindingSetHandle> s_LineBindingSetCache;

    // Helper to build a quad pipeline for a framebuffer (once) and cache it.
    static Ref<GraphicsPipeline> GetQuadPipelineForFB(nvrhi::IFramebuffer* framebuffer, nvrhi::RasterFillMode fillMode)
    {
        auto key = MakeFramebufferKey(framebuffer);
        auto it = s_QuadPSOCache.find(key);
        if (it != s_QuadPSOCache.end())
            return it->second;
        
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.depthWrite = true;
        params.depthTest = true;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;

        // create binding layout
        nvrhi::BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));

        for (uint8_t i = 0; i < MAX_TEXTURE_BATCH_COUNT; i++)
        {
            bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(i));
        }

        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.comparison = nvrhi::ComparisonFunc::LessOrEqual;

        auto attributes = Vertex2DQuad::GetAttributes();
        GraphicsPipelineCreateInfo createInfo;
        createInfo.attributes = attributes.data();
        createInfo.attributeCount = static_cast<uint32_t>(attributes.size());

        auto shaderContext = Renderer::GetShaderLibrary().Get("batch_2d_quad");
        Ref<GraphicsPipeline> gp = GraphicsPipeline::Create();
        gp->AddShader(shaderContext[nvrhi::ShaderType::Vertex].handle, nvrhi::ShaderType::Vertex)
            .AddShader(shaderContext[nvrhi::ShaderType::Pixel].handle, nvrhi::ShaderType::Pixel)
            .AddBindingLayout(bindingLayout)
            .Build(framebuffer, params, createInfo);

        s_QuadPSOCache.emplace(key, gp);

        return gp;
    }

    // Helper to build a line pipeline for a framebuffer (once) and cache it.
    static Ref<GraphicsPipeline> GetLinePipelineForFB(nvrhi::IFramebuffer* framebuffer)
    {
        auto key = MakeFramebufferKey(framebuffer);
        auto it = s_LinePSOCache.find(key);
        if (it != s_LinePSOCache.end())
            return it->second;
        
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.depthWrite = true;
        params.depthTest = true;
        params.enableDepthStencil = false;

        params.fillMode = nvrhi::RasterFillMode::Wireframe;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.primitiveType = nvrhi::PrimitiveType::LineList;

        auto attributes = Vertex2DLine::GetAttributes();
        GraphicsPipelineCreateInfo createInfo;
        createInfo.attributes = attributes.data();
        createInfo.attributeCount = static_cast<uint32_t>(attributes.size());

        auto shaderContext = Renderer::GetShaderLibrary().Get("batch_2d_line");
        Ref<GraphicsPipeline> gp = GraphicsPipeline::Create();
        nvrhi::BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

        gp->AddShader(shaderContext[nvrhi::ShaderType::Vertex].handle, nvrhi::ShaderType::Vertex)
            .AddShader(shaderContext[nvrhi::ShaderType::Pixel].handle, nvrhi::ShaderType::Pixel)
            .AddBindingLayout(bindingLayout)
            .Build(framebuffer, params, createInfo);

        s_LinePSOCache.emplace(key, gp);

        return gp;
    }

    static nvrhi::BindingSetHandle GetQuadBindingSet(nvrhi::IBindingLayout *bindingLayout, const std::vector<Ref<Texture>> &textures)
    {
        auto it = s_QuadBindingSetCache.find(bindingLayout);
        if (it != s_QuadBindingSetCache.end())
            return it->second;

        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        // then add textures
        const auto samplerDesc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Repeat)
            .setAllFilters(true);

        nvrhi::SamplerHandle sampler = device->createSampler(samplerDesc);

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraConstantBuffer()->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));
        for (uint8_t i = 0; i < MAX_TEXTURE_BATCH_COUNT; ++i)
        {
            Ref<Texture> tex = textures[i];
            if (!tex)
                tex = Renderer::GetWhiteTexture();
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(i, tex->GetHandle()));
        }

        nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
        LOG_ASSERT(bindingSet, "[Renderer 2D] Failed to create binding");

        return bindingSet;
    }

    static nvrhi::BindingSetHandle GetLineBindingSet(nvrhi::IBindingLayout *bindingLayout)
    {
        auto it = s_LineBindingSetCache.find(bindingLayout);
        if (it != s_LineBindingSetCache.end())
            return it->second;

        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        // create binding set
        nvrhi::BindingSetDesc bindingSetDesc;
        // add constant buffer
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraConstantBuffer()->GetHandle()));

        nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
        LOG_ASSERT(bindingSet, "[Renderer 2D] Failed to create binding");

        return bindingSet;
    }

    Ref<Renderer2D> Renderer2D::Create()
    {
        return CreateRef<Renderer2D>();
    }

    Renderer2D::Renderer2D()
    {
        InitQuadData();
        InitLineData();
    }

    Renderer2D::~Renderer2D()
    {
        s_QuadPSOCache.clear();
        s_LinePSOCache.clear();
        
        s_QuadBindingSetCache.clear();
        s_LineBindingSetCache.clear();
    }

    void Renderer2D::InitQuadData()
    {
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        size_t vertAllocSize = m_QuadBatch.maxVertices * sizeof(Vertex2DQuad);
        m_QuadBatch.vertexBufferBase = new Vertex2DQuad[vertAllocSize];

        m_QuadBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);
        m_QuadBatch.indexBuffer = IndexBuffer::Create(m_QuadBatch.maxIndices * sizeof(uint32_t));

        // create texture
        m_QuadBatch.textureSlots.resize(MAX_TEXTURE_BATCH_COUNT);
        m_QuadBatch.textureSlots[0] = Renderer::GetWhiteTexture();

        // write index buffer
        std::vector<uint32_t> indices(m_QuadBatch.maxIndices);

        uint32_t offset = 0;
        for (uint32_t i = 0; i < m_QuadBatch.maxIndices; i += 6)
        {
            indices[0 + i] = offset + 0;
            indices[1 + i] = offset + 1;
            indices[2 + i] = offset + 2;

            indices[3 + i] = offset + 0;
            indices[4 + i] = offset + 3;
            indices[5 + i] = offset + 1;

            offset += 4;
        }

        m_QuadBatch.indexBuffer->SetData(Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
        
        QUAD_POSITIONS[0] = {-0.5f, -0.5f, 0.0f, 1.0f }; // bottom-left
        QUAD_POSITIONS[1] = { 0.5f,  0.5f, 0.0f, 1.0f }; // top-right
        QUAD_POSITIONS[2] = {-0.5f,  0.5f, 0.0f, 1.0f }; // top-left
        QUAD_POSITIONS[3] = { 0.5f, -0.5f, 0.0f, 1.0f }; // bottom-right
    }

    void Renderer2D::InitLineData()
    {
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        size_t vertAllocSize = m_LineBatch.maxVertices * sizeof(Vertex2DLine);
        m_LineBatch.vertexBufferBase = new Vertex2DLine[vertAllocSize];
        m_LineBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);
    }

    void Renderer2D::ClearPipelineCache()
    {
        s_LinePSOCache.clear();
        s_QuadPSOCache.clear();
    }

    void Renderer2D::Begin(nvrhi::ICommandList* cmd)
    {
        // Quad data
        m_QuadBatch.indexCount = 0;
        m_QuadBatch.count = 0;
        m_QuadBatch.vertexBufferPtr = m_QuadBatch.vertexBufferBase;

        // Line data
        m_LineBatch.indexCount = 0;
        m_LineBatch.count = 0;
        m_LineBatch.vertexBufferPtr = m_LineBatch.vertexBufferBase;

        m_Cmd = cmd;
    }

    void Renderer2D::Flush(nvrhi::IFramebuffer *framebuffer)
    {
        const nvrhi::Viewport &viewport = framebuffer->getFramebufferInfo().getViewport();

         if (m_LineBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t*>(m_LineBatch.vertexBufferPtr) - reinterpret_cast<uint8_t*>(m_LineBatch.vertexBufferBase);
            m_LineBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_LineBatch.vertexBufferBase, bufferSize));

            Ref<GraphicsPipeline> gp = GetLinePipelineForFB(framebuffer);
            nvrhi::BindingSetHandle bindingSet = GetLineBindingSet(gp->GetBindingLayout(0));

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_LineBatch.vertexBuffer->GetHandle(), 0, 0});
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_LineBatch.indexCount;
            args.instanceCount = 1;

            m_Cmd->draw(args);
        }

        if (m_QuadBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t*>(m_QuadBatch.vertexBufferPtr) - reinterpret_cast<uint8_t*>(m_QuadBatch.vertexBufferBase);
            m_QuadBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_QuadBatch.vertexBufferBase, bufferSize));

            Ref<GraphicsPipeline> gp = GetQuadPipelineForFB(framebuffer, m_FillMode);
            nvrhi::BindingSetHandle bindingSet = GetQuadBindingSet(gp->GetBindingLayout(0), m_QuadBatch.textureSlots);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_QuadBatch.vertexBuffer->GetHandle(), 0, 0})
                .setIndexBuffer({ m_QuadBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT});
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_QuadBatch.indexCount;
            args.instanceCount = 1;

            m_Cmd->drawIndexed(args);
        }
    }

    void Renderer2D::End()
    {
        m_QuadBatch.indexCount = 0;
        m_QuadBatch.count = 0;

        m_LineBatch.indexCount = 0;
        m_LineBatch.count = 0;
    }
    
    void Renderer2D::DrawBox(const glm::mat4& transform, const glm::vec4& color)
    {
        if (m_LineBatch.count >= m_LineBatch.maxCount)
            Renderer2D::End();

        static glm::vec4 cubeVertices[8] =
        {
            {-0.5f, -0.5f, -0.5f, 1.0f}, // 0
            { 0.5f, -0.5f, -0.5f, 1.0f}, // 1
            { 0.5f,  0.5f, -0.5f, 1.0f}, // 2
            {-0.5f,  0.5f, -0.5f, 1.0f}, // 3
            {-0.5f, -0.5f,  0.5f, 1.0f}, // 4
            { 0.5f, -0.5f,  0.5f, 1.0f}, // 5
            { 0.5f,  0.5f,  0.5f, 1.0f}, // 6
            {-0.5f,  0.5f,  0.5f, 1.0f}  // 7
        };

        static int edgeIndices[12][2] =
        {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, // bottom face
            {4, 5}, {5, 6}, {6, 7}, {7, 4}, // top face
            {0, 4}, {1, 5}, {2, 6}, {3, 7}  // vertical edges
        };

        for (int i = 0; i < 12; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                glm::vec4 position = transform * cubeVertices[edgeIndices[i][j]];
                m_LineBatch.vertexBufferPtr->position = position;
                m_LineBatch.vertexBufferPtr->color = color;
                m_LineBatch.vertexBufferPtr++;
                m_LineBatch.indexCount++;
            }
        }

        m_LineBatch.count++;
    }

    void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color)
    {
        if (m_LineBatch.count >= m_LineBatch.maxCount)
            Renderer2D::End();

        static constexpr int indices[8][2] =
        {
            {0, 2},
            {2, 1},
            {1, 3},
            {3, 0}
        };

        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                glm::vec4 position = transform * QUAD_POSITIONS[indices[i][j]];
                m_LineBatch.vertexBufferPtr->position = position;
                m_LineBatch.vertexBufferPtr->color = color;
                m_LineBatch.vertexBufferPtr++;
                m_LineBatch.indexCount++;
            }
        }

        m_LineBatch.count++;
    }

    void Renderer2D::DrawLine(const std::vector<glm::vec3>& positions, const glm::vec4& color)
    {
        if (m_LineBatch.count >= m_LineBatch.maxCount)
            Renderer2D::End();

        for (auto& pos : positions)
        {
            m_LineBatch.vertexBufferPtr->position = pos;
            m_LineBatch.vertexBufferPtr->color = color;
            m_LineBatch.vertexBufferPtr++;

            m_LineBatch.indexCount++;
        }

        m_LineBatch.count++;
    }

    void Renderer2D::DrawLine(const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec4& color)
    {
        if (m_LineBatch.count >= m_LineBatch.maxCount)
            Renderer2D::End();

        m_LineBatch.vertexBufferPtr->position = pos0;
        m_LineBatch.vertexBufferPtr->color = color;
        m_LineBatch.vertexBufferPtr++;

        m_LineBatch.vertexBufferPtr->position = pos1;
        m_LineBatch.vertexBufferPtr->color = color;
        m_LineBatch.vertexBufferPtr++;

        m_LineBatch.indexCount += 2;
        m_LineBatch.count++;
    }

    void Renderer2D::DrawAABB(const AABB& aabb, const glm::vec4& color /*= glm::vec4(1.0f)*/)
    {
        // Bottom face
        DrawLine({ {aabb.min.x, aabb.min.y, aabb.min.z}, {aabb.max.x, aabb.min.y, aabb.min.z} }, color);
        DrawLine({ {aabb.max.x, aabb.min.y, aabb.min.z}, {aabb.max.x, aabb.min.y, aabb.max.z} }, color);
        DrawLine({ {aabb.max.x, aabb.min.y, aabb.max.z}, {aabb.min.x, aabb.min.y, aabb.max.z} }, color);
        DrawLine({ {aabb.min.x, aabb.min.y, aabb.max.z}, {aabb.min.x, aabb.min.y, aabb.min.z} }, color);

        // Top face
        DrawLine({ {aabb.min.x, aabb.max.y, aabb.min.z}, {aabb.max.x, aabb.max.y, aabb.min.z} }, color);
        DrawLine({ {aabb.max.x, aabb.max.y, aabb.min.z}, {aabb.max.x, aabb.max.y, aabb.max.z} }, color);
        DrawLine({ {aabb.max.x, aabb.max.y, aabb.max.z}, {aabb.min.x, aabb.max.y, aabb.max.z} }, color);
        DrawLine({ {aabb.min.x, aabb.max.y, aabb.max.z}, {aabb.min.x, aabb.max.y, aabb.min.z} }, color);

        // Vertical edges
        DrawLine({ {aabb.min.x, aabb.min.y, aabb.min.z}, {aabb.min.x, aabb.max.y, aabb.min.z} }, color);
        DrawLine({ {aabb.max.x, aabb.min.y, aabb.min.z}, {aabb.max.x, aabb.max.y, aabb.min.z} }, color);
        DrawLine({ {aabb.max.x, aabb.min.y, aabb.max.z}, {aabb.max.x, aabb.max.y, aabb.max.z} }, color);
        DrawLine({ {aabb.min.x, aabb.min.y, aabb.max.z}, {aabb.min.x, aabb.max.y, aabb.max.z} }, color);
    }

    void Renderer2D::DrawQuad(const Rect &rect, float rotation, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &tilingFactor)
    {
        if (m_QuadBatch.count >= m_QuadBatch.maxCount)
            Renderer2D::End();

        static constexpr uint32_t quadVertexCount = 4;
        static constexpr glm::vec2 textureCoords[] =
        {
            { 0.0f, 1.0f },
            { 1.0f, 0.0f },
            { 0.0f, 0.0f },
            { 1.0f, 1.0f }
        };

        const glm::vec4 positions[4] =
        {
            { rect.min.x, rect.min.y, 0.0f, 1.0f }, // bottom-left
            { rect.max.x, rect.max.y, 0.0f, 1.0f }, // top-right
            { rect.min.x, rect.max.y, 0.0f, 1.0f }, // top-left
            { rect.max.x, rect.min.y, 0.0f, 1.0f }, // bottom-right
        };

        uint32_t texIndex = GetOrInsertTexture(texture);

        for (uint32_t i = 0; i < quadVertexCount; ++i)
        {
            m_QuadBatch.vertexBufferPtr->position     = positions[i];
            m_QuadBatch.vertexBufferPtr->texCoord     = textureCoords[i];
            m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            m_QuadBatch.vertexBufferPtr->color        = color;
            m_QuadBatch.vertexBufferPtr->texIndex     = texIndex;
            m_QuadBatch.vertexBufferPtr++;
        }

        m_QuadBatch.indexCount += 6;
        m_QuadBatch.count++;
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, f32 rotation, const glm::vec4& color, const Ref<Texture>& texture, const glm::vec2& tilingFactor)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) 
            * glm::rotate(glm::mat4(1.0f), rotation, {0.0f, 0.0f, 1.0f })
            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        DrawQuad(transform, color, texture, tilingFactor);
    }

    void Renderer2D::DrawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, const Ref<Texture>& texture, const glm::vec2 &tilingFactor)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        DrawQuad(transform, color, texture, tilingFactor);
    }
    
    void Renderer2D::DrawQuad(const glm::mat4 &transform, const glm::vec4 &color, const Ref<Texture>& texture, const glm::vec2 &tilingFactor)
    {
        if (m_QuadBatch.count >= m_QuadBatch.maxCount)
            Renderer2D::End();

        static constexpr uint32_t quadVertexCount = 4;
        static constexpr glm::vec2 textureCoords[] = {
            { 0.0f, 1.0f },
            { 1.0f, 0.0f },
            { 0.0f, 0.0f },
            { 1.0f, 1.0f }
        };

        uint32_t texIndex = GetOrInsertTexture(texture);

        for (uint32_t i = 0; i < quadVertexCount; ++i)
        {
            m_QuadBatch.vertexBufferPtr->position     = transform * QUAD_POSITIONS[i];
            m_QuadBatch.vertexBufferPtr->texCoord     = textureCoords[i];
            m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            m_QuadBatch.vertexBufferPtr->color        = color;
            m_QuadBatch.vertexBufferPtr->texIndex     = texIndex;
            m_QuadBatch.vertexBufferPtr++;
        }

        m_QuadBatch.indexCount += 6;
        m_QuadBatch.count++;
    }

    uint32_t Renderer2D::GetOrInsertTexture(const Ref<Texture>& texture)
    {
        if (texture == nullptr)
            return 0;

        uint32_t textureIndex = 0;

        // find texture
        for (uint32_t i = 0; i < m_QuadBatch.textureSlotIndex; ++i)
        {
            if (*m_QuadBatch.textureSlots[i] == *texture)
            {
                textureIndex = i;
                break;
            }
        }

        // insert if not found
        if (textureIndex == 0)
        {
            if (m_QuadBatch.textureSlotIndex >= MAX_TEXTURE_BATCH_COUNT)
            {
                End();
                return MAX_TEXTURE_BATCH_COUNT;
            }
            
            textureIndex = m_QuadBatch.textureSlotIndex;
            m_QuadBatch.textureSlots[m_QuadBatch.textureSlotIndex] = texture;
            m_QuadBatch.textureSlotIndex++;

            s_QuadBindingSetCache.clear(); // reset (so we can recreate it)
        }

        return textureIndex;
    }
}
