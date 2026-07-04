// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_RENDERER_HPP
#define IGN_SCENE_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "iscene_renderer.hpp"

#include "ignite/graphics/framebuffer_key.hpp"

namespace ignite
{
    class WidgetRenderer;
    class MeshComponent;

	struct CameraRenderTarget
	{
		Ref<RenderTarget> sceneRT;
		Ref<RenderTarget> widgetRT;
		Ref<RenderTarget> compositeRT;
		Ref<RenderTarget> debugRT;
	};

	struct DebugGridBindingKey
	{
		nvrhi::IBindingLayout *layout = nullptr;
		nvrhi::IBuffer *gridBuffer = nullptr;

		bool operator==(const DebugGridBindingKey &other) const noexcept
		{
			return layout == other.layout && gridBuffer == other.gridBuffer;
		}
	};

	struct DebugGridBindingKeyHash
	{
		size_t operator()(const DebugGridBindingKey &k) const noexcept
		{
			size_t h = std::hash<const void *>{}(k.layout);
			h ^= (std::hash<const void *>{}(k.gridBuffer) + 0x9e3779b9 + (h << 6) + (h >> 2));
			return h;
		}
	};

	struct CompositeBindingKey
	{
		nvrhi::IBindingLayout *layout = nullptr;
		nvrhi::ITexture *sceneTex = nullptr;
		nvrhi::ITexture *uiTex = nullptr;
		nvrhi::ITexture *edgeTex = nullptr;
		nvrhi::ITexture *bloomTex = nullptr;
		nvrhi::ITexture *ssaoTex = nullptr;
		nvrhi::ITexture *depthTex = nullptr;
		nvrhi::ITexture *debugTex = nullptr;
		nvrhi::IBuffer *postProcessBuffer = nullptr;
		nvrhi::ISampler *sampler = nullptr;

		bool operator==(const CompositeBindingKey &other) const noexcept
		{
			return layout == other.layout && sceneTex == other.sceneTex
				&& uiTex == other.uiTex && edgeTex == other.edgeTex && bloomTex == other.bloomTex
				&& ssaoTex == other.ssaoTex && depthTex == other.depthTex && debugTex == other.debugTex
				&& postProcessBuffer == other.postProcessBuffer && sampler == other.sampler;
		}
	};

	struct CompositeBindingKeyHash
	{
		size_t operator()(const CompositeBindingKey &k) const noexcept
		{
			size_t h = std::hash<const void *>{}(k.layout);
			h ^= (std::hash<const void *>{}(k.sceneTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			h ^= (std::hash<const void *>{}(k.uiTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			h ^= (std::hash<const void *>{}(k.edgeTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			h ^= (std::hash<const void *>{}(k.bloomTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			h ^= (std::hash<const void *>{}(k.ssaoTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			h ^= (std::hash<const void *>{}(k.depthTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			h ^= (std::hash<const void *>{}(k.debugTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			return h;
		}
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
        void CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, Ref<CameraRenderTarget> target, const PostProcessing &postProcessing, Ref<Texture> edgeTexture = nullptr, Ref<Texture> bloomTexture = nullptr, Ref<Texture> ssaoTexture = nullptr);

        void DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const DebugGridStyle &style, bool is2D);
        void DrawDebug2D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void DrawDebug3D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);

    private:
        Ref<GraphicsPipeline> GetDebugGridPipelineForFB(nvrhi::IFramebuffer *framebuffer);
        Ref<GraphicsPipeline> GetGeomPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetTransparentGeomPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetEnvPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetCompositePipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);

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

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_GeometryPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_TransparentGeometryPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_EnvironmentPSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_CompositePSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_DebugGridPSOCache;

        std::unordered_map<CompositeBindingKey, nvrhi::BindingSetHandle, CompositeBindingKeyHash> m_CompositeBindingSetCache;
        std::unordered_map<DebugGridBindingKey, nvrhi::BindingSetHandle, DebugGridBindingKeyHash> m_DebugGridBindingSetCache;

        std::unordered_map<nvrhi::IBindingLayout *, nvrhi::BindingSetHandle> m_CSMBindingSetCache;

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
