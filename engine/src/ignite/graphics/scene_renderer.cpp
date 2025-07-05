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
        m_CommandList = m_Device->createCommandList();
        
        // Create Environment
        CreateEnvironment();

        // Create render target
        RenderTargetCreateInfo createInfo = {};
        createInfo.attachments = 
        {
            FramebufferAttachments{ nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite }, // Depth
            FramebufferAttachments{ nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget }, // Main Color
            FramebufferAttachments{ nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget }, // Mouse picking
        };

        m_RenderTarget = RenderTarget::Create(createInfo);

        CreatePipelines();

        const uint32_t width = m_RenderTarget->GetWidth();
        const uint32_t height = m_RenderTarget->GetHeight();

        // Create Edge Detection
        m_EdgeDetection = EdgeDetection::Create();

        m_EdgeDetection->CreatePipeline();
        m_EdgeDetection->CreateOutputTexture(width, height);

        m_EdgeDetection->UpdateBindingSet(
            m_RenderTarget->GetColorAttachment(0),
            m_RenderTarget->GetColorAttachment(1),
            m_RenderTarget->GetDepthAttachment());

        m_SelectedEntities.reserve(100);

        m_EdgeDetectionParams.texelSize.x = 1.0f / static_cast<float>(width);
        m_EdgeDetectionParams.texelSize.y = 1.0f / static_cast<float>(height);

        m_EdgeDetectionParams.edgeThreshold = 0.001f;
        m_EdgeDetectionParams.outlineWidth = 1.0f;
        m_EdgeDetectionParams.depthSensitivity = 1.0f;
        m_EdgeDetectionParams.useObjectID = 1;
        m_EdgeDetectionParams.selectedCount = static_cast<uint32_t>(m_SelectedEntities.size());
        m_EdgeDetectionParams.outlineColor = { 1.0f, 0.75f, 0.0f, 1.0f };
    }

    void SceneRenderer::SetActiveScene(Scene* scene)
    {
        m_Scene = scene;
    }

    bool SceneRenderer::ShouldResize() const
    {
        return m_RenderTarget->GetWidth() != m_Scene->viewportWidth || m_RenderTarget->GetHeight() != m_Scene->viewportHeight;
    }

    void SceneRenderer::Resize(uint32_t width, uint32_t height)
    {
        m_RenderTarget->Resize(width, height);
        m_Scene->Resize(width, height);

        m_EdgeDetection->CreateOutputTexture(width, height);
        m_EdgeDetection->UpdateBindingSet(
            m_RenderTarget->GetColorAttachment(0),
            m_RenderTarget->GetColorAttachment(1),
            m_RenderTarget->GetDepthAttachment());

        m_EdgeDetectionParams.texelSize.x = 1.0f / static_cast<float>(width);
        m_EdgeDetectionParams.texelSize.y = 1.0f / static_cast<float>(height);
    }

    void SceneRenderer::CreatePipelines() const
    {
        Renderer2D::CreatePipelines(m_RenderTarget->GetFramebuffer());
        m_EnvironmentPipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());
        m_GeometryAnimPipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());
    }

    void SceneRenderer::Render(const ICamera *camera, bool renderEnvironment)
    {
        m_CommandList->open();
        
        CameraConstants cameraBuffer = { camera->GetViewProjectionMatrix(), glm::vec4(camera->position, 1.0f) };
        m_CommandList->writeBuffer(Renderer::GetCameraBufferHandle(), &cameraBuffer, sizeof(cameraBuffer));

        m_RenderTarget->ClearColorAttachmentFloat(m_CommandList, 0);
        m_RenderTarget->ClearColorAttachmentUint(m_CommandList, 1, static_cast<uint32_t>(-1));

        f32 farDepth = 1.0f; // LessOrEqual
        m_CommandList->clearDepthStencilTexture(m_RenderTarget->GetDepthAttachment(), 
            nvrhi::AllSubresources, 
            true, // clear depth ?
            farDepth, // depth
            true, // clear stencil?
            0 // stencil
        );

        nvrhi::IFramebuffer *framebuffer = m_RenderTarget->GetFramebuffer();
        
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

            m_Environment->Render(m_CommandList, framebuffer, m_EnvironmentPipeline);
        }
        
        Renderer2D::Begin(m_CommandList, framebuffer);

        for (entt::entity e : m_Scene->entities | std::views::values)
        {
            Entity entity = { e, m_Scene };
            auto &tr = entity.GetTransform();

            if (!tr.visible)
                continue;

            if (entity.HasComponent<MeshRenderer>())
            {
                MeshRenderer &meshRenderer = entity.GetComponent<MeshRenderer>();
                
                // not loaded mesh
                if (meshRenderer.mesh->data.meshIndex == -1)
                    continue;

                // write material constant buffer
                meshRenderer.material->WriteBuffer(m_CommandList);
                meshRenderer.WriteTransformBuffer(m_CommandList);

                // render
                auto state = nvrhi::GraphicsState();
                state.pipeline = m_GeometryAnimPipeline->GetHandle();
                state.framebuffer = framebuffer;
                state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

                state.bindings = { meshRenderer.bindingSet, meshRenderer.material->bindingSet };

                state.addVertexBuffer({ meshRenderer.mesh->GetVertexBuffer()->GetHandle(), 0, 0});
                state.setIndexBuffer({ meshRenderer.mesh->GetIndexBuffer()->GetHandle(), nvrhi::Format::R32_UINT});

                m_CommandList->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.setVertexCount(static_cast<uint32_t>(meshRenderer.mesh->data.indices.size()));
                args.instanceCount = 1;

                m_CommandList->drawIndexed(args);
            }

            if (entity.HasComponent<Sprite2D>())
            {
                Sprite2D &sprite = entity.GetComponent<Sprite2D>();
                Ref<Texture> texture = Project::GetAsset<Texture>(sprite.handle);
                Renderer2D::DrawQuad(tr.GetWorldMatrix(), sprite.color, texture, sprite.tilingFactor, static_cast<u32>(e));
            }
        }

        Renderer2D::Flush();
        Renderer2D::End();

        m_EdgeDetectionParams.useObjectID = 1;
        m_EdgeDetectionParams.selectedCount = static_cast<uint32_t>(m_SelectedEntities.size());

        if (!m_SelectedEntities.empty())
        {
            m_CommandList->writeBuffer(m_EdgeDetection->GetSelectedIDBuffer(), m_SelectedEntities.data(), m_SelectedEntities.size() * sizeof(uint32_t));
        }

        const uint32_t width = m_RenderTarget->GetWidth();
        const uint32_t height = m_RenderTarget->GetHeight();
        m_EdgeDetection->ExecuteCompute(m_CommandList, m_EdgeDetectionParams, width, height);

        m_CommandList->close();
        m_Device->executeCommandList(m_CommandList);
    }

    void SceneRenderer::SetFillMode(nvrhi::RasterFillMode mode) const
    {
        Renderer2D::SetFillMode(mode);
        Renderer2D::CreatePipelines(m_RenderTarget->GetFramebuffer());

        m_GeometryAnimPipeline->GetParams().fillMode = mode;
        m_GeometryAnimPipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());
    }

    void SceneRenderer::SetSelectedEntity(const Entity& entity)
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

    void SceneRenderer::UnselectEntity(const Entity& entity)
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

        m_CommandList->open();
        m_Environment->WriteBuffer(m_CommandList);
        m_CommandList->close();
        m_Device->executeCommandList(m_CommandList);

        m_Environment->SetSunDirection(50.0f, -27.0f);
    }

    void SceneRenderer::OnGuiRender()
    {
        ImGui::Begin("Debug");
       
        if (m_EdgeDetection->GetOutputTexture())
        {
            // ImTextureID tex = reinterpret_cast<ImTextureID>(m_EdgeDetection.outputTexture.Get());
            // float width = ImGui::GetContentRegionAvail().x;
            // float height = width / (16.0f / 9.0f);
            // 
            // ImGui::Image(tex, { width, height});

            ImGui::DragFloat("Edge Threshold", &m_EdgeDetectionParams.edgeThreshold, 0.025f, 0.001f, 100.0f);
            ImGui::DragFloat("Outline Width", &m_EdgeDetectionParams.outlineWidth, 0.025f, 0.0f, FLT_MAX);
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
