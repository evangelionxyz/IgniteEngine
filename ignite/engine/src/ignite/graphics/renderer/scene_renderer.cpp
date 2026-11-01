// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "renderer_2d.hpp"
#include "scene_renderer.hpp"
#include "ignite/graphics/bindless_system.hpp"
#include "ignite/graphics/binding_cache.hpp"
#include "ignite/math/frustum.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"
#include "ignite/physics/3d/physics_3d.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"
#include "ignite/project/project.hpp"

#include "ignite/graphics/font.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_renderer.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/objects/procedural_sky.hpp"
#include "ignite/imgui/imgui_nri.hpp"

namespace ignite
{
    static const std::array<glm::mat4, MAX_BONES> s_IdentitySkeleton = []()
    {
        std::array<glm::mat4, MAX_BONES> buf;
        for (int i = 0; i < MAX_BONES; ++i)
        {
            buf[i] = glm::mat4(1.0f);
        }
        return buf;
    }();

    static float Halton(uint32_t index, uint32_t base)
    {
        float result = 0.0f;
        float fraction = 1.0f / static_cast<float>(base);
        while (index > 0)
        {
            result += static_cast<float>(index % base) * fraction;
            index /= base;
            fraction /= static_cast<float>(base);
        }
        return result;
    }

    static glm::vec2 GetTAAJitter(uint64_t frameIndex, uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return { 0.0f, 0.0f };

        const auto haltonIndex = static_cast<uint32_t>(frameIndex % 8u) + 1u;
        const glm::vec2 halton = glm::vec2(Halton(haltonIndex, 2), Halton(haltonIndex, 3));
        return (halton * 2.0f - 1.0f) / glm::vec2(static_cast<float>(width), static_cast<float>(height));
    }

    // ===============================
    // Scene Renderer Implementation
    // ===============================
    SceneRenderer::SceneRenderer()
    {
        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        nri::SamplerDesc compositeSamplerDesc = {};
        compositeSamplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR };
        compositeSamplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
        compositeSamplerDesc.mipMax = 16.0f;
        nri.CreateSampler(*m_Device, compositeSamplerDesc, m_CompositeSampler);

        m_Renderer2D = Renderer2D::Create();
        m_OutlineJFA = OutlineJFA::Create();
        m_OutlineJFA->CreatePipeline();

        m_CascadedShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::HIGH);
        m_RuntimeMaterial   = CreateRef<Material>();

        m_TerrainRenderer = CreateRef<TerrainRenderer>();
        m_TerrainRenderer->Init();
    }

    SceneRenderer::~SceneRenderer()
    {
        m_WidgetRenderer = nullptr;
        m_WorldEnvironment = nullptr;

        m_AnimatedPSOCache.clear();
        m_TransparentAnimatedPSOCache.clear();
        m_AnimatedCSMPSOCache.clear();
        m_StaticPSOCache.clear();
        m_TransparentStaticPSOCache.clear();
        m_StaticCSMPSOCache.clear();
        m_SelectStaticPSOCache.clear();
        m_SelectAnimatedPSOCache.clear();
        m_EnvironmentPSOCache.clear();
        m_CompositePSOCache.clear();
        m_FXAAPSOCache.clear();
        m_DebugGridPSOCache.clear();
        m_CompositeBindingSetCache.clear();
        m_DebugGridBindingSetCache.clear();
        m_FXAABindingSetCache.clear();
        m_RenderTargets.clear();
        ImGui_NRI::ClearTextureCache();
        m_Blooms.clear();
        m_SSAOs.clear();

        m_ResolvedAssetsCache.clear();
        m_RuntimeMaterial.reset();
        m_TerrainRenderer.reset();

        if (m_Scene)
        {
            m_Scene->SetSceneRenderer(nullptr);
            m_Scene.reset();
        }
    }

    void SceneRenderer::SetActiveScene(const Ref<Scene> &scene)
    {
        if (m_Scene == scene)
        {
            return;
        }

        // The current frame may still reference scene textures, cached descriptor
        // sets, environment maps, and pipelines. Retire that work before releasing
        // any of those objects during a scene transition.
        if (m_Scene)
        {
            GPUUploadSync::DeviceWaitIdle(m_Device);
        }

        if (m_Scene)
        {
            m_Scene->SetSceneRenderer(nullptr);
        }

        if (m_WorldEnvironment)
        {
            if (scene == nullptr && m_WorldEnvironment->environment)
            {
                m_WorldEnvironment->environment.reset();
            }
            m_WorldEnvironment = nullptr;
        }

        m_SelectedEntities.clear();
        m_Has2DPreRenderCache = false;

        m_ResolvedAssetsCache.clear();

        m_Scene = scene;
        m_AnimatedPSOCache.clear();
        m_TransparentAnimatedPSOCache.clear();
        m_AnimatedCSMPSOCache.clear();
        m_StaticPSOCache.clear();
        m_TransparentStaticPSOCache.clear();
        m_StaticCSMPSOCache.clear();
        m_SelectStaticPSOCache.clear();
        m_SelectAnimatedPSOCache.clear();
        m_EnvironmentPSOCache.clear();
        m_CompositePSOCache.clear();
        m_FXAAPSOCache.clear();
        m_DebugGridPSOCache.clear();
        m_CompositeBindingSetCache.clear();
        m_DebugGridBindingSetCache.clear();
        m_FXAABindingSetCache.clear();

        m_RenderTargets.clear();
        ImGui_NRI::ClearTextureCache();
        m_EntityObjectIndexCache.clear();
        m_EntityBoneOffsetCache.clear();
        m_SocketObjectIndexCache.clear();

        m_Blooms.clear();
        m_SSAOs.clear();

        BindingCache::Clear();
        WidgetRenderer::ClearCache();

        if (m_RuntimeMaterial)
        {
            m_RuntimeMaterial->InvalidateBindingSet();
        }

        if (m_Renderer2D)
        {
            m_Renderer2D->InvalidatePreRenderCache();
        }

        if (m_Scene)
        {
            m_Scene->SetSceneRenderer(this);
        }
    }

    void SceneRenderer::BeginFrame()
    {
        m_ResizeWaitedThisFrame = false;
        m_Has2DPreRenderCache = false;
        if (m_Renderer2D)
        {
            m_Renderer2D->InvalidatePreRenderCache();
        }
    }

    RenderPlan SceneRenderer::BuildRenderPlan(ICamera *camera, bool drawDebug, const PostProcessing &postProcessing, bool isGameCamera)
    {
        RenderPlan plan;
        plan.isGameCamera = isGameCamera;

        if (!m_Scene || !m_Scene->registry)
            return plan;

        auto registry = m_Scene->registry;

        // 1. Check for Meshes (Static & Skeletal)
        {
            auto staticMeshView = registry->view<TransformComponent, RenderingComponent, StaticMeshComponent>();
            for (entt::entity e : staticMeshView)
            {
                const auto &[tr, rc, smc] = registry->get<TransformComponent, RenderingComponent, StaticMeshComponent>(e);
                if (rc.visible && smc.handle != AssetHandle(0))
                {
                    plan.hasMeshes = true;
                    break;
                }
            }

            if (!plan.hasMeshes)
            {
                auto skelMeshView = registry->view<TransformComponent, RenderingComponent, SkeletalMeshComponent>();
                for (entt::entity e : skelMeshView)
                {
                    const auto &[tr, rc, smc] = registry->get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(e);
                    if (rc.visible && smc.handle != AssetHandle(0))
                    {
                        plan.hasMeshes = true;
                        break;
                    }
                }
            }
        }

        // 2. Check for 2D Renderables (Sprites, Circles, Text)
        {
            auto spriteView = registry->view<TransformComponent, RenderingComponent, Sprite2DComponent>();
            for (entt::entity e : spriteView)
            {
                const auto &[tr, rc, sprite] = registry->get<TransformComponent, RenderingComponent, Sprite2DComponent>(e);
                if (rc.visible)
                {
                    plan.has2D = true;
                    break;
                }
            }

            if (!plan.has2D)
            {
                auto circleView = registry->view<TransformComponent, RenderingComponent, Circle2DComponent>();
                for (entt::entity e : circleView)
                {
                    const auto &[tr, rc, circle] = registry->get<TransformComponent, RenderingComponent, Circle2DComponent>(e);
                    if (rc.visible)
                    {
                        plan.has2D = true;
                        break;
                    }
                }
            }

            if (!plan.has2D)
            {
                auto textView = registry->view<TransformComponent, RenderingComponent, TextComponent>();
                for (entt::entity e : textView)
                {
                    const auto &[tr, rc, text] = registry->get<TransformComponent, RenderingComponent, TextComponent>(e);
                    if (rc.visible && !text.text.empty() && text.fontHandle != AssetHandle(0))
                    {
                        plan.has2D = true;
                        break;
                    }
                }
            }
        }

        // 3. Check for Terrain
        {
            auto terrainView = registry->view<TransformComponent, RenderingComponent, TerrainComponent>();
            for (entt::entity e : terrainView)
            {
                const auto &[tr, rc, tc] = registry->get<TransformComponent, RenderingComponent, TerrainComponent>(e);
                if (rc.visible && tc.data && !tc.data->heightmap.empty())
                {
                    plan.hasTerrain = true;
                    break;
                }
            }
        }

        // Check for Particle Systems
        {
            auto particleView = registry->view<TransformComponent, RenderingComponent, ParticleSystemComponent>();
            for (entt::entity e : particleView)
            {
                const auto &[tr, rc, pc] = registry->get<TransformComponent, RenderingComponent, ParticleSystemComponent>(e);
                if (rc.visible && pc.isPlaying)
                {
                    plan.hasParticles = true;
                    break;
                }
            }
        }

        // 4. Check for World Environment / Sky
        if (m_WorldEnvironment && m_WorldEnvironment->environment && !m_WorldEnvironment->dirtyEnvironment)
        {
            plan.hasEnvironment = true;
        }

        // 5. Check for Widgets
        if (m_Scene->GetRootWidget() != nullptr)
        {
            plan.hasWidgets = true;
        }

        // 6. Check for Active Directional Light with Cascaded Shadows
        {
            auto lightView = registry->view<TransformComponent, DirectionalLightComponent>();
            for (entt::entity e : lightView)
            {
                const auto &light = registry->get<DirectionalLightComponent>(e);
                if (light.cascadeShadow)
                {
                    plan.hasActiveShadows = true;
                    break;
                }
            }
        }

        // 7. Check for Selection / Object ID
        plan.requiresObjectId = !isGameCamera || !m_SelectedEntities.empty();

        // 8. Check for Debug Overlays
        if (drawDebug && (sceneRenderSettings.showBoundingBox || sceneRenderSettings.showPhysicsCollider))
        {
            plan.requiresDebugOverlay = true;
        }

        // 9. Check for Grid
        if (drawDebug)
        {
            if (camera && camera->projectionType == ProjectionType::Orthographic)
            {
                plan.requiresGrid = sceneRenderSettings.worldGrid2D.enabled;
            }
            else
            {
                plan.requiresGrid = sceneRenderSettings.worldGrid3D.enabled;
            }
        }

        // 10. Post processing requirements
        plan.requiresBloom = postProcessing.enableBloom;
        plan.requiresSSAO = postProcessing.enableSSAO;
        plan.requiresTAA = postProcessing.taaProperties.enable;

        return plan;
    }

    void SceneRenderer::Render(ICamera *camera, FrameContext *frameContext, bool drawDebug, RenderTarget *targetFramebuffer)
    {
        IGN_PROFILE_FUNCTION();

        if (!camera)
            return;

        Ref<CameraRenderTarget> target = GetOrCreateRenderTarget(camera);
        if (!target)
            return;

        EnsureSceneEnvironmentMap();

        // Scene post processing
        PostProcessing postProcessing = camera->postProcessing;
        CameraLens cameraLens = camera->lens;

        bool isGameCamera = false;
        if (Entity primaryCamera = m_Scene->GetPrimaryCamera())
        {
            const auto &cc = primaryCamera.GetComponent<CameraComponent>();
            postProcessing = cc.camera.postProcessing;
            cameraLens = cc.camera.lens;

            isGameCamera = camera == &cc.camera;
        }

        switch (sceneRenderSettings.sampleMethod)
        {
        case SampleMethod::NONE:
            postProcessing.taaProperties.enable = false;
            postProcessing.msaaProperties.enable = false;
            postProcessing.enableFXAA = false;
            sceneRenderSettings.enableFXAA = false;
            break;
        case SampleMethod::TAA:
            postProcessing.taaProperties.enable = true;
            postProcessing.taaProperties.blendFactor = sceneRenderSettings.taaProperties.blendFactor;
            postProcessing.msaaProperties.enable = false;
            postProcessing.enableFXAA = false;
            sceneRenderSettings.enableFXAA = false;
            break;
        case SampleMethod::MSAA:
            postProcessing.taaProperties.enable = false;
            postProcessing.msaaProperties.enable = true;
            postProcessing.msaaProperties.sampleCount = sceneRenderSettings.msaaProperties.sampleCount;
            postProcessing.enableFXAA = false;
            sceneRenderSettings.enableFXAA = false;
            break;
        case SampleMethod::FXAA:
            postProcessing.taaProperties.enable = false;
            postProcessing.msaaProperties.enable = false;
            postProcessing.enableFXAA = true;
            sceneRenderSettings.enableFXAA = true;
            break;
        }

        postProcessing.renderScale = glm::clamp(postProcessing.renderScale * sceneRenderSettings.renderScale, 0.25f, 1.0f);

        // Ensure MSAA render targets match current settings
        {
            const bool msaaEnabled = postProcessing.msaaProperties.enable;
            int desiredSampleCount = msaaEnabled ? postProcessing.msaaProperties.sampleCount : 1;
            if (desiredSampleCount <= 1) desiredSampleCount = 1;
            else if (desiredSampleCount <= 2) desiredSampleCount = 2;
            else if (desiredSampleCount <= 4) desiredSampleCount = 4;
            else desiredSampleCount = 8;

            const uint32_t baseWidth = target->compositeRT ? target->compositeRT->GetWidth() : m_ViewportWidth;
            const uint32_t baseHeight = target->compositeRT ? target->compositeRT->GetHeight() : m_ViewportHeight;
            const uint32_t renderWidth = std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(baseWidth) * postProcessing.renderScale)));
            const uint32_t renderHeight = std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(baseHeight) * postProcessing.renderScale)));

            EnsureMSAARenderTargets(target, desiredSampleCount, renderWidth, renderHeight);
        }

        // Create fresh command buffer for this frame
        nri::CommandBuffer *cmd = BeginCommandBuffer();
        {
            IGN_PROFILE_SCOPE("SceneRenderer::RecordEditorCommandList");

            glm::mat4 projection = camera->GetProjection();
            if (postProcessing.taaProperties.enable && target->sceneRT)
            {
                const glm::vec2 jitter = GetTAAJitter(m_TAAFrameIndex, target->sceneRT->GetWidth(), target->sceneRT->GetHeight());

                projection[2][0] += jitter.x;
                projection[2][1] += jitter.y;
            }

            // 1. Build render plan for this camera / frame
            RenderPlan plan = BuildRenderPlan(camera, drawDebug, postProcessing, isGameCamera);

            // 2. Empty-Scene Fast Path
            if (plan.IsEmptyScene())
            {
                IGN_PROFILE_SCOPE("SceneRenderer::EmptySceneFastPath");

                CameraBufferData cameraData = { projection, camera->GetView(), glm::vec4(camera->position, 1.0f) };
                frameContext->cameraBuffer.SetData(&cameraData, sizeof(cameraData));
                frameContext->sceneBuffer.SetData(&m_SceneGPUData, sizeof(m_SceneGPUData));

                CSM_GPUData sceneCascadeData = {};
                sceneCascadeData.cascadeIndex = -1;
                sceneCascadeData.shadowStrength = 0.0f;
                frameContext->csmBuffer.SetData(&sceneCascadeData, sizeof(sceneCascadeData));

                if (targetFramebuffer)
                {
                    targetFramebuffer->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                }
                else
                {
                    target->compositeRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                }

                if (target->previousPlan.requiresDebugOverlay && target->debugRT)
                {
                    target->debugRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                    target->debugRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
                }

                if (target->previousPlan.hasWidgets && target->widgetRT)
                {
                    target->widgetRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                    target->widgetRT->ClearDepthAttachment(cmd, 1.0f, 0);
                }

                if (plan.requiresGrid)
                {
                    RenderTarget *destFb = targetFramebuffer ? targetFramebuffer : target->compositeRT->GetFramebuffer();
                    const auto is2D = camera->projectionType == ProjectionType::Orthographic;
                    DrawDebugGrid(cmd, destFb, frameContext, is2D ? sceneRenderSettings.worldGrid2D : sceneRenderSettings.worldGrid3D, is2D);
                }

                SubmitCommandBuffer(cmd);
                target->previousPlan = plan;
                return;
            }

            // Full render path
            CameraBufferData cameraData = { projection, camera->GetView(), glm::vec4(camera->position, 1.0f) };
            {
                IGN_PROFILE_SCOPE("SceneRenderer::WriteFrameData");
                frameContext->cameraBuffer.SetData(&cameraData, sizeof(cameraData));
                frameContext->sceneBuffer.SetData(&m_SceneGPUData, sizeof(m_SceneGPUData));
            }

            if (plan.hasMeshes || plan.hasTerrain)
            {
                PreallocateGPUData(cmd, frameContext);
            }

            if (m_WorldEnvironment && m_WorldEnvironment->environment && !m_WorldEnvironment->gpuInitialized && !m_WorldEnvironment->dirtyEnvironment)
            {
                m_WorldEnvironment->environment->WriteBuffer(cmd);
                m_WorldEnvironment->gpuInitialized = true;
            }

            const auto width = target->compositeRT ? target->compositeRT->GetWidth() : m_ViewportWidth;
            const auto height = target->compositeRT ? target->compositeRT->GetHeight() : m_ViewportHeight;
            const auto renderWidth = target->sceneRT ? target->sceneRT->GetWidth() : width;
            const auto renderHeight = target->sceneRT ? target->sceneRT->GetHeight() : height;

            // Clear Render Targets (feature-gated)
            {
                target->sceneRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                if (plan.requiresObjectId || target->previousPlan.requiresObjectId)
                {
                    target->sceneRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
                }
                target->sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);

                // Keep the outline mask separate from the scene object-ID buffer used by picking.
                if (target->selectionRT)
                {
                    target->selectionRT->ClearColorAttachmentFloat(cmd, 0);
                    target->selectionRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
                }

                if (plan.hasWidgets)
                {
                    EnsureWidgetRT(target, width, height);
                    target->widgetRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                    target->widgetRT->ClearDepthAttachment(cmd, 1.0f, 0);
                }
                else if (target->previousPlan.hasWidgets && target->widgetRT)
                {
                    target->widgetRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                    target->widgetRT->ClearDepthAttachment(cmd, 1.0f, 0);
                }

                if (plan.requiresDebugOverlay)
                {
                    EnsureDebugRT(target, renderWidth, renderHeight);
                    target->debugRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                    target->debugRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
                }
                else if (target->previousPlan.requiresDebugOverlay && target->debugRT)
                {
                    target->debugRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                    target->debugRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
                }

                target->compositeRT->ClearColorAttachmentFloat(cmd, 0);
                target->compositeRT->ClearDepthAttachment(cmd, 1.0f, 0);
            }

            RenderTarget *sceneFramebuffer = target->sceneRT->GetFramebuffer();

            ShadowPass(cmd, camera, frameContext);

            if (m_WorldEnvironment && m_WorldEnvironment->environment && !m_WorldEnvironment->dirtyEnvironment)
            {
                if (m_WorldEnvironment->skyType == SkyType::ProceduralSky && m_WorldEnvironment->environment->GetProceduralSky())
                {
                    glm::vec3 sunDir = glm::vec3(m_SceneGPUData.sunDirection);
                    glm::vec3 sunCol = glm::vec3(m_SceneGPUData.sunColor);
                    float sunIntensity = glm::clamp(m_SceneGPUData.sunColor.a, 0.1f, 5.0f);
                    float sunRadius = m_SceneGPUData.sunAngularRadius;

                    m_WorldEnvironment->environment->GetProceduralSky()->GetParams() = m_WorldEnvironment->atmosphereParams;
                    m_WorldEnvironment->environment->GetProceduralSky()->RenderLUTs(cmd, sunDir, sunCol, sunIntensity, sunRadius, camera->position);
                }

                const Ref<GraphicsPipeline> envPSO = GetEnvironmentPSO(sceneFramebuffer, sceneRenderSettings.fillMode);
                m_WorldEnvironment->environment->Draw(cmd, sceneFramebuffer, envPSO, frameContext->cameraBuffer, frameContext->sceneBuffer);
            }

            RenderTarget *selectionFramebuffer = target->selectionRT
                ? target->selectionRT->GetFramebuffer()
                : nullptr;
            ColorPass(cmd, camera, frameContext, sceneFramebuffer, selectionFramebuffer, drawDebug);

            if (plan.hasWidgets && target->widgetRT)
            {
                UIPass(cmd, camera, target->widgetRT->GetFramebuffer(), frameContext);
            }

            if (plan.requiresDebugOverlay && target->debugRT)
            {
                RenderTarget *debugFramebuffer = target->debugRT->GetFramebuffer();
                DebugPass(cmd, camera, debugFramebuffer, frameContext);
            }

            // MSAA resolve: if the scene was rendered with multiple samples, resolve to a
            // single-sample texture so post-processing passes can sample it normally.
            const bool msaaActive = target->msaaSampleCount > 1 && target->sceneResolvedRT;
            if (msaaActive)
            {
                IGN_PROFILE_SCOPE("SceneRenderer::MSAAResolve");
                Ref<Texture> msaaColor = target->sceneRT->GetColorAttachment(0);
                Ref<Texture> resolvedColor = target->sceneResolvedRT->GetColorAttachment(0);
                if (msaaColor && resolvedColor)
                {
                    const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
                    nri::TextureBarrierDesc barriers[2] = {};
                    barriers[0].texture = resolvedColor->GetHandle();
                    barriers[0].before = { resolvedColor->GetCurrentAccess(), resolvedColor->GetCurrentLayout(), resolvedColor->GetCurrentStages() };
                    if (barriers[0].before.layout == nri::Layout::UNDEFINED)
                        barriers[0].before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
                    barriers[0].after = { nri::AccessBits::RESOLVE_DESTINATION, nri::Layout::RESOLVE_DESTINATION, nri::StageBits::RESOLVE };
                    barriers[0].mipNum = nri::REMAINING;
                    barriers[0].layerNum = nri::REMAINING;
                    barriers[0].planes = nri::PlaneBits::ALL;

                    barriers[1].texture = msaaColor->GetHandle();
                    barriers[1].before = { msaaColor->GetCurrentAccess(), msaaColor->GetCurrentLayout(), msaaColor->GetCurrentStages() };
                    if (barriers[1].before.layout == nri::Layout::UNDEFINED)
                        barriers[1].before = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                    barriers[1].after = { nri::AccessBits::RESOLVE_SOURCE, nri::Layout::RESOLVE_SOURCE, nri::StageBits::RESOLVE };
                    barriers[1].mipNum = nri::REMAINING;
                    barriers[1].layerNum = nri::REMAINING;
                    barriers[1].planes = nri::PlaneBits::ALL;

                    nri::BarrierDesc barrierDesc = {};
                    barrierDesc.textures = barriers;
                    barrierDesc.textureNum = 2;
                    nri.CmdBarrier(*cmd, barrierDesc);

                    nri.CmdResolveTexture(*cmd, *resolvedColor->GetHandle(), nullptr, *msaaColor->GetHandle(), nullptr, nri::ResolveOp::AVERAGE);

                    barriers[0].before = barriers[0].after;
                    barriers[0].after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                    barriers[1].before = barriers[1].after;
                    barriers[1].after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                    nri.CmdBarrier(*cmd, barrierDesc);

                    resolvedColor->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
                    msaaColor->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
                }
            }

            // Skip selection highlights/outlines for the game camera (gameplay viewport).
            // This avoids running OutlineJFA compute shaders and recreating the outline texture for the game's resolution.
            Ref<Texture> edgeTexture = nullptr;
            if (!isGameCamera && m_OutlineJFA && !m_SelectedEntities.empty())
            {
                if (!m_OutlineJFA->GetOutputTexture() || m_OutlineJFA->GetOutputTexture()->GetWidth() != static_cast<int>(renderWidth) || m_OutlineJFA->GetOutputTexture()->GetHeight() != static_cast<int>(renderHeight))
                {
                    m_OutlineJFA->CreateOutputTexture(renderWidth, renderHeight);
                }

                // Read the selected-object mask, not the scene object-ID buffer used by picking.
                Ref<Texture> edgeObjIDSrc = target->selectionRT ? target->selectionRT->GetColorAttachment(1) : nullptr;
                m_OutlineJFA->UpdateBindingSet(edgeObjIDSrc);

                constexpr uint32_t kMaxSelectedIDs = 100;
                const uint32_t selectedCount = static_cast<uint32_t>(std::min(m_SelectedEntities.size(), static_cast<size_t>(kMaxSelectedIDs)));
                if (selectedCount > 0)
                {
                    m_OutlineJFA->SetSelectedIDs(m_SelectedEntities.data(), selectedCount);

                    OutlineJFAParameter jfaParams;
                    jfaParams.outlineWidth = 2.5f;
                    jfaParams.outlineColor = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);
                    jfaParams.selectedCount = selectedCount;

                    m_OutlineJFA->ExecuteCompute(cmd, jfaParams, renderWidth, renderHeight);
                    edgeTexture = m_OutlineJFA->GetOutputTexture();
                }
            }

            Ref<Texture> bloomTexture = nullptr;
            if (postProcessing.enableBloom)
            {
                IGN_PROFILE_SCOPE("SceneRenderer::BloomPass");
                const auto &blooms = GetOrCreateBlooms(camera);
                Ref<Bloom> bloomInstance = blooms[frameContext->frameIndexInFlight];
                bloomInstance->settings.intensity = postProcessing.bloomIntensity;
                bloomInstance->settings.knee = postProcessing.bloomKnee;
                bloomInstance->settings.radius = postProcessing.bloomRadius;
                bloomInstance->settings.threshold = postProcessing.bloomThreshold;
                bloomInstance->settings.iterations = postProcessing.bloomIterations;
                bloomInstance->Resize(renderWidth, renderHeight);

                // Bloom reads from the resolved (single-sample) texture
                Ref<Texture> bloomSrc = (msaaActive && target->sceneResolvedRT) ? target->sceneResolvedRT->GetColorAttachment(0) : target->sceneRT->GetColorAttachment(0);
                bloomInstance->Build(cmd, bloomSrc, m_CompositeVertexBuffer);
                bloomTexture = bloomInstance->GetBloomTexture();
            }

            Ref<Texture> ssaoTexture = nullptr;
            if (postProcessing.enableSSAO && !msaaActive)
            {
                IGN_PROFILE_SCOPE_COLOR("SceneRenderer::HBAOPass", 0x404040FF);
                const auto &SSAOs = GetOrCreateSSAOs(cmd, camera);
                Ref<SSAO> ssaoInstance = SSAOs[frameContext->frameIndexInFlight];
                ssaoInstance->Resize(renderWidth, renderHeight);
                ssaoInstance->Build(cmd, target->sceneRT->GetDepthAttachment(), camera, postProcessing, m_CompositeVertexBuffer);
                ssaoTexture = ssaoInstance->GetAOTexture();
            }

            {
                IGN_PROFILE_SCOPE("SceneRenderer::CompositePass");
                RenderTarget *finalCompositeFB = targetFramebuffer ? targetFramebuffer : target->compositeRT->GetFramebuffer();
                
                if (sceneRenderSettings.enableFXAA && target->postProcessRT)
                {
                    CompositePass(cmd, camera, frameContext, target, cameraLens, postProcessing, edgeTexture, bloomTexture, ssaoTexture, msaaActive, target->postProcessRT->GetFramebuffer());
                    
                    IGN_PROFILE_SCOPE("SceneRenderer::FXAAPass");
                    FXAAPass(cmd, finalCompositeFB, target->postProcessRT->GetColorAttachment(0));
                }
                else
                {
                    CompositePass(cmd, camera, frameContext, target, cameraLens, postProcessing, edgeTexture, bloomTexture, ssaoTexture, msaaActive, finalCompositeFB);
                }
            }

            if (postProcessing.taaProperties.enable && !targetFramebuffer)
            {
                EnsureTAAHistoryRT(target, width, height);
                if (target->taaHistoryRT[frameContext->frameIndexInFlight])
                {
                    IGN_PROFILE_SCOPE("SceneRenderer::TAAHistoryCopy");
                    Ref<Texture> compositeColor = target->compositeRT->GetColorAttachment(0);
                    Ref<Texture> historyColor = target->taaHistoryRT[frameContext->frameIndexInFlight]->GetColorAttachment(0);
                    if (compositeColor && historyColor)
                    {
                        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
                        nri::TextureBarrierDesc barriers[2] = {};
                        barriers[0].texture = compositeColor->GetHandle();
                        barriers[0].before = { compositeColor->GetCurrentAccess(), compositeColor->GetCurrentLayout(), compositeColor->GetCurrentStages() };
                        if (barriers[0].before.layout == nri::Layout::UNDEFINED)
                            barriers[0].before = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                        barriers[0].after = { nri::AccessBits::COPY_SOURCE, nri::Layout::COPY_SOURCE, nri::StageBits::COPY };
                        barriers[0].mipNum = nri::REMAINING;
                        barriers[0].layerNum = nri::REMAINING;
                        barriers[0].planes = nri::PlaneBits::ALL;

                        barriers[1].texture = historyColor->GetHandle();
                        barriers[1].before = { historyColor->GetCurrentAccess(), historyColor->GetCurrentLayout(), historyColor->GetCurrentStages() };
                        if (barriers[1].before.layout == nri::Layout::UNDEFINED)
                            barriers[1].before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
                        barriers[1].after = { nri::AccessBits::COPY_DESTINATION, nri::Layout::COPY_DESTINATION, nri::StageBits::COPY };
                        barriers[1].mipNum = nri::REMAINING;
                        barriers[1].layerNum = nri::REMAINING;
                        barriers[1].planes = nri::PlaneBits::ALL;

                        nri::BarrierDesc barrierDesc = {};
                        barrierDesc.textures = barriers;
                        barrierDesc.textureNum = 2;
                        nri.CmdBarrier(*cmd, barrierDesc);

                        nri.CmdCopyTexture(*cmd, *historyColor->GetHandle(), nullptr, *compositeColor->GetHandle(), nullptr);

                        barriers[0].before = barriers[0].after;
                        barriers[0].after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                        barriers[1].before = barriers[1].after;
                        barriers[1].after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                        nri.CmdBarrier(*cmd, barrierDesc);

                        compositeColor->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
                        historyColor->SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
                    }

                    target->taaHistoryValid = true;
                    ++m_TAAFrameIndex;
                }
            }

            target->previousPlan = plan;
        }

        SubmitCommandBuffer(cmd);
    }

    void SceneRenderer::PreallocateGPUData(nri::CommandBuffer *cmd, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();

        m_EntityObjectIndexCache.clear();
        m_EntityBoneOffsetCache.clear();
        m_SocketObjectIndexCache.clear();

        // 1. Preallocate and upload Skeletal Meshes (including bone data)
        auto skeletalMeshView = m_Scene->registry->view<TransformComponent, RenderingComponent, SkeletalMeshComponent>();
        for (entt::entity e : skeletalMeshView)
        {
            const auto &[tr, rc, smc] = m_Scene->registry->get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(e);
            if (!rc.visible || smc.handle == AssetHandle(0))
                continue;

            auto skeletalMesh = ResolveAsset<SkeletalMesh>(smc.handle);
            if (!skeletalMesh)
                continue;

            glm::mat4 bones[MAX_BONES];
            FillBoneArray(bones, smc.finalBoneTransforms);
            uint32_t boneOffset = frameContext->boneAllocator.Allocate(cmd, bones, MAX_BONES);
            m_EntityBoneOffsetCache[e] = boneOffset;

            const auto &instances = skeletalMesh->GetMeshInstances();
            std::vector<uint32_t> indices;
            indices.reserve(instances.size());

            const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

            for (size_t idx = 0; idx < instances.size(); ++idx)
            {
                auto &meshInstance = instances[idx];
                auto &primitive = meshInstance->GetPrimitive();
                if (primitive && (!primitive->vertexBuffer || !primitive->indexBuffer))
                {
                    primitive->WriteBuffer(cmd);
                }

                Mesh_GPUData gpuData;
                if (idx < smc.cachedInstanceTransforms.size())
                {
                    gpuData = smc.cachedInstanceTransforms[idx];
                }
                else
                {
                    glm::mat4 meshTransform = meshInstance->global;
                    if (meshInstance->linkedJointIndex >= 0 && !smc.finalBoneTransforms.empty())
                    {
                        const size_t ji = static_cast<size_t>(meshInstance->linkedJointIndex);
                        if (ji < smc.finalBoneTransforms.size())
                        {
                            meshTransform = smc.finalBoneTransforms[ji] * meshTransform;
                        }
                    }
                    gpuData.transformation = tr.world.GetMatrix() * meshTransform;
                    gpuData.normal = smc.normalMatrix;
                }
                gpuData.objectID = objectID;
                gpuData.boneOffset = boneOffset;

                uint32_t objectIndex = frameContext->objectAllocator.Allocate(cmd, gpuData);
                indices.push_back(objectIndex);
            }
            m_EntityObjectIndexCache[e] = indices;

            // Handle socket attachments
            if (skeletalMesh->GetSkeletonHandle() != AssetHandle(0))
            {
                Ref<Skeleton> skeleton = ResolveAsset<Skeleton>(skeletalMesh->GetSkeletonHandle());
                if (skeleton)
                {
                    for (const auto &[socketName, attachedMeshHandle] : smc.socketAttachments)
                    {
                        if (attachedMeshHandle == AssetHandle(0))
                            continue;

                        auto attachedMeshAsset = ResolveAsset<Asset>(attachedMeshHandle);
                        if (!attachedMeshAsset)
                            continue;

                        glm::mat4 socketWorld = smc.GetSocketWorldTransform(tr.world.GetMatrix(), *skeleton, socketName);
                        glm::mat4 normalMatrix = glm::transpose(glm::inverse(glm::mat3(socketWorld)));

                        auto attachedStaticMesh = std::dynamic_pointer_cast<StaticMesh>(attachedMeshAsset);
                        if (attachedStaticMesh)
                        {
                            const auto &attachedInstances = attachedStaticMesh->GetMeshInstances();
                            std::vector<uint32_t> attachedIndices;
                            attachedIndices.reserve(attachedInstances.size());

                            for (size_t idx = 0; idx < attachedInstances.size(); ++idx)
                            {
                                auto &meshInstance = attachedInstances[idx];
                                auto &primitive = meshInstance->GetPrimitive();
                                if (primitive && (!primitive->vertexBuffer || !primitive->indexBuffer))
                                {
                                    primitive->WriteBuffer(cmd);
                                }

                                Mesh_GPUData gpuData;
                                if (idx < smc.cachedInstanceTransforms.size())
                                {
                                    gpuData = smc.cachedInstanceTransforms[idx];
                                }
                                else
                                {
                                    glm::mat4 meshTransform = meshInstance->global;
                                    gpuData.transformation = socketWorld * meshTransform;
                                    gpuData.normal = normalMatrix;
                                }
                                gpuData.objectID = objectID;
                                gpuData.boneOffset = 0;

                                uint32_t objectIndex = frameContext->objectAllocator.Allocate(cmd, gpuData);
                                attachedIndices.push_back(objectIndex);
                            }
                            m_SocketObjectIndexCache[std::make_pair(e, socketName)] = attachedIndices;
                        }
                        else
                        {
                            auto attachedSkeletalMesh = std::dynamic_pointer_cast<SkeletalMesh>(attachedMeshAsset);
                            if (attachedSkeletalMesh)
                            {
                                const auto &attachedInstances = attachedSkeletalMesh->GetMeshInstances();
                                std::vector<uint32_t> attachedIndices;
                                attachedIndices.reserve(attachedInstances.size());

                                for (size_t idx = 0; idx < attachedInstances.size(); ++idx)
                                {
                                    auto &meshInstance = attachedInstances[idx];
                                    auto &primitive = meshInstance->GetPrimitive();
                                    if (primitive && (!primitive->vertexBuffer || !primitive->indexBuffer))
                                    {
                                        primitive->WriteBuffer(cmd);
                                    }

                                    Mesh_GPUData gpuData;
                                    if (idx < smc.cachedInstanceTransforms.size())
                                    {
                                        gpuData = smc.cachedInstanceTransforms[idx];
                                    }
                                    else
                                    {
                                        glm::mat4 meshTransform = meshInstance->global;
                                        gpuData.transformation = socketWorld * meshTransform;
                                        gpuData.normal = normalMatrix;
                                    }
                                    gpuData.objectID = objectID;
                                    gpuData.boneOffset = 0;

                                    uint32_t objectIndex = frameContext->objectAllocator.Allocate(cmd, gpuData);
                                    attachedIndices.push_back(objectIndex);
                                }
                                m_SocketObjectIndexCache[std::make_pair(e, socketName)] = attachedIndices;
                            }
                        }
                    }
                }
            }
        }

        // 2. Preallocate and upload Static Meshes
        auto staticMeshView = m_Scene->registry->view<TransformComponent, RenderingComponent, StaticMeshComponent>();
        for (entt::entity e : staticMeshView)
        {
            const auto &[tr, rc, smc] = m_Scene->registry->get<TransformComponent, RenderingComponent, StaticMeshComponent>(e);
            if (!rc.visible || smc.handle == AssetHandle(0))
                continue;

            auto staticMesh = ResolveAsset<StaticMesh>(smc.handle);
            if (!staticMesh)
                continue;

            const auto &instances = staticMesh->GetMeshInstances();
            std::vector<uint32_t> indices;
            indices.reserve(instances.size());

            const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

            for (size_t idx = 0; idx < instances.size(); ++idx)
            {
                auto &meshInstance = instances[idx];
                auto &primitive = meshInstance->GetPrimitive();
                if (primitive && (!primitive->vertexBuffer || !primitive->indexBuffer))
                {
                    primitive->WriteBuffer(cmd);
                }

                Mesh_GPUData gpuData;
                if (idx < smc.cachedInstanceTransforms.size())
                {
                    gpuData = smc.cachedInstanceTransforms[idx];
                }
                else
                {
                    glm::mat4 meshTransform = meshInstance->global;
                    gpuData.transformation = tr.world.GetMatrix() * meshTransform;
                    gpuData.normal = smc.normalMatrix;
                }
                gpuData.objectID = objectID;
                gpuData.boneOffset = 0;

                uint32_t objectIndex = frameContext->objectAllocator.Allocate(cmd, gpuData);
                indices.push_back(objectIndex);
            }
            m_EntityObjectIndexCache[e] = indices;
        }

        // 3. Preallocate and upload Terrain Meshes
        auto terrainView = m_Scene->registry->view<TransformComponent, RenderingComponent, TerrainComponent>();
        for (entt::entity e : terrainView)
        {
            auto [tr, rc, tc] = terrainView.get<TransformComponent, RenderingComponent, TerrainComponent>(e);
            if (!rc.visible)
                continue;

            if (!tc.gpuInitialized || tc.chunks.empty())
            {
                m_TerrainRenderer->RebuildMesh(cmd, tc, tr.world.GetMatrix());
            }

            const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

            std::vector<uint32_t> indices;
            indices.reserve(tc.chunks.size());

            for (size_t idx = 0; idx < tc.chunks.size(); ++idx)
            {
                Mesh_GPUData gpuData;
                gpuData.transformation = tr.world.GetMatrix();
                gpuData.normal = glm::transpose(glm::inverse(glm::mat3(tr.world.GetMatrix())));
                gpuData.objectID = objectID;
                gpuData.boneOffset = 0;

                uint32_t objectIndex = frameContext->objectAllocator.Allocate(cmd, gpuData);
                indices.push_back(objectIndex);
            }
            m_EntityObjectIndexCache[e] = indices;
        }
    }

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

        // Differentiate resizing between edit-viewport and game-viewport post-processing instances
        bool isGameCamera = false;
        if (m_Scene)
        {
            if (Entity primaryCamera = m_Scene->GetPrimaryCamera())
            {
                const auto &cc = primaryCamera.GetComponent<CameraComponent>();
                if (camera == &cc.camera)
                {
                    isGameCamera = true;
                }
            }
        }

        PostProcessing postProcessing = camera ? camera->postProcessing : PostProcessing{};
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

        // Resize Bloom and SSAO instances for the specific camera
        {
            auto it = m_Blooms.find(camera);
            if (it != m_Blooms.end())
            {
                for (auto &bloom : it->second)
                {
                    if (bloom)
                        bloom->Resize(renderWidth, renderHeight);
                }
            }
        }

        {
            auto it = m_SSAOs.find(camera);
            if (it != m_SSAOs.end())
            {
                for (auto &ssao : it->second)
                {
                    if (ssao)
                        ssao->Resize(renderWidth, renderHeight);
                }
            }
        }

        {
            // Render targets
            auto it = m_RenderTargets.find(camera);
            if (it == m_RenderTargets.end() && camera)
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
    }

    void SceneRenderer::ShadowPass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();

        // Static
        Ref<GraphicsPipeline> staticCSMPSO = GetStaticCSMPSO();

        // Animated
        Ref<GraphicsPipeline> animatedCSMPSO = GetAnimatedCSMPSO();

        auto worldEnvView = m_Scene->registry->view<WorldEnvironment>();
        for (entt::entity e : worldEnvView)
        {
            WorldEnvironment &world = worldEnvView.get<WorldEnvironment>(e);
            m_SceneGPUData.exposure = world.exposure;
            m_SceneGPUData.gamma = world.gamma;
            m_SceneGPUData.ambient = world.ambient;
            m_SceneGPUData.skyType = static_cast<int>(world.skyType);
            break;
        }

        // Collect point lights
        {
            PointLightBufferData pointLightData = {};
            int pointLightCount = 0;

            auto pointLightView = m_Scene->registry->view<TransformComponent, RenderingComponent, PointLightComponent>();
            for (entt::entity e : pointLightView)
            {
                if (pointLightCount >= MAX_POINT_LIGHTS)
                    break;

                const auto &[tr, rc, light] = pointLightView.get<TransformComponent, RenderingComponent, PointLightComponent>(e);
                if (!rc.visible || !light.enabled)
                    continue;

                PointLight_GPUData &gpu = pointLightData.lights[pointLightCount];
                gpu.positionAndRange = glm::vec4(tr.world.translation, light.range);
                gpu.color = glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);
                gpu.attenuation = glm::vec4(light.constantAttenuation, light.linearAttenuation, light.quadraticAttenuation, 0.0f);
                ++pointLightCount;
            }

            m_SceneGPUData.numPointLights = pointLightCount;
            frameContext->pointLightBuffer.SetData(&pointLightData, sizeof(pointLightData));
        }

        // Collect spot lights
        {
            SpotLightBufferData spotLightData = {};
            int spotLightCount = 0;

            auto spotLightView = m_Scene->registry->view<TransformComponent, RenderingComponent, SpotLightComponent>();
            for (entt::entity e : spotLightView)
            {
                if (spotLightCount >= MAX_SPOT_LIGHTS)
                    break;

                const auto &[tr, rc, light] = spotLightView.get<TransformComponent, RenderingComponent, SpotLightComponent>(e);
                if (!rc.visible || !light.enabled)
                    continue;

                const glm::vec3 forward = glm::normalize(tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f));

                SpotLight_GPUData &gpu = spotLightData.lights[spotLightCount];
                gpu.positionAndRange = glm::vec4(tr.world.translation, light.range);
                gpu.directionAndAngle = glm::vec4(forward, glm::cos(glm::radians(light.outerConeAngle)));
                gpu.color = glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);
                gpu.attenuation = glm::vec4(light.constantAttenuation, light.linearAttenuation, light.quadraticAttenuation,
                    glm::cos(glm::radians(light.innerConeAngle)));
                ++spotLightCount;
            }

            m_SceneGPUData.numSpotLights = spotLightCount;
            frameContext->spotLightBuffer.SetData(&spotLightData, sizeof(spotLightData));
        }

        glm::vec3 sunDirection =
        {
            glm::cos(m_SceneGPUData.sungAngles.y) * glm::sin(m_SceneGPUData.sungAngles.x),
            glm::sin(m_SceneGPUData.sungAngles.y),
            glm::cos(m_SceneGPUData.sungAngles.y) * glm::cos(m_SceneGPUData.sungAngles.x)
        };

        bool cascadeShadow = false;
        float shadowDist = 200.0f; // default if no directional light found
        auto lightView = m_Scene->registry->view<TransformComponent, DirectionalLightComponent>();
        for (entt::entity e : lightView)
        {
            const TransformComponent &tr = lightView.get<TransformComponent>(e);
            const DirectionalLightComponent &light = lightView.get<DirectionalLightComponent>(e);

            sunDirection = glm::normalize(tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f));

            cascadeShadow = light.cascadeShadow;

            auto &csmData = m_CascadedShadowMap->GetGPUData();
            csmData.shadowStrength = light.cascadeShadow ? light.shadowStrength : 0.0f;
            csmData.minBias = light.shadowMinBias;
            csmData.maxBias = light.shadowMaxBias;
            csmData.pcfRadius = light.pcfRadius;
            shadowDist = light.shadowDistance;

            m_SceneGPUData.sunColor = glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);
            m_SceneGPUData.sungAngles.x = std::atan2(sunDirection.x, sunDirection.z);
            m_SceneGPUData.sungAngles.y = std::asin(glm::clamp(sunDirection.y, -1.0f, 1.0f));
            m_SceneGPUData.sunAngularRadius = glm::radians(light.angularRadius);
            m_SceneGPUData.sunDirection = glm::vec4(sunDirection, 0.0f);

            const auto quality = static_cast<ShadowMapQuality>(light.shadowResolution);
            if (m_CascadedShadowMap->GetQuality() != quality)
            {
                m_CascadedShadowMap->Resize(quality);

                Application::SubmitToMainThread([]() { SignalBus::Emit(AssetChangeSignal{ AssetHandle(0), AssetType::Environment }); });
            }

            break;
        }

        // If no directional light has cascaded shadows enabled, write default disabled CSM buffer and return immediately
        if (!cascadeShadow)
        {
            CSM_GPUData sceneCascadeData = {};
            sceneCascadeData.cascadeIndex = -1;
            sceneCascadeData.shadowStrength = 0.0f;
            frameContext->csmBuffer.SetData(&sceneCascadeData, sizeof(sceneCascadeData));
            return;
        }
        
        {
            m_CascadedShadowMap->ComputeMatrices(camera, sunDirection, shadowDist);

            // Share cascade data with the main scene pass (cascadeIndex is unused there)
            CSM_GPUData sceneCascadeData = m_CascadedShadowMap->GetGPUData();
            sceneCascadeData.cascadeIndex = -1;

            // Write the cascade data to the frame context buffer for use in the main scene pass
            frameContext->csmBuffer.SetData(&sceneCascadeData, sizeof(sceneCascadeData));

            for (int i = 0; i < NUM_CASCADES; ++i)
            {
                IGN_PROFILE_SCOPE("Prefetch per-cascaded GPU data");

                CSM_GPUData cascadeGpuData = sceneCascadeData;
                cascadeGpuData.cascadeIndex = i;
                frameContext->csmPerCascadeBuffers[i].SetData(&cascadeGpuData, sizeof(cascadeGpuData));

                // Clear the specific array layer for this cascade
                m_CascadedShadowMap->BeginCascade(cmd, i, frameContext->frameIndexInFlight);

                RenderTarget *csmFramebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(i, frameContext->frameIndexInFlight);
                if (!csmFramebuffer)
                    break;

                Frustum cascadeFrustum(cascadeGpuData.lightViewProj[i]);

                // Static Mesh Shadows — batch instanced
                {
                    IGN_PROFILE_SCOPE("SceneRenderer::StaticMeshShadow::BatchCollect");

                    m_ShadowBatchBuilder.Clear();

                    auto staticMeshView = m_Scene->registry->view<TransformComponent, RenderingComponent, StaticMeshComponent>();
                    std::vector<entt::entity> candidateEntities;
                    std::vector<AABB> candidateAABBs;

                    for (entt::entity e : staticMeshView)
                    {
                        const auto &[tr, rc, smc] = staticMeshView.get<TransformComponent, RenderingComponent, StaticMeshComponent>(e);
                        if (!rc.visible || smc.handle == AssetHandle(0))
                            continue;

                        candidateEntities.push_back(e);
                        candidateAABBs.push_back(smc.worldAABB);
                    }

                    if (!candidateEntities.empty())
                    {
                        std::vector<uint8_t> visibility(candidateEntities.size());
                        cascadeFrustum.IsAABBVisibleBatch(candidateAABBs.data(), candidateAABBs.size(), visibility.data());

                        for (size_t cIdx = 0; cIdx < candidateEntities.size(); ++cIdx)
                        {
                            if (!visibility[cIdx])
                                continue;

                            entt::entity e = candidateEntities[cIdx];
                            StaticMeshComponent &smc = m_Scene->registry->get<StaticMeshComponent>(e);
                            auto staticMesh = ResolveAsset<StaticMesh>(smc.handle);
                            if (!staticMesh)
                                continue;

                            const auto &instances = staticMesh->GetMeshInstances();
                            for (size_t idx = 0; idx < instances.size(); ++idx)
                            {
                                auto &meshInstance = instances[idx];
                                auto &primitive = meshInstance->GetPrimitive();
                                if (!primitive->vertexBuffer || !primitive->indexBuffer)
                                {
                                    primitive->WriteBuffer(cmd);
                                }

                                auto cacheIt = m_EntityObjectIndexCache.find(e);
                                if (cacheIt == m_EntityObjectIndexCache.end() || idx >= cacheIt->second.size())
                                    continue;

                                const uint32_t objectIndex = cacheIt->second[idx];

                                BatchKey key;
                                key.vertexBuffer = *primitive->vertexBuffer;
                                key.indexBuffer = *primitive->indexBuffer;
                                key.meshBindingSet = frameContext->staticMeshCSMBindingSet[i]->GetHandle();
                                key.materialBindingSet = nullptr; // CSM draws have no material binding
                                key.pipeline = staticCSMPSO ? staticCSMPSO->GetHandle() : nullptr;

                                m_ShadowBatchBuilder.Submit(key,
                                    *primitive->vertexBuffer, *primitive->indexBuffer,
                                    frameContext->staticMeshCSMBindingSet[i],
                                    nullptr, staticCSMPSO, primitive->indexBuffer->GetCount(), objectIndex);
                            }
                        }
                    }

                    // Terrain Shadows
                    auto terrainShadowView = m_Scene->registry->view<TransformComponent, RenderingComponent, TerrainComponent>();
                    for (entt::entity e : terrainShadowView)
                    {
                        auto [tr, rc, tc] = terrainShadowView.get<TransformComponent, RenderingComponent, TerrainComponent>(e);
                        if (!rc.visible)
                            continue;

                        auto cacheIt = m_EntityObjectIndexCache.find(e);
                        if (cacheIt == m_EntityObjectIndexCache.end())
                            continue;

                        for (size_t idx = 0; idx < tc.chunks.size(); ++idx)
                        {
                            if (idx >= cacheIt->second.size())
                                break;

                            auto &chunk = tc.chunks[idx];
                            if (!chunk.primitive || !chunk.primitive->vertexBuffer || !chunk.primitive->indexBuffer)
                                continue;

                            AABB worldBounds = chunk.bounds.Transform(tr.world.GetMatrix());
                            if (!cascadeFrustum.IsAABBVisible(worldBounds))
                                continue;

                            const uint32_t objectIndex = cacheIt->second[idx];

                            BatchKey key;
                            key.vertexBuffer = *chunk.primitive->vertexBuffer;
                            key.indexBuffer = *chunk.primitive->indexBuffer;
                            key.meshBindingSet = frameContext->staticMeshCSMBindingSet[i]->GetHandle();
                            key.materialBindingSet = nullptr;
                            key.pipeline = staticCSMPSO ? staticCSMPSO->GetHandle() : nullptr;

                            m_ShadowBatchBuilder.Submit(key,
                                *chunk.primitive->vertexBuffer, *chunk.primitive->indexBuffer,
                                frameContext->staticMeshCSMBindingSet[i],
                                nullptr, staticCSMPSO, chunk.primitive->indexBuffer->GetCount(), objectIndex);
                        }
                    }

                    // Flush instanced shadow draws for this cascade
                    FlushShadowBatches(cmd, frameContext, static_cast<uint32_t>(i));
                }

                // Skeletal Mesh
                {
                    IGN_PROFILE_SCOPE("SceneRenderer::SkeletalMeshShadow");

                    auto skelMeshView = m_Scene->registry->view<TransformComponent, RenderingComponent, SkeletalMeshComponent>();
                    std::vector<entt::entity> candidateSkelEntities;
                    std::vector<AABB> candidateSkelAABBs;

                    for (entt::entity e : skelMeshView)
                    {
                        const auto &[tr, rc, smc] = skelMeshView.get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(e);
                        if (!rc.visible || smc.handle == AssetHandle(0))
                            continue;

                        candidateSkelEntities.push_back(e);
                        candidateSkelAABBs.push_back(smc.worldAABB);
                    }

                    if (!candidateSkelEntities.empty())
                    {
                        std::vector<uint8_t> visibility(candidateSkelEntities.size());
                        cascadeFrustum.IsAABBVisibleBatch(candidateSkelAABBs.data(), candidateSkelAABBs.size(), visibility.data());

                        for (size_t cIdx = 0; cIdx < candidateSkelEntities.size(); ++cIdx)
                        {
                            if (!visibility[cIdx])
                                continue;

                            entt::entity e = candidateSkelEntities[cIdx];
                            const auto &[tr, rc, smc] = skelMeshView.get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(e);
                            auto skeletalMesh = ResolveAsset<SkeletalMesh>(smc.handle);
                            if (!skeletalMesh)
                                continue;

                            const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                            DrawMeshShadow(cmd, frameContext, csmFramebuffer, skeletalMesh, tr.world.GetMatrix(), smc.normalMatrix, objectID,
                                smc.finalBoneTransforms, smc.cachedInstanceTransforms, animatedCSMPSO, i, e);

                            // --- SOCKET SYSTEM: Render attached meshes for Shadows ---
                            if (skeletalMesh && skeletalMesh->GetSkeletonHandle() != AssetHandle(0))
                            {
                                Ref<Skeleton> skeleton = ResolveAsset<Skeleton>(skeletalMesh->GetSkeletonHandle());
                                if (skeleton)
                                {
                                    for (const auto &[socketName, attachedMeshHandle] : smc.socketAttachments)
                                    {
                                        if (attachedMeshHandle == AssetHandle(0))
                                            continue;

                                        auto attachedMeshAsset = ResolveAsset<Asset>(attachedMeshHandle);
                                        if (!attachedMeshAsset)
                                            continue;

                                        glm::mat4 socketWorld = smc.GetSocketWorldTransform(tr.world.GetMatrix(), *skeleton, socketName);
                                        glm::mat4 normalMatrix = glm::transpose(glm::inverse(glm::mat3(socketWorld)));

                                        if (attachedMeshAsset->GetAssetType() == AssetType::SkeletalMesh)
                                        {
                                            auto attachedMesh = attachedMeshAsset->As<SkeletalMesh>();
                                            DrawMeshShadow(cmd, frameContext, csmFramebuffer, attachedMesh, socketWorld, normalMatrix, objectID,
                                                smc.finalBoneTransforms, std::vector<Mesh_GPUData>(), animatedCSMPSO, i, e, socketName);
                                        }
                                        else if (attachedMeshAsset->GetAssetType() == AssetType::StaticMesh)
                                        {
                                            auto attachedMesh = attachedMeshAsset->As<StaticMesh>();
                                            DrawMeshShadow(cmd, frameContext, csmFramebuffer, attachedMesh, socketWorld, normalMatrix, objectID,
                                                std::vector<glm::mat4>(), std::vector<Mesh_GPUData>(), staticCSMPSO, i, e, socketName);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void SceneRenderer::ColorPass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext, RenderTarget *framebuffer, RenderTarget *selectionFramebuffer, bool drawDebug)
    {
        IGN_PROFILE_FUNCTION();

        Frustum frustum(camera);

        std::unordered_set<Material *> uploadedMaterialsThisPass;

        if (m_RuntimeMaterial)
        {
            m_RuntimeMaterial->UpdateBindingSet(GetEnvironmentMapColorTexture(), GetCascadedShadowMapDepthTexture());
        }

        // Static
        auto staticPSO = GetStaticPSO(framebuffer, sceneRenderSettings.fillMode);
        auto staticTransparentPSO = GetStaticTransparentPSO(framebuffer, sceneRenderSettings.fillMode);

        // Animated
        auto animatedPSO = GetAnimatedPSO(framebuffer, sceneRenderSettings.fillMode);
        auto animatedTransparentPSO = GetAnimatedTransparentPSO(framebuffer, sceneRenderSettings.fillMode);

        std::vector<TransparentDrawCall> transparentDrawCalls;

        // Static Meshes — collected into the batch builder for instanced dispatch
        {
            IGN_PROFILE_SCOPE("SceneRenderer::StaticMeshColor::BatchCollect");

            m_OpaqueBatchBuilder.Clear();

            auto staticMeshView = m_Scene->registry->view<TransformComponent, RenderingComponent, StaticMeshComponent>();
            std::vector<entt::entity> candidateStaticEntities;
            std::vector<AABB> candidateStaticAABBs;

            for (entt::entity e : staticMeshView)
            {
                const auto &[tr, rc, smc] = staticMeshView.get<TransformComponent, RenderingComponent, StaticMeshComponent>(e);
                if (!rc.visible || smc.handle == AssetHandle(0))
                    continue;

                candidateStaticEntities.push_back(e);
                candidateStaticAABBs.push_back(smc.worldAABB);
            }

            if (!candidateStaticEntities.empty())
            {
                std::vector<uint8_t> visibility(candidateStaticEntities.size());
                frustum.IsAABBVisibleBatch(candidateStaticAABBs.data(), candidateStaticAABBs.size(), visibility.data());

                for (size_t cIdx = 0; cIdx < candidateStaticEntities.size(); ++cIdx)
                {
                    if (!visibility[cIdx])
                        continue;

                    entt::entity e = candidateStaticEntities[cIdx];
                    const auto &[tr, rc, smc] = staticMeshView.get<TransformComponent, RenderingComponent, StaticMeshComponent>(e);
                    auto staticMesh = ResolveAsset<StaticMesh>(smc.handle);
                    if (!staticMesh)
                        continue;

                    const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

                    // For each sub-mesh primitive, look up the pre-allocated object index and submit to batch builder
                    const auto &instances = staticMesh->GetMeshInstances();
                    for (size_t idx = 0; idx < instances.size(); ++idx)
                    {
                        auto &meshInstance = instances[idx];
                        auto &primitive = meshInstance->GetPrimitive();
                        if (!primitive->vertexBuffer || !primitive->indexBuffer)
                        {
                            primitive->WriteBuffer(cmd);
                        }

                        // Look up pre-allocated object index from PreallocateGPUData
                        auto cacheIt = m_EntityObjectIndexCache.find(e);
                        if (cacheIt == m_EntityObjectIndexCache.end() || idx >= cacheIt->second.size())
                            continue;

                        const uint32_t objectIndex = cacheIt->second[idx];

                        Ref<Material> material = ResolveMeshMaterial(static_cast<int>(idx), smc.overrideMaterials, meshInstance->GetMaterialAssetHandle());
                        if (material)
                        {
                            if (uploadedMaterialsThisPass.insert(material.get()).second)
                                material->UploadToGpu(cmd);
                        }
                        else
                        {
                            if (uploadedMaterialsThisPass.insert(m_RuntimeMaterial.get()).second)
                                m_RuntimeMaterial->UploadToGpu(cmd);
                        }

                        const Ref<BindingSet> materialBindingSet = (material && material->GetBindingSet())
                            ? material->GetBindingSet()
                            : m_RuntimeMaterial->GetBindingSet();

                        if (!materialBindingSet)
                            continue;

                        MaterialType matType = material ? material->GetType() : m_RuntimeMaterial->GetType();
                        nri::CullMode matCullMode = material ? material->GetCullMode() : nri::CullMode::FRONT;

                        if (matType == MaterialType::Transparent)
                        {
                            TransparentDrawCall dc;
                            dc.meshBindingSet = frameContext->staticMeshBindingSet;
                            dc.materialBindingSet = materialBindingSet;
                            dc.vertexBuffer = primitive->vertexBuffer->GetHandle();
                            dc.indexBuffer = primitive->indexBuffer->GetHandle();
                            dc.indexCount = primitive->indexBuffer->GetCount();
                            dc.pushConstants_ObjectIndex = objectIndex;

                            AABB localAABB = meshInstance->localAABB;
                            AABB worldAABB = localAABB.Transform(tr.world.GetMatrix());
                            glm::vec3 worldCenter = worldAABB.GetCenter();
                            dc.distanceToCamera = glm::dot(worldCenter - camera->position, camera->GetForwardDirection());

                            dc.isSkeletal = false;
                            dc.gpuData.transformation = tr.world.GetMatrix();
                            dc.gpuData.normal = smc.normalMatrix;
                            dc.gpuData.objectID = objectID;
                            dc.meshInstance = meshInstance;
                            dc.cullMode = matCullMode;
                            transparentDrawCalls.push_back(dc);
                            continue;
                        }

                        auto meshPSO = GetStaticPSO(framebuffer, sceneRenderSettings.fillMode, matCullMode);

                        BatchKey key;
                        key.vertexBuffer = *primitive->vertexBuffer;
                        key.indexBuffer = *primitive->indexBuffer;
                        key.meshBindingSet = frameContext->staticMeshBindingSet->GetHandle();
                        key.materialBindingSet = materialBindingSet->GetHandle();
                        key.pipeline = meshPSO ? meshPSO->GetHandle() : nullptr;

                        m_OpaqueBatchBuilder.Submit(
                            key, *primitive->vertexBuffer, *primitive->indexBuffer,
                            frameContext->staticMeshBindingSet,
                            materialBindingSet, meshPSO,
                            primitive->indexBuffer->GetCount(), objectIndex);
                    }
                    Renderer::Stats.staticMeshCount++;
                }
            }

            // Terrain Meshes
            {
                auto terrainView = m_Scene->registry->view<TransformComponent, RenderingComponent, TerrainComponent>();
                for (entt::entity e : terrainView)
                {
                    auto [tr, rc, tc] = terrainView.get<TransformComponent, RenderingComponent, TerrainComponent>(e);
                    if (!rc.visible)
                        continue;

                    if (!tc.gpuInitialized || tc.chunks.empty())
                    {
                        m_TerrainRenderer->RebuildMesh(cmd, tc, tr.world.GetMatrix());
                    }

                    auto cacheIt = m_EntityObjectIndexCache.find(e);
                    if (cacheIt == m_EntityObjectIndexCache.end())
                        continue;

                    Ref<Material> material = ResolveAsset<Material>(tc.materialHandle);
                    if (material && uploadedMaterialsThisPass.insert(material.get()).second)
                        material->UploadToGpu(cmd);

                    if (material && !material->UpdateBindingSet(GetEnvironmentMapColorTexture(), GetCascadedShadowMapDepthTexture()))
                        continue;

                    const Ref<BindingSet> materialBindingSet = (material && material->GetBindingSet())
                        ? material->GetBindingSet() : m_RuntimeMaterial->GetBindingSet();

                    if (!materialBindingSet)
                        continue;

                    for (size_t idx = 0; idx < tc.chunks.size(); ++idx)
                    {
                        if (idx >= cacheIt->second.size())
                            break;

                        auto &chunk = tc.chunks[idx];
                        if (!chunk.primitive || !chunk.primitive->vertexBuffer || !chunk.primitive->indexBuffer)
                            continue;

                        AABB worldBounds = chunk.bounds.Transform(tr.world.GetMatrix());
                        if (!frustum.IsAABBVisible(worldBounds))
                            continue;

                        const uint32_t objectIndex = cacheIt->second[idx];

                        BatchKey key;
                        key.vertexBuffer = *chunk.primitive->vertexBuffer;
                        key.indexBuffer = *chunk.primitive->indexBuffer;
                        key.meshBindingSet = frameContext->staticMeshBindingSet->GetHandle();
                        key.materialBindingSet = materialBindingSet->GetHandle();
                        key.pipeline = staticPSO ? staticPSO->GetHandle() : nullptr;

                        m_OpaqueBatchBuilder.Submit(
                            key, *chunk.primitive->vertexBuffer, *chunk.primitive->indexBuffer,
                            frameContext->staticMeshBindingSet,
                            materialBindingSet, staticPSO,
                            chunk.primitive->indexBuffer->GetCount(), objectIndex);
                    }
                }
            }

            // Flush all opaque static mesh batches (instanced draw calls)
            FlushOpaqueBatches(cmd, frameContext, framebuffer);
        }

        // Skeletal Meshes
        {
            IGN_PROFILE_SCOPE("SceneRenderer::SkeletalMeshColor");

            auto skelMeshView = m_Scene->registry->view<TransformComponent, RenderingComponent, SkeletalMeshComponent>();
            std::vector<entt::entity> candidateSkelColorEntities;
            std::vector<AABB> candidateSkelColorAABBs;

            for (entt::entity e : skelMeshView)
            {
                const auto &[tr, rc, smc] = skelMeshView.get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(e);
                if (!rc.visible || smc.handle == AssetHandle(0))
                    continue;

                candidateSkelColorEntities.push_back(e);
                candidateSkelColorAABBs.push_back(smc.worldAABB);
            }

            if (!candidateSkelColorEntities.empty())
            {
                std::vector<uint8_t> visibility(candidateSkelColorEntities.size());
                frustum.IsAABBVisibleBatch(candidateSkelColorAABBs.data(), candidateSkelColorAABBs.size(), visibility.data());

                for (size_t cIdx = 0; cIdx < candidateSkelColorEntities.size(); ++cIdx)
                {
                    if (!visibility[cIdx])
                        continue;

                    entt::entity e = candidateSkelColorEntities[cIdx];
                    const auto &[tr, rc, smc] = skelMeshView.get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(e);
                    auto skeletalMesh = ResolveAsset<SkeletalMesh>(smc.handle);
                    if (!skeletalMesh)
                        continue;

                    const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                    DrawMesh(cmd, frameContext, framebuffer, skeletalMesh, tr.world.GetMatrix(), smc.normalMatrix, objectID,
                        smc.overrideMaterials, smc.finalBoneTransforms, smc.cachedInstanceTransforms,
                        camera, animatedPSO, transparentDrawCalls, uploadedMaterialsThisPass, e);
                    Renderer::Stats.skeletalMeshCount++;

                    // --- SOCKET SYSTEM: Render attached meshes ---
                    if (skeletalMesh && skeletalMesh->GetSkeletonHandle() != AssetHandle(0))
                    {
                        Ref<Skeleton> skeleton = ResolveAsset<Skeleton>(skeletalMesh->GetSkeletonHandle());
                        if (skeleton)
                        {
                            for (const auto &[socketName, attachedMeshHandle] : smc.socketAttachments)
                            {
                                if (attachedMeshHandle == AssetHandle(0))
                                    continue;

                                auto attachedMeshAsset = ResolveAsset<Asset>(attachedMeshHandle);
                                if (!attachedMeshAsset)
                                    continue;

                                glm::mat4 socketWorld = smc.GetSocketWorldTransform(tr.world.GetMatrix(), *skeleton, socketName);
                                glm::mat4 normalMatrix = glm::transpose(glm::inverse(glm::mat3(socketWorld)));

                                if (attachedMeshAsset->GetAssetType() == AssetType::SkeletalMesh)
                                {
                                    auto attachedMesh = attachedMeshAsset->As<SkeletalMesh>();
                                    DrawMesh(cmd, frameContext, framebuffer, attachedMesh, socketWorld, normalMatrix, objectID,
                                        std::unordered_map<int, AssetHandle>(), smc.finalBoneTransforms, std::vector<Mesh_GPUData>(),
                                        camera, animatedPSO, transparentDrawCalls, uploadedMaterialsThisPass, e, socketName);
                                }
                                else if (attachedMeshAsset->GetAssetType() == AssetType::StaticMesh)
                                {
                                    auto attachedMesh = attachedMeshAsset->As<StaticMesh>();
                                    DrawMesh(cmd, frameContext, framebuffer, attachedMesh, socketWorld, normalMatrix, objectID,
                                        std::unordered_map<int, AssetHandle>(), std::vector<glm::mat4>(), std::vector<Mesh_GPUData>(),
                                        camera, staticPSO, transparentDrawCalls, uploadedMaterialsThisPass, e, socketName);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Draw debug grid directly into scene framebuffer before transparent meshes
        if (drawDebug)
        {
            const auto is2D = camera->projectionType == ProjectionType::Orthographic;
            DrawDebugGrid(cmd, framebuffer, frameContext, is2D ? sceneRenderSettings.worldGrid2D : sceneRenderSettings.worldGrid3D, is2D);
        }

        // Transparent sub-pass: sorted back-to-front, alpha blending, no depth write
        if (!transparentDrawCalls.empty())
        {
            IGN_PROFILE_SCOPE("SceneRenderer::TransparentMeshes");

            // Sort back-to-front (farthest first)
            std::ranges::sort(transparentDrawCalls, [](const TransparentDrawCall &a, const TransparentDrawCall &b) {
                if (a.distanceToCamera != b.distanceToCamera)
                    return a.distanceToCamera > b.distanceToCamera;
                return a.pushConstants_ObjectIndex < b.pushConstants_ObjectIndex;
            });

            const bool startedRendering = framebuffer && !framebuffer->IsRendering();
            if (startedRendering)
                framebuffer->BeginRendering(cmd, false, false);

            const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
            for (const auto &dc : transparentDrawCalls)
            {
                auto pipeline = dc.isSkeletal
                    ? GetAnimatedTransparentPSO(framebuffer, sceneRenderSettings.fillMode, dc.cullMode)
                    : GetStaticTransparentPSO(framebuffer, sceneRenderSettings.fillMode, dc.cullMode);
                if (!pipeline || !pipeline->GetHandle() || !dc.meshBindingSet || !dc.materialBindingSet)
                    continue;

                nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *pipeline->GetPipelineLayout());
                nri.CmdSetPipeline(*cmd, *pipeline->GetHandle());

                nri::SetDescriptorSetDesc set0 = { 0, dc.meshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set1 = { 1, dc.materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
                nri.CmdSetDescriptorSet(*cmd, set0);
                nri.CmdSetDescriptorSet(*cmd, set1);
                nri.CmdSetDescriptorSet(*cmd, set2);

                const uint32_t vertexStride = dc.isSkeletal ? static_cast<uint32_t>(sizeof(VertexMeshAnim)) : static_cast<uint32_t>(sizeof(VertexMeshStatic));
                nri::VertexBufferDesc vbDesc = {};
                vbDesc.buffer = dc.vertexBuffer;
                vbDesc.offset = 0;
                vbDesc.stride = vertexStride;
                nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                nri.CmdSetIndexBuffer(*cmd, *dc.indexBuffer, 0, nri::IndexType::UINT32);

                uint32_t rootConstantVal = dc.pushConstants_ObjectIndex;
                if (!dc.isSkeletal)
                {
                    rootConstantVal = frameContext->instanceIndexAllocator.Allocate(cmd, &dc.pushConstants_ObjectIndex, 1);
                }

                nri::SetRootConstantsDesc rootConstDesc = {};
                rootConstDesc.rootConstantIndex = 0;
                rootConstDesc.data = &rootConstantVal;
                rootConstDesc.size = sizeof(uint32_t);
                rootConstDesc.offset = 0;
                rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
                nri.CmdSetRootConstants(*cmd, rootConstDesc);

                nri::DrawIndexedDesc drawDesc = {};
                drawDesc.indexNum = dc.indexCount;
                drawDesc.instanceNum = 1;
                nri.CmdDrawIndexed(*cmd, drawDesc);

                Renderer::Stats.drawCallCount++;
                Renderer::Stats.indexCount3D += dc.indexCount;
            }

            if (startedRendering)
                framebuffer->EndRendering(cmd);
        }

        // Particle Systems Pass (Smoke, Fire, Sparks, etc.)
        {
            IGN_PROFILE_SCOPE("SceneRenderer::ColorPass::ParticleSystems");
            const float dt = Application::GetDeltaTime();
            auto particleView = m_Scene->registry->view<TransformComponent, RenderingComponent, ParticleSystemComponent>();
            for (entt::entity e : particleView)
            {
                auto [tr, rc, pc] = m_Scene->registry->get<TransformComponent, RenderingComponent, ParticleSystemComponent>(e);
                if (!rc.visible || !pc.isPlaying)
                    continue;

                if (!pc.system)
                {
                    pc.system = ParticleSystem::Create(pc.settings.maxParticles);
                    pc.system->Reset(cmd);
                }

                const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                pc.system->Simulate(cmd, dt, tr.world.translation, pc.settings);
                pc.system->Render(cmd, camera, framebuffer, pc.settings, objectID);
            }
        }

        // 2D Pass
        {
            std::vector<PointLight2D_GPUData> pointLights2D;
            {
                IGN_PROFILE_SCOPE("SceneRenderer::2DPass::PointLightsView");

                auto pointLight2DView = m_Scene->registry->view<TransformComponent, RenderingComponent, PointLight2DComponent>();
                for (entt::entity e : pointLight2DView)
                {
                    const auto &[tr, rc, light] = m_Scene->registry->get<TransformComponent, RenderingComponent, PointLight2DComponent>(e);

                    if (!rc.visible || !light.enabled)
                        continue;

                    PointLight2D_GPUData gpuLight;
                    gpuLight.position = glm::vec4(tr.world.translation, 1.0f);
                    gpuLight.color = light.color;
                    gpuLight.radius = light.radius;
                    gpuLight.intensity = light.intensity;
                    pointLights2D.push_back(gpuLight);
                }
            }

            m_Renderer2D->SetPointLights2D(pointLights2D);
            m_Renderer2D->Begin(cmd);
            {
                IGN_PROFILE_SCOPE("SceneRenderer::2DPass::Circle2DView");

                auto circle2DView = m_Scene->registry->view<TransformComponent, RenderingComponent, Circle2DComponent>();
                for (entt::entity e : circle2DView)
                {
                    const auto &[tr, rc, circle] = m_Scene->registry->get<TransformComponent, RenderingComponent, Circle2DComponent>(e);
                    if (!rc.visible)
                        continue;

                    const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                    m_Renderer2D->DrawCircle(tr.world.GetMatrix(), circle.color, circle.thickness, circle.fade, objectID);
                }
            }
            {
                IGN_PROFILE_SCOPE("SceneRenderer::2DPass::Quad2DView");
                auto quad2DView = m_Scene->registry->view<TransformComponent, RenderingComponent, Sprite2DComponent>();
                for (entt::entity e : quad2DView)
                {
                    const auto &[tr, rc, sprite] = m_Scene->registry->get<TransformComponent, RenderingComponent, Sprite2DComponent>(e);
                    if (!rc.visible)
                        continue;

                    const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

                    // Copy UV
                    glm::vec2 uv0 = sprite.uv0;
                    glm::vec2 uv1 = sprite.uv1;

                    if (sprite.flipY)
                    {
                        std::swap(uv0.y, uv1.y);
                    }

                    if (sprite.flipX)
                    {
                        std::swap(uv0.x, uv1.x);
                    }

                    // Find material if available (use frame cache — avoids repeated AssetManager map lookups)
                    Ref<Material2D> mat2d = ResolveAsset<Material2D>(sprite.materialHandle);

                    // Render with Material2D
                    if (mat2d)
                    {
                        Ref<Texture> texture = ResolveAsset<Texture>(mat2d->textureHandle);
                        m_Renderer2D->DrawQuad(tr.world.GetMatrix(), mat2d->data.baseColor,
                            mat2d->data.additiveColor, mat2d->data.type, texture, uv0, uv1,
                            mat2d->data.tilingFactor, objectID);
                    }
                    else // Render with default
                    {
                        Ref<Texture> texture = ResolveAsset<Texture>(sprite.handle);
                        m_Renderer2D->DrawQuad(tr.world.GetMatrix(), sprite.color,
                            texture, uv0, uv1, sprite.tilingFactor, objectID);
                    }

                }
            }

            {
                IGN_PROFILE_SCOPE("SceneRenderer::2DPass::TextView");

                auto textView = m_Scene->registry->view<TransformComponent, RenderingComponent, TextComponent>();
                for (entt::entity e : textView)
                {
                    const auto &[tr, rc, text] = m_Scene->registry->get<TransformComponent, RenderingComponent, TextComponent>(e);
                    if (!rc.visible || text.fontHandle == AssetHandle(0) || text.text.empty())
                        continue;

                    const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

                    auto font = ResolveAsset<Font>(text.fontHandle);
                    if (!font)
                        continue;

                    Ref<Texture> fontAtlas = font->GetAtlasTexture();
                    if (!fontAtlas || !fontAtlas->IsReady())
                        continue;

                    glm::vec4 textColor = text.color;
                    if (text.material2dHandle != AssetHandle(0))
                    {
                        if (auto material2d = ResolveAsset<Material2D>(text.material2dHandle))
                        {
                            textColor = text.color + material2d->data.baseColor;
                        }
                    }

                    m_Renderer2D->DrawString(text.text, font, textColor, tr.world.GetMatrix(), text.kerning, text.lineSpacing, objectID);
                }
            }

            m_Renderer2D->Flush(framebuffer, frameContext->cameraBuffer);
            // m_Renderer2D->BuildPreRenderCache();
            m_Has2DPreRenderCache = true;
            m_Renderer2D->End();
        }

        if (!m_SelectedEntities.empty() && selectionFramebuffer)
        {
            RenderSelectedEntitiesIDOverlay(cmd, camera, frameContext, selectionFramebuffer, uploadedMaterialsThisPass);
        }
    }

    void SceneRenderer::UIPass(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();
        (void)frameContext;

        if (!m_Scene || !framebuffer)
            return;

        // Set root widget
        auto rootWidget = m_Scene->GetRootWidget();
        if (!rootWidget)
            return;

        m_WidgetRenderer->SetActiveWidget(rootWidget);

        const uint32_t width = std::max(1u, framebuffer->GetWidth());
        const uint32_t height = std::max(1u, framebuffer->GetHeight());
        if (m_WidgetRenderer->GetWidth() != width || m_WidgetRenderer->GetHeight() != height)
        {
            m_WidgetRenderer->Resize(width, height);
        }

        uint32_t mouseX = 0;
        uint32_t mouseY = 0;
        bool isHovered = false;
        bool useOverride = false;

        // Check per-camera input map first
        auto it = m_CameraInputStates.find(camera);
        if (it != m_CameraInputStates.end() && it->second.useOverride)
        {
            mouseX = it->second.mouseX;
            mouseY = it->second.mouseY;
            isHovered = it->second.hovered;
            useOverride = true;
        }
        else
        {
            // Fallback to legacy Editor/Gameplay mouse state
            bool isGameCamera = false;
            if (m_Scene)
            {
                if (Entity primaryCamera = m_Scene->GetPrimaryCamera())
                {
                    const auto &cc = primaryCamera.GetComponent<CameraComponent>();
                    if (camera == &cc.camera)
                    {
                        isGameCamera = true;
                    }
                }
            }

            useOverride = isGameCamera ? m_UseGameplayWidgetMouseOverride : m_UseEditorWidgetMouseOverride;
            if (useOverride)
            {
                mouseX = isGameCamera ? m_GameplayWidgetMouseX : m_EditorWidgetMouseX;
                mouseY = isGameCamera ? m_GameplayWidgetMouseY : m_EditorWidgetMouseY;
                isHovered = isGameCamera ? m_GameplayWidgetMouseHovered : m_EditorWidgetMouseHovered;
            }
        }

        if (useOverride)
        {
            if (isHovered)
            {
                m_WidgetRenderer->SetMousePosition(mouseX, mouseY);
            }
            else
            {
                const uint32_t offscreen = std::numeric_limits<uint32_t>::max() / 2u;
                m_WidgetRenderer->SetMousePosition(offscreen, offscreen);
            }
        }
        else
        {
            const glm::ivec2 mousePos = InputSystem::GetMousePosition();
            m_WidgetRenderer->SetMousePosition(
                static_cast<uint32_t>(std::max(mousePos.x, 0)),
                static_cast<uint32_t>(std::max(mousePos.y, 0)));
        }

        m_WidgetRenderer->Update(0.0f);
        m_WidgetRenderer->Render(cmd, framebuffer);
    }

    void SceneRenderer::DebugPass(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext)
    {
        DrawDebug2D(cmd, framebuffer, frameContext);
        DrawDebug3D(cmd, camera, framebuffer, frameContext);
    }

    void SceneRenderer::SetCameraWidgetMousePosition(ICamera *camera, uint32_t mouseX, uint32_t mouseY, bool hovered)
    {
        if (!camera)
            return;

        auto &state = m_CameraInputStates[camera];
        state.mouseX = mouseX;
        state.mouseY = mouseY;
        state.hovered = hovered;
        state.useOverride = true;
    }

    void SceneRenderer::SetEditorWidgetMousePosition(uint32_t mouseX, uint32_t mouseY, bool hovered)
    {
        m_EditorWidgetMouseX = mouseX;
        m_EditorWidgetMouseY = mouseY;
        m_EditorWidgetMouseHovered = hovered;
        m_UseEditorWidgetMouseOverride = true;
    }

    void SceneRenderer::SetGameplayWidgetMousePosition(uint32_t mouseX, uint32_t mouseY, bool hovered)
    {
        m_GameplayWidgetMouseX = mouseX;
        m_GameplayWidgetMouseY = mouseY;
        m_GameplayWidgetMouseHovered = hovered;
        m_UseGameplayWidgetMouseOverride = true;
    }

    void SceneRenderer::DrawDebugGrid(nri::CommandBuffer *cmd, RenderTarget *framebuffer, FrameContext *frameContext, const DebugGridStyle &style, bool is2D)
    {
        IGN_PROFILE_FUNCTION();

        if (!framebuffer)
            return;

        if (!style.enabled)
            return;

        DebugGrid_GPUData gpuData;
        gpuData.thinColor = style.thinColor;
        gpuData.thickColor = style.thickColor;
        gpuData.xAxisColor = style.xAxisColor;
        gpuData.yAxisColor = style.yAxisColor;
        gpuData.zAxisColor = style.zAxisColor;
        gpuData.settings0 = glm::vec4(glm::max(style.cellSize, 0.0001f), glm::max(style.minPixelsBetweenCells, 0.1f), glm::max(style.gridSize, 1.0f), glm::max(style.majorLineScale, 1.0f));
        gpuData.settings1 = glm::vec4(is2D ? 1.0f : 0.0f, style.enableXAxis ? 1.0f : 0.0f, style.enableYAxis ? 1.0f : 0.0f, style.enableZAxis ? 1.0f : 0.0f);

        // Set buffer data before bind it
        m_DebugGridBuffer.SetData(&gpuData, sizeof(gpuData));

        // Bind buffer
        Ref<GraphicsPipeline> gridPipeline = GetDebugGridPSO(framebuffer);
        if (!gridPipeline || !gridPipeline->GetHandle())
            return;

        Ref<BindingSet> bindingSet = GetOrCreateDebugGridBindingSet(gridPipeline->GetPipelineLayout(),
            frameContext->cameraBuffer, m_DebugGridBuffer);
        if (!bindingSet || !bindingSet->GetHandle())
            return;

        const bool startedRendering = !framebuffer->IsRendering();
        if (startedRendering)
            framebuffer->BeginRendering(cmd, false, false);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *gridPipeline->GetPipelineLayout());
        nri.CmdSetPipeline(*cmd, *gridPipeline->GetHandle());

        nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        nri::DrawDesc drawDesc = {};
        drawDesc.instanceNum = 1;
        drawDesc.vertexNum = 6;
        nri.CmdDraw(*cmd, drawDesc);

        if (startedRendering)
            framebuffer->EndRendering(cmd);
    }

    void SceneRenderer::DrawDebug2D(nri::CommandBuffer *cmd, RenderTarget *framebuffer, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();

        if (!framebuffer)
            return;

        m_Renderer2D->Begin(cmd);

        // 2D Physics debug draw
        const glm::vec4 kPhysicsDebugColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
        constexpr int kCircleDebugSegments = 24;
        constexpr float kTwoPi = 6.28318530718f;

        if (sceneRenderSettings.showPhysicsCollider)
        {
            auto boxCollider2DView = m_Scene->registry->view<TransformComponent, RenderingComponent, BoxCollider2DComponent>();
            for (entt::entity e : boxCollider2DView)
            {
                const auto &[tr, rc, box] = m_Scene->registry->get<TransformComponent, RenderingComponent, BoxCollider2DComponent>(e);

                if (!rc.visible)
                    continue;

                const glm::mat4 world = tr.world.GetMatrix();

                const glm::vec2 halfExtents = box.size;
                const glm::vec2 offset = box.offset;

                const glm::vec4 c0 = world * glm::vec4(offset.x - halfExtents.x, offset.y - halfExtents.y, 0.0f, 1.0f);
                const glm::vec4 c1 = world * glm::vec4(offset.x + halfExtents.x, offset.y - halfExtents.y, 0.0f, 1.0f);
                const glm::vec4 c2 = world * glm::vec4(offset.x + halfExtents.x, offset.y + halfExtents.y, 0.0f, 1.0f);
                const glm::vec4 c3 = world * glm::vec4(offset.x - halfExtents.x, offset.y + halfExtents.y, 0.0f, 1.0f);

                m_Renderer2D->DrawLine(glm::vec3(c0), glm::vec3(c1), kPhysicsDebugColor);
                m_Renderer2D->DrawLine(glm::vec3(c1), glm::vec3(c2), kPhysicsDebugColor);
                m_Renderer2D->DrawLine(glm::vec3(c2), glm::vec3(c3), kPhysicsDebugColor);
                m_Renderer2D->DrawLine(glm::vec3(c3), glm::vec3(c0), kPhysicsDebugColor);
            }

            auto circleCollider2DView = m_Scene->registry->view<TransformComponent, RenderingComponent, CircleCollider2DComponent>();
            for (entt::entity e : circleCollider2DView)
            {
                const auto &[tr, rc, circle] = m_Scene->registry->get<TransformComponent, RenderingComponent, CircleCollider2DComponent>(e);

                if (!rc.visible)
                    continue;

                const glm::mat4 world = tr.world.GetMatrix();
                const glm::vec3 centerWorld = glm::vec3(world * glm::vec4(circle.center, 0.0f, 1.0f));

                const float worldScaleX = glm::length(glm::vec2(world[0]));
                const float worldScaleY = glm::length(glm::vec2(world[1]));
                const float scaledRadius = circle.radius * glm::max(worldScaleX, worldScaleY);

                for (int i = 0; i < kCircleDebugSegments; ++i)
                {
                    const float t0 = (static_cast<float>(i) / static_cast<float>(kCircleDebugSegments)) * kTwoPi;
                    const float t1 = (static_cast<float>(i + 1) / static_cast<float>(kCircleDebugSegments)) * kTwoPi;

                    const glm::vec3 p0 = centerWorld + glm::vec3(std::cos(t0) * scaledRadius, std::sin(t0) * scaledRadius, 0.0f);
                    const glm::vec3 p1 = centerWorld + glm::vec3(std::cos(t1) * scaledRadius, std::sin(t1) * scaledRadius, 0.0f);

                    m_Renderer2D->DrawLine(p0, p1, kPhysicsDebugColor);
                }
            }
        }
        m_Renderer2D->Flush(framebuffer, frameContext->cameraBuffer);
        m_Renderer2D->End();
    }

    void SceneRenderer::DrawDebug3D(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();

        if (!framebuffer || !m_Scene || !m_Scene->registry)
            return;

        const glm::vec4 kPhysicsDebugColor = glm::vec4(0.5f, 1.0f, 1.0f, 1.0f);
        constexpr int kCircleSegments = 24;
        constexpr float kTwoPi = 6.28318530718f;
        constexpr float kPi = 3.14159265359f;

        auto DrawCircleRing = [this, kTwoPi](const glm::vec3 &center, const glm::vec3 &axisA, const glm::vec3 &axisB, int segments, const glm::vec4 &color)
            {
                for (int i = 0; i < segments; ++i)
                {
                    const float t0 = (static_cast<float>(i) / static_cast<float>(segments)) * kTwoPi;
                    const float t1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * kTwoPi;

                    const glm::vec3 p0 = center + axisA * std::cos(t0) + axisB * std::sin(t0);
                    const glm::vec3 p1 = center + axisA * std::cos(t1) + axisB * std::sin(t1);
                    m_Renderer2D->DrawLine(p0, p1, color);
                }
            };

        auto DrawArc = [this, kPi](const glm::vec3 &center, const glm::vec3 &axisA, const glm::vec3 &axisB, int segments, const glm::vec4 &color)
            {
                for (int i = 0; i < segments; ++i)
                {
                    const float t0 = (static_cast<float>(i) / static_cast<float>(segments)) * kPi;
                    const float t1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * kPi;

                    const glm::vec3 p0 = center + axisA * std::cos(t0) + axisB * std::sin(t0);
                    const glm::vec3 p1 = center + axisA * std::cos(t1) + axisB * std::sin(t1);
                    m_Renderer2D->DrawLine(p0, p1, color);
                }
            };

        const glm::vec4 colliderColor(0.2f, 0.9f, 0.2f, 1.0f);
        const glm::vec4 boundsColor(0.7f, 0.2f, 0.8f, 1.0f);

        if (sceneRenderSettings.showBoundingBox || sceneRenderSettings.showPhysicsCollider)
        {
            m_Renderer2D->Begin(cmd);
        }

        if (sceneRenderSettings.showBoundingBox)
        {
            // Draw bounding boxes
            for (entt::entity e : m_Scene->registry->view<TransformComponent, RenderingComponent, StaticMeshComponent>())
            {
                const auto &[tr, rc, smc] = m_Scene->registry->get<TransformComponent, RenderingComponent, StaticMeshComponent>(e);
                if (!rc.visible || smc.handle == AssetHandle(0))
                    continue;
                m_Renderer2D->DrawAABB(smc.worldAABB, boundsColor);
            }

            for (entt::entity e : m_Scene->registry->view<TransformComponent, RenderingComponent, SkeletalMeshComponent>())
            {
                const auto &[tr, rc, smc] = m_Scene->registry->get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(e);
                if (!rc.visible || smc.handle == AssetHandle(0))
                    continue;
                m_Renderer2D->DrawAABB(smc.worldAABB, boundsColor);
            }

            for (entt::entity e : m_Scene->registry->view<TransformComponent, RenderingComponent, TerrainComponent>())
            {
                const auto &[tr, rc, tc] = m_Scene->registry->get<TransformComponent, RenderingComponent, TerrainComponent>(e);
                if (!rc.visible)
                    continue;

                glm::vec3 minPos = glm::vec3(std::numeric_limits<float>::max());
                glm::vec3 maxPos = glm::vec3(std::numeric_limits<float>::min());
                for (size_t idx = 0; idx < tc.chunks.size(); ++idx)
                {
                    auto &chunk = tc.chunks[idx];
                    if (!chunk.primitive || !chunk.primitive->vertexBuffer || !chunk.primitive->indexBuffer)
                        continue;

                    AABB worldBounds = chunk.bounds.Transform(tr.world.GetMatrix());
                    minPos = glm::min(minPos, worldBounds.min);
                    maxPos = glm::max(maxPos, worldBounds.max);
                    m_Renderer2D->DrawAABB(worldBounds, { 0.9f, 0.9f, 0.9, 0.5f });
                }

                m_Renderer2D->DrawAABB(AABB::FromMinMax(minPos, maxPos), boundsColor);
            }
        }

        if (sceneRenderSettings.showPhysicsCollider)
        {
            // Box Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, BoxColliderComponent>())
            {
                if (const auto *rc = m_Scene->registry->try_get<RenderingComponent>(e); rc && !rc->visible)
                    continue;

                const auto &[tr, box] = m_Scene->registry->get<TransformComponent, BoxColliderComponent>(e);
                const glm::vec3 centerWorld = tr.world.translation + tr.world.rotation * (box.center * tr.world.scale);
                const glm::vec3 sizeWorld = glm::abs(box.scale * tr.world.scale) * 2.0f;
                const glm::mat4 boxTransform = glm::translate(glm::mat4(1.0f), centerWorld)
                    * glm::toMat4(tr.world.rotation)
                    * glm::scale(glm::mat4(1.0f), sizeWorld);
                m_Renderer2D->DrawBox(boxTransform, colliderColor);
            }

            // Sphere Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, SphereColliderComponent>())
            {
                if (const auto *rc = m_Scene->registry->try_get<RenderingComponent>(e); rc && !rc->visible)
                    continue;

                const auto &[tr, sphere] = m_Scene->registry->get<TransformComponent, SphereColliderComponent>(e);
                const glm::vec3 centerWorld = tr.world.translation + tr.world.rotation * (sphere.center * tr.world.scale);
                const float maxScale = glm::compMax(glm::abs(tr.world.scale));
                const float radiusWorld = sphere.radius * maxScale;

                const glm::vec3 axisX = (tr.world.rotation * glm::vec3(1.0f, 0.0f, 0.0f)) * radiusWorld;
                const glm::vec3 axisY = (tr.world.rotation * glm::vec3(0.0f, 1.0f, 0.0f)) * radiusWorld;
                const glm::vec3 axisZ = (tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f)) * radiusWorld;

                DrawCircleRing(centerWorld, axisX, axisY, kCircleSegments, colliderColor);
                DrawCircleRing(centerWorld, axisX, axisZ, kCircleSegments, colliderColor);
                DrawCircleRing(centerWorld, axisY, axisZ, kCircleSegments, colliderColor);
            }

            // Capsule Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, CapsuleColliderComponent>())
            {
                if (const auto *rc = m_Scene->registry->try_get<RenderingComponent>(e); rc && !rc->visible)
                    continue;

                const auto &[tr, capsule] = m_Scene->registry->get<TransformComponent, CapsuleColliderComponent>(e);
                const float maxAxis = glm::compMax(glm::abs(tr.world.scale));
                const float scaledRadius = capsule.radius * maxAxis;
                const float scaledHalfHeight = glm::max(capsule.height * 0.5f - capsule.radius, 0.0f) * maxAxis;

                const glm::vec3 center = tr.world.translation + tr.world.rotation * (capsule.center * tr.world.scale);
                const glm::vec3 right = (tr.world.rotation * glm::vec3(1.0f, 0.0f, 0.0f)) * scaledRadius;
                const glm::vec3 forward = (tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f)) * scaledRadius;
                const glm::vec3 upHeight = (tr.world.rotation * glm::vec3(0.0f, 1.0f, 0.0f)) * scaledHalfHeight;
                const glm::vec3 upRadius = (tr.world.rotation * glm::vec3(0.0f, 1.0f, 0.0f)) * scaledRadius;

                const glm::vec3 topCenter = center + upHeight;
                const glm::vec3 bottomCenter = center - upHeight;

                // Draw capsule as two circle rings and connecting lines
                DrawCircleRing(topCenter, right, forward, kCircleSegments, colliderColor);
                DrawCircleRing(bottomCenter, right, forward, kCircleSegments, colliderColor);

                // Draw connecting lines between the top and bottom circles
                m_Renderer2D->DrawLine(topCenter + forward, bottomCenter + forward, colliderColor);
                m_Renderer2D->DrawLine(topCenter - forward, bottomCenter - forward, colliderColor);
                m_Renderer2D->DrawLine(topCenter + right, bottomCenter + right, colliderColor);
                m_Renderer2D->DrawLine(topCenter - right, bottomCenter - right, colliderColor);

                // Draw arcs to represent the rounded ends of the capsule
                DrawArc(topCenter, forward, upRadius, kCircleSegments / 2, colliderColor);
                DrawArc(topCenter, right, upRadius, kCircleSegments / 2, colliderColor);
                DrawArc(bottomCenter, forward, -upRadius, kCircleSegments / 2, colliderColor);
                DrawArc(bottomCenter, right, -upRadius, kCircleSegments / 2, colliderColor);
            }

            // Character Controllers
            for (entt::entity e : m_Scene->registry->view<TransformComponent, CharacterControllerComponent>())
            {
                if (const auto *rc = m_Scene->registry->try_get<RenderingComponent>(e); rc && !rc->visible)
                    continue;

                const auto &[tr, cc] = m_Scene->registry->get<TransformComponent, CharacterControllerComponent>(e);
                const float maxAxis = glm::compMax(glm::abs(tr.world.scale));
                const float scaledRadius = cc.radius * maxAxis;
                const float scaledHalfHeight = glm::max(cc.height * 0.5f - cc.radius, 0.0f) * maxAxis;

                const glm::vec3 center = tr.world.translation + tr.world.rotation * (cc.center * tr.world.scale);
                const glm::vec3 right = (tr.world.rotation * glm::vec3(1.0f, 0.0f, 0.0f)) * scaledRadius;
                const glm::vec3 forward = (tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f)) * scaledRadius;
                const glm::vec3 upHeight = (tr.world.rotation * glm::vec3(0.0f, 1.0f, 0.0f)) * scaledHalfHeight;
                const glm::vec3 upRadius = (tr.world.rotation * glm::vec3(0.0f, 1.0f, 0.0f)) * scaledRadius;

                const glm::vec3 topCenter = center + upHeight;
                const glm::vec3 bottomCenter = center - upHeight;

                // Draw capsule as two circle rings and connecting lines
                DrawCircleRing(topCenter, right, forward, kCircleSegments, colliderColor);
                DrawCircleRing(bottomCenter, right, forward, kCircleSegments, colliderColor);

                // Draw connecting lines between the top and bottom circles
                m_Renderer2D->DrawLine(topCenter + forward, bottomCenter + forward, colliderColor);
                m_Renderer2D->DrawLine(topCenter - forward, bottomCenter - forward, colliderColor);
                m_Renderer2D->DrawLine(topCenter + right, bottomCenter + right, colliderColor);
                m_Renderer2D->DrawLine(topCenter - right, bottomCenter - right, colliderColor);

                // Draw arcs to represent the rounded ends of the capsule
                DrawArc(topCenter, forward, upRadius, kCircleSegments / 2, colliderColor);
                DrawArc(topCenter, right, upRadius, kCircleSegments / 2, colliderColor);
                DrawArc(bottomCenter, forward, -upRadius, kCircleSegments / 2, colliderColor);
                DrawArc(bottomCenter, right, -upRadius, kCircleSegments / 2, colliderColor);
            }

            // Plane Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, PlaneColliderComponent>())
            {
                if (const auto *rc = m_Scene->registry->try_get<RenderingComponent>(e); rc && !rc->visible)
                    continue;

                const auto &[tr, plane] = m_Scene->registry->get<TransformComponent, PlaneColliderComponent>(e);
                const glm::vec3 centerWorld = tr.world.translation + tr.world.rotation * (plane.center * tr.world.scale);
                const glm::vec3 scaleWorld = glm::abs(plane.scale * tr.world.scale);
                const glm::mat4 planeMat = glm::translate(glm::mat4(1.0f), centerWorld) * glm::toMat4(tr.world.rotation);

                glm::vec3 p0 = glm::vec3(planeMat * glm::vec4(-0.5f * scaleWorld.x, 0.0f, -0.5f * scaleWorld.z, 1.0f));
                glm::vec3 p1 = glm::vec3(planeMat * glm::vec4(0.5f * scaleWorld.x, 0.0f, -0.5f * scaleWorld.z, 1.0f));
                glm::vec3 p2 = glm::vec3(planeMat * glm::vec4(0.5f * scaleWorld.x, 0.0f, 0.5f * scaleWorld.z, 1.0f));
                glm::vec3 p3 = glm::vec3(planeMat * glm::vec4(-0.5f * scaleWorld.x, 0.0f, 0.5f * scaleWorld.z, 1.0f));

                m_Renderer2D->DrawLine(p0, p1, colliderColor);
                m_Renderer2D->DrawLine(p1, p2, colliderColor);
                m_Renderer2D->DrawLine(p2, p3, colliderColor);
                m_Renderer2D->DrawLine(p3, p0, colliderColor);

                glm::vec3 normStart = centerWorld;
                glm::vec3 normEnd = centerWorld + (tr.world.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
                m_Renderer2D->DrawLine(normStart, normEnd, colliderColor);
            }

            // Mesh Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, MeshColliderComponent>())
            {
                if (const auto *rc = m_Scene->registry->try_get<RenderingComponent>(e); rc && !rc->visible)
                    continue;

                const auto &[tr, mc] = m_Scene->registry->get<TransformComponent, MeshColliderComponent>(e);
                const glm::vec3 centerWorld = tr.world.translation + tr.world.rotation * (mc.center * tr.world.scale);
                const glm::mat4 meshMat = glm::translate(glm::mat4(1.0f), centerWorld)
                    * glm::toMat4(tr.world.rotation)
                    * glm::scale(glm::mat4(1.0f), tr.world.scale);

                if (!mc.vertices.empty() && !mc.indices.empty())
                {
                    for (size_t i = 0; i + 2 < mc.indices.size(); i += 3)
                    {
                        glm::vec3 v0 = glm::vec3(meshMat * glm::vec4(mc.vertices[mc.indices[i]], 1.0f));
                        glm::vec3 v1 = glm::vec3(meshMat * glm::vec4(mc.vertices[mc.indices[i + 1]], 1.0f));
                        glm::vec3 v2 = glm::vec3(meshMat * glm::vec4(mc.vertices[mc.indices[i + 2]], 1.0f));

                        m_Renderer2D->DrawLine(v0, v1, colliderColor);
                        m_Renderer2D->DrawLine(v1, v2, colliderColor);
                        m_Renderer2D->DrawLine(v2, v0, colliderColor);
                    }
                }
                else
                {
                    m_Renderer2D->DrawBox(meshMat, colliderColor);
                }
            }

            // HeightField Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, RenderingComponent, HeightFieldColliderComponent>())
            {
                const auto &[tr, rc, hfc] = m_Scene->registry->get<TransformComponent, RenderingComponent, HeightFieldColliderComponent>(e);
                if (!rc.visible || hfc.sampleCount < 2 || hfc.heights.empty() || hfc.heights.size() != static_cast<size_t>(hfc.sampleCount) * static_cast<size_t>(hfc.sampleCount))
                    continue;

                const uint32_t sampleCount = hfc.sampleCount;
                const uint32_t stride = std::max(1u, sampleCount / 64u);
                const glm::mat4 worldMat = tr.world.GetMatrix();

                for (uint32_t z = 0; z < sampleCount - 1; z += stride)
                {
                    const uint32_t nextZ = std::min(z + stride, sampleCount - 1);
                    for (uint32_t x = 0; x < sampleCount - 1; x += stride)
                    {
                        const uint32_t nextX = std::min(x + stride, sampleCount - 1);

                        auto getPos = [&](uint32_t ix, uint32_t iz) -> glm::vec3
                        {
                            float h = hfc.heights[iz * sampleCount + ix];
                            glm::vec3 loc = hfc.center + glm::vec3(static_cast<float>(ix), h, static_cast<float>(iz)) * hfc.scale;
                            return glm::vec3(worldMat * glm::vec4(loc, 1.0f));
                        };

                        glm::vec3 v00 = getPos(x, z);
                        glm::vec3 v10 = getPos(nextX, z);
                        glm::vec3 v01 = getPos(x, nextZ);
                        glm::vec3 v11 = getPos(nextX, nextZ);

                        m_Renderer2D->DrawLine(v00, v10, colliderColor);
                        m_Renderer2D->DrawLine(v00, v01, colliderColor);
                        m_Renderer2D->DrawLine(v10, v01, colliderColor);
                        m_Renderer2D->DrawLine(v10, v11, colliderColor);
                        m_Renderer2D->DrawLine(v01, v11, colliderColor);
                    }
                }
            }

            // Terrain HeightField Colliders Debug Draw
            for (entt::entity e : m_Scene->registry->view<TransformComponent, RenderingComponent, TerrainComponent>())
            {
                const auto &[tr, rc, tc] = m_Scene->registry->get<TransformComponent, RenderingComponent, TerrainComponent>(e);
                if (!rc.visible || !tc.data || tc.data->heightmap.empty() || tc.data->resolution < 2)
                    continue;

                const uint32_t res = tc.data->resolution;
                const uint32_t stride = std::max(1u, res / 64u);
                const float step = tc.worldSize / static_cast<float>(res - 1);
                const float halfSize = tc.worldSize * 0.5f;
                const glm::mat4 worldMat = tr.world.GetMatrix();

                for (uint32_t z = 0; z < res - 1; z += stride)
                {
                    const uint32_t nextZ = std::min(z + stride, res - 1);
                    for (uint32_t x = 0; x < res - 1; x += stride)
                    {
                        const uint32_t nextX = std::min(x + stride, res - 1);

                        auto getPos = [&](uint32_t ix, uint32_t iz) -> glm::vec3
                        {
                            float posX = static_cast<float>(ix) * step - halfSize;
                            float posZ = static_cast<float>(iz) * step - halfSize;
                            float posY = tc.data->GetHeight(ix, iz) * tc.maxHeight;
                            return glm::vec3(worldMat * glm::vec4(posX, posY, posZ, 1.0f));
                        };

                        glm::vec3 v00 = getPos(x, z);
                        glm::vec3 v10 = getPos(nextX, z);
                        glm::vec3 v01 = getPos(x, nextZ);
                        glm::vec3 v11 = getPos(nextX, nextZ);

                        m_Renderer2D->DrawLine(v00, v10, colliderColor);
                        m_Renderer2D->DrawLine(v00, v01, colliderColor);
                        m_Renderer2D->DrawLine(v10, v01, colliderColor);
                        m_Renderer2D->DrawLine(v10, v11, colliderColor);
                        m_Renderer2D->DrawLine(v01, v11, colliderColor);
                    }
                }
            }

            // Physics Joint debug visualization
            for (const entt::entity e : m_Scene->registry->view<TransformComponent, PhysicsJointComponent>())
            {
                const auto &[trA, jointComp] = m_Scene->registry->get<TransformComponent, PhysicsJointComponent>(e);
                const auto &d = jointComp.desc;

                bool isBroken = false;
                if (auto rtJoint = jointComp.runtimeJoint.lock())
                {
                    isBroken = rtJoint->IsBroken();
                }

                const glm::vec4 jointColor = (!d.enabled || isBroken)
                    ? glm::vec4(0.95f, 0.25f, 0.25f, 1.0f)
                    : glm::vec4(1.0f, 0.65f, 0.1f, 1.0f);
                const glm::vec4 linkColor = glm::vec4(0.2f, 0.85f, 1.0f, 1.0f);
                const glm::vec4 axisColor = glm::vec4(0.95f, 0.2f, 0.85f, 1.0f);

                glm::vec3 anchorA = (d.space == physics::JointSpace::Local)
                    ? (trA.world.translation + trA.world.rotation * (trA.world.scale * d.localAnchorA))
                    : d.localAnchorA;

                glm::vec3 axisA = (d.space == physics::JointSpace::Local)
                    ? (trA.world.rotation * d.localAxisA)
                    : d.localAxisA;
                if (glm::length2(axisA) > 1e-6f)
                    axisA = glm::normalize(axisA);
                else
                    axisA = glm::vec3(1.0f, 0.0f, 0.0f);

                glm::vec3 normalA = (d.space == physics::JointSpace::Local)
                    ? (trA.world.rotation * d.localNormalA)
                    : d.localNormalA;
                normalA = normalA - glm::dot(normalA, axisA) * axisA;
                if (glm::length2(normalA) > 1e-6f)
                    normalA = glm::normalize(normalA);
                else
                    normalA = glm::abs(axisA.y) < 0.9f ? glm::normalize(glm::cross(axisA, glm::vec3(0.0f, 1.0f, 0.0f))) : glm::normalize(glm::cross(axisA, glm::vec3(1.0f, 0.0f, 0.0f)));

                // Draw marker at anchorA
                constexpr float markerSize = 0.12f;
                m_Renderer2D->DrawLine(anchorA - glm::vec3(markerSize, 0.0f, 0.0f), anchorA + glm::vec3(markerSize, 0.0f, 0.0f), jointColor);
                m_Renderer2D->DrawLine(anchorA - glm::vec3(0.0f, markerSize, 0.0f), anchorA + glm::vec3(0.0f, markerSize, 0.0f), jointColor);
                m_Renderer2D->DrawLine(anchorA - glm::vec3(0.0f, 0.0f, markerSize), anchorA + glm::vec3(0.0f, 0.0f, markerSize), jointColor);

                // Line from entity A origin to anchor A
                m_Renderer2D->DrawLine(trA.world.translation, anchorA, jointColor);

                // Connected entity visualization
                if (jointComp.connectedEntity != UUID(0))
                {
                    Entity connected = SceneManager::GetEntity(m_Scene.get(), jointComp.connectedEntity);
                    if (connected.IsValid())
                    {
                        const auto &trB = connected.GetTransform();
                        glm::vec3 anchorB = anchorA;
                        if (!d.autoConfigureConnected || d.type == physics::JointType::Distance)
                        {
                            anchorB = (d.space == physics::JointSpace::Local)
                                ? (trB.world.translation + trB.world.rotation * (trB.world.scale * d.localAnchorB))
                                : d.localAnchorB;
                        }

                        m_Renderer2D->DrawLine(anchorA, anchorB, linkColor);
                        m_Renderer2D->DrawLine(anchorB, trB.world.translation, linkColor);
                    }
                }

                // Joint-type specific gizmos
                if (d.type == physics::JointType::Hinge)
                {
                    m_Renderer2D->DrawLine(anchorA - axisA * 0.35f, anchorA + axisA * 0.35f, axisColor);
                    m_Renderer2D->DrawLine(anchorA, anchorA + normalA * 0.3f, jointColor);
                    if (d.enableLimits)
                    {
                        const glm::vec3 minDir = glm::angleAxis(glm::radians(d.limitsMin), axisA) * normalA;
                        const glm::vec3 maxDir = glm::angleAxis(glm::radians(d.limitsMax), axisA) * normalA;
                        m_Renderer2D->DrawLine(anchorA, anchorA + minDir * 0.3f, linkColor);
                        m_Renderer2D->DrawLine(anchorA, anchorA + maxDir * 0.3f, linkColor);
                    }
                }
                else if (d.type == physics::JointType::Slider)
                {
                    const float minLim = d.enableLimits ? d.limitsMin : -0.5f;
                    const float maxLim = d.enableLimits ? d.limitsMax : 0.5f;
                    const glm::vec3 pMin = anchorA + axisA * minLim;
                    const glm::vec3 pMax = anchorA + axisA * maxLim;
                    m_Renderer2D->DrawLine(pMin, pMax, axisColor);
                    m_Renderer2D->DrawLine(pMin - normalA * 0.1f, pMin + normalA * 0.1f, jointColor);
                    m_Renderer2D->DrawLine(pMax - normalA * 0.1f, pMax + normalA * 0.1f, jointColor);
                }
                else if (d.type == physics::JointType::Cone)
                {
                    const float coneLen = 0.4f;
                    const float halfAngleRad = glm::radians(glm::clamp(d.halfConeAngle, 0.0f, 180.0f));
                    const glm::vec3 biNormalA = glm::normalize(glm::cross(axisA, normalA));
                    const glm::vec3 r0 = glm::angleAxis(halfAngleRad, normalA) * axisA;
                    const glm::vec3 r1 = glm::angleAxis(-halfAngleRad, normalA) * axisA;
                    const glm::vec3 r2 = glm::angleAxis(halfAngleRad, biNormalA) * axisA;
                    const glm::vec3 r3 = glm::angleAxis(-halfAngleRad, biNormalA) * axisA;
                    m_Renderer2D->DrawLine(anchorA, anchorA + axisA * coneLen, axisColor);
                    m_Renderer2D->DrawLine(anchorA, anchorA + r0 * coneLen, jointColor);
                    m_Renderer2D->DrawLine(anchorA, anchorA + r1 * coneLen, jointColor);
                    m_Renderer2D->DrawLine(anchorA, anchorA + r2 * coneLen, jointColor);
                    m_Renderer2D->DrawLine(anchorA, anchorA + r3 * coneLen, jointColor);
                }
            }
        }

        if (sceneRenderSettings.showBoundingBox || sceneRenderSettings.showPhysicsCollider)
        {
            m_Renderer2D->Flush(framebuffer, frameContext->cameraBuffer);
            m_Renderer2D->End();
        }
    }

    Ref<CameraRenderTarget> SceneRenderer::GetRenderTarget(ICamera *camera)
    {
        auto it = m_RenderTargets.find(camera);
        if (it != m_RenderTargets.end())
            return it->second;
        return nullptr;
    }

    void SceneRenderer::CompositePass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext,
        Ref<CameraRenderTarget> target, const CameraLens &lens, const PostProcessing &postProcessing,
        Ref<Texture> edgeTexture, Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, bool msaaResolved,
        RenderTarget *targetFramebuffer)
    {
        IGN_PROFILE_FUNCTION();

        RenderTarget *compositeFramebuffer = targetFramebuffer ? targetFramebuffer : target->compositeRT->GetFramebuffer();

        // Setup Post Processing settings
        if (camera)
        {
            m_PostProcessingData.flags.x = (postProcessing.enableBloom && bloomTexture) ? 1.0f : 0.0f;
            m_PostProcessingData.flags.y = (postProcessing.enableBloom && bloomTexture) ? postProcessing.bloomIntensity : 1.0f;
            m_PostProcessingData.flags.z = postProcessing.enableVignette ? 1.0f : 0.0f;
            m_PostProcessingData.flags.w = postProcessing.enableChromAb ? 1.0f : 0.0f;
            m_PostProcessingData.tonemapMode = static_cast<int>(postProcessing.tonemapMode);
            m_PostProcessingData.vignetteParams = glm::vec4(
                postProcessing.vignetteRadius,
                glm::max(postProcessing.vignetteSoftness, 0.001f),
                postProcessing.vignetteIntensity,
                postProcessing.chromAbAmount);
            m_PostProcessingData.chromAbParams.x = postProcessing.chromAbRadial;
            m_PostProcessingData.chromAbParams.y = (postProcessing.enableSSAO && ssaoTexture) ? 1.0f : 0.0f;
            m_PostProcessingData.chromAbParams.z = postProcessing.aoIntensity;
            m_PostProcessingData.vignetteColor = glm::vec4(postProcessing.vignetteColor, 1.0f);
            const bool enableTAA = postProcessing.taaProperties.enable && target->taaHistoryValid;
            m_PostProcessingData.taaParams = glm::vec4(
                enableTAA ? 1.0f : 0.0f,
                glm::clamp(postProcessing.taaProperties.blendFactor, 0.01f, 1.0f),
                target->taaHistoryValid ? 1.0f : 0.0f, 0.0f
            );
            m_PostProcessingData.projectionInv = glm::inverse(camera->GetProjection());
            m_PostProcessingData.enableDOF = lens.enabledDOF ? 1 : 0;
            m_PostProcessingData.focalLength = lens.focalLength;
            m_PostProcessingData.focalDistance = lens.focalDistance;
            m_PostProcessingData.fStop = lens.fStop;
            m_PostProcessingData.focusRange = lens.focusRange;
            m_PostProcessingData.blurAmount = lens.blurAmount;

            if (m_WorldEnvironment)
            {
                m_PostProcessingData.exposure = m_WorldEnvironment->exposure;
                m_PostProcessingData.gamma = m_WorldEnvironment->gamma;

                m_PostProcessingData.fogColor = m_WorldEnvironment->fogColor;
                m_PostProcessingData.fogDensity = m_WorldEnvironment->fogDensity;
                m_PostProcessingData.fogStart = m_WorldEnvironment->fogStart;
                m_PostProcessingData.fogEnd = m_WorldEnvironment->fogEnd;
            }
            else
            {
                m_PostProcessingData.tonemapMode = 0; // Reinhard
                m_PostProcessingData.exposure = 1.1f;
                m_PostProcessingData.gamma = 2.2f;
                m_PostProcessingData.fogDensity = 0.0f;
            }
        }

        m_CompositePostProcessBuffer.SetData(&m_PostProcessingData, sizeof(m_PostProcessingData));

        const uint32_t previousHistoryIndex = (frameContext->frameIndexInFlight + 2u) % 3u;
        Ref<Texture> taaHistoryTexture = target->taaHistoryRT[previousHistoryIndex]
            ? target->taaHistoryRT[previousHistoryIndex]->GetColorAttachment(0)
            : Renderer::GetBlackTexture();

        Ref<GraphicsPipeline> compositePipeline = GetCompositePSO(compositeFramebuffer, nri::FillMode::SOLID);
        if (!compositePipeline || !compositePipeline->GetHandle())
            return;

        Ref<BindingSet> bindingSet = GetOrCreateCompositeBindingSet(compositePipeline->GetPipelineLayout(), target,
            edgeTexture, bloomTexture, ssaoTexture, taaHistoryTexture, m_CompositePostProcessBuffer, m_CompositeSampler, msaaResolved);
        if (!bindingSet || !bindingSet->GetHandle())
            return;

        auto isDepthFormat = [](nri::Format format) -> bool
        {
            return format == nri::Format::D16_UNORM || format == nri::Format::D32_SFLOAT ||
                   format == nri::Format::D24_UNORM_S8_UINT || format == nri::Format::D32_SFLOAT_S8_UINT;
        };

        auto hasStencil = [](nri::Format format) -> bool
        {
            return format == nri::Format::D24_UNORM_S8_UINT || format == nri::Format::D32_SFLOAT_S8_UINT;
        };

        auto transitionToSRV = [&](Ref<Texture> &tex)
        {
            if (tex && tex->GetHandle() && tex->GetCurrentLayout() != nri::Layout::SHADER_RESOURCE)
            {
                nri::TextureBarrierDesc b = {};
                b.texture = tex->GetHandle();
                b.before = { tex->GetCurrentAccess(), tex->GetCurrentLayout(), tex->GetCurrentStages() };
                b.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                b.mipOffset = 0;
                b.mipNum = nri::REMAINING;
                b.layerOffset = 0;
                b.layerNum = nri::REMAINING;
                b.planes = isDepthFormat(tex->GetFormat())
                    ? (hasStencil(tex->GetFormat()) ? (nri::PlaneBits::DEPTH | nri::PlaneBits::STENCIL) : nri::PlaneBits::DEPTH)
                    : nri::PlaneBits::ALL;

                nri::BarrierDesc bd = {};
                bd.textures = &b;
                bd.textureNum = 1;
                DeviceManager::GetInstance()->GetCoreInterface().CmdBarrier(*cmd, bd);
                tex->SetCurrentState(b.after.layout, b.after.access, b.after.stages);
            }
        };

        const bool hasResolved = msaaResolved && target->sceneResolvedRT;
        Ref<Texture> compositeDepth = (target->sceneRT && target->sceneRT->GetDepthAttachment() && !hasResolved) ? target->sceneRT->GetDepthAttachment() : nullptr;
        Ref<Texture> compositeColor = hasResolved ? (target->sceneResolvedRT ? target->sceneResolvedRT->GetColorAttachment(0) : nullptr) : (target->sceneRT ? target->sceneRT->GetColorAttachment(0) : nullptr);
        Ref<Texture> compositeObjectID = hasResolved
            ? (target->sceneResolvedRT ? target->sceneResolvedRT->GetColorAttachment(1) : nullptr)
            : (target->sceneRT ? target->sceneRT->GetColorAttachment(1) : nullptr);
        Ref<Texture> compositeDebug = (target->debugRT ? target->debugRT->GetColorAttachment(0) : nullptr);
        Ref<Texture> compositeWidget = (target->widgetRT ? target->widgetRT->GetColorAttachment(0) : nullptr);

        transitionToSRV(compositeColor);
        transitionToSRV(compositeWidget);
        transitionToSRV(edgeTexture);
        transitionToSRV(bloomTexture);
        transitionToSRV(ssaoTexture);
        transitionToSRV(compositeDepth);
        transitionToSRV(compositeDebug);
        transitionToSRV(compositeObjectID);
        transitionToSRV(taaHistoryTexture);

        const bool startedRendering = compositeFramebuffer && !compositeFramebuffer->IsRendering();
        if (startedRendering)
            compositeFramebuffer->BeginRendering(cmd, false, false);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *compositePipeline->GetPipelineLayout());
        nri.CmdSetPipeline(*cmd, *compositePipeline->GetHandle());

        nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        nri::VertexBufferDesc vbDesc = {};
        vbDesc.buffer = m_CompositeVertexBuffer->GetHandle();
        vbDesc.offset = 0;
        vbDesc.stride = sizeof(VertexScreen);
        nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);

        nri::DrawDesc drawDesc = {};
        drawDesc.instanceNum = 1;
        drawDesc.vertexNum = 6;
        nri.CmdDraw(*cmd, drawDesc);

        if (startedRendering)
            compositeFramebuffer->EndRendering(cmd);
    }

    Ref<Texture> SceneRenderer::GetCascadedShadowMapDepthTexture() const
    {
        FrameContext *frameContext = Renderer::GetCurrentFrameContext();
        return m_CascadedShadowMap ? m_CascadedShadowMap->GetDepthTexture(frameContext->frameIndexInFlight) : nullptr;
    }

    Ref<CascadedShadowMap> SceneRenderer::GetCascadedShadowMap()
    {
        return m_CascadedShadowMap;
    }

    Ref<Texture> SceneRenderer::GetEnvironmentMapColorTexture() const
    {
        if (!m_WorldEnvironment || !m_WorldEnvironment->environment)
            return nullptr;

        if (m_WorldEnvironment->skyType == SkyType::ProceduralSky)
        {
            Ref<ProceduralSky> proceduralSky = m_WorldEnvironment->environment->GetProceduralSky();
            if (proceduralSky && proceduralSky->GetSkyViewLUT())
                return proceduralSky->GetSkyViewLUT();
        }

        return m_WorldEnvironment->environment->GetHDRTexture();
    }

    void SceneRenderer::SetFillMode(nri::FillMode mode)
    {
        sceneRenderSettings.fillMode = mode;

        // Recreate pipelines

        // ANIMATED
        m_AnimatedPSOCache.clear();
        m_TransparentAnimatedPSOCache.clear();
        m_AnimatedCSMPSOCache.clear();

        // STATIC
        m_StaticPSOCache.clear();
        m_TransparentStaticPSOCache.clear();
        m_StaticCSMPSOCache.clear();

        // ENV & Debug
        m_EnvironmentPSOCache.clear();
        m_CompositePSOCache.clear();
        m_DebugGridPSOCache.clear();

        m_CompositePSOCache.clear();
        m_DebugGridPSOCache.clear();

        m_CompositeBindingSetCache.clear();
        m_DebugGridBindingSetCache.clear();

        // m_Renderer2D->SetFillMode(mode);
    }

    void SceneRenderer::SetSelectedEntity(const Entity &entity)
    {
        const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(entity.GetUUID()));
        const auto it = std::ranges::find_if(m_SelectedEntities, [&](const uint32_t id)
        {
            return id == objectID;
        });

        // push back if not found
        if (it == m_SelectedEntities.end())
        {
            m_SelectedEntities.push_back(objectID);
        }
    }

    void SceneRenderer::UnselectEntity(const Entity &entity)
    {
        const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(entity.GetUUID()));
        std::erase_if(m_SelectedEntities, [&objectID](uint32_t id)
        {
            return id == objectID;
        });
    }

    void SceneRenderer::ClearSelectedEntities()
    {
        m_SelectedEntities.clear();
    }

    // Helper to build a debug-grid pipeline per framebuffer (once)
    Ref<GraphicsPipeline> SceneRenderer::GetDebugGridPSO(RenderTarget *framebuffer)
    {
        if (!framebuffer)
            return nullptr;

        auto key = MakeFramebufferKey(framebuffer, nri::FillMode::SOLID);
        auto it = m_DebugGridPSOCache.find(key);
        if (it != m_DebugGridPSOCache.end())
        {
            return it->second;
        }

        const bool hasDepthAttachment = framebuffer->HasDepthAttachment();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.srcBlend = nri::BlendFactor::SRC_ALPHA;
        params.destBlend = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.srcBlendAlpha = nri::BlendFactor::ONE;
        params.destBlendAlpha = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.enableDepthWrite = false;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;
        params.depthFunc = nri::CompareOp::LESS_EQUAL;

        BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.SetRegisterSpace(0);
        bindingLayoutDesc.SetVisibility(nri::StageBits::ALL);
        bindingLayoutDesc.AddItem(BindingLayoutItem::VolatileConstantBuffer(0));
        bindingLayoutDesc.AddItem(BindingLayoutItem::VolatileConstantBuffer(1));

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/infinite_grid.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/infinite_grid.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        auto gp = GraphicsPipeline::Create("Infinite Grid Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(bindingLayoutDesc)
            .Build(framebuffer, params);

        m_DebugGridPSOCache.clear();
        m_DebugGridPSOCache.emplace(key, gp);
        return gp;
    }

    Ref<BindingSet> SceneRenderer::GetOrCreateDebugGridBindingSet(nri::PipelineLayout *pipelineLayout, const ConstantBuffer &cameraBuffer, const ConstantBuffer &gridBuffer)
    {
        DebugGridBindingKey key{ pipelineLayout, cameraBuffer.GetDescriptor(), gridBuffer.GetDescriptor() };
        auto it = m_DebugGridBindingSetCache.find(key);
        if (it != m_DebugGridBindingSetCache.end())
        {
            return it->second;
        }

        auto bindingSetDesc = BindingSetDesc();
        bindingSetDesc.AddItem(BindingSetItem::ConstantBuffer(0, cameraBuffer.GetDescriptor())); // volatile
        bindingSetDesc.AddItem(BindingSetItem::ConstantBuffer(1, gridBuffer.GetDescriptor())); // volatile

        Ref<BindingSet> bindingSet = BindingSet::Create("Debug Grid Binding Set", BindingSet::AllocateAndUpdate(pipelineLayout, 0, bindingSetDesc));
        LOG_ASSERT(bindingSet && bindingSet->GetHandle(), "[Debug Grid] Failed to create binding set");
        if (bindingSet && bindingSet->GetHandle())
        {
            m_DebugGridBindingSetCache.emplace(key, bindingSet);
        }

        return bindingSet;
    }

    Ref<GraphicsPipeline> SceneRenderer::GetOrCreateMeshPSO(
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
        RenderTarget *framebuffer,
        nri::FillMode fillMode,
        nri::CullMode cullMode,
        const char *vertexShaderPath,
        const char *pixelShaderPath,
        EBindingLayout meshLayout,
        bool transparent)
    {
        if (!framebuffer)
            return nullptr;

        auto key = MakeFramebufferKey(framebuffer, fillMode, cullMode);
        auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }

        const bool hasDepthAttachment = framebuffer->HasDepthAttachment();

        GraphicsPipelineParams params;
        params.enableBlend = transparent;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.cullMode = cullMode;
        params.depthFunc = nri::CompareOp::LESS_EQUAL;

        if (transparent)
        {
            params.srcBlend = nri::BlendFactor::SRC_ALPHA;
            params.destBlend = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
            params.srcBlendAlpha = nri::BlendFactor::ONE;
            params.destBlendAlpha = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
            params.enableDepthWrite = false;
        }
        else
        {
            params.enableDepthWrite = hasDepthAttachment;
        }

        Ref<Shader> vertexShader = Shader::Create(vertexShaderPath, UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create(pixelShaderPath, UMBRA_SHADER_TYPE_PIXEL, true);

        auto gp = GraphicsPipeline::Create("Mesh Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(meshLayout))
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MATERIAL))
            .AddBindingLayout(BindlessSystem::GetBindingLayout())
            .Build(framebuffer, params);

        cache.emplace(key, gp);
        return gp;
    }

    Ref<GraphicsPipeline> SceneRenderer::GetOrCreateCMSPSO(
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
        RenderTarget *framebuffer, const char *vertexShaderPath, const char *pixelShaderPath,
        EBindingLayout meshLayout)
    {
        if (!framebuffer)
            return nullptr;

        auto key = MakeFramebufferKey(framebuffer, nri::FillMode::SOLID);
        auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }

        GraphicsPipelineParams params;
        params.enableDepthWrite = true;
        params.enableDepthTest = true;
        params.depthFunc = nri::CompareOp::LESS;
        params.cullMode = nri::CullMode::FRONT;
        params.fillMode = nri::FillMode::SOLID;

        Ref<Shader> vertexShader = Shader::Create(vertexShaderPath, UMBRA_SHADER_TYPE_VERTEX, false);
        Ref<Shader> pixelShader = Shader::Create(pixelShaderPath, UMBRA_SHADER_TYPE_PIXEL, false);

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create("CSM Shadow Pipeline");
        pipeline->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(meshLayout))
            .Build(framebuffer, params);

        if (pipeline)
        {
            cache.clear();
            cache.emplace(key, pipeline);
        }

        return pipeline;
    }

    // Helper to build a geometry pipeline for a framebuffer (once) and cache it.
    Ref<GraphicsPipeline> SceneRenderer::GetAnimatedPSO(RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode)
    {
        return GetOrCreateMeshPSO(m_AnimatedPSOCache, framebuffer, fillMode, cullMode,
            "resources/shaders/mesh_anim.vertex.hlsl", "resources/shaders/mesh_anim.pixel.hlsl",
            EBindingLayout::MESH_ANIM, false);
    }

    Ref<GraphicsPipeline> SceneRenderer::GetAnimatedTransparentPSO(RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode)
    {
        return GetOrCreateMeshPSO(m_TransparentAnimatedPSOCache, framebuffer, fillMode, cullMode,
            "resources/shaders/mesh_anim.vertex.hlsl", "resources/shaders/mesh_anim.pixel.hlsl",
            EBindingLayout::MESH_ANIM, true);
    }

    Ref<GraphicsPipeline> SceneRenderer::GetStaticPSO(RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode)
    {
        return GetOrCreateMeshPSO(m_StaticPSOCache, framebuffer, fillMode, cullMode,
            "resources/shaders/mesh_static.vertex.hlsl", "resources/shaders/mesh_static.pixel.hlsl",
            EBindingLayout::MESH_STATIC, false);
    }

    Ref<GraphicsPipeline> SceneRenderer::GetStaticTransparentPSO(RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode)
    {
        return GetOrCreateMeshPSO(m_TransparentStaticPSOCache, framebuffer, fillMode, cullMode,
            "resources/shaders/mesh_static.vertex.hlsl", "resources/shaders/mesh_static.pixel.hlsl",
            EBindingLayout::MESH_STATIC, true);
    }

    Ref<GraphicsPipeline> SceneRenderer::GetOrCreateSelectMeshPSO(
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
        RenderTarget *framebuffer, nri::FillMode fillMode,
        const char *vertexShaderPath, const char *pixelShaderPath,
        EBindingLayout meshLayout)
    {
        if (!framebuffer)
            return nullptr;

        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }

        GraphicsPipelineParams params;
        params.enableBlend = false;
        params.enableDepthTest = false;
        params.enableDepthWrite = false;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.depthFunc = nri::CompareOp::ALWAYS;
        params.cullMode = nri::CullMode::FRONT;
        params.colorWriteMask[0] = nri::ColorWriteBits::NONE;

        Ref<Shader> vertexShader = Shader::Create(vertexShaderPath, UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create(pixelShaderPath, UMBRA_SHADER_TYPE_PIXEL, true);

        auto gp = GraphicsPipeline::Create("Mesh Select Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(meshLayout))
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MATERIAL))
            .AddBindingLayout(BindlessSystem::GetBindingLayout())
            .Build(framebuffer, params);

        cache.clear();
        cache.emplace(key, gp);
        return gp;
    }

    Ref<GraphicsPipeline> SceneRenderer::GetStaticSelectPSO(RenderTarget *framebuffer, nri::FillMode fillMode)
    {
        return GetOrCreateSelectMeshPSO(m_SelectStaticPSOCache, framebuffer, fillMode,
            "resources/shaders/mesh_static.vertex.hlsl", "resources/shaders/mesh_static.pixel.hlsl",
            EBindingLayout::MESH_STATIC);
    }

    Ref<GraphicsPipeline> SceneRenderer::GetAnimatedSelectPSO(RenderTarget *framebuffer, nri::FillMode fillMode)
    {
        return GetOrCreateSelectMeshPSO(m_SelectAnimatedPSOCache, framebuffer, fillMode,
            "resources/shaders/mesh_anim.vertex.hlsl", "resources/shaders/mesh_anim.pixel.hlsl",
            EBindingLayout::MESH_ANIM);
    }

    void SceneRenderer::RenderSelectedEntitiesIDOverlay(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext, RenderTarget *framebuffer, std::unordered_set<Material *> &uploadedMaterialsThisPass)
    {
        if (m_SelectedEntities.empty() || !m_Scene || !framebuffer)
            return;

        IGN_PROFILE_FUNCTION();

        auto staticSelectPSO = GetStaticSelectPSO(framebuffer, sceneRenderSettings.fillMode);
        auto animatedSelectPSO = GetAnimatedSelectPSO(framebuffer, sceneRenderSettings.fillMode);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        const bool startedRendering = !framebuffer->IsRendering();
        if (startedRendering)
            framebuffer->BeginRendering(cmd, false, false);

        // 1. Draw selected static meshes
        auto staticMeshView = m_Scene->registry->view<TransformComponent, RenderingComponent, StaticMeshComponent>();
        for (entt::entity e : staticMeshView)
        {
            const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
            if (std::ranges::find(m_SelectedEntities, objectID) == m_SelectedEntities.end())
                continue;

            const auto &[tr, rc, smc] = staticMeshView.get<TransformComponent, RenderingComponent, StaticMeshComponent>(e);
            if (!rc.visible || smc.handle == AssetHandle(0))
                continue;

            auto staticMesh = ResolveAsset<StaticMesh>(smc.handle);
            if (!staticMesh)
                continue;

            auto cacheIt = m_EntityObjectIndexCache.find(e);
            if (cacheIt == m_EntityObjectIndexCache.end())
                continue;

            const auto &instances = staticMesh->GetMeshInstances();
            for (size_t idx = 0; idx < instances.size(); ++idx)
            {
                if (idx >= cacheIt->second.size())
                    break;

                auto &meshInstance = instances[idx];
                auto &primitive = meshInstance->GetPrimitive();
                if (!primitive || !primitive->vertexBuffer || !primitive->indexBuffer)
                    continue;

                const uint32_t objectIndex = cacheIt->second[idx];

                // Pre-allocate instance index BEFORE setting graphics state to avoid buffer write inside render pass
                uint32_t baseOffset = frameContext->instanceIndexAllocator.Allocate(cmd, &objectIndex, 1);

                Ref<Material> material = ResolveMeshMaterial(static_cast<int>(idx), smc.overrideMaterials, meshInstance->GetMaterialAssetHandle());
                if (material && uploadedMaterialsThisPass.insert(material.get()).second)
                {
                    material->UploadToGpu(cmd);
                }

                const Ref<BindingSet> materialBindingSet = (material && material->GetBindingSet())
                    ? material->GetBindingSet()
                    : m_RuntimeMaterial->GetBindingSet();

                if (!materialBindingSet || !staticSelectPSO || !staticSelectPSO->GetHandle())
                    continue;

                nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *staticSelectPSO->GetPipelineLayout());
                nri.CmdSetPipeline(*cmd, *staticSelectPSO->GetHandle());

                nri::SetDescriptorSetDesc set0 = { 0, frameContext->staticMeshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set1 = { 1, materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
                nri.CmdSetDescriptorSet(*cmd, set0);
                nri.CmdSetDescriptorSet(*cmd, set1);
                nri.CmdSetDescriptorSet(*cmd, set2);

                nri::VertexBufferDesc vbDesc = {};
                vbDesc.buffer = primitive->vertexBuffer->GetHandle();
                vbDesc.offset = 0;
                vbDesc.stride = sizeof(VertexMeshStatic);
                nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                nri.CmdSetIndexBuffer(*cmd, *primitive->indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

                nri::SetRootConstantsDesc rootConstDesc = {};
                rootConstDesc.rootConstantIndex = 0;
                rootConstDesc.data = &baseOffset;
                rootConstDesc.size = sizeof(baseOffset);
                rootConstDesc.offset = 0;
                rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
                nri.CmdSetRootConstants(*cmd, rootConstDesc);

                nri::DrawIndexedDesc drawDesc = {};
                drawDesc.indexNum = primitive->indexBuffer->GetCount();
                drawDesc.instanceNum = 1;
                nri.CmdDrawIndexed(*cmd, drawDesc);
            }
        }

        // 2. Draw selected skeletal meshes
        auto skelMeshView = m_Scene->registry->view<TransformComponent, RenderingComponent, SkeletalMeshComponent>();
        for (entt::entity e : skelMeshView)
        {
            const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
            if (std::ranges::find(m_SelectedEntities, objectID) == m_SelectedEntities.end())
                continue;

            const auto &[tr, rc, smc] = skelMeshView.get<TransformComponent, RenderingComponent, SkeletalMeshComponent>(e);
            if (!rc.visible || smc.handle == AssetHandle(0))
                continue;

            auto skeletalMesh = ResolveAsset<SkeletalMesh>(smc.handle);
            if (!skeletalMesh)
                continue;

            auto cacheIt = m_EntityObjectIndexCache.find(e);
            if (cacheIt == m_EntityObjectIndexCache.end())
                continue;

            const auto &instances = skeletalMesh->GetMeshInstances();
            for (size_t idx = 0; idx < instances.size(); ++idx)
            {
                if (idx >= cacheIt->second.size())
                    break;

                auto &meshInstance = instances[idx];
                auto &primitive = meshInstance->GetPrimitive();
                if (!primitive || !primitive->vertexBuffer || !primitive->indexBuffer)
                    continue;

                const uint32_t objectIndex = cacheIt->second[idx];

                Ref<Material> material = ResolveMeshMaterial(static_cast<int>(idx), smc.overrideMaterials, meshInstance->GetMaterialAssetHandle());
                if (material && uploadedMaterialsThisPass.insert(material.get()).second)
                {
                    material->UploadToGpu(cmd);
                }

                const Ref<BindingSet> materialBindingSet = (material && material->GetBindingSet())
                    ? material->GetBindingSet()
                    : m_RuntimeMaterial->GetBindingSet();

                if (!materialBindingSet || !animatedSelectPSO || !animatedSelectPSO->GetHandle())
                    continue;

                nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *animatedSelectPSO->GetPipelineLayout());
                nri.CmdSetPipeline(*cmd, *animatedSelectPSO->GetHandle());

                nri::SetDescriptorSetDesc set0 = { 0, frameContext->animatedBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set1 = { 1, materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
                nri.CmdSetDescriptorSet(*cmd, set0);
                nri.CmdSetDescriptorSet(*cmd, set1);
                nri.CmdSetDescriptorSet(*cmd, set2);

                nri::VertexBufferDesc vbDesc = {};
                vbDesc.buffer = primitive->vertexBuffer->GetHandle();
                vbDesc.offset = 0;
                vbDesc.stride = sizeof(VertexMeshAnim);
                nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                nri.CmdSetIndexBuffer(*cmd, *primitive->indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

                nri::SetRootConstantsDesc rootConstDesc = {};
                rootConstDesc.rootConstantIndex = 0;
                rootConstDesc.data = &objectIndex;
                rootConstDesc.size = sizeof(objectIndex);
                rootConstDesc.offset = 0;
                rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
                nri.CmdSetRootConstants(*cmd, rootConstDesc);

                nri::DrawIndexedDesc drawDesc = {};
                drawDesc.indexNum = primitive->indexBuffer->GetCount();
                drawDesc.instanceNum = 1;
                nri.CmdDrawIndexed(*cmd, drawDesc);
            }

            // Socket attachments
            if (skeletalMesh && skeletalMesh->GetSkeletonHandle() != AssetHandle(0))
            {
                Ref<Skeleton> skeleton = ResolveAsset<Skeleton>(skeletalMesh->GetSkeletonHandle());
                if (skeleton)
                {
                    for (const auto &[socketName, attachedMeshHandle] : smc.socketAttachments)
                    {
                        if (attachedMeshHandle == AssetHandle(0))
                            continue;

                        auto attachedMeshAsset = ResolveAsset<Asset>(attachedMeshHandle);
                        if (!attachedMeshAsset)
                            continue;

                        auto attachedStaticMesh = std::dynamic_pointer_cast<StaticMesh>(attachedMeshAsset);
                        if (attachedStaticMesh)
                        {
                            auto socketCacheIt = m_SocketObjectIndexCache.find(std::make_pair(e, socketName));
                            if (socketCacheIt == m_SocketObjectIndexCache.end())
                                continue;

                            const auto &attachedInstances = attachedStaticMesh->GetMeshInstances();
                            for (size_t attIdx = 0; attIdx < attachedInstances.size(); ++attIdx)
                            {
                                if (attIdx >= socketCacheIt->second.size())
                                    break;

                                auto &meshInstance = attachedInstances[attIdx];
                                auto &primitive = meshInstance->GetPrimitive();
                                if (!primitive || !primitive->vertexBuffer || !primitive->indexBuffer)
                                    continue;

                                const uint32_t objectIndex = socketCacheIt->second[attIdx];
                                uint32_t baseOffset = frameContext->instanceIndexAllocator.Allocate(cmd, &objectIndex, 1);

                                Ref<Material> material = ResolveMeshMaterial(static_cast<int>(attIdx), smc.overrideMaterials, meshInstance->GetMaterialAssetHandle());
                                if (material && uploadedMaterialsThisPass.insert(material.get()).second)
                                {
                                    material->UploadToGpu(cmd);
                                }

                                const Ref<BindingSet> materialBindingSet = (material && material->GetBindingSet())
                                    ? material->GetBindingSet()
                                    : m_RuntimeMaterial->GetBindingSet();

                                if (!materialBindingSet || !staticSelectPSO || !staticSelectPSO->GetHandle())
                                    continue;

                                nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *staticSelectPSO->GetPipelineLayout());
                                nri.CmdSetPipeline(*cmd, *staticSelectPSO->GetHandle());

                                nri::SetDescriptorSetDesc set0 = { 0, frameContext->staticMeshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                                nri::SetDescriptorSetDesc set1 = { 1, materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                                nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
                                nri.CmdSetDescriptorSet(*cmd, set0);
                                nri.CmdSetDescriptorSet(*cmd, set1);
                                nri.CmdSetDescriptorSet(*cmd, set2);

                                nri::VertexBufferDesc vbDesc = {};
                                vbDesc.buffer = primitive->vertexBuffer->GetHandle();
                                vbDesc.offset = 0;
                                vbDesc.stride = sizeof(VertexMeshStatic);
                                nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                                nri.CmdSetIndexBuffer(*cmd, *primitive->indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

                                nri::SetRootConstantsDesc rootConstDesc = {};
                                rootConstDesc.rootConstantIndex = 0;
                                rootConstDesc.data = &baseOffset;
                                rootConstDesc.size = sizeof(baseOffset);
                                rootConstDesc.offset = 0;
                                rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
                                nri.CmdSetRootConstants(*cmd, rootConstDesc);

                                nri::DrawIndexedDesc drawDesc = {};
                                drawDesc.indexNum = primitive->indexBuffer->GetCount();
                                drawDesc.instanceNum = 1;
                                nri.CmdDrawIndexed(*cmd, drawDesc);
                            }
                        }
                    }
                }
            }
        }

        // 3. Draw selected terrain
        auto terrainView = m_Scene->registry->view<TransformComponent, RenderingComponent, TerrainComponent>();
        for (entt::entity e : terrainView)
        {
            const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
            if (std::ranges::find(m_SelectedEntities, objectID) == m_SelectedEntities.end())
                continue;

            const auto &[tr, rc, tc] = terrainView.get<TransformComponent, RenderingComponent, TerrainComponent>(e);
            if (!rc.visible)
                continue;

            auto cacheIt = m_EntityObjectIndexCache.find(e);
            if (cacheIt == m_EntityObjectIndexCache.end())
                continue;

            Ref<Material> material = ResolveAsset<Material>(tc.materialHandle);
            if (material && uploadedMaterialsThisPass.insert(material.get()).second)
            {
                material->UploadToGpu(cmd);
            }

            const Ref<BindingSet> materialBindingSet = (material && material->GetBindingSet())
                ? material->GetBindingSet()
                : m_RuntimeMaterial->GetBindingSet();

            if (!materialBindingSet || !staticSelectPSO || !staticSelectPSO->GetHandle())
                continue;

            for (size_t idx = 0; idx < tc.chunks.size(); ++idx)
            {
                if (idx >= cacheIt->second.size())
                    break;

                auto &chunk = tc.chunks[idx];
                if (!chunk.primitive || !chunk.primitive->vertexBuffer || !chunk.primitive->indexBuffer)
                    continue;

                const uint32_t objectIndex = cacheIt->second[idx];
                uint32_t baseOffset = frameContext->instanceIndexAllocator.Allocate(cmd, &objectIndex, 1);

                nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *staticSelectPSO->GetPipelineLayout());
                nri.CmdSetPipeline(*cmd, *staticSelectPSO->GetHandle());

                nri::SetDescriptorSetDesc set0 = { 0, frameContext->staticMeshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set1 = { 1, materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
                nri.CmdSetDescriptorSet(*cmd, set0);
                nri.CmdSetDescriptorSet(*cmd, set1);
                nri.CmdSetDescriptorSet(*cmd, set2);

                nri::VertexBufferDesc vbDesc = {};
                vbDesc.buffer = chunk.primitive->vertexBuffer->GetHandle();
                vbDesc.offset = 0;
                vbDesc.stride = sizeof(VertexMeshStatic);
                nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                nri.CmdSetIndexBuffer(*cmd, *chunk.primitive->indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

                nri::SetRootConstantsDesc rootConstDesc = {};
                rootConstDesc.rootConstantIndex = 0;
                rootConstDesc.data = &baseOffset;
                rootConstDesc.size = sizeof(baseOffset);
                rootConstDesc.offset = 0;
                rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
                nri.CmdSetRootConstants(*cmd, rootConstDesc);

                nri::DrawIndexedDesc drawDesc = {};
                drawDesc.indexNum = chunk.primitive->indexBuffer->GetCount();
                drawDesc.instanceNum = 1;
                nri.CmdDrawIndexed(*cmd, drawDesc);
            }
        }

        if (startedRendering)
            framebuffer->EndRendering(cmd);

        // Particle Systems Pass (Selected outline mask)
        {
            auto particleView = m_Scene->registry->view<TransformComponent, RenderingComponent, ParticleSystemComponent>();
            for (entt::entity e : particleView)
            {
                const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                if (std::ranges::find(m_SelectedEntities, objectID) == m_SelectedEntities.end())
                    continue;

                auto [tr, rc, pc] = particleView.get<TransformComponent, RenderingComponent, ParticleSystemComponent>(e);
                if (!rc.visible || !pc.isPlaying)
                    continue;

                if (pc.system)
                {
                    pc.system->Render(cmd, camera, framebuffer, pc.settings, objectID);
                }
            }
        }

        // 4. Draw selected 2D renderables (Circles, Quads/Sprites, Text)
        if (m_Renderer2D)
        {
            bool hasSelected2D = false;
            m_Renderer2D->Begin(cmd);

            auto circle2DView = m_Scene->registry->view<TransformComponent, RenderingComponent, Circle2DComponent>();
            for (entt::entity e : circle2DView)
            {
                const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                if (std::ranges::find(m_SelectedEntities, objectID) == m_SelectedEntities.end())
                    continue;

                const auto &[tr, rc, circle] = m_Scene->registry->get<TransformComponent, RenderingComponent, Circle2DComponent>(e);
                if (!rc.visible)
                    continue;

                m_Renderer2D->DrawCircle(tr.world.GetMatrix(), circle.color, circle.thickness, circle.fade, objectID);
                hasSelected2D = true;
            }

            auto quad2DView = m_Scene->registry->view<TransformComponent, RenderingComponent, Sprite2DComponent>();
            for (entt::entity e : quad2DView)
            {
                const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                if (std::ranges::find(m_SelectedEntities, objectID) == m_SelectedEntities.end())
                    continue;

                const auto &[tr, rc, sprite] = m_Scene->registry->get<TransformComponent, RenderingComponent, Sprite2DComponent>(e);
                if (!rc.visible)
                    continue;

                glm::vec2 uv0 = sprite.uv0;
                glm::vec2 uv1 = sprite.uv1;
                if (sprite.flipY)
                {
                    std::swap(uv0.y, uv1.y);
                }
                if (sprite.flipX)
                {
                    std::swap(uv0.x, uv1.x);
                }

                Ref<Material2D> mat2d = ResolveAsset<Material2D>(sprite.materialHandle);
                if (mat2d)
                {
                    Ref<Texture> texture = ResolveAsset<Texture>(mat2d->textureHandle);
                    m_Renderer2D->DrawQuad(tr.world.GetMatrix(), mat2d->data.baseColor,
                        mat2d->data.additiveColor, mat2d->data.type, texture, uv0, uv1,
                        mat2d->data.tilingFactor, objectID);
                }
                else
                {
                    Ref<Texture> texture = ResolveAsset<Texture>(sprite.handle);
                    m_Renderer2D->DrawQuad(tr.world.GetMatrix(), sprite.color,
                        texture, uv0, uv1, sprite.tilingFactor, objectID);
                }
                hasSelected2D = true;
            }

            auto textView = m_Scene->registry->view<TransformComponent, RenderingComponent, TextComponent>();
            for (entt::entity e : textView)
            {
                const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                if (std::ranges::find(m_SelectedEntities, objectID) == m_SelectedEntities.end())
                    continue;

                const auto &[tr, rc, text] = m_Scene->registry->get<TransformComponent, RenderingComponent, TextComponent>(e);
                if (!rc.visible || text.fontHandle == AssetHandle(0) || text.text.empty())
                    continue;

                auto font = ResolveAsset<Font>(text.fontHandle);
                if (!font)
                    continue;

                Ref<Texture> fontAtlas = font->GetAtlasTexture();
                if (!fontAtlas || !fontAtlas->IsReady())
                    continue;

                glm::vec4 textColor = text.color;
                if (text.material2dHandle != AssetHandle(0))
                {
                    if (auto material2d = ResolveAsset<Material2D>(text.material2dHandle))
                    {
                        textColor = text.color + material2d->data.baseColor;
                    }
                }

                m_Renderer2D->DrawString(text.text, font, textColor, tr.world.GetMatrix(), text.kerning, text.lineSpacing, objectID);
                hasSelected2D = true;
            }

            if (hasSelected2D)
            {
                m_Renderer2D->FlushSelect(framebuffer, frameContext->cameraBuffer);
            }
            m_Renderer2D->End();
        }
    }

    Ref<GraphicsPipeline> SceneRenderer::GetAnimatedCSMPSO()
    {
        RenderTarget *framebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(0);
        return GetOrCreateCMSPSO(m_AnimatedCSMPSOCache, framebuffer,
            "resources/shaders/csm_anim.vertex.hlsl", "resources/shaders/csm.pixel.hlsl",
            EBindingLayout::MESH_ANIM);
    }

    Ref<GraphicsPipeline> SceneRenderer::GetStaticCSMPSO()
    {
        RenderTarget *framebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(0);
        return GetOrCreateCMSPSO(m_StaticCSMPSOCache, framebuffer,
            "resources/shaders/csm_static.vertex.hlsl", "resources/shaders/csm.pixel.hlsl",
            EBindingLayout::MESH_STATIC);
    }

    // Helper to build an environment pipeline per framebuffer (once)
    Ref<GraphicsPipeline> SceneRenderer::GetEnvironmentPSO(RenderTarget *framebuffer, nri::FillMode fillMode)
    {
        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = m_EnvironmentPSOCache.find(key);
        if (it != m_EnvironmentPSOCache.end())
        {
            return it->second;
        }

        const bool hasDepthAttachment = framebuffer->HasDepthAttachment();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.enableDepthWrite = hasDepthAttachment;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.cullMode = nri::CullMode::FRONT;
        params.depthFunc = nri::CompareOp::ALWAYS;

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/skybox.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/skybox.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        auto gp = GraphicsPipeline::Create("Skybox Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::ENVIRONMENT))
            .Build(framebuffer, params);

        m_EnvironmentPSOCache.clear();
        m_EnvironmentPSOCache.emplace(key, gp);
        return gp;
    }

    // Helper to build a composite pipeline per framebuffer (once)
    Ref<GraphicsPipeline> SceneRenderer::GetCompositePSO(RenderTarget *framebuffer, nri::FillMode fillMode)
    {
        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = m_CompositePSOCache.find(key);

        if (it != m_CompositePSOCache.end())
        {
            return it->second;
        }

        // Binding layout
        BindingLayoutDesc layoutDesc = {};
        layoutDesc.SetRegisterSpace(0);
        layoutDesc.SetVisibility(nri::StageBits::ALL);
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(0)); // scene
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(1)); // ui
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(2)); // edge
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(3)); // bloom
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(4)); // ssao
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(5)); // depth
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(6)); // debug
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(7)); // objectID
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(8)); // TAA history
        layoutDesc.AddItem(BindingLayoutItem::VolatileConstantBuffer(0)); // post-process params
        layoutDesc.AddItem(BindingLayoutItem::Sampler(0)); // sampler

        const bool hasDepthAttachment = framebuffer->HasDepthAttachment();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.enableDepthWrite = hasDepthAttachment;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.cullMode = nri::CullMode::NONE;

        // Create pipeline
        Ref<Shader> vertexShader = Shader::Create("resources/shaders/composite.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/composite.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        auto gp = GraphicsPipeline::Create("Composite Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(layoutDesc)
            .Build(framebuffer, params);

        LOG_INFO("[Composite] Created new pipeline with forced shader recompilation");

        m_CompositePSOCache.emplace(key, gp);

        return gp;
    }

    Ref<GraphicsPipeline> SceneRenderer::GetFXAAPSO(RenderTarget *framebuffer)
    {
        auto key = MakeFramebufferKey(framebuffer, nri::FillMode::SOLID);
        auto it = m_FXAAPSOCache.find(key);
        if (it != m_FXAAPSOCache.end())
            return it->second;

        BindingLayoutDesc layoutDesc = {};
        layoutDesc.SetVisibility(nri::StageBits::ALL);
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(0)); // inputTexture
        layoutDesc.AddItem(BindingLayoutItem::Sampler(0));     // sampler
        layoutDesc.AddItem(BindingLayoutItem::VolatileConstantBuffer(0)); // FXAABuffer

        GraphicsPipelineParams params;
        params.enableBlend = false;
        params.enableDepthWrite = false;
        params.enableDepthTest = false;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;

        // Create pipeline
        Ref<Shader> vertexShader = Shader::Create("resources/shaders/fxaa.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/fxaa.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        Ref<GraphicsPipeline> gp = GraphicsPipeline::Create("FXAAPass");
        gp->SetShaders({vertexShader, pixelShader })
            .AddBindingLayout(layoutDesc)
            .Build(framebuffer, params);

        m_FXAAPSOCache.emplace(key, gp);
        return gp;
    }

    void SceneRenderer::FXAAPass(nri::CommandBuffer *cmd, RenderTarget *framebuffer, Ref<Texture> inputTexture)
    {
        IGN_PROFILE_FUNCTION();

        if (!framebuffer || !inputTexture)
            return;
        
        glm::vec4 inverseScreenSize = glm::vec4(1.0f / static_cast<float>(framebuffer->GetWidth()), 1.0f / static_cast<float>(framebuffer->GetHeight()), 0.0f, 0.0f);
        m_FXAABuffer.SetData(&inverseScreenSize, sizeof(glm::vec4));

        Ref<GraphicsPipeline> pso = GetFXAAPSO(framebuffer);
        if (!pso || !pso->GetHandle())
            return;

        nri::PipelineLayout *layout = pso->GetPipelineLayout();

        FXAABindingKey key { layout, inputTexture->GetShaderResourceView() };
        auto it = m_FXAABindingSetCache.find(key);
        Ref<BindingSet> bindingSet;
        if (it != m_FXAABindingSetCache.end())
        {
            bindingSet = it->second;
        }
        else
        {
            BindingSetDesc desc = BindingSetDesc()
                .AddItem(BindingSetItem::TextureSRV(0, inputTexture->GetShaderResourceView()))
                .AddItem(BindingSetItem::Sampler(0, m_CompositeSampler))
                .AddItem(BindingSetItem::ConstantBuffer(0, m_FXAABuffer.GetDescriptor()));

            bindingSet = BindingSet::Create("FXAA Binding Set", BindingSet::AllocateAndUpdate(layout, 0, desc));
            LOG_ASSERT(bindingSet && bindingSet->GetHandle(), "[FXAA] Failed to create FXAA Binding Set");
            m_FXAABindingSetCache.emplace(key, bindingSet);
        }

        if (!bindingSet || !bindingSet->GetHandle())
            return;

        const bool startedRendering = !framebuffer->IsRendering();
        if (startedRendering)
            framebuffer->BeginRendering(cmd, false, false);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *pso->GetPipelineLayout());
        nri.CmdSetPipeline(*cmd, *pso->GetHandle());

        nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        nri::VertexBufferDesc vbDesc = {};
        vbDesc.buffer = m_CompositeVertexBuffer->GetHandle();
        vbDesc.offset = 0;
        vbDesc.stride = sizeof(VertexScreen);
        nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);

        nri::DrawDesc drawDesc = {};
        drawDesc.instanceNum = 1;
        drawDesc.vertexNum = 6;
        nri.CmdDraw(*cmd, drawDesc);

        if (startedRendering)
            framebuffer->EndRendering(cmd);
    }

    Ref<BindingSet> SceneRenderer::GetOrCreateCompositeBindingSet(nri::PipelineLayout *pipelineLayout, Ref<CameraRenderTarget> target, Ref<Texture> edgeTexture,
            Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, Ref<Texture> taaHistoryTexture, const ConstantBuffer &postProcessBuffer, nri::Descriptor *sampler, bool useResolvedScene)
    {
        Ref<Texture> edge = edgeTexture ? edgeTexture : Renderer::GetBlackTexture();
        Ref<Texture> bloom = bloomTexture ? bloomTexture : Renderer::GetBlackTexture();
        Ref<Texture> ssao = ssaoTexture ? ssaoTexture : Renderer::GetWhiteTexture();
        Ref<Texture> taaHistory = taaHistoryTexture ? taaHistoryTexture : Renderer::GetBlackTexture();
        Ref<Texture> debug = (target->debugRT && target->debugRT->GetColorAttachment(0)) ? target->debugRT->GetColorAttachment(0) : Renderer::GetBlackTexture();
        Ref<Texture> widget = (target->widgetRT && target->widgetRT->GetColorAttachment(0)) ? target->widgetRT->GetColorAttachment(0) : Renderer::GetBlackTexture();

        // When MSAA is active use the resolved single-sample textures for the composite pass
        const bool hasResolved = useResolvedScene && target->sceneResolvedRT;
        Ref<Texture> depth = (target->sceneRT && target->sceneRT->GetDepthAttachment() && !hasResolved) ? target->sceneRT->GetDepthAttachment() : Renderer::GetBlackTexture();
        Ref<Texture> sceneColor = (hasResolved ? target->sceneResolvedRT->GetColorAttachment(0) : (target->sceneRT ? target->sceneRT->GetColorAttachment(0) : Renderer::GetBlackTexture()));
        Ref<Texture> objectIDTex = hasResolved
            ? (target->sceneResolvedRT->GetColorAttachment(1) ? target->sceneResolvedRT->GetColorAttachment(1) : Renderer::GetBlackUIntTexture())
            : ((target->sceneRT && target->sceneRT->GetColorAttachment(1)) ? target->sceneRT->GetColorAttachment(1) : Renderer::GetBlackUIntTexture());

        CompositeBindingKey key
        {
            pipelineLayout,
            sceneColor->GetShaderResourceView(),
            widget->GetShaderResourceView(),
            edge->GetShaderResourceView(),
            bloom->GetShaderResourceView(),
            ssao->GetShaderResourceView(),
            depth->GetShaderResourceView(),
            debug->GetShaderResourceView(),
            objectIDTex->GetShaderResourceView(),
            taaHistory->GetShaderResourceView(),
            postProcessBuffer.GetDescriptor(),
            sampler
        };

        auto it = m_CompositeBindingSetCache.find(key);
        if (it != m_CompositeBindingSetCache.end())
        {
            return it->second;
        }

        // Composite Binding set
        auto bindingSetDesc = BindingSetDesc();
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(0, sceneColor->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(1, widget->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(2, edge->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(3, bloom->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(4, ssao->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(5, depth->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(6, debug->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(7, objectIDTex->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(8, taaHistory->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::ConstantBuffer(0, postProcessBuffer.GetDescriptor()));
        bindingSetDesc.AddItem(BindingSetItem::Sampler(0, sampler));

        Ref<BindingSet> bindingSet = BindingSet::Create("Composite Binding Set", BindingSet::AllocateAndUpdate(pipelineLayout, 0, bindingSetDesc));
        LOG_ASSERT(bindingSet && bindingSet->GetHandle(), "[Composite] Failed to create Composite Binding Set");

        m_CompositeBindingSetCache.emplace(key, bindingSet);

        return bindingSet;
    }

    void SceneRenderer::EnsureWidgetRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height)
    {
        if (!target->widgetRT)
        {
            RenderTargetCreateInfo widgetRTCreateInfo = {};
            widgetRTCreateInfo.width = width;
            widgetRTCreateInfo.height = height;
            widgetRTCreateInfo.attachments =
            {
                FramebufferAttachments{ "[Scene DepthAttachment]", nri::Format::D32_SFLOAT_S8_UINT, nri::Layout::DEPTH_STENCIL_ATTACHMENT },
                FramebufferAttachments{ "[Scene ColorAttachment]", nri::Format::RGBA8_UNORM, nri::Layout::COLOR_ATTACHMENT },
            };
            target->widgetRT = RenderTarget::Create(widgetRTCreateInfo, "[Scene Renderer] Widget RT");
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
            RenderTargetCreateInfo debugRTCreateInfo = {};
            debugRTCreateInfo.width = renderWidth;
            debugRTCreateInfo.height = renderHeight;
            debugRTCreateInfo.attachments =
            {
                FramebufferAttachments{ "[Scene DebugAttachment]", nri::Format::RGBA8_UNORM, nri::Layout::COLOR_ATTACHMENT },
                FramebufferAttachments{ "[Scene DebugObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT },
                FramebufferAttachments{ "[Scene DepthAttachment]", nri::Format::D32_SFLOAT_S8_UINT, nri::Layout::DEPTH_STENCIL_ATTACHMENT }
            };
            debugRTCreateInfo.depthAttachmentOverride = (target->sceneRT && target->msaaSampleCount == 1) ? target->sceneRT->GetDepthAttachment() : nullptr;
            target->debugRT = RenderTarget::Create(debugRTCreateInfo, "[Scene Renderer] Debug RT");
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
            RenderTargetCreateInfo taaHistoryRTCreateInfo = {};
            taaHistoryRTCreateInfo.width = width;
            taaHistoryRTCreateInfo.height = height;
            taaHistoryRTCreateInfo.attachments =
            {
                FramebufferAttachments{ "[TAA History Color Attachment]", nri::Format::RGBA8_UNORM, nri::Layout::SHADER_RESOURCE }
            };
            for (Ref<RenderTarget> &historyRT : target->taaHistoryRT)
            {
                historyRT = RenderTarget::Create(taaHistoryRTCreateInfo, "[Scene Renderer] TAA History RT");
            }
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

        const bool sampleCountChanged = (target->msaaSampleCount != desiredSampleCount);
        const bool needsResolveRT = (desiredSampleCount > 1);
        const bool resolveRTMissing = needsResolveRT && (!target->sceneResolvedRT);

        if (!sampleCountChanged && !resolveRTMissing)
            return;

        target->msaaSampleCount = desiredSampleCount;

        // Recreate sceneRT with the desired sample count
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

        // Manage resolved target
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

        // Invalidate debugRT so EnsureDebugRT recreates it with matching depth attachment or separate depth attachment
        target->debugRT = nullptr;

        m_CompositeBindingSetCache.clear();
    }

    Ref<CameraRenderTarget> SceneRenderer::GetOrCreateRenderTarget(ICamera *camera)
    {
        auto it = m_RenderTargets.find(camera);
        if (it != m_RenderTargets.end())
            return it->second;

        Ref<CameraRenderTarget> target = CreateRef<CameraRenderTarget>();

        // Determine MSAA sample count from camera or scene settings
        const PostProcessing &pp = camera ? camera->postProcessing : PostProcessing{};
        bool msaaEnabled = false;
        int sampleCount = 1;
        if (sceneRenderSettings.sampleMethod == SampleMethod::MSAA)
        {
            msaaEnabled = true;
            sampleCount = sceneRenderSettings.msaaProperties.sampleCount;
        }
        else if (sceneRenderSettings.sampleMethod == SampleMethod::NONE || sceneRenderSettings.sampleMethod == SampleMethod::FXAA || sceneRenderSettings.sampleMethod == SampleMethod::TAA)
        {
            msaaEnabled = false;
            sampleCount = 1;
        }
        else
        {
            msaaEnabled = pp.msaaProperties.enable;
            sampleCount = pp.msaaProperties.sampleCount;
        }

        if (sampleCount <= 1) sampleCount = 1;
        else if (sampleCount <= 2) sampleCount = 2;
        else if (sampleCount <= 4) sampleCount = 4;
        else sampleCount = 8;
        target->msaaSampleCount = sampleCount;

        // =========================================
        // Create Render Targets
        RenderTargetCreateInfo sceneRTCreateInfo = {};
        sceneRTCreateInfo.sampleCount = sampleCount;
        sceneRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Scene DepthAttachment]", nri::Format::D32_SFLOAT_S8_UINT, nri::Layout::DEPTH_STENCIL_ATTACHMENT }, // Depth
            FramebufferAttachments{ "[Scene ColorAttachment]", nri::Format::RGBA16_SFLOAT, nri::Layout::COLOR_ATTACHMENT }, // HDR Main Color
            FramebufferAttachments{ "[Scene ObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT } // Object ID
        };

        target->sceneRT = RenderTarget::Create(sceneRTCreateInfo, "[Scene Renderer] Scene RT");

        // Selection IDs are rendered into a separate single-sample mask. This pass intentionally
        // ignores scene depth so selected objects remain outlineable through occluders, without
        // modifying the scene ID buffer consumed by PickEntity.
        RenderTargetCreateInfo selectionRTCreateInfo = {};
        selectionRTCreateInfo.sampleCount = 1;
        selectionRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Selection Mask ColorAttachment]", nri::Format::RGBA16_SFLOAT, nri::Layout::COLOR_ATTACHMENT },
            FramebufferAttachments{ "[Selection Mask ObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT }
        };
        target->selectionRT = RenderTarget::Create(selectionRTCreateInfo, "[Scene Renderer] Selection Mask RT");

        // If MSAA is active, create a single-sample resolve target that downstream passes (bloom, composite) read from
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

        // Composite RT (always needed for final output)
        RenderTargetCreateInfo compositeRTCreateInfo = {};
        compositeRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Composite Color Attachment]", nri::Format::RGBA8_UNORM, nri::Layout::COLOR_ATTACHMENT } // Main Color
        };
        target->compositeRT = RenderTarget::Create(compositeRTCreateInfo, "[Scene Renderer] Composite RT");
        target->postProcessRT = RenderTarget::Create(compositeRTCreateInfo, "[Scene Renderer] Post Process RT");

        if (!m_WidgetRenderer)
        {
            m_WidgetRenderer = WidgetRenderer::Create(1280, 720);
        }

        m_RenderTargets.emplace(camera, target);

        const glm::uvec2 vpSize = camera ? camera->GetViewportSize() : glm::uvec2(0);
        if (vpSize.x > 0 && vpSize.y > 0)
        {
            ResizeFramebuffer(camera, vpSize.x, vpSize.y);
        }

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
        {
            Ref<Bloom> bloom = CreateRef<Bloom>(resolution, resolution);
            blooms[i] = bloom;
        }

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
        {
            Ref<SSAO> ssao = CreateRef<SSAO>(cmd, resolution, resolution);
            SSAOs[i] = ssao;
        }

        m_SSAOs.emplace(camera, SSAOs);
        return SSAOs;
    }

    // FlushOpaqueBatches — dispatch all opaque static mesh batches as instanced draws.
    // Each DrawBatch contains N instances sharing the same vertex buffer, index buffer,
    // material binding set, and PSO. The object indices are uploaded to the
    // InstanceIndexBuffer and referenced via push constant base offset.
    void SceneRenderer::FlushOpaqueBatches(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *framebuffer)
    {
        if (m_OpaqueBatchBuilder.IsEmpty() || !framebuffer)
            return;

        IGN_PROFILE_SCOPE("SceneRenderer::FlushOpaqueBatches");

        m_OpaqueBatchBuilder.Finalize();
        const auto &batches = m_OpaqueBatchBuilder.GetBatches();

        // Batch upload all instance indices upfront in a single contiguous writeBuffer call
        std::vector<uint32_t> allObjectIndices;
        std::vector<uint32_t> batchBaseOffsets;
        batchBaseOffsets.reserve(batches.size());

        for (const DrawBatch &batch : batches)
        {
            const auto offset = static_cast<uint32_t>(allObjectIndices.size());
            batchBaseOffsets.push_back(offset);
            allObjectIndices.insert(allObjectIndices.end(), batch.objectIndices.begin(), batch.objectIndices.end());
        }

        const uint32_t globalBaseOffset = frameContext->instanceIndexAllocator
            .Allocate(cmd, allObjectIndices.data(), static_cast<uint32_t>(allObjectIndices.size()));

        const bool startedRendering = !framebuffer->IsRendering();
        if (startedRendering)
            framebuffer->BeginRendering(cmd, false, false);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        for (size_t i = 0; i < batches.size(); ++i)
        {
            const DrawBatch &batch = batches[i];
            if (!batch.meshBindingSet || !batch.materialBindingSet || !batch.vertexBuffer || !batch.indexBuffer || !batch.pipeline || !batch.pipeline->GetHandle())
                continue;

            const uint32_t baseOffset = globalBaseOffset + batchBaseOffsets[i];

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *batch.pipeline->GetPipelineLayout());
            nri.CmdSetPipeline(*cmd, *batch.pipeline->GetHandle());

            nri::SetDescriptorSetDesc set0 = { 0, batch.meshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
            nri::SetDescriptorSetDesc set1 = { 1, batch.materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
            nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
            nri.CmdSetDescriptorSet(*cmd, set0);
            nri.CmdSetDescriptorSet(*cmd, set1);
            nri.CmdSetDescriptorSet(*cmd, set2);

            nri::VertexBufferDesc vbDesc = {};
            vbDesc.buffer = batch.vertexBuffer;
            vbDesc.offset = 0;
            vbDesc.stride = batch.vertexStride ? batch.vertexStride : static_cast<uint32_t>(sizeof(VertexMeshStatic));
            nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
            nri.CmdSetIndexBuffer(*cmd, *batch.indexBuffer, 0, nri::IndexType::UINT32);

            nri::SetRootConstantsDesc rootConstDesc = {};
            rootConstDesc.rootConstantIndex = 0;
            rootConstDesc.data = &baseOffset;
            rootConstDesc.size = sizeof(baseOffset);
            rootConstDesc.offset = 0;
            rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
            nri.CmdSetRootConstants(*cmd, rootConstDesc);

            nri::DrawIndexedDesc drawDesc = {};
            drawDesc.indexNum = batch.indexCount;
            drawDesc.instanceNum = batch.GetInstanceCount();
            nri.CmdDrawIndexed(*cmd, drawDesc);

            Renderer::Stats.drawCallCount++;
            Renderer::Stats.indexCount3D += batch.indexCount * batch.GetInstanceCount();
        }

        if (startedRendering)
            framebuffer->EndRendering(cmd);
    }

    // ---------------------------------------------------------------------------
    // FlushShadowBatches — dispatch all CSM shadow batches as instanced draws.
    // Similar to FlushOpaqueBatches but uses the CSM framebuffer and no material
    // binding set (shadow pass only writes depth).
    // ---------------------------------------------------------------------------
    void SceneRenderer::FlushShadowBatches(nri::CommandBuffer *cmd, FrameContext *frameContext, uint32_t cascadeIndex)
    {
        if (m_ShadowBatchBuilder.IsEmpty())
            return;

        IGN_PROFILE_SCOPE("SceneRenderer::FlushShadowBatches");

        m_ShadowBatchBuilder.Finalize();
        const auto &batches = m_ShadowBatchBuilder.GetBatches();

        // Batch upload all instance indices upfront in a single contiguous writeBuffer call
        std::vector<uint32_t> allObjectIndices;
        std::vector<uint32_t> batchBaseOffsets;
        batchBaseOffsets.reserve(batches.size());

        for (const DrawBatch &batch : batches)
        {
            const auto offset = static_cast<uint32_t>(allObjectIndices.size());
            batchBaseOffsets.push_back(offset);
            allObjectIndices.insert(allObjectIndices.end(), batch.objectIndices.begin(), batch.objectIndices.end());
        }

        const uint32_t globalBaseOffset = frameContext->instanceIndexAllocator.Allocate(
            cmd,
            allObjectIndices.data(),
            static_cast<uint32_t>(allObjectIndices.size()));

        RenderTarget *csmFramebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(cascadeIndex, frameContext->frameIndexInFlight);
        if (!csmFramebuffer)
            return;

        const bool startedRendering = !csmFramebuffer->IsRendering();
        if (startedRendering)
            csmFramebuffer->BeginRendering(cmd, false, false);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        for (size_t i = 0; i < batches.size(); ++i)
        {
            const DrawBatch &batch = batches[i];
            if (!batch.meshBindingSet || !batch.vertexBuffer || !batch.indexBuffer || !batch.pipeline || !batch.pipeline->GetHandle())
                continue;

            const uint32_t baseOffset = globalBaseOffset + batchBaseOffsets[i];

            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *batch.pipeline->GetPipelineLayout());
            nri.CmdSetPipeline(*cmd, *batch.pipeline->GetHandle());

            nri::SetDescriptorSetDesc set0 = { 0, batch.meshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
            nri.CmdSetDescriptorSet(*cmd, set0);

            nri::VertexBufferDesc vbDesc = {};
            vbDesc.buffer = batch.vertexBuffer;
            vbDesc.offset = 0;
            vbDesc.stride = batch.vertexStride ? batch.vertexStride : static_cast<uint32_t>(sizeof(VertexMeshStatic));
            nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
            nri.CmdSetIndexBuffer(*cmd, *batch.indexBuffer, 0, nri::IndexType::UINT32);

            nri::SetRootConstantsDesc rootConstDesc = {};
            rootConstDesc.rootConstantIndex = 0;
            rootConstDesc.data = &baseOffset;
            rootConstDesc.size = sizeof(baseOffset);
            rootConstDesc.offset = 0;
            rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
            nri.CmdSetRootConstants(*cmd, rootConstDesc);

            nri::DrawIndexedDesc drawDesc = {};
            drawDesc.indexNum = batch.indexCount;
            drawDesc.instanceNum = batch.GetInstanceCount();
            nri.CmdDrawIndexed(*cmd, drawDesc);

            Renderer::Stats.shadowDrawCallCount++;
        }

        if (startedRendering)
            csmFramebuffer->EndRendering(cmd);
    }

    template<typename MeshT>
    void SceneRenderer::DrawMesh(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *framebuffer,
        const Ref<MeshT> &mesh, const glm::mat4 &parentTransform, const glm::mat4 &normalMatrix, uint32_t objectID,
        const std::unordered_map<int, AssetHandle> &overrideMaterials, const std::vector<glm::mat4> &boneTransforms,
        const std::vector<Mesh_GPUData> &cachedInstanceTransforms, ICamera *camera, Ref<GraphicsPipeline> opaquePSO,
        std::vector<TransparentDrawCall> &transparentDrawCalls, std::unordered_set<Material *> &uploadedMaterialsThisPass,
        entt::entity entity, const std::string &socketName)
    {
        (void)opaquePSO;
        if (!framebuffer)
            return;

        constexpr bool isSkeletal = std::is_same_v<MeshT, SkeletalMesh>;

        static_assert(std::is_same_v<MeshT, StaticMesh> || std::is_same_v<MeshT, SkeletalMesh>,
            "DrawPreviewMeshImpl: MeshT must be StaticMesh or SkeletalMesh");

        const auto &instances = mesh->GetMeshInstances();
        for (const auto &meshInstance : instances)
        {
            auto &primitive = meshInstance->GetPrimitive();
            if (primitive && (!primitive->vertexBuffer || !primitive->indexBuffer))
            {
                primitive->WriteBuffer(cmd);
            }
        }

        const bool startedRendering = !framebuffer->IsRendering();
        if (startedRendering)
            framebuffer->BeginRendering(cmd, false, false);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        for (size_t idx = 0; idx < instances.size(); ++idx)
        {
            auto &meshInstance = instances[idx];

            Mesh_GPUData gpuData;
            if (idx < cachedInstanceTransforms.size())
            {
                gpuData = cachedInstanceTransforms[idx];
            }
            else
            {
                glm::mat4 meshTransform = meshInstance->global;
                if constexpr (isSkeletal)
                {
                    if (meshInstance->linkedJointIndex >= 0 && !boneTransforms.empty())
                    {
                        const size_t ji = static_cast<size_t>(meshInstance->linkedJointIndex);
                        if (ji < boneTransforms.size())
                        {
                            meshTransform = boneTransforms[ji] * meshTransform;
                        }
                    }
                }

                gpuData.transformation = parentTransform * meshTransform;
                gpuData.normal = normalMatrix;
            }

            gpuData.objectID = objectID;

            uint32_t PushConstant_ObjectIndex = 0;
            bool foundInCache = false;
            if (entity != entt::null)
            {
                if (!socketName.empty())
                {
                    auto key = std::make_pair(entity, socketName);
                    auto cacheIt = m_SocketObjectIndexCache.find(key);
                    if (cacheIt != m_SocketObjectIndexCache.end() && idx < cacheIt->second.size())
                    {
                        PushConstant_ObjectIndex = cacheIt->second[idx];
                        foundInCache = true;
                    }
                }
                else
                {
                    auto cacheIt = m_EntityObjectIndexCache.find(entity);
                    if (cacheIt != m_EntityObjectIndexCache.end() && idx < cacheIt->second.size())
                    {
                        PushConstant_ObjectIndex = cacheIt->second[idx];
                        foundInCache = true;
                    }
                }
            }

            if (!foundInCache)
            {
                if constexpr (isSkeletal)
                {
                    glm::mat4 bones[MAX_BONES];
                    FillBoneArray(bones, boneTransforms);
                    gpuData.boneOffset = frameContext->boneAllocator.Allocate(cmd, bones, MAX_BONES);
                }
                else
                {
                    gpuData.boneOffset = 0;
                }

                PushConstant_ObjectIndex = frameContext->objectAllocator.Allocate(cmd, gpuData);
            }

            Ref<BindingSet> meshBindingSet = frameContext->staticMeshBindingSet;
            if constexpr (isSkeletal)
            {
                meshBindingSet = frameContext->animatedBindingSet;
            }

            auto &primitive = meshInstance->GetPrimitive();

            Ref<Material> material = ResolveMeshMaterial(static_cast<int>(idx), overrideMaterials, meshInstance->GetMaterialAssetHandle());

            const Ref<BindingSet> materialBindingSet = (material && material->GetBindingSet())
                ? material->GetBindingSet()
                : m_RuntimeMaterial->GetBindingSet();

            if (meshBindingSet && materialBindingSet && primitive && primitive->vertexBuffer && primitive->indexBuffer)
            {
                if (material)
                {
                    if (uploadedMaterialsThisPass.insert(material.get()).second)
                    {
                        material->UploadToGpu(cmd);
                    }
                }
                else
                {
                    if (uploadedMaterialsThisPass.insert(m_RuntimeMaterial.get()).second)
                    {
                        m_RuntimeMaterial->UploadToGpu(cmd);
                    }
                }

                MaterialType materialType = material ? material->GetType() : m_RuntimeMaterial->GetType();

                if (materialType == MaterialType::Transparent)
                {
                    TransparentDrawCall dc;
                    dc.meshBindingSet = meshBindingSet;
                    dc.materialBindingSet = materialBindingSet;
                    dc.vertexBuffer = primitive->vertexBuffer->GetHandle();
                    dc.indexBuffer = primitive->indexBuffer->GetHandle();
                    dc.indexCount = primitive->indexBuffer->GetCount();
                    dc.pushConstants_ObjectIndex = PushConstant_ObjectIndex;

                    AABB localAABB = meshInstance->localAABB;
                    AABB worldAABB = localAABB.Transform(gpuData.transformation);
                    glm::vec3 worldCenter = worldAABB.GetCenter();
                    dc.distanceToCamera = glm::dot(worldCenter - camera->position, camera->GetForwardDirection());

                    dc.isSkeletal = isSkeletal;
                    dc.gpuData = gpuData;
                    dc.meshInstance = meshInstance;
                    dc.cullMode = material ? material->GetCullMode() : (isSkeletal ? nri::CullMode::FRONT : nri::CullMode::NONE);
                    if constexpr (isSkeletal)
                    {
                        // Bones are indexed via pushConstants_ObjectIndex and GPU buffer allocator
                    }
                    transparentDrawCalls.push_back(dc);
                }
                else
                {
                    auto meshPSO = isSkeletal
                        ? GetAnimatedPSO(framebuffer, sceneRenderSettings.fillMode, material ? material->GetCullMode() : nri::CullMode::FRONT)
                        : GetStaticPSO(framebuffer, sceneRenderSettings.fillMode, material ? material->GetCullMode() : nri::CullMode::FRONT);
                    if (!meshPSO || !meshPSO->GetHandle())
                        continue;

                    nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *meshPSO->GetPipelineLayout());
                    nri.CmdSetPipeline(*cmd, *meshPSO->GetHandle());

                    nri::SetDescriptorSetDesc set0 = { 0, meshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                    nri::SetDescriptorSetDesc set1 = { 1, materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                    nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
                    nri.CmdSetDescriptorSet(*cmd, set0);
                    nri.CmdSetDescriptorSet(*cmd, set1);
                    nri.CmdSetDescriptorSet(*cmd, set2);

                    constexpr uint32_t vertexStride = isSkeletal ? static_cast<uint32_t>(sizeof(VertexMeshAnim)) : static_cast<uint32_t>(sizeof(VertexMeshStatic));
                    nri::VertexBufferDesc vbDesc = {};
                    vbDesc.buffer = primitive->vertexBuffer->GetHandle();
                    vbDesc.offset = 0;
                    vbDesc.stride = vertexStride;
                    nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                    nri.CmdSetIndexBuffer(*cmd, *primitive->indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

                    uint32_t rootConstantVal = PushConstant_ObjectIndex;
                    if constexpr (!isSkeletal)
                    {
                        rootConstantVal = frameContext->instanceIndexAllocator.Allocate(cmd, &PushConstant_ObjectIndex, 1);
                    }

                    nri::SetRootConstantsDesc rootConstDesc = {};
                    rootConstDesc.rootConstantIndex = 0;
                    rootConstDesc.data = &rootConstantVal;
                    rootConstDesc.size = sizeof(rootConstantVal);
                    rootConstDesc.offset = 0;
                    rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
                    nri.CmdSetRootConstants(*cmd, rootConstDesc);

                    const uint32_t idxCount = primitive->indexBuffer->GetCount();
                    nri::DrawIndexedDesc drawDesc = {};
                    drawDesc.indexNum = idxCount;
                    drawDesc.instanceNum = 1;
                    nri.CmdDrawIndexed(*cmd, drawDesc);

                    Renderer::Stats.drawCallCount++;
                    Renderer::Stats.indexCount3D += idxCount;
                    Renderer::Stats.vertexCount3D += primitive->vertexBuffer->GetByteSize() / sizeof(float); // approx
                }
            }
        }

        if (startedRendering)
            framebuffer->EndRendering(cmd);
    }

    template<typename MeshT>
    void SceneRenderer::DrawMeshShadow(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *csmFramebuffer,
        const Ref<MeshT> &mesh, const glm::mat4 &parentTransform, const glm::mat4 &normalMatrix, uint32_t objectID,
        const std::vector<glm::mat4> &boneTransforms, const std::vector<Mesh_GPUData> &cachedInstanceTransforms,
        const Ref<GraphicsPipeline> &csmPSO, uint32_t cascadeIndex, entt::entity entity, const std::string &socketName)
    {
        if (!csmFramebuffer || !csmPSO || !csmPSO->GetHandle())
            return;

        constexpr bool isSkeletal = std::is_same_v<MeshT, SkeletalMesh>;

        const auto &instances = mesh->GetMeshInstances();
        for (const auto &meshInstance : instances)
        {
            auto &primitive = meshInstance->GetPrimitive();
            if (primitive && (!primitive->vertexBuffer || !primitive->indexBuffer))
                primitive->WriteBuffer(cmd);
        }

        const bool startedRendering = !csmFramebuffer->IsRendering();
        if (startedRendering)
            csmFramebuffer->BeginRendering(cmd, false, false);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        for (size_t idx = 0; idx < instances.size(); ++idx)
        {
            auto &meshInstance = instances[idx];

            auto &primitive = meshInstance->GetPrimitive();
            if (!primitive || !primitive->vertexBuffer || !primitive->indexBuffer)
                continue;

            Mesh_GPUData gpuData;
            if (idx < cachedInstanceTransforms.size())
            {
                gpuData = cachedInstanceTransforms[idx];
            }
            else
            {
                glm::mat4 meshTransform = meshInstance->global;
                if constexpr (isSkeletal)
                {
                    if (meshInstance->linkedJointIndex >= 0 && !boneTransforms.empty())
                    {
                        const auto ji = static_cast<size_t>(meshInstance->linkedJointIndex);
                        if (ji < boneTransforms.size())
                        {
                            meshTransform = boneTransforms[ji] * meshTransform;
                        }
                    }
                }
                gpuData.transformation = parentTransform * meshTransform;
                gpuData.objectID = objectID;
                gpuData.normal = normalMatrix;
            }

            uint32_t PushConstant_ObjectIndex = 0;
            bool foundInCache = false;
            if (entity != entt::null)
            {
                if (!socketName.empty())
                {
                    auto key = std::make_pair(entity, socketName);
                    auto cacheIt = m_SocketObjectIndexCache.find(key);
                    if (cacheIt != m_SocketObjectIndexCache.end() && idx < cacheIt->second.size())
                    {
                        PushConstant_ObjectIndex = cacheIt->second[idx];
                        foundInCache = true;
                    }
                }
                else
                {
                    auto cacheIt = m_EntityObjectIndexCache.find(entity);
                    if (cacheIt != m_EntityObjectIndexCache.end() && idx < cacheIt->second.size())
                    {
                        PushConstant_ObjectIndex = cacheIt->second[idx];
                        foundInCache = true;
                    }
                }
            }

            if (!foundInCache)
            {
                if constexpr (isSkeletal)
                {
                    glm::mat4 bones[MAX_BONES];
                    FillBoneArray(bones, boneTransforms);
                    gpuData.boneOffset = frameContext->boneAllocator.Allocate(cmd, bones, MAX_BONES);
                }
                else
                {
                    gpuData.boneOffset = 0;
                }
                PushConstant_ObjectIndex = frameContext->objectAllocator.Allocate(cmd, gpuData);
            }

            Ref<BindingSet> meshBindingSet = frameContext->staticMeshCSMBindingSet[cascadeIndex];
            if constexpr (isSkeletal)
            {
                meshBindingSet = frameContext->animatedMeshCSMBindingSet[cascadeIndex];
            }

            if (meshBindingSet && meshBindingSet->GetHandle())
            {
                nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *csmPSO->GetPipelineLayout());
                nri.CmdSetPipeline(*cmd, *csmPSO->GetHandle());

                nri::SetDescriptorSetDesc set0 = { 0, meshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri.CmdSetDescriptorSet(*cmd, set0);

                constexpr uint32_t vertexStride = isSkeletal ? static_cast<uint32_t>(sizeof(VertexMeshAnim)) : static_cast<uint32_t>(sizeof(VertexMeshStatic));
                nri::VertexBufferDesc vbDesc = {};
                vbDesc.buffer = primitive->vertexBuffer->GetHandle();
                vbDesc.offset = 0;
                vbDesc.stride = vertexStride;
                nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                nri.CmdSetIndexBuffer(*cmd, *primitive->indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

                uint32_t rootConstantVal = PushConstant_ObjectIndex;
                if constexpr (!isSkeletal)
                {
                    rootConstantVal = frameContext->instanceIndexAllocator.Allocate(cmd, &PushConstant_ObjectIndex, 1);
                }

                nri::SetRootConstantsDesc rootConstDesc = {};
                rootConstDesc.rootConstantIndex = 0;
                rootConstDesc.data = &rootConstantVal;
                rootConstDesc.size = sizeof(rootConstantVal);
                rootConstDesc.offset = 0;
                rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
                nri.CmdSetRootConstants(*cmd, rootConstDesc);

                const uint32_t idxCount = primitive->indexBuffer->GetCount();
                nri::DrawIndexedDesc drawDesc = {};
                drawDesc.indexNum = idxCount;
                drawDesc.instanceNum = 1;
                nri.CmdDrawIndexed(*cmd, drawDesc);

                Renderer::Stats.shadowDrawCallCount++;
            }
        }

        if (startedRendering)
            csmFramebuffer->EndRendering(cmd);
    }

    // Explicit template instantiations
    template void SceneRenderer::DrawMesh<StaticMesh>(
        nri::CommandBuffer *, FrameContext *, RenderTarget *, const Ref<StaticMesh> &, const glm::mat4 &, const glm::mat4 &, uint32_t,
        const std::unordered_map<int, AssetHandle> &, const std::vector<glm::mat4> &, const std::vector<Mesh_GPUData> &,
        ICamera *, Ref<GraphicsPipeline>, std::vector<TransparentDrawCall> &, std::unordered_set<Material *> &, entt::entity, const std::string &);

    template void SceneRenderer::DrawMesh<SkeletalMesh>(
        nri::CommandBuffer *, FrameContext *, RenderTarget *, const Ref<SkeletalMesh> &, const glm::mat4 &, const glm::mat4 &, uint32_t,
        const std::unordered_map<int, AssetHandle> &, const std::vector<glm::mat4> &, const std::vector<Mesh_GPUData> &,
        ICamera *, Ref<GraphicsPipeline>, std::vector<TransparentDrawCall> &, std::unordered_set<Material *> &, entt::entity, const std::string &);

    template void SceneRenderer::DrawMeshShadow<StaticMesh>(
        nri::CommandBuffer *, FrameContext *, RenderTarget *, const Ref<StaticMesh> &, const glm::mat4 &, const glm::mat4 &, uint32_t,
        const std::vector<glm::mat4> &, const std::vector<Mesh_GPUData> &, const Ref<GraphicsPipeline> &, uint32_t, entt::entity, const std::string &);

    template void SceneRenderer::DrawMeshShadow<SkeletalMesh>(
        nri::CommandBuffer *, FrameContext *, RenderTarget *, const Ref<SkeletalMesh> &, const glm::mat4 &, const glm::mat4 &, uint32_t,
        const std::vector<glm::mat4> &, const std::vector<Mesh_GPUData> &, const Ref<GraphicsPipeline> &, uint32_t, entt::entity, const std::string &);
}
