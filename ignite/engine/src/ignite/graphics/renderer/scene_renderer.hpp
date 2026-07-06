// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_RENDERER_HPP
#define IGN_SCENE_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "iscene_renderer.hpp"

#include "ignite/graphics/hash_keys.hpp"

namespace ignite
{
    class WidgetRenderer;
    class SkeletalMeshComponent;

	struct CameraRenderTarget
	{
		Ref<RenderTarget> sceneRT;
		Ref<RenderTarget> widgetRT;
		Ref<RenderTarget> compositeRT;
		Ref<RenderTarget> debugRT;
	};

    class IGN_API SceneRenderer : public ISceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();

        void BeginFrame();
        void SetActiveScene(const Ref<Scene> &scene);
        
        void Render(ICamera *camera, bool drawDebug);
        // void RenderGameplayTo(ICamera *camera);
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

        Ref<CameraRenderTarget> GetRenderTarget(ICamera *camera);

    private:
        void ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera);
        void ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer);
        void UIPass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
		void DebugPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer);
        void CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, Ref<CameraRenderTarget> target, const CameraLens &lens, const PostProcessing &postProcessing, Ref<Texture> edgeTexture = nullptr, Ref<Texture> bloomTexture = nullptr, Ref<Texture> ssaoTexture = nullptr);

        void DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const DebugGridStyle &style, bool is2D);
        void DrawDebug2D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void DrawDebug3D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);

    private:
        struct TransparentDrawCall
        {
            nvrhi::BindingSetHandle meshBindingSet;
            nvrhi::BindingSetHandle materialBindingSet;
            nvrhi::BufferHandle vertexBuffer;
            nvrhi::BufferHandle indexBuffer;
            uint32_t indexCount;
            float distanceToCamera;
            bool isSkeletal;
        };

        template<typename MeshT>
        void DrawMesh(
            nvrhi::ICommandList *cmd,
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
            std::unordered_set<Material *> &uploadedMaterialsThisPass);

        template<typename MeshT>
        void DrawMeshShadow(
            nvrhi::ICommandList *cmd,
            const Ref<MeshT> &mesh,
            const glm::mat4 &parentTransform,
            const glm::mat4 &normalMatrix,
            uint32_t objectID,
            const std::vector<glm::mat4> &boneTransforms,
            const std::vector<Mesh_GPUData> &cachedInstanceTransforms,
            nvrhi::GraphicsState &csmState,
            const Ref<ConstantBuffer> &csmBuffer);

        Ref<GraphicsPipeline> GetOrCreateMeshPSO(
            std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            nvrhi::IFramebuffer *framebuffer,
            nvrhi::RasterFillMode fillMode,
            const char *vertexShaderPath,
            const char *pixelShaderPath,
            EBindingLayout meshLayout,
            bool transparent);

		Ref<GraphicsPipeline> GetOrCreateCMSPSO(
			std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> &cache,
            nvrhi::IFramebuffer *framebuffer,
            const char *vertexShaderPath, 
            const char *pixelShaderPath,
			EBindingLayout meshLayout
        );

        Ref<GraphicsPipeline> GetDebugGridPSO(nvrhi::IFramebuffer *framebuffer);
        Ref<GraphicsPipeline> GetAnimatedPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetAnimatedTransparentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);

        Ref<GraphicsPipeline> GetStaticPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
		Ref<GraphicsPipeline> GetStaticTransparentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);

		Ref<GraphicsPipeline> GetAnimatedCSMPSO();
		Ref<GraphicsPipeline> GetStaticCSMPSO();

        Ref<GraphicsPipeline> GetEnvironmentPSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetCompositePSO(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);

        nvrhi::BindingSetHandle GetOrCreateDebugGridBindingSet(nvrhi::IBindingLayout *bindingLayout, const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &gridBuffer);
        nvrhi::BindingSetHandle GetOrCreateCompositeBindingSet(nvrhi::IBindingLayout *bindingLayout, Ref<CameraRenderTarget> target, Ref<Texture> edgeTexture, Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, Ref<ConstantBuffer> postProcessBuffer, nvrhi::ISampler *sampler);
        nvrhi::BindingSetHandle GetOrCreateCSMBindingSet(nvrhi::IBindingLayout *bindingLayout, Ref<ConstantBuffer> skinnedMeshGPUDataBuffer, Ref<ConstantBuffer> csmGPUDataBuffer);
        
        Ref<CameraRenderTarget> GetOrCreateRenderTarget(ICamera *camera);

		virtual void AddAssetPin(AssetHandle handle) override;
		virtual std::string_view BuildAssetPinName(AssetHandle handle) override;

    private:
        Ref<WidgetRenderer> m_WidgetRenderer;

        std::unordered_map<ICamera *, Ref<CameraRenderTarget>> m_RenderTargets;

        Ref<Bloom> m_EditorBloom;
        Ref<Bloom> m_GameplayBloom;

        Ref<SSAO> m_EditorSSAO;
        Ref<SSAO> m_GameplaySSAO;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_AnimatedPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_TransparentAnimatedPSOCache;
		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_AnimatedCSMPSOCache;

		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_StaticPSOCache;
		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_TransparentStaticPSOCache;
		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_StaticCSMPSOCache;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_EnvironmentPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_CompositePSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_DebugGridPSOCache;

        std::unordered_map<CompositeBindingKey, nvrhi::BindingSetHandle, CompositeBindingKeyHash> m_CompositeBindingSetCache;
        std::unordered_map<DebugGridBindingKey, nvrhi::BindingSetHandle, DebugGridBindingKeyHash> m_DebugGridBindingSetCache;

        uint32_t m_EditorWidgetMouseX = 0;
        uint32_t m_EditorWidgetMouseY = 0;
        bool m_EditorWidgetMouseHovered = false;
        bool m_UseEditorWidgetMouseOverride = false;

        uint32_t m_GameplayWidgetMouseX = 0;
        uint32_t m_GameplayWidgetMouseY = 0;
        bool m_GameplayWidgetMouseHovered = false;
        bool m_UseGameplayWidgetMouseOverride = false;
    };
}

#endif
