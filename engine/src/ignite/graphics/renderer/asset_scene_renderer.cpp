// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_scene_renderer.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/project/project.hpp"
#include "ignite/animation/skeleton.hpp"

#include <algorithm>

namespace ignite
{
    namespace
    {
        struct CompositePostProcess_GPUData
        {
            glm::vec4 flags = glm::vec4(0.0f);
            glm::vec4 vignetteParams = glm::vec4(0.0f);
            glm::vec4 chromAbParams = glm::vec4(0.0f);
            glm::vec4 vignetteColor = glm::vec4(0.0f);
        };
    }

    AssetSceneRenderer::AssetSceneRenderer()
    {
        m_PreviewMesh = nullptr;
        m_SourceMaterial = nullptr;
        m_RuntimeMaterial = CreateRef<Material>();

        auto samplerDesc = nvrhi::SamplerDesc();
        samplerDesc.setAllFilters(false);
        samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
        m_EnvironmentTexture = Renderer::GetBlackTexture();

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
        m_GeometryPipelineCache.clear();
        m_CompositePipelineCache.clear();
    }

    void AssetSceneRenderer::SetMaterial(const Ref<Material> &material)
    {
        m_SourceMaterial = material;
        SyncRuntimeMaterialFromSource();
    }

    void AssetSceneRenderer::SetPreviewMesh(const Ref<Mesh> &mesh)
    {
        m_PreviewMesh = mesh;
    }

    void AssetSceneRenderer::SetBoneTransforms(const std::vector<glm::mat4> &boneTransforms)
    {
        m_BoneTransforms = boneTransforms;
    }

    void AssetSceneRenderer::SetEnvironmentTexture(const Ref<Texture> &texture)
    {
        m_EnvironmentTextureLoadAttempted = true;
        m_EnvironmentTexture = texture ? texture : Renderer::GetBlackTexture();
        if (!m_EnvironmentTexture || !m_EnvironmentTexture->GetHandle())
        {
            m_EnvironmentTexture = Renderer::GetBlackTexture();
        }

        if (m_RuntimeMaterial)
        {
            m_RuntimeMaterial->InvalidateBindingSet();
        }

        m_LastBoundEnvironmentTexture = nullptr;
    }

    void AssetSceneRenderer::SetProject(Project *project)
    {
        m_Project = project;
        if (m_RuntimeMaterial)
        {
            m_RuntimeMaterial->InvalidateBindingSet();
        }
    }

    void AssetSceneRenderer::Render(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT)
    {
        SyncRuntimeMaterialFromSource();

        if (!camera || !sceneRT || !uiRT || !compositeRT || !m_PreviewMesh || !m_RuntimeMaterial)
        {
            return;
        }

        nvrhi::CommandListHandle cmd = m_Device->createCommandList();
        cmd->open();

        if (!m_EnvironmentTextureLoadAttempted)
        {
            m_EnvironmentTextureLoadAttempted = true;

            TextureCreateInfo textureCI;
            textureCI.dimension = nvrhi::TextureDimension::Texture2D;
            textureCI.format = nvrhi::Format::RGBA32_FLOAT;
            textureCI.flip = true;
            textureCI.keepInitialState = true;
            textureCI.initialState = nvrhi::ResourceStates::ShaderResource;

            Ref<Texture> defaultEnvironment = Texture::Create("resources/hdr/rogland_clear_night_4k.hdr", textureCI, cmd, "Asset Preview HDR");
            if (defaultEnvironment && defaultEnvironment->GetHandle())
            {
                m_EnvironmentTexture = defaultEnvironment;
            }
            else
            {
                m_EnvironmentTexture = Renderer::GetBlackTexture();
            }
        }

        CameraBufferData cameraBufferData = { camera->GetProjection(), camera->GetView(), glm::vec4(camera->position, 1.0f) };
        m_CameraBuffer->SetData(cmd, Buffer(&cameraBufferData, sizeof(CameraBufferData)));

        m_SceneBuffer->SetData(cmd, Buffer(&m_SceneGPUData, sizeof(SceneBufferData)));
        m_CSMGPUData = {};
        m_CSMGPUData.cascadeIndex = -1;
        m_CSMGPUData.shadowStrength = 0.0f;
        m_CascadedShadowMapBuffer->SetData(cmd, Buffer(&m_CSMGPUData, sizeof(CascadedShadowMapBufferData)));

        uiRT->ClearColorAttachmentFloat(cmd, 0);
        uiRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
        uiRT->ClearDepthAttachment(cmd, 1.0f, 0);

        sceneRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.08f, 0.08f, 0.1f, 1.0f));
        sceneRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
        sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);

        compositeRT->ClearColorAttachmentFloat(cmd, 0);

        DrawPreviewMesh(cmd, sceneRT->GetFramebuffer());
        CompositePass(cmd, compositeRT->GetFramebuffer(), sceneRT->GetColorAttachment(0), uiRT->GetColorAttachment(0));

        cmd->close();
        
        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            m_Device->executeCommandList(cmd);
        }
    }

    Ref<Texture> AssetSceneRenderer::GetEnvironmentMapColorTexture() const
    {
        if (m_EnvironmentTexture && m_EnvironmentTexture->GetHandle())
        {
            return m_EnvironmentTexture;
        }

        return Renderer::GetBlackTexture();
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

    void AssetSceneRenderer::DrawPreviewMesh(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
    {
        if (!m_PreviewMesh || !m_RuntimeMaterial)
        {
            return;
        }

        Ref<Texture> environmentTexture = GetEnvironmentMapColorTexture();
        nvrhi::ITexture *currentEnvHandle = environmentTexture ? environmentTexture->GetHandle() : nullptr;
        if (m_LastBoundEnvironmentTexture != currentEnvHandle)
        {
            m_RuntimeMaterial->InvalidateBindingSet();
            m_LastBoundEnvironmentTexture = currentEnvHandle;
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

        Ref<GraphicsPipeline> pipeline;
        if (auto it = m_GeometryPipelineCache.find(framebuffer); it != m_GeometryPipelineCache.end())
        {
            pipeline = it->second;
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

            pipeline = GraphicsPipeline::Create();
            pipeline->SetShaders({ vertexShader, pixelShader })
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM))
                .AddBindingLayout(Renderer::GetBindingLayout(GLayoutMap::MATERIAL))
                .Build(framebuffer, params);

            m_GeometryPipelineCache[framebuffer] = pipeline;
        }

        nvrhi::GraphicsState state;
        state.pipeline = pipeline->GetHandle();
        state.framebuffer = framebuffer;
        state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

            if (!m_BoneTransforms.empty())
            {
                if (!m_SkeletonGpuBuffer)
                {
                    m_SkeletonGpuBuffer = ConstantBuffer::Create(sizeof(GPUSkeletonBuffer), false, 1, "Preview Skeleton Buffer");
                }

                GPUSkeletonBuffer skeletonGPUData{};
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

                        Ref<Asset> texAsset = assetManager->GetAsset(textureHandle);
                        return texAsset && texAsset->IsReady();
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

