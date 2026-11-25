// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "scene_capture_renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/bindless_system.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/renderer/batch_builder.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/procedural_sky.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ignite
{
    namespace
    {
        constexpr uint32_t kMaxCaptureObjects = 8192;
        constexpr uint32_t kMaxCaptureInstanceIndices = 32768;

        AABB TransformAABB(const AABB &localBox, const glm::mat4 &transform)
        {
            const glm::vec3 corners[8] = {
                { localBox.min.x, localBox.min.y, localBox.min.z },
                { localBox.max.x, localBox.min.y, localBox.min.z },
                { localBox.min.x, localBox.max.y, localBox.min.z },
                { localBox.max.x, localBox.max.y, localBox.min.z },
                { localBox.min.x, localBox.min.y, localBox.max.z },
                { localBox.max.x, localBox.min.y, localBox.max.z },
                { localBox.min.x, localBox.max.y, localBox.max.z },
                { localBox.max.x, localBox.max.y, localBox.max.z }
            };

            glm::vec3 wMin(std::numeric_limits<float>::max());
            glm::vec3 wMax(std::numeric_limits<float>::lowest());
            for (const glm::vec3 &c : corners)
            {
                const glm::vec3 wc = glm::vec3(transform * glm::vec4(c, 1.0f));
                wMin = glm::min(wMin, wc);
                wMax = glm::max(wMax, wc);
            }
            return AABB::FromMinMax(wMin, wMax);
        }
    }

    SceneCaptureRenderer::SceneCaptureRenderer() = default;

    SceneCaptureRenderer::~SceneCaptureRenderer()
    {
        auto *dm = DeviceManager::GetInstance();
        if (dm)
        {
            const auto &nri = dm->GetCoreInterface();
            GPUUploadSync::DeviceWaitIdle(dm->GetDevice());

            if (m_SyncCmdBuffer)
            {
                nri.DestroyCommandBuffer(m_SyncCmdBuffer);
                m_SyncCmdBuffer = nullptr;
            }
            if (m_SyncCmdAllocator)
            {
                nri.DestroyCommandAllocator(m_SyncCmdAllocator);
                m_SyncCmdAllocator = nullptr;
            }
            if (m_SyncFence)
            {
                nri.DestroyFence(m_SyncFence);
                m_SyncFence = nullptr;
            }
        }

        m_MeshPSOCache.clear();
        m_ShadowPSO.reset();
        m_LocalShadowPSO.reset();
        m_SkyPSO.reset();
        m_ShadowBindingSet.reset();

        for (auto &rt : m_LocalShadowFaceRTs) rt.reset();
        for (auto &bs : m_LocalShadowFaceBindingSets) bs.reset();
        for (auto &cb : m_LocalShadowFaceCameraBuffers) cb.reset();
        m_BakeLocalShadowCubeArray.reset();
        m_LocalShadowDepthTex.reset();

        for (auto &slot : m_CaptureSlots)
        {
            for (auto &bs : slot.staticFaceBindingSets)
            {
                bs.reset();
            }
            for (auto &cb : slot.faceCameraBuffers)
            {
                cb.reset();
            }
            slot.sceneBuffer.reset();
            slot.giBuffer.reset();
        }

        m_PointLightBuffer.reset();
        m_SpotLightBuffer.reset();
        m_ShadowSceneBuffer.reset();
        m_ShadowCameraBuffer.reset();
        m_ShadowGIBuffer.reset();

        m_BakeShadowMap.reset();
        for (auto &rt : m_FaceRenderTargets)
        {
            rt.reset();
        }
        m_ColorCube.reset();
        m_DepthCube.reset();
        m_ScratchIDTexture.reset();

        m_PreparedBatches.clear();
        m_RetainedMaterials.clear();

        if (m_CommonInitialized)
        {
            m_ObjectBuffer.Shutdown();
            m_InstanceIndexBuffer.Shutdown();
            m_ReflectionProbeBuffer.Shutdown();
            m_CommonInitialized = false;
        }
    }

    void SceneCaptureRenderer::GetCubemapFaceViewProj(const glm::vec3 &pos, uint32_t face, float nearPlane, float farPlane,
        glm::mat4 &outView, glm::mat4 &outProj)
    {
        static constexpr std::array<glm::vec3, 6> kTargets = {
            glm::vec3( 1.0f,  0.0f,  0.0f), // +X
            glm::vec3(-1.0f,  0.0f,  0.0f), // -X
            glm::vec3( 0.0f,  1.0f,  0.0f), // +Y
            glm::vec3( 0.0f, -1.0f,  0.0f), // -Y
            glm::vec3( 0.0f,  0.0f,  1.0f), // +Z
            glm::vec3( 0.0f,  0.0f, -1.0f), // -Z
        };
        static constexpr std::array<glm::vec3, 6> kUps = {
            glm::vec3(0.0f, -1.0f,  0.0f), // +X
            glm::vec3(0.0f, -1.0f,  0.0f), // -X
            glm::vec3(0.0f,  0.0f,  1.0f), // +Y
            glm::vec3(0.0f,  0.0f, -1.0f), // -Y
            glm::vec3(0.0f, -1.0f,  0.0f), // +Z
            glm::vec3(0.0f, -1.0f,  0.0f), // -Z
        };

        const uint32_t clampedFace = std::min(face, 5u);
        outView = glm::lookAt(pos, pos + kTargets[clampedFace], kUps[clampedFace]);
        outProj = glm::perspectiveZO(glm::radians(90.0f), 1.0f, std::max(nearPlane, 1e-3f), std::max(farPlane, nearPlane + 1.0f));
        // NRI flips VkViewport with negative height by default (originBottomLeft = false); negate proj[1][1]
        // so +Y ends up at top row (v=0) matching Vulkan TextureCube sampling conventions.
        outProj[1][1] *= -1.0f;
    }

    nri::CullMode SceneCaptureRenderer::InvertCullModeForYFlip(nri::CullMode cullMode)
    {
        if (cullMode == nri::CullMode::FRONT)
        {
            return nri::CullMode::BACK;
        }
        if (cullMode == nri::CullMode::BACK)
        {
            return nri::CullMode::FRONT;
        }
        return cullMode;
    }

    void SceneCaptureRenderer::EnsureCommonGPUResources()
    {
        if (m_CommonInitialized)
        {
            return;
        }

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
        {
            return;
        }
        nri::Device *device = dm->GetDevice();

        m_ObjectBuffer.Initialize(kMaxCaptureObjects, device);
        m_InstanceIndexBuffer.Initialize(kMaxCaptureInstanceIndices, device);
        m_ReflectionProbeBuffer.Initialize(MAX_REFLECTION_PROBES, device);

        m_ObjectAllocator.SetBuffer(m_ObjectBuffer);
        m_InstanceIndexAllocator.SetBuffer(m_InstanceIndexBuffer);

        std::array<ReflectionProbe_GPUData, MAX_REFLECTION_PROBES> emptyProbes{};
        m_ReflectionProbeBuffer.SetData(emptyProbes.data(), 0);

        m_PointLightBuffer = ConstantBuffer::Create(sizeof(PointLightBufferData), true, 1, "[SceneCapture PointLights]");
        m_SpotLightBuffer = ConstantBuffer::Create(sizeof(SpotLightBufferData), true, 1, "[SceneCapture SpotLights]");
        m_ShadowSceneBuffer = ConstantBuffer::Create(sizeof(Scene_GPUData), true, 1, "[SceneCapture ShadowScene]");
        m_ShadowCameraBuffer = ConstantBuffer::Create(sizeof(CameraBufferData), true, 1, "[SceneCapture ShadowCam]");
        m_ShadowGIBuffer = ConstantBuffer::Create(sizeof(GI_GPUData), true, 1, "[SceneCapture ShadowGI]");

        GI_GPUData defaultGI{};
        m_ShadowGIBuffer->SetData(&defaultGI, sizeof(defaultGI));

        m_BakeShadowMap = CreateRef<CascadedShadowMap>(ShadowMapQuality::ULTRA);

        nri::PipelineLayout *staticLayout = Renderer::GetPipelineLayout(EBindingLayout::MESH_STATIC);
        LOG_ASSERT(staticLayout != nullptr, "[SceneCaptureRenderer] MESH_STATIC pipeline layout is null");

        {
            BindingSetDesc desc;
            desc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
            desc.AddItem(BindingSetItem::ConstantBuffer(1, m_ShadowCameraBuffer->GetDescriptor()));
            desc.AddItem(BindingSetItem::StructuredBufferSRV(2, m_ObjectBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::StructuredBufferSRV(3, m_InstanceIndexBuffer.GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(4, m_ShadowSceneBuffer->GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(5, m_BakeShadowMap->GetGPUDataBuffer()->GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(6, m_PointLightBuffer->GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(7, m_SpotLightBuffer->GetDescriptor()));
            desc.AddItem(BindingSetItem::ConstantBuffer(8, m_ShadowGIBuffer->GetDescriptor()));
            desc.AddItem(BindingSetItem::StructuredBufferSRV(9, m_ReflectionProbeBuffer.GetDescriptor()));
            m_ShadowBindingSet = BindingSet::Create("SceneCapture Shadow Set0", staticLayout, 0, desc);
        }

        for (uint32_t slotIdx = 0; slotIdx < kMaxCaptureSlots; ++slotIdx)
        {
            CaptureUniformSlot &slot = m_CaptureSlots[slotIdx];
            slot.sceneBuffer = ConstantBuffer::Create(sizeof(Scene_GPUData), true, 1, "[SceneCapture Scene]");
            slot.giBuffer = ConstantBuffer::Create(sizeof(GI_GPUData), true, 1, "[SceneCapture GI]");

            for (uint32_t face = 0; face < 6; ++face)
            {
                slot.faceCameraBuffers[face] = ConstantBuffer::Create(sizeof(CameraBufferData), true, 1, "[SceneCapture FaceCam]");

                BindingSetDesc desc;
                desc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
                desc.AddItem(BindingSetItem::ConstantBuffer(1, slot.faceCameraBuffers[face]->GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(2, m_ObjectBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(3, m_InstanceIndexBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(4, slot.sceneBuffer->GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(5, m_BakeShadowMap->GetGPUDataBuffer()->GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(6, m_PointLightBuffer->GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(7, m_SpotLightBuffer->GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(8, slot.giBuffer->GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(9, m_ReflectionProbeBuffer.GetDescriptor()));
                slot.staticFaceBindingSets[face] = BindingSet::Create("SceneCapture Face Set0", staticLayout, 0, desc);
            }
        }

        m_CommonInitialized = true;
    }

    void SceneCaptureRenderer::EnsureCaptureTargets(uint32_t resolution)
    {
        resolution = std::max(resolution, 4u);
        if (m_TargetResolution == resolution && m_ColorCube && m_DepthCube && m_FaceRenderTargets[0])
        {
            return;
        }

        for (auto &rt : m_FaceRenderTargets)
        {
            rt.reset();
        }
        m_MeshPSOCache.clear();
        m_SkyPSO.reset();

        m_TargetResolution = resolution;

        TextureCreateInfo colorInfo;
        colorInfo.width = resolution;
        colorInfo.height = resolution;
        colorInfo.depth = 1;
        colorInfo.mipLevels = 1;
        colorInfo.arraySize = 6;
        colorInfo.sampleCount = 1;
        colorInfo.dimension = nri::TextureType::TEXTURE_2D;
        colorInfo.format = nri::Format::RGBA16_SFLOAT;
        colorInfo.isCubeMap = true;
        colorInfo.isRenderTarget = true;
        colorInfo.isUAV = false;
        colorInfo.bindless = false;
        m_ColorCube = Texture::Create(colorInfo, "[SceneCapture ColorCube]");

        TextureCreateInfo depthInfo;
        depthInfo.width = resolution;
        depthInfo.height = resolution;
        depthInfo.depth = 1;
        depthInfo.mipLevels = 1;
        depthInfo.arraySize = 6;
        depthInfo.sampleCount = 1;
        depthInfo.dimension = nri::TextureType::TEXTURE_2D;
        depthInfo.format = nri::Format::D32_SFLOAT;
        depthInfo.isCubeMap = true;
        depthInfo.isRenderTarget = true;
        depthInfo.isUAV = false;
        depthInfo.bindless = false;
        m_DepthCube = Texture::Create(depthInfo, "[SceneCapture DepthCube]");

        TextureCreateInfo idInfo;
        idInfo.width = resolution;
        idInfo.height = resolution;
        idInfo.depth = 1;
        idInfo.mipLevels = 1;
        idInfo.arraySize = 1;
        idInfo.sampleCount = 1;
        idInfo.dimension = nri::TextureType::TEXTURE_2D;
        idInfo.format = nri::Format::R32_UINT;
        idInfo.isCubeMap = false;
        idInfo.isRenderTarget = true;
        idInfo.isUAV = false;
        idInfo.bindless = false;
        m_ScratchIDTexture = Texture::Create(idInfo, "[SceneCapture ScratchID]");

        for (uint32_t face = 0; face < 6; ++face)
        {
            RenderTargetCreateInfo rtInfo;
            rtInfo.width = resolution;
            rtInfo.height = resolution;
            rtInfo.sampleCount = 1;
            rtInfo.attachments = {
                FramebufferAttachments{ "CaptureColor", nri::Format::RGBA16_SFLOAT, nri::Layout::COLOR_ATTACHMENT, 0, face, 1 },
                FramebufferAttachments{ "CaptureID", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT, 0, 0, 1 },
                FramebufferAttachments{ "CaptureDepth", nri::Format::D32_SFLOAT, nri::Layout::DEPTH_STENCIL_ATTACHMENT, 0, face, 1 }
            };
            rtInfo.colorAttachmentOverrides = { m_ColorCube, m_ScratchIDTexture };
            rtInfo.depthAttachmentOverride = m_DepthCube;

            m_FaceRenderTargets[face] = RenderTarget::Create(rtInfo, "[SceneCapture FaceRT]");
            m_FaceRenderTargets[face]->SetClearColorAttachmentFloat(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), 0);
            m_FaceRenderTargets[face]->SetClearColorAttachmentUint(0xFFFFFFFFu, 1);
            m_FaceRenderTargets[face]->SetClearDepthAttachment(1.0f, 0);
        }
    }

    void SceneCaptureRenderer::EnsureSyncCommandResources()
    {
        if (m_SyncCmdBuffer && m_SyncFence)
        {
            return;
        }

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !dm->GetGraphicsQueue())
        {
            return;
        }

        const auto &nri = dm->GetCoreInterface();
        nri.CreateCommandAllocator(*dm->GetGraphicsQueue(), m_SyncCmdAllocator);
        nri.CreateCommandBuffer(*m_SyncCmdAllocator, m_SyncCmdBuffer);
        nri::FenceDesc fenceDesc = {};
        nri.CreateFence(*dm->GetDevice(), fenceDesc, m_SyncFence);
    }

    Ref<GraphicsPipeline> SceneCaptureRenderer::GetOrCreateMeshPSO(nri::CullMode cullMode)
    {
        const uint8_t key = static_cast<uint8_t>(cullMode);
        if (auto it = m_MeshPSOCache.find(key); it != m_MeshPSOCache.end())
        {
            return it->second;
        }

        if (!m_FaceRenderTargets[0])
        {
            return nullptr;
        }

        GraphicsPipelineParams params;
        params.enableBlend = false;
        params.enableDepthTest = true;
        params.enableDepthWrite = true;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = cullMode;
        params.depthFunc = nri::CompareOp::LESS_EQUAL;

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/mesh_static.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/mesh_static.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create("SceneCapture StaticMesh PSO");
        pipeline->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MESH_STATIC))
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MATERIAL))
            .AddBindingLayout(BindlessSystem::GetBindingLayout())
            .Build(m_FaceRenderTargets[0].get(), params);

        m_MeshPSOCache.emplace(key, pipeline);
        return pipeline;
    }

    Ref<GraphicsPipeline> SceneCaptureRenderer::GetOrCreateShadowPSO()
    {
        if (m_ShadowPSO)
        {
            return m_ShadowPSO;
        }

        if (!m_BakeShadowMap)
        {
            return nullptr;
        }

        RenderTarget *cascadeRT = m_BakeShadowMap->GetCascadeFramebuffer(0, 0);
        if (!cascadeRT)
        {
            return nullptr;
        }

        GraphicsPipelineParams params;
        params.enableBlend = false;
        params.enableDepthTest = true;
        params.enableDepthWrite = true;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;
        params.depthFunc = nri::CompareOp::LESS_EQUAL;

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/csm_static.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/csm.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

        m_ShadowPSO = GraphicsPipeline::Create("SceneCapture Shadow PSO");
        m_ShadowPSO->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MESH_STATIC))
            .Build(cascadeRT, params);

        return m_ShadowPSO;
    }

    void SceneCaptureRenderer::EnsureLocalShadowLightResources(uint32_t lightSlot)
    {
        if (!m_CommonInitialized || lightSlot >= kMaxBakeLocalShadowLights)
        {
            return;
        }

        if (!m_BakeLocalShadowCubeArray)
        {
            TextureCreateInfo cubeInfo;
            cubeInfo.width = kBakeLocalShadowResolution;
            cubeInfo.height = kBakeLocalShadowResolution;
            cubeInfo.depth = 1;
            cubeInfo.mipLevels = 1;
            cubeInfo.arraySize = kMaxBakeLocalShadowLights * 6;
            cubeInfo.sampleCount = 1;
            cubeInfo.dimension = nri::TextureType::TEXTURE_2D;
            cubeInfo.format = nri::Format::R32_SFLOAT;
            cubeInfo.isCubeMap = true;
            cubeInfo.isRenderTarget = true;
            cubeInfo.isUAV = false;
            cubeInfo.bindless = true;
            cubeInfo.forceBindless = true;
            cubeInfo.samplerLinearFiltering = true;
            cubeInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
            cubeInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
            cubeInfo.samplerAddressW = nri::AddressMode::CLAMP_TO_EDGE;
            m_BakeLocalShadowCubeArray = Texture::Create(cubeInfo, "[SceneCapture LocalShadowCubeArray]");
        }

        if (!m_LocalShadowDepthTex)
        {
            TextureCreateInfo depthInfo;
            depthInfo.width = kBakeLocalShadowResolution;
            depthInfo.height = kBakeLocalShadowResolution;
            depthInfo.depth = 1;
            depthInfo.mipLevels = 1;
            depthInfo.arraySize = 1;
            depthInfo.sampleCount = 1;
            depthInfo.dimension = nri::TextureType::TEXTURE_2D;
            depthInfo.format = nri::Format::D32_SFLOAT;
            depthInfo.isCubeMap = false;
            depthInfo.isRenderTarget = true;
            depthInfo.isUAV = false;
            depthInfo.bindless = false;
            m_LocalShadowDepthTex = Texture::Create(depthInfo, "[SceneCapture LocalShadowDepth]");
        }

        if (!m_BakeLocalShadowCubeArray || !m_LocalShadowDepthTex)
        {
            return;
        }

        nri::PipelineLayout *staticLayout = Renderer::GetPipelineLayout(EBindingLayout::MESH_STATIC);
        if (!staticLayout)
        {
            return;
        }

        for (uint32_t face = 0; face < 6; ++face)
        {
            const uint32_t slice = lightSlot * 6u + face;
            if (!m_LocalShadowFaceRTs[slice])
            {
                RenderTargetCreateInfo rtInfo;
                rtInfo.width = kBakeLocalShadowResolution;
                rtInfo.height = kBakeLocalShadowResolution;
                rtInfo.sampleCount = 1;
                rtInfo.attachments = {
                    FramebufferAttachments{ "LocalShadowDist", nri::Format::R32_SFLOAT, nri::Layout::COLOR_ATTACHMENT, 0, slice, 1 },
                    FramebufferAttachments{ "LocalShadowDepth", nri::Format::D32_SFLOAT, nri::Layout::DEPTH_STENCIL_ATTACHMENT, 0, 0, 1 }
                };
                rtInfo.colorAttachmentOverrides = { m_BakeLocalShadowCubeArray };
                rtInfo.depthAttachmentOverride = m_LocalShadowDepthTex;

                m_LocalShadowFaceRTs[slice] = RenderTarget::Create(rtInfo, "[SceneCapture LocalShadowFaceRT]");
                m_LocalShadowFaceRTs[slice]->SetClearColorAttachmentFloat(glm::vec4(1.0f), 0);
                m_LocalShadowFaceRTs[slice]->SetClearDepthAttachment(1.0f, 0);
            }

            if (!m_LocalShadowFaceCameraBuffers[slice])
            {
                m_LocalShadowFaceCameraBuffers[slice] = ConstantBuffer::Create(sizeof(CameraBufferData), true, 1, "[SceneCapture LocalShadowCam]");
            }

            if (!m_LocalShadowFaceBindingSets[slice] && m_LocalShadowFaceCameraBuffers[slice])
            {
                BindingSetDesc desc;
                desc.AddItem(BindingSetItem::PushConstants(0, sizeof(uint32_t)));
                desc.AddItem(BindingSetItem::ConstantBuffer(1, m_LocalShadowFaceCameraBuffers[slice]->GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(2, m_ObjectBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(3, m_InstanceIndexBuffer.GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(4, m_ShadowSceneBuffer->GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(5, m_BakeShadowMap->GetGPUDataBuffer()->GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(6, m_PointLightBuffer->GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(7, m_SpotLightBuffer->GetDescriptor()));
                desc.AddItem(BindingSetItem::ConstantBuffer(8, m_ShadowGIBuffer->GetDescriptor()));
                desc.AddItem(BindingSetItem::StructuredBufferSRV(9, m_ReflectionProbeBuffer.GetDescriptor()));
                m_LocalShadowFaceBindingSets[slice] = BindingSet::Create("SceneCapture LocalShadow Set0", staticLayout, 0, desc);
            }
        }
    }

    Ref<GraphicsPipeline> SceneCaptureRenderer::GetOrCreateLocalShadowPSO()
    {
        if (m_LocalShadowPSO)
        {
            return m_LocalShadowPSO;
        }

        RenderTarget *sampleRT = nullptr;
        for (const auto &rt : m_LocalShadowFaceRTs)
        {
            if (rt)
            {
                sampleRT = rt.get();
                break;
            }
        }
        if (!sampleRT)
        {
            return nullptr;
        }

        GraphicsPipelineParams params;
        params.enableBlend = false;
        params.enableDepthTest = true;
        params.enableDepthWrite = true;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;
        params.depthFunc = nri::CompareOp::LESS_EQUAL;

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/gi_point_shadow.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/gi_point_shadow.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

        m_LocalShadowPSO = GraphicsPipeline::Create("SceneCapture LocalShadow PSO");
        m_LocalShadowPSO->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::MESH_STATIC))
            .Build(sampleRT, params);

        return m_LocalShadowPSO;
    }

    Ref<GraphicsPipeline> SceneCaptureRenderer::GetOrCreateSkyPSO()
    {
        if (m_SkyPSO)
        {
            return m_SkyPSO;
        }

        if (!m_FaceRenderTargets[0])
        {
            return nullptr;
        }

        GraphicsPipelineParams params;
        params.enableBlend = false;
        params.enableDepthTest = true;
        params.enableDepthWrite = false;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;
        params.depthFunc = nri::CompareOp::ALWAYS;

        Ref<Shader> vertexShader = Shader::Create("resources/shaders/skybox.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
        Ref<Shader> pixelShader = Shader::Create("resources/shaders/skybox.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

        m_SkyPSO = GraphicsPipeline::Create("SceneCapture Sky PSO");
        m_SkyPSO->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(Renderer::GetBindingLayout(EBindingLayout::ENVIRONMENT))
            .Build(m_FaceRenderTargets[0].get(), params);

        return m_SkyPSO;
    }

    void SceneCaptureRenderer::PrepareScene(Scene *scene, AssetManager *assetManager, nri::CommandBuffer *cmd, const GI_GPUData *giDataOverride)
    {
        IGN_PROFILE_FUNCTION();

        m_PreparedBatches.clear();
        m_RetainedMaterials.clear();
        m_ActiveEnvironment.reset();
        m_PreparedEnvTexture.reset();
        m_HasPreparedSkyIBL = false;
        m_HasDirectionalLight = false;
        m_HasBakeLocalShadows = false;
        m_HasOverrideGIData = (giDataOverride != nullptr);
        if (giDataOverride)
        {
            m_OverrideGIData = *giDataOverride;
        }

        if (!scene || !cmd)
        {
            return;
        }

        EnsureCommonGPUResources();
        if (!m_CommonInitialized)
        {
            return;
        }

        m_ObjectAllocator.BeginFrame();
        m_InstanceIndexAllocator.BeginFrame();

        // -------------------------------------------------------------------
        // 1. Collect directional, point, and spot lights (respecting bakeIndirect)
        // -------------------------------------------------------------------
        m_BaseSceneData = Scene_GPUData{};
        m_BaseSceneData.exposure = 1.0f;
        m_BaseSceneData.gamma = 2.2f;
        m_BaseSceneData.ambient = 0.0f;
        m_BaseSceneData.enableIBL = 0;
        m_BaseSceneData.renderMode = RENDER_MODE_COLOR;
        m_BaseSceneData.debugShadow = 0;

        m_SunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
        bool hasShadowedSun = false;
        float sunShadowStrength = 0.0f;
        float sunMinBias = 0.0002f;
        float sunMaxBias = 0.002f;
        float sunPcfRadius = 1.5f;
        bool sunSoftShadows = true;

        struct LocalShadowLight
        {
            uint32_t slot = 0;
            glm::vec3 position = glm::vec3(0.0f);
            float range = 1.0f;
        };
        std::vector<LocalShadowLight> localShadowLights;

        if (scene->registry)
        {
            auto dirLightView = scene->registry->view<TransformComponent, DirectionalLightComponent>();
            for (auto entity : dirLightView)
            {
                const auto &tr = dirLightView.get<TransformComponent>(entity);
                const auto &light = dirLightView.get<DirectionalLightComponent>(entity);
                if (!light.bakeIndirect)
                {
                    continue;
                }

                glm::vec3 dir = tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                if (glm::dot(dir, dir) > 1e-6f)
                {
                    m_SunDirection = glm::normalize(dir);
                }
                m_HasDirectionalLight = true;
                m_BaseSceneData.sunColor = glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);
                m_BaseSceneData.sungAngles.x = std::atan2(m_SunDirection.x, m_SunDirection.z);
                m_BaseSceneData.sungAngles.y = std::asin(glm::clamp(m_SunDirection.y, -1.0f, 1.0f));
                m_BaseSceneData.sunAngularRadius = glm::radians(light.angularRadius);
                m_BaseSceneData.sunDirection = glm::vec4(m_SunDirection, 0.0f);

                hasShadowedSun = light.cascadeShadow && light.shadowStrength > 0.0f;
                sunShadowStrength = light.shadowStrength;
                sunMinBias = light.shadowMinBias;
                sunMaxBias = light.shadowMaxBias;
                sunPcfRadius = light.pcfRadius;
                sunSoftShadows = light.softShadows;
                break;
            }

            if (!m_HasDirectionalLight)
            {
                m_BaseSceneData.sunColor = glm::vec4(0.0f);
                m_BaseSceneData.sunDirection = glm::vec4(m_SunDirection, 0.0f);
                m_BaseSceneData.sungAngles = glm::vec2(std::atan2(m_SunDirection.x, m_SunDirection.z), std::asin(m_SunDirection.y));
            }

            PointLightBufferData pointData{};
            int pointCount = 0;
            auto pointView = scene->registry->view<TransformComponent, PointLightComponent>();
            for (auto entity : pointView)
            {
                if (pointCount >= MAX_POINT_LIGHTS)
                {
                    break;
                }
                const auto &tr = pointView.get<TransformComponent>(entity);
                const auto &light = pointView.get<PointLightComponent>(entity);
                if (!light.enabled || !light.bakeIndirect)
                {
                    continue;
                }

                const uint32_t pi = static_cast<uint32_t>(pointCount++);
                PointLight_GPUData &gpu = pointData.lights[pi];
                const float clampedRange = std::max(light.range, 0.001f);
                gpu.positionAndRange = glm::vec4(tr.world.translation, clampedRange);
                gpu.color = glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);
                gpu.attenuation = glm::vec4(std::max(light.constantAttenuation, 0.001f), light.linearAttenuation, light.quadraticAttenuation, 0.0f);

                if (pi < kMaxBakePointShadows && light.castShadow)
                {
                    gpu.shadowParams = glm::vec4(static_cast<float>(pi), light.shadowBias,
                        light.softShadows ? light.pcfRadius : 0.0f, light.shadowStrength);
                    localShadowLights.push_back({ pi, tr.world.translation, clampedRange });
                }
                else
                {
                    gpu.shadowParams = glm::vec4(-1.0f, light.shadowBias, 0.0f, 0.0f);
                }
            }
            m_BaseSceneData.numPointLights = pointCount;
            m_PointLightBuffer->SetData(&pointData, sizeof(pointData));

            SpotLightBufferData spotData{};
            int spotCount = 0;
            auto spotView = scene->registry->view<TransformComponent, SpotLightComponent>();
            for (auto entity : spotView)
            {
                if (spotCount >= MAX_SPOT_LIGHTS)
                {
                    break;
                }
                const auto &tr = spotView.get<TransformComponent>(entity);
                const auto &light = spotView.get<SpotLightComponent>(entity);
                if (!light.enabled || !light.bakeIndirect)
                {
                    continue;
                }

                glm::vec3 forward = tr.world.rotation * glm::vec3(0.0f, 0.0f, 1.0f);
                if (glm::dot(forward, forward) > 1e-6f)
                {
                    forward = glm::normalize(forward);
                }
                else
                {
                    forward = glm::vec3(0.0f, 0.0f, 1.0f);
                }

                const uint32_t si = static_cast<uint32_t>(spotCount++);
                SpotLight_GPUData &gpu = spotData.lights[si];
                const float clampedRange = std::max(light.range, 0.001f);
                gpu.positionAndRange = glm::vec4(tr.world.translation, clampedRange);
                gpu.directionAndAngle = glm::vec4(forward, glm::cos(glm::radians(light.outerConeAngle)));
                gpu.color = glm::vec4(light.color.r, light.color.g, light.color.b, light.intensity);
                gpu.attenuation = glm::vec4(std::max(light.constantAttenuation, 0.001f), light.linearAttenuation, light.quadraticAttenuation,
                    glm::cos(glm::radians(light.innerConeAngle)));

                if (si < kMaxBakeSpotShadows && light.castShadow)
                {
                    gpu.shadowParams = glm::vec4(static_cast<float>(kMaxBakePointShadows + si), light.shadowBias,
                        light.softShadows ? light.pcfRadius : 0.0f, light.shadowStrength);
                    localShadowLights.push_back({ kMaxBakePointShadows + si, tr.world.translation, clampedRange });
                }
                else
                {
                    gpu.shadowParams = glm::vec4(-1.0f, light.shadowBias, 0.0f, 0.0f);
                }
            }
            m_BaseSceneData.numSpotLights = spotCount;
            m_SpotLightBuffer->SetData(&spotData, sizeof(spotData));
        }

        // -------------------------------------------------------------------
        // 2. Active WorldEnvironment (HDRI / ProceduralSky LUT)
        // -------------------------------------------------------------------
        Ref<Texture> envMapTex = Renderer::GetBlackTexture();
        if (WorldEnvironment *worldEnv = scene->GetActiveWorldEnvironment())
        {
            if (!worldEnv->environment)
            {
                worldEnv->environment = Environment::Create();
                worldEnv->gpuInitialized = false;
            }
            if (worldEnv->environment)
            {
                m_ActiveEnvironment = worldEnv->environment;
                m_ActiveSkyType = worldEnv->skyType;
                m_ActiveEnvironment->SetSkyType(worldEnv->skyType);
                m_BaseSceneData.skyType = static_cast<int32_t>(worldEnv->skyType);
                m_ActiveEnvironment->WriteBuffer(cmd);

                if (worldEnv->skyType == SkyType::ProceduralSky && m_ActiveEnvironment->GetProceduralSky())
                {
                    Ref<ProceduralSky> sky = m_ActiveEnvironment->GetProceduralSky();
                    sky->GetParams() = worldEnv->atmosphereParams;
                    const float clampedSunIntensity = glm::clamp(m_BaseSceneData.sunColor.a, 0.1f, 5.0f);
                    sky->RenderLUTs(cmd, m_SunDirection, glm::vec3(m_BaseSceneData.sunColor), clampedSunIntensity,
                        m_BaseSceneData.sunAngularRadius, glm::vec3(0.0f));
                    if (sky->GetSkyViewLUT())
                    {
                        envMapTex = sky->GetSkyViewLUT();
                    }
                }
                else
                {
                    if (worldEnv->skyType == SkyType::HDRI && worldEnv->hdrHandle != AssetHandle(0) && assetManager)
                    {
                        Ref<Texture> currentHdr = m_ActiveEnvironment->GetHDRTexture();
                        if (worldEnv->dirtyEnvironment || !currentHdr || currentHdr == Renderer::GetBlackTexture() || !currentHdr->IsReady())
                        {
                            Ref<Texture> hdrTex = assetManager->GetAsset<Texture>(worldEnv->hdrHandle);
                            if (!hdrTex || !hdrTex->IsReady())
                            {
                                hdrTex = assetManager->GetAssetImmediate<Texture>(worldEnv->hdrHandle);
                            }
                            if (hdrTex && hdrTex->IsReady())
                            {
                                m_ActiveEnvironment->SetTexture(hdrTex);
                                worldEnv->dirtyEnvironment = false;
                            }
                        }
                    }

                    if (m_ActiveEnvironment->GetHDRTexture())
                    {
                        envMapTex = m_ActiveEnvironment->GetHDRTexture();
                    }
                }

                if (worldEnv->enableIBL && envMapTex && envMapTex != Renderer::GetBlackTexture())
                {
                    m_PreparedEnvTexture = envMapTex;
                    m_HasPreparedSkyIBL = true;
                }
            }
        }

        // -------------------------------------------------------------------
        // 3. Collect contributeGI static meshes & build isolated GPU batches
        // -------------------------------------------------------------------
        glm::vec3 sceneMin(std::numeric_limits<float>::max());
        glm::vec3 sceneMax(std::numeric_limits<float>::lowest());
        bool anyBounds = false;

        struct SubmeshEntry
        {
            nri::Buffer *vb = nullptr;
            nri::Buffer *ib = nullptr;
            uint32_t indexCount = 0;
            uint32_t objectIndex = 0;
            Ref<BindingSet> matSet;
            nri::CullMode cullMode = nri::CullMode::FRONT;
            AABB worldAABB{};
            bool castShadow = true;
        };
        std::vector<SubmeshEntry> entries;
        BatchBuilder shadowBatchBuilder;
        Ref<Texture> bakeShadowTex = m_BakeShadowMap->GetDepthTexture(0);
        Ref<Material> defaultMaterial = Renderer::GetDefaultMaterial();
        std::unordered_set<Material *> uploadedMaterials;

        auto resolveMaterial = [&](int submeshIdx, const std::unordered_map<int, AssetHandle> &overrides, AssetHandle instanceHandle) -> Ref<Material>
        {
            AssetHandle matHandle = instanceHandle;
            if (auto it = overrides.find(submeshIdx); it != overrides.end() && it->second != AssetHandle(0))
            {
                matHandle = it->second;
            }
            if (assetManager && matHandle != AssetHandle(0))
            {
                Ref<Material> mat = assetManager->GetAsset<Material>(matHandle);
                if (!mat)
                {
                    mat = assetManager->GetAssetImmediate<Material>(matHandle);
                }
                if (mat)
                {
                    return mat;
                }
            }
            return defaultMaterial;
        };

        auto ensureMaterialTexturesLoaded = [&](const Ref<Material> &mat)
        {
            if (!assetManager || !mat)
            {
                return;
            }
            const AssetHandle texHandles[6] = {
                mat->baseColorTextureHandle,
                mat->emissiveTextureHandle,
                mat->metallicTextureHandle,
                mat->roughnessTextureHandle,
                mat->normalTextureHandle,
                mat->occlusionTextureHandle
            };
            for (AssetHandle h : texHandles)
            {
                if (h == AssetHandle(0))
                {
                    continue;
                }
                Ref<Texture> tex = assetManager->GetAsset<Texture>(h);
                if (!tex || !tex->IsReady())
                {
                    (void)assetManager->GetAssetImmediate<Texture>(h);
                }
            }
        };

        if (scene->registry)
        {
            auto meshView = scene->registry->view<TransformComponent, StaticMeshComponent>();
            for (auto entity : meshView)
            {
                const auto *rc = scene->registry->try_get<RenderingComponent>(entity);
                if (rc)
                {
                    if (!rc->visible || !rc->contributeGI || rc->mobility != EntityMobility::Static)
                    {
                        continue;
                    }
                }
                const bool castShadow = (!rc || rc->castShadow);
                const bool receiveShadow = (!rc || rc->receiveShadow);

                const auto &tr = meshView.get<TransformComponent>(entity);
                auto &smc = meshView.get<StaticMeshComponent>(entity);
                if (smc.handle == AssetHandle(0) || !assetManager)
                {
                    continue;
                }

                Ref<StaticMesh> mesh = assetManager->GetAsset<StaticMesh>(smc.handle);
                if (!mesh)
                {
                    mesh = assetManager->GetAssetImmediate<StaticMesh>(smc.handle);
                }
                if (!mesh)
                {
                    continue;
                }

                const glm::mat4 entityWorld = tr.world.GetMatrix();
                const auto &instances = mesh->GetMeshInstances();
                for (size_t idx = 0; idx < instances.size(); ++idx)
                {
                    const auto &inst = instances[idx];
                    if (!inst)
                    {
                        continue;
                    }
                    auto &primitive = inst->GetPrimitive();
                    if (!primitive || primitive->vertices.empty() || primitive->indices.empty())
                    {
                        continue;
                    }
                    if (!primitive->vertexBuffer || !primitive->indexBuffer)
                    {
                        primitive->WriteBuffer(cmd);
                    }
                    if (!primitive->vertexBuffer || !primitive->indexBuffer)
                    {
                        continue;
                    }

                    Ref<Material> material = resolveMaterial(static_cast<int>(idx), smc.overrideMaterials, inst->GetMaterialAssetHandle());
                    if (!material)
                    {
                        continue;
                    }
                    // Exclude transparents from GI capture
                    if (material->GetType() == MaterialType::Transparent)
                    {
                        continue;
                    }

                    if (material->UpdateBindingSet(envMapTex, bakeShadowTex) && uploadedMaterials.insert(material.get()).second)
                    {
                        ensureMaterialTexturesLoaded(material);
                        material->UploadToGpu(cmd);
                    }
                    Ref<BindingSet> matSet = material->GetBindingSet();
                    if (!matSet)
                    {
                        continue;
                    }
                    m_RetainedMaterials.push_back(material);

                    glm::mat4 worldTransform = entityWorld * inst->global;
                    if (std::abs(glm::determinant(worldTransform)) < 1e-6f)
                    {
                        worldTransform = glm::mat4(1.0f);
                    }

                    AABB instAABB = TransformAABB(inst->localAABB, worldTransform);
                    sceneMin = glm::min(sceneMin, instAABB.min);
                    sceneMax = glm::max(sceneMax, instAABB.max);
                    anyBounds = true;

                    Mesh_GPUData objData{};
                    objData.transformation = worldTransform;
                    objData.normal = glm::mat4(glm::transpose(glm::inverse(glm::mat3(worldTransform))));
                    objData.objectID = 0xFFFFFFFFu;
                    objData.boneOffset = 0;
                    objData.receiveShadow = receiveShadow ? 1u : 0u;
                    const uint32_t objectIdx = m_ObjectAllocator.Allocate(cmd, objData);

                    nri::Buffer *vb = primitive->vertexBuffer->GetHandle();
                    nri::Buffer *ib = primitive->indexBuffer->GetHandle();
                    const uint32_t indexCount = primitive->indexBuffer->GetCount();

                    entries.push_back({ vb, ib, indexCount, objectIdx, matSet, material->GetCullMode(), instAABB, castShadow });

                    if (castShadow)
                    {
                        BatchKey shadowKey{ vb, ib, m_ShadowBindingSet ? m_ShadowBindingSet->GetHandle() : nullptr, nullptr, nullptr };
                        shadowBatchBuilder.Submit(shadowKey, vb, ib, m_ShadowBindingSet, nullptr, nullptr, indexCount, objectIdx, sizeof(VertexMeshStatic));
                    }
                }
            }
        }

        if (anyBounds)
        {
            m_SceneBounds = AABB::FromMinMax(sceneMin, sceneMax);
        }
        else
        {
            m_SceneBounds = AABB::FromMinMax(glm::vec3(-20.0f), glm::vec3(20.0f));
        }

        // Group static mesh submeshes by (vb, ib, materialBindingSet, cullMode) and pre-allocate instance indices
        struct BatchAccumKey
        {
            nri::Buffer *vb = nullptr;
            nri::Buffer *ib = nullptr;
            nri::DescriptorSet *matSet = nullptr;
            nri::CullMode cullMode = nri::CullMode::FRONT;

            bool operator==(const BatchAccumKey &o) const noexcept
            {
                return vb == o.vb && ib == o.ib && matSet == o.matSet && cullMode == o.cullMode;
            }
        };
        struct BatchAccumKeyHash
        {
            size_t operator()(const BatchAccumKey &k) const noexcept
            {
                size_t h = std::hash<const void *>{}(k.vb);
                h ^= std::hash<const void *>{}(k.ib) + 0x9e3779b9u + (h << 6) + (h >> 2);
                h ^= std::hash<const void *>{}(k.matSet) + 0x9e3779b9u + (h << 6) + (h >> 2);
                h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.cullMode)) + 0x9e3779b9u + (h << 6) + (h >> 2);
                return h;
            }
        };

        struct BatchAccumVal
        {
            PreparedBatch batch;
            std::vector<uint32_t> objectIndices;
            std::vector<uint32_t> shadowObjectIndices;
        };
        std::unordered_map<BatchAccumKey, BatchAccumVal, BatchAccumKeyHash> grouped;
        grouped.reserve(entries.size());
        for (const auto &e : entries)
        {
            BatchAccumKey k{ e.vb, e.ib, e.matSet->GetHandle(), e.cullMode };
            auto &acc = grouped[k];
            if (acc.objectIndices.empty())
            {
                acc.batch.vertexBuffer = e.vb;
                acc.batch.indexBuffer = e.ib;
                acc.batch.indexCount = e.indexCount;
                acc.batch.vertexStride = sizeof(VertexMeshStatic);
                acc.batch.materialBindingSet = e.matSet;
                acc.batch.cullMode = e.cullMode;
            }
            acc.objectIndices.push_back(e.objectIndex);
            acc.batch.instanceObjectIndices.push_back(e.objectIndex);
            acc.batch.instanceWorldAABBs.push_back(e.worldAABB);
            if (e.castShadow)
            {
                acc.shadowObjectIndices.push_back(e.objectIndex);
            }
        }

        m_PreparedBatches.reserve(grouped.size());
        for (auto &[_, acc] : grouped)
        {
            acc.batch.instanceCount = static_cast<uint32_t>(acc.objectIndices.size());
            acc.batch.baseInstanceOffset = m_InstanceIndexAllocator.Allocate(cmd, acc.objectIndices.data(), acc.batch.instanceCount);
            acc.batch.shadowInstanceCount = static_cast<uint32_t>(acc.shadowObjectIndices.size());
            acc.batch.shadowBaseInstanceOffset = (acc.batch.shadowInstanceCount > 0)
                ? m_InstanceIndexAllocator.Allocate(cmd, acc.shadowObjectIndices.data(), acc.batch.shadowInstanceCount)
                : 0u;
            m_PreparedBatches.push_back(std::move(acc.batch));
        }

        // -------------------------------------------------------------------
        // 4. Render single ortho bake shadow map into cascade 0
        // -------------------------------------------------------------------
        m_BakeShadowMap->ComputeBakeMatrix(m_SceneBounds, m_SunDirection);
        auto &csmData = m_BakeShadowMap->GetGPUData();
        csmData.shadowStrength = hasShadowedSun ? sunShadowStrength : 0.0f;
        csmData.minBias = sunMinBias;
        csmData.maxBias = sunMaxBias;
        csmData.pcfRadius = sunPcfRadius;
        csmData.cascadeIndex = 0;
        csmData.softShadows = sunSoftShadows ? 1 : 0;
        m_BakeShadowMap->GetGPUDataBuffer()->SetData(&csmData, sizeof(CSM_GPUData));

        m_ShadowSceneBuffer->SetData(&m_BaseSceneData, sizeof(m_BaseSceneData));

        shadowBatchBuilder.Finalize();
        RenderTarget *cascadeRT = m_BakeShadowMap->GetCascadeFramebuffer(0, 0);
        if (cascadeRT)
        {
            cascadeRT->BeginRendering(cmd, false, true);
            Ref<GraphicsPipeline> shadowPSO = GetOrCreateShadowPSO();
            if (hasShadowedSun && shadowPSO && shadowPSO->GetHandle() && m_ShadowBindingSet && !shadowBatchBuilder.IsEmpty())
            {
                const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
                nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *shadowPSO->GetPipelineLayout());
                nri.CmdSetPipeline(*cmd, *shadowPSO->GetHandle());

                nri::SetDescriptorSetDesc set0 = { 0, m_ShadowBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                nri.CmdSetDescriptorSet(*cmd, set0);

                for (const DrawBatch &batch : shadowBatchBuilder.GetBatches())
                {
                    const uint32_t count = batch.GetInstanceCount();
                    if (count == 0)
                    {
                        continue;
                    }
                    const uint32_t baseOffset = m_InstanceIndexAllocator.Allocate(cmd, batch.objectIndices.data(), count);

                    nri::VertexBufferDesc vbDesc = {};
                    vbDesc.buffer = batch.vertexBuffer;
                    vbDesc.offset = 0;
                    vbDesc.stride = batch.vertexStride;
                    nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                    nri.CmdSetIndexBuffer(*cmd, *batch.indexBuffer, 0, nri::IndexType::UINT32);

                    nri::SetRootConstantsDesc rootConst = {};
                    rootConst.rootConstantIndex = 0;
                    rootConst.data = &baseOffset;
                    rootConst.size = sizeof(uint32_t);
                    rootConst.offset = 0;
                    rootConst.bindPoint = nri::BindPoint::GRAPHICS;
                    nri.CmdSetRootConstants(*cmd, rootConst);

                    nri::DrawIndexedDesc drawDesc = {};
                    drawDesc.indexNum = batch.indexCount;
                    drawDesc.instanceNum = count;
                    nri.CmdDrawIndexed(*cmd, drawDesc);
                }
            }
            cascadeRT->EndRendering(cmd);
        }
        m_BakeShadowMap->EnsureShaderResourceLayout(cmd, 0);

        // -------------------------------------------------------------------
        // 5. Render bake-only local-light shadow cubemap array (point + spot)
        // -------------------------------------------------------------------
        if (!localShadowLights.empty())
        {
            const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
            for (const LocalShadowLight &light : localShadowLights)
            {
                EnsureLocalShadowLightResources(light.slot);
                for (uint32_t face = 0; face < 6; ++face)
                {
                    const uint32_t slice = light.slot * 6u + face;
                    if (!m_LocalShadowFaceCameraBuffers[slice])
                    {
                        continue;
                    }

                    glm::mat4 faceView(1.0f);
                    glm::mat4 faceProj(1.0f);
                    GetCubemapFaceViewProj(light.position, face, 0.05f, std::max(light.range, 0.5f), faceView, faceProj);

                    CameraBufferData camData{};
                    camData.viewProjection = faceProj;
                    camData.view = faceView;
                    camData.position = glm::vec4(light.position, std::max(light.range, 1e-4f));
                    m_LocalShadowFaceCameraBuffers[slice]->SetData(&camData, sizeof(camData));
                }
            }

            Ref<GraphicsPipeline> localShadowPSO = GetOrCreateLocalShadowPSO();
            for (const LocalShadowLight &light : localShadowLights)
            {
                for (uint32_t face = 0; face < 6; ++face)
                {
                    const uint32_t slice = light.slot * 6u + face;
                    RenderTarget *rt = m_LocalShadowFaceRTs[slice].get();
                    if (!rt)
                    {
                        continue;
                    }

                    rt->BeginRendering(cmd, true, true);
                    if (localShadowPSO && localShadowPSO->GetHandle() && m_LocalShadowFaceBindingSets[slice])
                    {
                        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *localShadowPSO->GetPipelineLayout());
                        nri.CmdSetPipeline(*cmd, *localShadowPSO->GetHandle());

                        nri::SetDescriptorSetDesc set0 = { 0, m_LocalShadowFaceBindingSets[slice]->GetHandle(), nri::BindPoint::GRAPHICS };
                        nri.CmdSetDescriptorSet(*cmd, set0);

                        for (const PreparedBatch &batch : m_PreparedBatches)
                        {
                            if (batch.shadowInstanceCount == 0 || !batch.vertexBuffer || !batch.indexBuffer)
                            {
                                continue;
                            }

                            nri::VertexBufferDesc vbDesc = {};
                            vbDesc.buffer = batch.vertexBuffer;
                            vbDesc.offset = 0;
                            vbDesc.stride = batch.vertexStride;
                            nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                            nri.CmdSetIndexBuffer(*cmd, *batch.indexBuffer, 0, nri::IndexType::UINT32);

                            nri::SetRootConstantsDesc rootConst = {};
                            rootConst.rootConstantIndex = 0;
                            rootConst.data = &batch.shadowBaseInstanceOffset;
                            rootConst.size = sizeof(uint32_t);
                            rootConst.offset = 0;
                            rootConst.bindPoint = nri::BindPoint::GRAPHICS;
                            nri.CmdSetRootConstants(*cmd, rootConst);

                            nri::DrawIndexedDesc drawDesc = {};
                            drawDesc.indexNum = batch.indexCount;
                            drawDesc.instanceNum = batch.shadowInstanceCount;
                            nri.CmdDrawIndexed(*cmd, drawDesc);
                        }
                    }
                    rt->EndRendering(cmd);
                }
            }

            m_HasBakeLocalShadows = (m_BakeLocalShadowCubeArray && m_BakeLocalShadowCubeArray->GetBindlessIndex() != UINT32_MAX);
        }

        BindlessSystem::FlushPendingWrites();
    }

    CaptureResult SceneCaptureRenderer::CaptureCubemap(nri::CommandBuffer *cmd, const glm::vec3 &worldPos, const CaptureSettings &settings)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd)
        {
            return {};
        }

        EnsureCommonGPUResources();
        if (!m_CommonInitialized)
        {
            return {};
        }

        EnsureCaptureTargets(settings.resolution);
        if (!m_ColorCube || !m_DepthCube || !m_FaceRenderTargets[0])
        {
            return {};
        }

        CaptureUniformSlot &slot = m_CaptureSlots[m_CaptureSlotCursor];
        m_CaptureSlotCursor = (m_CaptureSlotCursor + 1) % kMaxCaptureSlots;

        Scene_GPUData sceneData = m_BaseSceneData;
        sceneData.exposure = 1.0f;
        sceneData.gamma = 2.2f;
        sceneData.renderMode = RENDER_MODE_COLOR;
        sceneData.debugShadow = 0;
        sceneData.enableIBL = (settings.mode == CaptureMode::Reflection && settings.includeSky) ? 1 : 0;

        // Avoid injecting an aliased 12x procedural sun disk directly into low-res irradiance cubes;
        // surface shading still uses scene.lightAngle + scene.lightColor.
        if (settings.mode == CaptureMode::Irradiance)
        {
            sceneData.sunDirection = glm::vec4(0.0f);
        }
        slot.sceneBuffer->SetData(&sceneData, sizeof(sceneData));

        GI_GPUData giData = m_HasOverrideGIData ? m_OverrideGIData : GI_GPUData{};
        if (settings.enablePreviousGI)
        {
            giData = settings.previousGIData;
        }
        else
        {
            giData.biasAndFade.w = 0.0f;
            giData.giMode = 0;
        }
        giData.captureMode = static_cast<int32_t>(settings.mode);
        giData.bakePointShadowCubeArrayIndex = (m_HasBakeLocalShadows && m_BakeLocalShadowCubeArray)
            ? static_cast<int32_t>(m_BakeLocalShadowCubeArray->GetBindlessIndex())
            : -1;
        slot.giBuffer->SetData(&giData, sizeof(giData));

        for (uint32_t face = 0; face < 6; ++face)
        {
            glm::mat4 faceView(1.0f);
            glm::mat4 faceProj(1.0f);
            GetCubemapFaceViewProj(worldPos, face, settings.nearPlane, settings.farPlane, faceView, faceProj);

            CameraBufferData camData{};
            camData.viewProjection = faceProj;
            camData.view = faceView;
            camData.position = glm::vec4(worldPos, 1.0f);
            slot.faceCameraBuffers[face]->SetData(&camData, sizeof(camData));
        }

        // Pre-filter batches for Reflection captures so small enclosing objects (e.g. a chrome sphere
        // hosting a local ReflectionProbe at its center) do not self-occlude their own cubemap.
        struct DrawSlice
        {
            const PreparedBatch *batch = nullptr;
            uint32_t baseOffset = 0;
            uint32_t instanceCount = 0;
        };
        std::vector<DrawSlice> drawSlices;
        drawSlices.reserve(m_PreparedBatches.size());
        const bool checkExcludeOrigin = (settings.mode == CaptureMode::Reflection && settings.excludeOriginRadius > 0.0f);
        for (const PreparedBatch &batch : m_PreparedBatches)
        {
            if (batch.instanceCount == 0 || !batch.vertexBuffer || !batch.indexBuffer || !batch.materialBindingSet)
            {
                continue;
            }
            if (!checkExcludeOrigin || batch.instanceWorldAABBs.size() != batch.instanceCount)
            {
                drawSlices.push_back({ &batch, batch.baseInstanceOffset, batch.instanceCount });
                continue;
            }

            std::vector<uint32_t> keptIndices;
            bool anyExcluded = false;
            const float r = settings.excludeOriginRadius;
            for (size_t k = 0; k < batch.instanceWorldAABBs.size(); ++k)
            {
                const AABB &wb = batch.instanceWorldAABBs[k];
                const glm::vec3 size = wb.GetSize();
                const float maxDim = std::max({ size.x, size.y, size.z });
                const bool containsOrigin =
                    worldPos.x >= wb.min.x && worldPos.x <= wb.max.x &&
                    worldPos.y >= wb.min.y && worldPos.y <= wb.max.y &&
                    worldPos.z >= wb.min.z && worldPos.z <= wb.max.z;
                const float distToCenter = glm::length(wb.GetCenter() - worldPos);
                const bool isSelfEnclosing = (containsOrigin || distToCenter < r) && (maxDim < r * 4.0f);
                if (isSelfEnclosing)
                {
                    if (!anyExcluded)
                    {
                        keptIndices.reserve(batch.instanceCount - 1u);
                        for (size_t prev = 0; prev < k; ++prev)
                        {
                            keptIndices.push_back(batch.instanceObjectIndices[prev]);
                        }
                        anyExcluded = true;
                    }
                }
                else if (anyExcluded)
                {
                    keptIndices.push_back(batch.instanceObjectIndices[k]);
                }
            }

            if (!anyExcluded)
            {
                drawSlices.push_back({ &batch, batch.baseInstanceOffset, batch.instanceCount });
            }
            else if (!keptIndices.empty())
            {
                const uint32_t filteredCount = static_cast<uint32_t>(keptIndices.size());
                const uint32_t filteredBase = m_InstanceIndexAllocator.Allocate(cmd, keptIndices.data(), filteredCount);
                drawSlices.push_back({ &batch, filteredBase, filteredCount });
            }
        }

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        Ref<GraphicsPipeline> skyPSO = settings.includeSky ? GetOrCreateSkyPSO() : nullptr;

        for (uint32_t face = 0; face < 6; ++face)
        {
            RenderTarget *rt = m_FaceRenderTargets[face].get();
            rt->BeginRendering(cmd, true, true);

            if (settings.includeSky && m_ActiveEnvironment && skyPSO && skyPSO->GetHandle())
            {
                m_ActiveEnvironment->Draw(cmd, rt, skyPSO, slot.faceCameraBuffers[face], slot.sceneBuffer);
            }

            if (slot.staticFaceBindingSets[face])
            {
                for (const DrawSlice &slice : drawSlices)
                {
                    const PreparedBatch &batch = *slice.batch;

                    const nri::CullMode faceCullMode = (settings.mode == CaptureMode::Irradiance)
                        ? nri::CullMode::NONE
                        : InvertCullModeForYFlip(batch.cullMode);

                    Ref<GraphicsPipeline> meshPSO = GetOrCreateMeshPSO(faceCullMode);
                    if (!meshPSO || !meshPSO->GetHandle())
                    {
                        continue;
                    }

                    nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *meshPSO->GetPipelineLayout());
                    nri.CmdSetPipeline(*cmd, *meshPSO->GetHandle());

                    nri::SetDescriptorSetDesc set0 = { 0, slot.staticFaceBindingSets[face]->GetHandle(), nri::BindPoint::GRAPHICS };
                    nri::SetDescriptorSetDesc set1 = { 1, batch.materialBindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
                    nri::SetDescriptorSetDesc set2 = { 2, BindlessSystem::GetDescriptorSet(), nri::BindPoint::GRAPHICS };
                    nri.CmdSetDescriptorSet(*cmd, set0);
                    nri.CmdSetDescriptorSet(*cmd, set1);
                    nri.CmdSetDescriptorSet(*cmd, set2);

                    nri::VertexBufferDesc vbDesc = {};
                    vbDesc.buffer = batch.vertexBuffer;
                    vbDesc.offset = 0;
                    vbDesc.stride = batch.vertexStride;
                    nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
                    nri.CmdSetIndexBuffer(*cmd, *batch.indexBuffer, 0, nri::IndexType::UINT32);

                    nri::SetRootConstantsDesc rootConst = {};
                    rootConst.rootConstantIndex = 0;
                    rootConst.data = &slice.baseOffset;
                    rootConst.size = sizeof(uint32_t);
                    rootConst.offset = 0;
                    rootConst.bindPoint = nri::BindPoint::GRAPHICS;
                    nri.CmdSetRootConstants(*cmd, rootConst);

                    nri::DrawIndexedDesc drawDesc = {};
                    drawDesc.indexNum = batch.indexCount;
                    drawDesc.instanceNum = slice.instanceCount;
                    nri.CmdDrawIndexed(*cmd, drawDesc);
                }
            }

            rt->EndRendering(cmd);
        }

        return { m_ColorCube, m_DepthCube };
    }

    CaptureResult SceneCaptureRenderer::CaptureCubemapSync(Scene *scene, AssetManager *assetManager, const glm::vec3 &worldPos, const CaptureSettings &settings)
    {
        IGN_PROFILE_FUNCTION();

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !dm->GetGraphicsQueue())
        {
            return {};
        }

        EnsureCommonGPUResources();
        EnsureSyncCommandResources();
        if (!m_SyncCmdAllocator || !m_SyncCmdBuffer || !m_SyncFence)
        {
            return {};
        }

        const auto &nri = dm->GetCoreInterface();
        nri.ResetCommandAllocator(*m_SyncCmdAllocator);
        nri.BeginCommandBuffer(*m_SyncCmdBuffer, nullptr);

        PrepareScene(scene, assetManager, m_SyncCmdBuffer, settings.enablePreviousGI ? &settings.previousGIData : nullptr);
        CaptureResult result = CaptureCubemap(m_SyncCmdBuffer, worldPos, settings);

        nri.EndCommandBuffer(*m_SyncCmdBuffer);

        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            nri::FenceSubmitDesc signalDesc = {};
            signalDesc.fence = m_SyncFence;
            signalDesc.value = m_SyncFenceValue;

            nri::QueueSubmitDesc submitDesc = {};
            submitDesc.commandBuffers = &m_SyncCmdBuffer;
            submitDesc.commandBufferNum = 1;
            submitDesc.signalFences = &signalDesc;
            submitDesc.signalFenceNum = 1;
            nri.QueueSubmit(*dm->GetGraphicsQueue(), submitDesc);
            nri.Wait(*m_SyncFence, m_SyncFenceValue);
            ++m_SyncFenceValue;
        }

        return result;
    }
}
