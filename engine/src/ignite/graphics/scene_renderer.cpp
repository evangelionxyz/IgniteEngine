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

namespace ignite
{
    SceneRenderer::SceneRenderer()
    {
    }

    SceneRenderer::~SceneRenderer()
    {
    }

    void SceneRenderer::Create()
    {
        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.depthWrite = true;
        params.depthTest = true;
        params.enableDepthStencil = false;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::Front;

        // Mesh pipeline
        {
            auto attributes = VertexMesh::GetAttributes();
            GraphicsPiplineCreateInfo pci;
            pci.attributes = attributes.data();
            pci.attributeCount = static_cast<uint32_t>(attributes.size());

            m_GeometryPipeline = GraphicsPipeline::Create(params, &pci, Renderer::GetBindingLayout(GPipeline::MESH));
            m_GeometryPipeline->AddShader("default_mesh.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("default_mesh.pixel.hlsl", nvrhi::ShaderType::Pixel)
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

            m_EnvironmentPipeline = GraphicsPipeline::Create(params, &pci, Renderer::GetBindingLayout(GPipeline::ENVIRONMENT));
            m_EnvironmentPipeline->AddShader("skybox.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("skybox.pixel.hlsl", nvrhi::ShaderType::Pixel)
                .Build();
        }

        // Batch Quad 2D
        {
            params.fillMode = nvrhi::RasterFillMode::Solid;
            params.cullMode = nvrhi::RasterCullMode::None;
            params.comparison = nvrhi::ComparisonFunc::LessOrEqual;

            auto attributes = Vertex2DQuad::GetAttributes();
            GraphicsPiplineCreateInfo pci;
            pci.attributes = attributes.data();
            pci.attributeCount = static_cast<uint32_t>(attributes.size());

            auto shaderContext = Renderer::GetShaderLibrary().Get("batch_2d_quad");

            m_BatchQuadPipeline = GraphicsPipeline::Create(params, &pci, Renderer::GetBindingLayout(GPipeline::QUAD2D));
            m_BatchQuadPipeline->AddShader(shaderContext[nvrhi::ShaderType::Vertex].handle, nvrhi::ShaderType::Vertex)
                .AddShader(shaderContext[nvrhi::ShaderType::Pixel].handle, nvrhi::ShaderType::Pixel)
                .Build();
        }

        // Batch Line 2D Pipeline
        {
            params.fillMode = nvrhi::RasterFillMode::Wireframe;
            params.cullMode = nvrhi::RasterCullMode::None;
            params.primitiveType = nvrhi::PrimitiveType::LineList;

            auto attributes = Vertex2DLine::GetAttributes();
            GraphicsPiplineCreateInfo pci;
            pci.attributes = attributes.data();
            pci.attributeCount = static_cast<uint32_t>(attributes.size());

            m_BatchLinePipeline = GraphicsPipeline::Create(params, &pci, Renderer::GetBindingLayout(GPipeline::LINE));

            auto shaderContext = Renderer::GetShaderLibrary().Get("batch_2d_line");

            m_BatchLinePipeline->AddShader(shaderContext[nvrhi::ShaderType::Vertex].handle, nvrhi::ShaderType::Vertex)
                .AddShader(shaderContext[nvrhi::ShaderType::Pixel].handle, nvrhi::ShaderType::Pixel)
                .Build();
        }

        m_Device = Application::GetRenderDevice();
        m_CommandList = m_Device->createCommandList();
        
        // Create Environment
        CreateEnvironment();

        // Create render target
        RenderTargetCreateInfo createInfo = {};
        createInfo.attachments = 
        {
            FramebufferAttachments{ nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite }, // Depth
            FramebufferAttachments{ nvrhi::Format::SRGBA8_UNORM, nvrhi::ResourceStates::RenderTarget }, // Main Color
            FramebufferAttachments{ nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget }, // Mouse picking
        };

        m_RenderTarget = RenderTarget::Create(createInfo);
        CreatePipelines(m_RenderTarget->GetFramebuffer());
    }

    void SceneRenderer::SetActiveScene(Scene* scene)
    {
        m_Scene = scene;
        m_Scene->sceneRenderer = this;
    }

    bool SceneRenderer::ShouldResize() const
    {
        return m_RenderTarget->GetWidth() != m_Scene->viewportWidth || m_RenderTarget->GetHeight() != m_Scene->viewportHeight;
    }

    void SceneRenderer::Resize(uint32_t width, uint32_t height) const
    {
        m_RenderTarget->Resize(width, height);
        m_Scene->Resize(width, height);
    }

    void SceneRenderer::CreatePipelines(nvrhi::IFramebuffer *framebuffer) const
    {
        m_BatchQuadPipeline->CreatePipeline(framebuffer);
        m_BatchLinePipeline->CreatePipeline(framebuffer);
        m_EnvironmentPipeline->CreatePipeline(framebuffer);
        m_GeometryPipeline->CreatePipeline(framebuffer);
    }

    void SceneRenderer::Render(const ICamera *camera, bool renderEnvironment)
    {
        m_CommandList->open();
        
        CameraBuffer cameraBuffer = { camera->GetViewProjectionMatrix(), glm::vec4(camera->position, 1.0f) };
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
                auto meshRendererView = m_Scene->registry->view<MeshRenderer>();
                for (entt::entity e : meshRendererView)
                {
                    MeshRenderer &mr = meshRendererView.get<MeshRenderer>(e);
                    mr.mesh->CreateBindingSet();
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
                if (meshRenderer.meshIndex == -1)
                    continue;

                // write material constant buffer
                m_CommandList->writeBuffer(meshRenderer.mesh->materialBufferHandle, &meshRenderer.mesh->material.data, sizeof(meshRenderer.mesh->material.data));
                m_CommandList->writeBuffer(meshRenderer.mesh->objectBufferHandle, &meshRenderer.meshBuffer, sizeof(meshRenderer.meshBuffer));

                // render
                auto state = nvrhi::GraphicsState();
                state.pipeline = m_GeometryPipeline->GetHandle();
                state.framebuffer = framebuffer;
                state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
                state.addBindingSet(meshRenderer.mesh->bindingSets[GPipeline::MESH]);
                state.setIndexBuffer({ meshRenderer.mesh->indexBuffer, nvrhi::Format::R32_UINT });
                state.addVertexBuffer({ meshRenderer.mesh->vertexBuffer, 0, 0 });

                m_CommandList->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.setVertexCount(static_cast<uint32_t>(meshRenderer.mesh->data.indices.size()));
                args.instanceCount = 1;

                m_CommandList->drawIndexed(args);
            }

            if (entity.HasComponent<Sprite2D>())
            {
                auto &sprite = entity.GetComponent<Sprite2D>();
                Renderer2D::DrawQuad(tr.GetWorldMatrix(), sprite.color, sprite.texture, sprite.tilingFactor, static_cast<u32>(e));
            }
        }

        Renderer2D::Flush(m_BatchQuadPipeline, m_BatchLinePipeline);
        Renderer2D::End();

        m_CommandList->close();
        m_Device->executeCommandList(m_CommandList);
    }

    void SceneRenderer::SetFillMode(nvrhi::RasterFillMode mode) const
    {
        m_BatchQuadPipeline->GetParams().fillMode = mode;
        m_BatchQuadPipeline->ResetHandle();

        m_GeometryPipeline->GetParams().fillMode = mode;
        m_GeometryPipeline->ResetHandle();
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

}