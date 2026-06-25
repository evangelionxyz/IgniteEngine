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
		nvrhi::IBuffer *postProcessBuffer = nullptr;
		nvrhi::ISampler *sampler = nullptr;

		bool operator==(const CompositeBindingKey &other) const noexcept
		{
			return layout == other.layout && sceneTex == other.sceneTex
				&& uiTex == other.uiTex && edgeTex == other.edgeTex && bloomTex == other.bloomTex
				&& ssaoTex == other.ssaoTex && postProcessBuffer == other.postProcessBuffer
				&& sampler == other.sampler;
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
			return h;
		}
	};

    class IGN_API SceneRenderer : public ISceneRenderer
    {
    public:
        SceneRenderer();
        ~SceneRenderer();

        virtual void OnUpdate(float deltaTime) override;

        void BeginFrame();
        void SetActiveScene(const Ref<Scene> &scene);
        
        void RenderEditorTo(ICamera *camera);
        void RenderGameplayTo(ICamera *camera);
        void SetEditorWidgetMousePosition(uint32_t mouseX, uint32_t mouseY, bool hovered);
        void SetGameplayWidgetMousePosition(uint32_t mouseX, uint32_t mouseY, bool hovered);

        virtual void ResizeFramebuffer(uint32_t width, uint32_t height) override;
        void ResizeGameplayFramebuffer(uint32_t width, uint32_t height);
        void SetFillMode(nvrhi::RasterFillMode mode);

        void SetSelectedEntity(const Entity& entity);
        void UnselectEntity(const Entity& entity);
        void ClearSelectedEntities();

		virtual Ref<Texture> GetEnvironmentMapColorTexture() const override;
        virtual Ref<Texture> GetCascadedShadowMapDepthTexture() const override;

        virtual Ref<CascadedShadowMap> GetCascadedShadowMap() override;
        Ref<Renderer2D> &GetRenderer2D() { return m_Renderer2D; }

        DebugGridSettings &GetDebugGridSettings() { return m_DebugGridSettings; }
        const DebugGridSettings &GetDebugGridSettings() const { return m_DebugGridSettings; }
        void SetDebugGridSettings(const DebugGridSettings &settings) { m_DebugGridSettings = settings; }

        const Ref<RenderTarget> &GetCompositeRT() { return m_CompositeRT; }
        const Ref<RenderTarget> &GetSceneRT() { return m_SceneRT; }
        const Ref<RenderTarget> &GetWidgetRT() { return m_WidgetRT; }

        const Ref<RenderTarget> &GetGameplayCompositeRT() { return m_GameplayCompositeRT; }
        const Ref<RenderTarget> &GetGameplaySceneRT() { return m_GameplaySceneRT; }
        const Ref<RenderTarget> &GetGameplayWidgetRT() { return m_GameplayWidgetRT; }
    private:
        Ref<Mesh> ResolveMesh(Project *project, AssetHandle handle);
        Ref<Material> ResolveMaterial(Project *project, AssetHandle handle);

        void UploadSkeletonBuffers(nvrhi::ICommandList *cmd);
        void ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera);
        void ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer);
        void UIPass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void CompositePass(nvrhi::ICommandList *cmd, ICamera *camera, const PostProcessing &postProcessing,  nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture, Ref<Texture> edgeTexture = nullptr, Ref<Texture> bloomTexture = nullptr, Ref<Texture> ssaoTexture = nullptr);

        void DrawIcons(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, ICamera *camera);
        void DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const DebugGridStyle &style, bool is2D);
        void DrawDebug2D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);
        void DrawDebug3D(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer);

    private:
        void Clear3DAssetResolveCache();
        
        Ref<GraphicsPipeline> GetDebugGridPipelineForFB(nvrhi::IFramebuffer *framebuffer);
        Ref<GraphicsPipeline> GetGeomPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetEnvPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);
        Ref<GraphicsPipeline> GetCompositePipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode);

        nvrhi::BindingSetHandle GetOrCreateDebugGridBindingSet(nvrhi::IBindingLayout *bindingLayout, const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &gridBuffer);
        nvrhi::BindingSetHandle GetOrCreateCompositeBindingSet(nvrhi::IBindingLayout *bindingLayout, Ref<Texture> sceneTexture, Ref<Texture> uiTexture, Ref<Texture> edgeTexture, Ref<Texture> bloomTexture, Ref<Texture> ssaoTexture, Ref<ConstantBuffer> postProcessBuffer, nvrhi::ISampler *sampler);
        nvrhi::BindingSetHandle GetOrCreateCSMBindingSet(nvrhi::IBindingLayout *bindingLayout, Ref<ConstantBuffer> skinnedMeshGPUDataBuffer, Ref<ConstantBuffer> csmGPUDataBuffer);

    private:
        Ref<WidgetRenderer> m_WidgetRenderer;

        Ref<RenderTarget> m_SceneRT;
        Ref<RenderTarget> m_WidgetRT;
        Ref<RenderTarget> m_CompositeRT;

        Ref<RenderTarget> m_GameplaySceneRT;
        Ref<RenderTarget> m_GameplayWidgetRT;
        Ref<RenderTarget> m_GameplayCompositeRT;

        Ref<Bloom> m_EditorBloom;
        Ref<SSAO> m_EditorSSAO;

        Ref<Bloom> m_GameplayBloom;
        Ref<SSAO> m_GameplaySSAO;

        WorldEnvironment *m_WorldEnvironment = nullptr;

        std::unordered_map<std::string, Ref<Texture>> m_Icons;
        std::unordered_map<AssetResolveKey, Ref<Mesh>, AssetResolveKeyHash> m_MeshResolveCache;
        std::unordered_map<AssetResolveKey, Ref<Material>, AssetResolveKeyHash> m_MaterialResolveCache;

		std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_GeometryPSOCache;
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
