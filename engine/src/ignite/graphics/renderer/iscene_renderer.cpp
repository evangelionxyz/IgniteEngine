// Copyright (c) 2026 Evangelion Manuhutu

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
    struct CompositePostProcess_GPUData
    {
        glm::vec4 flags = glm::vec4(0.0f);
        glm::vec4 vignetteParams = glm::vec4(0.0f);
        glm::vec4 chromAbParams = glm::vec4(0.0f);
        glm::vec4 vignetteColor = glm::vec4(0.0f);
    };

    struct DebugGrid_GPUData
    {
        glm::vec4 thinColor = glm::vec4(0.0f);
        glm::vec4 thickColor = glm::vec4(0.0f);
        glm::vec4 xAxisColor = glm::vec4(0.0f);
        glm::vec4 yAxisColor = glm::vec4(0.0f);
        glm::vec4 zAxisColor = glm::vec4(0.0f);
        glm::vec4 settings0 = glm::vec4(0.0f);
        glm::vec4 settings1 = glm::vec4(0.0f);
    };

    ISceneRenderer::ISceneRenderer()
    {
        m_Device = DeviceManager::GetInstance()->GetDevice();
        auto samplerDesc = nvrhi::SamplerDesc();
        samplerDesc.setAllFilters(false);
        samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_CompositeSampler = m_Device->createSampler(samplerDesc);

        std::array vertices
        {
            VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
            VertexScreen{ { -1.0f,  1.0f }, { 0.0f, 0.0f } },
            VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },

            VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },
            VertexScreen{ {  1.0f, -1.0f }, { 1.0f, 1.0f } },
            VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
        };

        m_CompositeVertexBuffer = VertexBuffer::Create(sizeof(vertices));

        m_Renderer2D = Renderer2D::Create();
        m_EdgeDetection = EdgeDetection::Create();
        m_EdgeDetection->CreatePipeline();
        m_DebugGridBuffer = ConstantBuffer::Create(sizeof(DebugGrid_GPUData), true, 16, "Debug Grid Buffer");
        m_CompositePostProcessBuffer = ConstantBuffer::Create(sizeof(CompositePostProcess_GPUData), true, 16, "Composite PostProcess Buffer");
        m_EditorBloom = CreateRef<Bloom>(1280, 720);
        m_EditorSSAO = CreateRef<SSAO>(1280, 720);
        
        m_GameplayBloom = CreateRef<Bloom>(1280, 720);
        m_GameplaySSAO = CreateRef<SSAO>(1280, 720);

        m_CascadedShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::HIGH);

        m_SceneBuffer = ConstantBuffer::Create(sizeof(SceneBufferData), false, 1, "[SceneRenderer] Scene Buffer");
        m_CascadedShadowMapBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMapBufferData), false, 1, "[SceneRenderer] CSM Buffer");
        m_CameraBuffer = ConstantBuffer::Create(sizeof(CameraBufferData), false, 1, "[SceneRenderer] Camera buffer");
    }

    void ISceneRenderer::EnsureCompositeVertexBufferUploaded(nvrhi::ICommandList *cmd)
    {
        if (!m_CompositeVertexBufferUploadPending || !cmd || !m_CompositeVertexBuffer)
        {
            return;
        }

        const std::array vertices
        {
            VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
            VertexScreen{ { -1.0f,  1.0f }, { 0.0f, 0.0f } },
            VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },

            VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },
            VertexScreen{ {  1.0f, -1.0f }, { 1.0f, 1.0f } },
            VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
        };

        m_CompositeVertexBuffer->SetData(cmd, Buffer((void *)vertices.data(), sizeof(vertices)));
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
        m_CascadedShadowMapBuffer = nullptr;
        m_CameraBuffer = nullptr;
    }

    void ISceneRenderer::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        if (m_EditorBloom)
        {
            m_EditorBloom->Resize(width, height);
        }

        if (m_EditorSSAO)
        {
            m_EditorSSAO->Resize(width, height);
        }

        if (m_GameplayBloom)
        {
            m_GameplayBloom->Resize(width, height);
        }

        if (m_GameplaySSAO)
        {
            m_GameplaySSAO->Resize(width, height);
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
        if (m_CascadedShadowMap)
        {
            return m_CascadedShadowMap->GetDepthTexture();
        }

        return nullptr;
    }

    Ref<CascadedShadowMap> ISceneRenderer::GetCascadedShadowMap()
    {
        return m_CascadedShadowMap;
    }
}

