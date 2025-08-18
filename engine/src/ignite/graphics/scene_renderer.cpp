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

#include "mesh.hpp"
#include "renderer.hpp"
#include "renderer_2d.hpp"
#include "environment.hpp"
#include "ui_renderer.hpp"
#include "ui/ui_manager.hpp"

#include "ignite/scene/scene.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"

#include "ignite/core/application.hpp"
#include "ignite/core/input/input.hpp"

#include <ranges>
#include <cstdlib>

#include "ignite/project/project.hpp"

namespace ignite
{
    static SceneRenderer *s_SceneRenderer = nullptr;

    static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_GeometryPSOCache;
    static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_EnvironmentPSOCache;
    static std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> s_CompositePSOCache;

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
            h ^= (std::hash<const void*>{}(k.sceneTex) + 0x9e3779b9 + (h<<6) + (h>>2));
            h ^= (std::hash<const void*>{}(k.uiTex) + 0x9e3779b9 + (h<<6) + (h>>2));
            return h;
        }
    };

    static std::unordered_map<CompositeBindingKey, nvrhi::BindingSetHandle, CompositeBindingKeyHash> s_CompositeBindingSetCache;

    // Helper to build a geometry pipeline for a framebuffer (once) and cache it.
    static Ref<GraphicsPipeline> GetGeomPipelineForFB(nvrhi::IFramebuffer* framebuffer, nvrhi::RasterFillMode fillMode)
    {
        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = s_GeometryPSOCache.find(key);
        if (it != s_GeometryPSOCache.end())
            return it->second;

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.depthWrite = true;
        params.depthTest = true;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.cullMode = nvrhi::RasterCullMode::None;

        auto attributes = VertexMesh_Anim::GetAttributes();
        GraphicsPipelineCreateInfo pci;
        pci.attributes = attributes.data();
        pci.attributeCount = static_cast<uint32_t>(attributes.size());

        auto gp = GraphicsPipeline::Create(params, &pci);
        gp->AddShader("mesh_anim.vertex.hlsl", nvrhi::ShaderType::Vertex)
          .AddShader("mesh_anim.pixel.hlsl", nvrhi::ShaderType::Pixel, "main", true)
          .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM))
          .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MATERIAL))
          .Build(framebuffer);

    s_GeometryPSOCache.emplace(key, gp);
        return gp;
    }

    // Helper to build an environment pipeline per framebuffer (once)
    static Ref<GraphicsPipeline> GetEnvPipelineForFB(nvrhi::IFramebuffer* framebuffer, nvrhi::RasterFillMode fillMode)
    {
        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = s_EnvironmentPSOCache.find(key);
        if (it != s_EnvironmentPSOCache.end())
            return it->second;

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.depthWrite = true;
        params.depthTest = true;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.cullMode = nvrhi::RasterCullMode::Front;
        params.comparison = nvrhi::ComparisonFunc::Always;

        auto attribute = Environment::GetAttribute();
        GraphicsPipelineCreateInfo pci;
        pci.attributes = &attribute;
        pci.attributeCount = 1;

        auto gp = GraphicsPipeline::Create(params, &pci);
        gp->AddShader("skybox.vertex.hlsl", nvrhi::ShaderType::Vertex)
          .AddShader("skybox.pixel.hlsl", nvrhi::ShaderType::Pixel, "main", true)
          .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::ENVIRONMENT))
          .Build(framebuffer);
        
        s_EnvironmentPSOCache.emplace(key, gp);
        return gp;
    }

    // Helper to build a composite pipeline per framebuffer (once)
    static Ref<GraphicsPipeline> GetCompositePipelineForFB(nvrhi::IFramebuffer* framebuffer, nvrhi::RasterFillMode fillMode)
    {
        auto key = MakeFramebufferKey(framebuffer, fillMode);
        auto it = s_CompositePSOCache.find(key);
        if (it != s_CompositePSOCache.end())
            return it->second;

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
        params.depthWrite = false;
        params.depthTest = false;
        params.enableDepthStencil = false;
        params.fillMode = fillMode;
        params.cullMode = nvrhi::RasterCullMode::None;

        auto attributes = VertexScreen::GetAttributes();
        GraphicsPipelineCreateInfo pci;
        pci.attributes = attributes.data();
        pci.attributeCount = static_cast<uint32_t>(attributes.size());

        // Create pipeline
        Ref<GraphicsPipeline> gp = GraphicsPipeline::Create(params, &pci);
        gp->AddShader("composite.vertex.hlsl", nvrhi::ShaderType::Vertex)
            .AddShader("composite.pixel.hlsl", nvrhi::ShaderType::Pixel, "main", true)
            .AddBindingLayout(bindingLayout)
            .Build(framebuffer);

        s_CompositePSOCache.emplace(key, gp);

        return gp;
    }

    static nvrhi::BindingSetHandle CreateCompositeBindingSet(nvrhi::IBindingLayout *bindingLayout, nvrhi::TextureHandle sceneTexture, nvrhi::TextureHandle uiTexture)
    {
        CompositeBindingKey key{ bindingLayout, sceneTexture.Get(), uiTexture.Get() };
        auto it = s_CompositeBindingSetCache.find(key);
        if (it != s_CompositeBindingSetCache.end())
            return it->second;
        
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // Composite Binding set
        auto bindingSetDesc = nvrhi::BindingSetDesc();
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, sceneTexture));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, uiTexture));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, Renderer::GetWhiteTexture()->GetSampler()));

    nvrhi::BindingSetHandle bindingSet = device->createBindingSet(bindingSetDesc, bindingLayout);
    s_CompositeBindingSetCache.emplace(key, bindingSet);

        return bindingSet;
    }

    SceneRenderer::SceneRenderer()
    {
        s_SceneRenderer = this;
    }

    SceneRenderer::~SceneRenderer()
    {
        s_GeometryPSOCache.clear();
        s_EnvironmentPSOCache.clear();
        s_CompositePSOCache.clear();
        s_CompositeBindingSetCache.clear();
    }

    void SceneRenderer::Create()
    {
        m_CommandList = CommandList::Create();

        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // Composite Pipeline & geometry
        {
            // Geometry
            VertexScreen vertices[]
            {
                { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
                { { -1.0f,  1.0f }, { 0.0f, 0.0f } },
                { {  1.0f,  1.0f }, { 1.0f, 0.0f } },

                { {  1.0f,  1.0f }, { 1.0f, 0.0f } },
                { {  1.0f, -1.0f }, { 1.0f, 1.0f } },
                { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
            };

            m_CompositeVertexBuffer = VertexBuffer::Create(sizeof(vertices));
            m_CompositeVertexBuffer->SetData(Buffer(vertices, sizeof(vertices)));
        }

        m_Device = Application::GetGraphicsDevice();
        
        // Create Environment
        CreateEnvironment();

        m_Renderer2D = Renderer2D::Create();
        m_UIRenderer = UIRenderer::Create(1280, 720);
        m_UIRenderer->SetUIManager(&UIManager::GetInstance());

        CreateDemoUI();
    }

    void SceneRenderer::SetActiveScene(Scene* scene)
    {
        m_Scene = scene;
    }

    void SceneRenderer::RenderTo(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT, bool renderEnvironment)
    {
        m_EntityBounds.clear();

        // Update UI system
        m_UIRenderer->Update(0.016f); // Assuming ~60 FPS for now

        m_CommandList->Begin();

        auto cmd = m_CommandList->GetActiveHandle();

        // setup camera constants
        CameraConstants cameraBuffer = { camera->GetViewProjectionMatrix(), glm::vec4(camera->position, 1.0f) };

        // Scene Render Target
        sceneRT->ClearColorAttachmentFloat(cmd, 0);
        sceneRT->ClearDepthAttachment(cmd, 1.0f, 0); // far depth = 1.0f == LessOrEqual

        nvrhi::IFramebuffer *framebuffer = sceneRT->GetFramebuffer();

        if (renderEnvironment)
        {
            Ref<GraphicsPipeline> envPSO = GetEnvPipelineForFB(framebuffer, m_FillMode);
            m_Environment->Begin(cmd, camera, framebuffer, envPSO);
        }

        auto skeletalMeshView = m_Scene->registry->view<Transform, SkeletalMesh>();
        for (entt::entity e : skeletalMeshView)
        {
            Transform &tr = m_Scene->registry->get<Transform>(e);
            if (!tr.visible)
                continue;

            SkeletalMesh &sm = m_Scene->registry->get<SkeletalMesh>(e);
            // if (Ref<MeshAsset> meshAsset = Project::GetActive()->GetAsset<MeshAsset>(sm.meshHandle))
            {
                // render each mesh
                for (size_t i = 0; i < sm.meshes.size(); ++i)
                {
                    auto &m = sm.meshes[i];
                    LOG_ASSERT(m, "[SceneRenderer] Mesh instance is null");

                    // Reload environment data
                    if (m_Environment->IsInvalidating())
                    {
                        m->UpdateBindingSet();
                        m->material->UpdateBindingSet();
                    }

                    m->constant.transformation = tr.GetWorldMatrix();
                    // Compute normal matrix (inverse-transpose of upper-left 3x3)
                    const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(m->constant.transformation)));
                    m->constant.normal = glm::mat4(normalMat3);

#if 0
                    // Recalculate the AABB using the full world transform.
                    glm::mat4 worldMatrix = tr.GetWorldMatrix();
                    glm::vec3 origMin = m->mesh.data.aabb.min;
                    glm::vec3 origMax = m->mesh.data.aabb.max;

                    glm::vec3 corners[8] =
                    {
                        glm::vec3(origMin.x, origMin.y, origMin.z),
                        glm::vec3(origMax.x, origMin.y, origMin.z),
                        glm::vec3(origMin.x, origMax.y, origMin.z),
                        glm::vec3(origMax.x, origMax.y, origMin.z),
                        glm::vec3(origMin.x, origMin.y, origMax.z),
                        glm::vec3(origMax.x, origMin.y, origMax.z),
                        glm::vec3(origMin.x, origMax.y, origMax.z),
                        glm::vec3(origMax.x, origMax.y, origMax.z)
                    };

                    glm::vec3 transformedMin(FLT_MAX);
                    glm::vec3 transformedMax(-FLT_MAX);

                    for (const auto &corner : corners)
                    {
                        glm::vec3 worldPos = glm::vec3(worldMatrix * glm::vec4(corner, 1.0f));
                        transformedMin = glm::min(transformedMin, worldPos);
                        transformedMax = glm::max(transformedMax, worldPos);
                    }

                    AABB worldAABB;
                    worldAABB.min = transformedMin;
                    worldAABB.max = transformedMax;
                    m_EntityBounds.push_back(worldAABB);
#endif
                    m->constantBuffer->SetData(cmd, Buffer(&m->constant, sizeof(m->constant)));
                    m->material->WriteBuffer(cmd);

                    // render
                    Ref<GraphicsPipeline> geomPSO = GetGeomPipelineForFB(framebuffer, m_FillMode);

                    auto state = nvrhi::GraphicsState();
                    state.pipeline = geomPSO->GetHandle();
                    state.framebuffer = framebuffer;
                    state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

                    state.bindings = { m->bindingSet, m->material->bindingSet };

                    state.addVertexBuffer({ m->mesh.GetVertexBuffer()->GetHandle(), 0, 0 });
                    state.setIndexBuffer({ m->mesh.GetIndexBuffer()->GetHandle(), nvrhi::Format::R32_UINT });

                    cmd->setGraphicsState(state);

                    // push camera constants
                    cmd->setPushConstants(&cameraBuffer, sizeof(CameraConstants));

                    nvrhi::DrawArguments args;
                    args.setVertexCount(m->mesh.GetIndicesCount());
                    args.instanceCount = 1;

                    cmd->drawIndexed(args);
                }
            }

        }

        // 2D Pass
        m_Renderer2D->Begin(cmd, cameraBuffer);
        auto object2DView = m_Scene->registry->view<Transform, Sprite2D>();
        for (entt::entity e : object2DView)
        {
            Transform &tr = m_Scene->registry->get<Transform>(e);
            if (!tr.visible)
                continue;

            Sprite2D &sprite = m_Scene->registry->get<Sprite2D>(e);
            Ref<Texture> texture = Project::GetAsset<Texture>(sprite.handle);
            m_Renderer2D->DrawQuad(tr.GetWorldMatrix(), sprite.color, texture, sprite.tilingFactor);
        }

        for (auto &aabb : m_EntityBounds)
        {
            m_Renderer2D->DrawAABB(aabb, { 1.0f, 0.0f, 0.0f, 1.0f });
        }

        m_Renderer2D->Flush(framebuffer);
        m_Renderer2D->End();

        // UI Pass
        {
            uiRT->ClearColorAttachmentFloat(cmd, 0);
            nvrhi::IFramebuffer *framebuffer = uiRT->GetFramebuffer();
            m_UIRenderer->Render(cmd, framebuffer);
        }

        // Composite Pass
        {
            compositeRT->ClearColorAttachmentFloat(cmd, 0);
            nvrhi::IFramebuffer *framebuffer = compositeRT->GetFramebuffer();

            Ref<GraphicsPipeline> compositePipeline = GetCompositePipelineForFB(framebuffer, m_FillMode);
            nvrhi::BindingSetHandle bindingSet = CreateCompositeBindingSet(compositePipeline->GetBindingLayout(0),
                sceneRT->GetColorAttachment(0), uiRT->GetColorAttachment(0));

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

        if (renderEnvironment)
        {
            m_Environment->End();
        }

        m_CommandList->Submit();
    }

    void SceneRenderer::UpdateUIInput(const glm::vec2 &viewportMousePos, const glm::vec2 &viewportPos, const glm::vec2 &viewportSize, bool mousePressed)
    {
        UIManager& uiManager = UIManager::GetInstance();
        uiManager.SetMousePosition(viewportMousePos, viewportPos, viewportSize);
        uiManager.HandleMouseClick(mousePressed);
    }

    void SceneRenderer::CreateDemoUI()
    {
        UIManager& uiManager = UIManager::GetInstance();

        // Enable layout grid
        uiManager.SetLayoutGridVisible(false);
        uiManager.SetLayoutGridSize(50);

        for (int i = 0; i < static_cast<int>(UIAlignment::COUNT); ++i)
        {
            // Create demo buttons
            const UIAlignment alignment = static_cast<UIAlignment>(i);

            if (alignment == UIAlignment::CENTER)
            {
                TextureCreateInfo createInfo = {};
                createInfo.mipLevels = 1;
                createInfo.format = nvrhi::Format::RGBA8_UNORM;

                Ref<Texture> image = Texture::Create("resources/textures/cursor_128px.png", createInfo);

                auto commandList = m_Device->createCommandList();
                commandList->open();
                image->Write(commandList);
                commandList->close();
                m_Device->executeCommandList(commandList);

                auto button = uiManager.CreateButton("button", glm::vec2(0), glm::vec2(50.0f, 50.0f));
                button->SetAlignment(alignment);
                button->SetImage(image);

                float r = 1.0f;
                float g = 1.0f;
                float b = 1.0f;
    
                button->SetColors(
                    glm::vec4(r, g, b, 1.0f), // normal - green
                    glm::vec4(r + 0.4f, g + 0.4f, b + 0.4f, 1.0f), // hover - lighter green
                    glm::vec4(r - 0.4f, g - 0.4f, b - 0.4f, 1.0f)  // pressed - darker green
                );
            }
            else
            {
                auto button = uiManager.CreateButton("button", glm::vec2(0), glm::vec2(100.0f, 50.0f));
                button->SetAlignment(alignment);
    
                float r = static_cast<float>(rand()) / RAND_MAX * 0.5f + 0.2f;
                float g = static_cast<float>(rand()) / RAND_MAX * 0.5f + 0.2f;
                float b = static_cast<float>(rand()) / RAND_MAX * 0.5f + 0.2f;
    
                button->SetColors(
                    glm::vec4(r, g, b, 1.0f), // normal - green
                    glm::vec4(r + 0.4f, g + 0.4f, b + 0.4f, 1.0f), // hover - lighter green
                    glm::vec4(r - 0.4f, g - 0.4f, b - 0.4f, 1.0f)  // pressed - darker green
                );
                button->SetOnClick([]() {
                    LOG_INFO("Play button clicked!");
                });
            }
        }
    }

    void SceneRenderer::SetFillMode(nvrhi::RasterFillMode mode)
    {
        m_FillMode = mode;

        // Recreate pipeline
        s_GeometryPSOCache.clear();
        s_EnvironmentPSOCache.clear();
        s_CompositePSOCache.clear();
    s_CompositeBindingSetCache.clear();

        m_Renderer2D->ClearPipelineCache();
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

    void SceneRenderer::CreateEnvironment()
    {
        // create environment
        m_Environment = Environment::Create();
        m_Environment->LoadTexture("resources/hdr/klippad_sunrise_2_2k.hdr");

        auto commandList = m_CommandList->GetActiveHandle();

        commandList->open();
        m_Environment->WriteBuffer(commandList);
        commandList->close();
        m_Device->executeCommandList(commandList);

        m_Environment->SetSunDirection(50.0f, -27.0f);
    }

    SceneRenderer* SceneRenderer::GetActive()
    {
        return s_SceneRenderer;
    }
}
