// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GI_BAKER_HPP
#define IGN_GI_BAKER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/gi/scene_capture_renderer.hpp"
#include "ignite/graphics/gi/ibl_prefilter.hpp"
#include "ignite/graphics/gi/irradiance_volume.hpp"
#include "ignite/graphics/gi/gi_asset_io.hpp"
#include "ignite/graphics/gi/sh.hpp"

#include <NRI.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <vector>

namespace ignite
{
    class Scene;
    class AssetManager;
    class Entity;
    class TransformComponent;
    class IrradianceVolumeComponent;

    // Orchestrates offline/in-editor irradiance probe grid baking using SceneCaptureRenderer
    // and IBLPrefilter SH projection, supporting both synchronous and time-sliced execution.
    class IGN_API GIBaker
    {
    public:
        static constexpr uint32_t kMaxBatchProbes = SceneCaptureRenderer::kMaxCaptureSlots;

        GIBaker() = default;
        ~GIBaker();

        static void ComputeVolumeGrid(const TransformComponent &tr, const IrradianceVolumeComponent &vol,
            glm::vec3 &outBoundsMin, glm::ivec3 &outDims, float &outSpacing);

        bool BakeVolumeSync(Scene *scene, AssetManager *assetManager, Entity volumeEntity,
            IrradianceVolumeData &outData, const std::filesystem::path &savePath = {});

        bool BakeReflectionProbes(Scene *scene, AssetManager *assetManager, const IrradianceVolume *activeVolume,
            const std::filesystem::path &outputDir, std::vector<std::filesystem::path> *outSavedPaths = nullptr);

        bool StartBake(Scene *scene, AssetManager *assetManager, Entity volumeEntity,
            const std::filesystem::path &savePath = {});

        // Advances baking by up to maxProbesThisStep probes. Returns true when baking is still in progress.
        bool StepBake(uint32_t maxProbesThisStep = 16);
        void CancelBake();

        bool IsBaking() const { return m_IsBaking; }
        bool DidFinishSuccessfully() const { return m_FinishedSuccessfully; }
        float GetProgress() const;
        uint32_t GetCompletedProbes() const { return m_CompletedProbes; }
        uint32_t GetTotalProbes() const { return m_TotalProbes; }
        const IrradianceVolumeData &GetLastBakedData() const { return m_BakedData; }

    private:
        enum class BakePhase : uint8_t
        {
            Idle = 0,
            BounceCapture,
            RelocationCapture
        };

        void EnsureCommandResources();
        void EnsureBatchBuffers();
        void SubmitAndWaitCommandBuffer();
        bool PrepareBounce(uint32_t bounceIndex);
        void DilateInvalidProbes();
        void PackWorkingData(IrradianceVolumeData &outData, bool includeVisibilityAndOffsets) const;
        void FinalizeBakedData();
        bool AdvanceAfterBounce();
        glm::vec3 GetProbeBasePosition(uint32_t probeIdx) const;

        SceneCaptureRenderer m_Capturer;
        IBLPrefilter m_Prefilter;

        nri::CommandAllocator *m_CmdAllocator = nullptr;
        nri::CommandBuffer *m_CmdBuffer = nullptr;
        nri::Fence *m_Fence = nullptr;
        uint64_t m_FenceValue = 1;

        nri::Buffer *m_BatchSHGpuBuffer = nullptr;
        nri::Buffer *m_BatchSHReadbackBuffer = nullptr;
        nri::Descriptor *m_BatchSHGpuUAV = nullptr;
        bool m_BatchSHWritten = false;

        nri::Buffer *m_BatchStatsGpuBuffer = nullptr;
        nri::Buffer *m_BatchStatsReadbackBuffer = nullptr;
        nri::Descriptor *m_BatchStatsGpuUAV = nullptr;

        nri::Buffer *m_BatchOctGpuBuffer = nullptr;
        nri::Buffer *m_BatchOctReadbackBuffer = nullptr;
        nri::Descriptor *m_BatchOctGpuUAV = nullptr;
        bool m_BatchVisWritten = false;

        Scene *m_Scene = nullptr;
        AssetManager *m_AssetManager = nullptr;
        std::filesystem::path m_SavePath;
        uint64_t m_BakeHash = 0;

        BakePhase m_Phase = BakePhase::Idle;
        glm::vec3 m_BoundsMin = glm::vec3(0.0f);
        glm::ivec3 m_Dims = glm::ivec3(0);
        float m_Spacing = 1.5f;
        uint32_t m_Resolution = 32;
        uint32_t m_ProbeCount = 0;
        uint32_t m_CurrentBounce = 0;
        uint32_t m_TotalBounces = 1;
        uint32_t m_NextProbeIndex = 0;
        uint32_t m_CompletedProbes = 0;
        uint32_t m_TotalProbes = 0;

        std::vector<gi::SHL2> m_ProbeSH;
        std::vector<float> m_ProbeValidity;
        std::vector<glm::vec3> m_ProbeOffsets;
        std::vector<float> m_ProbeState;
        std::vector<std::array<glm::vec2, 100>> m_ProbeVisTiles;

        std::vector<uint32_t> m_RelocationQueue;
        std::vector<uint8_t> m_CandidateFromInvalid;
        uint32_t m_RelocationCursor = 0;

        Ref<IrradianceVolume> m_PrevBounceVolume;
        GI_GPUData m_PrevBounceGIData{};

        IrradianceVolumeData m_BakedData;
        bool m_IsBaking = false;
        bool m_FinishedSuccessfully = false;
    };
}

#endif

