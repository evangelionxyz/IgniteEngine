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

#include "ignite/scene/scene.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"

#include "ignite/core/application.hpp"

#include <ranges>

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
            GraphicsPiplineCreateInfo pci;
            pci.attributes = attributes.data();
            pci.attributeCount = static_cast<uint32_t>(attributes.size());

            // Binding layout
            nvrhi::BindingLayoutDesc layoutDesc = {};
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // scene
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // edge detection
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0)); // sampler
            nvrhi::BindingLayoutHandle bindingLayout = device->createBindingLayout(layoutDesc);

            // Create pipeline
            m_CompositePipeline = GraphicsPipeline::Create(params, &pci);
            m_CompositePipeline->AddShader("composite.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("composite.pixel.hlsl", nvrhi::ShaderType::Pixel)
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
            GraphicsPiplineCreateInfo pci;
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
            GraphicsPiplineCreateInfo pci;
            pci.attributes = &attribute;
            pci.attributeCount = 1;

            m_EnvironmentPipeline = GraphicsPipeline::Create(params, &pci);
            m_EnvironmentPipeline->AddShader("skybox.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("skybox.pixel.hlsl", nvrhi::ShaderType::Pixel)
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::ENVIRONMENT))
                .Build();
        }

        m_Device = Application::GetGraphicsDevice();
        
        // Create Environment
        CreateEnvironment();
        CreateRenderTargets();
        CreatePipelines();

        const uint32_t width = m_SceneRenderTarget->GetWidth();
        const uint32_t height = m_SceneRenderTarget->GetHeight();

        // Create Edge Detection
        m_EdgeDetection = EdgeDetection::Create();

        m_EdgeDetection->CreatePipeline();
        m_EdgeDetection->CreateOutputTexture(width, height);

        m_EdgeDetection->UpdateBindingSet(m_SceneRenderTarget->GetColorAttachment(0),
            m_SceneRenderTarget->GetColorAttachment(1), m_SceneRenderTarget->GetDepthAttachment());

        // Composite Binding set
        CompositeUpdateBindingSet();

        m_SelectedEntities.reserve(100);
        m_EdgeDetectionParams.texelSize.x = 1.0f / static_cast<float>(width);
        m_EdgeDetectionParams.texelSize.y = 1.0f / static_cast<float>(height);
        m_EdgeDetectionParams.depthSensitivity = 1.0f;
        m_EdgeDetectionParams.selectedCount = static_cast<uint32_t>(m_SelectedEntities.size());
        m_EdgeDetectionParams.outlineColor = { 1.0f, 0.75f, 0.0f, 1.0f };
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

        m_Scene->Resize(width, height);

        m_EdgeDetection->CreateOutputTexture(width, height);
        m_EdgeDetection->UpdateBindingSet(m_SceneRenderTarget->GetColorAttachment(0),
            m_SceneRenderTarget->GetColorAttachment(1), m_SceneRenderTarget->GetDepthAttachment());

        CompositeUpdateBindingSet();

        m_EdgeDetectionParams.texelSize.x = 1.0f / static_cast<float>(width);
        m_EdgeDetectionParams.texelSize.y = 1.0f / static_cast<float>(height);
    }

    void SceneRenderer::CreatePipelines()
    {
        Renderer2D::CreatePipelines(m_SceneRenderTarget->GetFramebuffer());
        m_EnvironmentPipeline->CreatePipeline(m_SceneRenderTarget->GetFramebuffer());
        m_GeometryAnimPipeline->CreatePipeline(m_SceneRenderTarget->GetFramebuffer());

        // Composite
        m_CompositePipeline->CreatePipeline(m_CompositeRenderTarget->GetFramebuffer());
    }

    void SceneRenderer::Render(const ICamera *camera, bool renderEnvironment)
    {
        nvrhi::CommandListHandle commandList = Renderer::GetActiveCommandList();
        commandList->open();

        // Composite
        m_CompositeRenderTarget->ClearColorAttachmentFloat(commandList, 0);
        
        // Scene Render Target
        CameraConstants cameraBuffer = { camera->GetViewProjectionMatrix(), glm::vec4(camera->position, 1.0f) };
        commandList->writeBuffer(Renderer::GetCameraBufferHandle(), &cameraBuffer, sizeof(cameraBuffer));
        m_SceneRenderTarget->ClearColorAttachmentFloat(commandList, 0);
        m_SceneRenderTarget->ClearColorAttachmentUint(commandList, 1, static_cast<uint32_t>(-1));
        static const f32 farDepth = 1.0f; // LessOrEqual
        commandList->clearDepthStencilTexture(m_SceneRenderTarget->GetDepthAttachment(), nvrhi::AllSubresources,
            true, farDepth, true, 0); // depth & stencil

        nvrhi::IFramebuffer *sceneFramebuffer = m_SceneRenderTarget->GetFramebuffer();

        if (renderEnvironment)
        {
            if (m_Environment->isUpdatingTexture)
            {
                const auto &meshRendererView = m_Scene->registry->view<MeshRenderer>();
                for (entt::entity e : meshRendererView)
                {
                    const MeshRenderer &mr = meshRendererView.get<MeshRenderer>(e);
                    mr.material->UpdateBindingSet();
                }

                m_Environment->isUpdatingTexture = false;
            }

            m_Environment->Render(commandList, sceneFramebuffer, m_EnvironmentPipeline);
        }


        auto skeletalMeshview = m_Scene->registry->view<Transform, SkeletalMesh>();
        for (entt::entity e : skeletalMeshview)
        {
            Transform &tr = m_Scene->registry->get<Transform>(e);
            if (!tr.visible)
                continue;

            SkeletalMesh &sm = m_Scene->registry->get<SkeletalMesh>(e);
            Ref<MeshAsset> meshAsset = Project::GetActive()->GetAsset<MeshAsset>(sm.meshHandle);
            if (!meshAsset)
                continue;

            // render each mesh
            for (size_t i = 0; i < sm.meshes.size(); ++i)
            {
                SkeletalMesh::RenderMesh &m = sm.meshes[i];
                commandList->writeBuffer(m.constantBuffer, &m.constant, sizeof(m.constant));
                m.material->WriteBuffer(commandList);

                // render
                auto state = nvrhi::GraphicsState();
                state.pipeline = m_GeometryAnimPipeline->GetHandle();
                state.framebuffer = sceneFramebuffer;
                state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(sceneFramebuffer->getFramebufferInfo().getViewport());

                state.bindings = { m.bindingSet, m.material->bindingSet };

                state.addVertexBuffer({ m.mesh.GetVertexBuffer()->GetHandle(), 0, 0 });
                state.setIndexBuffer({ m.mesh.GetIndexBuffer()->GetHandle(), nvrhi::Format::R32_UINT });

                commandList->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.setVertexCount(static_cast<uint32_t>(m.mesh.data.indices.size()));
                args.instanceCount = 1;

                commandList->drawIndexed(args);
            }
        }

        // 2D Pass
        Renderer2D::Begin(commandList, sceneFramebuffer);
        auto object2DView = m_Scene->registry->view<Transform, Sprite2D>();
        for (entt::entity e : object2DView)
        {
            Transform &tr = m_Scene->registry->get<Transform>(e);
            if (!tr.visible)
                continue;

            Sprite2D &sprite = m_Scene->registry->get<Sprite2D>(e);
            Ref<Texture> texture = Project::GetAsset<Texture>(sprite.handle);
            Renderer2D::DrawQuad(tr.GetWorldMatrix(), sprite.color, texture, sprite.tilingFactor, static_cast<u32>(e));
        }

        Renderer2D::Flush();
        Renderer2D::End();

#if 0

        for (entt::entity e : m_Scene->entities | std::views::values)
        {
            Entity entity = { e, m_Scene };
            auto &tr = entity.GetTransform();

            if (!tr.visible)
                continue;

            if (entity.HasComponent<SkeletalMesh>())
            {
                SkeletalMesh &sm = entity.GetComponent<SkeletalMesh>();

                Ref<MeshAsset> meshAsset = Project::GetActive()->GetAsset<MeshAsset>(sm.meshHandle);
                if (!meshAsset)
                    continue;

                // render each mesh
                for (size_t i = 0; i < sm.meshes.size(); ++i)
                {
                    Mesh &mesh = sm.meshes[i];
                    commandList->writeBuffer(sm.meshesTransformBuffer[i], &sm.meshesTransform[i], sizeof(sm.meshesTransform[i]));

                    Material &mat = sm.materials[mesh.data.materialIndex];
                    mat.WriteBuffer(commandList);
                }
            }

            if (entity.HasComponent<MeshRenderer>())
            {
                MeshRenderer &meshRenderer = entity.GetComponent<MeshRenderer>();

                // not loaded mesh
                if (!meshRenderer.mesh || meshRenderer.mesh->data.meshIndex == -1)
                    continue;

                // write material constant buffer
                meshRenderer.material->WriteBuffer(commandList);
                commandList->writeBuffer(meshRenderer.transformBufferHandle, &meshRenderer.transformData, sizeof(meshRenderer.transformData));

                // render
                auto state = nvrhi::GraphicsState();
                state.pipeline = m_GeometryAnimPipeline->GetHandle();
                state.framebuffer = sceneFramebuffer;
                state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(sceneFramebuffer->getFramebufferInfo().getViewport());

                state.bindings = { meshRenderer.bindingSet, meshRenderer.material->bindingSet };

                state.addVertexBuffer({ meshRenderer.mesh->GetVertexBuffer()->GetHandle(), 0, 0 });
                state.setIndexBuffer({ meshRenderer.mesh->GetIndexBuffer()->GetHandle(), nvrhi::Format::R32_UINT });

                commandList->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.setVertexCount(static_cast<uint32_t>(meshRenderer.mesh->data.indices.size()));
                args.instanceCount = 1;

                commandList->drawIndexed(args);
            }
        }

#endif
        m_EdgeDetectionParams.selectedCount = static_cast<uint32_t>(m_SelectedEntities.size());
        if (!m_SelectedEntities.empty())
        {
            commandList->writeBuffer(m_EdgeDetection->GetSelectedIDBuffer(), m_SelectedEntities.data(), m_SelectedEntities.size() * sizeof(uint32_t));
        }

        const uint32_t width = m_SceneRenderTarget->GetWidth();
        const uint32_t height = m_SceneRenderTarget->GetHeight();

        m_EdgeDetection->ExecuteCompute(commandList, m_EdgeDetectionParams, width, height);

        // Composite render
        {
            auto graphicsState = nvrhi::GraphicsState();
            graphicsState.pipeline = m_CompositePipeline->GetHandle();
            graphicsState.framebuffer = m_CompositeRenderTarget->GetFramebuffer();
            graphicsState.vertexBuffers = { nvrhi::VertexBufferBinding { m_CompositeVertexBuffer->GetHandle(), 0, 0 } };
            graphicsState.viewport = nvrhi::ViewportState().addViewportAndScissorRect(m_CompositeRenderTarget->GetFramebuffer()->getFramebufferInfo().getViewport());
            graphicsState.bindings = { m_CompositeBindingSet };
            commandList->setGraphicsState(graphicsState);

            auto args = nvrhi::DrawArguments();
            args.instanceCount = 1;
            args.vertexCount = 6;
            commandList->draw(args);
        }

        commandList->close();
        m_Device->executeCommandList(commandList);
    }

    void SceneRenderer::SetFillMode(nvrhi::RasterFillMode mode) const
    {
        Renderer2D::SetFillMode(mode);
        Renderer2D::CreatePipelines(m_SceneRenderTarget->GetFramebuffer());

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
            m_SelectedEntities.push_back(entity);
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
            it = m_SelectedEntities.erase(it);
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

        nvrhi::CommandListHandle commandList = Renderer::GetActiveCommandList();

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
            FramebufferAttachments{ nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget }, // Main Color
            FramebufferAttachments{ nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget }, // Mouse picking
        };

        m_SceneRenderTarget = RenderTarget::Create(createInfo);

        // Composite render target
        createInfo = {};
        createInfo.attachments =
        {
            FramebufferAttachments{ nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget } // Main Color
        };

        m_CompositeRenderTarget = RenderTarget::Create(createInfo);
    }

    void SceneRenderer::CompositeUpdateBindingSet()
    {
        // Composite Binding set
        auto bindingSetDesc = nvrhi::BindingSetDesc();
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_SceneRenderTarget->GetColorAttachment(0)));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, m_EdgeDetection->GetOutputTexture()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, Renderer::GetWhiteTexture()->GetSampler()));
        m_CompositeBindingSet = Application::GetGraphicsDevice()->createBindingSet(bindingSetDesc, m_CompositePipeline->GetBindingLayout(0));
    }

    void SceneRenderer::OnGuiRender()
    {
        ImGui::Begin("Debug");
       
        if (m_EdgeDetection->GetOutputTexture())
        {
            ImGui::DragFloat("Depth Sensitivity", &m_EdgeDetectionParams.depthSensitivity, 0.025f, 0.0f, FLT_MAX);
            ImGui::ColorEdit4("Color", &m_EdgeDetectionParams.outlineColor.x);
        }

        ImGui::End();
    }

    SceneRenderer* SceneRenderer::GetActive()
    {
        return s_SceneRenderer;
    }
}
