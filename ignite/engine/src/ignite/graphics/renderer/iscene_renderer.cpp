// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "iscene_renderer.hpp"
#include "renderer_2d.hpp"
#include "ignite/graphics/bindless_system.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/core/application.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/project/project.hpp"

#include "ignite/graphics/objects/environment.hpp"
#include "ignite/scene/component.hpp"

namespace ignite
{
    ISceneRenderer::ISceneRenderer()
        : m_CascadedShadowMapBuffer(sizeof(CSM_GPUData), false, 1, "[SceneRenderer] CSM Buffer")
        , m_CompositePostProcessBuffer(sizeof(CompositePostProcess_GPUData), true, 16, "Composite PostProcess Buffer")
        , m_FXAABuffer(sizeof(glm::vec4), true, 16, "FXAA Buffer")
        , m_DebugGridBuffer(sizeof(DebugGrid_GPUData), true, 16, "Debug Grid Buffer")
    {
        for (int i = 0; i < NUM_CASCADES; ++i)
        {
            m_CSMPerCascadeBuffers[i] = ConstantBuffer::Create(sizeof(CSM_GPUData), false, 1, "[SceneRenderer] CSM Per-Cascade Buffer " + std::to_string(i));
        }

        static const std::array screenVertices
        {
            VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
            VertexScreen{ { -1.0f,  1.0f }, { 0.0f, 0.0f } },
            VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },

            VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },
            VertexScreen{ {  1.0f, -1.0f }, { 1.0f, 1.0f } },
            VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
        };

        m_CompositeVertexBuffer = VertexBuffer::Create(sizeof(screenVertices));
        auto *deviceManager = DeviceManager::GetInstance();
        m_Device = deviceManager->GetDevice();

        const auto &nri = deviceManager->GetCoreInterface();
        nri::Queue *graphicsQueue = deviceManager->GetGraphicsQueue();

        nri::FenceDesc fenceDesc = {};
        fenceDesc.initialValue = 0;
        nri.CreateFence(*m_Device, fenceDesc, m_CommandFence);

        for (auto &slot : m_CommandBufferSlots)
        {
            nri.CreateCommandAllocator(*graphicsQueue, slot.allocator);
            nri.CreateCommandBuffer(*slot.allocator, slot.commandBuffer);
            slot.fenceValue = 0;
        }

        m_CompositeVertexBuffer->SetData(screenVertices.data(), sizeof(screenVertices));
        m_CompositeVertexBufferUploadPending = false;
    }

    nri::CommandBuffer *ISceneRenderer::BeginCommandBuffer()
    {
        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        auto &slot = m_CommandBufferSlots[m_CurrentSlotIndex];
        m_CurrentSlotIndex = (m_CurrentSlotIndex + 1) % static_cast<uint32_t>(m_CommandBufferSlots.size());

        if (m_CommandFence && slot.fenceValue > 0 && nri.GetFenceValue(*m_CommandFence) < slot.fenceValue)
        {
            nri.Wait(*m_CommandFence, slot.fenceValue);
        }

        nri.ResetCommandAllocator(*slot.allocator);
        nri.BeginCommandBuffer(*slot.commandBuffer, BindlessSystem::GetDescriptorPool());
        return slot.commandBuffer;
    }

    void ISceneRenderer::SubmitCommandBuffer(nri::CommandBuffer *cmd, bool waitIdle)
    {
        if (!cmd)
            return;

        auto *deviceManager = DeviceManager::GetInstance();
        const auto &nri = deviceManager->GetCoreInterface();
        nri.EndCommandBuffer(*cmd);

        const uint64_t signalValue = m_NextFenceValue++;
        for (auto &slot : m_CommandBufferSlots)
        {
            if (slot.commandBuffer == cmd)
            {
                slot.fenceValue = signalValue;
                break;
            }
        }

        nri::FenceSubmitDesc signalFence = {};
        signalFence.fence = m_CommandFence;
        signalFence.value = signalValue;
        signalFence.stages = nri::StageBits::ALL;

        nri::QueueSubmitDesc submitDesc = {};
        submitDesc.commandBuffers = &cmd;
        submitDesc.commandBufferNum = 1;
        if (m_CommandFence)
        {
            submitDesc.signalFences = &signalFence;
            submitDesc.signalFenceNum = 1;
        }

        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            nri.QueueSubmit(*deviceManager->GetGraphicsQueue(), submitDesc);
        }

        if (waitIdle && m_CommandFence)
        {
            nri.Wait(*m_CommandFence, signalValue);
        }
    }

    void ISceneRenderer::EnsureSceneEnvironmentMap()
    {
        IGN_PROFILE_FUNCTION();

        if (!m_WorldEnvironment)
        {
            m_WorldEnvironment = m_Scene->GetActiveWorldEnvironment();
        }

        if (m_WorldEnvironment)
        {
            if (!m_WorldEnvironment->environment)
            {
                m_WorldEnvironment->environment = Environment::Create();
                m_WorldEnvironment->gpuInitialized = false;
                m_WorldEnvironment->dirtyEnvironment = true;
            }

            m_WorldEnvironment->environment->SetSkyType(m_WorldEnvironment->skyType);

            if (m_WorldEnvironment->skyType == SkyType::HDRI)
            {
                const bool isHDRLoaded = m_WorldEnvironment->hdrHandle != AssetHandle(0);
                if (m_WorldEnvironment->dirtyEnvironment && m_WorldEnvironment->environment)
                {
                    Ref<Texture> hdrTexture;
                    if (isHDRLoaded)
                    {
                        hdrTexture = ResolveAsset<Texture>(m_WorldEnvironment->hdrHandle);
                        if (hdrTexture && hdrTexture->IsReady())
                        {
                            m_WorldEnvironment->environment->SetTexture(hdrTexture);
                        }
                    }
                    else
                    {
                        m_WorldEnvironment->environment->SetTexture(Renderer::GetBlackTexture());
                    }

                    // Keep retrieve HDR If it is loaded, but still empty
                    if (isHDRLoaded && (!hdrTexture || !hdrTexture->IsReady()))
                        m_WorldEnvironment->dirtyEnvironment = true;
                    else
                        m_WorldEnvironment->dirtyEnvironment = false;
                }
            }
            else
            {
                m_WorldEnvironment->dirtyEnvironment = false;
            }
        }
    }

    void ISceneRenderer::FillBoneArray(glm::mat4 (&out)[MAX_BONES], const std::vector<glm::mat4> &boneTransforms)
    {
        IGN_PROFILE_FUNCTION();

        const size_t boneCount = std::min(static_cast<size_t>(MAX_BONES), boneTransforms.size());
        if (boneCount > 0)
        {
            std::memcpy(out, boneTransforms.data(), boneCount * sizeof(glm::mat4));
        }
        for (size_t i = boneCount; i < MAX_BONES; ++i)
        {
            out[i] = glm::mat4(1.0f);
        }
    }

    Ref<Material> ISceneRenderer::ResolveMeshMaterial(int instanceIndex, const std::unordered_map<int, AssetHandle> &overrideMaterials, AssetHandle defaultMaterialHandle)
    {
        Ref<Material> material = nullptr;

        // Retrieve the Override Materials if available
        auto overrideIt = overrideMaterials.find(instanceIndex);
        if (overrideIt != overrideMaterials.end() && overrideIt->second != AssetHandle(0))
        {
            material = ResolveAsset<Material>(overrideIt->second);
        }

        // Retrive the attached material
        if (!material && defaultMaterialHandle != AssetHandle(0))
        {
            material = ResolveAsset<Material>(defaultMaterialHandle);
        }

        // Fallback
        if (!material)
        {
            material = Renderer::GetDefaultMaterial();
        }

        if (material)
        {
            if (!material->UpdateBindingSet(GetEnvironmentMapColorTexture(), GetCascadedShadowMapDepthTexture()))
            {
                return nullptr;
            }
        }

        return material;
    }

    ISceneRenderer::~ISceneRenderer()
    {
        GPUUploadSync::DeviceWaitIdle(m_Device);

        auto *deviceManager = DeviceManager::GetInstance();
        if (deviceManager)
        {
            const auto &nri = deviceManager->GetCoreInterface();
            if (m_CompositeSampler)
            {
                nri.DestroyDescriptor(m_CompositeSampler);
                m_CompositeSampler = nullptr;
            }

            for (auto &slot : m_CommandBufferSlots)
            {
                if (slot.commandBuffer)
                {
                    nri.DestroyCommandBuffer(slot.commandBuffer);
                    slot.commandBuffer = nullptr;
                }
                if (slot.allocator)
                {
                    nri.DestroyCommandAllocator(slot.allocator);
                    slot.allocator = nullptr;
                }
            }

            if (m_CommandFence)
            {
                nri.DestroyFence(m_CommandFence);
                m_CommandFence = nullptr;
            }
        }

        if (m_Scene)
        {
            m_Scene->SetSceneRenderer(nullptr);
            m_Scene = nullptr;
        }

        if (m_Renderer2D)
        {
            m_Renderer2D->InvalidatePreRenderCache();
        }

        m_Has2DPreRenderCache = false;
        m_MeshBindingSet = nullptr;
        m_SelectedEntities.clear();

        for (auto &CSMPerCascadeBuffer : m_CSMPerCascadeBuffers)
        {
            CSMPerCascadeBuffer = nullptr;
        }

        m_CascadedShadowMap.reset();
        m_CompositeVertexBuffer.reset();
        m_Renderer2D.reset();
        m_OutlineJFA.reset();
    }

    void ISceneRenderer::ResizeFramebuffer([[maybe_unused]] ICamera *camera, uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;
        
        // Set the viewport size
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        if (m_Renderer2D)
            m_Renderer2D->InvalidatePreRenderCache();
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

    void ISceneRenderer::SetSampleMethod(SampleMethod method)
    {
        sceneRenderSettings.SetSampleMethod(method);
    }
}
