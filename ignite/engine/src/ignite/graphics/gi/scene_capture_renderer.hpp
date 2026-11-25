// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SCENE_CAPTURE_RENDERER_HPP
#define IGN_SCENE_CAPTURE_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/frame_context.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/objects/environment.hpp"

#include <NRI.h>
#include <glm/glm.hpp>
#include <array>
#include <unordered_map>
#include <vector>

namespace ignite
{
    class Scene;
    class AssetManager;
    class Material;

    enum class CaptureMode : int32_t
    {
        Irradiance = 1,
        Reflection = 2
    };

    struct CaptureSettings
    {
        uint32_t resolution = 32;
        float nearPlane = 0.05f;
        float farPlane = 500.0f;
        CaptureMode mode = CaptureMode::Irradiance;
        bool includeSky = true;
        bool enablePreviousGI = false;
        float excludeOriginRadius = 0.0f;
        GI_GPUData previousGIData{};
    };

    struct CaptureResult
    {
        Ref<Texture> colorCube;
        Ref<Texture> depthCube;
    };

    // Renders an arbitrary world-space HDR cubemap of a Scene using isolated GPU uniform/instance
    // buffers so active viewport FrameContext resources are never mutated.
    class IGN_API SceneCaptureRenderer
    {
    public:
        static constexpr uint32_t kMaxCaptureSlots = 16;
        static constexpr uint32_t kMaxBakePointShadows = 8;
        static constexpr uint32_t kMaxBakeSpotShadows = 4;
        static constexpr uint32_t kMaxBakeLocalShadowLights = kMaxBakePointShadows + kMaxBakeSpotShadows;
        static constexpr uint32_t kBakeLocalShadowResolution = 256;

        SceneCaptureRenderer();
        ~SceneCaptureRenderer();

        // Computes Vulkan-oriented cubemap face view/projection matrices (with proj[1][1] *= -1).
        static void GetCubemapFaceViewProj(const glm::vec3 &pos, uint32_t face, float nearPlane, float farPlane,
            glm::mat4 &outView, glm::mat4 &outProj);

        // Extracts contributeGI static meshes, lights with bakeIndirect, computes tight scene AABB,
        // uploads GPU instance buffers, and renders the single ortho bake shadow map into cascade 0.
        void PrepareScene(Scene *scene, AssetManager *assetManager, nri::CommandBuffer *cmd, const GI_GPUData *giDataOverride = nullptr);

        // Updates only the previous-bounce GI override without re-uploading scene geometry or shadow maps.
        void SetOverrideGIData(const GI_GPUData *giDataOverride)
        {
            m_HasOverrideGIData = (giDataOverride != nullptr);
            if (giDataOverride)
            {
                m_OverrideGIData = *giDataOverride;
            }
        }

        // Renders all 6 faces at worldPos into cached colorCube + depthCube targets using the
        // currently prepared scene. Must be called outside RenderTarget::BeginRendering.
        CaptureResult CaptureCubemap(nri::CommandBuffer *cmd, const glm::vec3 &worldPos, const CaptureSettings &settings);

        // Convenience synchronous capture that records PrepareScene + CaptureCubemap onto a dedicated
        // command buffer, submits to the graphics queue, and waits on a fence before returning.
        CaptureResult CaptureCubemapSync(Scene *scene, AssetManager *assetManager, const glm::vec3 &worldPos, const CaptureSettings &settings);

        // Advances the internal uniform slot counter (called automatically by CaptureCubemap).
        // Ensures distinct command buffer captures inside a batch do not overwrite in-flight uniform slots.
        void ResetCaptureSlotCursor() { m_CaptureSlotCursor = 0; }

        const AABB &GetPreparedSceneBounds() const { return m_SceneBounds; }
        size_t GetPreparedBatchCount() const { return m_PreparedBatches.size(); }
        Ref<Texture> GetPreparedEnvironmentTexture() const { return m_PreparedEnvTexture; }
        SkyType GetPreparedSkyType() const { return m_ActiveSkyType; }
        bool IsPreparedSkyIBLEnabled() const { return m_HasPreparedSkyIBL; }

        // Returns a persistent per-resolution colorCube + depthCube if already allocated.
        CaptureResult GetCachedCaptureTargets() const { return { m_ColorCube, m_DepthCube }; }

    private:
        struct PreparedBatch
        {
            nri::Buffer *vertexBuffer = nullptr;
            nri::Buffer *indexBuffer = nullptr;
            uint32_t indexCount = 0;
            uint32_t vertexStride = 0;
            uint32_t baseInstanceOffset = 0;
            uint32_t instanceCount = 0;
            uint32_t shadowBaseInstanceOffset = 0;
            uint32_t shadowInstanceCount = 0;
            Ref<BindingSet> materialBindingSet;
            nri::CullMode cullMode = nri::CullMode::FRONT;
            std::vector<uint32_t> instanceObjectIndices;
            std::vector<AABB> instanceWorldAABBs;
        };

        struct CaptureUniformSlot
        {
            std::array<Ref<ConstantBuffer>, 6> faceCameraBuffers;
            Ref<ConstantBuffer> sceneBuffer;
            Ref<ConstantBuffer> giBuffer;
            std::array<Ref<BindingSet>, 6> staticFaceBindingSets;
        };

        void EnsureCommonGPUResources();
        void EnsureCaptureTargets(uint32_t resolution);
        void EnsureLocalShadowLightResources(uint32_t lightSlot);
        void EnsureSyncCommandResources();
        Ref<GraphicsPipeline> GetOrCreateMeshPSO(nri::CullMode cullMode);
        Ref<GraphicsPipeline> GetOrCreateShadowPSO();
        Ref<GraphicsPipeline> GetOrCreateLocalShadowPSO();
        Ref<GraphicsPipeline> GetOrCreateSkyPSO();

        static nri::CullMode InvertCullModeForYFlip(nri::CullMode cullMode);

        // Isolated scene-wide GPU buffers (never aliases SceneRenderer::FrameContext)
        ObjectBuffer m_ObjectBuffer;
        InstanceIndexBuffer m_InstanceIndexBuffer;
        ReflectionProbeBuffer m_ReflectionProbeBuffer;
        ObjectAllocator m_ObjectAllocator;
        InstanceIndexAllocator m_InstanceIndexAllocator;

        Ref<ConstantBuffer> m_PointLightBuffer;
        Ref<ConstantBuffer> m_SpotLightBuffer;
        Ref<ConstantBuffer> m_ShadowSceneBuffer;
        Ref<ConstantBuffer> m_ShadowCameraBuffer;
        Ref<ConstantBuffer> m_ShadowGIBuffer;
        Ref<BindingSet> m_ShadowBindingSet;

        std::array<CaptureUniformSlot, kMaxCaptureSlots> m_CaptureSlots;
        uint32_t m_CaptureSlotCursor = 0;

        Ref<CascadedShadowMap> m_BakeShadowMap;
        Ref<Environment> m_ActiveEnvironment;
        Ref<Texture> m_PreparedEnvTexture;
        SkyType m_ActiveSkyType = SkyType::HDRI;
        bool m_HasPreparedSkyIBL = false;
        bool m_HasDirectionalLight = false;
        glm::vec3 m_SunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
        Scene_GPUData m_BaseSceneData{};
        GI_GPUData m_OverrideGIData{};
        bool m_HasOverrideGIData = false;
        AABB m_SceneBounds = AABB::FromMinMax(glm::vec3(-20.0f), glm::vec3(20.0f));

        std::vector<PreparedBatch> m_PreparedBatches;
        std::vector<Ref<Material>> m_RetainedMaterials;

        // Cached capture targets keyed by resolution
        uint32_t m_TargetResolution = 0;
        Ref<Texture> m_ColorCube;
        Ref<Texture> m_DepthCube;
        Ref<Texture> m_ScratchIDTexture;
        std::array<Ref<RenderTarget>, 6> m_FaceRenderTargets;

        std::unordered_map<uint8_t, Ref<GraphicsPipeline>> m_MeshPSOCache;
        Ref<GraphicsPipeline> m_ShadowPSO;
        Ref<GraphicsPipeline> m_LocalShadowPSO;
        Ref<GraphicsPipeline> m_SkyPSO;

        Ref<Texture> m_BakeLocalShadowCubeArray;
        Ref<Texture> m_LocalShadowDepthTex;
        std::array<Ref<RenderTarget>, kMaxBakeLocalShadowLights * 6> m_LocalShadowFaceRTs;
        std::array<Ref<ConstantBuffer>, kMaxBakeLocalShadowLights * 6> m_LocalShadowFaceCameraBuffers;
        std::array<Ref<BindingSet>, kMaxBakeLocalShadowLights * 6> m_LocalShadowFaceBindingSets;
        bool m_HasBakeLocalShadows = false;

        // Dedicated command resources for CaptureCubemapSync
        nri::CommandAllocator *m_SyncCmdAllocator = nullptr;
        nri::CommandBuffer *m_SyncCmdBuffer = nullptr;
        nri::Fence *m_SyncFence = nullptr;
        uint64_t m_SyncFenceValue = 1;

        bool m_CommonInitialized = false;
    };
}

#endif

