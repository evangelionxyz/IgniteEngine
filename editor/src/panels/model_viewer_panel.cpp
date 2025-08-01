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

#include "model_viewer_panel.hpp"
#include "ignite/core/platform_utils.hpp"
#include "ignite/graphics/scene_renderer.hpp"

namespace ignite
{
    ModelViewerPanel::ModelViewerPanel()
        : IPanel("Model Viewer")
    {
        // Create scene render target
        RenderTargetCreateInfo createInfo = {};
        createInfo.attachments =
        {
            FramebufferAttachments{ nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite }, // Depth
            FramebufferAttachments{ nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget }, // Main Color
            FramebufferAttachments{ nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget }, // Mouse picking
        };

        m_RenderTarget = RenderTarget::Create(createInfo);

        m_RenderTarget->CreateFramebuffer();

        // Create graphics pipeline
        GraphicsPipelineCreateInfo pipelineInfo;
        auto attributes = VertexMesh_Anim::GetAttributes();
        pipelineInfo.attributes = attributes.data();
        pipelineInfo.attributeCount = static_cast<uint32_t>(attributes.size());

        GraphicsPipelineParams pipelineParams;
        pipelineParams.enableBlend = true;
        pipelineParams.depthWrite = true;
        pipelineParams.depthTest = true;
        pipelineParams.enableDepthStencil = false;
        pipelineParams.fillMode = nvrhi::RasterFillMode::Solid;
        pipelineParams.cullMode = nvrhi::RasterCullMode::None;

        m_Pipeline = GraphicsPipeline::Create(pipelineParams, &pipelineInfo);
        m_Pipeline->AddShader("mesh_anim.vertex.hlsl", nvrhi::ShaderType::Vertex)
            .AddShader("mesh_anim.pixel.hlsl", nvrhi::ShaderType::Pixel, "main", true)
            .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM))
            .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MATERIAL))
            .Build();

        m_Pipeline->CreatePipeline(m_RenderTarget->GetFramebuffer());

        m_Camera.CreatePerspective(60.0f, 1280.0f, 720.0f, 0.1f, 100.0f);

        m_CommandList = CommandList::Create();
    }

    ModelViewerPanel::~ModelViewerPanel()
    {
    }

    void ModelViewerPanel::OnGuiRender()
    {
        ImGui::Begin(m_WindowTitle.c_str(), &m_IsOpen);
        ImTextureID textureID = (ImTextureID)m_RenderTarget->GetColorAttachment(0).Get();
        ImGui::Image(textureID, ImVec2(400, 300)); // Placeholder for model image
        ImGui::SameLine();
        if (ImGui::Button("Load Model"))
        {
            std::string filepath = FileDialogs::OpenFile("Model Files (*.fbx;*.gltf;*.glb)\0*.fbx;*.gltf;*.glb\0");
            if (!filepath.empty())
            {
                m_ModelFilepath = std::filesystem::path(filepath);
                LoadModel(m_ModelFilepath);
            }
        }
        ImGui::End();
    }

    void ModelViewerPanel::OnUpdate(f32 deltaTime)
    {
        if (!m_Model || !m_IsOpen)
            return;

        m_Camera.UpdateProjectionMatrix();
        m_Camera.UpdateViewMatrix();
    }

    void ModelViewerPanel::OnRender()
    {
        if (!m_Model || !m_IsOpen)
            return;

        m_CommandList->Begin();
        auto cmd = m_CommandList->GetActiveHandle();

        CameraConstants cameraConstants = { m_Camera.GetViewProjectionMatrix(), glm::vec4(m_Camera.position, 1.0f) };

        m_RenderTarget->ClearColorAttachmentFloat(cmd);
        m_RenderTarget->ClearDepthAttachment(cmd, 1.0f, 0);

        if (m_Model)
        {
            for (size_t i = 0; i < m_Model->meshes.size(); ++i)
            {
                auto& mesh = m_Model->meshes[i];
                cmd->writeBuffer(mesh.constantBuffer, &mesh.constant, sizeof(mesh.constant));

                // get material
                mesh.material->WriteBuffer(cmd);

                // render
                auto state = nvrhi::GraphicsState();
                state.pipeline = m_Pipeline->GetHandle();
                state.framebuffer = m_RenderTarget->GetFramebuffer();
                state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(m_RenderTarget->GetFramebuffer()->getFramebufferInfo().getViewport());
                state.bindings = { mesh.bindingSet, mesh.material->bindingSet };

                state.addVertexBuffer({ mesh.mesh->GetVertexBuffer()->GetHandle(), 0, 0 });
                state.setIndexBuffer({ mesh.mesh->GetIndexBuffer()->GetHandle(), nvrhi::Format::R32_UINT });

                cmd->setGraphicsState(state);

                cmd->setPushConstants(&cameraConstants, sizeof(CameraConstants));

                nvrhi::DrawArguments args;
                args.setVertexCount(static_cast<uint32_t>(mesh.mesh->data.indices.size()));
                args.instanceCount = 1;

                cmd->drawIndexed(args);
            }
        }

        m_CommandList->Submit();
    }

    void ModelViewerPanel::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
    }

    void ModelViewerPanel::LoadModel(const std::filesystem::path& filepath)
    {
        auto asset = MeshImporter::ImportMeshSource(AssetHandle(), AssetMetaData{ AssetType::MeshSource, filepath });
        if (asset)
        {
            m_MeshAsset = std::dynamic_pointer_cast<MeshAsset>(asset);
            if (m_MeshAsset)
            {
                m_Model = CreateRef<Model>();
                m_Model->CreateMeshes(m_MeshAsset->meshesData, m_MeshAsset->materials);
            }
        }
    }

    void ModelViewerPanel::Model::CreateMeshes(const std::vector<MeshData>& meshData, const std::vector<Ref<Material>> &materials)
    {
        nvrhi::IDevice* device = Application::GetGraphicsDevice();

        meshes.resize(meshData.size());
        for (size_t i = 0; i < meshData.size(); ++i)
        {
            auto& data = meshData[i];
            meshes[i].mesh = CreateRef<Mesh>();

            meshes[i].mesh->data = data;
            meshes[i].mesh->CreateBuffers();
            meshes[i].mesh->WriteVertexBuffer();

            int materialIndex = data.materialIndex;
            if (materialIndex >= 0 && materialIndex < materials.size())
            {
                meshes[i].material = materials[materialIndex];
            }
            else
            {
                meshes[i].material = CreateRef<Material>();
            }

            // create per Mesh constant buffers
            auto bufferDesc = nvrhi::BufferDesc();
            bufferDesc.setIsConstantBuffer(true);
            bufferDesc.setIsVolatile(true);
            bufferDesc.setMaxVersions(16);
            bufferDesc.setInitialState(nvrhi::ResourceStates::ConstantBuffer);
            bufferDesc.setDebugName("MeshConstantBuffer");
            bufferDesc.setByteSize(sizeof(SkinnedMeshConstants));
            meshes[i].constantBuffer = device->createBuffer(bufferDesc);
            LOG_ASSERT(meshes[i].constantBuffer, "[MeshRenderer] Failed to create mesh constant buffer");

            // Create binding set
            auto desc = nvrhi::BindingSetDesc();
            desc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(CameraConstants)));
            desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, meshes[i].constantBuffer));
            desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, SceneRenderer::GetActive()->GetEnvironment()->GetDirLightBuffer()));
            desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, SceneRenderer::GetActive()->GetEnvironment()->GetParamsBuffer()));

            const auto newBindingSet = device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM));
            LOG_ASSERT(newBindingSet, "Failed to create binding set");

            if (newBindingSet)
            {
                meshes[i].bindingSet = newBindingSet;
            }
        }
    }

}