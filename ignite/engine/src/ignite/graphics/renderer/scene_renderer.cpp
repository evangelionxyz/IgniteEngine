// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "renderer_2d.hpp"
#include "scene_renderer.hpp"
#include "ignite/graphics/bindless_system.hpp"
#include "ignite/graphics/binding_cache.hpp"
#include "ignite/scene/icamera.hpp"
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
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/texture.hpp"

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
        auto compositeSamplerDesc = nvrhi::SamplerDesc();
        compositeSamplerDesc.setAllFilters(true);
        compositeSamplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_CompositeSampler = m_Device->createSampler(compositeSamplerDesc);

        m_Renderer2D = Renderer2D::Create();
        m_EdgeDetection = EdgeDetection::Create();
		m_EdgeDetection->CreatePipeline();

        m_CascadedShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::HIGH);
        m_RuntimeMaterial   = CreateRef<Material>();
    }

    SceneRenderer::~SceneRenderer()
    {
        m_WidgetRenderer = nullptr;
        m_WorldEnvironment = nullptr;

        m_AnimatedPSOCache.clear();
        m_EnvironmentPSOCache.clear();
        m_CompositePSOCache.clear();
        m_DebugGridPSOCache.clear();
        m_CompositeBindingSetCache.clear();
        m_DebugGridBindingSetCache.clear();
        m_RenderTargets.clear();

        m_ResolvedAssetsCache.clear();
        m_RuntimeMaterial.reset();
    }

    void SceneRenderer::SetActiveScene(const Ref<Scene> &scene)
    {
        if (m_Scene == scene)
        {
            return;
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
        m_EnvironmentPSOCache.clear();
        m_CompositePSOCache.clear();
        m_DebugGridPSOCache.clear();
        m_CompositeBindingSetCache.clear();
        m_DebugGridBindingSetCache.clear();
        if (auto *device = DeviceManager::GetInstance()->GetDevice())
        {
            GPUUploadSync::DeviceWaitIdle(device);
        }
        m_RenderTargets.clear();
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
        m_Has2DPreRenderCache = false;
        if (m_Renderer2D)
        {
            m_Renderer2D->InvalidatePreRenderCache();
        }
    }

    void SceneRenderer::Render(ICamera *camera, FrameContext *frameContext, bool drawDebug)
    {
        IGN_PROFILE_FUNCTION();

        if (!camera)
            return;

        Ref<CameraRenderTarget> target = GetOrCreateRenderTarget(camera);
        if (!target)
            return;

        EnsureSceneEnvironmentMap();

        // Create fresh command list for this frame
        nvrhi::CommandListHandle cmd = m_Device->createCommandList();
        {
            IGN_PROFILE_SCOPE("SceneRenderer::RecordEditorCommandList");
            cmd->open();

            // Scene post processing
            PostProcessing postProcessing = camera->postProcessing;
            CameraLens cameraLens = camera->lens;
            if (Entity primaryCamera = m_Scene->GetPrimaryCamera())
            {
                const auto &cc = primaryCamera.GetComponent<CameraComponent>();
				postProcessing = cc.camera.postProcessing;
				cameraLens = cc.camera.lens;
            }

            postProcessing.taaProperties.enable = postProcessing.taaProperties.enable || sceneRenderSettings.taaProperties.enable;
            postProcessing.taaProperties.blendFactor = sceneRenderSettings.taaProperties.enable ? sceneRenderSettings.taaProperties.blendFactor : postProcessing.taaProperties.blendFactor;
            postProcessing.msaaProperties.enable = postProcessing.msaaProperties.enable || sceneRenderSettings.msaaProperties.enable;
            postProcessing.msaaProperties.sampleCount = sceneRenderSettings.msaaProperties.enable ? sceneRenderSettings.msaaProperties.sampleCount : postProcessing.msaaProperties.sampleCount;
            postProcessing.renderScale = glm::clamp(postProcessing.renderScale * sceneRenderSettings.renderScale, 0.25f, 1.0f);

            // IMPORTANT!!!!
            // Write frame context buffers before using them
            glm::mat4 projection = camera->GetProjection();
            if (postProcessing.taaProperties.enable)
            {
                const glm::vec2 jitter = GetTAAJitter(m_TAAFrameIndex,
                    target->sceneRT->GetWidth(),
                    target->sceneRT->GetHeight());
                projection[2][0] += jitter.x;
                projection[2][1] += jitter.y;
            }
            CameraBufferData cameraData = { projection, camera->GetView(), glm::vec4(camera->position, 1.0f) };
            {
				IGN_PROFILE_SCOPE("SceneRenderer::WriteFrameData");
			    frameContext->cameraBuffer.SetData(cmd, &cameraData, sizeof(cameraData));
			    frameContext->sceneBuffer.SetData(cmd, &m_SceneGPUData, sizeof(m_SceneGPUData));
            }

            PreallocateGPUData(cmd, frameContext);

			if (m_WorldEnvironment && m_WorldEnvironment->environment && !m_WorldEnvironment->gpuInitialized && !m_WorldEnvironment->dirtyEnvironment)
			{
				m_WorldEnvironment->environment->WriteBuffer(cmd);
				m_WorldEnvironment->gpuInitialized = true;
			}

            // Clear Render Targets
            // far depth = 1.0f == LessOrEqual
            {
                target->widgetRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                target->widgetRT->ClearDepthAttachment(cmd, 1.0f, 0);

                target->sceneRT->ClearColorAttachmentFloat(cmd, 0);
                target->sceneRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
                target->sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);

                target->compositeRT->ClearColorAttachmentFloat(cmd, 0);
                target->compositeRT->ClearDepthAttachment(cmd, 1.0f, 0);

                target->debugRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
                target->debugRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
            }

            nvrhi::IFramebuffer *sceneFramebuffer = target->sceneRT->GetFramebuffer();

            ShadowPass(cmd, camera, frameContext);

            if (m_WorldEnvironment && m_WorldEnvironment->environment && !m_WorldEnvironment->dirtyEnvironment)
            {
                const Ref<GraphicsPipeline> envPSO = GetEnvironmentPSO(sceneFramebuffer, m_FillMode);
                m_WorldEnvironment->environment->Draw(cmd, sceneFramebuffer, envPSO,
                    frameContext->cameraBuffer.GetHandle(), frameContext->sceneBuffer.GetHandle());
            }

            ColorPass(cmd, camera, frameContext, sceneFramebuffer, drawDebug);
            UIPass(cmd, target->widgetRT->GetFramebuffer(), frameContext);

            if (drawDebug)
            {
                nvrhi::IFramebuffer *debugFramebuffer = target->debugRT->GetFramebuffer().Get();
				DebugPass(cmd, camera, debugFramebuffer, frameContext);
            }
            
            // Transition color and depth attachments to ShaderResource before they are read by post-processing
            cmd->setTextureState(target->sceneRT->GetColorAttachment(0)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
            cmd->setTextureState(target->sceneRT->GetColorAttachment(1)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
			cmd->setTextureState(target->sceneRT->GetDepthAttachment()->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

			cmd->setTextureState(target->widgetRT->GetColorAttachment(0)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
			cmd->setTextureState(target->debugRT->GetColorAttachment(0)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
			if (target->debugRT->GetColorAttachment(1))
			{
				cmd->setTextureState(target->debugRT->GetColorAttachment(1)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
			}

            cmd->commitBarriers();

            // MSAA resolve: if the scene was rendered with multiple samples, resolve to a
            // single-sample texture so post-processing passes can sample it normally.
            const bool msaaActive = target->msaaSampleCount > 1 && target->sceneResolvedRT;
            if (msaaActive)
            {
                IGN_PROFILE_SCOPE("SceneRenderer::MSAAResolve");
                Ref<Texture> msaaColor  = target->sceneRT->GetColorAttachment(0);
                Ref<Texture> msaaObjID  = target->sceneRT->GetColorAttachment(1);
                Ref<Texture> resolvedColor = target->sceneResolvedRT->GetColorAttachment(0);
                Ref<Texture> resolvedObjID = target->sceneResolvedRT->GetColorAttachment(1);

                cmd->setTextureState(resolvedColor->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ResolveDest);
                cmd->setTextureState(resolvedObjID->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ResolveDest);
                cmd->commitBarriers();

                cmd->resolveTexture(resolvedColor->GetHandle(), nvrhi::TextureSubresourceSet(), msaaColor->GetHandle(), nvrhi::TextureSubresourceSet());
                cmd->resolveTexture(resolvedObjID->GetHandle(), nvrhi::TextureSubresourceSet(), msaaObjID->GetHandle(), nvrhi::TextureSubresourceSet());

                cmd->setTextureState(resolvedColor->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
                cmd->setTextureState(resolvedObjID->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
                cmd->commitBarriers();
            }

			const auto width = target->sceneRT->GetWidth();
			const auto height = target->sceneRT->GetHeight();

            // Differentiate between Edit Viewport and Game Viewport cameras.
            // This is crucial to prevent resources (Bloom, SSAO, Edge Detection) from fighting
            // and constantly recreating textures when both viewports are active with different sizes.
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

            // Skip selection highlights/outlines for the game camera (gameplay viewport).
            // This avoids running EdgeDetection compute shaders and recreating the outline texture for the game's resolution.
            Ref<Texture> edgeTexture = nullptr;
            if (!isGameCamera && m_EdgeDetection && !m_SelectedEntities.empty())
            {
                if (!m_EdgeDetection->GetOutputTexture() || m_EdgeDetection->GetOutputTexture()->GetWidth() != static_cast<int>(width) || m_EdgeDetection->GetOutputTexture()->GetHeight() != static_cast<int>(height))
                {
                    m_EdgeDetection->CreateOutputTexture(width, height);
                }

                // Use the resolved (single-sample) color and object-ID textures for edge detection
                Ref<Texture> edgeColorSrc  = (msaaActive && target->sceneResolvedRT) ? target->sceneResolvedRT->GetColorAttachment(0) : target->sceneRT->GetColorAttachment(0);
                Ref<Texture> edgeObjIDSrc  = (msaaActive && target->sceneResolvedRT) ? target->sceneResolvedRT->GetColorAttachment(1) : target->sceneRT->GetColorAttachment(1);
                m_EdgeDetection->UpdateBindingSet(edgeColorSrc, edgeObjIDSrc, target->sceneRT->GetDepthAttachment());

                constexpr uint32_t kMaxSelectedIDs = 100;
                const uint32_t selectedCount = static_cast<uint32_t>(std::min(m_SelectedEntities.size(), static_cast<size_t>(kMaxSelectedIDs)));
                if (selectedCount > 0)
                {
                    cmd->writeBuffer(m_EdgeDetection->GetSelectedIDBuffer(), m_SelectedEntities.data(), sizeof(uint32_t) * selectedCount);

                    EdgeDetectionParameter edgeParams;
                    edgeParams.texelSize = glm::vec2(1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
                    edgeParams.edgeThreshold = 0.1f;
                    edgeParams.outlineWidth = 2.5f;
                    edgeParams.outlineColor = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);
                    edgeParams.depthSensitivity = 25.0f;
                    edgeParams.useObjectID = 1;
                    edgeParams.selectedCount = selectedCount;

                    m_EdgeDetection->ExecuteCompute(cmd, edgeParams, width, height);
                    edgeTexture = m_EdgeDetection->GetOutputTexture();
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
				bloomInstance->Resize(width, height);
				// Bloom reads from the resolved (single-sample) texture
				Ref<Texture> bloomSrc = (msaaActive && target->sceneResolvedRT) ? target->sceneResolvedRT->GetColorAttachment(0) : target->sceneRT->GetColorAttachment(0);
				bloomInstance->Build(cmd, bloomSrc, m_CompositeVertexBuffer);
				bloomTexture = bloomInstance->GetBloomTexture();
            }

            Ref<Texture> ssaoTexture = nullptr;
            if (postProcessing.enableSSAO)
            {
                IGN_PROFILE_SCOPE_COLOR("SceneRenderer::HBAOPass", 0x404040FF);
				const auto &SSAOs = GetOrCreateSSAOs(camera);
                Ref<SSAO> ssaoInstance = SSAOs[frameContext->frameIndexInFlight];
                ssaoInstance->Resize(width, height);
                ssaoInstance->Build(cmd, target->sceneRT->GetDepthAttachment(), camera, postProcessing, m_CompositeVertexBuffer);
                ssaoTexture = ssaoInstance->GetAOTexture();
            }

            {
                IGN_PROFILE_SCOPE("SceneRenderer::CompositePass");
                CompositePass(cmd, camera, frameContext, target, cameraLens, postProcessing, edgeTexture, bloomTexture, ssaoTexture, msaaActive);
            }

            if (postProcessing.taaProperties.enable && target->taaHistoryRT[frameContext->frameIndexInFlight])
            {
                IGN_PROFILE_SCOPE("SceneRenderer::TAAHistoryCopy");
                Ref<Texture> compositeColor = target->compositeRT->GetColorAttachment(0);
                Ref<Texture> historyColor = target->taaHistoryRT[frameContext->frameIndexInFlight]->GetColorAttachment(0);
                cmd->setTextureState(compositeColor->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::CopySource);
                cmd->setTextureState(historyColor->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::CopyDest);
                cmd->commitBarriers();
                cmd->copyTexture(historyColor->GetHandle(), nvrhi::TextureSlice(), compositeColor->GetHandle(), nvrhi::TextureSlice());
                cmd->setTextureState(historyColor->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
                cmd->setTextureState(compositeColor->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
                cmd->commitBarriers();
                target->taaHistoryValid = true;
                ++m_TAAFrameIndex;
            }

            cmd->close();
        }

        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            m_Device->executeCommandList(cmd);
        }
    }

    void SceneRenderer::PreallocateGPUData(nvrhi::ICommandList *cmd, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();

        m_EntityObjectIndexCache.clear();
        m_EntityBoneOffsetCache.clear();
        m_SocketObjectIndexCache.clear();

        // 1. Preallocate and upload Skeletal Meshes (including bone data)
        auto skeletalMeshView = m_Scene->registry->view<TransformComponent, SkeletalMeshComponent>();
        for (entt::entity e : skeletalMeshView)
        {
            const auto &[tr, smc] = m_Scene->registry->get<TransformComponent, SkeletalMeshComponent>(e);
            if (!tr.visible || smc.handle == AssetHandle(0))
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
        auto staticMeshView = m_Scene->registry->view<TransformComponent, StaticMeshComponent>();
        for (entt::entity e : staticMeshView)
        {
            const auto &[tr, smc] = m_Scene->registry->get<TransformComponent, StaticMeshComponent>(e);
            if (!tr.visible || smc.handle == AssetHandle(0))
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
    }

    void SceneRenderer::ResizeFramebuffer(ICamera *camera, uint32_t width, uint32_t height)
    {
		IGN_PROFILE_FUNCTION();

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
				auto target = it->second;

				target->sceneRT->Resize(renderWidth, renderHeight);
				if (target->sceneResolvedRT)
					target->sceneResolvedRT->Resize(renderWidth, renderHeight);
				target->widgetRT->Resize(width, height);
				target->compositeRT->Resize(width, height);
				for (Ref<RenderTarget> &historyRT : target->taaHistoryRT)
				{
					if (historyRT)
						historyRT->Resize(width, height);
				}
				target->taaHistoryValid = false;

				target->debugRT->GetCreateInfo().depthAttachmentOverride = target->sceneRT->GetDepthAttachment();
				target->debugRT->Resize(renderWidth, renderHeight);
			}
        }
        
    }

    void SceneRenderer::ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera, FrameContext *frameContext)
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
            break;
        }

        // Collect point lights
        {
            PointLightBufferData pointLightData = {};
            int pointLightCount = 0;

            auto pointLightView = m_Scene->registry->view<TransformComponent, PointLightComponent>();
            for (entt::entity e : pointLightView)
            {
                if (pointLightCount >= MAX_POINT_LIGHTS)
                    break;

                const TransformComponent &tr = pointLightView.get<TransformComponent>(e);
                const PointLightComponent &light = pointLightView.get<PointLightComponent>(e);

                if (!tr.visible || !light.enabled)
                    continue;

                PointLight_GPUData &gpu = pointLightData.lights[pointLightCount];
                gpu.positionAndRange = glm::vec4(tr.world.translation, light.range);
                gpu.color = glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);
                gpu.attenuation = glm::vec4(light.constantAttenuation, light.linearAttenuation, light.quadraticAttenuation, 0.0f);
                ++pointLightCount;
            }

            m_SceneGPUData.numPointLights = pointLightCount;
			frameContext->pointLightBuffer.SetData(cmd, &pointLightData, sizeof(PointLight_GPUData));
        }

        // Collect spot lights
        {
            SpotLightBufferData spotLightData = {};
            int spotLightCount = 0;

            auto spotLightView = m_Scene->registry->view<TransformComponent, SpotLightComponent>();
            for (entt::entity e : spotLightView)
            {
                if (spotLightCount >= MAX_SPOT_LIGHTS)
                    break;

                const TransformComponent &tr = spotLightView.get<TransformComponent>(e);
                const SpotLightComponent &light = spotLightView.get<SpotLightComponent>(e);

                if (!tr.visible || !light.enabled)
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
			frameContext->spotLightBuffer.SetData(cmd, &spotLightData, sizeof(SpotLight_GPUData));
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

            const auto quality = static_cast<ShadowMapQuality>(light.shadowResolution);
            if (m_CascadedShadowMap->GetQuality() != quality)
            {
                m_CascadedShadowMap->Resize(quality);

                Application::SubmitToMainThread([]()
                {
                    SignalBus::Emit(AssetChangeSignal{ AssetHandle(0), AssetType::Environment });
                });
            }

            break;
        }

		// If no directional light is found, we still need to write default cascade data to the frame context buffer
		if (!cascadeShadow)
		{
            CSM_GPUData sceneCascadeData = {};
            sceneCascadeData.cascadeIndex = -1;
            sceneCascadeData.shadowStrength = 0.0f;
			// Write the cascade data to the frame context buffer for use in the main scene pass
			frameContext->csmBuffer.SetData(cmd, &sceneCascadeData, sizeof(sceneCascadeData));

            for (int i = 0; i < NUM_CASCADES; ++i)
            {
                IGN_PROFILE_SCOPE("Prefetch per-cascaded GPU data");
                CSM_GPUData cascadeGpuData = {};
                cascadeGpuData.cascadeIndex = i;
                frameContext->csmPerCascadeBuffers[i].SetData(cmd, &cascadeGpuData, sizeof(cascadeGpuData));
                m_CascadedShadowMap->BeginCascade(cmd, i, frameContext->frameIndexInFlight);
            }
		}
        else
        {
			m_CascadedShadowMap->ComputeMatrices(camera, sunDirection, shadowDist);

			// Share cascade data with the main scene pass (cascadeIndex is unused there)
			CSM_GPUData sceneCascadeData = m_CascadedShadowMap->GetGPUData();
			sceneCascadeData.cascadeIndex = -1;

			// Write the cascade data to the frame context buffer for use in the main scene pass
			frameContext->csmBuffer.SetData(cmd, &sceneCascadeData, sizeof(sceneCascadeData));

			for (int i = 0; i < NUM_CASCADES; ++i)
			{
				IGN_PROFILE_SCOPE("Prefetch per-cascaded GPU data");

				CSM_GPUData cascadeGpuData = sceneCascadeData;
				cascadeGpuData.cascadeIndex = i;
				frameContext->csmPerCascadeBuffers[i].SetData(cmd, &cascadeGpuData, sizeof(cascadeGpuData));

				// Clear the specific array layer for this cascade
				m_CascadedShadowMap->BeginCascade(cmd, i, frameContext->frameIndexInFlight);

				nvrhi::IFramebuffer *csmFramebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(i, frameContext->frameIndexInFlight);
				nvrhi::Viewport viewport = csmFramebuffer->getFramebufferInfo().getViewport();

				Frustum cascadeFrustum(cascadeGpuData.lightViewProj[i]);

				nvrhi::GraphicsState staticState = nvrhi::GraphicsState();
				staticState.framebuffer = csmFramebuffer;
				staticState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(viewport);
				staticState.pipeline = staticCSMPSO->GetHandle();

				// Static Mesh
				{
					IGN_PROFILE_SCOPE("SceneRenderer::StaticMeshShadow");

					auto skelMeshView = m_Scene->registry->view<TransformComponent, StaticMeshComponent>();
					for (entt::entity e : skelMeshView)
					{
						TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
						if (!tr.visible)
							continue;

						StaticMeshComponent &smc = m_Scene->registry->get<StaticMeshComponent>(e);
						if (smc.handle == AssetHandle(0))
							continue;

						auto skeletalMesh = ResolveAsset<StaticMesh>(smc.handle);
						if (!skeletalMesh)
							continue;

						// Perform cascade frustum culling
						if (!cascadeFrustum.IsAABBVisible(smc.worldAABB))
							continue;

						const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
						DrawMeshShadow(cmd, frameContext, skeletalMesh, tr.world.GetMatrix(), smc.normalMatrix, objectID, std::vector<glm::mat4>(),
							smc.cachedInstanceTransforms, staticState, i, e);
					}
				}

				nvrhi::GraphicsState animatedState = nvrhi::GraphicsState();
				animatedState.framebuffer = csmFramebuffer;
				animatedState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(viewport);
				animatedState.pipeline = animatedCSMPSO->GetHandle();

				// Skeletal Mesh
				{
					IGN_PROFILE_SCOPE("SceneRenderer::SkeletalMeshShadow");

					auto skelMeshView = m_Scene->registry->view<TransformComponent, SkeletalMeshComponent>();
					for (entt::entity e : skelMeshView)
					{
						TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
						if (!tr.visible)
							continue;

						SkeletalMeshComponent &smc = m_Scene->registry->get<SkeletalMeshComponent>(e);
						if (smc.handle == AssetHandle(0))
							continue;

						auto skeletalMesh = ResolveAsset<SkeletalMesh>(smc.handle);
						if (!skeletalMesh)
							continue;

						// Perform cascade frustum culling
						if (!cascadeFrustum.IsAABBVisible(smc.worldAABB))
							continue;

						const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
						DrawMeshShadow(cmd, frameContext, skeletalMesh, tr.world.GetMatrix(), smc.normalMatrix, objectID,
                            smc.finalBoneTransforms, smc.cachedInstanceTransforms, animatedState, i, e);

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
										DrawMeshShadow(cmd, frameContext, attachedMesh, socketWorld, normalMatrix, objectID,
											smc.finalBoneTransforms, std::vector<Mesh_GPUData>(), animatedState, i, e, socketName);
									}
									else if (attachedMeshAsset->GetAssetType() == AssetType::StaticMesh)
									{
										auto attachedMesh = attachedMeshAsset->As<StaticMesh>();
										DrawMeshShadow(cmd, frameContext, attachedMesh, socketWorld, normalMatrix, objectID,
											std::vector<glm::mat4>(), std::vector<Mesh_GPUData>(), staticState, i, e, socketName);
									}
								}
							}
						}
					}
				}
			}
			
        }

		cmd->setTextureState(m_CascadedShadowMap->GetDepthTexture(frameContext->frameIndexInFlight)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		cmd->commitBarriers();
    }

    void SceneRenderer::ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, FrameContext *frameContext, nvrhi::IFramebuffer *framebuffer, bool drawDebug)
    {
        IGN_PROFILE_FUNCTION();

        Frustum frustum(camera);

        std::unordered_set<Material *> uploadedMaterialsThisPass;
        
        if (m_RuntimeMaterial)
        {
            m_RuntimeMaterial->UpdateBindingSet(GetEnvironmentMapColorTexture(), GetCascadedShadowMapDepthTexture());
        }

        // Static
		auto staticPSO = GetStaticPSO(framebuffer, m_FillMode);
		auto staticTransparentPSO = GetStaticTransparentPSO(framebuffer, m_FillMode);

        // Animated
        auto animatedPSO = GetAnimatedPSO(framebuffer, m_FillMode);
        auto animatedTransparentPSO = GetAnimatedTransparentPSO(framebuffer, m_FillMode);

        std::vector<TransparentDrawCall> transparentDrawCalls;

        // Static Meshes
        {
			IGN_PROFILE_SCOPE("SceneRenderer::SkeletalMeshColor");

            auto staticMeshView = m_Scene->registry->view<TransformComponent, StaticMeshComponent>();
            for (entt::entity e : staticMeshView)
            {
                const auto &[tr, smc] = m_Scene->registry->get<TransformComponent, StaticMeshComponent>(e);
                if (!tr.visible || smc.handle == AssetHandle(0))
                    continue;

                auto staticMesh = ResolveAsset<StaticMesh>(smc.handle);
                if (!staticMesh)
                    continue;

                if (!frustum.IsAABBVisible(smc.worldAABB))
                    continue;

                const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
                DrawMesh(cmd, frameContext, framebuffer, staticMesh, tr.world.GetMatrix(), smc.normalMatrix, objectID, 
                    smc.overrideMaterials, std::vector<glm::mat4>(), smc.cachedInstanceTransforms, 
                    camera, staticPSO, transparentDrawCalls, uploadedMaterialsThisPass, e);
                Renderer::Stats.staticMeshCount++;
            }
        }

        // Skeletal Meshes
        {
            IGN_PROFILE_SCOPE("SceneRenderer::SkeletalMeshColor");

            auto skelMeshView = m_Scene->registry->view<TransformComponent, SkeletalMeshComponent>();
            for (entt::entity e : skelMeshView)
            {
                const auto &[tr, smc] = m_Scene->registry->get<TransformComponent, SkeletalMeshComponent>(e);
                if (!tr.visible || smc.handle == AssetHandle(0))
                    continue;

                auto skeletalMesh = ResolveAsset<SkeletalMesh>(smc.handle);
                if (!skeletalMesh)
                    continue;

                if (!frustum.IsAABBVisible(smc.worldAABB))
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
            std::ranges::sort(transparentDrawCalls, [](const TransparentDrawCall &a, const TransparentDrawCall &b) { return a.distanceToCamera > b.distanceToCamera; });

            nvrhi::GraphicsState transparentGState = nvrhi::GraphicsState();
            transparentGState.framebuffer = framebuffer;
            transparentGState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

            for (const auto &dc : transparentDrawCalls)
            {
                // OLD mesh instance data update
                // dc.meshInstance->SetData(cmd, (void*)&dc.gpuData, sizeof(Mesh_GPUData));
                if (dc.isSkeletal)
                {
                    auto skelInstance = std::static_pointer_cast<SkeletalMeshInstance>(dc.meshInstance);
					// OLD mesh instance data update
                    // skelInstance->SetSkeletonData(cmd, (void*)dc.bones, sizeof(dc.bones));
                }

                auto &pipeline = dc.isSkeletal ? animatedTransparentPSO : staticTransparentPSO;
                transparentGState.pipeline = pipeline->GetHandle();

                transparentGState.bindings = { dc.meshBindingSet, dc.materialBindingSet, BindlessSystem::GetDescriptorTable() };
                transparentGState.vertexBuffers = { nvrhi::VertexBufferBinding{ dc.vertexBuffer, 0, 0 } };
                transparentGState.setIndexBuffer({ dc.indexBuffer, nvrhi::Format::R32_UINT });

                cmd->setGraphicsState(transparentGState);

				// Push constants for object ID
				cmd->setPushConstants(&dc.pushConstants_ObjectIndex, sizeof(dc.pushConstants_ObjectIndex));

                nvrhi::DrawArguments args;
                args.setVertexCount(dc.indexCount);
                args.instanceCount = 1;

                cmd->drawIndexed(args);
                Renderer::Stats.drawCallCount++;
                Renderer::Stats.indexCount3D += dc.indexCount;
            }
        }

        // 2D Pass
        {
            std::vector<PointLight2D_GPUData> pointLights2D;
            {
                IGN_PROFILE_SCOPE("SceneRenderer::2DPass::PointLightsView");

                auto pointLight2DView = m_Scene->registry->view<TransformComponent, PointLight2DComponent>();
                for (entt::entity e : pointLight2DView)
                {
                    const auto &[tr, light] = m_Scene->registry->get<TransformComponent, PointLight2DComponent>(e);

                    if (!tr.visible || !light.enabled)
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

                auto circle2DView = m_Scene->registry->view<TransformComponent, Circle2DComponent>();
                for (entt::entity e : circle2DView)
                {
                    const auto &[tr, circle] = m_Scene->registry->get<TransformComponent, Circle2DComponent>(e);
                    
                    if (!tr.visible)
                        continue;

					const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
					m_Renderer2D->DrawCircle(tr.world.GetMatrix(), circle.color, circle.thickness, circle.fade, objectID);
                }
            }
            {
                IGN_PROFILE_SCOPE("SceneRenderer::2DPass::Quad2DView");
                Project *project = m_Scene->GetProject();
                auto quad2DView = m_Scene->registry->view<TransformComponent, Sprite2DComponent>();
                for (entt::entity e : quad2DView)
                {
                    const auto &[tr, sprite] = m_Scene->registry->get<TransformComponent, Sprite2DComponent>(e);
                    
                    if (!tr.visible)
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

                auto textView = m_Scene->registry->view<TransformComponent, TextComponent>();
                for (entt::entity e : textView)
                {
                    const auto &[tr, text] = m_Scene->registry->get<TransformComponent, TextComponent>(e);
                    if (!tr.visible || text.fontHandle == AssetHandle(0) || text.text.empty())
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

            m_Renderer2D->Flush(framebuffer, frameContext->cameraBuffer.GetHandle());
            // m_Renderer2D->BuildPreRenderCache();
            m_Has2DPreRenderCache = true;
            m_Renderer2D->End();
        }
    }

    void SceneRenderer::UIPass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();

        // Set root widget
        auto rootWidget = m_Scene->GetRootWidget();
        if (!rootWidget)
            return;

        m_WidgetRenderer->SetActiveWidget(rootWidget);

        const nvrhi::Viewport viewport = framebuffer->getFramebufferInfo().getViewport();
        const uint32_t width = std::max(1u, static_cast<uint32_t>(viewport.maxX - viewport.minX));
        const uint32_t height = std::max(1u, static_cast<uint32_t>(viewport.maxY - viewport.minY));
        if (m_WidgetRenderer->GetWidth() != width || m_WidgetRenderer->GetHeight() != height)
        {
            m_WidgetRenderer->Resize(width, height);
        }

        // const bool isGameplayFramebuffer = framebuffer == m_GameplayWidgetRT->GetFramebuffer();
        // const bool useMouseOverride = isGameplayFramebuffer ? m_UseGameplayWidgetMouseOverride : m_UseEditorWidgetMouseOverride;
        // const bool isHovered = isGameplayFramebuffer ? m_GameplayWidgetMouseHovered : m_EditorWidgetMouseHovered;
        // const uint32_t mouseX = isGameplayFramebuffer ? m_GameplayWidgetMouseX : m_EditorWidgetMouseX;
        // const uint32_t mouseY = isGameplayFramebuffer ? m_GameplayWidgetMouseY : m_EditorWidgetMouseY;
        // const glm::ivec2 mousePos = InputSystem::GetMousePosition();

        // if (useMouseOverride)
        // {
        //     if (isHovered)
        //     {
        //         m_WidgetRenderer->SetMousePosition(mouseX, mouseY);
        //     }
        //     else
        //     {
        //         const uint32_t offscreen = std::numeric_limits<uint32_t>::max() / 2u;
        //         m_WidgetRenderer->SetMousePosition(offscreen, offscreen);
        //     }
        // }
        // else
        // {
        //     m_WidgetRenderer->SetMousePosition(
        //         static_cast<uint32_t>(std::max(mousePos.x, 0)),
        //         static_cast<uint32_t>(std::max(mousePos.y, 0)));
        // }
        // m_WidgetRenderer->Update(0.0f);
        // m_WidgetRenderer->Render(cmd, framebuffer);
    }

	void SceneRenderer::DebugPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext)
	{
        DrawDebug2D(cmd, framebuffer, frameContext);
        DrawDebug3D(cmd, camera, framebuffer, frameContext);
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

    void SceneRenderer::DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext, const DebugGridStyle &style, bool is2D)
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
        m_DebugGridBuffer.SetData(cmd, &gpuData, sizeof(gpuData));

        // Bind buffer
        Ref<GraphicsPipeline> gridPipeline = GetDebugGridPSO(framebuffer);
        nvrhi::BindingSetHandle bindingSet = GetOrCreateDebugGridBindingSet(gridPipeline->GetBindingLayout(0),
            frameContext->cameraBuffer.GetHandle(), m_DebugGridBuffer.GetHandle());

        auto graphicsState = nvrhi::GraphicsState();
        graphicsState.pipeline = gridPipeline->GetHandle();
        graphicsState.framebuffer = framebuffer;
        graphicsState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
        graphicsState.bindings = { bindingSet };

        cmd->setGraphicsState(graphicsState);

        nvrhi::DrawArguments args;
        args.instanceCount = 1;
        args.vertexCount = 6;
        cmd->draw(args);
    }

    void SceneRenderer::DrawDebug2D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();
        
		if (!framebuffer)
			return;

        m_Renderer2D->Begin(cmd);

        // 2D Physics debug draw
        constexpr glm::vec4 kPhysicsDebugColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
        constexpr int kCircleDebugSegments = 24;
        constexpr float kTwoPi = 6.28318530718f;

        if (sceneRenderSettings.showPhysicsCollider)
        {
            auto boxCollider2DView = m_Scene->registry->view<TransformComponent, BoxCollider2DComponent>();
            for (entt::entity e : boxCollider2DView)
            {
                const auto &[tr, box] = m_Scene->registry->get<TransformComponent, BoxCollider2DComponent>(e);
                
                if (!tr.visible)
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

            auto circleCollider2DView = m_Scene->registry->view<TransformComponent, CircleCollider2DComponent>();
            for (entt::entity e : circleCollider2DView)
            {
                const auto &[tr, circle] = m_Scene->registry->get<TransformComponent, CircleCollider2DComponent>(e);
                
                if (!tr.visible)
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
        m_Renderer2D->Flush(framebuffer, frameContext->cameraBuffer.GetHandle());
        m_Renderer2D->End();
    }

    void SceneRenderer::DrawDebug3D(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();

		if (!framebuffer || !m_Scene || !m_Scene->registry)
			return;

        if (sceneRenderSettings.showPhysicsCollider)
        {
            m_Renderer2D->Begin(cmd);

			constexpr glm::vec4 kPhysicsDebugColor = glm::vec4(0.5f, 1.0f, 1.0f, 1.0f);
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

            constexpr glm::vec4 colliderColor(0.2f, 0.9f, 0.2f, 1.0f);

            // 1. Box Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, BoxColliderComponent>())
            {
                const auto &[tr, box] = m_Scene->registry->get<TransformComponent, BoxColliderComponent>(e);
                glm::mat4 boxTransform = tr.world.GetMatrix()
                    * glm::translate(glm::mat4(1.0f), box.center)
                    * glm::scale(glm::mat4(1.0f), box.scale * 2.0f);
                m_Renderer2D->DrawBox(boxTransform, colliderColor);
            }

            // 2. Sphere Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, SphereColliderComponent>())
            {
                const auto &[tr, sphere] = m_Scene->registry->get<TransformComponent, SphereColliderComponent>(e);
                glm::vec3 centerWorld = glm::vec3(tr.world.GetMatrix() * glm::vec4(sphere.center, 1.0f));
                float maxScale = std::max({ std::abs(tr.world.scale.x), std::abs(tr.world.scale.y), std::abs(tr.world.scale.z) });
                float radiusWorld = sphere.radius * maxScale;

                glm::mat4 xy = glm::translate(glm::mat4(1.0f), centerWorld) * glm::scale(glm::mat4(1.0f), glm::vec3(radiusWorld * 2.0f));
                glm::mat4 xz = glm::translate(glm::mat4(1.0f), centerWorld) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(radiusWorld * 2.0f));
                glm::mat4 yz = glm::translate(glm::mat4(1.0f), centerWorld) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(radiusWorld * 2.0f));

                m_Renderer2D->DrawCircle(xy, colliderColor);
                m_Renderer2D->DrawCircle(xz, colliderColor);
                m_Renderer2D->DrawCircle(yz, colliderColor);
            }

            // 3. Capsule Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, CapsuleColliderComponent>())
            {
                const auto &[tr, capsule] = m_Scene->registry->get<TransformComponent, CapsuleColliderComponent>(e);
                
				const glm::mat4 world = tr.world.GetMatrix();
				const float maxAxis = glm::compMax(glm::abs(tr.world.scale));
				const float scaledRadius = capsule.radius * maxAxis;
				const float scaledHalfHeight = glm::max(capsule.height * 0.5f - capsule.radius, 0.0f) * maxAxis;

				const glm::vec3 center = glm::vec3(world * glm::vec4(capsule.center, 1.0f));
				const glm::vec3 right = glm::normalize(glm::vec3(world[0])) * scaledRadius;
				const glm::vec3 forward = glm::normalize(glm::vec3(world[2])) * scaledRadius;
				const glm::vec3 upHeight = glm::normalize(glm::vec3(world[1])) * scaledHalfHeight;
				const glm::vec3 upRadius = glm::normalize(glm::vec3(world[1])) * scaledRadius;

				const glm::vec3 topCenter = center + upHeight;
				const glm::vec3 bottomCenter = center - upHeight;

				// Draw capsule as two circle rings and connecting lines
				DrawCircleRing(topCenter, right, forward, kCircleSegments, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
				DrawCircleRing(bottomCenter, right, forward, kCircleSegments, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));

				// Draw connecting lines between the top and bottom circles
				m_Renderer2D->DrawLine(topCenter + forward, bottomCenter + forward, kPhysicsDebugColor);
				m_Renderer2D->DrawLine(topCenter - forward, bottomCenter - forward, kPhysicsDebugColor);
				m_Renderer2D->DrawLine(topCenter + right, bottomCenter + right, kPhysicsDebugColor);
				m_Renderer2D->DrawLine(topCenter - right, bottomCenter - right, kPhysicsDebugColor);

				// Draw arcs to represent the rounded ends of the capsule
				DrawArc(topCenter, forward, upRadius, kCircleSegments / 2, kPhysicsDebugColor);
				DrawArc(topCenter, right, upRadius, kCircleSegments / 2, kPhysicsDebugColor);
				DrawArc(bottomCenter, forward, -upRadius, kCircleSegments / 2, kPhysicsDebugColor);
				DrawArc(bottomCenter, right, -upRadius, kCircleSegments / 2, kPhysicsDebugColor);
            }

            // 4. Character Controllers
            for (entt::entity e : m_Scene->registry->view<TransformComponent, CharacterControllerComponent>())
            {
                const auto &[tr, cc] = m_Scene->registry->get<TransformComponent, CharacterControllerComponent>(e);
				const glm::mat4 world = tr.world.GetMatrix();
				const float maxAxis = glm::compMax(glm::abs(tr.world.scale));
				const float scaledRadius = cc.radius * maxAxis;
				const float scaledHalfHeight = glm::max(cc.height * 0.5f - cc.radius, 0.0f) * maxAxis;

				const glm::vec3 center = glm::vec3(world * glm::vec4(cc.center, 1.0f));
				const glm::vec3 right = glm::normalize(glm::vec3(world[0])) * scaledRadius;
				const glm::vec3 forward = glm::normalize(glm::vec3(world[2])) * scaledRadius;
				const glm::vec3 upHeight = glm::normalize(glm::vec3(world[1])) * scaledHalfHeight;
				const glm::vec3 upRadius = glm::normalize(glm::vec3(world[1])) * scaledRadius;

				const glm::vec3 topCenter = center + upHeight;
				const glm::vec3 bottomCenter = center - upHeight;

				// Draw capsule as two circle rings and connecting lines
				DrawCircleRing(topCenter, right, forward, kCircleSegments, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
				DrawCircleRing(bottomCenter, right, forward, kCircleSegments, glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));

				// Draw connecting lines between the top and bottom circles
				m_Renderer2D->DrawLine(topCenter + forward, bottomCenter + forward, kPhysicsDebugColor);
				m_Renderer2D->DrawLine(topCenter - forward, bottomCenter - forward, kPhysicsDebugColor);
				m_Renderer2D->DrawLine(topCenter + right, bottomCenter + right, kPhysicsDebugColor);
				m_Renderer2D->DrawLine(topCenter - right, bottomCenter - right, kPhysicsDebugColor);

				// Draw arcs to represent the rounded ends of the capsule
				DrawArc(topCenter, forward, upRadius, kCircleSegments / 2, kPhysicsDebugColor);
				DrawArc(topCenter, right, upRadius, kCircleSegments / 2, kPhysicsDebugColor);
				DrawArc(bottomCenter, forward, -upRadius, kCircleSegments / 2, kPhysicsDebugColor);
				DrawArc(bottomCenter, right, -upRadius, kCircleSegments / 2, kPhysicsDebugColor);
            }

            // 5. Plane Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, PlaneColliderComponent>())
            {
                const auto &[tr, plane] = m_Scene->registry->get<TransformComponent, PlaneColliderComponent>(e);
                glm::mat4 planeMat = tr.world.GetMatrix() * glm::translate(glm::mat4(1.0f), plane.center);

                glm::vec3 p0 = glm::vec3(planeMat * glm::vec4(-0.5f * plane.scale.x, 0.0f, -0.5f * plane.scale.z, 1.0f));
                glm::vec3 p1 = glm::vec3(planeMat * glm::vec4( 0.5f * plane.scale.x, 0.0f, -0.5f * plane.scale.z, 1.0f));
                glm::vec3 p2 = glm::vec3(planeMat * glm::vec4( 0.5f * plane.scale.x, 0.0f,  0.5f * plane.scale.z, 1.0f));
                glm::vec3 p3 = glm::vec3(planeMat * glm::vec4(-0.5f * plane.scale.x, 0.0f,  0.5f * plane.scale.z, 1.0f));

                m_Renderer2D->DrawLine(p0, p1, colliderColor);
                m_Renderer2D->DrawLine(p1, p2, colliderColor);
                m_Renderer2D->DrawLine(p2, p3, colliderColor);
                m_Renderer2D->DrawLine(p3, p0, colliderColor);

                glm::vec3 normStart = glm::vec3(planeMat * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                glm::vec3 normEnd = glm::vec3(planeMat * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                m_Renderer2D->DrawLine(normStart, normEnd, colliderColor);
            }

            // 6. Mesh Colliders
            for (entt::entity e : m_Scene->registry->view<TransformComponent, MeshColliderComponent>())
            {
                const auto &[tr, meshComp] = m_Scene->registry->get<TransformComponent, MeshColliderComponent>(e);
                glm::mat4 meshMat = tr.world.GetMatrix() * glm::translate(glm::mat4(1.0f), meshComp.center);

                if (!meshComp.vertices.empty() && !meshComp.indices.empty())
                {
                    for (size_t i = 0; i + 2 < meshComp.indices.size(); i += 3)
                    {
                        glm::vec3 v0 = glm::vec3(meshMat * glm::vec4(meshComp.vertices[meshComp.indices[i]], 1.0f));
                        glm::vec3 v1 = glm::vec3(meshMat * glm::vec4(meshComp.vertices[meshComp.indices[i + 1]], 1.0f));
                        glm::vec3 v2 = glm::vec3(meshMat * glm::vec4(meshComp.vertices[meshComp.indices[i + 2]], 1.0f));

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

            m_Renderer2D->Flush(framebuffer, frameContext->cameraBuffer.GetHandle());
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


    void SceneRenderer::CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, FrameContext *frameContext,
        Ref<CameraRenderTarget> target, const CameraLens &lens, const PostProcessing &postProcessing,
        Ref<Texture> edgeTexture, Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, bool msaaResolved)
    {
        IGN_PROFILE_FUNCTION();

        auto compositeFramebuffer = target->compositeRT->GetFramebuffer();
        
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

        m_CompositePostProcessBuffer.SetData(cmd, &m_PostProcessingData, sizeof(m_PostProcessingData));
        cmd->setBufferState(m_CompositePostProcessBuffer.GetHandle(), nvrhi::ResourceStates::ConstantBuffer);

        const uint32_t previousHistoryIndex = (frameContext->frameIndexInFlight + 2u) % 3u;
        Ref<Texture> taaHistoryTexture = target->taaHistoryRT[previousHistoryIndex]
            ? target->taaHistoryRT[previousHistoryIndex]->GetColorAttachment(0)
            : Renderer::GetBlackTexture();

        Ref<GraphicsPipeline> compositePipeline = GetCompositePSO(compositeFramebuffer, nvrhi::RasterFillMode::Solid);
        nvrhi::BindingSetHandle bindingSet = GetOrCreateCompositeBindingSet(compositePipeline->GetBindingLayout(0), target, 
            edgeTexture, bloomTexture, ssaoTexture, taaHistoryTexture, m_CompositePostProcessBuffer.GetHandle(), m_CompositeSampler.Get(), msaaResolved);

        cmd->setBufferState(m_CompositeVertexBuffer->GetHandle(), nvrhi::ResourceStates::VertexBuffer);

        auto graphicsState = nvrhi::GraphicsState();
        graphicsState.pipeline = compositePipeline->GetHandle();
        graphicsState.framebuffer = compositeFramebuffer;
        graphicsState.vertexBuffers = { nvrhi::VertexBufferBinding{ m_CompositeVertexBuffer->GetHandle(), 0, 0 } };
        graphicsState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(compositeFramebuffer->getFramebufferInfo().getViewport());
        graphicsState.bindings = { bindingSet };
        cmd->setGraphicsState(graphicsState);

        auto args = nvrhi::DrawArguments();
        args.instanceCount = 1;
        args.vertexCount = 6;
        cmd->draw(args);
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
        return (m_WorldEnvironment && m_WorldEnvironment->environment) 
            ? m_WorldEnvironment->environment->GetHDRTexture() : nullptr;
    }

    void SceneRenderer::SetFillMode(nvrhi::RasterFillMode mode)
    {
        m_FillMode = mode;

        // Recreate pipelines
        m_AnimatedPSOCache.clear();
        m_StaticPSOCache.clear();
		m_TransparentAnimatedPSOCache.clear();
		m_TransparentStaticPSOCache.clear();
        m_EnvironmentPSOCache.clear();
        m_CompositePSOCache.clear();
        m_DebugGridPSOCache.clear();
        m_CompositeBindingSetCache.clear();
        m_DebugGridBindingSetCache.clear();

        m_Renderer2D->SetFillMode(mode);
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
    Ref<GraphicsPipeline> SceneRenderer::GetDebugGridPSO(nvrhi::IFramebuffer *framebuffer)
    {
        if (!framebuffer)
            return nullptr;

        auto key = MakeFramebufferKey(framebuffer, nvrhi::RasterFillMode::Solid);
        auto it = m_DebugGridPSOCache.find(key);
        if (it != m_DebugGridPSOCache.end())
        {
            return it->second;
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
        bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.srcBlend = nvrhi::BlendFactor::SrcAlpha;
        params.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        params.srcBlendAlpha = nvrhi::BlendFactor::One;
        params.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        params.enableDepthWrite = false;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthStencil = false;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;
        params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

        nvrhi::BindingLayoutDesc bindingLayoutDesc;
        bindingLayoutDesc.setRegisterSpace(0);
        bindingLayoutDesc.setRegisterSpaceIsDescriptorSet(true);
        bindingLayoutDesc.setVisibility(nvrhi::ShaderType::All);
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
        bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(1));
        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/infinite_grid.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/infinite_grid.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        auto gp = GraphicsPipeline::Create("Infinite Grid Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(bindingLayout)
            .Build(framebuffer, params);

        m_DebugGridPSOCache.clear();
        m_DebugGridPSOCache.emplace(key, gp);
        return gp;
    }

    nvrhi::BindingSetHandle SceneRenderer::GetOrCreateDebugGridBindingSet(nvrhi::IBindingLayout *bindingLayout, const nvrhi::BufferHandle &cameraBuffer, const nvrhi::BufferHandle &gridBuffer)
    {
        DebugGridBindingKey key{ bindingLayout, cameraBuffer.Get(), gridBuffer.Get() };
        auto it = m_DebugGridBindingSetCache.find(key);
        if (it != m_DebugGridBindingSetCache.end())
        {
            return it->second;
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        auto bindingSetDesc = nvrhi::BindingSetDesc();
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer)); // volatile
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, gridBuffer)); // volatile

        nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
        LOG_ASSERT(bindingSet, "[Debug Grid] Failed to create binding set");
        if (bindingSet)
        {
            m_DebugGridBindingSetCache.emplace(key, bindingSet);
        }

        return bindingSet;
    }

    Ref<GraphicsPipeline> SceneRenderer::GetOrCreateMeshPSO(
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
        nvrhi::IFramebuffer *framebuffer,
        nvrhi::RasterFillMode fillMode,
        const char *vertexShaderPath,
        const char *pixelShaderPath,
        EBindingLayout meshLayout,
        bool transparent)
    {
        if (!framebuffer)
            return nullptr;

        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = cache.find(key);
        if (it != cache.end())
        {
            return it->second;
        }

        const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
        bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

        GraphicsPipelineParams params;
        params.enableBlend = transparent;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

        if (transparent)
        {
            params.srcBlend = nvrhi::BlendFactor::SrcAlpha;
            params.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
            params.srcBlendAlpha = nvrhi::BlendFactor::One;
            params.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
            params.enableDepthWrite = false;
            params.cullMode = nvrhi::RasterCullMode::None;
        }
        else
        {
            params.enableDepthWrite = hasDepthAttachment;
            params.cullMode = nvrhi::RasterCullMode::Front;
        }

        Ref<Shader> vertexShader = Shader::Create(vertexShaderPath, UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create(pixelShaderPath, UMBRA_SHADER_TYPE_PIXEL, true);

        auto gp = GraphicsPipeline::Create("Mesh Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(meshLayout))
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MATERIAL))
            .AddBindingLayout(BindlessSystem::GetBindingLayout())
            .Build(framebuffer, params);

        cache.clear();
        cache.emplace(key, gp);
        return gp;
    }

	Ref<GraphicsPipeline> SceneRenderer::GetOrCreateCMSPSO(
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
        nvrhi::IFramebuffer *framebuffer, const char *vertexShaderPath, const char *pixelShaderPath, 
        EBindingLayout meshLayout)
	{
        if (!framebuffer)
            return nullptr;

		auto key = MakeFramebufferKey(framebuffer, nvrhi::RasterFillMode::Solid);
		auto it = cache.find(key);
		if (it != cache.end())
		{
			return it->second;
		}

		GraphicsPipelineParams params;
		params.enableDepthWrite = true;
		params.enableDepthTest = true;
		params.depthFunc = nvrhi::ComparisonFunc::Less;
		params.cullMode = nvrhi::RasterCullMode::Front;
		params.fillMode = nvrhi::RasterFillMode::Solid;

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
    Ref<GraphicsPipeline> SceneRenderer::GetAnimatedPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
    {
        return GetOrCreateMeshPSO(m_AnimatedPSOCache, framebuffer, fillMode,
            "resources/shaders/mesh_anim.vertex.hlsl", "resources/shaders/mesh_anim.pixel.hlsl",
            EBindingLayout::MESH_ANIM, false);
    }

	Ref<GraphicsPipeline> SceneRenderer::GetAnimatedTransparentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
        return GetOrCreateMeshPSO(m_TransparentAnimatedPSOCache, framebuffer, fillMode,
            "resources/shaders/mesh_anim.vertex.hlsl", "resources/shaders/mesh_anim.pixel.hlsl",
            EBindingLayout::MESH_ANIM, true);
	}

	Ref<GraphicsPipeline> SceneRenderer::GetStaticPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
        return GetOrCreateMeshPSO(m_StaticPSOCache, framebuffer, fillMode,
            "resources/shaders/mesh_static.vertex.hlsl", "resources/shaders/mesh_static.pixel.hlsl",
            EBindingLayout::MESH_STATIC, false);
	}

	Ref<GraphicsPipeline> SceneRenderer::GetStaticTransparentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
        return GetOrCreateMeshPSO(m_TransparentStaticPSOCache, framebuffer, fillMode,
            "resources/shaders/mesh_static.vertex.hlsl", "resources/shaders/mesh_static.pixel.hlsl",
            EBindingLayout::MESH_STATIC, true);
	}

	Ref<GraphicsPipeline> SceneRenderer::GetAnimatedCSMPSO()
	{
        nvrhi::IFramebuffer *framebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(0);
        return GetOrCreateCMSPSO(m_AnimatedCSMPSOCache, framebuffer,
            "resources/shaders/csm_anim.vertex.hlsl", "resources/shaders/csm.pixel.hlsl",
            EBindingLayout::MESH_ANIM);
	}

	Ref<GraphicsPipeline> SceneRenderer::GetStaticCSMPSO()
	{
		nvrhi::IFramebuffer *framebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(0);
		return GetOrCreateCMSPSO(m_StaticCSMPSOCache, framebuffer,
			"resources/shaders/csm_static.vertex.hlsl", "resources/shaders/csm.pixel.hlsl",
			EBindingLayout::MESH_STATIC);
	}

	// Helper to build an environment pipeline per framebuffer (once)
    Ref<GraphicsPipeline> SceneRenderer::GetEnvironmentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
    {
        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = m_EnvironmentPSOCache.find(key);
        if (it != m_EnvironmentPSOCache.end())
        {
            return it->second;
        }

        const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
        bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.enableDepthWrite = hasDepthAttachment;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.cullMode = nvrhi::RasterCullMode::Front;
        params.depthFunc = nvrhi::ComparisonFunc::Always;

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
    Ref<GraphicsPipeline> SceneRenderer::GetCompositePSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
    {
        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = m_CompositePSOCache.find(key);

        if (it != m_CompositePSOCache.end())
        {
            return it->second;
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // Binding layout
        nvrhi::BindingLayoutDesc layoutDesc = {};
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // scene
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // ui
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2)); // edge
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3)); // bloom
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(4)); // ssao
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(5)); // depth
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(6)); // debug
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(7)); // objectID
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(8)); // TAA history
        layoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)); // post-process params
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0)); // sampler
        nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(layoutDesc);

        const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
        bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.enableDepthWrite = hasDepthAttachment;
        params.enableDepthTest = hasDepthAttachment;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.cullMode = nvrhi::RasterCullMode::None;

        // Create pipeline
        Ref<Shader> vertexShader = Shader::Create("resources/shaders/composite.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, true);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/composite.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, true);

        auto gp = GraphicsPipeline::Create("Composite Pipeline");
        gp->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(bindingLayout)
            .Build(framebuffer, params);

        LOG_INFO("[Composite] Created new pipeline with forced shader recompilation");

        m_CompositePSOCache.emplace(key, gp);

        return gp;
    }

	nvrhi::BindingSetHandle SceneRenderer::GetOrCreateCompositeBindingSet(nvrhi::IBindingLayout *bindingLayout, Ref<CameraRenderTarget> target, Ref<Texture> edgeTexture,
			Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, Ref<Texture> taaHistoryTexture, const nvrhi::BufferHandle &postProcessBuffer, nvrhi::ISampler *sampler, bool useResolvedScene)
    {
        Ref<Texture> edge = edgeTexture ? edgeTexture : Renderer::GetBlackTexture();
        Ref<Texture> bloom = bloomTexture ? bloomTexture : Renderer::GetBlackTexture();
        Ref<Texture> ssao = ssaoTexture ? ssaoTexture : Renderer::GetWhiteTexture();
        Ref<Texture> taaHistory = taaHistoryTexture ? taaHistoryTexture : Renderer::GetBlackTexture();
        Ref<Texture> depth = target->sceneRT->GetDepthAttachment() ? target->sceneRT->GetDepthAttachment() : Renderer::GetBlackTexture();
        Ref<Texture> debug = target->debugRT->GetColorAttachment(0) ? target->debugRT->GetColorAttachment(0) : Renderer::GetBlackTexture();

        // When MSAA is active use the resolved single-sample textures for the composite pass
        const bool hasResolved = useResolvedScene && target->sceneResolvedRT;
        Ref<Texture> sceneColor = (hasResolved ? target->sceneResolvedRT->GetColorAttachment(0) : target->sceneRT->GetColorAttachment(0));
        Ref<Texture> objectIDTex = hasResolved
            ? (target->sceneResolvedRT->GetColorAttachment(1) ? target->sceneResolvedRT->GetColorAttachment(1) : Renderer::GetBlackUIntTexture())
            : (target->sceneRT->GetColorAttachment(1) ? target->sceneRT->GetColorAttachment(1) : Renderer::GetBlackUIntTexture());

        CompositeBindingKey key
        {
            bindingLayout,
            sceneColor->GetHandle(),
            target->widgetRT->GetColorAttachment(0)->GetHandle(),
            edge->GetHandle(),
            bloom->GetHandle(),
            ssao->GetHandle(),
            depth->GetHandle(),
            debug->GetHandle(),
            objectIDTex->GetHandle(),
            taaHistory->GetHandle(),
            postProcessBuffer,
            sampler
        };

        auto it = m_CompositeBindingSetCache.find(key);
        if (it != m_CompositeBindingSetCache.end())
        {
            return it->second;
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // Composite Binding set
        auto bindingSetDesc = nvrhi::BindingSetDesc();
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, sceneColor->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, target->widgetRT->GetColorAttachment(0)->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, edge->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, bloom->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, ssao->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, depth->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(6, debug->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(7, objectIDTex->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(8, taaHistory->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, postProcessBuffer));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));

        nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
        LOG_ASSERT(bindingSet, "[Composite] Failed to create Composite Binding Set");

        m_CompositeBindingSetCache.emplace(key, bindingSet);

        return bindingSet;
    }

    Ref<CameraRenderTarget> SceneRenderer::GetOrCreateRenderTarget(ICamera *camera)
    {
        auto it = m_RenderTargets.find(camera);
        if (it != m_RenderTargets.end())
            return it->second;

        Ref<CameraRenderTarget> target = CreateRef<CameraRenderTarget>();

        // Determine MSAA sample count from camera or scene settings
        const PostProcessing &pp = camera->postProcessing;
        const bool msaaEnabled = pp.msaaProperties.enable || sceneRenderSettings.msaaProperties.enable;
        const int sampleCount = msaaEnabled
            ? (sceneRenderSettings.msaaProperties.enable ? sceneRenderSettings.msaaProperties.sampleCount : pp.msaaProperties.sampleCount)
            : 1;
        target->msaaSampleCount = sampleCount;

        // =========================================
        // Create Render Targets
        RenderTargetCreateInfo sceneRTCreateInfo = {};
        sceneRTCreateInfo.sampleCount = sampleCount;
        sceneRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Scene DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite}, // Depth
            FramebufferAttachments{ "[Scene ColorAttachment]", nvrhi::Format::RGBA16_FLOAT, nvrhi::ResourceStates::RenderTarget}, // HDR Main Color
            FramebufferAttachments{ "[Scene ObjectIDAttachment]", nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget} // Object ID
        };

        target->sceneRT = RenderTarget::Create(sceneRTCreateInfo, "[Scene Renderer] Scene RT");

        // If MSAA is active, create a single-sample resolve target that downstream passes (bloom, composite) read from
        if (sampleCount > 1)
        {
            RenderTargetCreateInfo resolvedRTCreateInfo = {};
            resolvedRTCreateInfo.sampleCount = 1;
            resolvedRTCreateInfo.attachments =
            {
                FramebufferAttachments{ "[Scene Resolved ColorAttachment]", nvrhi::Format::RGBA16_FLOAT, nvrhi::ResourceStates::ShaderResource },
                FramebufferAttachments{ "[Scene Resolved ObjectIDAttachment]", nvrhi::Format::R32_UINT, nvrhi::ResourceStates::ShaderResource }
            };
            target->sceneResolvedRT = RenderTarget::Create(resolvedRTCreateInfo, "[Scene Renderer] Scene Resolved RT");
        }

        // Widget RT
        RenderTargetCreateInfo widgetRTCreateInfo = {};
        widgetRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Scene DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite}, // Depth
            FramebufferAttachments{ "[Scene ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget}, // Main Color
        };
        target->widgetRT = RenderTarget::Create(widgetRTCreateInfo, "[Scene Renderer] Widget RT");

        RenderTargetCreateInfo compositeRTCreateInfo = {};
        compositeRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Composite Color Attachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget} // Main Color
        };

        target->compositeRT = RenderTarget::Create(compositeRTCreateInfo, "[Scene Renderer] Composite RT");

        RenderTargetCreateInfo taaHistoryRTCreateInfo = {};
        taaHistoryRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[TAA History Color Attachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::ShaderResource }
        };
        for (Ref<RenderTarget> &historyRT : target->taaHistoryRT)
        {
            historyRT = RenderTarget::Create(taaHistoryRTCreateInfo, "[Scene Renderer] TAA History RT");
        }

        m_WidgetRenderer = WidgetRenderer::Create(1280, 720);

        RenderTargetCreateInfo debugRTCreateInfo = {};
        debugRTCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Scene DebugAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget },
            FramebufferAttachments{ "[Scene DebugObjectIDAttachment]", nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget },
            FramebufferAttachments{ "[Scene DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite }
        };

        debugRTCreateInfo.depthAttachmentOverride = target->sceneRT->GetDepthAttachment();
        target->debugRT = RenderTarget::Create(debugRTCreateInfo, "[Scene Renderer] Debug RT");

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

	std::vector<Ref<SSAO>> SceneRenderer::GetOrCreateSSAOs(ICamera *camera)
	{
		constexpr uint32_t resolution = 1080;

		auto it = m_SSAOs.find(camera);
        if (it != m_SSAOs.end())
			return it->second;

		const uint32_t maxFrame = DeviceManager::GetInstance()->GetDeviceParameters().maxFramesInFlight;
		std::vector<Ref<SSAO>> SSAOs(maxFrame);
		for (uint32_t i = 0; i < maxFrame; ++i)
		{
			Ref<SSAO> ssao = CreateRef<SSAO>(resolution, resolution);
			SSAOs[i] = ssao;
		}

		m_SSAOs.emplace(camera, SSAOs);
		return SSAOs;
	}

	template<typename MeshT>
    void SceneRenderer::DrawMesh(
        nvrhi::ICommandList *cmd,
		FrameContext *frameContext,
        nvrhi::IFramebuffer *framebuffer,
        const Ref<MeshT> &mesh,
        const glm::mat4 &parentTransform,
        const glm::mat4 &normalMatrix,
        uint32_t objectID,
        const std::unordered_map<int, AssetHandle> &overrideMaterials,
        const std::vector<glm::mat4> &boneTransforms,
        const std::vector<Mesh_GPUData> &cachedInstanceTransforms,
        ICamera *camera,
        Ref<GraphicsPipeline> opaquePSO,
        std::vector<TransparentDrawCall> &transparentDrawCalls,
        std::unordered_set<Material *> &uploadedMaterialsThisPass,
        entt::entity entity,
        const std::string &socketName)
    {
        if (!framebuffer)
            return;

        constexpr bool isSkeletal = std::is_same_v<MeshT, SkeletalMesh>;

		static_assert(std::is_same_v<MeshT, StaticMesh> || std::is_same_v<MeshT, SkeletalMesh>,
			"DrawPreviewMeshImpl: MeshT must be StaticMesh or SkeletalMesh");

        auto graphicsState = nvrhi::GraphicsState();
        graphicsState.pipeline = opaquePSO->GetHandle();
        graphicsState.framebuffer = framebuffer;
        graphicsState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

        const auto &instances = mesh->GetMeshInstances();
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

			nvrhi::BindingSetHandle meshBindingSet = frameContext->staticMeshBindingSet;
            if constexpr (isSkeletal)
            {
				meshBindingSet = frameContext->animatedBindingSet;
            }

            auto &primitive = meshInstance->GetPrimitive();
            if (!primitive->vertexBuffer || !primitive->indexBuffer)
            {
                primitive->WriteBuffer(cmd);
            }

            Ref<Material> material = ResolveMeshMaterial(static_cast<int>(idx), overrideMaterials, meshInstance->GetMaterialAssetHandle());

            const nvrhi::BindingSetHandle materialBindingSet = (material && material->GetBindingSet())
                ? material->GetBindingSet()
                : m_RuntimeMaterial->GetBindingSet();

            if (meshBindingSet && materialBindingSet && primitive->vertexBuffer && primitive->indexBuffer)
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
                    if constexpr (isSkeletal)
                    {
                        // Bones are indexed via pushConstants_ObjectIndex and GPU buffer allocator
                    }
                    transparentDrawCalls.push_back(dc);
                }
                else
                {
                    graphicsState.bindings = { meshBindingSet, materialBindingSet, BindlessSystem::GetDescriptorTable() };
                    graphicsState.vertexBuffers = { nvrhi::VertexBufferBinding{ primitive->vertexBuffer->GetHandle(), 0, 0 } };
                    graphicsState.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });

                    cmd->setGraphicsState(graphicsState);
					
					// Push the object ID to the shader via push constants
                    cmd->setPushConstants(&PushConstant_ObjectIndex, sizeof(PushConstant_ObjectIndex));

                    const uint32_t idxCount = primitive->indexBuffer->GetCount();
                    nvrhi::DrawArguments args;
                    args.setVertexCount(idxCount);
                    args.instanceCount = 1;
                    cmd->drawIndexed(args);

                    Renderer::Stats.drawCallCount++;
                    Renderer::Stats.indexCount3D += idxCount;
                    Renderer::Stats.vertexCount3D += primitive->vertexBuffer->GetByteSize() / sizeof(float); // approx
                }
            }
        }
    }

    template<typename MeshT>
    void SceneRenderer::DrawMeshShadow(
        nvrhi::ICommandList *cmd,
		FrameContext *frameContext,
        const Ref<MeshT> &mesh,
        const glm::mat4 &parentTransform,
        const glm::mat4 &normalMatrix,
        uint32_t objectID,
        const std::vector<glm::mat4> &boneTransforms,
        const std::vector<Mesh_GPUData> &cachedInstanceTransforms,
        nvrhi::GraphicsState &csmState,
        uint32_t cascadeIndex,
        entt::entity entity,
        const std::string &socketName)
    {
        constexpr bool isSkeletal = std::is_same_v<MeshT, SkeletalMesh>;

        const auto &instances = mesh->GetMeshInstances();
        for (size_t idx = 0; idx < instances.size(); ++idx)
        {
            auto &meshInstance = instances[idx];

            auto &primitive = meshInstance->GetPrimitive();
            if (!primitive)
                continue;

            if (!primitive->vertexBuffer || !primitive->indexBuffer)
                primitive->WriteBuffer(cmd);

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

			nvrhi::BindingSetHandle meshBindingSet = frameContext->staticMeshCSMBindingSet[cascadeIndex];
            if constexpr (isSkeletal)
            {
				meshBindingSet = frameContext->animatedMeshCSMBindingSet[cascadeIndex];
            }

            if (meshBindingSet)
            {
                csmState.bindings = { meshBindingSet };
                csmState.vertexBuffers = { nvrhi::VertexBufferBinding{ primitive->vertexBuffer->GetHandle(), 0, 0 } };
                csmState.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });

                cmd->setGraphicsState(csmState);

				// Push the object ID to the shader via push constants
				cmd->setPushConstants(&PushConstant_ObjectIndex, sizeof(PushConstant_ObjectIndex));

                const uint32_t idxCount = primitive->indexBuffer->GetCount();
                nvrhi::DrawArguments args;
                args.setVertexCount(idxCount);
                args.instanceCount = 1;
                cmd->drawIndexed(args);

                Renderer::Stats.shadowDrawCallCount++;
            }
        }
    }

    // Explicit template instantiations
    template void SceneRenderer::DrawMesh<StaticMesh>(
        nvrhi::ICommandList *, FrameContext *, nvrhi::IFramebuffer *, const Ref<StaticMesh> &, const glm::mat4 &, const glm::mat4 &, uint32_t,
        const std::unordered_map<int, AssetHandle> &, const std::vector<glm::mat4> &, const std::vector<Mesh_GPUData> &,
        ICamera *, Ref<GraphicsPipeline>, std::vector<TransparentDrawCall> &, std::unordered_set<Material *> &, entt::entity, const std::string &);

    template void SceneRenderer::DrawMesh<SkeletalMesh>(
        nvrhi::ICommandList *, FrameContext *, nvrhi::IFramebuffer *, const Ref<SkeletalMesh> &, const glm::mat4 &, const glm::mat4 &, uint32_t,
        const std::unordered_map<int, AssetHandle> &, const std::vector<glm::mat4> &, const std::vector<Mesh_GPUData> &,
        ICamera *, Ref<GraphicsPipeline>, std::vector<TransparentDrawCall> &, std::unordered_set<Material *> &, entt::entity, const std::string &);

    template void SceneRenderer::DrawMeshShadow<StaticMesh>(
        nvrhi::ICommandList *, FrameContext *, const Ref<StaticMesh> &, const glm::mat4 &, const glm::mat4 &, uint32_t,
        const std::vector<glm::mat4> &, const std::vector<Mesh_GPUData> &, nvrhi::GraphicsState &, uint32_t, entt::entity, const std::string &);

    template void SceneRenderer::DrawMeshShadow<SkeletalMesh>(
        nvrhi::ICommandList *, FrameContext *, const Ref<SkeletalMesh> &, const glm::mat4 &, const glm::mat4 &, uint32_t,
        const std::vector<glm::mat4> &, const std::vector<Mesh_GPUData> &, nvrhi::GraphicsState &, uint32_t, entt::entity, const std::string &);
}
