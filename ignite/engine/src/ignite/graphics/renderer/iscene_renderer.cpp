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
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/project/project.hpp"

#include "ignite/graphics/objects/environment.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{
    ISceneRenderer::ISceneRenderer()
        : m_CascadedShadowMapBuffer(sizeof(CSM_GPUData), false, 1, "[SceneRenderer] CSM Buffer")
        , m_CompositePostProcessBuffer(sizeof(CompositePostProcess_GPUData), true, 16, "Composite PostProcess Buffer")
        , m_FXAABuffer(sizeof(FXAA_GPUData), true, 16, "FXAA Buffer")
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
        m_GizmoRenderer = CreateRef<GizmoRenderer>();

        m_AssetChangeSignalToken = SignalBus::Subscribe<AssetChangeSignal>(
            [this](const AssetChangeSignal &signal)
            {
                if (signal.handle == AssetHandle(0))
                    return;
                std::lock_guard<std::mutex> lock(m_DirtyResolvedAssetsMutex);
                m_DirtyResolvedHandles.insert(signal.handle);
            });
    }

    void ISceneRenderer::DrainDirtyResolvedAssets()
    {
        std::unordered_set<AssetHandle> dirty;
        {
            std::lock_guard<std::mutex> lock(m_DirtyResolvedAssetsMutex);
            if (m_DirtyResolvedHandles.empty())
                return;
            dirty.swap(m_DirtyResolvedHandles);
        }

        for (auto it = m_ResolvedAssetsCache.begin(); it != m_ResolvedAssetsCache.end();)
        {
            if (dirty.contains(it->first.handle))
            {
                it = m_ResolvedAssetsCache.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    nri::CommandBuffer *ISceneRenderer::BeginCommandBuffer()
    {
        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        m_ActiveSlotIndex = m_CurrentSlotIndex;
        auto &slot = m_CommandBufferSlots[m_ActiveSlotIndex];
        m_CurrentSlotIndex = (m_CurrentSlotIndex + 1) % static_cast<uint32_t>(m_CommandBufferSlots.size());

        if (m_CommandFence && slot.fenceValue > 0)
        {
            const uint64_t currentFenceValue = nri.GetFenceValue(*m_CommandFence);
            if (currentFenceValue == std::numeric_limits<uint64_t>::max())
            {
                LOG_ERROR("[SceneRenderer] BeginCommandBuffer: fence returned UINT64_MAX (device lost)");
                return nullptr;
            }
            if (currentFenceValue < slot.fenceValue)
            {
                IGN_PROFILE_SCOPE("ISceneRenderer::WaitSlotFence");
                nri.Wait(*m_CommandFence, slot.fenceValue);
            }
        }

        // GPU has completed previous execution of this slot; safe to release retained assets.
        slot.retainedAssets.clear();
        slot.retainedAssetSet.clear();
        slot.retainedRenderTargets.clear();

        nri.ResetCommandAllocator(*slot.allocator);
        nri.BeginCommandBuffer(*slot.commandBuffer, BindlessSystem::GetDescriptorPool());
        m_IsCommandBufferRecording = true;
        return slot.commandBuffer;
    }

    void ISceneRenderer::SubmitCommandBuffer(nri::CommandBuffer *cmd)
    {
        if (!cmd)
            return;

        m_IsCommandBufferRecording = false;

        auto *deviceManager = DeviceManager::GetInstance();
        const auto &nri = deviceManager->GetCoreInterface();
        nri.EndCommandBuffer(*cmd);

        const uint64_t signalValue = m_NextFenceValue;
        if (m_CommandFence)
        {
            const uint64_t currentFenceValue = nri.GetFenceValue(*m_CommandFence);
            if (currentFenceValue == std::numeric_limits<uint64_t>::max())
            {
                LOG_ERROR("[SceneRenderer] Command fence returned UINT64_MAX; skipping queue submission");
                return;
            }
            if (signalValue <= currentFenceValue)
            {
                m_NextFenceValue = currentFenceValue + 1;
                LOG_ERROR("[SceneRenderer] Corrected non-increasing fence signal value {} to {}", signalValue, m_NextFenceValue);
                return;
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
            const nri::Result result = nri.QueueSubmit(*deviceManager->GetGraphicsQueue(), submitDesc);
            if (result != nri::Result::SUCCESS)
            {
                LOG_ERROR("[SceneRenderer] QueueSubmit failed with NRI result {}", static_cast<uint32_t>(result));
                return;
            }
        }

        ++m_NextFenceValue;
        for (auto &slot : m_CommandBufferSlots)
        {
            if (slot.commandBuffer == cmd)
            {
                slot.fenceValue = signalValue;
                break;
            }
        }
    }

    void ISceneRenderer::WaitForPreviousCommandBuffer()
    {
        IGN_PROFILE_SCOPE("ISceneRenderer::WaitForPreviousCommandBuffer");
        if (!m_CommandFence || m_NextFenceValue <= 1)
            return;

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        const uint64_t lastSubmittedFenceValue = m_NextFenceValue - 1;
        if (nri.GetFenceValue(*m_CommandFence) < lastSubmittedFenceValue)
        {
            nri.Wait(*m_CommandFence, lastSubmittedFenceValue);
        }
    }

    void ISceneRenderer::RetainAssetForCurrentCommandBuffer(const Ref<Asset> &asset)
    {
        if (!asset || !m_IsCommandBufferRecording)
            return;

        auto &slot = m_CommandBufferSlots[m_ActiveSlotIndex];
        if (slot.retainedAssetSet.insert(asset.get()).second)
        {
            slot.retainedAssets.push_back(asset);
        }
    }

    void ISceneRenderer::RetainRenderTargetForCurrentCommandBuffer(const Ref<RenderTarget> &renderTarget)
    {
        if (!renderTarget || !m_IsCommandBufferRecording)
            return;

        auto &slot = m_CommandBufferSlots[m_ActiveSlotIndex];
        if (std::find(slot.retainedRenderTargets.begin(), slot.retainedRenderTargets.end(), renderTarget) == slot.retainedRenderTargets.end())
        {
            slot.retainedRenderTargets.push_back(renderTarget);
        }
    }

    void ISceneRenderer::ClearRetainedCommandBufferAssets()
    {
        for (auto &slot : m_CommandBufferSlots)
        {
            slot.retainedAssets.clear();
            slot.retainedAssetSet.clear();
            slot.retainedRenderTargets.clear();
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

    Ref<Material> ISceneRenderer::ResolveMeshMaterial(AssetHandle materialHandle)
    {
        Ref<Material> material = nullptr;

        // Retrieve the attached material
        if (materialHandle != AssetHandle(0))
        {
            material = ResolveAsset<Material>(materialHandle);
        }

        // Fallback to default material
        if (!material)
        {
            material = Renderer::GetDefaultMaterial();
        }

        if (material)
        {
            RetainAssetForCurrentCommandBuffer(material);
            if (!material->UpdateBindingSet(GetEnvironmentMapColorTexture(), GetCascadedShadowMapDepthTexture()))
            {
                return nullptr;
            }
        }

        return material;
    }

    ISceneRenderer::~ISceneRenderer()
    {
        if (m_AssetChangeSignalToken != kInvalidSignalToken)
        {
            SignalBus::Unsubscribe<AssetChangeSignal>(m_AssetChangeSignalToken);
            m_AssetChangeSignalToken = kInvalidSignalToken;
        }

        GPUUploadSync::DeviceWaitIdle(m_Device);
        ClearRetainedCommandBufferAssets();

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
        m_GizmoRenderer.reset();
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

    Ref<Texture> ISceneRenderer::GetEnvironmentMapColorTexture()
    {
        return nullptr;
    }

    Ref<Texture> ISceneRenderer::GetCascadedShadowMapDepthTexture()
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

    void SceneRenderSettings::ApplyOverallScalability(ScalabilityLevel level)
    {
        scalability.overall = level;
        scalability.shadows = level;
        scalability.globalIllumination = level;
        // Ambient occlusion uses explicit type (NONE, SSAO, GTAO) without a scalability level.
        scalability.reflections = level;
        scalability.resolutionScale = level;
        scalability.postProcessing = level;
        ApplyIndividualScalability();
    }

    void SceneRenderSettings::ApplyIndividualScalability()
    {
        switch (scalability.shadows)
        {
        case ScalabilityLevel::Low: shadowQuality = ShadowMapQuality::LOW; break;
        case ScalabilityLevel::Medium: shadowQuality = ShadowMapQuality::MEDIUM; break;
        case ScalabilityLevel::High: shadowQuality = ShadowMapQuality::HIGH; break;
        case ScalabilityLevel::Ultra: shadowQuality = ShadowMapQuality::ULTRA; break;
        }

        switch (scalability.globalIllumination)
        {
        case ScalabilityLevel::Low: giModeOverride = 0; break;    // Off
        case ScalabilityLevel::Medium: giModeOverride = 1; break; // Fast
        case ScalabilityLevel::High: giModeOverride = 1; break;   // Fast
        case ScalabilityLevel::Ultra: giModeOverride = 2; break;  // LeakResistant
        }

        switch (scalability.ambientOcclusion)
        {
        case AOMode::Off:
            aoModeOverride = 0; // NONE
            break;
        case AOMode::SSAO:
            aoModeOverride = 1; // SSAO
            break;
        case AOMode::GTAO:
            aoModeOverride = 2; // GTAO
            switch (scalability.postProcessing)
            {
            case ScalabilityLevel::Low:
                gtaoResolutionScale = 0.35f;
                gtaoSliceCount = 2;
                gtaoStepsPerSlice = 2;
                gtaoDenoiseRadius = 2;
                gtaoUpsampleMode = 0;
                break;
            case ScalabilityLevel::Medium:
                gtaoResolutionScale = 0.5f;
                gtaoSliceCount = 2;
                gtaoStepsPerSlice = 2;
                gtaoDenoiseRadius = 2;
                gtaoUpsampleMode = 1;
                break;
            case ScalabilityLevel::High:
                gtaoResolutionScale = 0.5f;
                gtaoSliceCount = 3;
                gtaoStepsPerSlice = 3;
                gtaoDenoiseRadius = 3;
                gtaoUpsampleMode = 1;
                break;
            case ScalabilityLevel::Ultra:
                gtaoResolutionScale = 0.5f;
                gtaoSliceCount = 4;
                gtaoStepsPerSlice = 4;
                gtaoDenoiseRadius = 4;
                gtaoUpsampleMode = 2;
                break;
            }
            break;
        }

        switch (scalability.reflections)
        {
        case ScalabilityLevel::Low:
            enableSSR = false;
            break;
        case ScalabilityLevel::Medium:
            enableSSR = true;
            ssrIntensity = 0.7f;
            ssrMaxRoughness = 0.45f;
            ssrResolutionScale = 0.25f;
            ssrMaxSteps = 20;
            ssrBinarySearchSteps = 3;
            break;
        case ScalabilityLevel::High:
            enableSSR = true;
            ssrIntensity = 1.0f;
            ssrMaxRoughness = 0.6f;
            ssrResolutionScale = 0.5f;
            ssrMaxSteps = 32;
            ssrBinarySearchSteps = 4;
            break;
        case ScalabilityLevel::Ultra:
            enableSSR = true;
            ssrIntensity = 1.0f;
            ssrMaxRoughness = 0.75f;
            ssrResolutionScale = 0.5f;
            ssrMaxSteps = 48;
            ssrBinarySearchSteps = 5;
            break;
        }

        switch (scalability.resolutionScale)
        {
        case ScalabilityLevel::Low: renderScale = 0.65f; break;
        case ScalabilityLevel::Medium: renderScale = 0.80f; break;
        case ScalabilityLevel::High: renderScale = 1.0f; break;
        case ScalabilityLevel::Ultra: renderScale = 1.0f; break;
        }

        switch (scalability.postProcessing)
        {
        case ScalabilityLevel::Low:
            enableSpecularAA = false;
            enableBloom = false;
            bloomResolutionScale = 0.5f;
            bloomMaxIterations = 3;
            break;
        case ScalabilityLevel::Medium:
            enableSpecularAA = true;
            enableBloom = true;
            bloomResolutionScale = 0.5f;
            bloomMaxIterations = 4;
            break;
        case ScalabilityLevel::High:
            enableSpecularAA = true;
            enableBloom = true;
            bloomResolutionScale = 0.75f;
            bloomMaxIterations = 5;
            break;
        case ScalabilityLevel::Ultra:
            enableSpecularAA = true;
            enableBloom = true;
            bloomResolutionScale = 0.75f;
            bloomMaxIterations = 5;
            break;
        }
    }

    void SceneRenderSettings::SyncToScene(Scene *scene, ICamera *editCamera) const
    {
        auto applyPP = [&](PostProcessing &pp)
        {
            switch (sampleMethod)
            {
            case SampleMethod::NONE:
                pp.taaProperties.enable = false;
                pp.msaaProperties.enable = false;
                pp.enableFXAA = false;
                break;
            case SampleMethod::TAA:
                pp.taaProperties.enable = true;
                pp.taaProperties.blendFactor = taaProperties.blendFactor;
                pp.msaaProperties.enable = false;
                pp.enableFXAA = false;
                break;
            case SampleMethod::MSAA:
                pp.taaProperties.enable = false;
                pp.msaaProperties.enable = true;
                pp.msaaProperties.sampleCount = msaaProperties.sampleCount;
                pp.enableFXAA = false;
                break;
            case SampleMethod::FXAA:
                pp.taaProperties.enable = false;
                pp.msaaProperties.enable = false;
                pp.enableFXAA = true;
                break;
            }

            const AOMode effectiveAO = (aoModeOverride >= 0)
                ? static_cast<AOMode>(std::clamp(aoModeOverride, 0, 2))
                : scalability.ambientOcclusion;
            pp.enableSSAO = (effectiveAO != AOMode::Off);
            pp.aoMode = effectiveAO;
            pp.enableBloom = enableBloom;
            pp.renderScale = renderScale;
        };

        if (editCamera)
        {
            applyPP(editCamera->postProcessing);
        }

        if (!scene || !scene->registry)
            return;

        for (entt::entity e : scene->registry->view<CameraComponent>())
        {
            auto &cc = scene->registry->get<CameraComponent>(e);
            applyPP(cc.camera.postProcessing);
            cc.dirty = true;
        }

        for (entt::entity e : scene->registry->view<DirectionalLightComponent>())
        {
            auto &light = scene->registry->get<DirectionalLightComponent>(e);
            light.shadowResolution = static_cast<uint32_t>(shadowQuality);
        }
    }

    void SceneRenderSettings::SyncFromPostProcessing(const PostProcessing &pp)
    {
        if (pp.taaProperties.enable)
        {
            SetSampleMethod(SampleMethod::TAA);
            taaProperties.blendFactor = std::clamp(pp.taaProperties.blendFactor, 0.01f, 1.0f);
        }
        else if (pp.msaaProperties.enable)
        {
            SetSampleMethod(SampleMethod::MSAA);
            const int scVal = pp.msaaProperties.sampleCount;
            msaaProperties.sampleCount = (scVal <= 1) ? 1 : (scVal <= 2 ? 2 : (scVal <= 4 ? 4 : 8));
        }
        else if (pp.enableFXAA)
        {
            SetSampleMethod(SampleMethod::FXAA);
        }
        else
        {
            SetSampleMethod(SampleMethod::NONE);
        }

        const AOMode effectiveAO = pp.enableSSAO ? pp.aoMode : AOMode::Off;
        scalability.ambientOcclusion = effectiveAO;
        aoModeOverride = static_cast<int>(effectiveAO);

        enableBloom = pp.enableBloom;
        renderScale = glm::clamp(pp.renderScale, 0.25f, 1.0f);
    }

    void SceneRenderSettings::SyncFromDirectionalLight(uint32_t shadowResolution)
    {
        const uint32_t clamped = std::min(shadowResolution, static_cast<uint32_t>(ShadowMapQuality::ULTIMATE));
        shadowQuality = static_cast<ShadowMapQuality>(clamped);
        scalability.shadows = static_cast<ScalabilityLevel>(std::min<uint32_t>(clamped, static_cast<uint32_t>(ScalabilityLevel::Ultra)));
    }

    bool SceneRenderSettings::SerializeToFile(const std::filesystem::path &filepath) const
    {
        if (filepath.empty())
            return false;

        Serializer sr(filepath);
        sr.BeginMap();
        sr.BeginMap("GameSettings");
        {
            sr.BeginMap("Scalability");
            sr.AddKeyValue("Overall", static_cast<int>(scalability.overall));
            sr.AddKeyValue("Shadows", static_cast<int>(scalability.shadows));
            sr.AddKeyValue("GlobalIllumination", static_cast<int>(scalability.globalIllumination));
            sr.AddKeyValue("AmbientOcclusion", AOModeToString(scalability.ambientOcclusion));
            sr.AddKeyValue("Reflections", static_cast<int>(scalability.reflections));
            sr.AddKeyValue("ResolutionScale", static_cast<int>(scalability.resolutionScale));
            sr.AddKeyValue("PostProcessing", static_cast<int>(scalability.postProcessing));
            sr.EndMap();

            sr.BeginMap("AntiAliasing");
            sr.AddKeyValue("Method", static_cast<int>(sampleMethod));
            sr.AddKeyValue("MSAASampleCount", msaaProperties.sampleCount);
            sr.AddKeyValue("TAABlendFactor", taaProperties.blendFactor);
            sr.AddKeyValue("FXAASubpixQuality", fxaaProperties.subpixQuality);
            sr.AddKeyValue("FXAAEdgeThreshold", fxaaProperties.edgeThreshold);
            sr.AddKeyValue("FXAAEdgeThresholdMin", fxaaProperties.edgeThresholdMin);
            sr.EndMap();

            sr.BeginMap("SceneRenderer");
            sr.AddKeyValue("RenderScale", renderScale);
            sr.AddKeyValue("ShadowQuality", static_cast<int>(shadowQuality));
            sr.AddKeyValue("GIModeOverride", giModeOverride);
            sr.AddKeyValue("AOModeOverride", aoModeOverride);
            sr.AddKeyValue("EnableDepthPrepass", enableDepthPrepass);
            sr.AddKeyValue("EnableSpecularAA", enableSpecularAA);
            sr.AddKeyValue("EnableSSR", enableSSR);
            sr.AddKeyValue("EnableBloom", enableBloom);
            sr.AddKeyValue("SSRIntensity", ssrIntensity);
            sr.AddKeyValue("SSRMaxRoughness", ssrMaxRoughness);
            sr.EndMap();
        }
        sr.EndMap();
        sr.EndMap();
        sr.Serialize(filepath);
        return true;
    }

    bool SceneRenderSettings::DeserializeFromFile(const std::filesystem::path &filepath)
    {
        if (filepath.empty() || !std::filesystem::exists(filepath))
            return false;

        JsonNode root = Serializer::Deserialize(filepath);
        JsonNode gs = root["GameSettings"];
        if (!gs)
            return false;

        auto readLevel = [](JsonNode node, const char *key, ScalabilityLevel &out)
        {
            if (node[key])
                out = static_cast<ScalabilityLevel>(std::clamp(node[key].as<int>(), 0, 3));
        };

        if (JsonNode sc = gs["Scalability"])
        {
            readLevel(sc, "Overall", scalability.overall);
            readLevel(sc, "Shadows", scalability.shadows);
            readLevel(sc, "GlobalIllumination", scalability.globalIllumination);
            if (JsonNode aoNode = sc["AmbientOcclusion"])
            {
                const std::string aoStr = aoNode.as<std::string>("");
                if (!aoStr.empty())
                {
                    scalability.ambientOcclusion = AOModeFromString(aoStr);
                }
                else
                {
                    const int aoInt = aoNode.as<int>(2);
                    scalability.ambientOcclusion = (aoInt <= 0) ? AOMode::Off : (aoInt == 1 ? AOMode::SSAO : AOMode::GTAO);
                }
            }
            readLevel(sc, "Reflections", scalability.reflections);
            readLevel(sc, "ResolutionScale", scalability.resolutionScale);
            readLevel(sc, "PostProcessing", scalability.postProcessing);
            ApplyIndividualScalability();
        }

        if (JsonNode sr = gs["SceneRenderer"])
        {
            if (sr["TAABlendFactor"]) taaProperties.blendFactor = std::clamp(sr["TAABlendFactor"].as<float>(), 0.01f, 1.0f);
            if (sr["MSAASampleCount"])
            {
                const int scVal = sr["MSAASampleCount"].as<int>();
                msaaProperties.sampleCount = (scVal <= 2) ? 2 : (scVal <= 4 ? 4 : 8);
            }
            if (sr["SampleMethod"]) SetSampleMethod(static_cast<SampleMethod>(std::clamp(sr["SampleMethod"].as<int>(), 0, 3)));
            if (sr["RenderScale"]) renderScale = std::clamp(sr["RenderScale"].as<float>(), 0.25f, 1.0f);
            if (sr["ShadowQuality"]) shadowQuality = static_cast<ShadowMapQuality>(std::clamp(sr["ShadowQuality"].as<int>(), 0, 4));
            if (sr["GIModeOverride"]) giModeOverride = std::clamp(sr["GIModeOverride"].as<int>(), -1, 2);
            if (sr["AOModeOverride"])
            {
                aoModeOverride = std::clamp(sr["AOModeOverride"].as<int>(), -1, 2);
                if (aoModeOverride >= 0)
                    scalability.ambientOcclusion = static_cast<AOMode>(aoModeOverride);
            }
            if (sr["EnableDepthPrepass"]) enableDepthPrepass = sr["EnableDepthPrepass"].as<bool>();
            if (sr["EnableSpecularAA"]) enableSpecularAA = sr["EnableSpecularAA"].as<bool>();
            if (sr["EnableSSR"]) enableSSR = sr["EnableSSR"].as<bool>();
            if (sr["EnableBloom"]) enableBloom = sr["EnableBloom"].as<bool>();
            if (sr["SSRIntensity"]) ssrIntensity = std::clamp(sr["SSRIntensity"].as<float>(), 0.0f, 2.0f);
            if (sr["SSRMaxRoughness"]) ssrMaxRoughness = std::clamp(sr["SSRMaxRoughness"].as<float>(), 0.05f, 1.0f);
        }

        if (JsonNode aa = gs["AntiAliasing"])
        {
            if (aa["MSAASampleCount"])
            {
                const int scVal = aa["MSAASampleCount"].as<int>();
                msaaProperties.sampleCount = (scVal <= 2) ? 2 : (scVal <= 4 ? 4 : 8);
            }
            if (aa["TAABlendFactor"])
                taaProperties.blendFactor = std::clamp(aa["TAABlendFactor"].as<float>(), 0.01f, 1.0f);
            if (aa["FXAASubpixQuality"])
                fxaaProperties.subpixQuality = std::clamp(aa["FXAASubpixQuality"].as<float>(), 0.0f, 1.0f);
            if (aa["FXAAEdgeThreshold"])
                fxaaProperties.edgeThreshold = std::clamp(aa["FXAAEdgeThreshold"].as<float>(), 0.05f, 0.5f);
            if (aa["FXAAEdgeThresholdMin"])
                fxaaProperties.edgeThresholdMin = std::clamp(aa["FXAAEdgeThresholdMin"].as<float>(), 0.01f, 0.2f);
            if (aa["Method"])
                SetSampleMethod(static_cast<SampleMethod>(std::clamp(aa["Method"].as<int>(), 0, 3)));
        }
        return true;
    }
}
