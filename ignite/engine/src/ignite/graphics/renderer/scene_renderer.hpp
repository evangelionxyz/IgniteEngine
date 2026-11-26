// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_RENDERER_HPP
#define IGN_SCENE_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "iscene_renderer.hpp"
#include "ignite/graphics/hash_keys.hpp"
#include "ignite/graphics/gpu_timer.hpp"
#include "ignite/graphics/gtao.hpp"
#include "ignite/graphics/ssr.hpp"
#include "ignite/graphics/gi/ibl_prefilter.hpp"
#include "ignite/graphics/gi/irradiance_volume.hpp"
#include "ignite/graphics/gi/reflection_probe_set.hpp"
#include "ignite/graphics/gi/sh.hpp"
#include "batch_builder.hpp"
#include "frame_packet.hpp"
#include "ignite/terrain/terrain_renderer.hpp"
#include "ignite/scene/icamera.hpp"
#include <entt/entt.hpp>

namespace ignite
{
    class Entity;
    class WidgetRenderer;
    class SkeletalMeshComponent;

    struct CameraWidgetInputState
    {
        uint32_t mouseX = 0;
        uint32_t mouseY = 0;
        bool hovered = false;
        bool useOverride = false;
    };

    struct RenderPlan
    {
        bool hasMeshes = false;
        bool has2D = false;
        bool hasTerrain = false;
        bool hasEnvironment = false;
        bool hasParticles = false;
        bool hasWidgets = false;
        bool hasScreenText = false;
        bool hasActiveShadows = false;
        bool requiresObjectId = false;
        bool requiresDebugOverlay = false;
        bool requiresGrid = false;
        bool requiresGizmo = false;
        bool requiresBloom = false;
        bool requiresSSAO = false;
        bool requiresTAA = false;
        bool isGameCamera = false;

        bool HasRenderables() const
        {
            return hasMeshes || has2D || hasTerrain || hasEnvironment || hasParticles;
        }

        bool IsEmptyScene() const
        {
            return !HasRenderables() && !hasWidgets && !hasScreenText && !requiresDebugOverlay && !requiresObjectId && !requiresGizmo;
        }
    };

    struct CameraRenderTarget
    {
        Ref<RenderTarget> sceneRT;         // MSAA render target (sampleCount > 1) or regular
        Ref<RenderTarget> sceneResolvedRT; // Single-sample resolve target (only used when MSAA is active)
        Ref<RenderTarget> selectionRT;     // Single-sample selected-object ID mask for outlines
        Ref<RenderTarget> widgetRT;
        Ref<RenderTarget> gizmoRT;
        Ref<RenderTarget> postProcessRT;  // Intermediate LDR target for tonemapping before FXAA/UI
        Ref<RenderTarget> compositeRT;
        Ref<RenderTarget> debugRT;
        Ref<RenderTarget> taaHistoryRT[3];
        Ref<RenderTarget> depthPrepassRT;  // RG16F octahedral view-normal + shared scene depth (prepass)
        Ref<Texture> prevSceneColor;       // Single-sample previous frame HDR scene color for SSR
        glm::mat4 prevViewProj = glm::mat4(1.0f);
        bool prevSceneColorValid = false;

        bool taaHistoryValid = false;
        int msaaSampleCount = 1;           // Tracks the current MSAA sample count (1 = no MSAA)
        RenderPlan previousPlan;
    };

    class IGN_API SceneRenderer : public ISceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();

        void BeginFrame();
        void SetActiveScene(const Ref<Scene> &scene);

        void Render(ICamera *camera, FrameContext *frameContext, bool drawDebug, RenderTarget *targetFramebuffer = nullptr);

        void SetCameraWidgetMousePosition(ICamera *camera, uint32_t mouseX, uint32_t mouseY, bool hovered);
        void SetEditorWidgetMousePosition(uint32_t mouseX, uint32_t mouseY, bool hovered);
        void SetGameplayWidgetMousePosition(uint32_t mouseX, uint32_t mouseY, bool hovered);

        virtual void ResizeFramebuffer(ICamera *camera, uint32_t width, uint32_t height) override;
        void SetFillMode(nri::FillMode mode);

        void SetSelectedEntity(const Entity& entity);
        void UnselectEntity(const Entity& entity);
        void ClearSelectedEntities();

        int GetRenderMode() const { return m_SceneGPUData.renderMode; }
        void SetRenderMode(int renderMode) { m_SceneGPUData.renderMode = renderMode; }

        int GetDebugShadowMode() const { return m_SceneGPUData.debugShadow; }
        void SetDebugShadowMode(int debugShadow) { m_SceneGPUData.debugShadow = debugShadow; }

        using DebugProbeGPU = gi::DebugProbeGPU;

        void SetDebugProbes(std::vector<DebugProbeGPU> probes);
        void ClearDebugProbes();

        void SetIrradianceVolume(const Ref<IrradianceVolume> &volume, const std::string &sourcePath = "");
        void ClearIrradianceVolume();
        Ref<IrradianceVolume> GetIrradianceVolume() const { return m_ActiveIrradianceVolume; }

        void SetReflectionProbeSet(const Ref<ReflectionProbeSet> &probeSet);
        void ClearReflectionProbeSet();
        Ref<ReflectionProbeSet> GetReflectionProbeSet() const { return m_ActiveReflectionProbeSet; }

        virtual Ref<Texture> GetEnvironmentMapColorTexture() override;
        virtual Ref<Texture> GetCascadedShadowMapDepthTexture() override;

        virtual Ref<CascadedShadowMap> GetCascadedShadowMap() override;
        Ref<Renderer2D> &GetRenderer2D() { return m_Renderer2D; }
        Ref<TerrainRenderer> &GetTerrainRenderer() { return m_TerrainRenderer; }

        Ref<CameraRenderTarget> GetRenderTarget(ICamera *camera);

        void PreallocateGPUData(nri::CommandBuffer *cmd, FrameContext *frameContext);
        const FramePacket &GetLastFramePacket() const { return m_FramePacket; }
        const std::unordered_map<std::string, double> &GetLastGPUTimingsMs() const { return m_LastGPUTimingsMs; }

    private:
        void ClearResources();
        void CollectSceneLighting(FrameContext *frameContext);

        RenderPlan BuildRenderPlan(ICamera *camera, bool drawDebug, const PostProcessing &postProcessing, bool isGameCamera);
        void EnsureWidgetRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height);
        void EnsureGizmoRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height);
        void EnsureDebugRT(Ref<CameraRenderTarget> target, uint32_t renderWidth, uint32_t renderHeight);
        void EnsureTAAHistoryRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height);
        void EnsureMSAARenderTargets(Ref<CameraRenderTarget> target, int desiredSampleCount, uint32_t renderWidth, uint32_t renderHeight);

        void ShadowPass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext);
        void LocalLightShadowPass(nri::CommandBuffer *cmd, FrameContext *frameContext);
        void EnsureLocalShadowResources();
        void ShutdownLocalShadowResources();
        void UpdateSkyIBL(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext);
        void RenderDebugProbes(nri::CommandBuffer *cmd, RenderTarget *framebuffer, FrameContext *frameContext);
        void ColorPass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext, RenderTarget *framebuffer, RenderTarget *selectionFramebuffer, bool drawDebug);
        void UIPass(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext);
        void DebugPass(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext);
        void CompositePass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext, Ref<CameraRenderTarget> target, const CameraLens &lens, const PostProcessing &postProcessing, Ref<Texture> edgeTexture = nullptr, Ref<Texture> bloomTexture = nullptr, Ref<Texture> ssaoTexture = nullptr, Ref<Texture> gizmoTexture = nullptr, bool msaaResolved = false, RenderTarget *targetFramebuffer = nullptr);
        void FXAAPass(nri::CommandBuffer *cmd, RenderTarget *framebuffer, Ref<Texture> inputTexture);

        void DrawDebugGrid(nri::CommandBuffer *cmd, RenderTarget *framebuffer, FrameContext *frameContext, const DebugGridStyle &style, int planeMode);
        void DrawDebug2D(nri::CommandBuffer *cmd, RenderTarget *framebuffer, FrameContext *frameContext);
        void DrawDebug3D(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext);

        /// Flush all accumulated opaque static-mesh batches to the command list.
        void FlushOpaqueBatches(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *framebuffer);

        /// Flush all accumulated shadow (CSM) static-mesh batches to the command list.
        void FlushShadowBatches(nri::CommandBuffer *cmd, FrameContext *frameContext, uint32_t cascadeIndex);

        void FlushBatches(BatchBuilder &builder, nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *framebuffer);

        // Depth + normal prepass / GTAO / SSR
        void EnsureDepthPrepassRT(Ref<CameraRenderTarget> target, uint32_t renderWidth, uint32_t renderHeight);
        void EnsurePrevSceneColor(Ref<CameraRenderTarget> target, uint32_t renderWidth, uint32_t renderHeight);
        std::vector<Ref<GTAO>> GetOrCreateGTAOs(ICamera *camera, uint32_t renderWidth, uint32_t renderHeight);
        std::vector<Ref<SSR>> GetOrCreateSSRs(ICamera *camera, uint32_t renderWidth, uint32_t renderHeight);
        void DepthPrepass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext, RenderTarget *depthPrepassFB);
        Ref<GraphicsPipeline> GetOrCreateDepthPrepassPSO(std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            RenderTarget *framebuffer, nri::CullMode cullMode, const char *vertexShaderPath, EBindingLayout meshLayout);
        Ref<GraphicsPipeline> GetStaticDepthPrepassPSO(RenderTarget *framebuffer, nri::CullMode cullMode = nri::CullMode::FRONT);
        Ref<GraphicsPipeline> GetAnimatedDepthPrepassPSO(RenderTarget *framebuffer, nri::CullMode cullMode = nri::CullMode::FRONT);
        bool IsEntitySelected(entt::entity e) const;
        bool HasSelectedGIEntity() const;

    private:
        struct TransparentDrawCall
        {
            Ref<BindingSet> meshBindingSet;
            Ref<BindingSet> materialBindingSet;
            nri::Buffer *vertexBuffer = nullptr;
            nri::Buffer *indexBuffer = nullptr;
            uint32_t indexCount = 0;
            uint32_t pushConstants_ObjectIndex = 0;
            float distanceToCamera = 0.0f;
            bool isSkeletal = false;
            Mesh_GPUData gpuData;
            Ref<MeshInstance> meshInstance;
            nri::CullMode cullMode = nri::CullMode::NONE;
        };

        template<typename MeshT>
        void DrawMesh(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *framebuffer, const Ref<MeshT> &mesh, const glm::mat4 &parentTransform,
            const glm::mat4 &normalMatrix, uint32_t objectID, const std::vector<glm::mat4> &boneTransforms,
            const std::vector<Mesh_GPUData> &cachedInstanceTransforms, ICamera *camera, Ref<GraphicsPipeline> opaquePSO, std::vector<TransparentDrawCall> &transparentDrawCalls,
            std::unordered_set<Material *> &uploadedMaterialsThisPass, entt::entity entity = entt::null, const std::string &socketName = "");

        template<typename MeshT>
        void DrawMeshShadow(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *csmFramebuffer, const Ref<MeshT> &mesh, const glm::mat4 &parentTransform,
            const glm::mat4 &normalMatrix, uint32_t objectID, const std::vector<glm::mat4> &boneTransforms, const std::vector<Mesh_GPUData> &cachedInstanceTransforms,
            const Ref<GraphicsPipeline> &csmPSO, uint32_t cascadeIndex, entt::entity entity = entt::null, const std::string &socketName = "");

        Ref<GraphicsPipeline> GetOrCreateMeshPSO(std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode, const char *vertexShaderPath, const char *pixelShaderPath,
            EBindingLayout meshLayout, bool transparent, bool depthWrite = true);

        Ref<GraphicsPipeline> GetOrCreateCMSPSO(std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            RenderTarget *framebuffer, const char *vertexShaderPath, const char *pixelShaderPath, EBindingLayout meshLayout);

        Ref<GraphicsPipeline> GetDebugGridPSO(RenderTarget *framebuffer);
        Ref<GraphicsPipeline> GetAnimatedPSO(RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode = nri::CullMode::FRONT);
        Ref<GraphicsPipeline> GetAnimatedTransparentPSO(RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode = nri::CullMode::NONE);

        Ref<GraphicsPipeline> GetStaticPSO(RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode = nri::CullMode::FRONT);
        Ref<GraphicsPipeline> GetStaticTransparentPSO(RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode = nri::CullMode::NONE);

        Ref<GraphicsPipeline> GetOrCreateSelectMeshPSO(std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            RenderTarget *framebuffer, nri::FillMode fillMode, const char *vertexShaderPath, const char *pixelShaderPath,
            EBindingLayout meshLayout);
        Ref<GraphicsPipeline> GetStaticSelectPSO(RenderTarget *framebuffer, nri::FillMode fillMode);
        Ref<GraphicsPipeline> GetAnimatedSelectPSO(RenderTarget *framebuffer, nri::FillMode fillMode);
        void RenderSelectedEntitiesIDOverlay(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext, RenderTarget *framebuffer, std::unordered_set<Material *> &uploadedMaterialsThisPass);

        Ref<GraphicsPipeline> GetAnimatedCSMPSO();
        Ref<GraphicsPipeline> GetStaticCSMPSO();

        Ref<GraphicsPipeline> GetEnvironmentPSO(RenderTarget *framebuffer, nri::FillMode fillMode);
        Ref<GraphicsPipeline> GetCompositePSO(RenderTarget *framebuffer, nri::FillMode fillMode);
        Ref<GraphicsPipeline> GetFXAAPSO(RenderTarget *framebuffer);
        Ref<GraphicsPipeline> GetDebugProbePSO(RenderTarget *framebuffer, nri::FillMode fillMode);
        void EnsureSkySHBuffers();
        void EnsureDebugProbeBuffers(size_t requiredCount);
        void ShutdownGIResources();

        Ref<BindingSet> GetOrCreateDebugGridBindingSet(nri::PipelineLayout *pipelineLayout, const ConstantBuffer &cameraBuffer, const ConstantBuffer &gridBuffer);
        Ref<BindingSet> GetOrCreateCompositeBindingSet(nri::PipelineLayout *pipelineLayout, Ref<CameraRenderTarget> target, Ref<Texture> edgeTexture,
            Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, Ref<Texture> taaHistoryTexture, Ref<Texture> gizmoTexture, const ConstantBuffer &postProcessBuffer, nri::Descriptor *sampler, bool useResolvedScene = false);

        Ref<CameraRenderTarget> GetOrCreateRenderTarget(ICamera *camera);
        std::vector<Ref<Bloom>> GetOrCreateBlooms(ICamera *camera);
        std::vector<Ref<SSAO>> GetOrCreateSSAOs(nri::CommandBuffer *cmd, ICamera *camera);

    private:
        Ref<WidgetRenderer> m_WidgetRenderer;
        Ref<Renderer2D> m_ScreenTextRenderer2D;
        Ref<Renderer2D> m_DebugRenderer2D;
        Ref<ConstantBuffer> m_ScreenCameraBuffer;

        std::unordered_map<ICamera *, Ref<CameraRenderTarget>> m_RenderTargets;
        std::unordered_map<ICamera *, std::vector<Ref<Bloom>>> m_Blooms;
        std::unordered_map<ICamera *, std::vector<Ref<SSAO>>> m_SSAOs;
        std::unordered_map<ICamera *, std::vector<Ref<GTAO>>> m_GTAOs;
        std::unordered_map<ICamera *, std::vector<Ref<SSR>>> m_SSRs;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_StaticDepthPrepassPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_AnimatedDepthPrepassPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_StaticReadOnlyDepthPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_AnimatedReadOnlyDepthPSOCache;
        bool m_ColorPassDepthReadOnly = false;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_AnimatedPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_TransparentAnimatedPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_AnimatedCSMPSOCache;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_StaticPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_TransparentStaticPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_StaticCSMPSOCache;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_SelectStaticPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_SelectAnimatedPSOCache;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_EnvironmentPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_CompositePSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_FXAAPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_DebugGridPSOCache;

        std::unordered_map<CompositeBindingKey, Ref<BindingSet>, CompositeBindingKeyHash> m_CompositeBindingSetCache;
        std::unordered_map<DebugGridBindingKey, Ref<BindingSet>, DebugGridBindingKeyHash> m_DebugGridBindingSetCache;
        std::unordered_map<FXAABindingKey, Ref<BindingSet>, FXAABindingKeyHash> m_FXAABindingSetCache;

        std::unordered_map<ICamera *, CameraWidgetInputState> m_CameraInputStates;

        uint32_t m_EditorWidgetMouseX = 0;
        uint32_t m_EditorWidgetMouseY = 0;
        bool m_EditorWidgetMouseHovered = false;
        bool m_UseEditorWidgetMouseOverride = false;

        uint32_t m_GameplayWidgetMouseX = 0;
        uint32_t m_GameplayWidgetMouseY = 0;
        bool m_GameplayWidgetMouseHovered = false;
        bool m_UseGameplayWidgetMouseOverride = false;

        Ref<Material> m_RuntimeMaterial;

        uint64_t m_TAAFrameIndex = 0;
        bool m_ResizeWaitedThisFrame = false;
        uint32_t m_ViewsRenderedThisFrame = 0;

        std::unordered_map<entt::entity, std::vector<uint32_t>> m_EntityObjectIndexCache;
        std::unordered_map<entt::entity, uint32_t> m_EntityBoneOffsetCache;
        std::map<std::pair<entt::entity, std::string>, std::vector<uint32_t>> m_SocketObjectIndexCache;

        Ref<TerrainRenderer> m_TerrainRenderer;

        // -----------------------------------------------------------------------
        // Batch builders for GPU instancing of opaque static meshes.
        // Populated during ColorPass/ShadowPass, flushed at end of each pass.
        // -----------------------------------------------------------------------
        BatchBuilder m_OpaqueBatchBuilder;   // Color pass opaque static meshes
        BatchBuilder m_ShadowBatchBuilder;   // Shadow pass (CSM) static meshes
        BatchBuilder m_DepthPrepassBatchBuilder; // Depth + normal prepass static meshes
        FramePacket m_FramePacket;

        std::array<GPUTimer, 6> m_GPUTimers;
        std::unordered_map<std::string, double> m_LastGPUTimingsMs;
        GI_GPUData m_GIGPUData{};

        struct SkyIBLState
        {
            bool valid = false;
            int32_t skyType = -1;
            nri::Texture *srcTexture = nullptr;
            glm::vec4 sunColor = glm::vec4(0.0f);
            glm::vec3 sunDir = glm::vec3(0.0f);
            uint64_t atmoHash = 0;
            float altitudeKm = 0.0f;
        } m_SkyIBLState;

        Ref<IBLPrefilter> m_IBLPrefilter;
        Ref<Texture> m_SkyRawCube;
        Ref<Texture> m_SkyPrefilterCube;

        struct SkySHReadbackSlot
        {
            nri::Buffer *readbackBuffer = nullptr;
            bool pending = false;
        };
        nri::Buffer *m_SkySHGpuBuffer = nullptr;
        nri::Descriptor *m_SkySHGpuUAV = nullptr;
        bool m_SkySHGpuWritten = false;
        std::array<SkySHReadbackSlot, 4> m_SkySHReadbacks{};
        gi::SHL2 m_CachedSkySH{};
        bool m_HasValidSkySH = false;

        std::vector<DebugProbeGPU> m_DebugProbes;
        nri::Buffer *m_DebugProbeBuffer = nullptr;
        nri::Descriptor *m_DebugProbeSRV = nullptr;
        uint8_t *m_DebugProbeMapped = nullptr;
        size_t m_DebugProbeCapacity = 0;
        BindingLayoutDesc m_DebugProbeLayout;
        nri::PipelineLayout *m_DebugProbePipelineLayout = nullptr;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_DebugProbePSOCache;

        Ref<IrradianceVolume> m_ActiveIrradianceVolume;
        std::string m_LoadedIrradiancePath;

        Ref<ReflectionProbeSet> m_ActiveReflectionProbeSet;
        std::string m_LoadedReflectionSignature;

        // Runtime Static (cached by hash) + Dynamic (movable casters) Point & Spot Light shadow cube arrays
        static constexpr uint32_t kMaxLocalShadowPointLights = 8;
        static constexpr uint32_t kMaxLocalShadowSpotLights = 4;
        static constexpr uint32_t kMaxLocalShadowLights = kMaxLocalShadowPointLights + kMaxLocalShadowSpotLights;
        static constexpr uint32_t kLocalShadowResolution = 256;

        Ref<Texture> m_StaticLocalShadowCubeArray;
        Ref<Texture> m_StaticLocalShadowDepthTex;
        std::array<Ref<RenderTarget>, kMaxLocalShadowLights * 6> m_StaticLocalShadowFaceRTs;
        std::array<Ref<ConstantBuffer>, kMaxLocalShadowLights * 6> m_StaticLocalShadowFaceCameraBuffers;
        uint64_t m_StaticLocalShadowHash = 0;
        bool m_StaticLocalShadowValid = false;
        bool m_StaticLocalShadowInitialized = false;

        Ref<Texture> m_DynamicLocalShadowCubeArray;
        Ref<Texture> m_DynamicLocalShadowDepthTex;
        std::array<Ref<RenderTarget>, kMaxLocalShadowLights * 6> m_DynamicLocalShadowFaceRTs;
        std::array<Ref<ConstantBuffer>, kMaxLocalShadowLights * 6> m_DynamicLocalShadowFaceCameraBuffers;
        bool m_HasDynamicLocalShadows = false;
        bool m_DynamicLocalShadowInitialized = false;

        Ref<GraphicsPipeline> m_LocalShadowStaticPSO;
        Ref<GraphicsPipeline> m_LocalShadowAnimPSO;
        std::unordered_map<FrameContext *, std::array<Ref<BindingSet>, kMaxLocalShadowLights * 6>> m_StaticLocalShadowStaticBindingSets;
        std::unordered_map<FrameContext *, std::array<Ref<BindingSet>, kMaxLocalShadowLights * 6>> m_StaticLocalShadowAnimBindingSets;
        std::unordered_map<FrameContext *, std::array<Ref<BindingSet>, kMaxLocalShadowLights * 6>> m_DynamicLocalShadowStaticBindingSets;
        std::unordered_map<FrameContext *, std::array<Ref<BindingSet>, kMaxLocalShadowLights * 6>> m_DynamicLocalShadowAnimBindingSets;

        Ref<GraphicsPipeline> GetOrCreateLocalShadowPSO(bool animated);
        Ref<BindingSet> GetOrCreateLocalShadowBindingSet(
            std::unordered_map<FrameContext *, std::array<Ref<BindingSet>, kMaxLocalShadowLights * 6>> &cache,
            FrameContext *frameContext, uint32_t slice, ConstantBuffer *camBuf, bool animated);
        void RenderLocalShadowPassCube(nri::CommandBuffer *cmd, FrameContext *frameContext,
            const std::vector<LocalShadowLightItem> &lights, bool staticOnly, bool includeMovableOnly,
            std::array<Ref<RenderTarget>, kMaxLocalShadowLights * 6> &faceRTs,
            std::array<Ref<ConstantBuffer>, kMaxLocalShadowLights * 6> &faceCamBuffers,
            std::unordered_map<FrameContext *, std::array<Ref<BindingSet>, kMaxLocalShadowLights * 6>> &staticBindingCache,
            std::unordered_map<FrameContext *, std::array<Ref<BindingSet>, kMaxLocalShadowLights * 6>> &animBindingCache,
            const Ref<GraphicsPipeline> &staticPSO, const Ref<GraphicsPipeline> &animPSO);
    };
}

#endif
