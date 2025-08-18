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

    SceneRenderer::SceneRenderer()
    {
        s_SceneRenderer = this;
    }

    SceneRenderer::~SceneRenderer()
    {
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

            GraphicsPipelineParams params;
            params.enableBlend = false;
            params.depthWrite = false;
            params.depthTest = false;
            params.enableDepthStencil = false;
            params.fillMode = nvrhi::RasterFillMode::Solid;
            params.cullMode = nvrhi::RasterCullMode::None;

            auto attributes = VertexScreen::GetAttributes();
            GraphicsPipelineCreateInfo pci;
            pci.attributes = attributes.data();
            pci.attributeCount = static_cast<uint32_t>(attributes.size());

            // Binding layout
            nvrhi::BindingLayoutDesc layoutDesc = {};
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // scene
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // ui
            // layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2)); // edge detection
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0)); // sampler
            nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(layoutDesc);

            // Create pipeline
            m_CompositePipeline = GraphicsPipeline::Create(params, &pci);
            m_CompositePipeline->AddShader("composite.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("composite.pixel.hlsl", nvrhi::ShaderType::Pixel, "main", true)
                .AddBindingLayout(bindingLayout)
                .Build();
        }

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.depthWrite = true;
        params.depthTest = true;
        params.enableDepthStencil = false;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;

        // Geometry pipeline
        {
            auto attributes = VertexMesh_Anim::GetAttributes();
            GraphicsPipelineCreateInfo pci;
            pci.attributes = attributes.data();
            pci.attributeCount = static_cast<uint32_t>(attributes.size());

            m_GeometryAnimPipeline = GraphicsPipeline::Create(params, &pci);
            m_GeometryAnimPipeline->AddShader("mesh_anim.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("mesh_anim.pixel.hlsl", nvrhi::ShaderType::Pixel, "main", true)
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM))
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MATERIAL))
                .Build();
        }

        // Environment Pipeline
        {
            params.cullMode = nvrhi::RasterCullMode::Front;
            params.comparison = nvrhi::ComparisonFunc::Always;

            auto attribute = Environment::GetAttribute();
            GraphicsPipelineCreateInfo pci;
            pci.attributes = &attribute;
            pci.attributeCount = 1;

            m_EnvironmentPipeline = GraphicsPipeline::Create(params, &pci);
            m_EnvironmentPipeline->AddShader("skybox.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("skybox.pixel.hlsl", nvrhi::ShaderType::Pixel, "main", true)
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::ENVIRONMENT))
                .Build();
        }

        m_Device = Application::GetGraphicsDevice();
        
        // Create Environment
        CreateEnvironment();
        CreateRenderTargets();
        CreatePipelines();

        m_Renderer2D = Renderer2D::Create(m_SceneRenderTarget);

        const uint32_t width = m_SceneRenderTarget->GetWidth();
        const uint32_t height = m_SceneRenderTarget->GetHeight();

        // Create UI Renderer
        m_UIRenderer = UIRenderer::Create(width, height);

        // Set up UI Manager with UI Renderer
        m_UIRenderer->SetUIManager(&UIManager::GetInstance());
        UIManager::GetInstance().SetViewportSize(width, height);

        // Create Edge Detection
        // m_EdgeDetection = EdgeDetection::Create();

        // m_EdgeDetection->CreatePipeline();
        // m_EdgeDetection->CreateOutputTexture(width, height);

        // m_EdgeDetection->UpdateBindingSet(m_SceneRenderTarget->GetColorAttachment(0), m_SceneRenderTarget->GetColorAttachment(1), m_SceneRenderTarget->GetDepthAttachment());

        CompositeUpdateBindingSet();

        // m_SelectedEntities.reserve(100);
        // m_EdgeDetectionParams.texelSize.x = 1.0f / static_cast<float>(width);
        // m_EdgeDetectionParams.texelSize.y = 1.0f / static_cast<float>(height);
        // m_EdgeDetectionParams.depthSensitivity = 1.0f;
        // m_EdgeDetectionParams.selectedCount = static_cast<uint32_t>(m_SelectedEntities.size());
        // m_EdgeDetectionParams.outlineColor = { 1.0f, 0.75f, 0.0f, 1.0f };

        // Create some demo UI elements
        CreateDemoUI();
    }

    void SceneRenderer::SetActiveScene(Scene* scene)
    {
        m_Scene = scene;
    }

    bool SceneRenderer::ShouldResize() const
    {
        return m_SceneRenderTarget->GetWidth() != m_Scene->viewportWidth || m_SceneRenderTarget->GetHeight() != m_Scene->viewportHeight;
    }

    void SceneRenderer::Resize(uint32_t width, uint32_t height)
    {
        m_SceneRenderTarget->Resize(width, height);
        m_CompositeRenderTarget->Resize(width, height);

        m_UIRenderer->Resize(width, height);
        m_Scene->Resize(width, height);

        // UIManager::GetInstance().SetViewportSize(width, height);

        // m_EdgeDetection->CreateOutputTexture(width, height);
        // m_EdgeDetection->UpdateBindingSet(m_SceneRenderTarget->GetColorAttachment(0), m_SceneRenderTarget->GetColorAttachment(1), m_SceneRenderTarget->GetDepthAttachment());

        CompositeUpdateBindingSet();

        // m_EdgeDetectionParams.texelSize.x = 1.0f / static_cast<float>(width);
        // m_EdgeDetectionParams.texelSize.y = 1.0f / static_cast<float>(height);
    }

    void SceneRenderer::CreatePipelines()
    {
        m_EnvironmentPipeline->CreatePipeline(m_SceneRenderTarget->GetFramebuffer());
        m_GeometryAnimPipeline->CreatePipeline(m_SceneRenderTarget->GetFramebuffer());

        // Composite
        m_CompositePipeline->CreatePipeline(m_CompositeRenderTarget->GetFramebuffer());
    }

    void SceneRenderer::Render(ICamera *camera, bool renderEnvironment)
    {
        m_EntityBounds.clear();

        // Update UI system
        m_UIRenderer->Update(0.016f); // Assuming ~60 FPS for now

        m_CommandList->Begin();

        auto cmd = m_CommandList->GetActiveHandle();

        // setup camera constants
        CameraConstants cameraBuffer = { camera->GetViewProjectionMatrix(), glm::vec4(camera->position, 1.0f) };

        // Scene Render Target
        m_SceneRenderTarget->ClearColorAttachmentFloat(cmd, 0);
        m_SceneRenderTarget->ClearDepthAttachment(cmd, 1.0f, 0); // far depth = 1.0f == LessOrEqual

        nvrhi::IFramebuffer *sceneFramebuffer = m_SceneRenderTarget->GetFramebuffer();

        if (renderEnvironment)
        {
            m_Environment->Begin(cmd, camera, sceneFramebuffer, m_EnvironmentPipeline);
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

                    cmd->writeBuffer(m->constantBuffer, &m->constant, sizeof(m->constant));
                    m->material->WriteBuffer(cmd);

                    // render
                    auto state = nvrhi::GraphicsState();
                    state.pipeline = m_GeometryAnimPipeline->GetHandle();
                    state.framebuffer = sceneFramebuffer;
                    state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(sceneFramebuffer->getFramebufferInfo().getViewport());

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
        m_Renderer2D->Begin(cmd, camera);
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

        m_Renderer2D->Flush();
        m_Renderer2D->End();

        UIPass(cmd);

        // m_EdgeDetectionParams.selectedCount = static_cast<uint32_t>(m_SelectedEntities.size());
        // if (!m_SelectedEntities.empty())
        // {
        //     cmd->writeBuffer(m_EdgeDetection->GetSelectedIDBuffer(), m_SelectedEntities.data(), m_SelectedEntities.size() * sizeof(uint32_t));
        // }
        // const uint32_t width = m_SceneRenderTarget->GetWidth();
        // const uint32_t height = m_SceneRenderTarget->GetHeight();
        // m_EdgeDetection->ExecuteCompute(cmd, m_EdgeDetectionParams, width, height);
        
        CompositePass(cmd);

        if (renderEnvironment)
        {
            m_Environment->End();
        }

        m_CommandList->Submit();
    }

    void SceneRenderer::CompositePass(nvrhi::ICommandList *cmd)
    {
        // Composite
        m_CompositeRenderTarget->ClearColorAttachmentFloat(cmd, 0);

        auto graphicsState = nvrhi::GraphicsState();
        graphicsState.pipeline = m_CompositePipeline->GetHandle();
        graphicsState.framebuffer = m_CompositeRenderTarget->GetFramebuffer();
        graphicsState.vertexBuffers = { nvrhi::VertexBufferBinding { m_CompositeVertexBuffer->GetHandle(), 0, 0 } };
        graphicsState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(m_CompositeRenderTarget->GetFramebuffer()->getFramebufferInfo().getViewport());
        graphicsState.bindings = { m_CompositeBindingSet };
        cmd->setGraphicsState(graphicsState);

        auto args = nvrhi::DrawArguments();
        args.instanceCount = 1;
        args.vertexCount = 6;
        cmd->draw(args);
    }

    void SceneRenderer::UIPass(nvrhi::ICommandList *cmd)
    {
        // UI Pass
        m_UIRenderer->Render(cmd);
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

        // Create demo text (placeholder for now)
        // auto titleText = uiManager.CreateText("UI Demo", glm::vec2(300, 50));
        // titleText->SetAlignment(UIAlignment::CENTER);
        // titleText->SetColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)); // yellow
    }

    void SceneRenderer::SetFillMode(nvrhi::RasterFillMode mode) const
    {
        m_Renderer2D->SetFillMode(mode);

        m_GeometryAnimPipeline->GetParams().fillMode = mode;
        m_GeometryAnimPipeline->CreatePipeline(m_SceneRenderTarget->GetFramebuffer());
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

    void SceneRenderer::CreateRenderTargets()
    {
        // Create scene render target
        RenderTargetCreateInfo createInfo = {};
        createInfo.attachments =
        {
            FramebufferAttachments{ nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite }, // Depth
            FramebufferAttachments{ nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget } // Main Color
        };

        m_SceneRenderTarget = RenderTarget::Create(createInfo);

        // Composite render target
        createInfo = {};
        createInfo.attachments = {FramebufferAttachments{ nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget } }; // Main Color
        m_CompositeRenderTarget = RenderTarget::Create(createInfo);
    }

    void SceneRenderer::CompositeUpdateBindingSet()
    {
        // Composite Binding set
        auto bindingSetDesc = nvrhi::BindingSetDesc();
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_SceneRenderTarget->GetColorAttachment(0)));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_UIRenderer->GetRenderTarget()->GetColorAttachment(0)));
        // bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, m_EdgeDetection->GetOutputTexture()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, Renderer::GetWhiteTexture()->GetSampler()));

        m_CompositeBindingSet = Application::GetGraphicsDevice()->createBindingSet(bindingSetDesc, m_CompositePipeline->GetBindingLayout(0));
    }

    void SceneRenderer::OnGuiRender()
    {
        ImGui::Begin("Debug");
       
        // if (m_EdgeDetection->GetOutputTexture())
        // {
        //     ImGui::DragFloat("Depth Sensitivity", &m_EdgeDetectionParams.depthSensitivity, 0.025f, 0.0f, FLT_MAX);
        //     ImGui::ColorEdit4("Color", &m_EdgeDetectionParams.outlineColor.x);
        // }

        // UI System Debug Controls
        if (ImGui::CollapsingHeader("UI System", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const ImVec2 vpSize = { static_cast<float>(m_Scene->viewportWidth), static_cast<float>(m_Scene->viewportHeight) };
            const ImVec2 &maxRegion = ImGui::GetContentRegionAvail();
            const float height = maxRegion.x / (vpSize.x / vpSize.y);

            ImTextureID texId = reinterpret_cast<ImTextureID>(m_UIRenderer->GetRenderTarget()->GetColorAttachment(0).Get());
            ImGui::Image(texId, {maxRegion.x, height});

            UIManager& uiManager = UIManager::GetInstance();
            UILayoutGrid& grid = uiManager.GetLayoutGrid();
            
            bool gridVisible = grid.IsVisible();
            if (ImGui::Checkbox("Show Layout Grid", &gridVisible))
            {
                grid.SetVisible(gridVisible);
            }
            
            if (gridVisible)
            {
                int gridSize = static_cast<int>(grid.GetGridSize());
                if (ImGui::SliderInt("Grid Size", &gridSize, 10, 100))
                {
                    grid.SetGridSize(static_cast<uint32_t>(gridSize));
                }
                
                glm::vec4 gridColor = grid.GetGridColor();
                if (ImGui::ColorEdit4("Grid Color", &gridColor.x))
                {
                    grid.SetGridColor(gridColor);
                }
                
                glm::vec4 majorGridColor = grid.GetMajorGridColor();
                if (ImGui::ColorEdit4("Major Grid Color", &majorGridColor.x))
                {
                    grid.SetMajorGridColor(majorGridColor);
                }
                
                int majorInterval = static_cast<int>(grid.GetMajorGridInterval());
                if (ImGui::SliderInt("Major Grid Interval", &majorInterval, 1, 10))
                {
                    grid.SetMajorGridInterval(static_cast<uint32_t>(majorInterval));
                }
            }
            
            // Mouse position info
            glm::vec2 mousePos = uiManager.GetMousePosition();
            ImGui::Text("UI Mouse Position: %.1f, %.1f", mousePos.x, mousePos.y);
            
            // Show if any button is hovered
            bool anyButtonHovered = false;
            for (const auto& widget : uiManager.GetWidgets())
            {
                if (auto button = widget->As<UIButton>())
                {
                    if (button->IsHovered())
                    {
                        anyButtonHovered = true;
                        ImGui::Text("Hovered Button: %s", button->GetText().c_str());
                        const Rect &rect = button->GetAlignedRect();
                        ImGui::Text("Button Bounds: (%.1f, %.1f) to (%.1f, %.1f)", rect.min.x, rect.min.y, rect.max.x, rect.max.y);
                        break;
                    }
                }
            }
            if (!anyButtonHovered)
            {
                ImGui::Text("No buttons hovered");
            }
            
            // Snap to grid example
            glm::vec2 snappedPos = grid.SnapToGrid(mousePos);
            ImGui::Text("Snapped Position: %.1f, %.1f", snappedPos.x, snappedPos.y);
            
            // Widget count
            ImGui::Text("Widget Count: %zu", uiManager.GetWidgets().size());
            
            // Add/Remove demo widgets
            static int buttonCount = 0;
            if (ImGui::Button("Add Demo Button"))
            {
                buttonCount++;
                
                auto newButton = uiManager.CreateButton(
                    "Button " + std::to_string(buttonCount), 
                    glm::vec2((buttonCount % 5) * 140, 200 + (buttonCount / 5) * 60), 
                    glm::vec2(120, 40)
                );
                
                // Random color
                float r = static_cast<float>(rand()) / RAND_MAX * 0.5f + 0.2f;
                float g = static_cast<float>(rand()) / RAND_MAX * 0.5f + 0.2f;
                float b = static_cast<float>(rand()) / RAND_MAX * 0.5f + 0.2f;
                
                newButton->SetColors(
                    glm::vec4(r, g, b, 1.0f),
                    glm::vec4(r + 0.2f, g + 0.2f, b + 0.2f, 1.0f),
                    glm::vec4(r - 0.1f, g - 0.1f, b - 0.1f, 1.0f)
                );
                
                newButton->SetOnClick([]() {
                    LOG_INFO("Button pressed");
                });
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Clear All Widgets"))
            {
                buttonCount = 0;
                uiManager.ClearWidgets();
                CreateDemoUI(); // Recreate default demo UI
            }
        }

        ImGui::End();
    }

    SceneRenderer* SceneRenderer::GetActive()
    {
        return s_SceneRenderer;
    }
}
