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
    void SobelEdgeDetection::Initialize()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        
        auto bufferDesc = nvrhi::BufferDesc();
        bufferDesc.byteSize = sizeof(EdgeDetectionParams);
        bufferDesc.isConstantBuffer = true;
        bufferDesc.isVolatile = true;
        bufferDesc.debugName = "Edge detection constant buffer";
        bufferDesc.initialState = nvrhi::ResourceStates::ConstantBuffer;
        bufferDesc.keepInitialState = true;
        bufferDesc.maxVersions = 16;
        constantBuffer = device->createBuffer(bufferDesc);

        // Create linear sampler
        nvrhi::SamplerDesc samplerDesc;
        samplerDesc.minFilter = true;
        samplerDesc.magFilter = true;
        samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
        samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
        linearSampler = device->createSampler(samplerDesc);

        // Create binding layout
        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::All;
        layoutDesc.bindings = 
        {
            // constant buffer
            nvrhi::BindingLayoutItem::VolatileConstantBuffer(0),

            // Textures
            nvrhi::BindingLayoutItem::Texture_SRV(0), // Scene texture
            nvrhi::BindingLayoutItem::Texture_SRV(1), // Depth texture
            nvrhi::BindingLayoutItem::Texture_SRV(2), // Object ID texture

            // Sampler
            nvrhi::BindingLayoutItem::Sampler(0),

            // Output texture (for Compute)
            nvrhi::BindingLayoutItem::Texture_UAV(0)
        };

        bindingLayout = device->createBindingLayout(layoutDesc);

        CreateShaders();
    }

    void SobelEdgeDetection::CreateShaders()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        ShaderMake::ShaderContextDesc desc;

        Ref<ShaderMake::ShaderContext> computeContext = CreateRef<ShaderMake::ShaderContext>("sobel_edge_detection.compute.hlsl",
            ShaderMake::ShaderType::Compute, desc, false);

        Ref<ShaderMake::ShaderContext> vertexContext = CreateRef<ShaderMake::ShaderContext>("sobel_edge_detection.vertex.hlsl",
            ShaderMake::ShaderType::Vertex, desc, false);

        Ref<ShaderMake::ShaderContext> pixelContext = CreateRef<ShaderMake::ShaderContext>("sobel_edge_detection.pixel.hlsl",
            ShaderMake::ShaderType::Pixel, desc, false);

        Renderer::GetShaderLibrary().GetContext()->CompileShader({computeContext, vertexContext, pixelContext});

        computeShader = device->createShader(nvrhi::ShaderType::Compute, computeContext->blob.data.data(), computeContext->blob.dataSize());
        vertexShader = device->createShader(nvrhi::ShaderType::Vertex, vertexContext->blob.data.data(), vertexContext->blob.dataSize());
        pixelShader = device->createShader(nvrhi::ShaderType::Pixel, pixelContext->blob.data.data(), pixelContext->blob.dataSize());
    }

    void SobelEdgeDetection::CreateOutputTexture(const uint32_t width, const uint32_t height)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        
        nvrhi::TextureDesc desc;
        desc.width = width;
        desc.height = height;
        desc.format = nvrhi::Format::RGBA8_UNORM;
        desc.initialState = nvrhi::ResourceStates::UnorderedAccess | nvrhi::ResourceStates::ShaderResource;
        desc.isUAV = true;
        desc.debugName = "SobelDetection Output Texture";
        outputTexture = device->createTexture(desc);
    }

    void SobelEdgeDetection::CreatePipelines(nvrhi::IFramebuffer *framebuffer)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // Create compute pipeline
        nvrhi::ComputePipelineDesc computeDesc;
        computeDesc.CS = computeShader;
        computeDesc.bindingLayouts = { bindingLayout };
        computePipeline = device->createComputePipeline(computeDesc);

        // Create graphics pipeline for full-screen quad approach

        // position and uv
        constexpr float screenVertices[] =
        {
            0.0f, 0.0f, -1.0f, -1.0f,
            0.0f, 1.0f, -1.0f,  1.0f,
            1.0f, 1.0f,  1.0f,  1.0f,
            1.0f, 1.0f,  1.0f,  1.0f,
            0.0f, 1.0f, -1.0f,  1.0f,
            0.0f, 0.0f, -1.0f, -1.0f,
        };

        auto bufferDesc = nvrhi::BufferDesc();
        bufferDesc.byteSize = sizeof(screenVertices);
        bufferDesc.isVertexBuffer = true;
        bufferDesc.isVolatile = false;
        bufferDesc.debugName = "Edge detection vertex buffer";
        bufferDesc.initialState = nvrhi::ResourceStates::VertexBuffer;
        bufferDesc.keepInitialState = true;
        vertexBuffer = device->createBuffer(bufferDesc);

        std::array attributes = { nvrhi::VertexAttributeDesc()
            .setName("POSITION")
            .setFormat(nvrhi::Format::RG32_FLOAT)
            .setOffset(0)
            .setElementStride(sizeof(screenVertices)),
            nvrhi::VertexAttributeDesc()
            .setName("UV")
            .setFormat(nvrhi::Format::RG32_FLOAT)
            .setOffset(sizeof(float) * 2)
            .setElementStride(sizeof(screenVertices))
        };

        vertexInputLayout = device->createInputLayout(attributes.data(), attributes.size(), vertexShader);

        nvrhi::GraphicsPipelineDesc graphicsDesc;
        graphicsDesc.primType = nvrhi::PrimitiveType::TriangleList;
        graphicsDesc.inputLayout = vertexInputLayout;
        graphicsDesc.VS = vertexShader;
        graphicsDesc.PS = pixelShader;
        graphicsDesc.bindingLayouts = { bindingLayout };
        graphicsPipeline = device->createGraphicsPipeline(graphicsDesc, framebuffer);
    }

    void SobelEdgeDetection::UpdateBindingSet(const nvrhi::TextureHandle& inSceneTexture, const nvrhi::TextureHandle& inDepthTexture, const nvrhi::TextureHandle& inObjectIDTexture)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        
        nvrhi::BindingSetDesc desc;
        desc.bindings =
        {
            nvrhi::BindingSetItem::ConstantBuffer(0, this->constantBuffer),

            nvrhi::BindingSetItem::Texture_SRV(0, inSceneTexture),
            nvrhi::BindingSetItem::Texture_SRV(1, inDepthTexture),
            nvrhi::BindingSetItem::Texture_SRV(2, inObjectIDTexture),

            nvrhi::BindingSetItem::Sampler(0, this->linearSampler),

            nvrhi::BindingSetItem::Texture_UAV(0, outputTexture),
        };

        bindingSet = device->createBindingSet(desc, bindingLayout);
        LOG_ASSERT(bindingSet, "[Scene renderer] Failed to create binding set");
    }

    void SobelEdgeDetection::ExecuteCompute(nvrhi::ICommandList *commandList, const EdgeDetectionParams& params,
        const uint32_t width, const uint32_t height)
    {
        // Update constant buffer
        commandList->writeBuffer(constantBuffer, &params, sizeof(params));

        // Set compute pipeline
        nvrhi::ComputeState computeState;
        computeState.pipeline = computePipeline;
        computeState.bindings = { bindingSet };
        commandList->setComputeState(computeState);

        // Dispatch compute shader
        uint32_t groupsX = (width + 7) / 8; // 8x8 threads groups
        uint32_t groupsY = (height + 7) / 8;
        commandList->dispatch(groupsX, groupsY, 1);
    }

    void SobelEdgeDetection::ExecuteFullScreenQuad(nvrhi::ICommandList* commandList, const EdgeDetectionParams& params,
        nvrhi::IFramebuffer* framebuffer)
    {
        commandList->writeBuffer(constantBuffer, &params, sizeof(params));

        // set graphics pipeline
        nvrhi::GraphicsState graphicsState;
        graphicsState.pipeline = graphicsPipeline;
        graphicsState.bindings = { bindingSet };
        graphicsState.framebuffer = framebuffer;

        // configure viewport
        nvrhi::Viewport viewport;

        viewport.minX = 0; viewport.maxX = static_cast<float>(framebuffer->getFramebufferInfo().width);
        viewport.minY = 0; viewport.maxY = static_cast<float>(framebuffer->getFramebufferInfo().height);
        viewport.minZ = 0; viewport.maxZ = 1;

        graphicsState.viewport.addViewportAndScissorRect(viewport);

        commandList->setGraphicsState(graphicsState);
        commandList->draw(nvrhi::DrawArguments{3, 1, 0, 0});
    }

    void Composite::Initialize()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // Create linear sampler
        nvrhi::SamplerDesc samplerDesc;
        samplerDesc.minFilter = true;
        samplerDesc.magFilter = true;
        samplerDesc.addressU = nvrhi::SamplerAddressMode::Clamp;
        samplerDesc.addressV = nvrhi::SamplerAddressMode::Clamp;
        linearSampler = device->createSampler(samplerDesc);
        LOG_ASSERT(linearSampler, "[Composite] Failed to create linear sampler");

        // Create binding layout
        nvrhi::BindingLayoutDesc layoutDesc;
        layoutDesc.visibility = nvrhi::ShaderType::Pixel;
        layoutDesc.bindings =
        {
            nvrhi::BindingLayoutItem::Texture_SRV(0), // Scene texture
            nvrhi::BindingLayoutItem::Sampler(0)
        };

        bindingLayout = device->createBindingLayout(layoutDesc);
        LOG_ASSERT(bindingLayout, "[Composite] Failed to create binding layout");

        CreateShaders();

    }

    void Composite::CreateShaders()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        ShaderMake::ShaderContextDesc desc;

        Ref<ShaderMake::ShaderContext> vertexContext = CreateRef<ShaderMake::ShaderContext>("sobel_edge_detection.vertex.hlsl",
            ShaderMake::ShaderType::Vertex, desc, false);

        Ref<ShaderMake::ShaderContext> pixelContext = CreateRef<ShaderMake::ShaderContext>("composite.pixel.hlsl",
            ShaderMake::ShaderType::Pixel, desc, false);

        Renderer::GetShaderLibrary().GetContext()->CompileShader({ vertexContext, pixelContext });

        vertexShader = device->createShader(nvrhi::ShaderType::Vertex, vertexContext->blob.data.data(), vertexContext->blob.dataSize());
        pixelShader = device->createShader(nvrhi::ShaderType::Pixel, pixelContext->blob.data.data(), pixelContext->blob.dataSize());
    }

    void Composite::CreatePipelines(nvrhi::IFramebuffer* framebuffer)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        constexpr float screenVertices[] = {
            // pos            // uv
            -1.0f, -1.0f,   0.0f, 1.0f,
             1.0f, -1.0f,   1.0f, 1.0f,
            -1.0f,  1.0f,   0.0f, 0.0f,
            -1.0f,  1.0f,   0.0f, 0.0f,
             1.0f, -1.0f,   1.0f, 1.0f,
             1.0f,  1.0f,   1.0f, 0.0f
        };

        auto bufferDesc = nvrhi::BufferDesc()
            .setByteSize(sizeof(screenVertices))
            .setIsVertexBuffer(true)
            .setInitialState(nvrhi::ResourceStates::VertexBuffer)
            .setKeepInitialState(true)
            .setDebugName("Composite vertex buffer");

        vertexBuffer = device->createBuffer(bufferDesc);
        LOG_ASSERT(vertexBuffer, "[Composite] Failed to create vertex buffer");

        nvrhi::CommandListHandle commandList = device->createCommandList();
        commandList->open();
        commandList->writeBuffer(vertexBuffer, screenVertices, sizeof(screenVertices));
        commandList->close();
        device->executeCommandList(commandList);

        std::array attributes = {
            nvrhi::VertexAttributeDesc()
            .setName("POSITION")
            .setFormat(nvrhi::Format::RG32_FLOAT)
            .setOffset(0)
            .setElementStride(sizeof(float) * 4),
            nvrhi::VertexAttributeDesc()
            .setName("TEXCOORD0")
            .setFormat(nvrhi::Format::RG32_FLOAT)
            .setOffset(sizeof(float) * 2)
            .setElementStride(sizeof(float) * 4)
        };

        vertexInputLayout = device->createInputLayout(attributes.data(), static_cast<uint32_t>(attributes.size()), vertexShader);

        nvrhi::GraphicsPipelineDesc graphicsDesc;
        graphicsDesc.primType = nvrhi::PrimitiveType::TriangleList;
        graphicsDesc.inputLayout = vertexInputLayout;
        graphicsDesc.VS = vertexShader;
        graphicsDesc.PS = pixelShader;
        graphicsDesc.bindingLayouts = { bindingLayout };
        graphicsDesc.renderState.depthStencilState.depthTestEnable = false;
        graphicsDesc.renderState.depthStencilState.depthWriteEnable = false;

        graphicsPipeline = device->createGraphicsPipeline(graphicsDesc, framebuffer);
    }

    void Composite::UpdateBindingSet(const nvrhi::TextureHandle &texture)
    {
        if (!texture)
            return;

        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        nvrhi::BindingSetDesc desc;
        desc.bindings =
        {
            nvrhi::BindingSetItem::Texture_SRV(0, texture),
            nvrhi::BindingSetItem::Sampler(0, this->linearSampler)
        };

        if (bindingSet)
            bindingSet.Reset();

        bindingSet = device->createBindingSet(desc, bindingLayout);
        LOG_ASSERT(bindingSet, "[Composite] Failed to create binding set");
    }

    void Composite::Execute(nvrhi::ICommandList* commandList, nvrhi::IFramebuffer* framebuffer)
    {
        if (!bindingSet)
            return;

        nvrhi::GraphicsState graphicsState;
        graphicsState.pipeline = graphicsPipeline;
        graphicsState.bindings = { bindingSet };
        graphicsState.framebuffer = framebuffer;
        graphicsState.viewport.addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
        graphicsState.vertexBuffers.push_back({ vertexBuffer, 0, 0 });

        commandList->setGraphicsState(graphicsState);
        commandList->draw(nvrhi::DrawArguments{ 6, 1, 0, 0 });
    }

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

        m_Device = Application::GetGraphicsDevice();
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

        CreatePipelines();

        m_EdgeDetection.Initialize();
        m_EdgeDetection.CreatePipelines(m_RenderTarget->GetFramebuffer());
        m_EdgeDetection.CreateOutputTexture(m_RenderTarget->GetWidth(), m_RenderTarget->GetHeight());
        m_EdgeDetection.UpdateBindingSet(
            m_RenderTarget->GetColorAttachment(0), 
            m_RenderTarget->GetDepthAttachment(),
            m_RenderTarget->GetColorAttachment(1));

        m_Composite.Initialize();
        m_Composite.CreatePipelines(m_RenderTarget->GetFramebuffer());
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

    void SceneRenderer::Resize(uint32_t width, uint32_t height)
    {
        m_RenderTarget->Resize(width, height);
        m_Scene->Resize(width, height);

        m_EdgeDetection.CreateOutputTexture(width, height);
        m_EdgeDetection.UpdateBindingSet(
            m_RenderTarget->GetColorAttachment(0),
            m_RenderTarget->GetDepthAttachment(),
            m_RenderTarget->GetColorAttachment(1));
    }

    void SceneRenderer::CreatePipelines() const
    {
        m_BatchQuadPipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());
        m_BatchLinePipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());
        m_EnvironmentPipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());
        m_GeometryPipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());
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
                const auto &meshRendererView = m_Scene->registry->view<MeshRenderer>();
                for (entt::entity e : meshRendererView)
                {
                    const MeshRenderer &mr = meshRendererView.get<MeshRenderer>(e);
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

        const uint32_t width = m_RenderTarget->GetWidth();
        const uint32_t height = m_RenderTarget->GetHeight();

        EdgeDetectionParams edParams;
        edParams.texelSize.x = 1.0f / static_cast<float>(width);
        edParams.texelSize.y = 1.0f / static_cast<float>(height);
        edParams.edgeThreshold = 0.1f;
        edParams.outlineWidth = 3.0f;
        edParams.outlineColor = { 1.0f, 1.0f, 0.0f, 1.0f };
        m_EdgeDetection.ExecuteCompute(m_CommandList, edParams, width, height);

        // m_Composite.UpdateBindingSet(m_EdgeDetection.outputTexture);
        // m_Composite.Execute(m_CommandList, framebuffer);

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

    void SceneRenderer::OnGuiRender()
    {
        ImGui::Begin("Debug");
       
        if (m_EdgeDetection.outputTexture)
        {
            ImTextureID tex = reinterpret_cast<ImTextureID>(m_EdgeDetection.outputTexture.Get());
            float width = ImGui::GetContentRegionAvail().x;
            float height = width / (16.0f / 9.0f);

            ImGui::Image(tex, { width, height});
        }

        ImGui::End();
    }
}
