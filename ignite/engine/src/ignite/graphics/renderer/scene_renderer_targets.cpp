// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "scene_renderer.hpp"
#include "ignite/graphics/binding_cache.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/ui/widget_renderer.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/profiler/profiler.hpp"

namespace ignite
{
    void SceneRenderer::ResizeFramebuffer(ICamera *camera, uint32_t width, uint32_t height)
    {
        IGN_PROFILE_FUNCTION();
        if (!camera || width == 0 || height == 0)
            return;

        // Descriptor sets cached for the old attachment views must not be reused
        // after those views are destroyed. Wait once for this camera resize, then
        // recreate all of its render targets without one device-wide stall per RT.
        if (!m_ResizeWaitedThisFrame)
        {
            GPUUploadSync::DeviceWaitIdle(m_Device);
            BindingCache::Clear();
            m_ResizeWaitedThisFrame = true;
        }

        ISceneRenderer::ResizeFramebuffer(camera, width, height);

        bool isGameCamera = false;
        if (m_Scene)
        {
            if (Entity primaryCamera = m_Scene->GetPrimaryCamera())
            {
                const auto &cc = primaryCamera.GetComponent<CameraComponent>();
                if (camera == &cc.camera)
                    isGameCamera = true;
            }
        }

        PostProcessing postProcessing = camera->postProcessing;
        if (isGameCamera && m_Scene)
        {
            if (Entity primaryCamera = m_Scene->GetPrimaryCamera())
            {
                const auto &cc = primaryCamera.GetComponent<CameraComponent>();
                postProcessing = cc.camera.postProcessing;
            }
        }
        postProcessing.renderScale = glm::clamp(postProcessing.renderScale * sceneRenderSettings.renderScale, 0.25f, 1.0f);

        const uint32_t renderWidth = std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(width) * postProcessing.renderScale)));
        const uint32_t renderHeight = std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(height) * postProcessing.renderScale)));

        m_CompositeBindingSetCache.clear();
        m_DebugGridBindingSetCache.clear();
        m_FXAABindingSetCache.clear();

        if (auto it = m_Blooms.find(camera); it != m_Blooms.end())
        {
            for (auto &bloom : it->second)
            {
                if (bloom)
                    bloom->Resize(renderWidth, renderHeight);
            }
        }

        if (auto it = m_SSAOs.find(camera); it != m_SSAOs.end())
        {
            for (auto &ssao : it->second)
            {
                if (ssao)
                    ssao->Resize(renderWidth, renderHeight);
            }
        }

        auto it = m_RenderTargets.find(camera);
        if (it == m_RenderTargets.end())
        {
            GetOrCreateRenderTarget(camera);
            it = m_RenderTargets.find(camera);
        }

        if (it != m_RenderTargets.end())
        {
            auto &target = it->second;

            if (target->sceneRT)
                target->sceneRT->Resize(renderWidth, renderHeight, false);
            if (target->selectionRT)
                target->selectionRT->Resize(renderWidth, renderHeight, false);
            if (target->sceneResolvedRT)
                target->sceneResolvedRT->Resize(renderWidth, renderHeight, false);
            if (target->widgetRT)
                target->widgetRT->Resize(width, height, false);
            if (target->compositeRT)
                target->compositeRT->Resize(width, height, false);
            if (target->postProcessRT)
                target->postProcessRT->Resize(width, height, false);
            for (Ref<RenderTarget> &historyRT : target->taaHistoryRT)
            {
                if (historyRT)
                    historyRT->Resize(width, height, false);
            }
            target->taaHistoryValid = false;

            if (target->debugRT)
            {
                target->debugRT->GetCreateInfo().depthAttachmentOverride = (target->sceneRT && target->msaaSampleCount == 1) ? target->sceneRT->GetDepthAttachment() : nullptr;
                target->debugRT->Resize(renderWidth, renderHeight, false);
            }
        }
    }

    void SceneRenderer::EnsureWidgetRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height)
    {
        if (!target->widgetRT)
        {
            RenderTargetCreateInfo createInfo = {};
            createInfo.width = width;
            createInfo.height = height;
            createInfo.attachments =
            {
                FramebufferAttachments{ "[Scene DepthAttachment]", nri::Format::D32_SFLOAT_S8_UINT, nri::Layout::DEPTH_STENCIL_ATTACHMENT },
                FramebufferAttachments{ "[Scene ColorAttachment]", nri::Format::RGBA8_UNORM, nri::Layout::COLOR_ATTACHMENT },
            };
            target->widgetRT = RenderTarget::Create(createInfo, "[Scene Renderer] Widget RT");
        }
        else if (target->widgetRT->GetWidth() != width || target->widgetRT->GetHeight() != height)
        {
            target->widgetRT->Resize(width, height);
        }
    }

    void SceneRenderer::EnsureDebugRT(Ref<CameraRenderTarget> target, uint32_t renderWidth, uint32_t renderHeight)
    {
        if (!target->debugRT)
        {
            RenderTargetCreateInfo createInfo = {};
            createInfo.width = renderWidth;
            createInfo.height = renderHeight;
            createInfo.attachments =
            {
                FramebufferAttachments{ "[Scene DebugAttachment]", nri::Format::RGBA8_UNORM, nri::Layout::COLOR_ATTACHMENT },
                FramebufferAttachments{ "[Scene DebugObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT },
                FramebufferAttachments{ "[Scene DepthAttachment]", nri::Format::D32_SFLOAT_S8_UINT, nri::Layout::DEPTH_STENCIL_ATTACHMENT }
            };
            createInfo.depthAttachmentOverride = (target->sceneRT && target->msaaSampleCount == 1) ? target->sceneRT->GetDepthAttachment() : nullptr;
            target->debugRT = RenderTarget::Create(createInfo, "[Scene Renderer] Debug RT");
        }
        else if (target->debugRT->GetWidth() != renderWidth || target->debugRT->GetHeight() != renderHeight)
        {
            target->debugRT->GetCreateInfo().depthAttachmentOverride = (target->sceneRT && target->msaaSampleCount == 1) ? target->sceneRT->GetDepthAttachment() : nullptr;
            target->debugRT->Resize(renderWidth, renderHeight);
        }
    }

    void SceneRenderer::EnsureTAAHistoryRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height)
    {
        if (!target->taaHistoryRT[0])
        {
            RenderTargetCreateInfo createInfo = {};
            createInfo.width = width;
            createInfo.height = height;
            createInfo.attachments =
            {
                FramebufferAttachments{ "[TAA History Color Attachment]", nri::Format::RGBA8_UNORM, nri::Layout::SHADER_RESOURCE }
            };
            for (Ref<RenderTarget> &historyRT : target->taaHistoryRT)
                historyRT = RenderTarget::Create(createInfo, "[Scene Renderer] TAA History RT");
        }
        else if (target->taaHistoryRT[0]->GetWidth() != width || target->taaHistoryRT[0]->GetHeight() != height)
        {
            for (Ref<RenderTarget> &historyRT : target->taaHistoryRT)
            {
                if (historyRT)
                    historyRT->Resize(width, height);
            }
        }
    }

    void SceneRenderer::EnsureMSAARenderTargets(Ref<CameraRenderTarget> target, int desiredSampleCount, uint32_t renderWidth, uint32_t renderHeight)
    {
        if (!target)
            return;

        const bool sampleCountChanged = target->msaaSampleCount != desiredSampleCount;
        const bool needsResolveRT = desiredSampleCount > 1;
        const bool resolveRTMissing = needsResolveRT && !target->sceneResolvedRT;
        if (!sampleCountChanged && !resolveRTMissing)
            return;

        target->msaaSampleCount = desiredSampleCount;

        RenderTargetCreateInfo sceneRTCreateInfo = {};
        sceneRTCreateInfo.width = renderWidth;
        sceneRTCreateInfo.height = renderHeight;
        sceneRTCreateInfo.sampleCount = desiredSampleCount;
        sceneRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Scene DepthAttachment]", nri::Format::D32_SFLOAT_S8_UINT, nri::Layout::DEPTH_STENCIL_ATTACHMENT },
            FramebufferAttachments{ "[Scene ColorAttachment]", nri::Format::RGBA16_SFLOAT, nri::Layout::COLOR_ATTACHMENT },
            FramebufferAttachments{ "[Scene ObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT }
        };
        target->sceneRT = RenderTarget::Create(sceneRTCreateInfo, "[Scene Renderer] Scene RT");

        if (desiredSampleCount > 1)
        {
            RenderTargetCreateInfo resolvedRTCreateInfo = {};
            resolvedRTCreateInfo.width = renderWidth;
            resolvedRTCreateInfo.height = renderHeight;
            resolvedRTCreateInfo.sampleCount = 1;
            resolvedRTCreateInfo.attachments =
            {
                FramebufferAttachments{ "[Scene Resolved ColorAttachment]", nri::Format::RGBA16_SFLOAT, nri::Layout::SHADER_RESOURCE },
                FramebufferAttachments{ "[Scene Resolved ObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::SHADER_RESOURCE }
            };
            target->sceneResolvedRT = RenderTarget::Create(resolvedRTCreateInfo, "[Scene Renderer] Scene Resolved RT");
        }
        else
        {
            target->sceneResolvedRT = nullptr;
        }

        target->debugRT = nullptr;
        m_CompositeBindingSetCache.clear();
    }

    Ref<CameraRenderTarget> SceneRenderer::GetOrCreateRenderTarget(ICamera *camera)
    {
        auto it = m_RenderTargets.find(camera);
        if (it != m_RenderTargets.end())
            return it->second;

        Ref<CameraRenderTarget> target = CreateRef<CameraRenderTarget>();
        const PostProcessing &pp = camera ? camera->postProcessing : PostProcessing{};
        int sampleCount = 1;
        if (sceneRenderSettings.sampleMethod == SampleMethod::MSAA)
            sampleCount = sceneRenderSettings.msaaProperties.sampleCount;
        else if (sceneRenderSettings.sampleMethod != SampleMethod::NONE && sceneRenderSettings.sampleMethod != SampleMethod::FXAA && sceneRenderSettings.sampleMethod != SampleMethod::TAA)
            sampleCount = pp.msaaProperties.enable ? pp.msaaProperties.sampleCount : 1;

        if (sampleCount <= 1) sampleCount = 1;
        else if (sampleCount <= 2) sampleCount = 2;
        else if (sampleCount <= 4) sampleCount = 4;
        else sampleCount = 8;
        target->msaaSampleCount = sampleCount;

        RenderTargetCreateInfo sceneRTCreateInfo = {};
        sceneRTCreateInfo.sampleCount = sampleCount;
        sceneRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Scene DepthAttachment]", nri::Format::D32_SFLOAT_S8_UINT, nri::Layout::DEPTH_STENCIL_ATTACHMENT },
            FramebufferAttachments{ "[Scene ColorAttachment]", nri::Format::RGBA16_SFLOAT, nri::Layout::COLOR_ATTACHMENT },
            FramebufferAttachments{ "[Scene ObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT }
        };
        target->sceneRT = RenderTarget::Create(sceneRTCreateInfo, "[Scene Renderer] Scene RT");

        RenderTargetCreateInfo selectionRTCreateInfo = {};
        selectionRTCreateInfo.sampleCount = 1;
        selectionRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Selection Mask ColorAttachment]", nri::Format::RGBA16_SFLOAT, nri::Layout::COLOR_ATTACHMENT },
            FramebufferAttachments{ "[Selection Mask ObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT }
        };
        target->selectionRT = RenderTarget::Create(selectionRTCreateInfo, "[Scene Renderer] Selection Mask RT");

        if (sampleCount > 1)
        {
            RenderTargetCreateInfo resolvedRTCreateInfo = {};
            resolvedRTCreateInfo.sampleCount = 1;
            resolvedRTCreateInfo.attachments =
            {
                FramebufferAttachments{ "[Scene Resolved ColorAttachment]", nri::Format::RGBA16_SFLOAT, nri::Layout::SHADER_RESOURCE },
                FramebufferAttachments{ "[Scene Resolved ObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::SHADER_RESOURCE }
            };
            target->sceneResolvedRT = RenderTarget::Create(resolvedRTCreateInfo, "[Scene Renderer] Scene Resolved RT");
        }

        RenderTargetCreateInfo compositeRTCreateInfo = {};
        compositeRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Composite Color Attachment]", nri::Format::RGBA8_UNORM, nri::Layout::COLOR_ATTACHMENT }
        };
        target->compositeRT = RenderTarget::Create(compositeRTCreateInfo, "[Scene Renderer] Composite RT");
        target->postProcessRT = RenderTarget::Create(compositeRTCreateInfo, "[Scene Renderer] Post Process RT");

        if (!m_WidgetRenderer)
            m_WidgetRenderer = WidgetRenderer::Create(1280, 720);

        m_RenderTargets.emplace(camera, target);

        const glm::uvec2 vpSize = camera ? camera->GetViewportSize() : glm::uvec2(0);
        if (vpSize.x > 0 && vpSize.y > 0)
            ResizeFramebuffer(camera, vpSize.x, vpSize.y);

        return target;
    }

    std::vector<Ref<Bloom>> SceneRenderer::GetOrCreateBlooms(ICamera *camera)
    {
        constexpr uint32_t resolution = 1080;
        auto it = m_Blooms.find(camera);
        if (it != m_Blooms.end())
            return it->second;

        const uint32_t maxFrame = DeviceManager::GetInstance()->GetDeviceParameters().maxFramesInFlight;
        std::vector<Ref<Bloom>> blooms(maxFrame);
        for (uint32_t i = 0; i < maxFrame; ++i)
            blooms[i] = CreateRef<Bloom>(resolution, resolution);

        m_Blooms.emplace(camera, blooms);
        return blooms;
    }

    std::vector<Ref<SSAO>> SceneRenderer::GetOrCreateSSAOs(nri::CommandBuffer *cmd, ICamera *camera)
    {
        constexpr uint32_t resolution = 1080;
        auto it = m_SSAOs.find(camera);
        if (it != m_SSAOs.end())
            return it->second;

        const uint32_t maxFrame = DeviceManager::GetInstance()->GetDeviceParameters().maxFramesInFlight;
        std::vector<Ref<SSAO>> SSAOs(maxFrame);
        for (uint32_t i = 0; i < maxFrame; ++i)
            SSAOs[i] = CreateRef<SSAO>(cmd, resolution, resolution);

        m_SSAOs.emplace(camera, SSAOs);
        return SSAOs;
    }
}
