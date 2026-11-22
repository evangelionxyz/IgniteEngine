// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_IBL_PREFILTER_HPP
#define IGN_IBL_PREFILTER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/graphics/gi/sh.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/shader.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"

#include <NRI.h>
#include <array>
#include <cstdint>

namespace ignite
{
    struct ProbeSHOutputGPU
    {
        glm::vec4 coeff[9] = {};
        float backfaceFraction = 0.0f;
        float totalSolidAngle = 0.0f;
        glm::vec2 _pad = glm::vec2(0.0f);
    };

    // GPU compute prefiltering utility for sky environments and reflection probes:
    //  1) Equirectangular HDRI or procedural sky-view LUT -> cubemap base mip
    //  2) Full 2x2 cubemap mip chain downsampling
    //  3) GGX importance-sampled specular prefilter mip chain (Karis split-sum)
    //  4) L2 SH projection + irradiance cosine convolution
    class IGN_API IBLPrefilter
    {
    public:
        static constexpr uint32_t kMaxMipLevels = 12;
        static constexpr uint32_t kMaxProjectSHCBCapacity = 64;

        IBLPrefilter();
        ~IBLPrefilter();

        // Populates mip 0 of dstCube (6 array layers) from a 2D HDRI or ProceduralSky LUT texture.
        // Transitions all mips of dstCube to SHADER_RESOURCE_STORAGE for subsequent mip generation.
        void ConvertEnv2DToCube(nri::CommandBuffer *cmd, const Ref<Texture> &srcEnv2D, const Ref<Texture> &dstCube,
            int32_t skyType, float cameraAltitudeKm);

        // Generates mips 1..M-1 of cube via 2x2 box downsampling from mip m-1 -> mip m,
        // leaving all mips in SHADER_RESOURCE layout with accurate Texture state tracking.
        void GenerateCubeMips(nri::CommandBuffer *cmd, const Ref<Texture> &cube);

        // Prefilters srcCubeWithMips into dstPrefilterCube using Karis GGX importance sampling,
        // leaving all mips of dstPrefilterCube in SHADER_RESOURCE layout.
        void PrefilterGGX(nri::CommandBuffer *cmd, const Ref<Texture> &srcCubeWithMips, const Ref<Texture> &dstPrefilterCube,
            uint32_t sampleCount = 64);

        // Convenience synchronous helper (submits + waits on fence): generates mips on srcCube and prefilters into dstPrefilterCube.
        bool PrefilterCubeSync(const Ref<Texture> &srcCube, const Ref<Texture> &dstPrefilterCube, uint32_t sampleCount = 64);

        // Synchronous helper for mip-0 only capture targets (e.g. SceneCaptureRenderer::CaptureCubemap output):
        // copies mip 0 into an internal mipped UAV cubemap, generates its mip chain, and prefilters into dstPrefilterCube.
        bool PrefilterCubeFromMip0Sync(const Ref<Texture> &srcMip0Cube, const Ref<Texture> &dstPrefilterCube, uint32_t sampleCount = 64);

        // Projects mip 0 of srcCube into L2 SH and writes ProbeSHOutputGPU[probeIndex] into outSHBufferUAV.
        // Transitions srcCube to SHADER_RESOURCE if needed and dispatches 1 group (64 threads).
        void ProjectCubeToSH(nri::CommandBuffer *cmd, const Ref<Texture> &srcCube, nri::Descriptor *outSHBufferUAV,
            uint32_t probeIndex = 0, bool convolveOverPi = true, float hanningWindow = 0.0f);

        // Synchronous helper for single-probe GPU SH projection with readback (primarily tests / offline queries).
        bool ProjectCubeToSHSync(const Ref<Texture> &srcCube, gi::SHL2 &outSH, float *outBackfaceFraction = nullptr,
            bool convolveOverPi = true, float hanningWindow = 0.0f);

        // Computes octahedral visibility moments (8x8 + 1px border = 10x10 float2 tile) and backface/frontface
        // relocation statistics from captured colorCube + depthCube into outStatsUAV[probeLocalIndex] and
        // outOctTilesUAV[probeLocalIndex * 100 .. +99].
        void ComputeProbeVisibility(nri::CommandBuffer *cmd, const Ref<Texture> &colorCube, const Ref<Texture> &depthCube,
            nri::Descriptor *outStatsUAV, nri::Descriptor *outOctTilesUAV, uint32_t probeLocalIndex,
            float nearPlane, float farPlane, float maxDistance, float sharpness = 50.0f);

    private:
        void EnsureInitialized();
        void EnsureSingleSHBuffers();
        void EnsureSyncCommandResources();
        static void TransitionTextureRange(nri::CommandBuffer *cmd, const Ref<Texture> &tex,
            uint32_t mipOffset, uint32_t mipNum,
            nri::Layout beforeLayout, nri::AccessBits beforeAccess, nri::StageBits beforeStages,
            nri::Layout afterLayout, nri::AccessBits afterAccess, nri::StageBits afterStages);

        bool m_Initialized = false;

        nri::Descriptor *m_LinearClampSampler = nullptr;

        BindingLayoutDesc m_EquirectLayout;
        BindingLayoutDesc m_DownsampleLayout;
        BindingLayoutDesc m_PrefilterLayout;
        BindingLayoutDesc m_ProjectSHLayout;
        BindingLayoutDesc m_ProbeVisLayout;

        nri::PipelineLayout *m_EquirectPipelineLayout = nullptr;
        nri::PipelineLayout *m_DownsamplePipelineLayout = nullptr;
        nri::PipelineLayout *m_PrefilterPipelineLayout = nullptr;
        nri::PipelineLayout *m_ProjectSHPipelineLayout = nullptr;
        nri::PipelineLayout *m_ProbeVisPipelineLayout = nullptr;

        nri::Pipeline *m_EquirectPipeline = nullptr;
        nri::Pipeline *m_DownsamplePipeline = nullptr;
        nri::Pipeline *m_PrefilterPipeline = nullptr;
        nri::Pipeline *m_ProjectSHPipeline = nullptr;
        nri::Pipeline *m_ProbeVisPipeline = nullptr;

        Ref<Shader> m_EquirectShader;
        Ref<Shader> m_DownsampleShader;
        Ref<Shader> m_PrefilterShader;
        Ref<Shader> m_ProjectSHShader;
        Ref<Shader> m_ProbeVisShader;

        Ref<ConstantBuffer> m_EquirectCB;
        std::array<Ref<ConstantBuffer>, kMaxMipLevels> m_DownsampleMipCBs;
        std::array<Ref<ConstantBuffer>, kMaxMipLevels> m_PrefilterMipCBs;
        std::array<Ref<ConstantBuffer>, kMaxProjectSHCBCapacity> m_ProjectSHCBs;
        std::array<Ref<ConstantBuffer>, kMaxProjectSHCBCapacity> m_ProbeVisCBs;
        uint32_t m_ProjectSHCBCursor = 0;
        uint32_t m_ProbeVisCBCursor = 0;

        nri::Buffer *m_SingleSHGpuBuffer = nullptr;
        nri::Buffer *m_SingleSHReadbackBuffer = nullptr;
        nri::Descriptor *m_SingleSHGpuUAV = nullptr;
        bool m_SingleSHGpuWritten = false;

        nri::CommandAllocator *m_SyncCmdAllocator = nullptr;
        nri::CommandBuffer *m_SyncCmdBuffer = nullptr;
        nri::Fence *m_SyncFence = nullptr;
        uint64_t m_SyncFenceValue = 1;
    };
}

#endif

