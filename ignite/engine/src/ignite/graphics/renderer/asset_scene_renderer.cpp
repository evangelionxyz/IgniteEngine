// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "asset_scene_renderer.hpp"

#include "renderer_2d.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_renderer.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/bindless_system.hpp"

#include <type_traits>

namespace ignite
{
    static const std::array<glm::mat4, MAX_BONES> s_IdentitySkeleton = []()
    {
        std::array<glm::mat4, MAX_BONES> buf;
        for (int i = 0; i < MAX_BONES; ++i)
        {
            buf[i] = glm::mat4(1.0f);
        }
        return buf;
    }();

    Ref<Texture> AssetSceneRenderer::m_DefaultEnvTexture;

    AssetSceneRenderer::AssetSceneRenderer()
    {
		auto samplerDesc = nvrhi::SamplerDesc();
		samplerDesc.setAllFilters(false);
		samplerDesc.setAllAddressModes(nvrhi::SamplerAddressMode::Clamp);
		m_CompositeSampler = m_Device->createSampler(samplerDesc);

        m_PreviewStaticMesh   = nullptr;
        m_PreviewSkeletalMesh = nullptr;
        m_PreviewWidget       = nullptr;
        m_SourceMaterial      = nullptr;
        m_RuntimeMaterial     = CreateRef<Material>();

        m_SceneGPUData.sunColor   = glm::vec4(1.0f, 0.98f, 0.92f, 3.0f);
        m_SceneGPUData.sungAngles = glm::vec2(glm::radians(45.0f), glm::radians(35.0f));
        m_SceneGPUData.ambient    = 0.5f;
        m_SceneGPUData.exposure   = 1.1f;
        m_SceneGPUData.gamma      = 2.2f;
    }

    AssetSceneRenderer::~AssetSceneRenderer()
    {
        ClearPinnedAssets();

        m_StaticGeometryPipelineCache.clear();
        m_StaticTransparentPipelineCache.clear();
        m_SkeletalGeometryPipelineCache.clear();
        m_SkeletalTransparentPipelineCache.clear();
        m_CompositePipelineCache.clear();

        m_Environment = nullptr;

        if (m_DefaultEnvTexture && m_DefaultEnvTexture.use_count() == 1)
        {
            m_DefaultEnvTexture = nullptr;
        }
    }

    void AssetSceneRenderer::BeginFrame()
    {
        m_Has2DPreRenderCache = false;
    }

    // ---------------------------------------------------------------------------
    // SetPreviewMaterial — material only; preview mesh is managed separately via
    // SetPreviewStaticMesh() so callers don't need to pass the mesh here.
    // ---------------------------------------------------------------------------
    void AssetSceneRenderer::SetPreviewMaterial(const Ref<Material> &material)
    {
        m_UseEnvironment = true;

        // Create environment lazily when a material preview is requested
        if (!m_Environment)
        {
            m_Environment = Environment::Create();
            m_EnvironmentTextureLoadAttempted = false;
        }

        m_SourceMaterial = material;
        SyncRuntimeMaterialFromSource();
    }

    void AssetSceneRenderer::SetPreviewSkeletalMesh(const Ref<SkeletalMesh> &mesh)
    {
        m_UseEnvironment = true;

        if (!m_Environment)
        {
            m_Environment = Environment::Create();
            m_EnvironmentTextureLoadAttempted = false;
        }

        if (!m_CascadedShadowMap)
        {
            m_CascadedShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::HIGH);
        }

        m_PreviewSkeletalMesh = mesh;
    }

    void AssetSceneRenderer::SetPreviewStaticMesh(const Ref<StaticMesh> &mesh)
    {
        m_UseEnvironment = true;

        if (!m_Environment)
        {
            m_Environment = Environment::Create();
            m_EnvironmentTextureLoadAttempted = false;
        }

        if (!m_CascadedShadowMap)
        {
            m_CascadedShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::HIGH);
        }

        m_PreviewStaticMesh = mesh;
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
            m_WidgetRenderer->SetActiveWidget(widget);
        }
    }

    void AssetSceneRenderer::SetPreviewMouseState(uint32_t mouseX, uint32_t mouseY, bool hovered)
    {
        m_PreviewMouseX       = mouseX;
        m_PreviewMouseY       = mouseY;
        m_PreviewMouseHovered = hovered;
    }

    void AssetSceneRenderer::Render(ICamera *camera, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT)
    {
        SyncRuntimeMaterialFromSource();

        // A mesh preview is valid whenever a static or skeletal mesh is set.
        // Per-instance materials on the mesh remove the hard requirement for m_RuntimeMaterial.
        const bool hasMeshPreview   = m_PreviewStaticMesh != nullptr || m_PreviewSkeletalMesh != nullptr;
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
                textureCI.dimension    = nvrhi::TextureDimension::Texture2D;
                textureCI.format       = nvrhi::Format::RGBA32_FLOAT;
                textureCI.flip         = true;
                textureCI.keepInitialState = true;
                textureCI.initialState = nvrhi::ResourceStates::ShaderResource;
                m_DefaultEnvTexture    = Texture::Create("resources/hdr/snowy_field_2k.hdr", textureCI, cmd, "Asset Preview HDR");
                m_DefaultEnvTexture->SetReadyFlag(true);
            }

            Ref<Texture> envTex;
            if (m_Environment)
            {
                envTex = (m_EnvTexHandle != AssetHandle(0))
                    ? AssetManager::GetInstance()->GetAsset<Texture>(m_EnvTexHandle)
                    : ((m_DefaultEnvTexture && m_DefaultEnvTexture->GetHandle()) ? m_DefaultEnvTexture : Renderer::GetBlackTexture());

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

            if (m_RuntimeMaterial && envTex)
            {
                m_RuntimeMaterial->InvalidateBindingSet();
            }
        }

        CameraBufferData cameraBufferData = { camera->GetProjection(), camera->GetView(), glm::vec4(camera->position, 1.0f) };
        m_CameraBuffer->SetData(cmd, Buffer(&cameraBufferData, sizeof(CameraBufferData)));
        m_SceneBuffer->SetData(cmd, Buffer(&m_SceneGPUData, sizeof(Scene_GPUData)));

        // Cascaded Shadow Map — disabled in preview (shadowStrength = 0)
        m_CSMGPUData               = {};
        m_CSMGPUData.cascadeIndex  = -1;
        m_CSMGPUData.shadowStrength = 0.0f;
        m_CascadedShadowMapBuffer->SetData(cmd, Buffer(&m_CSMGPUData, sizeof(CSM_GPUData)));

        uiRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
        uiRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
        uiRT->ClearDepthAttachment(cmd, 1.0f, 0);

        sceneRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
        sceneRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
        sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);
        
        compositeRT->ClearColorAttachmentFloat(cmd, 0);

		// Transition the scene and UI textures to shader resource state for sampling in the composite pass
		cmd->setTextureState(sceneRT->GetColorAttachment(0)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		cmd->setTextureState(sceneRT->GetColorAttachment(1)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		cmd->setTextureState(sceneRT->GetDepthAttachment()->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
		
        cmd->setTextureState(uiRT->GetColorAttachment(0)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        cmd->setTextureState(uiRT->GetColorAttachment(1)->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);
        cmd->setTextureState(uiRT->GetDepthAttachment()->GetHandle(), nvrhi::AllSubresources, nvrhi::ResourceStates::ShaderResource);

		cmd->commitBarriers();

        if (m_Environment && m_UseEnvironment)
        {
            DrawEnvironment(cmd, camera, sceneRT->GetFramebuffer());
        }

        if (hasMeshPreview)
        {
            DrawPreviewStaticMesh(cmd, sceneRT->GetFramebuffer());
            DrawPreviewSkeletalMesh(cmd, sceneRT->GetFramebuffer());
        }

        if (m_PreviewWidget)
        {
            const nvrhi::Viewport viewport = uiRT->GetFramebuffer()->getFramebufferInfo().getViewport();
            const uint32_t width  = std::max(1u, static_cast<uint32_t>(viewport.maxX - viewport.minX));
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

    Ref<Texture> AssetSceneRenderer::GetEnvironmentMapColorTexture() const
    {
        if (m_Environment && m_Environment->GetHDRTexture())
            return m_Environment->GetHDRTexture();
        return m_DefaultEnvTexture ? m_DefaultEnvTexture : nullptr;
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

        m_RuntimeMaterial->name                   = m_SourceMaterial->name;
        m_RuntimeMaterial->gpuData                = m_SourceMaterial->gpuData;
        m_RuntimeMaterial->baseColorTextureHandle = m_SourceMaterial->baseColorTextureHandle;
        m_RuntimeMaterial->emissiveTextureHandle  = m_SourceMaterial->emissiveTextureHandle;
        m_RuntimeMaterial->metallicTextureHandle  = m_SourceMaterial->metallicTextureHandle;
        m_RuntimeMaterial->roughnessTextureHandle = m_SourceMaterial->roughnessTextureHandle;
        m_RuntimeMaterial->normalTextureHandle    = m_SourceMaterial->normalTextureHandle;
        m_RuntimeMaterial->occlusionTextureHandle = m_SourceMaterial->occlusionTextureHandle;
        m_RuntimeMaterial->SetDirtyFlag(m_SourceMaterial->IsDirty());
        m_RuntimeMaterial->InvalidateBindingSet();
    }

    // ---------------------------------------------------------------------------
    // DrawEnvironment
    // ---------------------------------------------------------------------------
    void AssetSceneRenderer::DrawEnvironment(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *framebuffer)
    {
        Ref<GraphicsPipeline> envPipeline;
        if (auto it = m_EnvironmentPipelineCache.find(framebuffer); it != m_EnvironmentPipelineCache.end())
        {
            envPipeline = it->second;
        }
        else
        {
            const nvrhi::FramebufferDesc &fbDesc   = framebuffer->getDesc();
            const bool hasDepthAttachment = fbDesc.depthAttachment.texture != nullptr;

            GraphicsPipelineParams params;
            params.enableBlend       = true;
            params.enableDepthWrite  = hasDepthAttachment;
            params.enableDepthTest   = hasDepthAttachment;
            params.enableDepthStencil = false;
            params.fillMode          = nvrhi::RasterFillMode::Solid;
            params.cullMode          = nvrhi::RasterCullMode::Front;
            params.depthFunc         = nvrhi::ComparisonFunc::Always;

            Ref<Shader> vertexShader = Shader::Create("resources/shaders/skybox.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
            Ref<Shader> pixelShader  = Shader::Create("resources/shaders/skybox.pixel.hlsl",  UMBRA_SHADER_TYPE_PIXEL,  false);

            envPipeline = GraphicsPipeline::Create();
            envPipeline->SetShaders({ vertexShader, pixelShader })
                .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::ENVIRONMENT))
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

        m_Environment->Draw(cmd, framebuffer, envPipeline);
    }

    // ---------------------------------------------------------------------------
    // DrawPreviewMeshImpl — generic implementation for static and skeletal meshes.
    //
    // Key per-type differences handled via if constexpr:
    //   • Skeletal: applies bone transforms to the per-instance world matrix and
    //     uploads the skeleton constant buffer via SetSkeletonData.
    //   • Static:   no skeleton data needed.
    //
    // The caller (DrawPreviewStaticMesh / DrawPreviewSkeletalMesh) passes the
    // correct shader paths, binding layout enum, and pipeline cache references so
    // no global state is shared between the two mesh types.
    // ---------------------------------------------------------------------------
    template<typename MeshT>
    void AssetSceneRenderer::DrawPreviewMeshImpl(
        const Ref<MeshT>           &mesh,
        nvrhi::ICommandList        *cmd,
        nvrhi::IFramebuffer        *framebuffer,
        const char                 *vertexShaderPath,
        const char                 *pixelShaderPath,
        EBindingLayout              meshBindingLayout,
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> &opaqueCache,
        std::unordered_map<const nvrhi::IFramebuffer *, Ref<GraphicsPipeline>> &transparentCache)
    {
        static_assert(std::is_same_v<MeshT, StaticMesh> || std::is_same_v<MeshT, SkeletalMesh>,
            "DrawPreviewMeshImpl: MeshT must be StaticMesh or SkeletalMesh");

        if (!mesh || !m_RuntimeMaterial)
            return;

        // Update the shared runtime material binding set.
        // This is always required — even when per-instance materials are present,
        // the runtime material is the fallback for any instance with no assigned material.
        if (!m_RuntimeMaterial->UpdateBindingSet(GetEnvironmentMapColorTexture(), GetCascadedShadowMapDepthTexture()))
            return;

        m_RuntimeMaterial->UploadToGpu(cmd);

        // Pipeline selection: transparent vs opaque based on the runtime material type.
        const bool isTransparent = m_RuntimeMaterial->GetType() == MaterialType::Transparent;
        auto &pipelineCache      = isTransparent ? transparentCache : opaqueCache;

        Ref<GraphicsPipeline> geopPipeline;
        if (auto it = pipelineCache.find(framebuffer); it != pipelineCache.end())
        {
            geopPipeline = it->second;
        }
        else
        {
            GraphicsPipelineParams params;
            params.enableDepthTest   = true;
            params.enableDepthStencil = false;
            params.fillMode          = nvrhi::RasterFillMode::Solid;
            params.cullMode          = nvrhi::RasterCullMode::None;
            params.depthFunc         = nvrhi::ComparisonFunc::LessOrEqual;
            params.enableBlend       = true;

            if (isTransparent)
            {
                params.srcBlend       = nvrhi::BlendFactor::SrcAlpha;
                params.destBlend      = nvrhi::BlendFactor::InvSrcAlpha;
                params.srcBlendAlpha  = nvrhi::BlendFactor::One;
                params.destBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
                params.enableDepthWrite = false;
            }
            else
            {
                params.enableDepthWrite = true;
            }

            Ref<Shader> vertexShader = Shader::Create(vertexShaderPath, UMBRA_SHADER_TYPE_VERTEX, false);
            Ref<Shader> pixelShader  = Shader::Create(pixelShaderPath,  UMBRA_SHADER_TYPE_PIXEL,  false);

            geopPipeline = GraphicsPipeline::Create();
            geopPipeline->SetShaders({ vertexShader, pixelShader })
                .AddBindingLayout(Renderer::GetBindingLayout(meshBindingLayout))
                .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MATERIAL))
                .AddBindingLayout(BindlessSystem::GetBindingLayout())
                .Build(framebuffer, params);

            pipelineCache.clear();
            pipelineCache[framebuffer] = geopPipeline;
        }

        nvrhi::GraphicsState state;
        state.pipeline    = geopPipeline->GetHandle();
        state.framebuffer = framebuffer;
        state.viewport    = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());

        // Pre-build bone matrix array (only for skeletal meshes)
        glm::mat4 bones[MAX_BONES];
        if constexpr (std::is_same_v<MeshT, SkeletalMesh>)
        {
            if (!m_BoneTransforms.empty())
            {
                const size_t boneCount = std::min(static_cast<size_t>(MAX_BONES), m_BoneTransforms.size());
                std::memcpy(bones, m_BoneTransforms.data(), boneCount * sizeof(glm::mat4));
                if (boneCount < MAX_BONES)
                {
                    std::memcpy(&bones[boneCount], &s_IdentitySkeleton[boneCount],
                        (MAX_BONES - boneCount) * sizeof(glm::mat4));
                }
            }
            else
            {
                std::memcpy(bones, s_IdentitySkeleton.data(), sizeof(bones));
            }
        }

        for (auto &meshInstance : mesh->GetMeshInstances())
        {
            auto &primitive = meshInstance->GetPrimitive();
            if (!primitive)
                continue;

            if (!primitive->vertexBuffer || !primitive->indexBuffer)
                primitive->WriteBuffer(cmd);

            if (!meshInstance->UpdateBindingSet(m_CameraBuffer, m_SceneBuffer, m_CascadedShadowMapBuffer,
                    m_PointLightBuffer, m_SpotLightBuffer))
                continue;

            // Build per-instance GPU transform
            Mesh_GPUData gpuData;
            glm::mat4 meshTransform = meshInstance->global;

            if constexpr (std::is_same_v<MeshT, SkeletalMesh>)
            {
                // Apply the bone's world transform for meshes linked to a skeleton joint
                if (meshInstance->linkedJointIndex >= 0 && !m_BoneTransforms.empty())
                {
                    const auto ji = static_cast<size_t>(meshInstance->linkedJointIndex);
                    if (ji < m_BoneTransforms.size())
                        meshTransform = m_BoneTransforms[ji] * meshTransform;
                }
            }

            gpuData.transformation = meshTransform;
            if (glm::abs(glm::determinant(gpuData.transformation)) < 0.000001f)
                gpuData.transformation = glm::mat4(1.0f);

            const glm::mat3 normalMat3 = glm::transpose(glm::inverse(glm::mat3(gpuData.transformation)));
            gpuData.normal = glm::mat4(normalMat3);
            meshInstance->SetData(cmd, &gpuData, sizeof(Mesh_GPUData));

            if constexpr (std::is_same_v<MeshT, SkeletalMesh>)
            {
                meshInstance->SetSkeletonData(cmd, bones, sizeof(bones));
            }

            // Resolve material:
            //  - Material-preview mode (m_SourceMaterial set): always use the runtime material
            //    so the material being edited covers every mesh slot, ignoring per-instance overrides.
            //  - Mesh-preview mode (no m_SourceMaterial): per-instance material takes priority,
            //    with m_RuntimeMaterial as the fallback for unassigned slots.
            Ref<Material> material;
            if (!m_SourceMaterial)
            {
                const AssetHandle materialHandle = meshInstance->GetMaterialAssetHandle();
                if (materialHandle != AssetHandle(0))
                {
                    material = ResolveAsset<Material>(materialHandle);
                    if (material)
                    {
                        if (material->UpdateBindingSet(GetEnvironmentMapColorTexture(), GetCascadedShadowMapDepthTexture()))
                            material->UploadToGpu(cmd);
                    }
                }
            }

            const nvrhi::BindingSetHandle meshBindingSet     = meshInstance->GetBindingSet();
            const nvrhi::BindingSetHandle materialBindingSet = (material && material->GetBindingSet())
                ? material->GetBindingSet()
                : m_RuntimeMaterial->GetBindingSet();

            if (meshBindingSet && materialBindingSet)
            {
                state.bindings      = { meshBindingSet, materialBindingSet, BindlessSystem::GetDescriptorTable() };
                state.vertexBuffers = { nvrhi::VertexBufferBinding{ primitive->vertexBuffer->GetHandle(), 0, 0 } };
                state.setIndexBuffer({ primitive->indexBuffer->GetHandle(), nvrhi::Format::R32_UINT });
                cmd->setGraphicsState(state);

                nvrhi::DrawArguments args;
                args.setVertexCount(primitive->indexBuffer->GetCount());
                args.instanceCount = 1;
                cmd->drawIndexed(args);
            }
        }
    }

    // ---------------------------------------------------------------------------
    // DrawPreviewStaticMesh — delegates to the generic template.
    // Uses the static-mesh shaders and its own pipeline cache pair so it never
    // collides with the skeletal-mesh pipeline cache.
    // ---------------------------------------------------------------------------
    void AssetSceneRenderer::DrawPreviewStaticMesh(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
    {
        DrawPreviewMeshImpl(
            m_PreviewStaticMesh,
            cmd,
            framebuffer,
            "resources/shaders/mesh_static.vertex.hlsl",
            "resources/shaders/mesh_static.pixel.hlsl",
            EBindingLayout::MESH_STATIC,
            m_StaticGeometryPipelineCache,
            m_StaticTransparentPipelineCache);
    }

    // ---------------------------------------------------------------------------
    // DrawPreviewSkeletalMesh — delegates to the generic template.
    // Uses the animated-mesh shaders and its own pipeline cache pair.
    // ---------------------------------------------------------------------------
    void AssetSceneRenderer::DrawPreviewSkeletalMesh(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *framebuffer)
    {
        DrawPreviewMeshImpl(
            m_PreviewSkeletalMesh,
            cmd,
            framebuffer,
            "resources/shaders/mesh_anim.vertex.hlsl",
            "resources/shaders/mesh_anim.pixel.hlsl",
            EBindingLayout::MESH_ANIM,
            m_SkeletalGeometryPipelineCache,
            m_SkeletalTransparentPipelineCache);
    }

    // ---------------------------------------------------------------------------
    // CompositePass
    // ---------------------------------------------------------------------------
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
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)); // scene
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(1)); // ui
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(2)); // edge
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(3)); // bloom
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(4)); // ssao
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(5)); // depth
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Texture_SRV(6)); // debug
            layoutDesc.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)); // post-process params
            layoutDesc.addItem(nvrhi::BindingLayoutItem::Sampler(0));

            m_CompositeBindingLayout = m_Device->createBindingLayout(layoutDesc);

            GraphicsPipelineParams params;
            params.enableBlend        = true;
            params.enableDepthWrite   = false;
            params.enableDepthTest    = false;
            params.enableDepthStencil = false;
            params.fillMode           = nvrhi::RasterFillMode::Solid;
            params.cullMode           = nvrhi::RasterCullMode::None;

            Ref<Shader> vertexShader = Shader::Create("resources/shaders/composite.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
            Ref<Shader> pixelShader  = Shader::Create("resources/shaders/composite.pixel.hlsl",  UMBRA_SHADER_TYPE_PIXEL,  false);

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
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(5, Renderer::GetBlackTexture()->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(6, Renderer::GetBlackTexture()->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, m_CompositePostProcessBuffer->GetHandle()));
        bindingSetDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_CompositeSampler));

        nvrhi::BindingSetHandle bindingSet = m_Device->createBindingSet(bindingSetDesc, pipeline->GetBindingLayout(0));

        cmd->setBufferState(m_CompositeVertexBuffer->GetHandle(), nvrhi::ResourceStates::VertexBuffer);

        nvrhi::GraphicsState state;
        state.pipeline      = pipeline->GetHandle();
        state.framebuffer   = framebuffer;
        state.vertexBuffers = { nvrhi::VertexBufferBinding{ m_CompositeVertexBuffer->GetHandle(), 0, 0 } };
        state.viewport      = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
        state.bindings      = { bindingSet };
        cmd->setGraphicsState(state);

        nvrhi::DrawArguments args;
        args.instanceCount = 1;
        args.vertexCount   = 6;
        cmd->draw(args);
    }

    void AssetSceneRenderer::AddAssetPin(AssetHandle handle)
    {
        m_PinnedAssetHandles.push_back(handle);
        AssetManager::GetInstance()->AddAssetPin(handle, BuildAssetPinName(handle));
    }

    std::string_view AssetSceneRenderer::BuildAssetPinName(AssetHandle handle)
    {
        return std::format("asset_scene_renderer_", (uint64_t)handle);
    }

}
