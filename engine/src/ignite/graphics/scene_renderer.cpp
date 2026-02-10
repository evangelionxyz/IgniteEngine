/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "scene_renderer.hpp"
#include "framebuffer_key.hpp"

#include "renderer.hpp"
#include "renderer_2d.hpp"
#include "ui_renderer.hpp"
#include "ui/ui_manager.hpp"

#include "ignite/scene/scene.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"

#include "ignite/core/application.hpp"

#include "objects/shadow_map.hpp"

#include <ranges>
#include <cstdlib>
#include <algorithm>
#include <array>

#include "ignite/project/project.hpp"

namespace ignite {
	static SceneRenderer *s_SceneRenderer = nullptr;

	static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_GeometryPSOCache;
	static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_EnvironmentPSOCache;
	static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_CompositePSOCache;

	// Helper to build a geometry pipeline for a framebuffer (once) and cache it.
	static Ref<GraphicsPipeline> GetGeomPipelineForFB(nvrhi::IFramebuffer* framebuffer, nvrhi::RasterFillMode fillMode)
	{
		auto key = MakeFramebufferKey(framebuffer, fillMode);
		auto it = s_GeometryPSOCache.find(key);
		if (it != s_GeometryPSOCache.end())
		{
			for (auto itErase = s_GeometryPSOCache.begin(); itErase != s_GeometryPSOCache.end();)
			{
				if (itErase != it)
				{
					itErase = s_GeometryPSOCache.erase(itErase);
				}
				else
				{
					++itErase;
				}
			}
			return it->second;
		}

		GraphicsPipelineParams params;
		params.enableBlend = false;
		params.enableDepthWrite = true;
		params.enableDepthTest = true;
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
	static Ref<GraphicsPipeline> GetEnvPipelineForFB(nvrhi::IFramebuffer* framebuffer, nvrhi::RasterFillMode fillMode)
	{
		auto key = MakeFramebufferKey(framebuffer, fillMode);
		auto it = s_EnvironmentPSOCache.find(key);
		if (it != s_EnvironmentPSOCache.end())
		{
			for (auto itErase = s_EnvironmentPSOCache.begin(); itErase != s_EnvironmentPSOCache.end();)
			{
				if (itErase != it)
				{
					itErase = s_EnvironmentPSOCache.erase(itErase);
				}
				else
				{
					++itErase;
				}
			}
			return it->second;
		}

		GraphicsPipelineParams params;
		params.enableBlend = true;
		params.enableDepthWrite = true;
		params.enableDepthTest = true;
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
	static Ref<GraphicsPipeline> GetCompositePipelineForFB(nvrhi::IFramebuffer* framebuffer, nvrhi::RasterFillMode fillMode)
	{
		auto key = MakeFramebufferKey(framebuffer, fillMode);
		auto it = s_CompositePSOCache.find(key);

		if (it != s_CompositePSOCache.end())
		{
			for (auto itErase = s_CompositePSOCache.begin(); itErase != s_CompositePSOCache.end();)
			{
				if (itErase != it)
				{
					itErase = s_CompositePSOCache.erase(itErase);
				}
				else
				{
					++itErase;
				}
			}
			return it->second;
		}

		nvrhi::IDevice *device = Application::GetGraphicsDevice();

		// Binding layout
		nvrhi::BindingLayoutDesc layoutDesc = {};
		layoutDesc.visibility = nvrhi::ShaderType::All;
		layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // scene
		layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // ui
		layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0)); // sampler
		nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(layoutDesc);

		GraphicsPipelineParams params;
		params.enableBlend = false;
		params.enableDepthWrite = false;
		params.enableDepthTest = false;
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
		nvrhi::IBindingLayout* layout = nullptr;
		nvrhi::ITexture* sceneTex = nullptr;
		nvrhi::ITexture* uiTex = nullptr;
		bool operator==(const CompositeBindingKey& other) const noexcept
		{
			return layout == other.layout && sceneTex == other.sceneTex && uiTex == other.uiTex;
		}
	};

	struct CompositeBindingKeyHash
	{
		size_t operator()(const CompositeBindingKey& k) const noexcept
		{
			size_t h = std::hash<const void*>{}(k.layout);
			h ^= (std::hash<const void*>{}(k.sceneTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			h ^= (std::hash<const void*>{}(k.uiTex) + 0x9e3779b9 + (h << 6) + (h >> 2));
			return h;
		}
	};

	static std::unordered_map<CompositeBindingKey, nvrhi::BindingSetHandle, CompositeBindingKeyHash> s_CompositeBindingSetCache;
	static nvrhi::BindingSetHandle GetOrCreateCompositeBindingSet(nvrhi::IBindingLayout *bindingLayout,
		Ref<Texture> sceneTexture, Ref<Texture> uiTexture, nvrhi::SamplerHandle sampler)
	{
		CompositeBindingKey key{ bindingLayout, sceneTexture->GetHandle(), uiTexture->GetHandle() };
		auto it = s_CompositeBindingSetCache.find(key);
		if (it != s_CompositeBindingSetCache.end())
		{
			for (auto itErase = s_CompositeBindingSetCache.begin(); itErase != s_CompositeBindingSetCache.end();)
			{
				if (itErase != it)
				{
					itErase = s_CompositeBindingSetCache.erase(itErase);
				}
				else
				{
					++itErase;
				}
			}

			return it->second;
		}

		nvrhi::IDevice *device = Application::GetGraphicsDevice();
		// Composite Binding set
		auto bindingSetDesc = nvrhi::BindingSetDesc();
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, sceneTexture->GetHandle()));
		bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, uiTexture->GetHandle()));
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
		nvrhi::IBindingLayout* layout = nullptr;
		bool operator==(const CSMBindingKey& other) const noexcept { return layout == other.layout; }
	};

	struct CSMBindingKeyHash
	{
		size_t operator()(const CSMBindingKey& k) const noexcept
		{
			size_t h = std::hash<const void*>{}(k.layout);
			return h;
		}
	};

	static std::unordered_map<CSMBindingKey, nvrhi::BindingSetHandle, CSMBindingKeyHash> s_CSMBindingSetCache;
	static nvrhi::BindingSetHandle GetOrCreateCSMBindingSet(nvrhi::IBindingLayout* bindingLayout, Ref<ConstantBuffer> skinnedMeshGPUDataBuffer, Ref<ConstantBuffer> csmGPUDataBuffer)
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

		nvrhi::IDevice* device = Application::GetGraphicsDevice();

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
		s_SceneRenderer = this;

		auto device = Application::GetGraphicsDevice();
		auto samplerDesc = nvrhi::SamplerDesc();
		samplerDesc.setAllFilters(true);
		samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
		m_CompositeSampler = device->createSampler(samplerDesc);
	}

	SceneRenderer::~SceneRenderer()
	{
		s_GeometryPSOCache.clear();
		s_EnvironmentPSOCache.clear();
		s_CompositePSOCache.clear();
		s_CompositeBindingSetCache.clear();
		s_CSMBindingSetCache.clear();

		s_SceneRenderer = nullptr;
	}

	void SceneRenderer::Create()
	{
		m_Device = Application::GetGraphicsDevice();
		nvrhi::CommandListHandle cmd = m_Device->createCommandList();

		std::array vertices
		{
			VertexScreen{ { -1.0f, -1.0f }, { 0.0f, 1.0f } },
			VertexScreen{ { -1.0f,  1.0f }, { 0.0f, 0.0f } },
			VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },

			VertexScreen{ {  1.0f,  1.0f }, { 1.0f, 0.0f } },
			VertexScreen{ {  1.0f, -1.0f }, { 1.0f, 1.0f } },
            VertexScreen { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
		};

		m_CompositeVertexBuffer = VertexBuffer::Create(sizeof(vertices));

		cmd->open();
		m_CompositeVertexBuffer->SetData(cmd, Buffer(vertices.data(), sizeof(vertices)));
		cmd->close();
		Application::SubmitWorkerCommandList(cmd);

		m_Renderer2D = Renderer2D::Create();
		m_UIRenderer = UIRenderer::Create(1280, 720);
		m_UIRenderer->SetUIManager(&UIManager::GetInstance());

		m_CascadedShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::HIGH);
	}

	void SceneRenderer::SetActiveScene(const Ref<Scene> &scene)
	{
		if (m_Scene == scene)
		{
			return;
		}

		m_Scene = scene;

		if (m_Scene)
		{
			// Wait for GPU to finish all operations before releasing resources
			Application::GetGraphicsDevice()->waitForIdle();
			
			// Clear environment-related caches to release GPU resources
			s_GeometryPSOCache.clear();
			s_EnvironmentPSOCache.clear();
			s_CompositePSOCache.clear();
			s_CompositeBindingSetCache.clear();
			s_CSMBindingSetCache.clear();
			
			// Release environment resources
			m_Environment.reset();
			
			// Wait again to ensure environment destruction completes
			Application::GetGraphicsDevice()->waitForIdle();

			// Create environment
			nvrhi::CommandListHandle cmd = m_Device->createCommandList();
			cmd->open();
			m_Environment = Environment::Create(m_Scene.get());
			m_Environment->LoadTexture("resources/hdr/klippad_sunrise_2_2k.hdr", cmd);
			m_Environment->UpdateBindingSet();
			m_Environment->WriteBuffer(cmd);
			cmd->close();
			
			// Execute immediately and wait for completion
			m_Device->executeCommandList(cmd);
			m_Device->waitForIdle();
		}
	}

	void SceneRenderer::RenderTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment)
	{
		m_EntityBounds.clear();

		// Update UI system
		m_UIRenderer->Update(0.016f); // Assuming ~60 FPS for now

		// Create fresh command list for this frame
		nvrhi::CommandListHandle cmd = m_Device->createCommandList();
		cmd->open();

		m_Scene->WriteBuffer(cmd);

		// setup camera constants
		CameraBuffer cameraBuffer = { camera->projection, camera->view, glm::vec4(camera->position, 1.0f) };
		Renderer::GetCameraConstantBuffer()->SetData(cmd, Buffer(&cameraBuffer, sizeof(CameraBuffer)));

		// Clear Render Targets
		// far depth = 1.0f == LessOrEqual
		uiRT->ClearColorAttachmentFloat(cmd, 0);
		uiRT->ClearDepthAttachment(cmd, 1.0f, 0);

		sceneRT->ClearColorAttachmentFloat(cmd, 0);
		sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);

		compositeRT->ClearColorAttachmentFloat(cmd, 0);
		compositeRT->ClearDepthAttachment(cmd, 1.0f, 0);

		nvrhi::IFramebuffer *framebuffer = sceneRT->GetFramebuffer();

		// CSM Pass
		// ShadowPass(cmd, camera);

		if (renderEnvironment)
		{
			const Ref<GraphicsPipeline> envPSO = GetEnvPipelineForFB(framebuffer, m_FillMode);
			m_Environment->Begin(cmd, camera, framebuffer, envPSO);
		}

		// Color pass
		ColorPass(cmd, camera, framebuffer);

		// UI Pass
		m_UIRenderer->Render(cmd, uiRT->GetFramebuffer());

		// Composite Pass
		CompositePass(cmd, compositeRT->GetFramebuffer(), sceneRT->GetColorAttachment(0), uiRT->GetColorAttachment(0));

		if (renderEnvironment)
		{
			m_Environment->End();
		}

		cmd->close();
		Application::SubmitWorkerCommandList(cmd);
	}

	void SceneRenderer::ShadowPass(nvrhi::ICommandList *cmd, ICamera *camera)
	{
		auto meshView = m_Scene->registry->view<TransformComponent, StaticMeshComponent>();

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

		// Pass sunDirection directly (surface -> sun); the shadow map logic flips it so
		// the light camera looks back toward the scene while the shader still uses
		// the surface-to-light vector for shading.
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

			// Get the framebuffer for this specific cascade layer
			nvrhi::IFramebuffer* csmFramebuffer = m_CascadedShadowMap->GetCascadeFramebuffer(i);
			csmState.framebuffer = csmFramebuffer;

			// Set viewport for this cascade
			nvrhi::Viewport viewport = csmFramebuffer->getFramebufferInfo().getViewport();
			csmState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(viewport);

			for (entt::entity e : meshView)
			{
				auto& tr = m_Scene->registry->get<TransformComponent>(e);
				auto& smc = m_Scene->registry->get<StaticMeshComponent>(e);
#if 0
				if (!mesh.model)
					continue;

				MeshScene& meshScene = mesh.model->GetScene();
				for (auto& mesh : meshScene.flatMeshes)
				{
					CascadedShadowMapModel_GPUData gpuData;
					gpuData.transformation = tr.GetLocalMatrix() * mesh->local;
					std::fill(std::begin(gpuData.boneTransforms), std::end(gpuData.boneTransforms), glm::mat4(1.0f));

					m_CascadedShadowMap->GetModelGPUDataBuffer()->SetData(cmd, Buffer(&gpuData, sizeof(CascadedShadowMapModel_GPUData)));
					nvrhi::BindingSetHandle bindingSet = GetOrCreateCSMBindingSet(csmPipeline->GetBindingLayout(0),
						m_CascadedShadowMap->GetModelGPUDataBuffer(),
						m_CascadedShadowMap->GetGPUDataBuffer()
					);

					csmState.bindings = { bindingSet };
					csmState.vertexBuffers = { { mesh->vertexBuffer->GetHandle(), 0, 0 } };
					csmState.setIndexBuffer({ mesh->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });

					cmd->setGraphicsState(csmState);

					nvrhi::DrawArguments args;
					args.setVertexCount(mesh->indexBuffer->GetCount());
					args.instanceCount = 1;

					cmd->drawIndexed(args);
				}
#endif
			}
		}
	}

	void SceneRenderer::ColorPass(nvrhi::ICommandList* cmd, ICamera* camera, nvrhi::IFramebuffer *framebuffer)
	{
		auto meshView = m_Scene->registry->view<TransformComponent, StaticMeshComponent>();
		Ref<GraphicsPipeline> geomPSO = GetGeomPipelineForFB(framebuffer, m_FillMode);
		nvrhi::GraphicsState geomGState = nvrhi::GraphicsState();
		geomGState.pipeline = geomPSO->GetHandle();
		geomGState.framebuffer = framebuffer;
		geomGState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

		for (entt::entity e : meshView)
		{
			TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
			auto &smc = m_Scene->registry->get<StaticMeshComponent>(e);

			if (smc.handle == AssetHandle(0))
			{
				continue;
			}

			// Create per entity buffer (once per entity)
			if (!smc.perEntityBuffer)
			{
				smc.perEntityBuffer = ConstantBuffer::Create(
					sizeof(SkinnedMesh_GPUData),
					true,
					16,
					"Per-Entity Transform Buffer"
				);
			}

			// Create binding set (once per entity) - only if buffer handles changed
			if (!smc.meshBindingSet)
			{
				nvrhi::IDevice *device = Application::GetGraphicsDevice();
				auto desc = nvrhi::BindingSetDesc();
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraConstantBuffer()->GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, smc.perEntityBuffer->GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, m_Scene->GetSceneGPUDataBuffer()->GetHandle()));
				desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, m_Scene->GetCSMGPUDataBuffer()->GetHandle()));

				smc.meshBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM));
				LOG_ASSERT(smc.meshBindingSet, "Failed to create mesh binding set");
			}

			Ref<StaticMesh> sm = Project::GetInstance()->GetAsset<StaticMesh>(smc.handle);
			if (!sm)
			{
				continue;
			}

			// Update per entity transform data (every frame)
			SkinnedMesh_GPUData gpuData;
			gpuData.transformation = tr.GetLocalMatrix();// *mesh->local;

			const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(gpuData.transformation)));
			gpuData.normal = glm::mat4(normalMat3);

			std::fill(std::begin(gpuData.boneTransforms),
				std::end(gpuData.boneTransforms),
				glm::mat4(1.0f));

			// Write updated transform to buffer (every frame)
			smc.perEntityBuffer->SetData(cmd, Buffer(&gpuData, sizeof(gpuData)));

			for (auto &m : sm->GetMeshInstances())
			{
				auto &primitive = m->GetPrimitive();

				Ref<Material> material = Project::GetInstance()->GetAsset<Material>(m->GetMaterialHandle());
				if (!material)
				{
					continue;
				}
				
				if (!material->GetBindingSet())
				{
					material->UpdateBindingSet();
				}
				
				material->UploadToGpu(cmd);
				
				if (smc.meshBindingSet && material->GetBindingSet() && primitive->vertexBuffer && primitive->indexBuffer)
				{
					geomGState.bindings = { smc.meshBindingSet, material->GetBindingSet() };

					geomGState.addVertexBuffer({ primitive->vertexBuffer->GetHandle(), 0, 0 });
					geomGState.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });

					cmd->setGraphicsState(geomGState);

					nvrhi::DrawArguments args;
					args.setVertexCount(primitive->indexBuffer->GetCount());
					args.instanceCount = 1;

					cmd->drawIndexed(args);
				}
			}
		}

		// 2D Pass
		m_Renderer2D->Begin(cmd);
		auto object2DView = m_Scene->registry->view<TransformComponent, Sprite2DComponent>();
		for (entt::entity e : object2DView)
		{
			TransformComponent &tr = m_Scene->registry->get<TransformComponent>(e);
			if (!tr.visible)
				continue;

			Sprite2DComponent &sprite = m_Scene->registry->get<Sprite2DComponent>(e);
			Ref<Texture> texture = Project::GetInstance()->GetAsset<Texture>(sprite.handle);
			m_Renderer2D->DrawQuad(tr.GetWorldMatrix(), sprite.color, texture, sprite.tilingFactor);
		}

		for (auto &aabb : m_EntityBounds)
		{
			m_Renderer2D->DrawAABB(aabb, { 1.0f, 0.0f, 0.0f, 1.0f });
		}

		m_Renderer2D->Flush(framebuffer);
		m_Renderer2D->End();
	}

	void SceneRenderer::CompositePass(nvrhi::ICommandList* cmd, nvrhi::IFramebuffer* framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture)
	{
		Ref<GraphicsPipeline> compositePipeline = GetCompositePipelineForFB(framebuffer, nvrhi::RasterFillMode::Solid);
		nvrhi::BindingSetHandle bindingSet = GetOrCreateCompositeBindingSet(compositePipeline->GetBindingLayout(0),
			sceneTexture, uiTexture, m_CompositeSampler);

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
		UIManager& uiManager = UIManager::GetInstance();
		uiManager.SetMousePosition(viewportMousePos, viewportPos, viewportSize);
		uiManager.HandleMouseClick(mousePressed);
	}

	Ref<Texture> SceneRenderer::GetEnvironmentMapColorTexture() const
	{
		if (m_Environment)
		{
			return m_Environment->GetHDRTexture();
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
		s_CompositeBindingSetCache.clear();

		m_Renderer2D->SetFillMode(mode);
	}

	void SceneRenderer::SetSelectedEntity(const Entity &entity)
	{
		const auto it = std::ranges::find_if(m_SelectedEntities,
		[&](const uint32_t id)
		{
			return id == static_cast<uint32_t>(entity);
		});

		// push back if not found
		if (it == m_SelectedEntities.end())
		{
			m_SelectedEntities.push_back(entity);
		}
	}

	void SceneRenderer::UnselectEntity(const Entity &entity)
	{
		auto it = std::ranges::find_if(m_SelectedEntities,
		[&](const uint32_t id)
		{
			return id == static_cast<uint32_t>(entity);
		});

		// remove if found
		if (it != m_SelectedEntities.end())
		{
			it = m_SelectedEntities.erase(it);
		}
	}

	void SceneRenderer::ClearSelectedEntities()
	{
		m_SelectedEntities.clear();
	}

	SceneRenderer* SceneRenderer::GetActive()
	{
		return s_SceneRenderer;
	}
}
