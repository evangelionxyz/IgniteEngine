// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_ISCENE_RENDERER_HPP
#define IGN_ISCENE_RENDERER_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/base.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/graphics/bloom.hpp"
#include "ignite/graphics/ssao.hpp"
#include "ignite/graphics/outline_jfa.hpp"
#include "ignite/graphics/gizmo/gizmo.hpp"
#include "ignite/graphics/gizmo/gizmo_renderer.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"

#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/shadow_map.hpp"
#include "ignite/graphics/texture.hpp"

#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/signal_bus.hpp"

#include <array>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ignite
{
    class Scene;
    class ICamera;
    class Renderer2D;
    class CascadedShadowMap;
    class WorldEnvironment;

    enum class SampleMethod : uint8_t
    {
        NONE = 0,
        TAA,
        MSAA,
        FXAA,
        None = NONE
    };

    enum class ScalabilityLevel : uint8_t
    {
        Low = 0,
        Medium = 1,
        High = 2,
        Ultra = 3
    };

    struct GraphicsScalability
    {
        ScalabilityLevel overall = ScalabilityLevel::High;
        ScalabilityLevel shadows = ScalabilityLevel::High;
        ScalabilityLevel globalIllumination = ScalabilityLevel::High;
        ScalabilityLevel ambientOcclusion = ScalabilityLevel::High;
        ScalabilityLevel reflections = ScalabilityLevel::High;
        ScalabilityLevel resolutionScale = ScalabilityLevel::High;
        ScalabilityLevel postProcessing = ScalabilityLevel::High;
    };

    struct DebugGridStyle
    {
        bool enabled = true;
        bool enableXAxis = true;
        bool enableYAxis = true;
        bool enableZAxis = true;

        float cellSize = 0.25f;
        float minPixelsBetweenCells = 10.0f;
        float gridSize = 300.0f;
        float majorLineScale = 8.0f;

        glm::vec4 thinColor = glm::vec4(0.789f, 0.789f, 0.789f, 1.0f);
        glm::vec4 thickColor = glm::vec4(0.456f, 0.456f, 0.456f, 1.0f);
        glm::vec4 xAxisColor = glm::vec4(0.96f, 0.29f, 0.29f, 1.0f);
        glm::vec4 yAxisColor = glm::vec4(0.29f, 0.96f, 0.29f, 1.0f);
        glm::vec4 zAxisColor = glm::vec4(0.29f, 0.52f, 0.96f, 1.0f);
    };

    struct IGN_API SceneRenderSettings
    {
        DebugGridStyle worldGrid3D;
        DebugGridStyle worldGrid2D;

        bool showBoundingBox = false;
        bool showPhysicsCollider = false;
        bool showEntityNames = false;
        bool showGIProbes = false;
        bool renderEnvironment = true;

        int giModeOverride = -1;     // -1 = Volume default, 0 = Off, 1 = Fast, 2 = LeakResistant
        int aoModeOverride = -1;     // -1 = Camera PostProcessing, 0 = Off, 1 = LegacySSAO, 2 = GTAO
        bool enableDepthPrepass = true;
        bool enableSpecularAA = true;
        bool enableSSR = true;
        float ssrIntensity = 1.0f;
        float ssrMaxRoughness = 0.6f;

        float gtaoResolutionScale = 0.5f;
        uint32_t gtaoSliceCount = 3;
        uint32_t gtaoStepsPerSlice = 3;
        uint32_t gtaoDenoiseRadius = 3;
        uint32_t gtaoUpsampleMode = 1; // 0 = half-res bilinear, 1 = fast 2x2 bilateral, 2 = full 3x3 bilateral

        float ssrResolutionScale = 0.5f;
        uint32_t ssrMaxSteps = 32;
        uint32_t ssrBinarySearchSteps = 4;

        float bloomResolutionScale = 0.75f;
        int bloomMaxIterations = 5;

        SampleMethod sampleMethod = SampleMethod::FXAA;
        TAAProperties taaProperties;   // Current-frame weight; lower is smoother, higher is more responsive
        MSAAProperties msaaProperties; // Requested MSAA sample count for compatible render paths
        FXAAProperties fxaaProperties; // Tunable FXAA quality/threshold parameters
        bool enableFXAA = true;
        float renderScale = 1.0f;

        GraphicsScalability scalability;
        ShadowMapQuality shadowQuality = ShadowMapQuality::HIGH;

        nri::FillMode fillMode = nri::FillMode::SOLID;

        SceneRenderSettings()
        {
            worldGrid2D.enableZAxis = false;
            worldGrid2D.gridSize = 100.0f;
            SetSampleMethod(SampleMethod::FXAA);
        }

        void ApplyOverallScalability(ScalabilityLevel level);
        void ApplyIndividualScalability();
        bool SerializeToFile(const std::filesystem::path &filepath) const;
        bool DeserializeFromFile(const std::filesystem::path &filepath);

        void SetSampleMethod(SampleMethod method)
        {
            sampleMethod = method;
            enableFXAA = (method == SampleMethod::FXAA);
            fxaaProperties.enable = enableFXAA;
            msaaProperties.enable = (method == SampleMethod::MSAA);
            taaProperties.enable = (method == SampleMethod::TAA);
        }
    };

    struct DebugGrid_GPUData
    {
        glm::vec4 thinColor = glm::vec4(0.0f);
        glm::vec4 thickColor = glm::vec4(0.0f);
        glm::vec4 xAxisColor = glm::vec4(0.0f);
        glm::vec4 yAxisColor = glm::vec4(0.0f);
        glm::vec4 zAxisColor = glm::vec4(0.0f);
        glm::vec4 settings0 = glm::vec4(0.0f); // x=cellSize y=minPixelsBetweenCells z=gridSize w=majorLineScale
        glm::vec4 settings1 = glm::vec4(0.0f); // x=planeMode(0:XZ, 1:XY) y=enableX z=enableY w=enableZ
    };

    struct CompositePostProcess_GPUData
    {
        glm::vec4 flags = glm::vec4(0.0f); // x=enableBloom, y=bloomIntensity, z=enableVignette, w=enableChromAb
        glm::vec4 vignetteParams = glm::vec4(0.0f); // x=radius, y=softness, z=intensity, w=chromAbAmount
        glm::vec4 chromAbParams = glm::vec4(0.0f); // x=chromAbRadial, y=enableSSAO, z=ssaoIntensity
        glm::vec4 vignetteColor = glm::vec4(0.0f);

        int tonemapMode = 0;
        float exposure = 1.1f;
        float gamma = 2.2f;
        int enableDOF = 0;

        float focalLength = 120.0f;
        float focalDistance = 5.5f;
        float fStop = 1.4f;
        float focusRange = 5.0f;

        float blurAmount = 1.0f;
        float padding_dof[3] = { 0.0f, 0.0f, 0.0f };

        glm::vec4 fogColor = glm::vec4(0.5f, 0.6f, 0.7f, 1.0f);
        float fogDensity = 0.0f;
        float fogStart = 10.0f;
        float fogEnd = 100.0f;
        float padding_fog = 0.0f;

        glm::vec4 taaParams = glm::vec4(0.0f); // x=enableTAA, y=currentFrameWeight, z=historyValid, w=enableOutline
        glm::vec4 taaJitter = glm::vec4(0.0f); // xy=currJitterUV, zw=prevJitterUV

        glm::mat4 projectionInv = glm::mat4(1.0f);
        glm::mat4 taaReprojection = glm::mat4(1.0f); // prevUnjitteredViewProj * inv(currUnjitteredViewProj)
    };

    struct FXAA_GPUData
    {
        glm::vec2 inverseScreenSize = glm::vec2(1.0f / 1920.0f, 1.0f / 1080.0f);
        float subpixCap = 0.75f;
        float edgeThreshold = 0.125f;
        float edgeThresholdMin = 0.0417f;
        float bypass = 0.0f;
        float padding[2] = { 0.0f, 0.0f };
    };

    class IGN_API ISceneRenderer
    {
    public:
        ISceneRenderer();
        virtual ~ISceneRenderer();

        virtual void ResizeFramebuffer(ICamera *camera, uint32_t width, uint32_t height);

        int GetRenderMode() const { return m_SceneGPUData.renderMode; }
        void SetRenderMode(int renderMode) { m_SceneGPUData.renderMode = renderMode; }

        int GetDebugShadowMode() const { return m_SceneGPUData.debugShadow; }
        void SetDebugShadowMode(int debugShadow) { m_SceneGPUData.debugShadow = debugShadow; }

        PostProcessing &GetPostProcessingSettings() { return m_PostProcessing; }
        const PostProcessing &GetPostProcessingSettings() const { return m_PostProcessing; }
        const glm::uvec2 GetViewportSize() const { return { m_ViewportWidth, m_ViewportHeight }; }

        SampleMethod GetSampleMethod() const { return sceneRenderSettings.sampleMethod; }
        void SetSampleMethod(SampleMethod method);

        void SetGizmoDrawList(const GizmoDrawList &drawList) { m_GizmoDrawList = drawList; }
        void ClearGizmoDrawList() { m_GizmoDrawList.Clear(); }
        const GizmoDrawList &GetGizmoDrawList() const { return m_GizmoDrawList; }

        virtual Ref<Texture> GetEnvironmentMapColorTexture();
        virtual Ref<Texture> GetCascadedShadowMapDepthTexture();
        virtual Ref<CascadedShadowMap> GetCascadedShadowMap();

        template<typename T>
        Ref<T> ResolveAsset(AssetHandle handle)
        {
            if (handle == AssetHandle(0))
                return nullptr;

            if constexpr (std::is_same_v<T, StaticMesh> || std::is_same_v<T, Asset>)
            {
                if (Renderer::IsDefaultMeshHandle(handle))
                {
                    if (Ref<StaticMesh> defaultMesh = Renderer::GetDefaultMeshByHandle(handle))
                    {
                        RetainAssetForCurrentCommandBuffer(defaultMesh);
                        return std::static_pointer_cast<T>(defaultMesh);
                    }
                }
            }

            auto assetManager = AssetManager::GetInstance();
            if (!assetManager)
                return nullptr;

            if (auto project = assetManager->LockActiveProject())
            {
                DrainDirtyResolvedAssets();

                AssetResolveKey key{ project.get(), handle };
                auto it = m_ResolvedAssetsCache.find(key);
                if (it != m_ResolvedAssetsCache.end())
                {
                    if (Ref<Asset> cached = it->second.lock())
                    {
                        RetainAssetForCurrentCommandBuffer(cached);
                        return cached->As<T>();
                    }
                    m_ResolvedAssetsCache.erase(it);
                }

                Ref<T> asset = assetManager->GetAsset<T>(handle);
                if (asset)
                {
                    m_ResolvedAssetsCache.emplace(key, asset);
                    RetainAssetForCurrentCommandBuffer(asset);
                }

                return asset;
            }

            return nullptr;
        }

        SceneRenderSettings sceneRenderSettings;

    protected:
        struct CommandBufferSlot
        {
            nri::CommandAllocator *allocator = nullptr;
            nri::CommandBuffer *commandBuffer = nullptr;
            uint64_t fenceValue = 0;
            std::vector<Ref<Asset>> retainedAssets;
            std::unordered_set<const Asset *> retainedAssetSet;
            std::vector<Ref<RenderTarget>> retainedRenderTargets;
        };

        nri::CommandBuffer *BeginCommandBuffer();
        void SubmitCommandBuffer(nri::CommandBuffer *cmd);
        void WaitForPreviousCommandBuffer();
        uint32_t GetActiveCommandBufferSlotIndex() const { return m_ActiveSlotIndex; }
        void RetainAssetForCurrentCommandBuffer(const Ref<Asset> &asset);
        void RetainRenderTargetForCurrentCommandBuffer(const Ref<RenderTarget> &renderTarget);
        void ClearRetainedCommandBufferAssets();

        static void FillBoneArray(glm::mat4 (&out)[MAX_BONES], const std::vector<glm::mat4> &boneTransforms);
        Ref<Material> ResolveMeshMaterial(AssetHandle materialHandle);

        void EnsureSceneEnvironmentMap();
        void DrainDirtyResolvedAssets();

        Ref<CascadedShadowMap> m_CascadedShadowMap;

        Ref<VertexBuffer> m_CompositeVertexBuffer;
        nri::Descriptor *m_CompositeSampler = nullptr;

        Ref<Renderer2D> m_Renderer2D;
        Ref<OutlineJFA> m_OutlineJFA;
        Ref<GizmoRenderer> m_GizmoRenderer;
        GizmoDrawList m_GizmoDrawList;

        ConstantBuffer m_CompositePostProcessBuffer;
        ConstantBuffer m_FXAABuffer;
        ConstantBuffer m_DebugGridBuffer;
        ConstantBuffer m_CascadedShadowMapBuffer;
        Ref<ConstantBuffer> m_CSMPerCascadeBuffers[NUM_CASCADES];

        std::unordered_map<AssetResolveKey, WeakRef<Asset>, AssetResolveKeyHash> m_ResolvedAssetsCache;
        std::mutex m_DirtyResolvedAssetsMutex;
        std::unordered_set<AssetHandle> m_DirtyResolvedHandles;
        SignalToken m_AssetChangeSignalToken = kInvalidSignalToken;

        WorldEnvironment *m_WorldEnvironment = nullptr;
        PostProcessing m_PostProcessing;
        CompositePostProcess_GPUData m_PostProcessingData;

        Ref<BindingSet> m_MeshBindingSet;
        Scene_GPUData m_SceneGPUData;

        std::vector<uint32_t> m_SelectedEntities;
        Ref<Scene> m_Scene;
        nri::Device *m_Device = nullptr;

        std::array<CommandBufferSlot, 6> m_CommandBufferSlots;
        nri::Fence *m_CommandFence = nullptr;
        uint64_t m_NextFenceValue = 1;
        uint32_t m_CurrentSlotIndex = 0;
        uint32_t m_ActiveSlotIndex = 0;
        bool m_IsCommandBufferRecording = false;

        // This is the actual viewport resolution
        // Not the scaled one
        uint32_t m_ViewportWidth = 0;
        uint32_t m_ViewportHeight = 0;

        bool m_Has2DPreRenderCache = false;
        bool m_CompositeVertexBufferUploadPending = true;
    };
}

#endif
