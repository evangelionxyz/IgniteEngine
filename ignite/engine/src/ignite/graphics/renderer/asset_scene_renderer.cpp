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
#include "ignite/graphics/binding_cache.hpp"
#include "ignite/project/project.hpp"
#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_renderer.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/procedural_sky.hpp"
#include "ignite/graphics/bindless_system.hpp"

#include "ignite/scene/icamera.hpp"

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
        nri::SamplerDesc samplerDesc = {};
        samplerDesc.filters = { nri::Filter::NEAREST, nri::Filter::NEAREST, nri::Filter::NEAREST };
        samplerDesc.addressModes = { nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
        samplerDesc.mipMax = 16.0f;
        DeviceManager::GetInstance()->GetCoreInterface().CreateSampler(*m_Device, samplerDesc, m_CompositeSampler);

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
        m_StaticMeshBindingSets.clear();
        m_AnimatedBindingSets.clear();

        m_StaticGeometryPipelineCache.clear();
        m_StaticTransparentPipelineCache.clear();
        m_SkeletalGeometryPipelineCache.clear();
        m_SkeletalTransparentPipelineCache.clear();
        m_EnvironmentPipelineCache.clear();
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

    void AssetSceneRenderer::EnsureBindingSets(FrameContext *frameContext)
    {
        if (!frameContext)
            return;

        if (m_StaticMeshBindingSets.find(frameContext) != m_StaticMeshBindingSets.end())
            return;

        if (frameContext->staticMeshBindingSet && frameContext->animatedBindingSet)
        {
            m_StaticMeshBindingSets[frameContext] = frameContext->staticMeshBindingSet;
            m_AnimatedBindingSets[frameContext] = frameContext->animatedBindingSet;
            return;
        }

        BindingSetDesc staticDesc;
        staticDesc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
        staticDesc.AddItem(BindingSetItem::ConstantBuffer(1, frameContext->cameraBuffer.GetDescriptor()));
        staticDesc.AddItem(BindingSetItem::StructuredBuffer_SRV(2, frameContext->objectBuffer.GetDescriptor()));
        staticDesc.AddItem(BindingSetItem::StructuredBuffer_SRV(3, frameContext->instanceIndexBuffer.GetDescriptor()));
        staticDesc.AddItem(BindingSetItem::ConstantBuffer(4, frameContext->sceneBuffer.GetDescriptor()));
        staticDesc.AddItem(BindingSetItem::ConstantBuffer(5, frameContext->csmBuffer.GetDescriptor()));
        staticDesc.AddItem(BindingSetItem::ConstantBuffer(6, frameContext->pointLightBuffer.GetDescriptor()));
        staticDesc.AddItem(BindingSetItem::ConstantBuffer(7, frameContext->spotLightBuffer.GetDescriptor()));
        m_StaticMeshBindingSets[frameContext] = BindingSet::Create(staticDesc, Renderer::GetPipelineLayout(EBindingLayout::MESH_STATIC), 0);

        BindingSetDesc animDesc;
        animDesc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
        animDesc.AddItem(BindingSetItem::ConstantBuffer(1, frameContext->cameraBuffer.GetDescriptor()));
        animDesc.AddItem(BindingSetItem::StructuredBuffer_SRV(2, frameContext->objectBuffer.GetDescriptor()));
        animDesc.AddItem(BindingSetItem::StructuredBuffer_SRV(3, frameContext->boneBuffer.GetDescriptor()));
        animDesc.AddItem(BindingSetItem::ConstantBuffer(4, frameContext->sceneBuffer.GetDescriptor()));
        animDesc.AddItem(BindingSetItem::ConstantBuffer(5, frameContext->csmBuffer.GetDescriptor()));
        animDesc.AddItem(BindingSetItem::ConstantBuffer(6, frameContext->pointLightBuffer.GetDescriptor()));
        animDesc.AddItem(BindingSetItem::ConstantBuffer(7, frameContext->spotLightBuffer.GetDescriptor()));
        m_AnimatedBindingSets[frameContext] = BindingSet::Create(animDesc, Renderer::GetPipelineLayout(EBindingLayout::MESH_ANIM), 0);
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

    void AssetSceneRenderer::Render(ICamera *camera, FrameContext *frameContext, const Ref<RenderTarget> &sceneRT, const Ref<RenderTarget> &uiRT, const Ref<RenderTarget> &compositeRT)
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

        nri::CommandBuffer *cmd = BeginCommandBuffer();

        // Reload environment
        if (!m_EnvironmentTextureLoadAttempted && m_UseEnvironment)
        {
            m_EnvironmentTextureLoadAttempted = true;
            if (!m_DefaultEnvTexture && m_EnvTexHandle == AssetHandle(0))
            {
                TextureCreateInfo textureCI;
                textureCI.dimension       = nri::TextureType::TEXTURE_2D;
                textureCI.format          = nri::Format::RGBA32_SFLOAT;
                textureCI.flip            = true;
                textureCI.keepInitialState = true;
                textureCI.initialLayout   = nri::Layout::SHADER_RESOURCE;
                textureCI.initialAccess   = nri::AccessBits::SHADER_RESOURCE;
                m_DefaultEnvTexture       = Texture::Create("resources/hdr/snowy_field_2k.hdr", textureCI, cmd, "Asset Preview HDR");
                m_DefaultEnvTexture->SetReadyFlag(true);
            }

            Ref<Texture> envTex;
            if (m_Environment)
            {
                envTex = (m_EnvTexHandle != AssetHandle(0))
                    ? AssetManager::GetInstance()->GetAsset<Texture>(m_EnvTexHandle)
                    : ((m_DefaultEnvTexture && *m_DefaultEnvTexture) ? m_DefaultEnvTexture : Renderer::GetBlackTexture());

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

        EnsureBindingSets(frameContext);

        CameraBufferData cameraBufferData = { camera->GetProjection(), camera->GetView(), glm::vec4(camera->position, 1.0f) };
        frameContext->cameraBuffer.SetData(cmd, &cameraBufferData, sizeof(CameraBufferData));
        frameContext->sceneBuffer.SetData(cmd, &m_SceneGPUData, sizeof(Scene_GPUData));
        {
            // Cascaded Shadow Map — disabled in preview (shadowStrength = 0)
            CSM_GPUData csmGpuData = {};
            csmGpuData.cascadeIndex = -1;
            csmGpuData.shadowStrength = 0.0f;
            frameContext->csmBuffer.SetData(cmd, &csmGpuData, sizeof(csmGpuData));

            PointLightBufferData pointLightData = {};
            SpotLightBufferData spotLightData = {};
            frameContext->pointLightBuffer.SetData(cmd, &pointLightData, sizeof(pointLightData));
            frameContext->spotLightBuffer.SetData(cmd, &spotLightData, sizeof(spotLightData));
        }

        uiRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.0f));
        uiRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
        uiRT->ClearDepthAttachment(cmd, 1.0f, 0);

        sceneRT->ClearColorAttachmentFloat(cmd, 0, glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
        sceneRT->ClearColorAttachmentUint(cmd, 1, 0xFFFFFFFFu);
        sceneRT->ClearDepthAttachment(cmd, 1.0f, 0);

        if (m_Environment && m_UseEnvironment)
        {
            DrawEnvironment(cmd, camera, sceneRT.get(), frameContext);
        }

        if (hasMeshPreview)
        {
            DrawPreviewStaticMesh(cmd, sceneRT.get(), frameContext);
            DrawPreviewSkeletalMesh(cmd, sceneRT.get(), frameContext);
        }

        if (m_PreviewWidget)
        {
            const uint32_t width  = std::max(1u, uiRT->GetWidth());
            const uint32_t height = std::max(1u, uiRT->GetHeight());

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
            m_WidgetRenderer->Render(cmd, uiRT.get());
        }

        CompositePass(cmd, camera, compositeRT.get(), sceneRT->GetColorAttachment(0), uiRT->GetColorAttachment(0));
        SubmitCommandBuffer(cmd);
    }

    Ref<Texture> AssetSceneRenderer::GetEnvironmentMapColorTexture() const
    {
        if (!m_Environment)
            return m_DefaultEnvTexture ? m_DefaultEnvTexture : nullptr;

        if (m_Environment->GetSkyType() == SkyType::ProceduralSky)
        {
            Ref<ProceduralSky> proceduralSky = m_Environment->GetProceduralSky();
            if (proceduralSky && proceduralSky->GetSkyViewLUT())
                return proceduralSky->GetSkyViewLUT();
        }

        if (m_Environment->GetHDRTexture())
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
    void AssetSceneRenderer::DrawEnvironment(nri::CommandBuffer *cmd, [[maybe_unused]] ICamera *camera, RenderTarget *rt, FrameContext *frameContext)
    {
        Ref<GraphicsPipeline> envPipeline;
        if (auto it = m_EnvironmentPipelineCache.find(rt); it != m_EnvironmentPipelineCache.end())
        {
            envPipeline = it->second;
        }
        else
        {
            const bool hasDepthAttachment = rt->HasDepthAttachment();

            GraphicsPipelineParams params;
            params.enableBlend        = true;
            params.enableDepthWrite   = hasDepthAttachment;
            params.enableDepthTest    = hasDepthAttachment;
            params.enableDepthStencil = false;
            params.fillMode           = nri::FillMode::SOLID;
            params.cullMode           = nri::CullMode::FRONT;
            params.depthFunc          = nri::CompareOp::ALWAYS;

            Ref<Shader> vertexShader = Shader::Create("resources/shaders/skybox.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
            Ref<Shader> pixelShader  = Shader::Create("resources/shaders/skybox.pixel.hlsl",  UMBRA_SHADER_TYPE_PIXEL,  false);

            envPipeline = GraphicsPipeline::Create("Asset Preview Environment Pipeline");
            envPipeline->SetShaders({ vertexShader, pixelShader })
                .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::ENVIRONMENT))
                .Build(rt, params);

            m_EnvironmentPipelineCache.clear();
            m_EnvironmentPipelineCache[rt] = envPipeline;
        }

        if (m_EnvTextureInvalidating)
        {
            Ref<Texture> tex = m_Environment->GetHDRTexture();
            if (tex && tex->IsReady())
            {
                m_Environment->WriteBuffer(cmd);
                m_EnvTextureInvalidating = false;
            }
        }

        m_Environment->Draw(cmd, rt, envPipeline, frameContext->cameraBuffer, frameContext->sceneBuffer);
    }

    // ---------------------------------------------------------------------------
    // DrawPreviewMeshImpl — generic implementation for static and skeletal meshes.
    //
    // Key per-type differences handled via if constexpr:
    //   Skeletal: applies bone transforms to the per-instance world matrix and uploads the skeleton constant buffer via SetSkeletonData.
    //   Static:   no skeleton data needed.
    //
    // The caller (DrawPreviewStaticMesh / DrawPreviewSkeletalMesh) passes the
    // correct shader paths, binding layout enum, and pipeline cache references so
    // no global state is shared between the two mesh types.
    // ---------------------------------------------------------------------------
    template<typename MeshT>
    void AssetSceneRenderer::DrawPreviewMeshImpl(const Ref<MeshT> &mesh, nri::CommandBuffer *cmd, RenderTarget *rt, FrameContext *frameContext,
        const char *vertexShaderPath, const char *pixelShaderPath, EBindingLayout meshBindingLayout,
        std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> &opaqueCache,
        std::unordered_map<const RenderTarget *, Ref<GraphicsPipeline>> &transparentCache)
    {
        constexpr bool isSkeletal = std::is_same_v<MeshT, SkeletalMesh>;

        static_assert(std::is_same_v<MeshT, StaticMesh> || std::is_same_v<MeshT, SkeletalMesh>,
            "DrawPreviewMeshImpl: MeshT must be StaticMesh or SkeletalMesh");

        if (!mesh || !m_RuntimeMaterial || !rt)
            return;

        // Update the shared runtime material binding set.
        // This is always required — even when per-instance materials are present,
        // the runtime material is the fallback for any instance with no assigned material.
        if (!m_RuntimeMaterial->UpdateBindingSet(GetEnvironmentMapColorTexture(), GetCascadedShadowMapDepthTexture()))
            return;

        m_RuntimeMaterial->UploadToGpu(cmd);

        for (auto &meshInstance : mesh->GetMeshInstances())
        {
            auto &primitive = meshInstance->GetPrimitive();
            if (primitive && (!primitive->vertexBuffer || !primitive->indexBuffer))
                primitive->WriteBuffer(cmd);
        }

        // Pre-build bone matrix array (only for skeletal meshes)
        glm::mat4 bones[MAX_BONES];
        if constexpr (isSkeletal)
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

        uint32_t boneOffset = 0;
        if constexpr (isSkeletal)
        {
            boneOffset = frameContext->boneAllocator.Allocate(cmd, bones, MAX_BONES);
        }

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        const bool startedRendering = !rt->IsRendering();
        if (startedRendering)
            rt->BeginRendering(cmd, false, false);

        for (auto &meshInstance : mesh->GetMeshInstances())
        {
            auto &primitive = meshInstance->GetPrimitive();
            if (!primitive || !primitive->vertexBuffer || !primitive->indexBuffer)
                continue;

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

            const MaterialType materialType = material ? material->GetType() : m_RuntimeMaterial->GetType();
            const bool isTransparent = materialType == MaterialType::Transparent;
            auto &pipelineCache = isTransparent ? transparentCache : opaqueCache;

            Ref<GraphicsPipeline> geopPipeline;
            if (auto it = pipelineCache.find(rt); it != pipelineCache.end())
            {
                geopPipeline = it->second;
            }
            else
            {
                GraphicsPipelineParams params;
                params.enableDepthTest = true;
                params.enableDepthStencil = false;
                params.fillMode = nri::FillMode::SOLID;
                params.cullMode = nri::CullMode::NONE;
                params.depthFunc = nri::CompareOp::LESS_EQUAL;
                params.enableBlend = isTransparent;

                if (isTransparent)
                {
                    params.srcBlend = nri::BlendFactor::SRC_ALPHA;
                    params.destBlend = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
                    params.srcBlendAlpha = nri::BlendFactor::ONE;
                    params.destBlendAlpha = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
                    params.enableDepthWrite = false;
                }
                else
                {
                    params.enableDepthWrite = true;
                }

                Ref<Shader> vertexShader = Shader::Create(vertexShaderPath, UMBRA_SHADER_TYPE_VERTEX, false);
                Ref<Shader> pixelShader = Shader::Create(pixelShaderPath, UMBRA_SHADER_TYPE_PIXEL, false);

                geopPipeline = GraphicsPipeline::Create("Asset Preview Mesh Pipeline");
                geopPipeline->SetShaders({ vertexShader, pixelShader })
                    .AddBindingLayout(Renderer::GetBindingLayout(meshBindingLayout))
                    .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MATERIAL))
                    .AddBindingLayout(BindlessSystem::GetBindingLayout())
                    .Build(rt, params);

                pipelineCache.clear();
                pipelineCache[rt] = geopPipeline;
            }

            // Build per-instance GPU transform
            Mesh_GPUData gpuData;
            glm::mat4 meshTransform = meshInstance->global;

            if constexpr (isSkeletal)
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
            gpuData.objectID = 0xFFFFFFFFu;
            gpuData.boneOffset = boneOffset;

            // Allocate a unique object ID for this mesh instance and store it in the GPU data
            const uint32_t PushConstant_ObjectIndex = frameContext->objectAllocator.Allocate(cmd, gpuData);

            uint32_t baseOffset = 0;
            if constexpr (!isSkeletal)
            {
                baseOffset = frameContext->instanceIndexAllocator.Allocate(cmd, &PushConstant_ObjectIndex, 1);
            }

            EnsureBindingSets(frameContext);

            Ref<BindingSet> meshBindingSet = m_StaticMeshBindingSets[frameContext];
            if constexpr (isSkeletal)
            {
                meshBindingSet = m_AnimatedBindingSets[frameContext];
            }

            const Ref<BindingSet> materialBindingSet = (material && material->GetBindingSet())
                ? material->GetBindingSet()
                : m_RuntimeMaterial->GetBindingSet();

            if (meshBindingSet && materialBindingSet && geopPipeline && geopPipeline->GetHandle())
            {
                nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *geopPipeline->GetPipelineLayout());
                nri.CmdSetPipeline(*cmd, *geopPipeline->GetHandle());

                nri::SetDescriptorSetDesc set0 = { 0, meshBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set1 = { 1, materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
                nri.CmdSetDescriptorSet(*cmd, set0);
                nri.CmdSetDescriptorSet(*cmd, set1);
                nri.CmdSetDescriptorSet(*cmd, set2);

                constexpr uint32_t vertexStride = isSkeletal ? static_cast<uint32_t>(sizeof(VertexMeshAnim)) : static_cast<uint32_t>(sizeof(VertexMeshStatic));
                nri::VertexBufferDesc vbDesc = {};
                vbDesc.buffer = primitive->vertexBuffer->GetHandle();
                vbDesc.offset = 0;
                vbDesc.stride = vertexStride;
                nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                nri.CmdSetIndexBuffer(*cmd, *primitive->indexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

                // Push the object ID to the shader via push constants
                const uint32_t rootConstantVal = isSkeletal ? PushConstant_ObjectIndex : baseOffset;
                nri::SetRootConstantsDesc rootConstDesc = {};
                rootConstDesc.rootConstantIndex = 0;
                rootConstDesc.data = &rootConstantVal;
                rootConstDesc.size = sizeof(uint32_t);
                rootConstDesc.offset = 0;
                rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
                nri.CmdSetRootConstants(*cmd, rootConstDesc);

                nri::DrawIndexedDesc drawDesc = {};
                drawDesc.indexNum = primitive->indexBuffer->GetCount();
                drawDesc.instanceNum = 1;
                nri.CmdDrawIndexed(*cmd, drawDesc);
            }
        }

        if (startedRendering)
            rt->EndRendering(cmd);
    }

    // ---------------------------------------------------------------------------
    // DrawPreviewStaticMesh — delegates to the generic template.
    // Uses the static-mesh shaders and its own pipeline cache pair so it never
    // collides with the skeletal-mesh pipeline cache.
    // ---------------------------------------------------------------------------
    void AssetSceneRenderer::DrawPreviewStaticMesh(nri::CommandBuffer *cmd, RenderTarget *rt, FrameContext *frameContext)
    {
        DrawPreviewMeshImpl(m_PreviewStaticMesh, cmd, rt, frameContext,
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
    void AssetSceneRenderer::DrawPreviewSkeletalMesh(nri::CommandBuffer *cmd, RenderTarget *rt, FrameContext *frameContext)
    {
        DrawPreviewMeshImpl(m_PreviewSkeletalMesh, cmd, rt, frameContext,
            "resources/shaders/mesh_anim.vertex.hlsl",
            "resources/shaders/mesh_anim.pixel.hlsl",
            EBindingLayout::MESH_ANIM,
            m_SkeletalGeometryPipelineCache,
            m_SkeletalTransparentPipelineCache);
    }

    // ---------------------------------------------------------------------------
    // CompositePass
    // ---------------------------------------------------------------------------
    void AssetSceneRenderer::CompositePass(nri::CommandBuffer *cmd, ICamera *camera, RenderTarget *rt, Ref<Texture> sceneTexture, Ref<Texture> uiTexture)
    {
        if (!cmd || !rt || !sceneTexture || !uiTexture)
            return;

        m_PostProcessingData.flags.x = 0.0f;
        m_PostProcessingData.flags.y = m_PostProcessing.bloomIntensity;
        m_PostProcessingData.flags.z = m_PostProcessing.enableVignette ? 1.0f : 0.0f;
        m_PostProcessingData.flags.w = m_PostProcessing.enableChromAb ? 1.0f : 0.0f;
        m_PostProcessingData.tonemapMode = static_cast<int>(m_PostProcessing.tonemapMode);
        m_PostProcessingData.vignetteParams = glm::vec4(
            m_PostProcessing.vignetteRadius,
            glm::max(m_PostProcessing.vignetteSoftness, 0.001f),
            m_PostProcessing.vignetteIntensity,
            m_PostProcessing.chromAbAmount
        );
        m_PostProcessingData.chromAbParams = glm::vec4(m_PostProcessing.chromAbRadial, 0.0f, m_PostProcessing.aoIntensity, 0.0f);
        m_PostProcessingData.vignetteColor = glm::vec4(m_PostProcessing.vignetteColor, 1.0f);
        m_PostProcessingData.taaParams = glm::vec4(0.0f);
        m_PostProcessingData.enableDOF = camera && camera->lens.enabledDOF ? 1 : 0;
        if (camera)
        {
            m_PostProcessingData.projectionInv = glm::inverse(camera->GetProjection());
            m_PostProcessingData.focalLength = camera->lens.focalLength;
            m_PostProcessingData.focalDistance = camera->lens.focalDistance;
            m_PostProcessingData.fStop = camera->lens.fStop;
            m_PostProcessingData.focusRange = camera->lens.focusRange;
            m_PostProcessingData.blurAmount = camera->lens.blurAmount;
        }

        m_CompositePostProcessBuffer.SetData(cmd, &m_PostProcessingData, sizeof(m_PostProcessingData));

        Ref<GraphicsPipeline> pipeline;
        if (auto it = m_CompositePipelineCache.find(rt); it != m_CompositePipelineCache.end())
        {
            pipeline = it->second;
        }
        else
        {
            m_CompositeBindingLayout = BindingLayoutDesc{};
            m_CompositeBindingLayout.SetVisibility(nri::StageBits::ALL);
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(0)); // scene
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(1)); // ui
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(2)); // edge
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(3)); // bloom
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(4)); // ssao
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(5)); // depth
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(6)); // debug
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(7)); // objectID
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::TextureSRV(8)); // TAA history
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::VolatileConstantBuffer(0)); // post-process params
            m_CompositeBindingLayout.AddItem(BindingLayoutItem::Sampler(0));

            GraphicsPipelineParams params;
            params.enableBlend        = true;
            params.enableDepthWrite   = false;
            params.enableDepthTest    = false;
            params.enableDepthStencil = false;
            params.fillMode           = nri::FillMode::SOLID;
            params.cullMode           = nri::CullMode::NONE;

            Ref<Shader> vertexShader = Shader::Create("resources/shaders/composite.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
            Ref<Shader> pixelShader  = Shader::Create("resources/shaders/composite.pixel.hlsl",  UMBRA_SHADER_TYPE_PIXEL,  false);

            pipeline = GraphicsPipeline::Create("Asset Preview Composite Pipeline");
            pipeline->SetShaders({ vertexShader, pixelShader })
                .AddBindingLayout(m_CompositeBindingLayout)
                .Build(rt, params);

            m_CompositePipelineCache.clear();
            m_CompositePipelineCache[rt] = pipeline;
        }

        BindingSetDesc bindingSetDesc;
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(0, sceneTexture->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(1, uiTexture->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(2, Renderer::GetBlackTexture()->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(3, Renderer::GetBlackTexture()->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(4, Renderer::GetWhiteTexture()->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(5, Renderer::GetBlackTexture()->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(6, Renderer::GetBlackTexture()->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(7, Renderer::GetBlackUIntTexture()->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::TextureSRV(8, Renderer::GetBlackTexture()->GetShaderResourceView()));
        bindingSetDesc.AddItem(BindingSetItem::ConstantBuffer(0, m_CompositePostProcessBuffer.GetDescriptor()));
        bindingSetDesc.AddItem(BindingSetItem::Sampler(0, m_CompositeSampler));

        Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(bindingSetDesc, pipeline->GetPipelineLayout(), 0);
        if (!bindingSet || !pipeline || !pipeline->GetHandle())
            return;

        const bool startedRendering = !rt->IsRendering();
        if (startedRendering)
            rt->BeginRendering(cmd, false, false);

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *pipeline->GetPipelineLayout());
        nri.CmdSetPipeline(*cmd, *pipeline->GetHandle());

        nri::SetDescriptorSetDesc setDesc = {};
        setDesc.setIndex = 0;
        setDesc.descriptorSet = bindingSet->GetHandle();
        setDesc.bindPoint = nri::BindPoint::GRAPHICS;
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        nri::VertexBufferDesc vbDesc = {};
        vbDesc.buffer = m_CompositeVertexBuffer->GetHandle();
        vbDesc.offset = 0;
        vbDesc.stride = sizeof(VertexScreen);
        nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);

        nri::DrawDesc drawDesc = {};
        drawDesc.vertexNum = 6;
        drawDesc.instanceNum = 1;
        nri.CmdDraw(*cmd, drawDesc);

        if (startedRendering)
            rt->EndRendering(cmd);
    }
}
