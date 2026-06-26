// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "renderer_2d.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include "ignite/graphics/font.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/project/project.hpp"

#include <stb_image.h>
#include <algorithm>
#include <unordered_map>

namespace ignite
{
    static glm::vec4 QUAD_POSITIONS[4];
    const uint8_t MAX_TEXTURE_BATCH_COUNT = 32;

    static constexpr uint32_t s_BatchGrowThresholdPercent = 90;
    static constexpr uint32_t s_BatchShrinkThresholdPercent = 30;
    static constexpr uint32_t s_BatchShrinkFrameThreshold = 300;

    template<typename VertexType>
    static void ResizeBatch(BatchRender<VertexType> &batch, uint32_t newMaxCount, bool recreateIndexBuffer, nvrhi::ICommandList *uploadCmd)
    {
        IGN_PROFILE_FUNCTION();

        if (newMaxCount == 0 || newMaxCount == batch.maxCount)
            return;

        const uint32_t usedVertices = batch.vertexBufferPtr ? static_cast<uint32_t>(batch.vertexBufferPtr - batch.vertexBufferBase) : 0;

        auto newBase = new VertexType[newMaxCount * batch.verticesPerObject];
        if (batch.vertexBufferBase && usedVertices > 0)
        {
            std::copy_n(batch.vertexBufferBase, usedVertices, newBase);
        }

        delete[] batch.vertexBufferBase;
        batch.vertexBufferBase = newBase;
        batch.vertexBufferPtr = batch.vertexBufferBase + usedVertices;

        batch.maxCount = newMaxCount;
        batch.maxVertices = batch.maxCount * batch.verticesPerObject;
        batch.maxIndices = batch.maxCount * batch.indicesPerObject;

        const size_t verticesAllocSize = static_cast<size_t>(batch.maxVertices) * sizeof(VertexType);
        const size_t indicesAllocSize = static_cast<size_t>(batch.maxIndices) * sizeof(uint32_t);

        batch.vertexBuffer = VertexBuffer::Create(verticesAllocSize);

        if (recreateIndexBuffer && batch.indicesPerObject > 0)
        {
            batch.indexBuffer = IndexBuffer::Create(indicesAllocSize);

            std::vector<uint32_t> indices(batch.maxIndices);
            uint32_t offset = 0;
            for (uint32_t i = 0; i < batch.maxIndices; i += 6)
            {
                indices[0 + i] = offset + 0;
                indices[1 + i] = offset + 1;
                indices[2 + i] = offset + 2;
                indices[3 + i] = offset + 0;
                indices[4 + i] = offset + 3;
                indices[5 + i] = offset + 1;
                offset += 4;
            }

            if (uploadCmd)
            {
                batch.indexBuffer->SetData(uploadCmd, Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
            }
            else
            {
                nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
                nvrhi::CommandListHandle cmd = device->createCommandList();
                cmd->open();
                batch.indexBuffer->SetData(cmd, Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
                cmd->close();
                device->executeCommandList(cmd);
            }
        }

        if (batch.count > batch.maxCount)
            batch.count = batch.maxCount;

        if (batch.indicesPerObject > 0 && batch.indexCount > batch.maxIndices)
            batch.indexCount = batch.maxIndices;

        LOG_TRACE("[Renderer 2D] Resizing buffer Vertex: {} bytes, Index: {} bytes", verticesAllocSize, indicesAllocSize);
    }

    template<typename VertexType>
    static void EnsureBatchCapacity(BatchRender<VertexType> &batch, uint32_t additionalVertices, uint32_t additionalIndices, bool recreateIndexBuffer, nvrhi::ICommandList *uploadCmd)
    {
        IGN_PROFILE_FUNCTION();

        const uint32_t usedVertices = batch.vertexBufferPtr
            ? static_cast<uint32_t>(batch.vertexBufferPtr - batch.vertexBufferBase)
            : 0;

        const uint32_t requiredVertices = usedVertices + additionalVertices;
        const uint32_t requiredIndices = batch.indexCount + additionalIndices;

        const uint32_t vertexGrowThreshold = (batch.maxVertices * s_BatchGrowThresholdPercent) / 100;
        const uint32_t indexGrowThreshold = batch.maxIndices > 0
            ? (batch.maxIndices * s_BatchGrowThresholdPercent) / 100
            : 0;

        const bool needGrowByVertex = requiredVertices >= vertexGrowThreshold;
        const bool needGrowByIndex = batch.indicesPerObject > 0 && requiredIndices >= indexGrowThreshold;

        if (!needGrowByVertex && !needGrowByIndex)
            return;

        uint32_t newMaxCount = batch.maxCount;
        while (true)
        {
            const uint32_t newMaxVertices = newMaxCount * batch.verticesPerObject;
            const uint32_t newMaxIndices = newMaxCount * batch.indicesPerObject;

            const bool fitVertices = requiredVertices < (newMaxVertices * s_BatchGrowThresholdPercent) / 100;
            const bool fitIndices = batch.indicesPerObject == 0 || requiredIndices < (newMaxIndices * s_BatchGrowThresholdPercent) / 100;

            if (fitVertices && fitIndices)
                break;

            newMaxCount *= 2;
        }

        ResizeBatch(batch, newMaxCount, recreateIndexBuffer, uploadCmd);
        batch.lowUsageFrames = 0;
    }

    template<typename VertexType>
    static void TryShrinkBatch(BatchRender<VertexType> &batch, uint32_t usedVertices, uint32_t usedIndices, bool recreateIndexBuffer, nvrhi::ICommandList *uploadCmd)
    {
        IGN_PROFILE_FUNCTION();

        if (batch.maxCount <= batch.minCount)
            return;

        const bool lowVertexUsage = usedVertices < (batch.maxVertices * s_BatchShrinkThresholdPercent) / 100;
        const bool lowIndexUsage = batch.indicesPerObject == 0 || usedIndices < (batch.maxIndices * s_BatchShrinkThresholdPercent) / 100;

        if (lowVertexUsage && lowIndexUsage)
        {
            batch.lowUsageFrames++;
        }
        else
        {
            batch.lowUsageFrames = 0;
            return;
        }

        if (batch.lowUsageFrames < s_BatchShrinkFrameThreshold)
            return;

        const uint32_t targetByVertices = std::max(batch.minCount,
            static_cast<uint32_t>(std::max<uint32_t>(1, usedVertices) * 2 / std::max<uint32_t>(1, batch.verticesPerObject)));
        
        const uint32_t targetByIndices = batch.indicesPerObject > 0
            ? std::max(batch.minCount, static_cast<uint32_t>(std::max<uint32_t>(1, usedIndices) * 2 / std::max<uint32_t>(1, batch.indicesPerObject)))
            : batch.minCount;

        const uint32_t target = std::max(targetByVertices, targetByIndices);
        const uint32_t newMaxCount = std::max(batch.minCount, std::max(target, batch.maxCount / 2));

        if (newMaxCount < batch.maxCount)
        {
            ResizeBatch(batch, newMaxCount, recreateIndexBuffer, uploadCmd);
        }

        batch.lowUsageFrames = 0;
    }

    Ref<Renderer2D> Renderer2D::Create()
    {
        return CreateRef<Renderer2D>();
    }

    Renderer2D::Renderer2D()
    {
        InitQuadData();
        InitLineData();
        InitCircleData();
        InitTextData();

        m_Material2DLightingBuffer = ConstantBuffer::Create(sizeof(Material2DLighting_GPUData), false, 1, "Material2D Lighting Buffer");

        m_Material2DLightingData = CreateRef<Material2DLighting_GPUData>();
        m_Material2DLightingData->pointLightCount = 0;

        m_PreRenderCache = CreateRef<PreRenderCacheData>();
    }

    Renderer2D::~Renderer2D()
    {
        ClearPipelineCache();

        m_TextureResolveCache.clear();
        m_Material2DResolveCache.clear();
    }

    Ref<Texture> Renderer2D::ResolveTexture(Project *project, AssetHandle handle)
    {
        IGN_PROFILE_FUNCTION();

        if (!project || handle == AssetHandle(0))
            return nullptr;

        const AssetResolveKey key{ project, handle };
        auto it = m_TextureResolveCache.find(key);
        if (it != m_TextureResolveCache.end())
            return it->second;

        Ref<Texture> texture = project->GetAsset<Texture>(handle);
        if (texture)
        {
            m_TextureResolveCache.emplace(key, texture);
        }

        return texture;
    }

    Ref<Material2D> Renderer2D::ResolveMaterial2D(Project *project, AssetHandle handle)
    {
        IGN_PROFILE_FUNCTION();

        if (!project || handle == AssetHandle(0))
            return nullptr;

        const AssetResolveKey key{ project, handle };
        auto it = m_Material2DResolveCache.find(key);
        if (it != m_Material2DResolveCache.end())
            return it->second;

        Ref<Material2D> material = project->GetAsset<Material2D>(handle);
        if (material)
        {
            m_Material2DResolveCache.emplace(key, material);
        }

        return material;
    }

    void Renderer2D::ClearAssetResolveCache()
    {
        m_TextureResolveCache.clear();
        m_Material2DResolveCache.clear();
    }

    void Renderer2D::BuildPreRenderCache()
    {
        IGN_PROFILE_FUNCTION();

		m_PreRenderCache->valid = true;
        m_PreRenderCache->lightingData = m_Material2DLightingData;

        const uint32_t circleVertexCount = m_CircleBatch.vertexBufferPtr
            ? static_cast<uint32_t>(m_CircleBatch.vertexBufferPtr - m_CircleBatch.vertexBufferBase)
            : 0;
        m_PreRenderCache->circleVertices.assign(m_CircleBatch.vertexBufferBase, m_CircleBatch.vertexBufferBase + circleVertexCount);
        m_PreRenderCache->circleIndexCount = m_CircleBatch.indexCount;

        const uint32_t quadVertexCount = m_QuadBatch.vertexBufferPtr
            ? static_cast<uint32_t>(m_QuadBatch.vertexBufferPtr - m_QuadBatch.vertexBufferBase)
            : 0;
        m_PreRenderCache->quadVertices.assign(m_QuadBatch.vertexBufferBase, m_QuadBatch.vertexBufferBase + quadVertexCount);
        m_PreRenderCache->quadIndexCount = m_QuadBatch.indexCount;
        m_PreRenderCache->quadTextureSlots = m_QuadBatch.textureSlots;

        const uint32_t textVertexCount = m_TextBatch.vertexBufferPtr
            ? static_cast<uint32_t>(m_TextBatch.vertexBufferPtr - m_TextBatch.vertexBufferBase)
            : 0;
        m_PreRenderCache->textVertices.assign(m_TextBatch.vertexBufferBase, m_TextBatch.vertexBufferBase + textVertexCount);
        m_PreRenderCache->textIndexCount = m_TextBatch.indexCount;
        m_PreRenderCache->textTextureSlots = m_TextBatch.textureSlots;
    }

    bool Renderer2D::ReplayPreRenderCache(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const Ref<ConstantBuffer> &cameraBuffer)
    {
        IGN_PROFILE_FUNCTION();

        if (!m_PreRenderCache->valid || !cmd || !framebuffer)
        {
            return false;
        }

        m_Cmd = cmd;

        if (m_Material2DLightingBuffer)
        {
            m_Material2DLightingBuffer->SetData(m_Cmd, Buffer(&m_PreRenderCache->lightingData, sizeof(m_PreRenderCache->lightingData)));
        }

        const nvrhi::Viewport &viewport = framebuffer->getFramebufferInfo().getViewport();

        if (m_PreRenderCache->circleIndexCount > 0 && !m_PreRenderCache->circleVertices.empty())
        {
            m_CircleBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_PreRenderCache->circleVertices.data(), m_PreRenderCache->circleVertices.size() * sizeof(Vertex2DCircle)));

            Ref<GraphicsPipeline> gp = GetCirclePipelineForFB(framebuffer, m_FillMode);
            nvrhi::BindingSetHandle bindingSet = GetCircleBindingSet(gp->GetBindingLayout(0), cameraBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_CircleBatch.vertexBuffer->GetHandle(), 0, 0 })
                .setIndexBuffer({ m_CircleBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_PreRenderCache->circleIndexCount;
            args.instanceCount = 1;
            m_Cmd->drawIndexed(args);
        }

        if (m_PreRenderCache->quadIndexCount > 0 && !m_PreRenderCache->quadVertices.empty())
        {
            m_QuadBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_PreRenderCache->quadVertices.data(), m_PreRenderCache->quadVertices.size() * sizeof(Vertex2DQuad)));

            Ref<GraphicsPipeline> gp = GetQuadPipelineForFB(framebuffer, m_FillMode);
            nvrhi::BindingSetHandle bindingSet = GetQuadBindingSet(gp->GetBindingLayout(0), m_PreRenderCache->quadTextureSlots, cameraBuffer, m_Material2DLightingBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_QuadBatch.vertexBuffer->GetHandle(), 0, 0 })
                .setIndexBuffer({ m_QuadBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_PreRenderCache->quadIndexCount;
            args.instanceCount = 1;
            m_Cmd->drawIndexed(args);
        }

        if (m_PreRenderCache->textIndexCount > 0 && !m_PreRenderCache->textVertices.empty())
        {
            m_TextBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_PreRenderCache->textVertices.data(), m_PreRenderCache->textVertices.size() * sizeof(VertexText)));

            Ref<GraphicsPipeline> gp = GetTextPipelineForFB(framebuffer, m_FillMode);
            nvrhi::BindingSetHandle bindingSet = GetTextBindingSet(gp->GetBindingLayout(0), m_PreRenderCache->textTextureSlots, cameraBuffer, m_Material2DLightingBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_TextBatch.vertexBuffer->GetHandle(), 0, 0 })
                .setIndexBuffer({ m_TextBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_PreRenderCache->textIndexCount;
            args.instanceCount = 1;
            m_Cmd->drawIndexed(args);
        }

        return true;
    }

    void Renderer2D::InvalidatePreRenderCache()
    {
        m_PreRenderCache->valid = false;
        m_PreRenderCache->circleVertices.clear();
        m_PreRenderCache->quadVertices.clear();
        m_PreRenderCache->textVertices.clear();
        m_PreRenderCache->quadTextureSlots.clear();
        m_PreRenderCache->textTextureSlots.clear();
        m_PreRenderCache->circleIndexCount = 0;
        m_PreRenderCache->quadIndexCount = 0;
        m_PreRenderCache->textIndexCount = 0;
    }

    void Renderer2D::InitQuadData()
    {
        m_QuadBatch.minCount = 256;
        m_QuadBatch.maxCount = m_QuadBatch.minCount;
        m_QuadBatch.verticesPerObject = 4;
        m_QuadBatch.indicesPerObject = 6;
        m_QuadBatch.maxVertices = m_QuadBatch.maxCount * m_QuadBatch.verticesPerObject;
        m_QuadBatch.maxIndices = m_QuadBatch.maxCount * m_QuadBatch.indicesPerObject;

        size_t vertAllocSize = m_QuadBatch.maxVertices * sizeof(Vertex2DQuad);
        m_QuadBatch.vertexBufferBase = new Vertex2DQuad[m_QuadBatch.maxVertices];

        size_t indicesAllocSize = m_QuadBatch.maxIndices * sizeof(uint32_t);
        m_QuadBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);
        m_QuadBatch.indexBuffer = IndexBuffer::Create(indicesAllocSize);

        Renderer::Stats.quadVerticesSize += vertAllocSize;
        Renderer::Stats.quadIndicesSize += indicesAllocSize;

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

        auto device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();
        m_QuadBatch.indexBuffer->SetData(cmd, Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
        cmd->close();
        device->executeCommandList(cmd);

        QUAD_POSITIONS[0] = { -0.5f, -0.5f, 0.0f, 1.0f }; // bottom-left
        QUAD_POSITIONS[1] = { 0.5f,  0.5f, 0.0f, 1.0f }; // top-right
        QUAD_POSITIONS[2] = { -0.5f,  0.5f, 0.0f, 1.0f }; // top-left
        QUAD_POSITIONS[3] = { 0.5f, -0.5f, 0.0f, 1.0f }; // bottom-right
    }

    void Renderer2D::InitLineData()
    {
        m_LineBatch.minCount = 256;
        m_LineBatch.maxCount = m_LineBatch.minCount;
        m_LineBatch.verticesPerObject = 24;
        m_LineBatch.indicesPerObject = 0;
        m_LineBatch.maxVertices = m_LineBatch.maxCount * m_LineBatch.verticesPerObject;
        m_LineBatch.maxIndices = 0;

        size_t vertAllocSize = m_LineBatch.maxVertices * sizeof(Vertex2DLine);
        m_LineBatch.vertexBufferBase = new Vertex2DLine[m_LineBatch.maxVertices];
        m_LineBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);

        Renderer::Stats.lineVerticesSize += vertAllocSize;
    }

    void Renderer2D::InitCircleData()
    {
        m_CircleBatch.minCount = 256;
        m_CircleBatch.maxCount = m_CircleBatch.minCount;
        m_CircleBatch.verticesPerObject = 4;
        m_CircleBatch.indicesPerObject = 6;
        m_CircleBatch.maxVertices = m_CircleBatch.maxCount * m_CircleBatch.verticesPerObject;
        m_CircleBatch.maxIndices = m_CircleBatch.maxCount * m_CircleBatch.indicesPerObject;

        size_t vertAllocSize = m_CircleBatch.maxVertices * sizeof(Vertex2DCircle);
        m_CircleBatch.vertexBufferBase = new Vertex2DCircle[m_CircleBatch.maxVertices];
        m_CircleBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);

        size_t indicesAllocSize = m_CircleBatch.maxIndices * sizeof(uint32_t);
        m_CircleBatch.indexBuffer = IndexBuffer::Create(indicesAllocSize);

        Renderer::Stats.circleVerticesSize += vertAllocSize;
        Renderer::Stats.circleIndicesSize += indicesAllocSize;

        std::vector<uint32_t> indices(m_CircleBatch.maxIndices);

        uint32_t offset = 0;
        for (uint32_t i = 0; i < m_CircleBatch.maxIndices; i += 6)
        {
            indices[0 + i] = offset + 0;
            indices[1 + i] = offset + 1;
            indices[2 + i] = offset + 2;

            indices[3 + i] = offset + 0;
            indices[4 + i] = offset + 3;
            indices[5 + i] = offset + 1;

            offset += 4;
        }

        auto device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();
        m_CircleBatch.indexBuffer->SetData(cmd, Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
        cmd->close();
        device->executeCommandList(cmd);
    }

    void Renderer2D::InitTextData()
    {
        m_TextBatch.minCount = 256;
        m_TextBatch.maxCount = m_TextBatch.minCount;
        m_TextBatch.verticesPerObject = 4;
        m_TextBatch.indicesPerObject = 6;
        m_TextBatch.maxVertices = m_TextBatch.maxCount * m_TextBatch.verticesPerObject;
        m_TextBatch.maxIndices = m_TextBatch.maxCount * m_TextBatch.indicesPerObject;

        size_t vertAllocSize = m_TextBatch.maxVertices * sizeof(VertexText);
        m_TextBatch.vertexBufferBase = new VertexText[m_TextBatch.maxVertices];
        m_TextBatch.vertexBuffer = VertexBuffer::Create(vertAllocSize);

        size_t indicesAllocSize = m_TextBatch.maxIndices * sizeof(uint32_t);
        m_TextBatch.indexBuffer = IndexBuffer::Create(indicesAllocSize);
        m_TextBatch.textureSlots.resize(MAX_TEXTURE_BATCH_COUNT);
        m_TextBatch.textureSlots[0] = Renderer::GetWhiteTexture();

        Renderer::Stats.textVerticesSize += vertAllocSize;
        Renderer::Stats.textIndicesSize += indicesAllocSize;

        std::vector<uint32_t> indices(m_TextBatch.maxIndices);

        uint32_t offset = 0;
        for (uint32_t i = 0; i < m_TextBatch.maxIndices; i += 6)
        {
            indices[0 + i] = offset + 0;
            indices[1 + i] = offset + 1;
            indices[2 + i] = offset + 2;

            indices[3 + i] = offset + 0;
            indices[4 + i] = offset + 3;
            indices[5 + i] = offset + 1;

            offset += 4;
        }

        auto device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();
        cmd->open();
        m_TextBatch.indexBuffer->SetData(cmd, Buffer(indices.data(), indices.size() * sizeof(uint32_t)));
        cmd->close();
        device->executeCommandList(cmd);
    }

    void Renderer2D::ClearPipelineCache()
    {
        IGN_PROFILE_FUNCTION();

		m_LinePSOCache.clear();
		m_QuadPSOCache.clear();
		m_CirclePSOCache.clear();
		m_TextPSOCache.clear();

		m_QuadBindingSetCache.clear();
		m_LineBindingSetCache.clear();
		m_CircleBindingSetCache.clear();
		m_TextBindingSetCache.clear();
    }

    void Renderer2D::Begin(nvrhi::ICommandList *cmd)
    {
        IGN_PROFILE_FUNCTION();

        // Quad data
        m_QuadBatch.indexCount = 0;
        m_QuadBatch.count = 0;
        m_QuadBatch.textureSlotIndex = 1;
        m_QuadBatch.vertexBufferPtr = m_QuadBatch.vertexBufferBase;

        // Line data
        m_LineBatch.indexCount = 0;
        m_LineBatch.count = 0;
        m_LineBatch.vertexBufferPtr = m_LineBatch.vertexBufferBase;

        // Circle data
        m_CircleBatch.indexCount = 0;
        m_CircleBatch.count = 0;
        m_CircleBatch.vertexBufferPtr = m_CircleBatch.vertexBufferBase;

        // Text data
        m_TextBatch.indexCount = 0;
        m_TextBatch.count = 0;
        m_TextBatch.textureSlotIndex = 1;
        m_TextBatch.vertexBufferPtr = m_TextBatch.vertexBufferBase;

        if (m_Material2DLightingBuffer && m_Material2DLightingDirty)
        {
            m_Material2DLightingBuffer->SetData(cmd, Buffer(&m_Material2DLightingData, sizeof(m_Material2DLightingData)));
            m_Material2DLightingDirty = false;
        }

        m_Cmd = cmd;
    }

    void Renderer2D::Flush(nvrhi::IFramebuffer *framebuffer, const Ref<ConstantBuffer> &cameraBuffer)
    {
        IGN_PROFILE_FUNCTION();

        const nvrhi::Viewport &viewport = framebuffer->getFramebufferInfo().getViewport();

        if (m_LineBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t *>(m_LineBatch.vertexBufferPtr) - reinterpret_cast<uint8_t *>(m_LineBatch.vertexBufferBase);
            m_LineBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_LineBatch.vertexBufferBase, bufferSize));

            m_Cmd->setBufferState(m_LineBatch.vertexBuffer->GetHandle(), nvrhi::ResourceStates::VertexBuffer);

            Renderer::Stats.lineVerticesSize += bufferSize;

            Ref<GraphicsPipeline> gp = GetLinePipelineForFB(framebuffer);
            nvrhi::BindingSetHandle bindingSet = GetLineBindingSet(gp->GetBindingLayout(0), cameraBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_LineBatch.vertexBuffer->GetHandle(), 0, 0 });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_LineBatch.indexCount;
            args.instanceCount = 1;

            m_Cmd->draw(args);
        }

        if (m_CircleBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t *>(m_CircleBatch.vertexBufferPtr) - reinterpret_cast<uint8_t *>(m_CircleBatch.vertexBufferBase);
            m_CircleBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_CircleBatch.vertexBufferBase, bufferSize));

            Renderer::Stats.circleVerticesSize += bufferSize;

            Ref<GraphicsPipeline> gp = GetCirclePipelineForFB(framebuffer, m_FillMode);
            nvrhi::BindingSetHandle bindingSet = GetCircleBindingSet(gp->GetBindingLayout(0), cameraBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_CircleBatch.vertexBuffer->GetHandle(), 0, 0 })
                .setIndexBuffer({ m_CircleBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_CircleBatch.indexCount;
            args.instanceCount = 1;

            m_Cmd->drawIndexed(args);
        }

        if (m_QuadBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t *>(m_QuadBatch.vertexBufferPtr) - reinterpret_cast<uint8_t *>(m_QuadBatch.vertexBufferBase);
            m_QuadBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_QuadBatch.vertexBufferBase, bufferSize));

            Renderer::Stats.quadVerticesSize += bufferSize;

            Ref<GraphicsPipeline> gp = GetQuadPipelineForFB(framebuffer, m_FillMode);
            nvrhi::BindingSetHandle bindingSet = GetQuadBindingSet(gp->GetBindingLayout(0), m_QuadBatch.textureSlots, cameraBuffer, m_Material2DLightingBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_QuadBatch.vertexBuffer->GetHandle(), 0, 0 })
                .setIndexBuffer({ m_QuadBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_QuadBatch.indexCount;
            args.instanceCount = 1;

            m_Cmd->drawIndexed(args);
        }

        if (m_TextBatch.indexCount > 0)
        {
            const size_t bufferSize = reinterpret_cast<uint8_t *>(m_TextBatch.vertexBufferPtr) - reinterpret_cast<uint8_t *>(m_TextBatch.vertexBufferBase);
            m_TextBatch.vertexBuffer->SetData(m_Cmd, Buffer(m_TextBatch.vertexBufferBase, bufferSize));

            Renderer::Stats.textVerticesSize += bufferSize;

            Ref<GraphicsPipeline> gp = GetTextPipelineForFB(framebuffer, m_FillMode);
            nvrhi::BindingSetHandle bindingSet = GetTextBindingSet(gp->GetBindingLayout(0), m_TextBatch.textureSlots, cameraBuffer, m_Material2DLightingBuffer);

            const auto graphicsState = nvrhi::GraphicsState()
                .setPipeline(gp->GetHandle())
                .setFramebuffer(framebuffer)
                .addBindingSet(bindingSet)
                .setViewport(nvrhi::ViewportState().addViewportAndScissorRect(viewport))
                .addVertexBuffer(nvrhi::VertexBufferBinding{ m_TextBatch.vertexBuffer->GetHandle(), 0, 0 })
                .setIndexBuffer({ m_TextBatch.indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            m_Cmd->setGraphicsState(graphicsState);

            nvrhi::DrawArguments args;
            args.vertexCount = m_TextBatch.indexCount;
            args.instanceCount = 1;

            m_Cmd->drawIndexed(args);
        }
    }

    void Renderer2D::End()
    {
        IGN_PROFILE_FUNCTION();

        const uint32_t quadUsedVertices = m_QuadBatch.vertexBufferPtr
            ? static_cast<uint32_t>(m_QuadBatch.vertexBufferPtr - m_QuadBatch.vertexBufferBase)
            : 0;
        const uint32_t lineUsedVertices = m_LineBatch.vertexBufferPtr
            ? static_cast<uint32_t>(m_LineBatch.vertexBufferPtr - m_LineBatch.vertexBufferBase)
            : 0;
        const uint32_t circleUsedVertices = m_CircleBatch.vertexBufferPtr
            ? static_cast<uint32_t>(m_CircleBatch.vertexBufferPtr - m_CircleBatch.vertexBufferBase)
            : 0;
        const uint32_t textUsedVertices = m_TextBatch.vertexBufferPtr
            ? static_cast<uint32_t>(m_TextBatch.vertexBufferPtr - m_TextBatch.vertexBufferBase)
            : 0;

        TryShrinkBatch(m_QuadBatch, quadUsedVertices, m_QuadBatch.indexCount, true, m_Cmd);
        TryShrinkBatch(m_LineBatch, lineUsedVertices, 0, false, m_Cmd);
        TryShrinkBatch(m_CircleBatch, circleUsedVertices, m_CircleBatch.indexCount, true, m_Cmd);
        TryShrinkBatch(m_TextBatch, textUsedVertices, m_TextBatch.indexCount, true, m_Cmd);

        m_QuadBatch.indexCount = 0;
        m_QuadBatch.count = 0;
        m_QuadBatch.textureSlotIndex = 1;

        m_LineBatch.indexCount = 0;
        m_LineBatch.count = 0;

        m_CircleBatch.indexCount = 0;
        m_CircleBatch.count = 0;

        m_TextBatch.indexCount = 0;
        m_TextBatch.count = 0;
        m_TextBatch.textureSlotIndex = 1;
    }

    void Renderer2D::DrawBox(const glm::mat4 &transform, const glm::vec4 &color)
    {
        IGN_PROFILE_FUNCTION();

        EnsureBatchCapacity(m_LineBatch, 24, 0, false, m_Cmd);

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
        Renderer::Stats.lineCount++;
    }

    void Renderer2D::DrawRect(const glm::mat4 &transform, const glm::vec4 &color)
    {
        IGN_PROFILE_FUNCTION();

        EnsureBatchCapacity(m_LineBatch, 8, 0, false, m_Cmd);

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
        Renderer::Stats.lineCount++;
    }

    void Renderer2D::DrawLine(const std::vector<glm::vec3> &positions, const glm::vec4 &color)
    {
        IGN_PROFILE_FUNCTION();

        EnsureBatchCapacity(m_LineBatch, static_cast<uint32_t>(positions.size()), 0, false, m_Cmd);

        for (auto &pos : positions)
        {
            m_LineBatch.vertexBufferPtr->position = pos;
            m_LineBatch.vertexBufferPtr->color = color;
            m_LineBatch.vertexBufferPtr++;

            m_LineBatch.indexCount++;
        }

        m_LineBatch.count++;
        Renderer::Stats.lineCount++;
    }

    void Renderer2D::DrawLine(const glm::vec3 &pos0, const glm::vec3 &pos1, const glm::vec4 &color)
    {
        IGN_PROFILE_FUNCTION();

        EnsureBatchCapacity(m_LineBatch, 2, 0, false, m_Cmd);

        m_LineBatch.vertexBufferPtr->position = pos0;
        m_LineBatch.vertexBufferPtr->color = color;
        m_LineBatch.vertexBufferPtr++;

        m_LineBatch.vertexBufferPtr->position = pos1;
        m_LineBatch.vertexBufferPtr->color = color;
        m_LineBatch.vertexBufferPtr++;

        m_LineBatch.indexCount += 2;
        m_LineBatch.count++;
        Renderer::Stats.lineCount++;
    }

    void Renderer2D::DrawAABB(const AABB &aabb, const glm::vec4 &color)
    {
        IGN_PROFILE_FUNCTION();

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

    void Renderer2D::DrawCircle(const glm::vec3 &position, const glm::vec3 &scale, const glm::vec4 &color, float thickness, float fade, uint32_t objectID)
    {
        DrawCircle(glm::translate(position) * glm::scale(scale), color, thickness, fade, objectID);
    }

    void Renderer2D::DrawCircle(const glm::mat4 &transform, const glm::vec4 &color, float thickness, float fade, uint32_t objectID)
    {
        IGN_PROFILE_FUNCTION();

      EnsureBatchCapacity(m_CircleBatch, 4, 6, true, m_Cmd);

        for (uint32_t i = 0; i < 4; ++i)
        {
            m_CircleBatch.vertexBufferPtr->position = transform * QUAD_POSITIONS[i];
            m_CircleBatch.vertexBufferPtr->localPosition = QUAD_POSITIONS[i];
            m_CircleBatch.vertexBufferPtr->color = color;
            m_CircleBatch.vertexBufferPtr->objectID = objectID;
            m_CircleBatch.vertexBufferPtr++;
        }

        m_CircleBatch.indexCount += 6;
        m_CircleBatch.count++;
        Renderer::Stats.quadCount++;
    }

    void Renderer2D::DrawQuad(const Rect &rect, float rotation, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor, uint32_t objectID)
    {
        IGN_PROFILE_FUNCTION();

        EnsureBatchCapacity(m_QuadBatch, 4, 6, true, m_Cmd);

        static constexpr uint32_t quadVertexCount = 4;
        const glm::vec2 textureCoords[] =
        {
            { uv0.x, uv0.y },
            { uv1.x, uv1.y },
            { uv0.x, uv1.y },
            { uv1.x, uv0.y }
        };

        const glm::vec4 positions[4] =
        {
            { rect.min.x, rect.min.y, 0.0f, 1.0f }, // bottom-left
            { rect.max.x, rect.max.y, 0.0f, 1.0f }, // top-right
            { rect.min.x, rect.max.y, 0.0f, 1.0f }, // top-left
            { rect.max.x, rect.min.y, 0.0f, 1.0f }, // bottom-right
        };

        uint32_t texIndex = GetOrInsertQuadTexture(texture);

        for (uint32_t i = 0; i < quadVertexCount; ++i)
        {
            m_QuadBatch.vertexBufferPtr->position = positions[i];
            m_QuadBatch.vertexBufferPtr->texCoord = textureCoords[i];
            m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            m_QuadBatch.vertexBufferPtr->color = color;
            m_QuadBatch.vertexBufferPtr->additiveColor = glm::vec4(0.0f);
            m_QuadBatch.vertexBufferPtr->texIndex = texIndex;
            m_QuadBatch.vertexBufferPtr->materialType = MATERIAL_2D_TYPE_UNLIT;
            m_QuadBatch.vertexBufferPtr->objectID = objectID;
            m_QuadBatch.vertexBufferPtr++;
        }

        m_QuadBatch.indexCount += 6;
        m_QuadBatch.count++;
        Renderer::Stats.quadCount++;
    }

    void Renderer2D::DrawQuad(const glm::vec3 &position, const glm::vec2 &size, f32 rotation, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor, uint32_t objectID)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
            * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
            * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        DrawQuad(transform, color, texture, uv0, uv1, tilingFactor, objectID);
    }

    void Renderer2D::DrawQuad(const glm::vec3 &position, const glm::vec2 &size, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor, uint32_t objectID)
    {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        DrawQuad(transform, color, texture, uv0, uv1, tilingFactor, objectID);
    }

    void Renderer2D::DrawQuad(const glm::mat4 &transform, const glm::vec4 &color, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor, uint32_t objectID)
    {
       DrawQuad(transform, color, glm::vec4(0.0f), MATERIAL_2D_TYPE_UNLIT, texture, uv0, uv1, tilingFactor, objectID);
    }

    void Renderer2D::DrawQuad(const glm::mat4 &transform, const glm::vec4 &color, const glm::vec4 &additiveColor, Material2DType materialType, const Ref<Texture> &texture, const glm::vec2 &uv0, const glm::vec2 &uv1, const glm::vec2 &tilingFactor, uint32_t objectID)
    {
        IGN_PROFILE_FUNCTION();

        EnsureBatchCapacity(m_QuadBatch, 4, 6, true, m_Cmd);

        static constexpr uint32_t quadVertexCount = 4;
        const glm::vec2 textureCoords[] =
        {
            { uv0.x, uv0.y },
            { uv1.x, uv1.y },
            { uv0.x, uv1.y },
            { uv1.x, uv0.y }
        };

        uint32_t texIndex = GetOrInsertQuadTexture(texture);

        for (uint32_t i = 0; i < quadVertexCount; ++i)
        {
            m_QuadBatch.vertexBufferPtr->position = transform * QUAD_POSITIONS[i];
            m_QuadBatch.vertexBufferPtr->texCoord = textureCoords[i];
            m_QuadBatch.vertexBufferPtr->tilingFactor = tilingFactor;
            m_QuadBatch.vertexBufferPtr->color = color;
            m_QuadBatch.vertexBufferPtr->additiveColor = additiveColor;
            m_QuadBatch.vertexBufferPtr->texIndex = texIndex;
            m_QuadBatch.vertexBufferPtr->materialType = static_cast<uint32_t>(materialType);
            m_QuadBatch.vertexBufferPtr->objectID = objectID;
            m_QuadBatch.vertexBufferPtr++;
        }

        m_QuadBatch.indexCount += 6;
        m_QuadBatch.count++;
        Renderer::Stats.quadCount++;
    }

    void Renderer2D::SetPointLights2D(const std::vector<PointLight2D_GPUData> &pointLights)
    {
        m_Material2DLightingData->pointLightCount = std::min<uint32_t>(static_cast<uint32_t>(pointLights.size()), MAX_POINT_LIGHTS_2D);
        for (uint32_t i = 0; i < m_Material2DLightingData->pointLightCount; ++i)
        {
            m_Material2DLightingData->pointLights[i] = pointLights[i];
        }

        for (uint32_t i = m_Material2DLightingData->pointLightCount; i < MAX_POINT_LIGHTS_2D; ++i)
        {
            m_Material2DLightingData->pointLights[i] = PointLight2D_GPUData{};
        }

        m_Material2DLightingDirty = true;
        Renderer::Stats.pointLight2dCount++;

    }

    void Renderer2D::DrawString(const std::string &str, const Ref<Font> &font, const glm::vec4 &color, const glm::mat4 &transform, float kerning, float linespacing, uint32_t objectID)
    {
        IGN_PROFILE_FUNCTION();

        if (!font)
            return;

        Ref<Texture> fontAtlasTexture = font->GetAtlasTexture();
        if (!font || !font->IsReady())
            return;

        const auto &fontGeometry = font->GetGeometry();
        const auto &metrics = fontGeometry.getMetrics();

        uint32_t texIndex = GetOrInsertFontTexture(fontAtlasTexture);

        double x = 0.0;
        double y = 0.0;
        double maxX = 0.0;
        double minY = 0.0;

        double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
        const double spaceGlypAdvance = fontGeometry.getGlyph(' ')->getAdvance();

        for (size_t i = 0; i < str.size(); ++i)
        {
            char character = str[i];
            if (character == '\r')
            {
                continue;
            }

            if (character == '\n')
            {
                maxX= std::max(maxX, x);

                x = 0.0;
                y -= fsScale * metrics.lineHeight + linespacing;
                continue;
            }

            if (character == ' ')
            {
                float advance = static_cast<float>(spaceGlypAdvance);
                if (i < str.size() - 1)
                {
                    char nextCharacter = str[i + 1];
                    double dAdvence;
                    fontGeometry.getAdvance(dAdvence, character, nextCharacter);
                    advance = static_cast<float>(dAdvence);
                }

                x += fsScale * advance + kerning;
                continue;
            }

            if (character == '\t')
            {
                x += 4.0 * (fsScale * spaceGlypAdvance + kerning);
                continue;
            }

            auto glyph = fontGeometry.getGlyph(character);
            if (!glyph)
            {
                glyph = fontGeometry.getGlyph('?');
            }

            double atlasLeft, atlasBottom, atlasRight, atlasTop;
            glyph->getQuadAtlasBounds(atlasLeft, atlasBottom, atlasRight, atlasTop);
            glm::vec2 texCoordMin(static_cast<float>(atlasLeft), static_cast<float>(atlasBottom));
            glm::vec2 texCoordMax(static_cast<float>(atlasRight), static_cast<float>(atlasTop));

            double planeLeft, planeBottom, planeRight, planeTop;
            glyph->getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);
            glm::vec2 quadMin(static_cast<float>(planeLeft), static_cast<float>(planeBottom));
            glm::vec2 quadMax(static_cast<float>(planeRight), static_cast<float>(planeTop));

            quadMin *= fsScale;
            quadMax *= fsScale;

            quadMin += glm::vec2{ x, y };
            quadMax += glm::vec2{ x, y };

            float texelWidth = 1.0f / fontAtlasTexture->GetWidth();
            float texelHeight = 1.0f / fontAtlasTexture->GetHeight();

            texCoordMin *= glm::vec2{ texelWidth, texelHeight };
            texCoordMax *= glm::vec2{ texelWidth, texelHeight };

            {
                EnsureBatchCapacity(m_TextBatch, 4, 6, true, m_Cmd);

                m_TextBatch.vertexBufferPtr->position = transform * glm::vec4(quadMin, 0.0f, 1.0f);
                m_TextBatch.vertexBufferPtr->color = color;
                m_TextBatch.vertexBufferPtr->texCoord = texCoordMin;
                m_TextBatch.vertexBufferPtr->texIndex = texIndex;
                m_TextBatch.vertexBufferPtr->objectID = objectID;
                m_TextBatch.vertexBufferPtr++;

                m_TextBatch.vertexBufferPtr->position = transform * glm::vec4(quadMax, 0.0f, 1.0f);
                m_TextBatch.vertexBufferPtr->color = color;
                m_TextBatch.vertexBufferPtr->texCoord = texCoordMax;
                m_TextBatch.vertexBufferPtr->texIndex = texIndex;
                m_TextBatch.vertexBufferPtr->objectID = objectID;
                m_TextBatch.vertexBufferPtr++;

                m_TextBatch.vertexBufferPtr->position = transform * glm::vec4(quadMin.x, quadMax.y, 0.0f, 1.0f);
                m_TextBatch.vertexBufferPtr->color = color;
                m_TextBatch.vertexBufferPtr->texCoord = { texCoordMin.x, texCoordMax.y };
                m_TextBatch.vertexBufferPtr->texIndex = texIndex;
                m_TextBatch.vertexBufferPtr->objectID = objectID;
                m_TextBatch.vertexBufferPtr++;

                m_TextBatch.vertexBufferPtr->position = transform * glm::vec4(quadMax.x, quadMin.y, 0.0f, 1.0f);
                m_TextBatch.vertexBufferPtr->color = color;
                m_TextBatch.vertexBufferPtr->texCoord = { texCoordMax.x, texCoordMin.y };
                m_TextBatch.vertexBufferPtr->texIndex = texIndex;
                m_TextBatch.vertexBufferPtr->objectID = objectID;
                m_TextBatch.vertexBufferPtr++;

                m_TextBatch.indexCount += 6;
                m_TextBatch.count++;
                Renderer::Stats.textCount++;
            }

            if (i < str.size() - 1)
            {
                double advance = glyph->getAdvance();
                char nextCharacter = str[i + 1];
                fontGeometry.getAdvance(advance, character, nextCharacter);
                x += fsScale * advance + kerning;
            }
            else
            {
                x += fsScale * glyph->getAdvance() + kerning;
            }

            maxX = glm::max(maxX, x);
            minY = glm::min(minY, y);
        }
    }

    uint32_t Renderer2D::GetOrInsertQuadTexture(const Ref<Texture> &texture)
    {
        IGN_PROFILE_FUNCTION();

        if (texture == nullptr || (texture && !texture->GetHandle()))
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

            m_QuadBindingSetCache.clear(); // reset (so we can recreate it)
        }

        return textureIndex;
    }

    uint32_t Renderer2D::GetOrInsertFontTexture(const Ref<Texture> &texture)
    {
        IGN_PROFILE_FUNCTION();

        if (texture == nullptr || (texture && !texture->GetHandle()))
            return 0;

        uint32_t textureIndex = 0;

        // find texture
        for (uint32_t i = 0; i < m_TextBatch.textureSlotIndex; ++i)
        {
            if (*m_TextBatch.textureSlots[i] == *texture)
            {
                textureIndex = i;
                break;
            }
        }

        // insert if not found
        if (textureIndex == 0)
        {
            if (m_TextBatch.textureSlotIndex >= MAX_TEXTURE_BATCH_COUNT)
            {
                End();
                return MAX_TEXTURE_BATCH_COUNT;
            }

            textureIndex = m_TextBatch.textureSlotIndex;
            m_TextBatch.textureSlots[m_TextBatch.textureSlotIndex] = texture;
            m_TextBatch.textureSlotIndex++;

            m_TextBindingSetCache.clear(); // reset (so we can recreate it)
        }

        return textureIndex;
    }

	// Helper to build a quad pipeline for a framebuffer (once) and cache it.
	Ref<GraphicsPipeline> Renderer2D::GetQuadPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
		IGN_PROFILE_FUNCTION();

		auto key = MakeFramebufferKey(framebuffer);
		auto it = m_QuadPSOCache.find(key);
		if (it != m_QuadPSOCache.end())
			return it->second;

		m_QuadPSOCache.clear();
		m_QuadBindingSetCache.clear();

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
		bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

		GraphicsPipelineParams params;
		params.enableBlend = true;
		params.enableDepthWrite = hasDepthAttachment;
		params.enableDepthTest = hasDepthAttachment;
		params.enableDepthStencil = false;
		params.fillMode = fillMode;

		// create binding layout
		nvrhi::BindingLayoutDesc bindingLayoutDesc;
		bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(1));
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));

		for (uint8_t i = 0; i < MAX_TEXTURE_BATCH_COUNT; i++)
		{
			bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(i));
		}

		nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

		params.cullMode = nvrhi::RasterCullMode::None;
		params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

		Ref<Shader> vertexShader = Shader::Create("resources/shaders/batch_2d_quad.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
		Ref<Shader> pixelShader = Shader::Create("resources/shaders/batch_2d_quad.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

		Ref<GraphicsPipeline> gp = GraphicsPipeline::Create();
		gp->SetShaders({ vertexShader, pixelShader })
			.AddBindingLayout(bindingLayout)
			.Build(framebuffer, params);

		m_QuadPSOCache.emplace(key, gp);

		return gp;
	}

	Ref<GraphicsPipeline> Renderer2D::GetTextPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
		IGN_PROFILE_FUNCTION();

		auto key = MakeFramebufferKey(framebuffer);
		auto it = m_TextPSOCache.find(key);
		if (it != m_TextPSOCache.end())
			return it->second;

		m_TextPSOCache.clear();
		m_TextBindingSetCache.clear();

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
		bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

		GraphicsPipelineParams params;
		params.enableBlend = true;
		params.enableDepthWrite = hasDepthAttachment;
		params.enableDepthTest = hasDepthAttachment;
		params.enableDepthStencil = false;
		params.fillMode = fillMode;

		// create binding layout
		nvrhi::BindingLayoutDesc bindingLayoutDesc;
		bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(1));
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));

		for (uint8_t i = 0; i < MAX_TEXTURE_BATCH_COUNT; i++)
		{
			bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(i));
		}

		nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

		params.cullMode = nvrhi::RasterCullMode::None;
		params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

		Ref<Shader> vertexShader = Shader::Create("resources/shaders/msdf_font.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
		Ref<Shader> pixelShader = Shader::Create("resources/shaders/msdf_font.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

		Ref<GraphicsPipeline> gp = GraphicsPipeline::Create();
		gp->SetShaders({ vertexShader, pixelShader })
			.AddBindingLayout(bindingLayout)
			.Build(framebuffer, params);

		m_TextPSOCache.emplace(key, gp);

		return gp;
	}

	Ref<GraphicsPipeline> Renderer2D::GetCirclePipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
		IGN_PROFILE_FUNCTION();

		auto key = MakeFramebufferKey(framebuffer);
		auto it = m_CirclePSOCache.find(key);
		if (it != m_CirclePSOCache.end())
			return it->second;

		m_CirclePSOCache.clear();
		m_CircleBindingSetCache.clear();

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
		bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

		GraphicsPipelineParams params;
		params.enableBlend = true;
		params.enableDepthWrite = hasDepthAttachment;
		params.enableDepthTest = hasDepthAttachment;
		params.enableDepthStencil = false;
		params.fillMode = fillMode;
		params.cullMode = nvrhi::RasterCullMode::None;
		params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

		nvrhi::BindingLayoutDesc bindingLayoutDesc;
		bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));

		nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

		Ref<Shader> vertexShader = Shader::Create("resources/shaders/batch_2d_circle.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
		Ref<Shader> pixelShader = Shader::Create("resources/shaders/batch_2d_circle.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

		Ref<GraphicsPipeline> gp = GraphicsPipeline::Create();
		gp->SetShaders({ vertexShader, pixelShader })
			.AddBindingLayout(bindingLayout)
			.Build(framebuffer, params);

		m_CirclePSOCache.emplace(key, gp);

		return gp;
	}

	// Helper to build a line pipeline for a framebuffer (once) and cache it.
	Ref<GraphicsPipeline> Renderer2D::GetLinePipelineForFB(nvrhi::IFramebuffer *framebuffer)
	{
		IGN_PROFILE_FUNCTION();

		auto key = MakeFramebufferKey(framebuffer);
		auto it = m_LinePSOCache.find(key);
		if (it != m_LinePSOCache.end())
			return it->second;

		m_LinePSOCache.clear();
		m_LineBindingSetCache.clear();

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
		bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

		GraphicsPipelineParams params;
		params.enableBlend = true;
		params.enableDepthWrite = hasDepthAttachment;
		params.enableDepthTest = hasDepthAttachment;
		params.enableDepthStencil = false;
		params.fillMode = nvrhi::RasterFillMode::Wireframe;
		params.cullMode = nvrhi::RasterCullMode::None;
		params.primitiveType = nvrhi::PrimitiveType::LineList;
		params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

		Ref<GraphicsPipeline> gp = GraphicsPipeline::Create();
		nvrhi::BindingLayoutDesc bindingLayoutDesc;
		bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
		nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

		Ref<Shader> vertexShader = Shader::Create("resources/shaders/batch_2d_line.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
		Ref<Shader> pixelShader = Shader::Create("resources/shaders/batch_2d_line.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

		gp->SetShaders({ vertexShader, pixelShader })
			.AddBindingLayout(bindingLayout)
			.Build(framebuffer, params);

		m_LinePSOCache.emplace(key, gp);

		return gp;
	}

	nvrhi::BindingSetHandle Renderer2D::GetQuadBindingSet(nvrhi::IBindingLayout *bindingLayout, const std::vector<Ref<Texture>> &textures, const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &lightingBuffer)
	{
		IGN_PROFILE_FUNCTION();

		CameraLightingBindingKey key{ bindingLayout, cameraBuffer ? cameraBuffer->GetHandle() : nullptr, lightingBuffer ? lightingBuffer->GetHandle() : nullptr };
		auto it = m_QuadBindingSetCache.find(key);
		if (it != m_QuadBindingSetCache.end())
			return it->second;

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		nvrhi::SamplerHandle sampler;
		Ref<Texture> whiteTexture = Renderer::GetWhiteTexture();
		for (uint8_t i = 1; i < MAX_TEXTURE_BATCH_COUNT; ++i)
		{
			if (i >= textures.size())
				break;

			Ref<Texture> tex = textures[i];
			if (tex && tex.get() != whiteTexture.get() && tex->GetSampler())
			{
				sampler = tex->GetSampler();
				break;
			}
		}

		if (!sampler)
		{
			Ref<Texture> fallback = Renderer::GetWhiteTexture();
			sampler = fallback ? fallback->GetSampler() : nullptr;
		}

		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer->GetHandle()));
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, lightingBuffer->GetHandle()));
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

		m_QuadBindingSetCache.emplace(key, bindingSet);

		return bindingSet;
	}

	nvrhi::BindingSetHandle Renderer2D::GetTextBindingSet(nvrhi::IBindingLayout *bindingLayout, const std::vector<Ref<Texture>> &textures, const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &lightingBuffer)
	{
		IGN_PROFILE_FUNCTION();

		CameraLightingBindingKey key{ bindingLayout, cameraBuffer ? cameraBuffer->GetHandle() : nullptr, lightingBuffer ? lightingBuffer->GetHandle() : nullptr };
		auto it = m_TextBindingSetCache.find(key);
		if (it != m_TextBindingSetCache.end())
			return it->second;

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		nvrhi::SamplerHandle sampler;
		Ref<Texture> whiteTexture = Renderer::GetWhiteTexture();
		for (uint8_t i = 1; i < MAX_TEXTURE_BATCH_COUNT; ++i)
		{
			if (i >= textures.size())
				break;

			Ref<Texture> tex = textures[i];
			if (tex && tex.get() != whiteTexture.get() && tex->GetSampler())
			{
				sampler = tex->GetSampler();
				break;
			}
		}

		if (!sampler)
		{
			Ref<Texture> fallback = Renderer::GetWhiteTexture();
			sampler = fallback ? fallback->GetSampler() : nullptr;
		}

		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer->GetHandle()));
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, lightingBuffer->GetHandle()));
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

		m_TextBindingSetCache.emplace(key, bindingSet);

		return bindingSet;
	}

	nvrhi::BindingSetHandle Renderer2D::GetLineBindingSet(nvrhi::IBindingLayout *bindingLayout, const Ref<ConstantBuffer> &cameraBuffer)
	{
		IGN_PROFILE_FUNCTION();

		CameraBindingKey key{ bindingLayout, cameraBuffer ? cameraBuffer->GetHandle() : nullptr };
		auto it = m_LineBindingSetCache.find(key);
		if (it != m_LineBindingSetCache.end())
			return it->second;

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		// create binding set
		nvrhi::BindingSetDesc bindingSetDesc;
		// add constant buffer
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer->GetHandle()));

		nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
		LOG_ASSERT(bindingSet, "[Renderer 2D] Failed to create binding");

		m_LineBindingSetCache.emplace(key, bindingSet);

		return bindingSet;
	}

	nvrhi::BindingSetHandle Renderer2D::GetCircleBindingSet(nvrhi::IBindingLayout *bindingLayout, const Ref<ConstantBuffer> &cameraBuffer)
	{
		IGN_PROFILE_FUNCTION();

		CameraBindingKey key{ bindingLayout, cameraBuffer ? cameraBuffer->GetHandle() : nullptr };
		auto it = m_CircleBindingSetCache.find(key);
		if (it != m_CircleBindingSetCache.end())
			return it->second;

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		nvrhi::BindingSetDesc bindingSetDesc;
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer->GetHandle()));

		nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
		LOG_ASSERT(bindingSet, "[Renderer 2D] Failed to create binding");

		m_CircleBindingSetCache.emplace(key, bindingSet);

		return bindingSet;
	}
}
