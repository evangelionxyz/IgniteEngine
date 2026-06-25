// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "iscene_renderer.hpp"
#include "renderer_2d.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/core/application.hpp"
#include "ignite/scene/scene.hpp"

#include <array>

namespace ignite
{
    ISceneRenderer::ISceneRenderer()
    {
        m_SceneBuffer = ConstantBuffer::Create(sizeof(SceneBufferData), false, 1, "[SceneRenderer] Scene Buffer");
        m_CameraBuffer = ConstantBuffer::Create(sizeof(CameraBufferData), false, 1, "[SceneRenderer] Camera buffer");
        m_CascadedShadowMapBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMapBufferData), false, 1, "[SceneRenderer] CSM Buffer");
        for (int i = 0; i < NUM_CASCADES; ++i)
        {
            m_CSMPerCascadeBuffers[i] = ConstantBuffer::Create(sizeof(CascadedShadowMapBufferData), false, 1, "[SceneRenderer] CSM Per-Cascade Buffer " + std::to_string(i));
        }
    }

    void ISceneRenderer::EnsureCompositeVertexBufferUploaded(nvrhi::ICommandList *cmd)
    {
        if (!m_CompositeVertexBufferUploadPending || !cmd || !m_CompositeVertexBuffer)
        {
            return;
        }

        static constexpr std::array screenVertices
        {
            VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
            VertexScreen{ { -1.0f,  1.0f }, { 0.0f, 0.0f } },
            VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },

            VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },
            VertexScreen{ {  1.0f, -1.0f }, { 1.0f, 1.0f } },
            VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
        };

        m_CompositeVertexBuffer->SetData(cmd, Buffer((void *)screenVertices.data(), sizeof(screenVertices)));
        m_CompositeVertexBufferUploadPending = false;
    }

    ISceneRenderer::~ISceneRenderer()
    {
        GPUUploadSync::DeviceWaitIdle(m_Device);

        if (m_Scene)
        {
            m_Scene->SetSceneRenderer(nullptr);
            m_Scene = nullptr;
        }

        if (m_Renderer2D)
        {
            m_Renderer2D->ClearAssetResolveCache();
            m_Renderer2D->InvalidatePreRenderCache();
        }

        m_Has2DPreRenderCache = false;
        m_SelectedEntities.clear();

        m_MeshBindingSet = nullptr;

        m_SceneBuffer = nullptr;
        m_CameraBuffer = nullptr;
        m_CascadedShadowMapBuffer = nullptr;
        for (auto &CSMPerCascadeBuffer : m_CSMPerCascadeBuffers)
        {
            CSMPerCascadeBuffer = nullptr;
        }
    }

    void ISceneRenderer::ResizeFramebuffer(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        if (m_Renderer2D)
        {
            m_Renderer2D->InvalidatePreRenderCache();
        }

        m_Has2DPreRenderCache = false;
    }

    Ref<Texture> ISceneRenderer::GetEnvironmentMapColorTexture() const
    {
        return nullptr;
    }

    Ref<Texture> ISceneRenderer::GetCascadedShadowMapDepthTexture() const
    {
        return m_CascadedShadowMap ? m_CascadedShadowMap->GetDepthTexture() : nullptr;
    }

    Ref<CascadedShadowMap> ISceneRenderer::GetCascadedShadowMap()
    {
        return m_CascadedShadowMap;
    }
}

