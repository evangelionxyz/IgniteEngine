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
#include "ignite/scene/icamera.hpp"

#include <stb_image.h>

#include "shader_factory.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "graphics_pipeline.hpp"

#include "texture.hpp"

namespace ignite
{
    nvrhi::ICommandList *Renderer2D::renderCommandList = nullptr;
    nvrhi::IFramebuffer *Renderer2D::renderFramebuffer = nullptr;

    struct Renderer2DData
    {
        BatchRender<Vertex2DQuad> quadBatch;
        BatchRender<Vertex2DLine> lineBatch;
        CameraConstants cameraConstants;
        glm::vec4 quadPositions[4];
        const uint8_t MAX_TEXTURE_COUNT = 32;
    };

    static Renderer2DData* s_Data;

    void Renderer2D::Init()
    {
        s_Data = new Renderer2DData();
        InitQuadData();
        InitLineData();
    }

    void Renderer2D::Shutdown()
    {
        delete s_Data;
    }

    void Renderer2D::CreatePipelines(nvrhi::IFramebuffer* framebuffer)
    {
        s_Data->quadBatch.pipeline->CreatePipeline(framebuffer);
        s_Data->lineBatch.pipeline->CreatePipeline(framebuffer);
    }

    void Renderer2D::SetFillMode(nvrhi::RasterFillMode mode)
    {
        s_Data->quadBatch.pipeline->GetParams().fillMode = mode;
        s_Data->lineBatch.pipeline->GetParams().fillMode = mode;
    }

    void Renderer2D::InitQuadData()
    {
        nvrhi::IDevice* device = Application::GetGraphicsDevice();
        nvrhi::CommandListHandle commandList = device->createCommandList();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.depthWrite = true;
        params.depthTest = true;
        params.enableDepthStencil = false;

        // create binding layout
        nvrhi::BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(CameraConstants)));
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));

        for (uint8_t i = 0; i < s_Data->MAX_TEXTURE_COUNT; i++)
        {
            bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(i));
        }

        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.comparison = nvrhi::ComparisonFunc::LessOrEqual;

        auto attributes = Vertex2DQuad::GetAttributes();
        GraphicsPipelineCreateInfo pci;
        pci.attributes = attributes.data();
        pci.attributeCount = static_cast<uint32_t>(attributes.size());

        auto shaderContext = Renderer::GetShaderLibrary().Get("batch_2d_quad");
        s_Data->quadBatch.pipeline = GraphicsPipeline::Create(params, &pci);
        s_Data->quadBatch.pipeline->AddShader(shaderContext[nvrhi::ShaderType::Vertex].handle, nvrhi::ShaderType::Vertex)
            .AddShader(shaderContext[nvrhi::ShaderType::Pixel].handle, nvrhi::ShaderType::Pixel)
            .AddBindingLayout(bindingLayout)
            .Build();
        
        size_t vertAllocSize = s_Data->quadBatch.maxVertices * sizeof(Vertex2DQuad);
        s_Data->quadBatch.vertexBufferBase = new Vertex2DQuad[vertAllocSize];

        s_Data->quadBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);
        s_Data->quadBatch.indexBuffer = IndexBuffer::Create(s_Data->quadBatch.maxIndices * sizeof(uint32_t));

        // create texture
        s_Data->quadBatch.textureSlots.resize(s_Data->MAX_TEXTURE_COUNT);
        s_Data->quadBatch.textureSlots[0] = Renderer::GetWhiteTexture();

        // write index buffer
        uint32_t *indices = new uint32_t[s_Data->quadBatch.maxIndices];

        uint32_t offset = 0;
        for (uint32_t i = 0; i < s_Data->quadBatch.maxIndices; i += 6)
        {
            indices[0 + i] = offset + 0;
            indices[1 + i] = offset + 1;
            indices[2 + i] = offset + 2;

            indices[3 + i] = offset + 0;
            indices[4 + i] = offset + 3;
            indices[5 + i] = offset + 1;

            offset += 4;
        }

        s_Data->quadBatch.indexBuffer->SetData(Buffer(indices, s_Data->quadBatch.maxIndices * sizeof(uint32_t)));
        
        delete[] indices;

        // create binding sets
        const auto samplerDesc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Repeat)
            .setAllFilters(true);

        nvrhi::SamplerHandle sampler = device->createSampler(samplerDesc);

        // create binding set
        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(CameraConstants)));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));

        for (uint8_t i = 0; i < s_Data->MAX_TEXTURE_COUNT; i++)
        {
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(
                i,
                Renderer::GetWhiteTexture()->GetHandle(),
                nvrhi::Format::UNKNOWN,
                nvrhi::AllSubresources,
                nvrhi::TextureDimension::Texture2D));
        }

        s_Data->quadBatch.bindingSet = device->createBindingSet(bindingSetDesc, s_Data->quadBatch.pipeline->GetBindingLayout(0));
        LOG_ASSERT(s_Data->quadBatch.bindingSet, "[Renderer 2D] Failed to create binding set");
        
        s_Data->quadPositions[0] = {-0.5f, -0.5f, 0.0f, 1.0f }; // bottom-left
        s_Data->quadPositions[1] = { 0.5f,  0.5f, 0.0f, 1.0f }; // top-right
        s_Data->quadPositions[2] = {-0.5f,  0.5f, 0.0f, 1.0f }; // top-left
        s_Data->quadPositions[3] = { 0.5f, -0.5f, 0.0f, 1.0f }; // bottom-right
    }

    void Renderer2D::InitLineData()
    {
        nvrhi::IDevice* device = Application::GetGraphicsDevice();
        nvrhi::CommandListHandle commandList = device->createCommandList();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.depthWrite = true;
        params.depthTest = true;
        params.enableDepthStencil = false;

        params.fillMode = nvrhi::RasterFillMode::Wireframe;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.primitiveType = nvrhi::PrimitiveType::LineList;

        auto attributes = Vertex2DLine::GetAttributes();
        GraphicsPipelineCreateInfo pci;
        pci.attributes = attributes.data();
        pci.attributeCount = static_cast<uint32_t>(attributes.size());

        s_Data->lineBatch.pipeline = GraphicsPipeline::Create(params, &pci);

        auto shaderContext = Renderer::GetShaderLibrary().Get("batch_2d_line");

        nvrhi::BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(CameraConstants)));
        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

        s_Data->lineBatch.pipeline->AddShader(shaderContext[nvrhi::ShaderType::Vertex].handle, nvrhi::ShaderType::Vertex)
            .AddShader(shaderContext[nvrhi::ShaderType::Pixel].handle, nvrhi::ShaderType::Pixel)
            .AddBindingLayout(bindingLayout)
            .Build();
        
        size_t vertAllocSize = s_Data->lineBatch.maxVertices * sizeof(Vertex2DLine);
        s_Data->lineBatch.vertexBufferBase = new Vertex2DLine[vertAllocSize];

        s_Data->lineBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);

        // create binding set
        nvrhi::BindingSetDesc bindingSetDesc;
        // add constant buffer
        bindingSetDesc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(CameraConstants)));

        s_Data->lineBatch.bindingSet = device->createBindingSet(bindingSetDesc, s_Data->lineBatch.pipeline->GetBindingLayout(0));
        LOG_ASSERT(s_Data->lineBatch.bindingSet, "[Renderer 2D] Failed to create binding");
    }

    void Renderer2D::Begin(nvrhi::ICommandList* commandList, ICamera* camera, nvrhi::IFramebuffer* framebuffer)
    {
        s_Data->cameraConstants = { camera->GetViewProjectionMatrix(), glm::vec4(camera->position, 1.0f) };

        // Quad data
        s_Data->quadBatch.indexCount = 0;
        s_Data->quadBatch.count = 0;
        s_Data->quadBatch.vertexBufferPtr = s_Data->quadBatch.vertexBufferBase;

        // Line data
        s_Data->lineBatch.indexCount = 0;
        s_Data->lineBatch.count = 0;
        s_Data->lineBatch.vertexBufferPtr = s_Data->lineBatch.vertexBufferBase;

        renderCommandList = commandList;
        renderFramebuffer = framebuffer;
    }

    void Renderer2D::Flush()
    {
        nvrhi::Viewport viewport = renderFramebuffer->getFramebufferInfo().getViewport();

        if (s_Data->quadBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t*>(s_Data->quadBatch.vertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data->quadBatch.vertexBufferBase);
            s_Data->quadBatch.vertexBuffer->SetData(renderCommandList, Buffer(s_Data->quadBatch.vertexBufferBase, bufferSize));

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(s_Data->quadBatch.pipeline->GetHandle())
                .setFramebuffer(renderFramebuffer)
                .addBindingSet(s_Data->quadBatch.bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ s_Data->quadBatch.vertexBuffer->GetHandle(), 0, 0})
                .setIndexBuffer({ s_Data->quadBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT});

            renderCommandList->setGraphicsState(graphicsState);

            renderCommandList->setPushConstants(&s_Data->cameraConstants, sizeof(CameraConstants));

            nvrhi::DrawArguments args;
            args.vertexCount = s_Data->quadBatch.indexCount;
            args.instanceCount = 1;

            renderCommandList->drawIndexed(args);
        }

        if (s_Data->lineBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t*>(s_Data->lineBatch.vertexBufferPtr) - reinterpret_cast<uint8_t*>(s_Data->lineBatch.vertexBufferBase);
            s_Data->lineBatch.vertexBuffer->SetData(renderCommandList, Buffer(s_Data->lineBatch.vertexBufferBase, bufferSize));

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(s_Data->lineBatch.pipeline->GetHandle())
                .setFramebuffer(renderFramebuffer)
                .addBindingSet(s_Data->lineBatch.bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ s_Data->lineBatch.vertexBuffer->GetHandle(), 0, 0});

            renderCommandList->setGraphicsState(graphicsState);

            renderCommandList->setPushConstants(&s_Data->cameraConstants, sizeof(CameraConstants));

            nvrhi::DrawArguments args;
            args.vertexCount = s_Data->lineBatch.indexCount;
            args.instanceCount = 1;

            renderCommandList->draw(args);
        }
    }

    void Renderer2D::End()
    {
        s_Data->quadBatch.indexCount = 0;
        s_Data->quadBatch.count = 0;

        s_Data->lineBatch.indexCount = 0;
        s_Data->lineBatch.count = 0;
    }
    
    void Renderer2D::DrawBox(const glm::mat4& transform, const glm::vec4& color, uint32_t entityID)
    {
        if (s_Data->lineBatch.count >= s_Data->lineBatch.maxCount)
            Renderer2D::End();

        static glm::vec4 cubeVertices[8] = {
            {-0.5f, -0.5f, -0.5f, 1.0f}, // 0
            { 0.5f, -0.5f, -0.5f, 1.0f}, // 1
            { 0.5f,  0.5f, -0.5f, 1.0f}, // 2
            {-0.5f,  0.5f, -0.5f, 1.0f}, // 3
            {-0.5f, -0.5f,  0.5f, 1.0f}, // 4
            { 0.5f, -0.5f,  0.5f, 1.0f}, // 5
            { 0.5f,  0.5f,  0.5f, 1.0f}, // 6
            {-0.5f,  0.5f,  0.5f, 1.0f}  // 7
        };

        static int edgeIndices[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, // bottom face
            {4, 5}, {5, 6}, {6, 7}, {7, 4}, // top face
            {0, 4}, {1, 5}, {2, 6}, {3, 7}  // vertical edges
        };

        for (int i = 0; i < 12; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                glm::vec4 position = transform * cubeVertices[edgeIndices[i][j]];
                s_Data->lineBatch.vertexBufferPtr->position = position;
                s_Data->lineBatch.vertexBufferPtr->color = color;
                s_Data->lineBatch.vertexBufferPtr->entityID = entityID;
                s_Data->lineBatch.vertexBufferPtr++;
                s_Data->lineBatch.indexCount++;
            }
        }

        s_Data->lineBatch.count++;
    }

    void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, uint32_t entityID)
    {
        if (s_Data->lineBatch.count >= s_Data->lineBatch.maxCount)
            Renderer2D::End();

        static constexpr int indices[8][2] = {
            {0, 2},
            {2, 1},
            {1, 3},
            {3, 0}
        };

        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                glm::vec4 position = transform * s_Data->quadPositions[indices[i][j]];
                s_Data->lineBatch.vertexBufferPtr->position = position;
                s_Data->lineBatch.vertexBufferPtr->color = color;
                s_Data->lineBatch.vertexBufferPtr->entityID = entityID;
                s_Data->lineBatch.vertexBufferPtr++;
                s_Data->lineBatch.indexCount++;
            }
        }

        s_Data->lineBatch.count++;
    }

    void Renderer2D::DrawLine(const std::vector<glm::vec3>& positions, const glm::vec4& color, uint32_t entityID)
    {
        if (s_Data->lineBatch.count >= s_Data->lineBatch.maxCount)
            Renderer2D::End();

        for (auto& pos : positions)
        {
            s_Data->lineBatch.vertexBufferPtr->position = pos;
            s_Data->lineBatch.vertexBufferPtr->color = color;
            s_Data->lineBatch.vertexBufferPtr->entityID = entityID;
            s_Data->lineBatch.vertexBufferPtr++;

            s_Data->lineBatch.indexCount++;
        }

        s_Data->lineBatch.count++;
    }

    void Renderer2D::DrawLine(const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec4& color, uint32_t entityID)
    {
        if (s_Data->lineBatch.count >= s_Data->lineBatch.maxCount)
            Renderer2D::End();

        s_Data->lineBatch.vertexBufferPtr->position = pos0;
        s_Data->lineBatch.vertexBufferPtr->color = color;
        s_Data->lineBatch.vertexBufferPtr->entityID = entityID;
        s_Data->lineBatch.vertexBufferPtr++;

        s_Data->lineBatch.vertexBufferPtr->position = pos1;
        s_Data->lineBatch.vertexBufferPtr->color = color;
        s_Data->lineBatch.vertexBufferPtr->entityID = entityID;
        s_Data->lineBatch.vertexBufferPtr++;

        s_Data->lineBatch.indexCount += 2;
        s_Data->lineBatch.count++;
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

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, f32 rotation, const glm::vec4& color, const Ref<Texture>& texture, const glm::vec2& tilingFactor, uint32_t entityID)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) 
            * glm::rotate(glm::mat4(1.0f), rotation, {0.0f, 0.0f, 1.0f }) 
            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        DrawQuad(transform, color, texture, tilingFactor, entityID);
    }

    void Renderer2D::DrawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, const Ref<Texture>& texture, const glm::vec2 &tilingFactor, uint32_t entityID)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        DrawQuad(transform, color, texture, tilingFactor, entityID);
    }
    
    void Renderer2D::DrawQuad(const glm::mat4 &transform, const glm::vec4 &color, const Ref<Texture>& texture, const glm::vec2 &tilingFactor, uint32_t entityID)
    {
        if (s_Data->quadBatch.count >= s_Data->quadBatch.maxCount)
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
            s_Data->quadBatch.vertexBufferPtr->position     = transform * s_Data->quadPositions[i];
            s_Data->quadBatch.vertexBufferPtr->texCoord     = textureCoords[i];
            s_Data->quadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            s_Data->quadBatch.vertexBufferPtr->color        = color;
            s_Data->quadBatch.vertexBufferPtr->texIndex     = texIndex;
            s_Data->quadBatch.vertexBufferPtr->entityID     = entityID;
            s_Data->quadBatch.vertexBufferPtr++;
        }

        s_Data->quadBatch.indexCount += 6;
        s_Data->quadBatch.count++;
    }

    uint32_t Renderer2D::GetOrInsertTexture(const Ref<Texture>& texture)
    {
        if (texture == nullptr)
            return 0;

        uint32_t textureIndex = 0;

        // find texture
        for (uint32_t i = 0; i < s_Data->quadBatch.textureSlotIndex; ++i)
        {
            if (*s_Data->quadBatch.textureSlots[i] == *texture)
            {
                textureIndex = i;
                break;
            }
        }

        // insert if not found
        if (textureIndex == 0)
        {
            if (s_Data->quadBatch.textureSlotIndex >= s_Data->MAX_TEXTURE_COUNT)
            {
                End();
                return s_Data->MAX_TEXTURE_COUNT;
            }
            
            textureIndex = s_Data->quadBatch.textureSlotIndex;
            s_Data->quadBatch.textureSlots[s_Data->quadBatch.textureSlotIndex] = texture;
            s_Data->quadBatch.textureSlotIndex++;

            UpdateTextureBindings();
        }

        return textureIndex;
    }

    void Renderer2D::UpdateTextureBindings()
    {
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        // then add textures
        const auto samplerDesc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Repeat)
            .setAllFilters(true);

        nvrhi::SamplerHandle sampler = device->createSampler(samplerDesc);

        nvrhi::BindingSetDesc bindingSetDesc;
        bindingSetDesc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(CameraConstants)));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));
        for (uint8_t i = 0; i < s_Data->MAX_TEXTURE_COUNT; ++i)
        {
            Ref<Texture> tex = s_Data->quadBatch.textureSlots[i];
            if (!tex)
                tex = Renderer::GetWhiteTexture();
            bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(i, tex->GetHandle()));
        }

        s_Data->quadBatch.bindingSet = device->createBindingSet(bindingSetDesc, s_Data->quadBatch.pipeline->GetBindingLayout(0));
    }
}
