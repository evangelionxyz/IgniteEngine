// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "gizmo_renderer.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/shader.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ignite
{
    namespace
    {
        struct GizmoPushConstantsGPU
        {
            uint32_t baseInstance = 0;
            uint32_t drawMode = 0;
            float pad[2] = { 0.0f, 0.0f };
        };

        void AppendMeshData(
            std::vector<GizmoVertex> &allVerts,
            std::vector<uint32_t> &allIndices,
            const std::vector<GizmoVertex> &verts,
            const std::vector<uint32_t> &indices,
            uint32_t &outFirstIndex,
            uint32_t &outIndexCount,
            uint32_t &outVertexOffset)
        {
            outFirstIndex = static_cast<uint32_t>(allIndices.size());
            outIndexCount = static_cast<uint32_t>(indices.size());
            outVertexOffset = static_cast<uint32_t>(allVerts.size());

            const uint32_t baseVert = outVertexOffset;
            allVerts.insert(allVerts.end(), verts.begin(), verts.end());
            for (uint32_t idx : indices)
            {
                allIndices.push_back(baseVert + idx);
            }
        }

        void BuildCylinderMesh(std::vector<GizmoVertex> &verts, std::vector<uint32_t> &indices, uint32_t slices = 24)
        {
            verts.clear();
            indices.clear();

            // Side surface (y in [0, 1], radius 1)
            const uint32_t sideStart = 0;
            for (uint32_t i = 0; i <= slices; ++i)
            {
                const float a = (static_cast<float>(i) / static_cast<float>(slices)) * glm::two_pi<float>();
                const float c = std::cos(a);
                const float s = std::sin(a);
                const glm::vec3 n(c, 0.0f, s);
                verts.push_back({ glm::vec3(c, 0.0f, s), n, glm::vec4(1.0f), 0.0f, 0u });
                verts.push_back({ glm::vec3(c, 1.0f, s), n, glm::vec4(1.0f), 0.0f, 0u });
            }
            for (uint32_t i = 0; i < slices; ++i)
            {
                const uint32_t b0 = sideStart + i * 2;
                const uint32_t t0 = b0 + 1;
                const uint32_t b1 = b0 + 2;
                const uint32_t t1 = b0 + 3;
                indices.push_back(b0); indices.push_back(t0); indices.push_back(b1);
                indices.push_back(t0); indices.push_back(t1); indices.push_back(b1);
            }

            // Bottom & top caps
            const uint32_t botCenter = static_cast<uint32_t>(verts.size());
            verts.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec4(1.0f), 0.0f, 0u });
            for (uint32_t i = 0; i <= slices; ++i)
            {
                const float a = (static_cast<float>(i) / static_cast<float>(slices)) * glm::two_pi<float>();
                verts.push_back({ glm::vec3(std::cos(a), 0.0f, std::sin(a)), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec4(1.0f), 0.0f, 0u });
            }
            for (uint32_t i = 0; i < slices; ++i)
            {
                indices.push_back(botCenter);
                indices.push_back(botCenter + 1 + i);
                indices.push_back(botCenter + 2 + i);
            }

            const uint32_t topCenter = static_cast<uint32_t>(verts.size());
            verts.push_back({ glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(1.0f), 0.0f, 0u });
            for (uint32_t i = 0; i <= slices; ++i)
            {
                const float a = (static_cast<float>(i) / static_cast<float>(slices)) * glm::two_pi<float>();
                verts.push_back({ glm::vec3(std::cos(a), 1.0f, std::sin(a)), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(1.0f), 0.0f, 0u });
            }
            for (uint32_t i = 0; i < slices; ++i)
            {
                indices.push_back(topCenter);
                indices.push_back(topCenter + 2 + i);
                indices.push_back(topCenter + 1 + i);
            }
        }

        void BuildConeMesh(std::vector<GizmoVertex> &verts, std::vector<uint32_t> &indices, uint32_t slices = 24)
        {
            verts.clear();
            indices.clear();

            // Slope normal for unit radius at y=0, apex at y=1
            const float invSlope = 1.0f / std::sqrt(2.0f);
            for (uint32_t i = 0; i <= slices; ++i)
            {
                const float a = (static_cast<float>(i) / static_cast<float>(slices)) * glm::two_pi<float>();
                const float c = std::cos(a);
                const float s = std::sin(a);
                const glm::vec3 n = glm::normalize(glm::vec3(c * invSlope, invSlope, s * invSlope));
                verts.push_back({ glm::vec3(c, 0.0f, s), n, glm::vec4(1.0f), 0.0f, 0u });
                verts.push_back({ glm::vec3(0.0f, 1.0f, 0.0f), n, glm::vec4(1.0f), 0.0f, 0u });
            }
            for (uint32_t i = 0; i < slices; ++i)
            {
                const uint32_t b0 = i * 2;
                const uint32_t tip = b0 + 1;
                const uint32_t b1 = b0 + 2;
                indices.push_back(b0); indices.push_back(tip); indices.push_back(b1);
            }

            const uint32_t botCenter = static_cast<uint32_t>(verts.size());
            verts.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec4(1.0f), 0.0f, 0u });
            for (uint32_t i = 0; i <= slices; ++i)
            {
                const float a = (static_cast<float>(i) / static_cast<float>(slices)) * glm::two_pi<float>();
                verts.push_back({ glm::vec3(std::cos(a), 0.0f, std::sin(a)), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec4(1.0f), 0.0f, 0u });
            }
            for (uint32_t i = 0; i < slices; ++i)
            {
                indices.push_back(botCenter);
                indices.push_back(botCenter + 1 + i);
                indices.push_back(botCenter + 2 + i);
            }
        }

        void BuildCubeMesh(std::vector<GizmoVertex> &verts, std::vector<uint32_t> &indices)
        {
            verts.clear();
            indices.clear();

            struct Face { glm::vec3 n; glm::vec3 u; glm::vec3 v; };
            constexpr Face kFaces[6] = {
                { { 1, 0, 0 }, { 0, 0,-1 }, { 0, 1, 0 } },
                { {-1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },
                { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0,-1 } },
                { { 0,-1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },
                { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },
                { { 0, 0,-1 }, {-1, 0, 0 }, { 0, 1, 0 } }
            };

            for (const auto &f : kFaces)
            {
                const uint32_t base = static_cast<uint32_t>(verts.size());
                const glm::vec3 c = f.n * 0.5f;
                const glm::vec3 u = f.u * 0.5f;
                const glm::vec3 v = f.v * 0.5f;
                verts.push_back({ c - u - v, f.n, glm::vec4(1.0f), 0.0f, 0u });
                verts.push_back({ c + u - v, f.n, glm::vec4(1.0f), 0.0f, 0u });
                verts.push_back({ c + u + v, f.n, glm::vec4(1.0f), 0.0f, 0u });
                verts.push_back({ c - u + v, f.n, glm::vec4(1.0f), 0.0f, 0u });
                indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
                indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
            }
        }

        void BuildSphereMesh(std::vector<GizmoVertex> &verts, std::vector<uint32_t> &indices, uint32_t rings = 14, uint32_t sectors = 24)
        {
            verts.clear();
            indices.clear();

            for (uint32_t r = 0; r <= rings; ++r)
            {
                const float phi = (static_cast<float>(r) / static_cast<float>(rings)) * glm::pi<float>();
                const float sp = std::sin(phi);
                const float cp = std::cos(phi);
                for (uint32_t s = 0; s <= sectors; ++s)
                {
                    const float theta = (static_cast<float>(s) / static_cast<float>(sectors)) * glm::two_pi<float>();
                    const glm::vec3 n(sp * std::cos(theta), cp, sp * std::sin(theta));
                    verts.push_back({ n, n, glm::vec4(1.0f), 0.0f, 0u });
                }
            }

            const uint32_t stride = sectors + 1;
            for (uint32_t r = 0; r < rings; ++r)
            {
                for (uint32_t s = 0; s < sectors; ++s)
                {
                    const uint32_t a = r * stride + s;
                    const uint32_t b = a + stride;
                    indices.push_back(a); indices.push_back(b); indices.push_back(a + 1);
                    indices.push_back(a + 1); indices.push_back(b); indices.push_back(b + 1);
                }
            }
        }

        void BuildTorusMesh(std::vector<GizmoVertex> &verts, std::vector<uint32_t> &indices, uint32_t majorSegs = 64, uint32_t minorSegs = 14, float minorRadius = 0.026f)
        {
            verts.clear();
            indices.clear();

            for (uint32_t i = 0; i <= majorSegs; ++i)
            {
                const float u = (static_cast<float>(i) / static_cast<float>(majorSegs)) * glm::two_pi<float>();
                const float cu = std::cos(u);
                const float su = std::sin(u);
                const glm::vec3 ringCenter(cu, 0.0f, su);

                for (uint32_t j = 0; j <= minorSegs; ++j)
                {
                    const float v = (static_cast<float>(j) / static_cast<float>(minorSegs)) * glm::two_pi<float>();
                    const float cv = std::cos(v);
                    const float sv = std::sin(v);
                    const glm::vec3 radial(cu * cv, sv, su * cv);
                    const glm::vec3 pos = ringCenter + radial * minorRadius;
                    verts.push_back({ pos, glm::normalize(radial), glm::vec4(1.0f), 0.0f, 0u });
                }
            }

            const uint32_t stride = minorSegs + 1;
            for (uint32_t i = 0; i < majorSegs; ++i)
            {
                for (uint32_t j = 0; j < minorSegs; ++j)
                {
                    const uint32_t a = i * stride + j;
                    const uint32_t b = a + stride;
                    indices.push_back(a); indices.push_back(b); indices.push_back(a + 1);
                    indices.push_back(a + 1); indices.push_back(b); indices.push_back(b + 1);
                }
            }
        }

        void BuildOctahedronMesh(std::vector<GizmoVertex> &verts, std::vector<uint32_t> &indices)
        {
            verts.clear();
            indices.clear();

            const glm::vec3 px( 1, 0, 0), nx(-1, 0, 0);
            const glm::vec3 py( 0, 1, 0), ny( 0,-1, 0);
            const glm::vec3 pz( 0, 0, 1), nz( 0, 0,-1);

            const glm::vec3 tris[8][3] = {
                { py, pz, px }, { py, px, nz }, { py, nz, nx }, { py, nx, pz },
                { ny, px, pz }, { ny, nz, px }, { ny, nx, nz }, { ny, pz, nx }
            };

            for (const auto &tri : tris)
            {
                const glm::vec3 n = glm::normalize(glm::cross(tri[1] - tri[0], tri[2] - tri[0]));
                const uint32_t base = static_cast<uint32_t>(verts.size());
                verts.push_back({ tri[0], n, glm::vec4(1.0f), 0.0f, 0u });
                verts.push_back({ tri[1], n, glm::vec4(1.0f), 0.0f, 0u });
                verts.push_back({ tri[2], n, glm::vec4(1.0f), 0.0f, 0u });
                indices.push_back(base);
                indices.push_back(base + 1);
                indices.push_back(base + 2);
            }
        }
    }

    GizmoRenderer::GizmoRenderer()
        : m_FrameConstantBuffer(sizeof(GizmoFrameConstantsGPU), true, 16, "[GizmoRenderer] Frame Constants")
    {
        BuildUnitMeshes();
    }

    GizmoRenderer::~GizmoRenderer()
    {
        auto *dm = DeviceManager::GetInstance();
        nri::Device *device = dm ? dm->GetDevice() : nullptr;
        if (device)
        {
            GPUUploadSync::DeviceWaitIdle(device);
            const auto &nri = dm->GetCoreInterface();
            for (auto &slot : m_FrameSlots)
            {
                slot.bindingSetCache.clear();
                if (slot.instanceMapped && slot.instanceBuffer)
                {
                    nri.UnmapBuffer(*slot.instanceBuffer);
                    slot.instanceMapped = nullptr;
                }
                if (slot.instanceSRV)
                {
                    nri.DestroyDescriptor(slot.instanceSRV);
                    slot.instanceSRV = nullptr;
                }
                if (slot.instanceBuffer)
                {
                    nri.DestroyBuffer(slot.instanceBuffer);
                    slot.instanceBuffer = nullptr;
                }
                slot.dynamicVertexBuffer.reset();
            }
        }
        m_OpaquePSOCache.clear();
        m_TranslucentPSOCache.clear();
        m_MeshVertexBuffer.reset();
        m_MeshIndexBuffer.reset();
    }

    void GizmoRenderer::ClearPipelineCache()
    {
        m_OpaquePSOCache.clear();
        m_TranslucentPSOCache.clear();
        for (auto &slot : m_FrameSlots)
        {
            slot.bindingSetCache.clear();
        }
    }

    void GizmoRenderer::BuildUnitMeshes()
    {
        std::vector<GizmoVertex> allVerts;
        std::vector<uint32_t> allIndices;
        std::vector<GizmoVertex> tmpVerts;
        std::vector<uint32_t> tmpIndices;

        auto appendType = [&](GizmoMeshType type)
        {
            auto &slice = m_MeshSlices[static_cast<size_t>(type)];
            AppendMeshData(allVerts, allIndices, tmpVerts, tmpIndices, slice.firstIndex, slice.indexCount, slice.vertexOffset);
        };

        BuildCylinderMesh(tmpVerts, tmpIndices);
        appendType(GizmoMeshType::Cylinder);

        BuildConeMesh(tmpVerts, tmpIndices);
        appendType(GizmoMeshType::Cone);

        BuildCubeMesh(tmpVerts, tmpIndices);
        appendType(GizmoMeshType::Cube);

        BuildSphereMesh(tmpVerts, tmpIndices);
        appendType(GizmoMeshType::Sphere);

        BuildTorusMesh(tmpVerts, tmpIndices);
        appendType(GizmoMeshType::Torus);

        BuildOctahedronMesh(tmpVerts, tmpIndices);
        appendType(GizmoMeshType::Octahedron);

        m_MeshVertexBuffer = VertexBuffer::Create(allVerts.size() * sizeof(GizmoVertex), "[GizmoRenderer] Unit Mesh VB");
        m_MeshVertexBuffer->SetData(allVerts.data(), allVerts.size() * sizeof(GizmoVertex));

        m_MeshIndexBuffer = IndexBuffer::Create(allIndices.size() * sizeof(uint32_t), "[GizmoRenderer] Unit Mesh IB");
        m_MeshIndexBuffer->SetData(allIndices.data(), allIndices.size() * sizeof(uint32_t));
    }

    void GizmoRenderer::EnsureFrameSlotBuffers(FrameSlotResources &slot, size_t requiredInstances, size_t requiredDynamicVertices)
    {
        auto *dm = DeviceManager::GetInstance();
        nri::Device *device = dm ? dm->GetDevice() : nullptr;
        if (!device)
            return;

        const auto &nri = dm->GetCoreInterface();

        const size_t minInstances = std::max<size_t>(requiredInstances, 64u);
        if (!slot.instanceBuffer || minInstances > slot.instanceCapacity)
        {
            if (slot.instanceBuffer)
            {
                GPUUploadSync::DeviceWaitIdle(device);
                slot.bindingSetCache.clear();
                if (slot.instanceMapped)
                {
                    nri.UnmapBuffer(*slot.instanceBuffer);
                    slot.instanceMapped = nullptr;
                }
                if (slot.instanceSRV)
                {
                    nri.DestroyDescriptor(slot.instanceSRV);
                    slot.instanceSRV = nullptr;
                }
                nri.DestroyBuffer(slot.instanceBuffer);
                slot.instanceBuffer = nullptr;
            }

            slot.instanceCapacity = minInstances;
            const uint64_t byteSize = static_cast<uint64_t>(slot.instanceCapacity) * sizeof(GizmoInstanceGPU);

            nri::BufferDesc bufDesc = {};
            bufDesc.size = byteSize;
            bufDesc.structureStride = sizeof(GizmoInstanceGPU);
            bufDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE;
            nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_UPLOAD, 1.0f, bufDesc, slot.instanceBuffer);

            nri::BufferViewDesc viewDesc = {};
            viewDesc.buffer = slot.instanceBuffer;
            viewDesc.type = nri::BufferView::STRUCTURED_BUFFER;
            viewDesc.offset = 0;
            viewDesc.size = byteSize;
            viewDesc.structureStride = sizeof(GizmoInstanceGPU);
            nri.CreateBufferView(viewDesc, slot.instanceSRV);

            slot.instanceMapped = static_cast<uint8_t *>(nri.MapBuffer(*slot.instanceBuffer, 0, byteSize));
            slot.bindingSetCache.clear();
        }

        if (requiredDynamicVertices > 0 && (!slot.dynamicVertexBuffer || requiredDynamicVertices > slot.dynamicVertexCapacity))
        {
            if (slot.dynamicVertexBuffer)
            {
                GPUUploadSync::DeviceWaitIdle(device);
            }
            slot.dynamicVertexCapacity = std::max<size_t>(requiredDynamicVertices, 256u);
            slot.dynamicVertexBuffer = VertexBuffer::Create(
                slot.dynamicVertexCapacity * sizeof(GizmoVertex),
                "[GizmoRenderer] Dynamic Triangle VB");
        }
    }

    Ref<GraphicsPipeline> GizmoRenderer::GetOrCreatePipeline(RenderTarget *framebuffer, bool depthWrite)
    {
        if (!framebuffer)
            return nullptr;

        auto &cache = depthWrite ? m_OpaquePSOCache : m_TranslucentPSOCache;
        const FramebufferKey key = MakeFramebufferKey(framebuffer, nri::FillMode::SOLID, nri::CullMode::NONE);
        if (auto it = cache.find(key); it != cache.end())
            return it->second;

        const bool hasDepth = framebuffer->HasDepthAttachment();

        GraphicsPipelineParams params;
        params.enableBlend = true;
        params.srcBlend = nri::BlendFactor::ONE;
        params.destBlend = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.srcBlendAlpha = nri::BlendFactor::ONE;
        params.destBlendAlpha = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.enableDepthTest = hasDepth;
        params.enableDepthWrite = hasDepth && depthWrite;
        params.depthFunc = nri::CompareOp::LESS_EQUAL;
        params.enableDepthStencil = false;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;

        BindingLayoutDesc layoutDesc;
        layoutDesc.SetRegisterSpace(0);
        layoutDesc.SetVisibility(nri::StageBits::ALL);
        layoutDesc.AddItem(BindingLayoutItem::PushConstants(0, sizeof(GizmoPushConstantsGPU)));
        layoutDesc.AddItem(BindingLayoutItem::VolatileConstantBuffer(1));
        layoutDesc.AddItem(BindingLayoutItem::StructuredBufferSRV(0));
        layoutDesc.AddItem(BindingLayoutItem::TextureSRV(1));

        Ref<Shader> vs = Shader::Create("resources/shaders/gizmo.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX, false);
        Ref<Shader> ps = Shader::Create("resources/shaders/gizmo.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL, false);

        Ref<GraphicsPipeline> pso = GraphicsPipeline::Create(depthWrite ? "Gizmo Opaque Pipeline" : "Gizmo Translucent Pipeline");
        pso->SetShaders({ vs, ps })
            .AddBindingLayout(layoutDesc)
            .Build(framebuffer, params);

        cache.emplace(key, pso);
        return pso;
    }

    Ref<BindingSet> GizmoRenderer::GetOrCreateBindingSet(FrameSlotResources &slot, nri::PipelineLayout *layout, const Ref<Texture> &depthTexture)
    {
        Ref<Texture> safeDepth = (depthTexture && depthTexture->GetShaderResourceView())
            ? depthTexture
            : Renderer::GetWhiteTexture();
        if (!safeDepth || !safeDepth->GetShaderResourceView() || !slot.instanceSRV)
            return nullptr;

        const uint64_t key = reinterpret_cast<uint64_t>(safeDepth->GetShaderResourceView())
            ^ (reinterpret_cast<uint64_t>(layout) * 1099511628211ULL);
        if (auto it = slot.bindingSetCache.find(key); it != slot.bindingSetCache.end())
            return it->second;

        BindingSetDesc desc;
        desc.AddItem(BindingSetItem::ConstantBuffer(1, m_FrameConstantBuffer.GetDescriptor()));
        desc.AddItem(BindingSetItem::StructuredBuffer_SRV(0, slot.instanceSRV));
        desc.AddItem(BindingSetItem::TextureSRV(1, safeDepth->GetShaderResourceView()));

        Ref<BindingSet> set = BindingSet::Create("[GizmoRenderer] Binding Set",
            BindingSet::AllocateAndUpdate(layout, 0, desc));
        if (set && set->GetHandle())
        {
            slot.bindingSetCache.emplace(key, set);
        }
        return set;
    }

    void GizmoRenderer::Render(
        nri::CommandBuffer *cmd,
        RenderTarget *gizmoFB,
        ICamera *camera,
        const GizmoDrawList &drawList,
        const Ref<Texture> &sceneDepthTexture,
        const Scene_GPUData *sceneGPUData,
        const PointLightBufferData *pointLights,
        const SpotLightBufferData *spotLights,
        const GI_GPUData *giData)
    {
        if (!cmd || !gizmoFB || !camera || drawList.Empty())
            return;

        if (!m_MeshVertexBuffer || !m_MeshIndexBuffer)
            return;

        Ref<GraphicsPipeline> opaquePSO = GetOrCreatePipeline(gizmoFB, true);
        Ref<GraphicsPipeline> translucentPSO = GetOrCreatePipeline(gizmoFB, false);
        if (!opaquePSO || !opaquePSO->GetHandle() || !translucentPSO || !translucentPSO->GetHandle())
            return;

        const size_t totalInstances = drawList.opaqueInstances.size() + drawList.translucentInstances.size();
        const size_t totalDynamicVerts = drawList.dynamicOpaqueTriangles.size() + drawList.dynamicTranslucentTriangles.size();

        FrameSlotResources &slot = m_FrameSlots[m_CurrentSlot % kMaxSlots];
        m_CurrentSlot = (m_CurrentSlot + 1) % kMaxSlots;
        EnsureFrameSlotBuffers(slot, totalInstances, totalDynamicVerts);

        // Upload instances into StructuredBuffer
        std::vector<GizmoInstanceGPU> gpuInstances;
        gpuInstances.reserve(totalInstances);
        auto appendInstance = [&gpuInstances](const GizmoPrimitiveInstance &src)
        {
            GizmoInstanceGPU dst;
            dst.worldMatrix = src.worldMatrix;
            dst.color = src.color;
            dst.ringCenterAndFlag = src.ringCenterAndFlag;
            dst.hoverFactor = src.hoverFactor;
            dst.shadingMode = src.shadingMode;
            gpuInstances.push_back(dst);
        };
        for (const auto &inst : drawList.opaqueInstances) appendInstance(inst);
        for (const auto &inst : drawList.translucentInstances) appendInstance(inst);

        if (!gpuInstances.empty() && slot.instanceMapped)
        {
            std::memcpy(slot.instanceMapped, gpuInstances.data(), gpuInstances.size() * sizeof(GizmoInstanceGPU));
        }

        // Upload dynamic world-space triangles
        if (totalDynamicVerts > 0 && slot.dynamicVertexBuffer)
        {
            std::vector<GizmoVertex> dynVerts;
            dynVerts.reserve(totalDynamicVerts);
            dynVerts.insert(dynVerts.end(), drawList.dynamicOpaqueTriangles.begin(), drawList.dynamicOpaqueTriangles.end());
            dynVerts.insert(dynVerts.end(), drawList.dynamicTranslucentTriangles.begin(), drawList.dynamicTranslucentTriangles.end());
            slot.dynamicVertexBuffer->SetData(dynVerts.data(), dynVerts.size() * sizeof(GizmoVertex));
        }

        // Upload frame & lighting constants
        GizmoFrameConstantsGPU frameConsts;
        frameConsts.viewProjection = camera->GetProjection() * camera->GetView();
        frameConsts.cameraPosAndIsOrtho = glm::vec4(camera->position, camera->IsPerspective() ? 0.0f : 1.0f);
        frameConsts.cameraForwardAndNear = glm::vec4(camera->GetForwardDirection(), camera->nearPlane);

        const uint32_t fbW = std::max(1u, gizmoFB->GetWidth());
        const uint32_t fbH = std::max(1u, gizmoFB->GetHeight());
        const uint32_t depthW = (sceneDepthTexture && sceneDepthTexture->GetWidth() > 0) ? static_cast<uint32_t>(sceneDepthTexture->GetWidth()) : 1u;
        const uint32_t depthH = (sceneDepthTexture && sceneDepthTexture->GetHeight() > 0) ? static_cast<uint32_t>(sceneDepthTexture->GetHeight()) : 1u;

        frameConsts.viewportAndDepthSize = glm::vec4(
            static_cast<float>(fbW), static_cast<float>(fbH),
            static_cast<float>(depthW), static_cast<float>(depthH));
        frameConsts.occlusionParams = glm::vec4(
            drawList.occludedDimScale,
            drawList.occludedAlphaScale,
            drawList.depthBias,
            camera->farPlane);

        if (sceneGPUData)
        {
            const float az = sceneGPUData->sungAngles.x;
            const float el = sceneGPUData->sungAngles.y;
            const glm::vec3 sunDir(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));
            frameConsts.sunDirection = glm::vec4(glm::normalize(sunDir), 0.0f);
            frameConsts.sunColor = sceneGPUData->sunColor;
            frameConsts.lightingCounts.x = sceneGPUData->ambient;
        }

        if (pointLights && sceneGPUData)
        {
            const int count = std::clamp(sceneGPUData->numPointLights, 0, 8);
            frameConsts.lightingCounts.y = static_cast<float>(count);
            for (int i = 0; i < count; ++i)
            {
                frameConsts.pointLights[i].positionAndRange = pointLights->lights[i].positionAndRange;
                frameConsts.pointLights[i].color = pointLights->lights[i].color;
            }
        }

        if (spotLights && sceneGPUData)
        {
            const int count = std::clamp(sceneGPUData->numSpotLights, 0, 4);
            frameConsts.lightingCounts.z = static_cast<float>(count);
            for (int i = 0; i < count; ++i)
            {
                frameConsts.spotLights[i].positionAndRange = spotLights->lights[i].positionAndRange;
                frameConsts.spotLights[i].directionAndCosOuter = spotLights->lights[i].directionAndAngle;
                frameConsts.spotLights[i].color = spotLights->lights[i].color;
                frameConsts.spotLights[i].cosInnerAndPad = glm::vec4(spotLights->lights[i].attenuation.w, 0.0f, 0.0f, 0.0f);
            }
        }

        if (giData && giData->skySH[0].w > 0.5f)
        {
            frameConsts.lightingCounts.w = 1.0f;
            for (int i = 0; i < 9; ++i)
            {
                frameConsts.skySH[i] = giData->skySH[i];
            }
        }

        m_FrameConstantBuffer.SetData(&frameConsts, sizeof(frameConsts));

        Ref<BindingSet> bindingSet = GetOrCreateBindingSet(slot, opaquePSO->GetPipelineLayout(), sceneDepthTexture);
        if (!bindingSet || !bindingSet->GetHandle())
            return;

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        const bool startedRendering = !gizmoFB->IsRendering();
        if (startedRendering)
            gizmoFB->BeginRendering(cmd, false, false);

        auto bindCommonState = [&](const Ref<GraphicsPipeline> &pso)
        {
            nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *pso->GetPipelineLayout());
            nri.CmdSetPipeline(*cmd, *pso->GetHandle());
            nri::SetDescriptorSetDesc setDesc = { 0, bindingSet->GetHandle(), nri::BindPoint::GRAPHICS };
            nri.CmdSetDescriptorSet(*cmd, setDesc);
        };

        auto drawInstances = [&](const std::vector<GizmoPrimitiveInstance> &list, uint32_t baseOffset)
        {
            if (list.empty())
                return;

            nri::VertexBufferDesc vbDesc = {};
            vbDesc.buffer = m_MeshVertexBuffer->GetHandle();
            vbDesc.offset = 0;
            vbDesc.stride = sizeof(GizmoVertex);
            nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
            nri.CmdSetIndexBuffer(*cmd, *m_MeshIndexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

            for (size_t i = 0; i < list.size(); ++i)
            {
                const MeshSlice &slice = m_MeshSlices[static_cast<size_t>(list[i].meshType)];
                GizmoPushConstantsGPU push;
                push.baseInstance = baseOffset + static_cast<uint32_t>(i);
                push.drawMode = 0;

                nri::SetRootConstantsDesc rc = {};
                rc.rootConstantIndex = 0;
                rc.data = &push;
                rc.size = sizeof(push);
                rc.offset = 0;
                rc.bindPoint = nri::BindPoint::GRAPHICS;
                nri.CmdSetRootConstants(*cmd, rc);

                nri::DrawIndexedDesc drawDesc = {};
                drawDesc.indexNum = slice.indexCount;
                drawDesc.baseIndex = slice.firstIndex;
                drawDesc.instanceNum = 1;
                nri.CmdDrawIndexed(*cmd, drawDesc);
            }
        };

        auto drawDynamicTris = [&](uint32_t vertexStart, uint32_t vertexCount)
        {
            if (vertexCount == 0 || !slot.dynamicVertexBuffer)
                return;

            nri::VertexBufferDesc vbDesc = {};
            vbDesc.buffer = slot.dynamicVertexBuffer->GetHandle();
            vbDesc.offset = static_cast<uint64_t>(vertexStart) * sizeof(GizmoVertex);
            vbDesc.stride = sizeof(GizmoVertex);
            nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);

            GizmoPushConstantsGPU push;
            push.baseInstance = 0;
            push.drawMode = 1;

            nri::SetRootConstantsDesc rc = {};
            rc.rootConstantIndex = 0;
            rc.data = &push;
            rc.size = sizeof(push);
            rc.offset = 0;
            rc.bindPoint = nri::BindPoint::GRAPHICS;
            nri.CmdSetRootConstants(*cmd, rc);

            nri::DrawDesc drawDesc = {};
            drawDesc.vertexNum = vertexCount;
            drawDesc.instanceNum = 1;
            nri.CmdDraw(*cmd, drawDesc);
        };

        // 1. Opaque pass (writes gizmo depth for clean gizmo-to-gizmo self-occlusion)
        if (!drawList.opaqueInstances.empty() || !drawList.dynamicOpaqueTriangles.empty())
        {
            bindCommonState(opaquePSO);
            drawInstances(drawList.opaqueInstances, 0u);
            drawDynamicTris(0u, static_cast<uint32_t>(drawList.dynamicOpaqueTriangles.size()));
        }

        // 2. Translucent pass (planar quads, rotation angle wedge, faded back axes)
        if (!drawList.translucentInstances.empty() || !drawList.dynamicTranslucentTriangles.empty())
        {
            bindCommonState(translucentPSO);
            drawInstances(drawList.translucentInstances, static_cast<uint32_t>(drawList.opaqueInstances.size()));
            drawDynamicTris(
                static_cast<uint32_t>(drawList.dynamicOpaqueTriangles.size()),
                static_cast<uint32_t>(drawList.dynamicTranslucentTriangles.size()));
        }

        if (startedRendering)
            gizmoFB->EndRendering(cmd);
    }
}

