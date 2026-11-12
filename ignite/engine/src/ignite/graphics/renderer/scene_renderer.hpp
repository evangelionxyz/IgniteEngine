// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_RENDERER_HPP
#define IGN_SCENE_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "iscene_renderer.hpp"
#include "ignite/graphics/hash_keys.hpp"
#include "batch_builder.hpp"
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
        bool hasActiveShadows = false;
        bool requiresObjectId = false;
        bool requiresDebugOverlay = false;
        bool requiresGrid = false;
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
            return !HasRenderables() && !hasWidgets && !requiresDebugOverlay && !requiresObjectId;
        }
    };

    struct CameraRenderTarget
    {
        Ref<RenderTarget> sceneRT;         // MSAA render target (sampleCount > 1) or regular
        Ref<RenderTarget> sceneResolvedRT; // Single-sample resolve target (only used when MSAA is active)
        Ref<RenderTarget> selectionRT;     // Single-sample selected-object ID mask for outlines
        Ref<RenderTarget> widgetRT;
        Ref<RenderTarget> postProcessRT;  // Intermediate LDR target for tonemapping before FXAA/UI
        Ref<RenderTarget> compositeRT;
        Ref<RenderTarget> debugRT;
        Ref<RenderTarget> taaHistoryRT[3];

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

        virtual Ref<Texture> GetEnvironmentMapColorTexture() const override;
        virtual Ref<Texture> GetCascadedShadowMapDepthTexture() const override;

        virtual Ref<CascadedShadowMap> GetCascadedShadowMap() override;
        Ref<Renderer2D> &GetRenderer2D() { return m_Renderer2D; }
        Ref<TerrainRenderer> &GetTerrainRenderer() { return m_TerrainRenderer; }

        Ref<CameraRenderTarget> GetRenderTarget(ICamera *camera);

        void PreallocateGPUData(nri::CommandBuffer *cmd, FrameContext *frameContext);

    private:
        void ClearResources();

        RenderPlan BuildRenderPlan(ICamera *camera, bool drawDebug, const PostProcessing &postProcessing, bool isGameCamera);
        void EnsureWidgetRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height);
        void EnsureDebugRT(Ref<CameraRenderTarget> target, uint32_t renderWidth, uint32_t renderHeight);
        void EnsureTAAHistoryRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height);
        void EnsureMSAARenderTargets(Ref<CameraRenderTarget> target, int desiredSampleCount, uint32_t renderWidth, uint32_t renderHeight);

        void ShadowPass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext);
        void ColorPass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext, RenderTarget *framebuffer, RenderTarget *selectionFramebuffer, bool drawDebug);
        void UIPass(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext);
        void DebugPass(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext);
        void CompositePass(nri::CommandBuffer *cmd, ICamera *camera, FrameContext *frameContext, Ref<CameraRenderTarget> target, const CameraLens &lens, const PostProcessing &postProcessing, Ref<Texture> edgeTexture = nullptr, Ref<Texture> bloomTexture = nullptr, Ref<Texture> ssaoTexture = nullptr, bool msaaResolved = false, RenderTarget *targetFramebuffer = nullptr);
        void FXAAPass(nri::CommandBuffer *cmd, RenderTarget *framebuffer, Ref<Texture> inputTexture);

        void DrawDebugGrid(nri::CommandBuffer *cmd, RenderTarget *framebuffer, FrameContext *frameContext, const DebugGridStyle &style, bool is2D);
        void DrawDebug2D(nri::CommandBuffer *cmd, RenderTarget *framebuffer, FrameContext *frameContext);
        void DrawDebug3D(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *framebuffer, FrameContext *frameContext);

        /// Flush all accumulated opaque static-mesh batches to the command list.
        void FlushOpaqueBatches(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *framebuffer);

        /// Flush all accumulated shadow (CSM) static-mesh batches to the command list.
        void FlushShadowBatches(nri::CommandBuffer *cmd, FrameContext *frameContext, uint32_t cascadeIndex);

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
            glm::mat4 bones[MAX_BONES];
            nri::CullMode cullMode = nri::CullMode::NONE;
        };

        template<typename MeshT>
        void DrawMesh(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *framebuffer, const Ref<MeshT> &mesh, const glm::mat4 &parentTransform,
            const glm::mat4 &normalMatrix, uint32_t objectID, const std::unordered_map<int, AssetHandle> &overrideMaterials, const std::vector<glm::mat4> &boneTransforms,
            const std::vector<Mesh_GPUData> &cachedInstanceTransforms, ICamera *camera, Ref<GraphicsPipeline> opaquePSO, std::vector<TransparentDrawCall> &transparentDrawCalls,
            std::unordered_set<Material *> &uploadedMaterialsThisPass, entt::entity entity = entt::null, const std::string &socketName = "");

        template<typename MeshT>
        void DrawMeshShadow(nri::CommandBuffer *cmd, FrameContext *frameContext, RenderTarget *csmFramebuffer, const Ref<MeshT> &mesh, const glm::mat4 &parentTransform,
            const glm::mat4 &normalMatrix, uint32_t objectID, const std::vector<glm::mat4> &boneTransforms, const std::vector<Mesh_GPUData> &cachedInstanceTransforms,
            const Ref<GraphicsPipeline> &csmPSO, uint32_t cascadeIndex, entt::entity entity = entt::null, const std::string &socketName = "");

        Ref<GraphicsPipeline> GetOrCreateMeshPSO(std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            RenderTarget *framebuffer, nri::FillMode fillMode, nri::CullMode cullMode, const char *vertexShaderPath, const char *pixelShaderPath,
            EBindingLayout meshLayout, bool transparent);

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

        Ref<BindingSet> GetOrCreateDebugGridBindingSet(nri::PipelineLayout *pipelineLayout, const ConstantBuffer &cameraBuffer, const ConstantBuffer &gridBuffer);
        Ref<BindingSet> GetOrCreateCompositeBindingSet(nri::PipelineLayout *pipelineLayout, Ref<CameraRenderTarget> target, Ref<Texture> edgeTexture,
            Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, Ref<Texture> taaHistoryTexture, const ConstantBuffer &postProcessBuffer, nri::Descriptor *sampler, bool useResolvedScene = false);

        Ref<CameraRenderTarget> GetOrCreateRenderTarget(ICamera *camera);
        std::vector<Ref<Bloom>> GetOrCreateBlooms(ICamera *camera);
        std::vector<Ref<SSAO>> GetOrCreateSSAOs(nri::CommandBuffer *cmd, ICamera *camera);

    private:
        Ref<WidgetRenderer> m_WidgetRenderer;

        std::unordered_map<ICamera *, Ref<CameraRenderTarget>> m_RenderTargets;
        std::unordered_map<ICamera *, std::vector<Ref<Bloom>>> m_Blooms;
        std::unordered_map<ICamera *, std::vector<Ref<SSAO>>> m_SSAOs;

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
    };
}

#endif
