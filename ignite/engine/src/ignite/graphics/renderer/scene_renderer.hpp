// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_RENDERER_HPP
#define IGN_SCENE_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "iscene_renderer.hpp"
#include "ignite/graphics/hash_keys.hpp"
#include "batch_builder.hpp"
#include "ignite/terrain/terrain_renderer.hpp"
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

	struct CameraRenderTarget
	{
		Ref<RenderTarget> sceneRT;         // MSAA render target (sampleCount > 1) or regular
		Ref<RenderTarget> sceneResolvedRT; // Single-sample resolve target (only used when MSAA is active)
		Ref<RenderTarget> widgetRT;
		Ref<RenderTarget> compositeRT;
		Ref<RenderTarget> debugRT;
        Ref<RenderTarget> taaHistoryRT[3];
        bool taaHistoryValid = false;
        int msaaSampleCount = 1;           // Tracks the current MSAA sample count (1 = no MSAA)
	};

    struct RenderPlan
    {
        bool hasMeshes = false;
        bool has2D = false;
        bool hasTerrain = false;
        bool hasEnvironment = false;
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
            return hasMeshes || has2D || hasTerrain || hasEnvironment;
        }

        bool IsEmptyScene() const
        {
            return !HasRenderables() && !hasWidgets && !requiresDebugOverlay && !requiresObjectId;
        }
    };

    class IGN_API SceneRenderer : public ISceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();

        void BeginFrame();
        void SetActiveScene(const Ref<Scene> &scene);
        
        void Render(ICamera *camera, FrameContext *frameContext, bool drawDebug);
        
        void SetCameraWidgetMousePosition(ICamera *camera, uint32_t mouseX, uint32_t mouseY, bool hovered);
        void SetEditorWidgetMousePosition(uint32_t mouseX, uint32_t mouseY, bool hovered);
        void SetGameplayWidgetMousePosition(uint32_t mouseX, uint32_t mouseY, bool hovered);

        virtual void ResizeFramebuffer(ICamera *camera, uint32_t width, uint32_t height) override;
        void SetFillMode(nvrhi::RasterFillMode mode);

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

        void PreallocateGPUData(nvrhi::ICommandList *cmd, FrameContext *frameContext);

    private:
        RenderPlan BuildRenderPlan(ICamera *camera, bool drawDebug, const PostProcessing &postProcessing, bool isGameCamera);
        void EnsureWidgetRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height);
        void EnsureDebugRT(Ref<CameraRenderTarget> target, uint32_t renderWidth, uint32_t renderHeight);
        void EnsureTAAHistoryRT(Ref<CameraRenderTarget> target, uint32_t width, uint32_t height);

        void ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera, FrameContext *frameContext);
        void ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, FrameContext *frameContext, nvrhi::IFramebuffer *framebuffer, bool drawDebug);
        void UIPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext);
		void DebugPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext);
        void CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, FrameContext *frameContext, Ref<CameraRenderTarget> target, const CameraLens &lens, const PostProcessing &postProcessing, Ref<Texture> edgeTexture = nullptr, Ref<Texture> bloomTexture = nullptr, Ref<Texture> ssaoTexture = nullptr, bool msaaResolved = false);

        void DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext, const DebugGridStyle &style, bool is2D);
        void DrawDebug2D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebufferm, FrameContext *frameContext);
        void DrawDebug3D(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer, FrameContext *frameContext);

        /// Flush all accumulated opaque static-mesh batches to the command list.
        void FlushOpaqueBatches(nvrhi::ICommandList *cmd, FrameContext *frameContext, nvrhi::IFramebuffer *framebuffer);

        /// Flush all accumulated shadow (CSM) static-mesh batches to the command list.
        void FlushShadowBatches(nvrhi::ICommandList *cmd, FrameContext *frameContext, uint32_t cascadeIndex);

    private:
        struct TransparentDrawCall
        {
            nvrhi::BindingSetHandle meshBindingSet;
            nvrhi::BindingSetHandle materialBindingSet;
            nvrhi::BufferHandle vertexBuffer;
            nvrhi::BufferHandle indexBuffer;
            uint32_t indexCount;
            uint32_t pushConstants_ObjectIndex;
            float distanceToCamera;
            bool isSkeletal;
            Mesh_GPUData gpuData;
            Ref<MeshInstance> meshInstance;
            glm::mat4 bones[MAX_BONES];
        };

        template<typename MeshT>
        void DrawMesh(nvrhi::ICommandList *cmd, FrameContext *frameContext, nvrhi::IFramebuffer *framebuffer, const Ref<MeshT> &mesh, const glm::mat4 &parentTransform,
            const glm::mat4 &normalMatrix, uint32_t objectID, const std::unordered_map<int, AssetHandle> &overrideMaterials, const std::vector<glm::mat4> &boneTransforms,
            const std::vector<Mesh_GPUData> &cachedInstanceTransforms, ICamera *camera, Ref<GraphicsPipeline> opaquePSO, std::vector<TransparentDrawCall> &transparentDrawCalls,
            std::unordered_set<Material *> &uploadedMaterialsThisPass, entt::entity entity = entt::null, const std::string &socketName = "");

        template<typename MeshT>
        void DrawMeshShadow( nvrhi::ICommandList *cmd, FrameContext *frameContext, const Ref<MeshT> &mesh, const glm::mat4 &parentTransform,
            const glm::mat4 &normalMatrix, uint32_t objectID, const std::vector<glm::mat4> &boneTransforms, const std::vector<Mesh_GPUData> &cachedInstanceTransforms,
            nvrhi::GraphicsState &csmState, uint32_t cascadeIndex, entt::entity entity = entt::null, const std::string &socketName = "");

        Ref<GraphicsPipeline> GetOrCreateMeshPSO(std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode, const char *vertexShaderPath, const char *pixelShaderPath,
            EBindingLayout meshLayout, bool transparent);

		Ref<GraphicsPipeline> GetOrCreateCMSPSO(std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            nvrhi::IFramebuffer *framebuffer, const char *vertexShaderPath,  const char *pixelShaderPath, EBindingLayout meshLayout);

        Ref<GraphicsPipeline> GetDebugGridPSO(nvrhi::IFramebuffer *framebuffer);
        Ref<GraphicsPipeline> GetAnimatedPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetAnimatedTransparentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);

        Ref<GraphicsPipeline> GetStaticPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
		Ref<GraphicsPipeline> GetStaticTransparentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);

        Ref<GraphicsPipeline> GetOrCreateSelectMeshPSO(std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode, const char *vertexShaderPath, const char *pixelShaderPath,
            EBindingLayout meshLayout);
        Ref<GraphicsPipeline> GetStaticSelectPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetAnimatedSelectPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        void RenderSelectedEntitiesIDOverlay(nvrhi::ICommandList *cmd, ICamera *camera, FrameContext *frameContext, nvrhi::IFramebuffer *framebuffer, std::unordered_set<Material *> &uploadedMaterialsThisPass);

		Ref<GraphicsPipeline> GetAnimatedCSMPSO();
		Ref<GraphicsPipeline> GetStaticCSMPSO();

        Ref<GraphicsPipeline> GetEnvironmentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetCompositePSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);

        nvrhi::BindingSetHandle GetOrCreateDebugGridBindingSet(nvrhi::IBindingLayout *bindingLayout, const nvrhi::BufferHandle &cameraBuffer, const nvrhi::BufferHandle &gridBuffer);
        nvrhi::BindingSetHandle GetOrCreateCompositeBindingSet(nvrhi::IBindingLayout *bindingLayout, Ref<CameraRenderTarget> target, Ref<Texture> edgeTexture,
            Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, Ref<Texture> taaHistoryTexture, const nvrhi::BufferHandle &postProcessBuffer, nvrhi::ISampler *sampler, bool useResolvedScene = false);
        
        Ref<CameraRenderTarget> GetOrCreateRenderTarget(ICamera *camera);
		std::vector<Ref<Bloom>> GetOrCreateBlooms(ICamera *camera);
		std::vector<Ref<SSAO>> GetOrCreateSSAOs(nvrhi::ICommandList *cmd, ICamera *camera);

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
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_DebugGridPSOCache;

        std::unordered_map<CompositeBindingKey, nvrhi::BindingSetHandle, CompositeBindingKeyHash> m_CompositeBindingSetCache;
        std::unordered_map<DebugGridBindingKey, nvrhi::BindingSetHandle, DebugGridBindingKeyHash> m_DebugGridBindingSetCache;

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
