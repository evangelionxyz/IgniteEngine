// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_scene_renderer.hpp"

#include "renderer_2d.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/project/project.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_renderer.hpp"

#include <algorithm>
#include <limits>

namespace ignite
{
    AssetSceneRenderer::AssetSceneRenderer()
    {
        m_Device = DeviceManager::GetInstance()->GetDevice();

        {
            auto samplerDesc = nvrhi::SamplerDesc();
            samplerDesc.setAllFilters(false);
            samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
            m_CompositeSampler = m_Device->createSampler(samplerDesc);
        }

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
        m_CompositePostProcessBuffer = ConstantBuffer::Create(sizeof(CompositePostProcess_GPUData), true, 16, "Composite PostProcess Buffer");

        // m_Renderer2D = Renderer2D::Create();

        // m_EdgeDetection = EdgeDetection::Create();
        // m_EdgeDetection->CreatePipeline();
        // m_DebugGridBuffer = ConstantBuffer::Create(sizeof(DebugGrid_GPUData), true, 16, "Debug Grid Buffer");

        m_PreviewMesh = nullptr;
        m_PreviewWidget = nullptr;
        m_SourceMaterial = nullptr;
        m_RuntimeMaterial = CreateRef<Material>();

        m_SceneGPUData.sunColor = glm::vec4(1.0f, 0.98f, 0.92f, 3.0f);
        m_SceneGPUData.sungAngles = glm::vec2(glm::radians(45.0f), glm::radians(35.0f));
        m_SceneGPUData.ambient = 0.5f;
        m_SceneGPUData.exposure = 1.1f;
        m_SceneGPUData.gamma = 2.2f;
    }

    AssetSceneRenderer::~AssetSceneRenderer()
    {
        m_GeometryPipelineCache.clear();
        m_CompositePipelineCache.clear();
    }

    void AssetSceneRenderer::BeginFrame()
    {
        m_Has2DPreRenderCache = false;
    }

    void AssetSceneRenderer::SetPreviewMaterial(const Ref<Material> &material)
    {
        m_UseEnvironment = true;

        // Create environment lazily when a material preview is requested to save memory
        if (!m_Environment)
        {
            m_Environment = Environment::Create();
            m_EnvironmentTextureLoadAttempted = false;
        }

        m_SourceMaterial = material;
        SyncRuntimeMaterialFromSource();
    }

    void AssetSceneRenderer::SetPreviewMesh(const Ref<Mesh> &mesh)
    {
        m_UseEnvironment = true;

        // Create environment lazily when a mesh preview is requested to save memory
        if (!m_Environment)
        {
            m_Environment = Environment::Create();
            m_EnvironmentTextureLoadAttempted = false;
        }

        if (!m_CascadedShadowMap)
        {
            m_CascadedShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::HIGH);
        }

        m_PreviewMesh = mesh;
    }

    void AssetSceneRenderer::SetBoneTransforms(const std::vector<glm::mat4> &boneTransforms)
    {
        m_BoneTransforms = boneTransforms;
    }

    void AssetSceneRenderer::SetEnvironmentTexture(AssetHandle textureHandle)
    {
        m_EnvTexHandle = textureHandle;
        m_UseEnvironment = true;
        m_EnvironmentTextureLoadAttempted = false;
    }

    void AssetSceneRenderer::SetProject(Project *project)
    {
        m_Project = project;
        if (m_WidgetRenderer)
        {
            m_WidgetRenderer->SetProject(project);
        }
        if (m_RuntimeMaterial)
        {
            m_RuntimeMaterial->InvalidateBindingSet();
        }
    }

    void AssetSceneRenderer::SetPreviewWidget(const Ref<WidgetCanvas> &widget)
    {
        m_UseEnvironment = false;
        m_PreviewWidget = widget;

        if (!m_WidgetRenderer)
        {
            m_WidgetRenderer = WidgetRenderer::Create(1280, 720);
        }

        if (m_WidgetRenderer)
        {
            m_WidgetRenderer->SetPreviewWidget(widget);
        }
    }

    void AssetSceneRenderer::SetPreviewMouseState(uint32_t mouseX, uint32_t mouseY, bool hovered)
    {
        m_PreviewMouseX = mouseX;
        m_PreviewMouseY = mouseY;
        m_PreviewMouseHovered = hovered;
    }

    void AssetSceneRenderer::Render(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT)
    {
        SyncRuntimeMaterialFromSource();

        const bool hasMeshPreview = m_PreviewMesh && m_RuntimeMaterial;
        const bool hasWidgetPreview = m_PreviewWidget && m_WidgetRenderer;
        if (!camera || !sceneRT || !uiRT || !compositeRT || (!hasMeshPreview && !hasWidgetPreview))
        {
            return;
        }

        nvrhi::CommandListHandle cmd = m_Device->createCommandList();
        cmd->open();

        // Reload environment
        if (!m_EnvironmentTextureLoadAttempted && m_UseEnvironment)
        {
            m_EnvironmentTextureLoadAttempted = true;
            if (!m_DefaultEnvTexture && m_EnvTexHandle == AssetHandle(0))
            {
                TextureCreateInfo textureCI;
                textureCI.dimension = nvrhi::TextureDimension::Texture2D;
                textureCI.format = nvrhi::Format::RGBA32_FLOAT;
                textureCI.flip = true;
                textureCI.keepInitialState = true;
                textureCI.initialState = nvrhi::ResourceStates::ShaderResource;
                m_DefaultEnvTexture = Texture::Create("resources/hdr/snowy_field_2k.hdr", textureCI, cmd, "Asset Preview HDR");

                m_DefaultEnvTexture->SetReadyFlag(true);
            }

            Ref<Texture> envTex;
            if (m_Environment)
            {
                // prefer explicit handle if provided
                if (m_EnvTexHandle != AssetHandle(0))
                {
                    envTex = m_Project->GetAsset<Texture>(m_EnvTexHandle);
                }
                else
                {
                    envTex = (m_DefaultEnvTexture && m_DefaultEnvTexture->GetHandle()) ? m_DefaultEnvTexture : Renderer::GetBlackTexture();
                }

                // Texture not loaded yet
                // so we need to retrieve it again until we got it
                if (!envTex)
                {
                    m_EnvironmentTextureLoadAttempted = false;
                }
                else
                {
                    m_Environment->SetTexture(envTex);
                    m_EnvTextureInvalidating = true;
                }
            }

            // Refresh material
            if (m_RuntimeMaterial && envTex)
            {
                m_RuntimeMaterial->InvalidateBindingSet();
            }
        }

        CameraBufferData cameraBufferData = { camera->GetProjection(), camera->GetView(), glm::vec4(camera->position, 1.0f) };
        m_CameraBuffer->SetData(cmd, Buffer(&cameraBufferData, sizeof(CameraBufferData)));

        m_SceneBuffer->SetData(cmd, Buffer(&m_SceneGPUData, sizeof(SceneBufferData)));
        m_CSMGPUData = {};
        m_CSMGPUData.cascadeIndex = -1;
        m_CSMGPUData.shadowStrength = 0.0f;
        m_CascadedShadowMapBuffer->SetData(cmd, Buffer(&m_CSMGPUData, sizeof(CascadedShadowMapBufferData)));

        uiRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
        uiRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
        uiRT->ClearDepthAttachment(cmd, 1.0f, 0);

        sceneRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
        sceneRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
        sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);

        compositeRT->ClearColorAttachmentFloat(cmd, 0);

        if (m_Environment && m_UseEnvironment)
        {
            DrawEnvironment(cmd, camera, sceneRT->GetFramebuffer());
        }

        if (hasMeshPreview)
        {
            DrawPreviewMesh(cmd, sceneRT->GetFramebuffer());
        }

        if (m_PreviewWidget)
        {
            const nvrhi::Viewport viewport = uiRT->GetFramebuffer()->getFramebufferInfo().getViewport();
            const uint32_t width = std::max(1u, static_cast<uint32_t>(viewport.maxX - viewport.minX));
            const uint32_t height = std::max(1u, static_cast<uint32_t>(viewport.maxY - viewport.minY));

            if (m_WidgetRenderer->GetWidth() != width || m_WidgetRenderer->GetHeight() != height)
            {
                m_WidgetRenderer->Resize(width, height);
            }

            if (m_PreviewMouseHovered)
            {
                m_WidgetRenderer->SetMousePosition(m_PreviewMouseX, m_PreviewMouseY);
            }
            else
            {
                const uint32_t offscreen = std::numeric_limits<uint32_t>::max() / 2u;
                m_WidgetRenderer->SetMousePosition(offscreen, offscreen);
            }

            m_WidgetRenderer->Update(0.0f);
            m_WidgetRenderer->Render(cmd, uiRT->GetFramebuffer());
        }

        CompositePass(cmd, compositeRT->GetFramebuffer(), sceneRT->GetColorAttachment(0), uiRT->GetColorAttachment(0));
        cmd->close();
        
        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            m_Device->executeCommandList(cmd);
        }
    }

    void AssetSceneRenderer::SyncRuntimeMaterialFromSource()
    {
        if (!m_SourceMaterial)
        {
            return;
        }

        if (!m_RuntimeMaterial)
        {
            m_RuntimeMaterial = CreateRef<Material>();
        }

        m_RuntimeMaterial->name = m_SourceMaterial->name;
        m_RuntimeMaterial->gpuData = m_SourceMaterial->gpuData;
        m_RuntimeMaterial->baseColorTextureHandle = m_SourceMaterial->baseColorTextureHandle;
        m_RuntimeMaterial->emissiveTextureHandle = m_SourceMaterial->emissiveTextureHandle;
        m_RuntimeMaterial->metallicTextureHandle = m_SourceMaterial->metallicTextureHandle;
        m_RuntimeMaterial->roughnessTextureHandle = m_SourceMaterial->roughnessTextureHandle;
        m_RuntimeMaterial->normalTextureHandle = m_SourceMaterial->normalTextureHandle;
        m_RuntimeMaterial->occlusionTextureHandle = m_SourceMaterial->occlusionTextureHandle;
        m_RuntimeMaterial->SetDirtyFlag(m_SourceMaterial->IsDirty());
        m_RuntimeMaterial->InvalidateBindingSet();
    }

    void AssetSceneRenderer::DrawEnvironment(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer)
    {
        Ref<GraphicsPipeline> envPipeline;
        if (auto it = m_EnvironmentPipelineCache.find(framebuffer); it != m_EnvironmentPipelineCache.end())
        {
            envPipeline = it->second;
        }
        else
        {
            const nvrhi::FramebufferDesc &fbDesc = framebuffer->getDesc();
            bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

            GraphicsPipelineParams params;
            params.enableBlend = true;
            params.enableDepthWrite = hasDepthAttachment;
            params.enableDepthTest = hasDepthAttachment;
            params.enableDepthStencil = false;
            params.fillMode = nvrhi::RasterFillMode::Solid;
            params.cullMode = nvrhi::RasterCullMode::Front;
            params.depthFunc = nvrhi::ComparisonFunc::Always;

            Ref<Shader> vertexShader = Shader::Create("resources/shaders/skybox.vertex.hlsl", ShaderType::Vertex, false);
            Ref<Shader> pixelShader = Shader::Create("resources/shaders/skybox.pixel.hlsl", ShaderType::Pixel, false);

            envPipeline = GraphicsPipeline::Create();
            envPipeline->SetShaders({ vertexShader, pixelShader })
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::ENVIRONMENT))
                .Build(framebuffer, params);

            m_EnvironmentPipelineCache.clear();
            m_EnvironmentPipelineCache[framebuffer] = envPipeline;
        }

        if (m_EnvTextureInvalidating)
        {
            Ref<Texture> tex = m_Environment->GetHDRTexture();
            if (tex && tex->IsReady())
            {
                m_Environment->WriteBuffer(cmd);
                m_Environment->UpdateBindingSet(m_CameraBuffer, m_SceneBuffer);
                m_EnvTextureInvalidating = false;
            }
        }

        m_Environment->Draw(cmd, camera, framebuffer, envPipeline);
    }

    void AssetSceneRenderer::DrawPreviewMesh(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
    {
        if (!m_PreviewMesh || !m_RuntimeMaterial)
        {
            return;
        }

        MaterialTextures textures;
        if (m_Project)
        {
            auto *assetManager = m_Project->GetAssetManager();
            m_RuntimeMaterial->RetrieveTextures(assetManager, &textures);
            m_RuntimeMaterial->UpdateBindingSet(this, &textures, assetManager);
        }

        if (!m_RuntimeMaterial->GetBindingSet())
        {
            return;
        }

        m_RuntimeMaterial->UploadToGpu(cmd);

        Ref<GraphicsPipeline> geopPipeline;
        if (auto it = m_GeometryPipelineCache.find(framebuffer); it != m_GeometryPipelineCache.end())
        {
            geopPipeline = it->second;
        }
        else
        {
            GraphicsPipelineParams params;
            params.enableBlend = true;
            params.enableDepthWrite = true;
            params.enableDepthTest = true;
            params.enableDepthStencil = false;
            params.fillMode = nvrhi::RasterFillMode::Solid;
            params.cullMode = nvrhi::RasterCullMode::None;
            params.depthFunc = nvrhi::ComparisonFunc::LessOrEqual;

            Ref<Shader> vertexShader = Shader::Create("resources/shaders/mesh_anim.vertex.hlsl", ShaderType::Vertex, false);
            Ref<Shader> pixelShader = Shader::Create("resources/shaders/mesh_anim.pixel.hlsl", ShaderType::Pixel, false);

            geopPipeline = GraphicsPipeline::Create();
            geopPipeline->SetShaders({ vertexShader, pixelShader })
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM))
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MATERIAL))
                .Build(framebuffer, params);

            m_GeometryPipelineCache.clear();
            m_GeometryPipelineCache[framebuffer] = geopPipeline;
        }

        nvrhi::GraphicsState state;
        state.pipeline = geopPipeline->GetHandle();
        state.framebuffer = framebuffer;
        state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

        if (!m_BoneTransforms.empty())
        {
            if (!m_SkeletonGpuBuffer)
            {
                m_SkeletonGpuBuffer = ConstantBuffer::Create(sizeof(GPUSkeletonBuffer), false, 1, "Preview Skeleton Buffer");
            }

            GPUSkeletonBuffer skeletonGPUData {};
            const size_t boneCount = std::min(static_cast<size_t>(MAX_BONES), m_BoneTransforms.size());
            for (size_t i = 0; i < boneCount; ++i)
            {
                skeletonGPUData.bones[i] = m_BoneTransforms[i];
            }

            for (size_t i = boneCount; i < MAX_BONES; ++i)
            {
                skeletonGPUData.bones[i] = glm::mat4(1.0f);
            }

            m_SkeletonGpuBuffer->SetData(cmd, Buffer(&skeletonGPUData, sizeof(skeletonGPUData)));
        }

        for (auto &meshInstance : m_PreviewMesh->GetMeshInstances())
        {
            auto &primitive = meshInstance->GetPrimitive();
            if (!primitive)
            {
                continue;
            }

            if ((!primitive->vertexBuffer || !primitive->indexBuffer) && !primitive->vertices.empty() && !primitive->indices.empty())
            {
                primitive->CreateBuffer(cmd);
            }

            if (!primitive->vertexBuffer || !primitive->indexBuffer)
            {
                continue;
            }

            SkinnedMeshBufferData gpuData;

            // For non-skinned sub-meshes linked to a joint, apply the joint's animated transform
            glm::mat4 meshTransform = meshInstance->global;
            if (meshInstance->linkedJointIndex >= 0 && !m_BoneTransforms.empty())
            {
                const size_t ji = static_cast<size_t>(meshInstance->linkedJointIndex);
                if (ji < m_BoneTransforms.size())
                {
                    meshTransform = m_BoneTransforms[ji] * meshTransform;
                }
            }
            gpuData.transformation = meshTransform;
            if (glm::abs(glm::determinant(gpuData.transformation)) < 0.000001f)
            {
                gpuData.transformation = glm::mat4(1.0f);
            }
            const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(gpuData.transformation)));
            gpuData.normal = glm::mat4(normalMat3);
            meshInstance->SetData(cmd, &gpuData, sizeof(SkinnedMeshBufferData));
            meshInstance->EnsureBuffer(cmd, m_CameraBuffer, m_SceneBuffer, m_CascadedShadowMapBuffer, m_SkeletonGpuBuffer);

            nvrhi::BindingSetHandle meshBindingSet = meshInstance->GetBindingSet();
            
            Ref<Material> material = m_Project->GetAsset<Material>(meshInstance->GetMaterialHandle());
            {
                bool waitedForMaterialUpdate = false;
                if (material && (material->IsBindingSetDirty() || !material->GetBindingSet()))
                {
                    auto assetManager = m_Project->GetAssetManager();
                    auto isTextureReady = [&assetManager](AssetHandle textureHandle)
                    {
                        if (textureHandle == 0)
                        {
                            return true;
                        }

                        Ref<Texture> texture = assetManager->GetAsset<Texture>(textureHandle);
                        return texture && texture->IsReady();
                    };

                    // Only update if all textures are available and ready
                    bool allTexturesReady = true;
                    if (!isTextureReady(material->baseColorTextureHandle))
                        allTexturesReady = false;
                    if (!isTextureReady(material->emissiveTextureHandle))
                        allTexturesReady = false;
                    if (!isTextureReady(material->metallicTextureHandle))
                        allTexturesReady = false;
                    if (!isTextureReady(material->roughnessTextureHandle))
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
            

            if (material && !material->GetBindingSet())
            {
                MaterialTextures textures;
                auto assetManager = m_Project->GetAssetManager();
                material->RetrieveTextures(assetManager, &textures);
                material->UpdateBindingSet(this, &textures, assetManager);
            }

            if (material)
            {
                material->UploadToGpu(cmd);
            }

            if (meshBindingSet)
            {
                state.bindings = { meshBindingSet, material ? material->GetBindingSet() : m_RuntimeMaterial->GetBindingSet() };
                state.vertexBuffers = { nvrhi::VertexBufferBinding { primitive->vertexBuffer->GetHandle(), 0, 0 } };
                state.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
                cmd->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.setVertexCount(primitive->indexBuffer->GetCount());
                args.instanceCount = 1;
                cmd->drawIndexed(args);
            }
        }
    }

    void AssetSceneRenderer::CompositePass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture)
    {
        EnsureCompositeVertexBufferUploaded(cmd);

        CompositePostProcess_GPUData postProcessData;
        m_CompositePostProcessBuffer->SetData(cmd, Buffer(&postProcessData, sizeof(postProcessData)));

        Ref<GraphicsPipeline> pipeline;
        if (auto it = m_CompositePipelineCache.find(framebuffer); it != m_CompositePipelineCache.end())
        {
            pipeline = it->second;
        }
        else
        {
            nvrhi::BindingLayoutDesc layoutDesc = {};
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(4));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0));
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));

            m_CompositeBindingLayout = m_Device->createBindingLayout(layoutDesc);

            GraphicsPipelineParams params;
            params.enableBlend = true;
            params.enableDepthWrite = false;
            params.enableDepthTest = false;
            params.enableDepthStencil = false;
            params.fillMode = nvrhi::RasterFillMode::Solid;
            params.cullMode = nvrhi::RasterCullMode::None;

            Ref<Shader> vertexShader = Shader::Create("resources/shaders/composite.vertex.hlsl", ShaderType::Vertex, false);
            Ref<Shader> pixelShader = Shader::Create("resources/shaders/composite.pixel.hlsl", ShaderType::Pixel, false);

            pipeline = GraphicsPipeline::Create();
            pipeline->SetShaders({ vertexShader, pixelShader })
                .AddBindingLayout(m_CompositeBindingLayout)
                .Build(framebuffer, params);

            m_CompositePipelineCache.clear();
            m_CompositePipelineCache[framebuffer] = pipeline;
        }

        auto bindingSetDesc = nvrhi::BindingSetDesc();
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, sceneTexture->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(1, uiTexture->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(2, Renderer::GetBlackTexture()->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(3, Renderer::GetBlackTexture()->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(4, Renderer::GetWhiteTexture()->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_CompositePostProcessBuffer->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_CompositeSampler));

        nvrhi::BindingSetHandle bindingSet = m_Device->createBindingSet(bindingSetDesc, pipeline->GetBindingLayout(0));

        nvrhi::GraphicsState state;
        state.pipeline = pipeline->GetHandle();
        state.framebuffer = framebuffer;
        state.vertexBuffers = { nvrhi::VertexBufferBinding { m_CompositeVertexBuffer->GetHandle(), 0, 0 } };
        state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
        state.bindings = { bindingSet };
        cmd->setGraphicsState(state);

        nvrhi::DrawArguments args;
        args.instanceCount = 1;
        args.vertexCount = 6;
        cmd->draw(args);
    }
}
