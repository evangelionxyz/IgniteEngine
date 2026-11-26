// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GRAPHICS_GIZMO_RENDERER_HPP
#define IGN_GRAPHICS_GIZMO_RENDERER_HPP

#include "gizmo.hpp"
#include "ignite/core/base.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/buffers/index_buffer.hpp"
#include "ignite/graphics/buffers/vertex_buffer.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/hash_keys.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/scene/icamera.hpp"

#include <NRI.h>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ignite
{
    struct GizmoInstanceGPU
    {
        glm::mat4 worldMatrix = glm::mat4(1.0f);
        glm::vec4 color = glm::vec4(1.0f);
        glm::vec4 ringCenterAndFlag = glm::vec4(0.0f);
        float hoverFactor = 0.0f;
        uint32_t shadingMode = 0;
        float pad[2] = { 0.0f, 0.0f };
    };

    struct GizmoPointLightGPU
    {
        glm::vec4 positionAndRange = glm::vec4(0.0f);
        glm::vec4 color = glm::vec4(0.0f);
    };

    struct GizmoSpotLightGPU
    {
        glm::vec4 positionAndRange = glm::vec4(0.0f);
        glm::vec4 directionAndCosOuter = glm::vec4(0.0f);
        glm::vec4 color = glm::vec4(0.0f);
        glm::vec4 cosInnerAndPad = glm::vec4(0.0f);
    };

    struct GizmoFrameConstantsGPU
    {
        glm::mat4 viewProjection = glm::mat4(1.0f);
        glm::vec4 cameraPosAndIsOrtho = glm::vec4(0.0f);    // xyz = cameraPos, w = isOrtho
        glm::vec4 cameraForwardAndNear = glm::vec4(0.0f);   // xyz = camForward, w = nearPlane
        glm::vec4 viewportAndDepthSize = glm::vec4(1.0f);   // xy = gizmoViewportSize, zw = depthTexSize
        glm::vec4 occlusionParams = glm::vec4(0.48f, 0.42f, 0.00025f, 1000.0f); // x=dimScale, y=alphaScale, z=depthBias, w=farPlane
        glm::vec4 sunDirection = glm::vec4(0.4f, 0.8f, 0.45f, 0.0f);
        glm::vec4 sunColor = glm::vec4(1.0f, 0.98f, 0.92f, 0.0f); // rgb = color, w = intensity
        glm::vec4 lightingCounts = glm::vec4(0.0f); // x = ambient, y = numPointLights, z = numSpotLights, w = hasSkySH
        glm::vec4 skySH[9] = {};
        GizmoPointLightGPU pointLights[8] = {};
        GizmoSpotLightGPU spotLights[4] = {};
    };

    class IGN_API GizmoRenderer
    {
    public:
        GizmoRenderer();
        ~GizmoRenderer();

        void Render(
            nri::CommandBuffer *cmd,
            RenderTarget *gizmoFB,
            ICamera *camera,
            const GizmoDrawList &drawList,
            const Ref<Texture> &sceneDepthTexture,
            const Scene_GPUData *sceneGPUData = nullptr,
            const PointLightBufferData *pointLights = nullptr,
            const SpotLightBufferData *spotLights = nullptr,
            const GI_GPUData *giData = nullptr);

        void ClearPipelineCache();

    private:
        struct MeshSlice
        {
            uint32_t firstIndex = 0;
            uint32_t indexCount = 0;
            uint32_t vertexOffset = 0;
        };

        struct FrameSlotResources
        {
            nri::Buffer *instanceBuffer = nullptr;
            nri::Descriptor *instanceSRV = nullptr;
            uint8_t *instanceMapped = nullptr;
            size_t instanceCapacity = 0;

            Ref<VertexBuffer> dynamicVertexBuffer;
            size_t dynamicVertexCapacity = 0;

            std::unordered_map<uint64_t, Ref<BindingSet>> bindingSetCache;
        };

        void BuildUnitMeshes();
        void EnsureFrameSlotBuffers(FrameSlotResources &slot, size_t requiredInstances, size_t requiredDynamicVertices);
        Ref<GraphicsPipeline> GetOrCreatePipeline(RenderTarget *framebuffer, bool depthWrite);
        Ref<BindingSet> GetOrCreateBindingSet(FrameSlotResources &slot, nri::PipelineLayout *layout, const Ref<Texture> &depthTexture);

    private:
        static constexpr uint32_t kMaxSlots = 3;

        Ref<VertexBuffer> m_MeshVertexBuffer;
        Ref<IndexBuffer> m_MeshIndexBuffer;
        std::array<MeshSlice, 6> m_MeshSlices{};

        ConstantBuffer m_FrameConstantBuffer;
        std::array<FrameSlotResources, kMaxSlots> m_FrameSlots{};
        uint32_t m_CurrentSlot = 0;

        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_OpaquePSOCache;
        std::unordered_map<FramebufferKey, Ref<GraphicsPipeline>, FramebufferKeyHash> m_TranslucentPSOCache;
    };
}

#endif

