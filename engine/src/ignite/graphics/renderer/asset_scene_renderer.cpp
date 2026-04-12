// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_scene_renderer.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/project/project.hpp"

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

        m_SceneGPUDataBuffer = ConstantBuffer::Create(sizeof(Scene_GPUData), false, 1, "Asset Preview Scene Buffer");
        m_CSMGPUDataBuffer = ConstantBuffer::Create(sizeof(CascadedShadowMap_GPUData), false, 1, "Asset Preview CSM Buffer");
        m_PerEntityBuffer = ConstantBuffer::Create(sizeof(SkinnedMesh_GPUData), true, 512, "Asset Preview Per Entity Buffer");

        auto samplerDesc = nvrhi::SamplerDesc();
        samplerDesc.setAllFilters(false);
        samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);

        TextureCreateInfo textureCI;
        textureCI.dimension = nvrhi::TextureDimension::Texture2D;
        textureCI.format = nvrhi::Format::RGBA32_FLOAT;
        textureCI.flip = true;
        textureCI.keepInitialState = true;
        textureCI.initialState = nvrhi::ResourceStates::ShaderResource;

        nvrhi::CommandListHandle cmd = m_Device->createCommandList();
        cmd->open();
        m_EnvironmentTexture = Texture::Create("resources/hdr/rogland_clear_night_4k.hdr", textureCI, cmd, "Asset Preview HDR");
        cmd->close();
        m_Device->executeCommandList(cmd);

        if (!m_EnvironmentTexture || !m_EnvironmentTexture->GetHandle())
        {
            m_EnvironmentTexture = Renderer::GetBlackTexture();
        }

        auto desc = nvrhi::BindingSetDesc();
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, Renderer::GetCameraConstantBuffer()->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, m_PerEntityBuffer->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(2, m_SceneGPUDataBuffer->GetHandle()));
        desc.addItem(nvrhi::BindingSetItem::ConstantBuffer(3, m_CSMGPUDataBuffer->GetHandle()));

        m_MeshBindingSet = m_Device->createBindingSet(desc, Renderer::GetBindingLayout(GLayoutMap::MESH_ANIM));
        LOG_ASSERT(m_MeshBindingSet, "Failed to create asset preview mesh binding set");

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

    void AssetSceneRenderer::SetMaterial(const Ref<Material> &material)
    {
        m_SourceMaterial = material;
        SyncRuntimeMaterialFromSource();
    }

    void AssetSceneRenderer::SetPreviewMesh(const Ref<StaticMesh> &mesh)
    {
        m_PreviewMesh = mesh;
    }

    void AssetSceneRenderer::SetBoneTransforms(const std::vector<glm::mat4> &boneTransforms)
    {
        m_BoneTransforms = boneTransforms;
    }

    void AssetSceneRenderer::SetEnvironmentTexture(const Ref<Texture> &texture)
    {
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

        CameraBuffer cameraBuffer = { camera->GetProjection(), camera->GetView(), glm::vec4(camera->position, 1.0f) };
        Renderer::GetCameraConstantBuffer()->SetData(cmd, Buffer(&cameraBuffer, sizeof(CameraBuffer)));

        m_SceneGPUDataBuffer->SetData(cmd, Buffer(&m_SceneGPUData, sizeof(Scene_GPUData)));
        m_CSMGPUData = {};
        m_CSMGPUData.cascadeIndex = -1;
        m_CSMGPUData.shadowStrength = 0.0f;
        m_CSMGPUDataBuffer->SetData(cmd, Buffer(&m_CSMGPUData, sizeof(CascadedShadowMap_GPUData)));

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
        m_Device->executeCommandList(cmd);
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

        for (auto &meshInstance : m_PreviewMesh->GetMeshInstances())
        {
            auto &primitive = meshInstance->GetPrimitive();
            if (!primitive)
            {
                continue;
            }

            if ((!primitive->vertexBuffer || !primitive->indexBuffer)
                && !primitive->vertices.empty() && !primitive->indices.empty())
            {
                primitive->CreateBuffer(cmd);
            }

            if (!primitive->vertexBuffer || !primitive->indexBuffer)
            {
                continue;
            }

            SkinnedMesh_GPUData gpuData;
            gpuData.transformation = meshInstance->global;
            if (glm::abs(glm::determinant(gpuData.transformation)) < 0.000001f)
            {
                gpuData.transformation = glm::mat4(1.0f);
            }
            const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(gpuData.transformation)));
            gpuData.normal = glm::mat4(normalMat3);
            std::fill(std::begin(gpuData.boneTransforms), std::end(gpuData.boneTransforms), glm::mat4(1.0f));
            const size_t transformCount = std::min(static_cast<size_t>(MAX_BONES), m_BoneTransforms.size());
            for (size_t i = 0; i < transformCount; ++i)
            {
                gpuData.boneTransforms[i] = m_BoneTransforms[i];
            }
            m_PerEntityBuffer->SetData(cmd, Buffer(&gpuData, sizeof(SkinnedMesh_GPUData)));

            state.bindings = { m_MeshBindingSet, m_RuntimeMaterial->GetBindingSet() };
            state.vertexBuffers = { nvrhi::VertexBufferBinding { primitive->vertexBuffer->GetHandle(), 0, 0 } };
            state.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
            cmd->setGraphicsState(state);

            nvrhi::DrawArguments args;
            args.setVertexCount(primitive->indexBuffer->GetCount());
            args.instanceCount = 1;
            cmd->drawIndexed(args);
        }
    }

    void AssetSceneRenderer::CompositePass(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer, Ref<Texture> sceneTexture, Ref<Texture> uiTexture)
    {
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

