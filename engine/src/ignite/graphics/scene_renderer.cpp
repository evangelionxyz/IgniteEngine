// Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO

#include "scene_renderer.hpp"
#include "framebuffer_key.hpp"
#include "gpu_upload_sync.hpp"

#include "renderer.hpp"
#include "renderer_2d.hpp"
#include "ui_renderer.hpp"
#include "ui/ui_manager.hpp"
#include "font.hpp"

#include "ignite/scene/scene.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/physics/2d/physics_2d_component.hpp"

#include "ignite/core/application.hpp"

#include "objects/shadow_map.hpp"

#include <ranges>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

#include "ignite/project/project.hpp"
#include "ignite/core/profiler/profiler.hpp"

namespace ignite
{
    static WorldEnvironment *GetActiveWorldEnvironment(Scene *scene)
	{
		if (!scene || !scene->registry)
		{
			return nullptr;
		}

		auto view = scene->registry->view<WorldEnvironment>();
		WorldEnvironment *fallback = nullptr;
		for (entt::entity e : view)
		{
			WorldEnvironment &world = view.get<WorldEnvironment>(e);
			if (!world.enabled)
			{
				continue;
			}

			if (world.primary)
			{
				return &world;
			}

			if (!fallback)
			{
				fallback = &world;
			}
		}

		return fallback;
	}

	static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_GeometryPSOCache;
	static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_EnvironmentPSOCache;
	static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_CompositePSOCache;
	static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_DebugGridPSOCache;

	static void EmitSceneRendererCacheStatsToProfiler(size_t selectedEntityCount)
	{
		IGN_PROFILE_FUNCTION();

		IGN_PROFILE_PLOT("SceneRenderer.Cache.PSO.Geometry", static_cast<double>(s_GeometryPSOCache.size()));
		IGN_PROFILE_PLOT("SceneRenderer.Cache.PSO.Environment", static_cast<double>(s_EnvironmentPSOCache.size()));
		IGN_PROFILE_PLOT("SceneRenderer.Cache.PSO.Composite", static_cast<double>(s_CompositePSOCache.size()));
		IGN_PROFILE_PLOT("SceneRenderer.Cache.PSO.DebugGrid", static_cast<double>(s_DebugGridPSOCache.size()));
		IGN_PROFILE_PLOT("SceneRenderer.State.SelectedEntities", static_cast<double>(selectedEntityCount));
	}

	struct DebugGrid_GPUData
	{
		glm::vec4 thinColor = glm::vec4(0.0f);
		glm::vec4 thickColor = glm::vec4(0.0f);
		glm::vec4 xAxisColor = glm::vec4(0.0f);
		glm::vec4 yAxisColor = glm::vec4(0.0f);
		glm::vec4 zAxisColor = glm::vec4(0.0f);
		glm::vec4 settings0 = glm::vec4(0.0f); // x=cellSize y=minPixelsBetweenCells z=gridSize w=majorLineScale
		glm::vec4 settings1 = glm::vec4(0.0f); // x=planeMode(0:XZ, 1:XY) y=enableX z=enableY w=enableZ
	};

	// Helper to build a debug-grid pipeline per framebuffer (once)
	static Ref<GraphicsPipeline> GetDebugGridPipelineForFB(nvrhi::IFramebuffer *framebuffer)
	{
		auto key = MakeFramebufferKey(framebuffer, nvrhi::RasterFillMode::Solid);
		auto it = s_DebugGridPSOCache.find(key);
		if (it != s_DebugGridPSOCache.end())
		{
			return it->second;
		}

		s_DebugGridPSOCache.clear();

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
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0));
		bindingLayoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(1));
		nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(bindingLayoutDesc);

		Ref<Shader> vertexShader = Shader::Create("resources/shaders/infinite_grid.vertex.hlsl", ShaderType::Vertex, true);
		Ref<Shader> pixelShader = Shader::Create("resources/shaders/infinite_grid.pixel.hlsl", ShaderType::Pixel, true);

		auto gp = GraphicsPipeline::Create();
		gp->SetShaders({ vertexShader, pixelShader })
			.AddBindingLayout(bindingLayout)
			.Build(framebuffer, params);

		s_DebugGridPSOCache.emplace(key, gp);
		return gp;
	}

	struct DebugGridBindingKey
	{
		nvrhi::IBindingLayout *layout = nullptr;
		nvrhi::IBuffer *gridBuffer = nullptr;

		bool operator==(const DebugGridBindingKey &other) const noexcept
		{
			return layout == other.layout && gridBuffer == other.gridBuffer;
		}
	};

	std::unordered_set<Material *> s_UploadedMaterialsThisPass;

	struct DebugGridBindingKeyHash
	{
		size_t operator()(const DebugGridBindingKey &k) const noexcept
		{
			size_t h = std::hash<const void *>{}(k.layout);
			h ^= (std::hash<const void *>{}(k.gridBuffer) + 0x9e3779b9 + (h << 6) + (h >> 2));
			return h;
		}
	};

	static std::unordered_map<DebugGridBindingKey, nvrhi::BindingSetHandle, DebugGridBindingKeyHash> s_DebugGridBindingSetCache;

	static nvrhi::BindingSetHandle GetOrCreateDebugGridBindingSet(nvrhi::IBindingLayout *bindingLayout, Ref<ConstantBuffer> gridBuffer)
	{
		DebugGridBindingKey key{ bindingLayout, gridBuffer ? gridBuffer->GetHandle() : nullptr };
		auto it = s_DebugGridBindingSetCache.find(key);
		if (it != s_DebugGridBindingSetCache.end())
		{
			return it->second;
		}

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
		auto bindingSetDesc = nvrhi::BindingSetDesc();
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraConstantBuffer()->GetHandle()));
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, gridBuffer->GetHandle()));

		nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
		LOG_ASSERT(bindingSet, "[Debug Grid] Failed to create binding set");
		if (bindingSet)
		{
			s_DebugGridBindingSetCache.emplace(key, bindingSet);
		}

		return bindingSet;
	}

	// Helper to build a geometry pipeline for a framebuffer (once) and cache it.
	static Ref<GraphicsPipeline> GetGeomPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
		auto key = MakeFramebufferKey(framebuffer, fillMode);
		auto it = s_GeometryPSOCache.find(key);
		if (it != s_GeometryPSOCache.end())
		{
			return it->second;
		}

		s_GeometryPSOCache.clear();

		const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
		bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

		GraphicsPipelineParams params;
		params.enableBlend = true;
		params.enableDepthWrite = hasDepthAttachment;
		params.enableDepthTest = hasDepthAttachment;
		params.enableDepthStencil = false;
		params.fillMode = fillMode;
		params.cullMode = nvrhi::RasterCullMode::Front;
		params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

		Ref<Shader> vertexShader = Shader::Create("resources/shaders/mesh_anim.vertex.hlsl", ShaderType::Vertex, true);
		Ref<Shader> pixelShader = Shader::Create("resources/shaders/mesh_anim.pixel.hlsl", ShaderType::Pixel, true);

		auto gp = GraphicsPipeline::Create();
		gp->SetShaders({ vertexShader, pixelShader })
			.AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM))
			.AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MATERIAL))
			.Build(framebuffer, params);

		s_GeometryPSOCache.emplace(key, gp);
		return gp;
	}

	// Helper to build an environment pipeline per framebuffer (once)
	static Ref<GraphicsPipeline> GetEnvPipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
		auto key = MakeFramebufferKey(framebuffer, fillMode);
		auto it = s_EnvironmentPSOCache.find(key);
		if (it != s_EnvironmentPSOCache.end())
		{
			return it->second;
		}

		s_EnvironmentPSOCache.clear();

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

		Ref<Shader> vertexShader = Shader::Create("resources/shaders/skybox.vertex.hlsl", ShaderType::Vertex, true);
		Ref<Shader> pixelShader = Shader::Create("resources/shaders/skybox.pixel.hlsl", ShaderType::Pixel, true);

		auto gp = GraphicsPipeline::Create();
		gp->SetShaders({ vertexShader, pixelShader })
			.AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::ENVIRONMENT))
			.Build(framebuffer, params);

		s_EnvironmentPSOCache.emplace(key, gp);
		return gp;
	}

	// Helper to build a composite pipeline per framebuffer (once)
	static Ref<GraphicsPipeline> GetCompositePipelineForFB(nvrhi::IFramebuffer *framebuffer, nvrhi::RasterFillMode fillMode)
	{
		auto key = MakeFramebufferKey(framebuffer, fillMode);
		auto it = s_CompositePSOCache.find(key);

		if (it != s_CompositePSOCache.end())
		{
			return it->second;
		}

		s_CompositePSOCache.clear();

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		// Binding layout
		nvrhi::BindingLayoutDesc layoutDesc = {};
		layoutDesc.visibility = nvrhi::ShaderType::All;
		layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // scene
		layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // ui
        layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2)); // edge
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
		Ref<Shader> vertexShader = Shader::Create("resources/shaders/composite.vertex.hlsl", ShaderType::Vertex, true);
		Ref<Shader> pixelShader = Shader::Create("resources/shaders/composite.pixel.hlsl", ShaderType::Pixel, true);

		auto gp = GraphicsPipeline::Create();
		gp->SetShaders({ vertexShader, pixelShader })
			.AddBindingLayout(bindingLayout)
			.Build(framebuffer, params);

		LOG_INFO("[Composite] Created new pipeline with forced shader recompilation");

		s_CompositePSOCache.emplace(key, gp);

		return gp;
	}

	struct CompositeBindingKey
	{
		nvrhi::IBindingLayout *layout = nullptr;
		nvrhi::ITexture *sceneTex = nullptr;
		nvrhi::ITexture *uiTex = nullptr;
        nvrhi::ITexture *edgeTex = nullptr;
		bool operator==(const CompositeBindingKey &other) const noexcept
		{
            return layout == other.layout && sceneTex == other.sceneTex && uiTex == other.uiTex && edgeTex == other.edgeTex;
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
			return h;
		}
	};

	static std::unordered_map<CompositeBindingKey, nvrhi::BindingSetHandle, CompositeBindingKeyHash> s_CompositeBindingSetCache;

	static nvrhi::BindingSetHandle GetOrCreateCompositeBindingSet(nvrhi::IBindingLayout *bindingLayout,
        Ref<Texture> sceneTexture, Ref<Texture> uiTexture, Ref<Texture> edgeTexture, nvrhi::SamplerHandle sampler)
	{
        Ref<Texture> edge = edgeTexture ? edgeTexture : Renderer::GetBlackTexture();
		CompositeBindingKey key{ bindingLayout, sceneTexture->GetHandle(), uiTexture->GetHandle(), edge->GetHandle() };
		auto it = s_CompositeBindingSetCache.find(key);
		if (it != s_CompositeBindingSetCache.end())
		{
			return it->second;
		}

		s_CompositeBindingSetCache.clear();

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
		// Composite Binding set
		auto bindingSetDesc = nvrhi::BindingSetDesc();
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, sceneTexture->GetHandle()));
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, uiTexture->GetHandle()));
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, edge->GetHandle()));
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, sampler));

		nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
		LOG_ASSERT(bindingSet, "[Composite] Failed to create Composite Binding Set");
		if (bindingSet)
		{
			s_CompositeBindingSetCache.emplace(key, bindingSet);
		}

		return bindingSet;
	}

	struct CSMBindingKey
	{
		nvrhi::IBindingLayout *layout = nullptr;
		bool operator==(const CSMBindingKey &other) const noexcept { return layout == other.layout; }
	};

	struct CSMBindingKeyHash
	{
		size_t operator()(const CSMBindingKey &k) const noexcept
		{
			size_t h = std::hash<const void *>{}(k.layout);
			return h;
		}
	};

	static std::unordered_map<CSMBindingKey, nvrhi::BindingSetHandle, CSMBindingKeyHash> s_CSMBindingSetCache;

	static nvrhi::BindingSetHandle GetOrCreateCSMBindingSet(nvrhi::IBindingLayout *bindingLayout, Ref<ConstantBuffer> skinnedMeshGPUDataBuffer, Ref<ConstantBuffer> csmGPUDataBuffer)
	{
		CSMBindingKey key{ bindingLayout };
		auto it = s_CSMBindingSetCache.find(key);
		if (it != s_CSMBindingSetCache.end())
		{
			for (auto itErase = s_CSMBindingSetCache.begin(); itErase != s_CSMBindingSetCache.end();)
			{
				if (itErase != it)
				{
					itErase = s_CSMBindingSetCache.erase(itErase);
				}
				else
				{
					++itErase;
				}
			}

			return it->second;
		}

		nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

		// Composite Binding set
		auto bindingSetDesc = nvrhi::BindingSetDesc();
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, skinnedMeshGPUDataBuffer->GetHandle()));
		bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, csmGPUDataBuffer->GetHandle()));

		nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
		LOG_ASSERT(bindingSet, "[Composite] Failed to create Composite Binding Set");
		if (bindingSet)
		{
			s_CSMBindingSetCache.emplace(key, bindingSet);
		}

		return bindingSet;
	}

	SceneRenderer::SceneRenderer()
	{
		auto device = DeviceManager::GetInstance()->GetDevice();
		auto samplerDesc = nvrhi::SamplerDesc();
		samplerDesc.setAllFilters(false);
		samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
		m_CompositeSampler = device->createSampler(samplerDesc);

		m_Device = DeviceManager::GetInstance()->GetDevice();
		nvrhi::CommandListHandle cmd = m_Device->createCommandList();

		std::array vertices
		{
			VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
			VertexScreen{ { -1.0f,  1.0f }, { 0.0f, 0.0f } },
			VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },

			VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },
			VertexScreen{ {  1.0f, -1.0f }, { 1.0f, 1.0f } },
			VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
		};

		m_CompositeVertexBuffer = VertexBuffer::Create(sizeof(vertices));

		cmd->open();
		m_CompositeVertexBuffer->SetData(cmd, Buffer(vertices.data(), sizeof(vertices)));
		cmd->close();
		Application::SubmitWorkerCommandList(cmd);

		m_Renderer2D = Renderer2D::Create();
		m_UIRenderer = UIRenderer::Create(1280, 720);
		m_UIRenderer->SetUIManager(&UIManager::GetInstance());
		m_EdgeDetection = EdgeDetection::Create();
		m_EdgeDetection->CreatePipeline();
		m_DebugGridBuffer = ConstantBuffer::Create(sizeof(DebugGrid_GPUData), true, 16, "Debug Grid Buffer");

		m_CascadedShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::HIGH);
	}

	SceneRenderer::~SceneRenderer()
	{
		s_GeometryPSOCache.clear();
		s_EnvironmentPSOCache.clear();
		s_CompositePSOCache.clear();
		s_DebugGridPSOCache.clear();
		s_CompositeBindingSetCache.clear();
		s_DebugGridBindingSetCache.clear();
		s_CSMBindingSetCache.clear();
		Clear3DAssetResolveCache();
	
        if (m_Renderer2D)
        {
            m_Renderer2D->ClearAssetResolveCache();
			m_Renderer2D->InvalidatePreRenderCache();
        }
		m_Has2DPreRenderCache = false;
	}

	Ref<StaticMesh> SceneRenderer::ResolveStaticMesh(Project *project, AssetHandle handle)
	{
		if (!project || handle == AssetHandle(0))
		{
			return nullptr;
		}

		AssetResolveKey key{ project, handle };
		auto it = m_StaticMeshResolveCache.find(key);
		if (it != m_StaticMeshResolveCache.end())
		{
			return it->second;
		}

		Ref<StaticMesh> mesh = project->GetAsset<StaticMesh>(handle, AssetType::StaticMesh);
		if (mesh)
		{
			m_StaticMeshResolveCache.emplace(key, mesh);
		}

		return mesh;
	}

	Ref<SkeletalMesh> SceneRenderer::ResolveSkeletalMesh(Project *project, AssetHandle handle)
	{
		if (!project || handle == AssetHandle(0))
		{
			return nullptr;
		}

		AssetResolveKey key{ project, handle };
		auto it = m_SkeletalMeshResolveCache.find(key);
		if (it != m_SkeletalMeshResolveCache.end())
		{
			return it->second;
		}

		Ref<SkeletalMesh> mesh = project->GetAsset<SkeletalMesh>(handle, AssetType::SkeletalMesh);
		if (mesh)
		{
			m_SkeletalMeshResolveCache.emplace(key, mesh);
		}

		return mesh;
	}

	Ref<Material> SceneRenderer::ResolveMaterial(Project *project, AssetHandle handle)
	{
		if (!project || handle == AssetHandle(0))
		{
			return nullptr;
		}

		AssetResolveKey key{ project, handle };
		auto it = m_MaterialResolveCache.find(key);
		if (it != m_MaterialResolveCache.end())
		{
			return it->second;
		}

		Ref<Material> material = project->GetAsset<Material>(handle);
		if (material)
		{
			m_MaterialResolveCache.emplace(key, material);
		}

		return material;
	}

	void SceneRenderer::Clear3DAssetResolveCache()
	{
		m_StaticMeshResolveCache.clear();
		m_SkeletalMeshResolveCache.clear();
		m_MaterialResolveCache.clear();
        s_UploadedMaterialsThisPass.clear();
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

		m_SelectedEntities.clear();
		m_Has2DPreRenderCache = false;
		Clear3DAssetResolveCache();

		m_Scene = scene;

		s_GeometryPSOCache.clear();
		s_EnvironmentPSOCache.clear();
		s_CompositePSOCache.clear();
		s_DebugGridPSOCache.clear();
		s_CompositeBindingSetCache.clear();
		s_DebugGridBindingSetCache.clear();
		s_CSMBindingSetCache.clear();

		if (m_Renderer2D)
		{
			m_Renderer2D->ClearAssetResolveCache();
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

	void SceneRenderer::RenderEditorTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment)
	{
		IGN_PROFILE_FUNCTION();
		IGN_PROFILE_FRAME_NAMED("Editor Frame");

		WorldEnvironment *worldEnvironment = renderEnvironment ? GetActiveWorldEnvironment(m_Scene.get()) : nullptr;

		if (worldEnvironment)
		{
			if (!worldEnvironment->environment)
			{
				worldEnvironment->environment = Environment::Create(m_Scene.get());
				worldEnvironment->dirtyEnvironment = true;
				worldEnvironment->gpuInitialized = false;
				worldEnvironment->loadedHDRHandle = AssetHandle(0);
			}

			if (worldEnvironment->hdrHandle == AssetHandle(0))
			{
				if (worldEnvironment->loadedHDRHandle != AssetHandle(0))
				{
					worldEnvironment->environment->SetTexture(Renderer::GetBlackTexture());
					worldEnvironment->loadedHDRHandle = AssetHandle(0);
					worldEnvironment->dirtyEnvironment = true;
				}
			}
			else if (worldEnvironment->loadedHDRHandle != worldEnvironment->hdrHandle)
			{
				Ref<Asset> hdrAsset = m_Scene->GetProject()->GetAssetManager()->GetAsset(worldEnvironment->hdrHandle, AssetType::Texture);
				if (hdrAsset && hdrAsset->IsReady())
				{
					Ref<Texture> hdrTexture = std::static_pointer_cast<Texture>(hdrAsset);
					worldEnvironment->environment->SetTexture(hdrTexture);
					worldEnvironment->loadedHDRHandle = worldEnvironment->hdrHandle;
					worldEnvironment->dirtyEnvironment = true;
				}
			}

			if (worldEnvironment->dirtyEnvironment)
			{
                const auto &assets = m_Scene->GetProject()->GetAssetManager()->GetLoadedAssets();
				for (const auto &[handle, asset] : assets)
				{
					if (asset->GetAssetType() == AssetType::Material)
					{
						Ref<Material> material = std::static_pointer_cast<Material>(asset);
						if (material)
						{
							material->InvalidateBindingSet();
						}
					}
				}
			}
		}

		// Check if any materials need binding set creation/recreation
		{
			IGN_PROFILE_SCOPE("SceneRenderer::UpdateMaterialBindingSets");
			bool waitedForMaterialUpdate = false;
			const auto &assets = m_Scene->GetProject()->GetAssetManager()->GetLoadedAssets();
			for (const auto &[handle, asset] : assets)
			{
				if (asset->GetAssetType() == AssetType::Material)
				{
					Ref<Material> material = std::static_pointer_cast<Material>(asset);
					if (material && (material->IsBindingSetDirty() || !material->GetBindingSet()))
					{
						auto assetManager = m_Scene->GetProject()->GetAssetManager();
						auto isTextureReady = [assetManager](AssetHandle textureHandle)
						{
							if (textureHandle == 0)
							{
								return true;
							}

							Ref<Asset> texAsset = assetManager->GetAsset(textureHandle);
							return texAsset && texAsset->IsReady();
						};

						// Only update if all textures are available and ready
						bool allTexturesReady = true;
						if (!isTextureReady(material->baseColorTextureHandle))
							allTexturesReady = false;
						if (!isTextureReady(material->emissiveTextureHandle))
							allTexturesReady = false;
						if (!isTextureReady(material->metallicRoughnessTextureHandle))
							allTexturesReady = false;
						if (!isTextureReady(material->normalTextureHandle))
							allTexturesReady = false;
						if (!isTextureReady(material->occlusionTextureHandle))
							allTexturesReady = false;

						if (allTexturesReady)
						{
							MaterialTextures textures;
							material->RetrieveTextures(assetManager, &textures);

							if (!waitedForMaterialUpdate)
							{
								// Ensure no other GPU operations are in flight before updating material
								// This prevents threading errors when materials are being invalidated
								GPUUploadSync::DeviceWaitIdle(m_Device);
								waitedForMaterialUpdate = true;
							}

							material->UpdateBindingSet(this, &textures, assetManager);
						}
					}
				}
			}
		}

        // Create fresh command list for this frame
		nvrhi::CommandListHandle cmd = m_Device->createCommandList();
		{
			IGN_PROFILE_SCOPE("SceneRenderer::RecordEditorCommandList");
			cmd->open();
			m_Scene->WriteBuffer(cmd);

			if (worldEnvironment && worldEnvironment->environment)
			{
				worldEnvironment->environment->SetScene(m_Scene.get());

				if (!worldEnvironment->gpuInitialized)
				{
					worldEnvironment->environment->WriteBuffer(cmd);
					worldEnvironment->gpuInitialized = true;
					worldEnvironment->dirtyEnvironment = true;
				}

				if (worldEnvironment->dirtyEnvironment)
				{
					if (worldEnvironment->environment->UpdateBindingSet())
					{
						worldEnvironment->dirtyEnvironment = false;
					}
				}
			}

			// setup camera constants
			CameraBuffer cameraBuffer = { camera->GetProjection(), camera->GetView(), glm::vec4(camera->position, 1.0f) };
			Renderer::GetCameraConstantBuffer()->SetData(cmd, Buffer(&cameraBuffer, sizeof(CameraBuffer)));

			// Clear Render Targets
			// far depth = 1.0f == LessOrEqual
			{
				uiRT->ClearColorAttachmentFloat(cmd, 0);
				uiRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
				uiRT->ClearDepthAttachment(cmd, 1.0f, 0);

				sceneRT->ClearColorAttachmentFloat(cmd, 0);
				sceneRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
				sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);

				compositeRT->ClearColorAttachmentFloat(cmd, 0);
				compositeRT->ClearDepthAttachment(cmd, 1.0f, 0);
			}


			nvrhi::IFramebuffer *framebuffer = sceneRT->GetFramebuffer();
			if (renderEnvironment && worldEnvironment && worldEnvironment->environment && !worldEnvironment->dirtyEnvironment)
			{
				const Ref<GraphicsPipeline> envPSO = GetEnvPipelineForFB(framebuffer, m_FillMode);
				worldEnvironment->environment->Draw(cmd, camera, framebuffer, envPSO);
			}

			{
				IGN_PROFILE_SCOPE("SceneRenderer::ColorPass");
				ColorPass(cmd, camera, framebuffer);
			}
			if (camera->projectionType == ProjectionType::Orthographic)
			{
				IGN_PROFILE_SCOPE("SceneRenderer::DebugGrid2D");
				DrawDebugGrid(cmd, framebuffer, m_DebugGridSettings.world2D, true);
			}
			else
			{
				IGN_PROFILE_SCOPE("SceneRenderer::DebugGrid3D");
				DrawDebugGrid(cmd, framebuffer, m_DebugGridSettings.world3D, false);
			}

			{
				IGN_PROFILE_SCOPE("SceneRenderer::Physics2DDebug");
				DrawDebug2DPhysics(cmd, framebuffer);
			}

            {
                IGN_PROFILE_SCOPE("SceneRenderer::Physics3DDebug");
                DrawDebug3DPhysics(cmd, framebuffer);
            }

			Ref<Texture> edgeTexture = nullptr;
			if (m_EdgeDetection && !m_SelectedEntities.empty())
			{
				const uint32_t width = sceneRT->GetWidth();
				const uint32_t height = sceneRT->GetHeight();

				if (!m_EdgeDetection->GetOutputTexture() || m_EdgeDetection->GetOutputTexture()->GetWidth() != static_cast<int>(width) || m_EdgeDetection->GetOutputTexture()->GetHeight() != static_cast<int>(height))
				{
					m_EdgeDetection->CreateOutputTexture(width, height);
				}

				m_EdgeDetection->UpdateBindingSet(sceneRT->GetColorAttachment(0), sceneRT->GetColorAttachment(1), sceneRT->GetDepthAttachment());

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

			{
				IGN_PROFILE_SCOPE("SceneRenderer::CompositePass");
				CompositePass(cmd, compositeRT->GetFramebuffer(), sceneRT->GetColorAttachment(0), uiRT->GetColorAttachment(0), edgeTexture);
			}

			cmd->close();
		}

		{
			std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
			m_Device->executeCommandList(cmd);
		}
	}

	void SceneRenderer::RenderGameplayTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment /*= true*/)
	{
		IGN_PROFILE_FUNCTION();
		IGN_PROFILE_FRAME_NAMED("Gameplay Frame");
		WorldEnvironment *worldEnvironment = renderEnvironment ? GetActiveWorldEnvironment(m_Scene.get()) : nullptr;

		if (worldEnvironment)
		{
			if (!worldEnvironment->environment)
			{
				worldEnvironment->environment = Environment::Create(m_Scene.get());
				worldEnvironment->dirtyEnvironment = true;
				worldEnvironment->gpuInitialized = false;
				worldEnvironment->loadedHDRHandle = AssetHandle(0);
			}

			if (worldEnvironment->hdrHandle == AssetHandle(0))
			{
				if (worldEnvironment->loadedHDRHandle != AssetHandle(0))
				{
					worldEnvironment->environment->SetTexture(Renderer::GetBlackTexture());
					worldEnvironment->loadedHDRHandle = AssetHandle(0);
					worldEnvironment->dirtyEnvironment = true;
				}
			}
			else if (worldEnvironment->loadedHDRHandle != worldEnvironment->hdrHandle)
			{
				Ref<Asset> hdrAsset = m_Scene->GetProject()->GetAssetManager()->GetAsset(worldEnvironment->hdrHandle, AssetType::Texture);
				if (hdrAsset && hdrAsset->IsReady())
				{
					Ref<Texture> hdrTexture = std::static_pointer_cast<Texture>(hdrAsset);
					worldEnvironment->environment->SetTexture(hdrTexture);
					worldEnvironment->loadedHDRHandle = worldEnvironment->hdrHandle;
					worldEnvironment->dirtyEnvironment = true;
				}
			}

			if (worldEnvironment->dirtyEnvironment)
			{
				const auto &assets = m_Scene->GetProject()->GetAssetManager()->GetLoadedAssets();
				for (const auto &[handle, asset] : assets)
				{
					if (asset->GetAssetType() == AssetType::Material)
					{
						Ref<Material> material = std::static_pointer_cast<Material>(asset);
						if (material)
						{
							material->InvalidateBindingSet();
						}
					}
				}
			}
		}

		// Check if any materials need binding set creation/recreation
		{
			IGN_PROFILE_SCOPE("SceneRenderer::UpdateMaterialBindingSets");
            bool waitedForMaterialUpdate = false;
			const auto &assets = m_Scene->GetProject()->GetAssetManager()->GetLoadedAssets();
			for (const auto &[handle, asset] : assets)
			{
				if (asset->GetAssetType() == AssetType::Material)
				{
					Ref<Material> material = std::static_pointer_cast<Material>(asset);
					if (material && (material->IsBindingSetDirty() || !material->GetBindingSet()))
					{
						auto assetManager = m_Scene->GetProject()->GetAssetManager();
						auto isTextureReady = [&assetManager](AssetHandle textureHandle)
						{
							if (textureHandle == 0)
							{
								return true;
							}

							Ref<Asset> texAsset = assetManager->GetAsset(textureHandle);
							return texAsset && texAsset->IsReady();
						};

						// Only update if all textures are available and ready
						bool allTexturesReady = true;
						if (!isTextureReady(material->baseColorTextureHandle))
							allTexturesReady = false;
						if (!isTextureReady(material->emissiveTextureHandle))
							allTexturesReady = false;
						if (!isTextureReady(material->metallicRoughnessTextureHandle))
							allTexturesReady = false;
						if (!isTextureReady(material->normalTextureHandle))
							allTexturesReady = false;
						if (!isTextureReady(material->occlusionTextureHandle))
							allTexturesReady = false;

						if (allTexturesReady)
						{
							MaterialTextures textures;
							material->RetrieveTextures(assetManager, &textures);

							if (!waitedForMaterialUpdate)
							{
								// Ensure no other GPU operations are in flight before updating material
								// This prevents threading errors when materials are being invalidated
								GPUUploadSync::DeviceWaitIdle(m_Device);
								waitedForMaterialUpdate = true;
							}

							material->UpdateBindingSet(this, &textures, assetManager);
						}
					}
				}
			}
		}

		// Create fresh command list for this frame
		nvrhi::CommandListHandle cmd = m_Device->createCommandList();
		{
			IGN_PROFILE_SCOPE("SceneRenderer::RecordGameplayCommandList");
			cmd->open();
			m_Scene->WriteBuffer(cmd);

			if (worldEnvironment && worldEnvironment->environment)
			{
				worldEnvironment->environment->SetScene(m_Scene.get());

				if (!worldEnvironment->gpuInitialized)
				{
					worldEnvironment->environment->WriteBuffer(cmd);
					worldEnvironment->gpuInitialized = true;
					worldEnvironment->dirtyEnvironment = true;
				}

				if (worldEnvironment->dirtyEnvironment)
				{
					if (worldEnvironment->environment->UpdateBindingSet())
					{
						worldEnvironment->dirtyEnvironment = false;
					}
				}
			}

			// setup camera constants
			CameraBuffer cameraBuffer = { camera->GetProjection(), camera->GetView(), glm::vec4(camera->position, 1.0f) };
			Renderer::GetCameraConstantBuffer()->SetData(cmd, Buffer(&cameraBuffer, sizeof(CameraBuffer)));

			// Clear Render Targets
			// far depth = 1.0f == LessOrEqual
			{
				uiRT->ClearColorAttachmentFloat(cmd, 0);
				uiRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
				uiRT->ClearDepthAttachment(cmd, 1.0f, 0);

				sceneRT->ClearColorAttachmentFloat(cmd, 0);
				sceneRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
				sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);

				compositeRT->ClearColorAttachmentFloat(cmd, 0);
				compositeRT->ClearDepthAttachment(cmd, 1.0f, 0);
			}


			nvrhi::IFramebuffer *framebuffer = sceneRT->GetFramebuffer();

            if (renderEnvironment && worldEnvironment && worldEnvironment->environment && !worldEnvironment->dirtyEnvironment)
			{
				const Ref<GraphicsPipeline> envPSO = GetEnvPipelineForFB(framebuffer, m_FillMode);
				worldEnvironment->environment->Draw(cmd, camera, framebuffer, envPSO);
			}
			{
				IGN_PROFILE_SCOPE("SceneRenderer::ColorPass");
				ColorPass(cmd, camera, framebuffer);
			}
            {
                IGN_PROFILE_SCOPE("SceneRenderer::ShadowPass");
                ShadowPass(cmd, camera);
            }
			{
				IGN_PROFILE_SCOPE("SceneRenderer::CompositePass");
				CompositePass(cmd, compositeRT->GetFramebuffer(), sceneRT->GetColorAttachment(0), uiRT->GetColorAttachment(0));
			}

			cmd->close();
		}

		{
			std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
			m_Device->executeCommandList(cmd);
		}
	}

	void SceneRenderer::ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera)
	{
		IGN_PROFILE_FUNCTION();
		Project *project = m_Scene ? m_Scene->GetProject() : nullptr;

		nvrhi::GraphicsState csmState = nvrhi::GraphicsState();
		Ref<GraphicsPipeline> csmPipeline = m_CascadedShadowMap->GetPipeline();
		csmState.pipeline = csmPipeline->GetHandle();

		// Compute sun / light direction for shadows
		// Using spherical coordinates: x = azimuth (horizontal), y = elevation (vertical)
		// sunDirection points FROM scene TOWARD the sun
		const float azimuth = m_Scene->gpuData.sungAngles.x;
		const float elevation = m_Scene->gpuData.sungAngles.y;

		const glm::vec3 sunDirection = {
			cos(elevation) * sin(azimuth),   // X: left/right
			sin(elevation),                   // Y: up/down
			cos(elevation) * cos(azimuth)    // Z: front/back
		};

		m_CascadedShadowMap->ComputeMatrices(camera, sunDirection);

		// Share cascade data with the main scene pass (cascadeIndex is unused there)
		CascadedShadowMap_GPUData sceneCascadeData = m_CascadedShadowMap->GetGPUData();
		sceneCascadeData.cascadeIndex = -1;
		m_Scene->GetCSMGPUDataBuffer()->SetData(cmd, Buffer(&sceneCascadeData, sizeof(sceneCascadeData)));

		for (int i = 0; i < NUM_CASCADES; ++i)
		{
			CascadedShadowMap_GPUData cascadeGpuData = sceneCascadeData;
			cascadeGpuData.cascadeIndex = i;
			m_CascadedShadowMap->GetGPUDataBuffer()->SetData(cmd, Buffer(&cascadeGpuData, sizeof(cascadeGpuData)));

			// Clear the specific array layer for this cascade
			m_CascadedShadowMap->BeginCascade(cmd, i);

			nvrhi::IFramebuffer *csmFramebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(i);
			nvrhi::Viewport viewport = csmFramebuffer->getFramebufferInfo().getViewport();

			csmState.framebuffer = csmFramebuffer;
			csmState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(viewport);

			auto ensurePerEntityResources = [this](Ref<ConstantBuffer> &buffer, nvrhi::BindingSetHandle &bindingSet)
			{
				if (!buffer)
				{
					buffer = ConstantBuffer::Create(sizeof(SkinnedMesh_GPUData), true, 512, "Per-Entity Transform Buffer");
				}

				if (!bindingSet)
				{
					nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
					auto desc = nvrhi::BindingSetDesc();
					desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraConstantBuffer()->GetHandle()));
					desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, buffer->GetHandle()));
					desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, m_Scene->GetSceneGPUDataBuffer()->GetHandle()));
					desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, m_Scene->GetCSMGPUDataBuffer()->GetHandle()));

					bindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM));
					LOG_ASSERT(bindingSet, "Failed to create mesh binding set");
				}
			};

			// ===========================
			// Static Meshes
			// ===========================
           {
				IGN_PROFILE_SCOPE("SceneRenderer::StaticMeshes");
				auto staticMeshView = m_Scene->registry->view<TransformComponent, StaticMeshComponent>();
				for (entt::entity e : staticMeshView)
				{
					TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
					if (!tr.visible)
						continue;

					StaticMeshComponent &smc = m_Scene->registry->get<StaticMeshComponent>(e);
					if (smc.handle == AssetHandle(0))
						continue;

					ensurePerEntityResources(smc.perEntityBuffer, smc.meshBindingSet);

                    Ref<StaticMesh> sm = ResolveStaticMesh(project, smc.handle);
					if (!sm)
						continue;

					for (auto &m : sm->GetMeshInstances())
					{
						SkinnedMesh_GPUData gpuData;
						gpuData.transformation = tr.GetWorldMatrix() * m->global;
						gpuData.objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

						const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(gpuData.transformation)));
						gpuData.normal = glm::mat4(normalMat3);

						std::fill(std::begin(gpuData.boneTransforms), std::end(gpuData.boneTransforms), glm::mat4(1.0f));
						smc.perEntityBuffer->SetData(cmd, Buffer(&gpuData, sizeof(gpuData)));

						auto &primitive = m->GetPrimitive();
						if (smc.meshBindingSet && primitive->vertexBuffer && primitive->indexBuffer)
						{
							csmState.bindings = { smc.meshBindingSet };
							csmState.vertexBuffers.resize(0);
							csmState.vertexBuffers.push_back({ primitive->vertexBuffer->GetHandle(), 0, 0 });
							csmState.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });

							cmd->setGraphicsState(csmState);

							nvrhi::DrawArguments args;
							args.setVertexCount(primitive->indexBuffer->GetCount());
							args.instanceCount = 1;
							cmd->drawIndexed(args);
						}
					}
				}
			}

			{
				IGN_PROFILE_SCOPE("SceneRenderer::SkeletalMeshesShadow");
				auto skelMeshView = m_Scene->registry->view<TransformComponent, SkeletalMeshComponent>();
				for (entt::entity e : skelMeshView)
				{
					TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
					if (!tr.visible)
						continue;

					SkeletalMeshComponent &smc = m_Scene->registry->get<SkeletalMeshComponent>(e);
					if (smc.handle == AssetHandle(0))
						continue;

					ensurePerEntityResources(smc.perEntityBuffer, smc.meshBindingSet);

					Ref<SkeletalMesh> sm = ResolveSkeletalMesh(project, smc.handle);
					if (!sm)
						continue;

					for (auto &m : sm->GetMeshInstances())
					{
						SkinnedMesh_GPUData gpuData;
						gpuData.transformation = tr.GetWorldMatrix() * m->global;
						gpuData.objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

						const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(gpuData.transformation)));
						gpuData.normal = glm::mat4(normalMat3);

						std::fill(std::begin(gpuData.boneTransforms), std::end(gpuData.boneTransforms), glm::mat4(1.0f));

						const size_t transformCount = std::min(static_cast<size_t>(MAX_BONES), sm->boneTransforms.size());
						for (size_t j = 0; j < transformCount; ++j)
						{
							gpuData.boneTransforms[j] = sm->boneTransforms[j];
						}

                      smc.perEntityBuffer->SetData(cmd, Buffer(&gpuData, sizeof(gpuData)));

						auto &primitive = m->GetPrimitive();
						if (smc.meshBindingSet && primitive->vertexBuffer && primitive->indexBuffer)
						{
							csmState.bindings = { smc.meshBindingSet };
							csmState.vertexBuffers.resize(0);
							csmState.vertexBuffers.push_back({ primitive->vertexBuffer->GetHandle(), 0, 0 });
							csmState.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });

							cmd->setGraphicsState(csmState);

							nvrhi::DrawArguments args;
							args.setVertexCount(primitive->indexBuffer->GetCount());
							args.instanceCount = 1;
							cmd->drawIndexed(args);
						}
					}
				}
			}
		}
	}

	void SceneRenderer::ColorPass(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer)
	{
		IGN_PROFILE_FUNCTION();
		Project *project = m_Scene ? m_Scene->GetProject() : nullptr;
		Ref<GraphicsPipeline> geomPSO = GetGeomPipelineForFB(framebuffer, m_FillMode);

		nvrhi::GraphicsState geomGState = nvrhi::GraphicsState();
		geomGState.pipeline = geomPSO->GetHandle();
		geomGState.framebuffer = framebuffer;
		geomGState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());


		auto ensurePerEntityResources = [this](Ref<ConstantBuffer> &buffer, nvrhi::BindingSetHandle &bindingSet)
		{
			if (!buffer)
			{
				buffer = ConstantBuffer::Create(sizeof(SkinnedMesh_GPUData), true, 512, "Per-Entity Transform Buffer");
			}

			if (!bindingSet)
			{
				nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
				auto desc = nvrhi::BindingSetDesc();
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraConstantBuffer()->GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, buffer->GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, m_Scene->GetSceneGPUDataBuffer()->GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, m_Scene->GetCSMGPUDataBuffer()->GetHandle()));

				bindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM));
				LOG_ASSERT(bindingSet, "Failed to create mesh binding set");
			}
		};

		// ===========================
		// Skeletal Meshes
		// ===========================
		{
			IGN_PROFILE_SCOPE("SceneRenderer::SkeletalMeshes");
			auto skelMeshView = m_Scene->registry->view<TransformComponent, SkeletalMeshComponent>();
			for (entt::entity e : skelMeshView)
			{
				TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
				if (!tr.visible)
					continue;

				SkeletalMeshComponent &smc = m_Scene->registry->get<SkeletalMeshComponent>(e);
				if (smc.handle == AssetHandle(0))
				{
					continue;
				}

				ensurePerEntityResources(smc.perEntityBuffer, smc.meshBindingSet);

				Ref<SkeletalMesh> sm = ResolveSkeletalMesh(project, smc.handle);
				if (!sm)
				{
					continue;
				}

				for (auto &m : sm->GetMeshInstances())
				{
					SkinnedMesh_GPUData gpuData;
					gpuData.transformation = tr.GetWorldMatrix() * m->global;
					gpuData.objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

					const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(gpuData.transformation)));
					gpuData.normal = glm::mat4(normalMat3);

					std::fill(std::begin(gpuData.boneTransforms), std::end(gpuData.boneTransforms), glm::mat4(1.0f));

					const size_t transformCount = std::min(static_cast<size_t>(MAX_BONES), sm->boneTransforms.size());
					for (size_t i = 0; i < transformCount; ++i)
					{
						gpuData.boneTransforms[i] = sm->boneTransforms[i];
					}

                  smc.perEntityBuffer->SetData(cmd, Buffer(&gpuData, sizeof(gpuData)));

					auto &primitive = m->GetPrimitive();

					Ref<Material> material = ResolveMaterial(project, m->GetMaterialHandle());
					if (!material)
					{
						continue;
					}

					if (!material->GetBindingSet())
					{
						MaterialTextures textures;
						auto assetManager = m_Scene->GetProject()->GetAssetManager();
						material->RetrieveTextures(assetManager, &textures);
						material->UpdateBindingSet(this, &textures, assetManager);

						if (!material->GetBindingSet())
						{
							continue;
						}
					}

					if (s_UploadedMaterialsThisPass.insert(material.get()).second)
					{
						material->UploadToGpu(cmd);
					}

					if (smc.meshBindingSet && material->GetBindingSet() && primitive->vertexBuffer && primitive->indexBuffer)
					{
						geomGState.bindings = { smc.meshBindingSet, material->GetBindingSet() };
						geomGState.vertexBuffers.resize(0);
						geomGState.vertexBuffers.push_back({ primitive->vertexBuffer->GetHandle(), 0, 0 });
						geomGState.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });

						cmd->setGraphicsState(geomGState);

						nvrhi::DrawArguments args;
						args.setVertexCount(primitive->indexBuffer->GetCount());
						args.instanceCount = 1;

						cmd->drawIndexed(args);
					}
				}
			}
		}


		// ===========================
		// Static Meshes
		// ===========================
		{
			IGN_PROFILE_SCOPE("SceneRenderer::StaticMeshes");
			auto meshView = m_Scene->registry->view<TransformComponent, StaticMeshComponent>();
			for (entt::entity e : meshView)
			{
				TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
				if (!tr.visible)
					continue;

				StaticMeshComponent &smc = m_Scene->registry->get<StaticMeshComponent>(e);
				if (smc.handle == AssetHandle(0))
				{
					continue;
				}

				ensurePerEntityResources(smc.perEntityBuffer, smc.meshBindingSet);

				Ref<StaticMesh> sm = ResolveStaticMesh(project, smc.handle);
				if (!sm)
				{
					continue;
				}

				for (auto &m : sm->GetMeshInstances())
				{
					SkinnedMesh_GPUData gpuData;
					gpuData.transformation = tr.GetWorldMatrix() * m->global;
					gpuData.objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

					const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(gpuData.transformation)));
					gpuData.normal = glm::mat4(normalMat3);

					std::fill(std::begin(gpuData.boneTransforms), std::end(gpuData.boneTransforms), glm::mat4(1.0f));

                  smc.perEntityBuffer->SetData(cmd, Buffer(&gpuData, sizeof(gpuData)));

					auto &primitive = m->GetPrimitive();

					// Get override material first
					Ref<Material> material;
					if (smc.materialHandle != AssetHandle(0))
					{
						material = ResolveMaterial(project, smc.materialHandle);
					}
					else
					{
						material = ResolveMaterial(project, m->GetMaterialHandle());
					}

					if (!material)
					{
						continue;
					}

					if (!material->GetBindingSet())
					{
						MaterialTextures textures;
						auto assetManager = m_Scene->GetProject()->GetAssetManager();
						material->RetrieveTextures(assetManager, &textures);
						material->UpdateBindingSet(this, &textures, assetManager);

						if (!material->GetBindingSet())
						{
							continue;
						}
					}

					if (s_UploadedMaterialsThisPass.insert(material.get()).second)
					{
						material->UploadToGpu(cmd);
					}

					if (smc.meshBindingSet && material->GetBindingSet() && primitive->vertexBuffer && primitive->indexBuffer)
					{
						geomGState.bindings = { smc.meshBindingSet, material->GetBindingSet() };

						// Reset vertex buffers for this mesh (don't accumulate across loop iterations)
						geomGState.vertexBuffers.resize(0);
						geomGState.vertexBuffers.push_back({ primitive->vertexBuffer->GetHandle(), 0, 0 });
						geomGState.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });

						cmd->setGraphicsState(geomGState);

						nvrhi::DrawArguments args;
						args.setVertexCount(primitive->indexBuffer->GetCount());
						args.instanceCount = 1;

						cmd->drawIndexed(args);
					}
				}
			}
		}

		// 2D Pass
		{
			IGN_PROFILE_SCOPE("SceneRenderer::2DPass");
			if (m_Has2DPreRenderCache && m_Renderer2D->ReplayPreRenderCache(cmd, framebuffer))
			{
				IGN_PROFILE_SCOPE("SceneRenderer::2DPass::ReplayCache");
			}
			else
			{
				std::vector<PointLight2D_GPUData> pointLights2D;
				{
					IGN_PROFILE_SCOPE("SceneRenderer::2DPass::PointLightsView");

					auto pointLight2DView = m_Scene->registry->view<TransformComponent, PointLight2DComponent>();
					for (entt::entity e : pointLight2DView)
					{
						TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
						if (!tr.visible)
							continue;

						PointLight2DComponent &light = m_Scene->registry->get<PointLight2DComponent>(e);
						if (!light.enabled)
							continue;

						PointLight2D_GPUData gpuLight;
						gpuLight.position = glm::vec4(tr.translation, 1.0f);
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
						TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
						if (!tr.visible)
							continue;

						if (m_Scene->registry->all_of<Circle2DComponent>(e))
						{
							Circle2DComponent &circle = m_Scene->registry->get<Circle2DComponent>(e);
							const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));
							m_Renderer2D->DrawCircle(tr.GetWorldMatrix(), circle.color, circle.thickness, circle.fade, objectID);
						}
					}
				}
				{
					IGN_PROFILE_SCOPE("SceneRenderer::2DPass::Quad2DView");
					Project *project = m_Scene->GetProject();
					auto quad2DView = m_Scene->registry->view<TransformComponent, Sprite2DComponent>();
					for (entt::entity e : quad2DView)
					{
						TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
						if (!tr.visible)
							continue;

						Sprite2DComponent &sprite = m_Scene->registry->get<Sprite2DComponent>(e);
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
						Ref<Material2D> mat2d = m_Renderer2D->ResolveMaterial2D(project, sprite.materialHandle);

						// Render with Material2D
						if (mat2d)
						{
							Ref<Texture> texture = m_Renderer2D->ResolveTexture(project, mat2d->textureHandle);
							m_Renderer2D->DrawQuad(tr.GetWorldMatrix(), mat2d->data.baseColor, mat2d->data.additiveColor,
								mat2d->data.type, texture, uv0, uv1, mat2d->data.tilingFactor, objectID);
						}
						else // Render with default
						{
							Ref<Texture> texture = m_Renderer2D->ResolveTexture(project, sprite.handle);
							m_Renderer2D->DrawQuad(tr.GetWorldMatrix(), sprite.color, texture, uv0, uv1, sprite.tilingFactor, objectID);
						}

					}
				}

				{
					IGN_PROFILE_SCOPE("SceneRenderer::2DPass::TextView");
					auto textView = m_Scene->registry->view<TransformComponent, TextComponent>();
					for (entt::entity e : textView)
					{
						TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
						if (!tr.visible)
							continue;

						const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(m_Scene->registry->get<IDComponent>(e).uuid));

						TextComponent &text = m_Scene->registry->get<TextComponent>(e);
						if (text.fontHandle == AssetHandle(0) || text.text.empty())
							continue;

						Ref<Font> font = m_Scene->GetProject()->GetAsset<Font>(text.fontHandle, AssetType::Font);
						if (!font)
							continue;

						Ref<Texture> fontAtlas = font->GetAtlasTexture();
						if (!fontAtlas || !fontAtlas->IsReady())
							continue;

						glm::vec4 textColor = text.color;
						if (text.material2dHandle != AssetHandle(0))
						{
							if (Ref<Material2D> material = m_Scene->GetProject()->GetAsset<Material2D>(text.material2dHandle, AssetType::Material2D))
							{
								textColor = text.color + material->data.baseColor;
							}
						}

						m_Renderer2D->DrawString(text.text, font, textColor, tr.GetWorldMatrix(), text.kerning, text.lineSpacing, objectID);
					}
				}

				m_Renderer2D->Flush(framebuffer);
				m_Renderer2D->BuildPreRenderCache();
				m_Has2DPreRenderCache = true;
				m_Renderer2D->End();
			}
		} // !2D Pass
	}

	void SceneRenderer::DrawDebugGrid(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, const DebugGridStyle &style, bool is2D)
	{
       IGN_PROFILE_FUNCTION();
		if (!style.enabled || !m_DebugGridBuffer)
		{
			return;
		}

		Ref<GraphicsPipeline> gridPipeline = GetDebugGridPipelineForFB(framebuffer);
		nvrhi::BindingSetHandle bindingSet = GetOrCreateDebugGridBindingSet(gridPipeline->GetBindingLayout(0), m_DebugGridBuffer);

		DebugGrid_GPUData gpuData;
		gpuData.thinColor = style.thinColor;
		gpuData.thickColor = style.thickColor;
		gpuData.xAxisColor = style.xAxisColor;
		gpuData.yAxisColor = style.yAxisColor;
		gpuData.zAxisColor = style.zAxisColor;
		gpuData.settings0 = glm::vec4(
			glm::max(style.cellSize, 0.0001f),
			glm::max(style.minPixelsBetweenCells, 0.1f),
			glm::max(style.gridSize, 1.0f),
			glm::max(style.majorLineScale, 1.0f)
		);
		gpuData.settings1 = glm::vec4(
			is2D ? 1.0f : 0.0f,
			style.enableXAxis ? 1.0f : 0.0f,
			style.enableYAxis ? 1.0f : 0.0f,
			style.enableZAxis ? 1.0f : 0.0f
		);

		m_DebugGridBuffer->SetData(cmd, Buffer(&gpuData, sizeof(gpuData)));

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

	void SceneRenderer::DrawDebug2DPhysics(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
	{
		IGN_PROFILE_FUNCTION();
		m_Renderer2D->Begin(cmd);

		// 2D Physics debug draw
		constexpr glm::vec4 kPhysicsDebugColor = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
		constexpr int kCircleDebugSegments = 24;
		constexpr float kTwoPi = 6.28318530718f;

		auto boxCollider2DView = m_Scene->registry->view<TransformComponent, BoxCollider2DComponent>();
		for (entt::entity e : boxCollider2DView)
		{
			TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
			if (!tr.visible)
				continue;

			const BoxCollider2DComponent &box = m_Scene->registry->get<BoxCollider2DComponent>(e);
			const glm::mat4 world = tr.GetWorldMatrix();

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
			TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
			if (!tr.visible)
				continue;

			const CircleCollider2DComponent &circle = m_Scene->registry->get<CircleCollider2DComponent>(e);
			const glm::mat4 world = tr.GetWorldMatrix();
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

		m_Renderer2D->Flush(framebuffer);
		m_Renderer2D->End();
	}

	void SceneRenderer::DrawDebug3DPhysics(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
	{
		IGN_PROFILE_FUNCTION();
		m_Renderer2D->Begin(cmd);

		constexpr glm::vec4 kPhysicsDebugColor = glm::vec4(0.5f, 1.0f, 1.0f, 1.0f);
		constexpr int kCircleSegments = 24;
		constexpr float kTwoPi = 6.28318530718f;
		constexpr float kPi = 3.14159265359f;

		auto drawCircleRing = [this, kTwoPi](const glm::vec3 &center, const glm::vec3 &axisA, const glm::vec3 &axisB, int segments, const glm::vec4 &color)
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

        auto drawArc = [this, kPi](const glm::vec3 &center, const glm::vec3 &axisA, const glm::vec3 &axisB, int segments, const glm::vec4 &color)
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

		auto boxCollider = m_Scene->registry->view<TransformComponent, BoxColliderComponent>();
		for (entt::entity e : boxCollider)
		{
			TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
			if (!tr.visible)
				continue;

			const auto &box = m_Scene->registry->get<BoxColliderComponent>(e);
			const glm::mat4 world = tr.GetWorldMatrix();

			const glm::vec3 c = box.center;
			const glm::vec3 h = box.scale;
			const glm::vec3 corners[8] =
			{
				glm::vec3(world * glm::vec4(c.x - h.x, c.y - h.y, c.z - h.z, 1.0f)),
				glm::vec3(world * glm::vec4(c.x + h.x, c.y - h.y, c.z - h.z, 1.0f)),
				glm::vec3(world * glm::vec4(c.x + h.x, c.y + h.y, c.z - h.z, 1.0f)),
				glm::vec3(world * glm::vec4(c.x - h.x, c.y + h.y, c.z - h.z, 1.0f)),
				glm::vec3(world * glm::vec4(c.x - h.x, c.y - h.y, c.z + h.z, 1.0f)),
				glm::vec3(world * glm::vec4(c.x + h.x, c.y - h.y, c.z + h.z, 1.0f)),
				glm::vec3(world * glm::vec4(c.x + h.x, c.y + h.y, c.z + h.z, 1.0f)),
				glm::vec3(world * glm::vec4(c.x - h.x, c.y + h.y, c.z + h.z, 1.0f))
			};

			constexpr int edges[12][2] =
			{
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
			};

			for (const auto &edge : edges)
			{
				m_Renderer2D->DrawLine(corners[edge[0]], corners[edge[1]], kPhysicsDebugColor);
			}
		}

		auto sphereCollider = m_Scene->registry->view<TransformComponent, SphereColliderComponent>();
		for (entt::entity e : sphereCollider)
		{
			TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
			if (!tr.visible)
				continue;

			const auto &sphere = m_Scene->registry->get<SphereColliderComponent>(e);
			const glm::mat4 world = tr.GetWorldMatrix();

			const float maxAxis = glm::compMax(tr.scale);
			const float scaledRadius = sphere.radius * maxAxis;

			const glm::vec3 center = glm::vec3(world * glm::vec4(sphere.center, 1.0f));

            const glm::vec3 axisX = glm::normalize(glm::vec3(world[0])) * scaledRadius;
            const glm::vec3 axisY = glm::normalize(glm::vec3(world[1])) * scaledRadius;
            const glm::vec3 axisZ = glm::normalize(glm::vec3(world[2])) * scaledRadius;

            drawCircleRing(center, axisX, axisY, kCircleSegments, kPhysicsDebugColor);
            drawCircleRing(center, axisX, axisZ, kCircleSegments, kPhysicsDebugColor);
            drawCircleRing(center, axisY, axisZ, kCircleSegments, kPhysicsDebugColor);
		}

		auto capsuleCollider = m_Scene->registry->view<TransformComponent, CapsuleColliderComponent>();
		for (entt::entity e : capsuleCollider)
		{
			TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
			if (!tr.visible)
				continue;

			const auto &capsule = m_Scene->registry->get<CapsuleColliderComponent>(e);
			const glm::mat4 world = tr.GetWorldMatrix();
			const float maxAxis = std::max({ tr.scale.x, tr.scale.y, tr.scale.z });
			const float scaledRadius = capsule.radius * maxAxis;
			const float scaledHalfHeight = glm::max(capsule.height - capsule.radius, 0.0f) * maxAxis;

			const glm::vec3 center = glm::vec3(world * glm::vec4(capsule.center, 1.0f));
			const glm::vec3 right = glm::normalize(glm::vec3(world[0])) * scaledRadius;
			const glm::vec3 up = glm::normalize(glm::vec3(world[1])) * scaledRadius;
			const glm::vec3 forward = glm::normalize(glm::vec3(world[2])) * scaledHalfHeight;

			const glm::vec3 heightOffset = forward;
			const glm::vec3 topCenter = center + heightOffset;
			const glm::vec3 bottomCenter = center - heightOffset;

			drawCircleRing(topCenter, right, up, kCircleSegments, kPhysicsDebugColor);
			drawCircleRing(bottomCenter, right, up, kCircleSegments, kPhysicsDebugColor);

			m_Renderer2D->DrawLine(topCenter + right, bottomCenter + right, kPhysicsDebugColor);
			m_Renderer2D->DrawLine(topCenter - right, bottomCenter - right, kPhysicsDebugColor);
			m_Renderer2D->DrawLine(topCenter + up, bottomCenter + up, kPhysicsDebugColor);
			m_Renderer2D->DrawLine(topCenter - up, bottomCenter - up, kPhysicsDebugColor);

			const glm::vec3 axisRadius = forward;
			drawArc(topCenter, right, axisRadius, kCircleSegments / 2, kPhysicsDebugColor);
			drawArc(topCenter, up, axisRadius, kCircleSegments / 2, kPhysicsDebugColor);
			drawArc(bottomCenter, right, -axisRadius, kCircleSegments / 2, kPhysicsDebugColor);
			drawArc(bottomCenter, up, -axisRadius, kCircleSegments / 2, kPhysicsDebugColor);
		}

		auto meshCollider = m_Scene->registry->view<TransformComponent, MeshColliderComponent>();
		for (entt::entity e : meshCollider)
		{
			TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
			if (!tr.visible)
				continue;

			const auto &mesh = m_Scene->registry->get<MeshColliderComponent>(e);
			if (mesh.vertices.empty())
				continue;

			const glm::mat4 world = tr.GetWorldMatrix();

			if (!mesh.indices.empty() && mesh.indices.size() % 3 == 0)
			{
				for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
				{
					const uint32_t i0 = mesh.indices[i + 0];
					const uint32_t i1 = mesh.indices[i + 1];
					const uint32_t i2 = mesh.indices[i + 2];

					if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() || i2 >= mesh.vertices.size())
						continue;

					const glm::vec3 p0 = glm::vec3(world * glm::vec4(mesh.vertices[i0], 1.0f));
					const glm::vec3 p1 = glm::vec3(world * glm::vec4(mesh.vertices[i1], 1.0f));
					const glm::vec3 p2 = glm::vec3(world * glm::vec4(mesh.vertices[i2], 1.0f));

					m_Renderer2D->DrawLine(p0, p1, kPhysicsDebugColor);
					m_Renderer2D->DrawLine(p1, p2, kPhysicsDebugColor);
					m_Renderer2D->DrawLine(p2, p0, kPhysicsDebugColor);
				}
			}
			else if (mesh.vertices.size() >= 2)
			{
				for (size_t i = 0; i + 1 < mesh.vertices.size(); ++i)
				{
					const glm::vec3 p0 = glm::vec3(world * glm::vec4(mesh.vertices[i], 1.0f));
					const glm::vec3 p1 = glm::vec3(world * glm::vec4(mesh.vertices[i + 1], 1.0f));
					m_Renderer2D->DrawLine(p0, p1, kPhysicsDebugColor);
				}
			}
		}

		m_Renderer2D->Flush(framebuffer);
		m_Renderer2D->End();
	}

    void SceneRenderer::CompositePass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture, Ref<Texture> edgeTexture)
	{
		IGN_PROFILE_FUNCTION();
		Ref<GraphicsPipeline> compositePipeline = GetCompositePipelineForFB(framebuffer, nvrhi::RasterFillMode::Solid);
		nvrhi::BindingSetHandle bindingSet = GetOrCreateCompositeBindingSet(compositePipeline->GetBindingLayout(0), sceneTexture, uiTexture, edgeTexture, m_CompositeSampler);

		auto graphicsState = nvrhi::GraphicsState();
		graphicsState.pipeline = compositePipeline->GetHandle();
		graphicsState.framebuffer = framebuffer;
		graphicsState.vertexBuffers = { nvrhi::VertexBufferBinding { m_CompositeVertexBuffer->GetHandle(), 0, 0 } };
		graphicsState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
		graphicsState.bindings = { bindingSet };
		cmd->setGraphicsState(graphicsState);

		auto args = nvrhi::DrawArguments();
		args.instanceCount = 1;
		args.vertexCount = 6;
		cmd->draw(args);
	}

	void SceneRenderer::UpdateUIInput(const glm::vec2 &viewportMousePos, const glm::vec2 &viewportPos, const glm::vec2 &viewportSize, bool mousePressed)
	{
		UIManager &uiManager = UIManager::GetInstance();
		uiManager.SetMousePosition(viewportMousePos, viewportPos, viewportSize);
		uiManager.HandleMouseClick(mousePressed);
	}

	Ref<Texture> SceneRenderer::GetEnvironmentMapColorTexture() const
	{
		if (WorldEnvironment *world = GetActiveWorldEnvironment(m_Scene.get()))
		{
			if (world->environment)
			{
				return world->environment->GetHDRTexture();
			}
		}
		return nullptr;
	}

	Ref<Texture> SceneRenderer::GetCascadedShadowMapDepthTexture() const
	{
		if (m_CascadedShadowMap)
		{
			return m_CascadedShadowMap->GetDepthTexture();
		}
		return nullptr;
	}

	Ref<CascadedShadowMap> SceneRenderer::GetCascadedShadowMap()
	{
		return m_CascadedShadowMap;
	}

	void SceneRenderer::SetFillMode(nvrhi::RasterFillMode mode)
	{
		m_FillMode = mode;

		// Recreate pipelines
		s_GeometryPSOCache.clear();
		s_EnvironmentPSOCache.clear();
		s_CompositePSOCache.clear();
		s_DebugGridPSOCache.clear();
		s_CompositeBindingSetCache.clear();
		s_DebugGridBindingSetCache.clear();

		m_Renderer2D->SetFillMode(mode);
	}

	void SceneRenderer::SetSelectedEntity(const Entity &entity)
	{
		const uint32_t objectID = static_cast<uint32_t>(static_cast<uint64_t>(entity.GetUUID()));
		const auto it = std::ranges::find_if(m_SelectedEntities,
			[&](const uint32_t id)
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
}