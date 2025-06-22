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

namespace ignite
{
    static SceneRenderer *s_SceneRenderer = nullptr;

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

        bufferDesc = nvrhi::BufferDesc();
        bufferDesc.byteSize = sizeof(uint32_t) * 100; // allocate enough memory for selection
        bufferDesc.structStride = sizeof(uint32_t);
        bufferDesc.cpuAccess = nvrhi::CpuAccessMode::None;
        bufferDesc.debugName = "Selected Object IDs";
        bufferDesc.keepInitialState = true;
        bufferDesc.initialState = nvrhi::ResourceStates::ShaderResource;

        selectedIDBuffer = device->createBuffer(bufferDesc);

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
            nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3), // Object ID texture

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

        Renderer::GetShaderLibrary().GetContext()->CompileShader({ computeContext });
        computeShader = device->createShader(nvrhi::ShaderType::Compute, computeContext->blob.data.data(), computeContext->blob.dataSize());
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

    void SobelEdgeDetection::CreatePipeline()
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // Create compute pipeline
        nvrhi::ComputePipelineDesc computeDesc;
        computeDesc.CS = computeShader;
        computeDesc.bindingLayouts = { bindingLayout };
        computePipeline = device->createComputePipeline(computeDesc);
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
            nvrhi::BindingSetItem::StructuredBuffer_SRV(3, this->selectedIDBuffer),
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

    SceneRenderer::SceneRenderer()
    {
        s_SceneRenderer = this;
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
        params.cullMode = nvrhi::RasterCullMode::None;

        // Geometry pipeline
        {
            auto attributes = VertexMesh::GetAttributes();
            GraphicsPiplineCreateInfo pci;
            pci.attributes = attributes.data();
            pci.attributeCount = static_cast<uint32_t>(attributes.size());

            m_GeometryPipeline = GraphicsPipeline::Create(params, &pci);
            m_GeometryPipeline->AddShader("default_mesh.vertex.hlsl", nvrhi::ShaderType::Vertex)
                .AddShader("default_mesh.pixel.hlsl", nvrhi::ShaderType::Pixel, "main", true)
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MESH))
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
            m_BatchQuadPipeline = GraphicsPipeline::Create(params, &pci);
            m_BatchQuadPipeline->AddShader(shaderContext[nvrhi::ShaderType::Vertex].handle, nvrhi::ShaderType::Vertex)
                .AddShader(shaderContext[nvrhi::ShaderType::Pixel].handle, nvrhi::ShaderType::Pixel)
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::QUAD2D))
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

            m_BatchLinePipeline = GraphicsPipeline::Create(params, &pci);

            auto shaderContext = Renderer::GetShaderLibrary().Get("batch_2d_line");

            m_BatchLinePipeline->AddShader(shaderContext[nvrhi::ShaderType::Vertex].handle, nvrhi::ShaderType::Vertex)
                .AddShader(shaderContext[nvrhi::ShaderType::Pixel].handle, nvrhi::ShaderType::Pixel)
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::LINE))
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

        const uint32_t width = m_RenderTarget->GetWidth();
        const uint32_t height = m_RenderTarget->GetHeight();

        m_EdgeDetection.Initialize();
        m_EdgeDetection.CreatePipeline();
        m_EdgeDetection.CreateOutputTexture(width, height);
        m_EdgeDetection.UpdateBindingSet(
            m_RenderTarget->GetColorAttachment(0), 
            m_RenderTarget->GetDepthAttachment(),
            m_RenderTarget->GetColorAttachment(1));

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

        m_EdgeDetectionParams.texelSize.x = 1.0f / static_cast<float>(width);
        m_EdgeDetectionParams.texelSize.y = 1.0f / static_cast<float>(height);
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
        
        CameraConstants cameraBuffer = { camera->GetViewProjectionMatrix(), glm::vec4(camera->position, 1.0f) };
        m_CommandList->writeBuffer(Renderer::GetCameraBufferHandle(), &cameraBuffer, sizeof(cameraBuffer));

        m_RenderTarget->ClearColorAttachmentFloat(m_CommandList, 0);
        m_RenderTarget->ClearColorAttachmentUint(m_CommandList, 1, 
            static_cast<uint32_t>(-1));

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
                if (meshRenderer.meshIndex == -1)
                    continue;

                // write material constant buffer
                meshRenderer.material->WriteBuffer(m_CommandList);
                meshRenderer.WriteTransformBuffer(m_CommandList);

                // render
                auto state = nvrhi::GraphicsState();
                state.pipeline = m_GeometryPipeline->GetHandle();
                state.framebuffer = framebuffer;
                state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

                state.bindings = { meshRenderer.bindingSet, meshRenderer.material->bindingSet };

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

        m_EdgeDetectionParams.useObjectID = 1;
        m_EdgeDetectionParams.selectedCount = static_cast<uint32_t>(m_SelectedEntities.size());

        if (!m_SelectedEntities.empty())
        {
            m_CommandList->writeBuffer(m_EdgeDetection.selectedIDBuffer, m_SelectedEntities.data(), m_SelectedEntities.size() * sizeof(uint32_t));
        }

        const uint32_t width = m_RenderTarget->GetWidth();
        const uint32_t height = m_RenderTarget->GetHeight();
        m_EdgeDetection.ExecuteCompute(m_CommandList, m_EdgeDetectionParams, width, height);

        m_CommandList->close();
        m_Device->executeCommandList(m_CommandList);
    }

    void SceneRenderer::SetFillMode(nvrhi::RasterFillMode mode) const
    {
        m_BatchQuadPipeline->GetParams().fillMode = mode;
        m_BatchQuadPipeline->ResetHandle();
        m_BatchQuadPipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());

        m_GeometryPipeline->GetParams().fillMode = mode;
        m_GeometryPipeline->ResetHandle();
        m_GeometryPipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());
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
       
        if (m_EdgeDetection.outputTexture)
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
