// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "gi_baker.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/string_utils.hpp"

#include <glm/gtc/packing.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>
#include <utility>
#include <vector>

namespace ignite
{
    namespace
    {
        constexpr uint64_t kFNVOffset = 1469598103934665603ull;
        constexpr uint64_t kFNVPrime = 1099511628211ull;

        struct BakeHashRecord
        {
            uint64_t entity = 0;
            std::vector<uint8_t> bytes;
        };

        void HashBytes(uint64_t &hash, const void *data, size_t size)
        {
            const auto *bytes = static_cast<const uint8_t *>(data);
            for (size_t i = 0; i < size; ++i)
                hash = (hash ^ bytes[i]) * kFNVPrime;
        }

        template<typename T>
        void AppendBytes(std::vector<uint8_t> &out, const T &value)
        {
            const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
            out.insert(out.end(), bytes, bytes + sizeof(T));
        }

        void AppendTag(std::vector<uint8_t> &out, uint32_t tag)
        {
            AppendBytes(out, tag);
        }

        template<typename T>
        void AppendComponent(std::vector<uint8_t> &out, uint32_t tag, const T &component)
        {
            AppendTag(out, tag);
            AppendBytes(out, component);
        }
    }

    uint64_t ComputeSceneBakeHash(const Scene *scene)
    {
        if (!scene || !scene->registry)
            return 0;

        std::vector<BakeHashRecord> records;
        for (entt::entity e : scene->registry->view<IDComponent>())
        {
            const auto &id = scene->registry->get<IDComponent>(e);
            const uint64_t entityID = static_cast<uint64_t>(id.uuid);
            BakeHashRecord record;
            record.entity = entityID;
            AppendBytes(record.bytes, entityID);

            const auto *transform = scene->registry->try_get<TransformComponent>(e);
            const auto *rendering = scene->registry->try_get<RenderingComponent>(e);
            if (transform)
            {
                AppendTag(record.bytes, 1u);
                AppendBytes(record.bytes, transform->world.translation);
                AppendBytes(record.bytes, transform->world.rotation);
                AppendBytes(record.bytes, transform->world.scale);
            }

            if (rendering)
            {
                AppendTag(record.bytes, 2u);
                AppendBytes(record.bytes, rendering->visible);
                AppendBytes(record.bytes, rendering->contributeGI);
                AppendBytes(record.bytes, rendering->receiveGI);
                AppendBytes(record.bytes, static_cast<int>(rendering->mobility));
                AppendBytes(record.bytes, rendering->castShadow);
                AppendBytes(record.bytes, rendering->receiveShadow);
            }

            if (const auto *mesh = scene->registry->try_get<StaticMeshComponent>(e);
                mesh && (!rendering || rendering->IsStaticForGI()))
            {
                AppendTag(record.bytes, 3u);
                AppendBytes(record.bytes, static_cast<uint64_t>(mesh->handle));
                std::vector<std::pair<int, uint64_t>> overrides;
                overrides.reserve(mesh->overrideMaterials.size());
                for (const auto &[slot, handle] : mesh->overrideMaterials)
                    overrides.emplace_back(slot, static_cast<uint64_t>(handle));
                std::ranges::sort(overrides);
                for (const auto &[slot, handle] : overrides)
                {
                    AppendBytes(record.bytes, slot);
                    AppendBytes(record.bytes, handle);
                }
            }

            if (transform)
            {
                if (const auto *light = scene->registry->try_get<DirectionalLightComponent>(e); light && light->bakeIndirect)
                {
                    AppendTag(record.bytes, 10u);
                    AppendBytes(record.bytes, *light);
                }
                if (const auto *light = scene->registry->try_get<PointLightComponent>(e);
                    light && (light->bakeIndirect || (light->castShadow && light->bakeStaticShadow)))
                {
                    AppendTag(record.bytes, 11u);
                    AppendBytes(record.bytes, *light);
                }
                if (const auto *light = scene->registry->try_get<SpotLightComponent>(e);
                    light && (light->bakeIndirect || (light->castShadow && light->bakeStaticShadow)))
                {
                    AppendTag(record.bytes, 12u);
                    AppendBytes(record.bytes, *light);
                }
            }

            if (const auto *environment = scene->registry->try_get<WorldEnvironment>(e))
            {
                AppendTag(record.bytes, 20u);
                AppendBytes(record.bytes, static_cast<uint32_t>(environment->skyType));
                AppendBytes(record.bytes, static_cast<uint64_t>(environment->hdrHandle));
                AppendBytes(record.bytes, environment->exposure);
                AppendBytes(record.bytes, environment->gamma);
                AppendBytes(record.bytes, environment->ambient);
                AppendBytes(record.bytes, environment->enableIBL);
                AppendBytes(record.bytes, environment->atmosphereParams);
            }

            if (record.bytes.size() > sizeof(entityID))
                records.push_back(std::move(record));
        }

        std::ranges::sort(records, [](const BakeHashRecord &a, const BakeHashRecord &b) { return a.entity < b.entity; });
        uint64_t hash = kFNVOffset;
        const uint32_t version = 1u;
        HashBytes(hash, &version, sizeof(version));
        for (const auto &record : records)
            HashBytes(hash, record.bytes.data(), record.bytes.size());
        return hash;
    }

    bool IsSceneGIOutOfDate(const Scene *scene)
    {
        if (!scene || !scene->registry)
            return false;

        auto view = scene->registry->view<IrradianceVolumeComponent>();
        bool hasVolume = false;
        for (entt::entity e : view)
        {
            hasVolume = true;
            const auto &vol = view.get<IrradianceVolumeComponent>(e);
            if (vol.bakeHash == 0 || vol.bakedAssetPath.empty())
            {
                return true;
            }
        }
        if (!hasVolume)
            return false;

        const uint64_t currentHash = ComputeSceneBakeHash(scene);
        for (entt::entity e : view)
        {
            const auto &vol = view.get<IrradianceVolumeComponent>(e);
            if (vol.bakeHash != currentHash)
            {
                return true;
            }
        }
        return false;
    }

    GIBaker::~GIBaker()
    {
        m_PrevBounceVolume.reset();

        auto *dm = DeviceManager::GetInstance();
        if (!dm)
        {
            return;
        }

        const auto &nri = dm->GetCoreInterface();
        GPUUploadSync::DeviceWaitIdle(dm->GetDevice());

        if (m_CmdBuffer) { nri.DestroyCommandBuffer(m_CmdBuffer); m_CmdBuffer = nullptr; }
        if (m_CmdAllocator) { nri.DestroyCommandAllocator(m_CmdAllocator); m_CmdAllocator = nullptr; }
        if (m_Fence) { nri.DestroyFence(m_Fence); m_Fence = nullptr; }

        if (m_BatchSHGpuUAV) { nri.DestroyDescriptor(m_BatchSHGpuUAV); m_BatchSHGpuUAV = nullptr; }
        if (m_BatchSHGpuBuffer) { nri.DestroyBuffer(m_BatchSHGpuBuffer); m_BatchSHGpuBuffer = nullptr; }
        if (m_BatchSHReadbackBuffer) { nri.DestroyBuffer(m_BatchSHReadbackBuffer); m_BatchSHReadbackBuffer = nullptr; }

        if (m_BatchStatsGpuUAV) { nri.DestroyDescriptor(m_BatchStatsGpuUAV); m_BatchStatsGpuUAV = nullptr; }
        if (m_BatchStatsGpuBuffer) { nri.DestroyBuffer(m_BatchStatsGpuBuffer); m_BatchStatsGpuBuffer = nullptr; }
        if (m_BatchStatsReadbackBuffer) { nri.DestroyBuffer(m_BatchStatsReadbackBuffer); m_BatchStatsReadbackBuffer = nullptr; }

        if (m_BatchOctGpuUAV) { nri.DestroyDescriptor(m_BatchOctGpuUAV); m_BatchOctGpuUAV = nullptr; }
        if (m_BatchOctGpuBuffer) { nri.DestroyBuffer(m_BatchOctGpuBuffer); m_BatchOctGpuBuffer = nullptr; }
        if (m_BatchOctReadbackBuffer) { nri.DestroyBuffer(m_BatchOctReadbackBuffer); m_BatchOctReadbackBuffer = nullptr; }
    }

    void GIBaker::ComputeVolumeGrid(const TransformComponent &tr, const IrradianceVolumeComponent &vol,
        glm::vec3 &outBoundsMin, glm::ivec3 &outDims, float &outSpacing)
    {
        const glm::vec3 halfExt = glm::max(glm::abs(vol.extents * tr.world.scale), glm::vec3(0.5f));
        outSpacing = std::max(vol.spacing, 0.25f);
        outDims = glm::clamp(
            glm::ivec3(glm::ceil((halfExt * 2.0f) / outSpacing)) + glm::ivec3(1),
            glm::ivec3(2, 2, 2),
            glm::ivec3(64, 32, 64));

        const glm::vec3 span = glm::vec3(outDims - glm::ivec3(1)) * outSpacing;
        outBoundsMin = tr.world.translation - span * 0.5f;
    }

    glm::vec3 GIBaker::GetProbeBasePosition(uint32_t probeIdx) const
    {
        const uint32_t planeSize = static_cast<uint32_t>(m_Dims.x) * static_cast<uint32_t>(m_Dims.y);
        const uint32_t iz = probeIdx / std::max(planeSize, 1u);
        const uint32_t rem = probeIdx - iz * planeSize;
        const uint32_t iy = rem / static_cast<uint32_t>(std::max(m_Dims.x, 1));
        const uint32_t ix = rem - iy * static_cast<uint32_t>(m_Dims.x);
        return m_BoundsMin + glm::vec3(static_cast<float>(ix), static_cast<float>(iy), static_cast<float>(iz)) * m_Spacing;
    }

    void GIBaker::EnsureCommandResources()
    {
        if (m_CmdAllocator && m_CmdBuffer && m_Fence)
        {
            return;
        }

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !dm->GetGraphicsQueue())
        {
            return;
        }

        const auto &nri = dm->GetCoreInterface();
        nri.CreateCommandAllocator(*dm->GetGraphicsQueue(), m_CmdAllocator);
        nri.CreateCommandBuffer(*m_CmdAllocator, m_CmdBuffer);
        nri::FenceDesc fenceDesc = {};
        nri.CreateFence(*dm->GetDevice(), fenceDesc, m_Fence);
    }

    void GIBaker::EnsureBatchBuffers()
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
        {
            return;
        }

        const auto &nri = dm->GetCoreInterface();
        nri::Device *device = dm->GetDevice();

        if (!m_BatchSHGpuBuffer || !m_BatchSHReadbackBuffer || !m_BatchSHGpuUAV)
        {
            const uint64_t byteSize = static_cast<uint64_t>(kMaxBatchProbes) * sizeof(ProbeSHOutputGPU);
            nri::BufferDesc gpuDesc = {};
            gpuDesc.size = byteSize;
            gpuDesc.structureStride = sizeof(ProbeSHOutputGPU);
            gpuDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;
            nri.CreateCommittedBuffer(*device, nri::MemoryLocation::DEVICE, 1.0f, gpuDesc, m_BatchSHGpuBuffer);

            nri::BufferViewDesc viewDesc = {};
            viewDesc.buffer = m_BatchSHGpuBuffer;
            viewDesc.type = nri::BufferView::STORAGE_STRUCTURED_BUFFER;
            viewDesc.offset = 0;
            viewDesc.size = byteSize;
            viewDesc.structureStride = sizeof(ProbeSHOutputGPU);
            nri.CreateBufferView(viewDesc, m_BatchSHGpuUAV);

            nri::BufferDesc rbDesc = {};
            rbDesc.size = byteSize;
            rbDesc.usage = nri::BufferUsageBits::NONE;
            nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_READBACK, 1.0f, rbDesc, m_BatchSHReadbackBuffer);
        }

        if (!m_BatchStatsGpuBuffer || !m_BatchStatsReadbackBuffer || !m_BatchStatsGpuUAV)
        {
            const uint64_t byteSize = static_cast<uint64_t>(kMaxBatchProbes) * sizeof(gi::ProbeVisibilityStatsGPU);
            nri::BufferDesc gpuDesc = {};
            gpuDesc.size = byteSize;
            gpuDesc.structureStride = sizeof(gi::ProbeVisibilityStatsGPU);
            gpuDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;
            nri.CreateCommittedBuffer(*device, nri::MemoryLocation::DEVICE, 1.0f, gpuDesc, m_BatchStatsGpuBuffer);

            nri::BufferViewDesc viewDesc = {};
            viewDesc.buffer = m_BatchStatsGpuBuffer;
            viewDesc.type = nri::BufferView::STORAGE_STRUCTURED_BUFFER;
            viewDesc.offset = 0;
            viewDesc.size = byteSize;
            viewDesc.structureStride = sizeof(gi::ProbeVisibilityStatsGPU);
            nri.CreateBufferView(viewDesc, m_BatchStatsGpuUAV);

            nri::BufferDesc rbDesc = {};
            rbDesc.size = byteSize;
            rbDesc.usage = nri::BufferUsageBits::NONE;
            nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_READBACK, 1.0f, rbDesc, m_BatchStatsReadbackBuffer);
        }

        if (!m_BatchOctGpuBuffer || !m_BatchOctReadbackBuffer || !m_BatchOctGpuUAV)
        {
            const uint64_t byteSize = static_cast<uint64_t>(kMaxBatchProbes) * 100ull * sizeof(glm::vec2);
            nri::BufferDesc gpuDesc = {};
            gpuDesc.size = byteSize;
            gpuDesc.structureStride = sizeof(glm::vec2);
            gpuDesc.usage = nri::BufferUsageBits::SHADER_RESOURCE_STORAGE;
            nri.CreateCommittedBuffer(*device, nri::MemoryLocation::DEVICE, 1.0f, gpuDesc, m_BatchOctGpuBuffer);

            nri::BufferViewDesc viewDesc = {};
            viewDesc.buffer = m_BatchOctGpuBuffer;
            viewDesc.type = nri::BufferView::STORAGE_STRUCTURED_BUFFER;
            viewDesc.offset = 0;
            viewDesc.size = byteSize;
            viewDesc.structureStride = sizeof(glm::vec2);
            nri.CreateBufferView(viewDesc, m_BatchOctGpuUAV);

            nri::BufferDesc rbDesc = {};
            rbDesc.size = byteSize;
            rbDesc.usage = nri::BufferUsageBits::NONE;
            nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_READBACK, 1.0f, rbDesc, m_BatchOctReadbackBuffer);
        }
    }

    void GIBaker::SubmitAndWaitCommandBuffer()
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetGraphicsQueue() || !m_CmdBuffer || !m_Fence)
        {
            return;
        }

        const auto &nri = dm->GetCoreInterface();
        nri.EndCommandBuffer(*m_CmdBuffer);

        std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
        nri::FenceSubmitDesc signalDesc = { m_Fence, m_FenceValue };
        nri::QueueSubmitDesc submitDesc = {};
        submitDesc.commandBuffers = &m_CmdBuffer;
        submitDesc.commandBufferNum = 1;
        submitDesc.signalFences = &signalDesc;
        submitDesc.signalFenceNum = 1;
        nri.QueueSubmit(*dm->GetGraphicsQueue(), submitDesc);
        nri.Wait(*m_Fence, m_FenceValue);
        ++m_FenceValue;
    }

    void GIBaker::DilateInvalidProbes()
    {
        if (m_ProbeCount == 0 || m_Dims.x <= 0 || m_Dims.y <= 0 || m_Dims.z <= 0)
        {
            return;
        }

        std::vector<uint8_t> hasSource(m_ProbeCount, 0);
        bool anyValid = false;
        bool anyInvalid = false;
        for (uint32_t i = 0; i < m_ProbeCount; ++i)
        {
            if (m_ProbeValidity[i] > 0.5f)
            {
                hasSource[i] = 1;
                anyValid = true;
            }
            else
            {
                anyInvalid = true;
            }
        }

        if (!anyValid || !anyInvalid)
        {
            return;
        }

        const int32_t dx6[6] = { -1, 1, 0, 0, 0, 0 };
        const int32_t dy6[6] = { 0, 0, -1, 1, 0, 0 };
        const int32_t dz6[6] = { 0, 0, 0, 0, -1, 1 };

        std::vector<gi::SHL2> nextSH = m_ProbeSH;
        std::vector<uint8_t> nextHasSource = hasSource;

        const int maxIters = m_Dims.x + m_Dims.y + m_Dims.z;
        for (int iter = 0; iter < maxIters; ++iter)
        {
            bool changed = false;

            for (int32_t iz = 0; iz < m_Dims.z; ++iz)
            {
                for (int32_t iy = 0; iy < m_Dims.y; ++iy)
                {
                    for (int32_t ix = 0; ix < m_Dims.x; ++ix)
                    {
                        const uint32_t idx = (static_cast<uint32_t>(iz) * m_Dims.y + iy) * m_Dims.x + ix;
                        if (hasSource[idx])
                        {
                            continue;
                        }

                        gi::SHL2 accum{};
                        int count = 0;

                        // Prefer 6-connected axial neighbors first
                        for (int k = 0; k < 6; ++k)
                        {
                            const int32_t nx = ix + dx6[k];
                            const int32_t ny = iy + dy6[k];
                            const int32_t nz = iz + dz6[k];
                            if (nx < 0 || nx >= m_Dims.x || ny < 0 || ny >= m_Dims.y || nz < 0 || nz >= m_Dims.z)
                            {
                                continue;
                            }
                            const uint32_t nIdx = (static_cast<uint32_t>(nz) * m_Dims.y + ny) * m_Dims.x + nx;
                            if (hasSource[nIdx])
                            {
                                for (int c = 0; c < 9; ++c)
                                {
                                    accum.c[c] += m_ProbeSH[nIdx].c[c];
                                }
                                ++count;
                            }
                        }

                        // Fall back to 26-connected diagonal neighbors if no axial neighbor has valid SH yet
                        if (count == 0)
                        {
                            for (int32_t dz = -1; dz <= 1; ++dz)
                            {
                                for (int32_t dy = -1; dy <= 1; ++dy)
                                {
                                    for (int32_t dx = -1; dx <= 1; ++dx)
                                    {
                                        if (dx == 0 && dy == 0 && dz == 0)
                                        {
                                            continue;
                                        }
                                        const int32_t nx = ix + dx;
                                        const int32_t ny = iy + dy;
                                        const int32_t nz = iz + dz;
                                        if (nx < 0 || nx >= m_Dims.x || ny < 0 || ny >= m_Dims.y || nz < 0 || nz >= m_Dims.z)
                                        {
                                            continue;
                                        }
                                        const uint32_t nIdx = (static_cast<uint32_t>(nz) * m_Dims.y + ny) * m_Dims.x + nx;
                                        if (hasSource[nIdx])
                                        {
                                            for (int c = 0; c < 9; ++c)
                                            {
                                                accum.c[c] += m_ProbeSH[nIdx].c[c];
                                            }
                                            ++count;
                                        }
                                    }
                                }
                            }
                        }

                        if (count > 0)
                        {
                            const float inv = 1.0f / static_cast<float>(count);
                            for (int c = 0; c < 9; ++c)
                            {
                                nextSH[idx].c[c] = accum.c[c] * inv;
                            }
                            nextHasSource[idx] = 1;
                            changed = true;
                        }
                    }
                }
            }

            if (!changed)
            {
                break;
            }
            m_ProbeSH = nextSH;
            hasSource = nextHasSource;
        }
    }

    void GIBaker::PackWorkingData(IrradianceVolumeData &outData, bool includeVisibilityAndOffsets) const
    {
        outData = IrradianceVolumeData{};
        outData.dims = m_Dims;
        outData.boundsMin = m_BoundsMin;
        outData.spacing = m_Spacing;
        outData.shOrder = m_SHOrder;
        outData.flags = 0;
        outData.bakeHash = m_BakeHash;

        const size_t totalHalfCount = static_cast<size_t>(m_ProbeCount) * 4u;
        for (int t = 0; t < 7; ++t)
        {
            outData.shHalfRGBA[t].resize(totalHalfCount);
        }

        for (uint32_t probeIdx = 0; probeIdx < m_ProbeCount; ++probeIdx)
        {
            gi::SHL2 sh = m_ProbeSH[probeIdx];
            if (m_SHOrder == 1u)
            {
                for (int c = 4; c < 9; ++c)
                {
                    sh.c[c] = glm::vec3(0.0f);
                }
            }
            glm::vec4 packed[7]{};
            gi::PackSHL2To7RGBA(sh, m_ProbeValidity[probeIdx], packed);

            const size_t base = static_cast<size_t>(probeIdx) * 4u;
            for (int t = 0; t < 7; ++t)
            {
                outData.shHalfRGBA[t][base + 0u] = glm::packHalf1x16(packed[t].x);
                outData.shHalfRGBA[t][base + 1u] = glm::packHalf1x16(packed[t].y);
                outData.shHalfRGBA[t][base + 2u] = glm::packHalf1x16(packed[t].z);
                outData.shHalfRGBA[t][base + 3u] = glm::packHalf1x16(packed[t].w);
            }
        }

        if (includeVisibilityAndOffsets && m_ProbeCount > 0)
        {
            outData.flags |= IrradianceVolumeData::kFlagHasProbeOffsets;
            outData.probeOffsetsHalfRGBA.resize(totalHalfCount);
            for (uint32_t probeIdx = 0; probeIdx < m_ProbeCount; ++probeIdx)
            {
                const size_t base = static_cast<size_t>(probeIdx) * 4u;
                const glm::vec3 off = m_ProbeOffsets[probeIdx];
                outData.probeOffsetsHalfRGBA[base + 0u] = glm::packHalf1x16(off.x);
                outData.probeOffsetsHalfRGBA[base + 1u] = glm::packHalf1x16(off.y);
                outData.probeOffsetsHalfRGBA[base + 2u] = glm::packHalf1x16(off.z);
                outData.probeOffsetsHalfRGBA[base + 3u] = glm::packHalf1x16(m_ProbeState[probeIdx]);
            }

            if (m_ProbeVisTiles.size() == m_ProbeCount)
            {
                outData.visOctRes = 8;
                outData.visAtlasCols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(m_ProbeCount))));
                outData.visAtlasRows = (m_ProbeCount + outData.visAtlasCols - 1u) / std::max(outData.visAtlasCols, 1u);

                const uint32_t tileSize = outData.visOctRes + 2u;
                const uint32_t atlasW = outData.GetVisibilityAtlasWidth();
                const uint32_t atlasH = outData.GetVisibilityAtlasHeight();
                outData.visibilityAtlasHalfRG.assign(static_cast<size_t>(atlasW) * atlasH * 2u, 0);

                for (uint32_t probeIdx = 0; probeIdx < m_ProbeCount; ++probeIdx)
                {
                    const uint32_t col = probeIdx % outData.visAtlasCols;
                    const uint32_t row = probeIdx / outData.visAtlasCols;
                    const auto &tile = m_ProbeVisTiles[probeIdx];
                    for (uint32_t ty = 0; ty < tileSize; ++ty)
                    {
                        for (uint32_t tx = 0; tx < tileSize; ++tx)
                        {
                            const uint32_t ax = col * tileSize + tx;
                            const uint32_t ay = row * tileSize + ty;
                            const size_t dstBase = (static_cast<size_t>(ay) * atlasW + ax) * 2u;
                            const glm::vec2 m = tile[static_cast<size_t>(ty) * tileSize + tx];
                            outData.visibilityAtlasHalfRG[dstBase + 0u] = glm::packHalf1x16(m.x);
                            outData.visibilityAtlasHalfRG[dstBase + 1u] = glm::packHalf1x16(m.y);
                        }
                    }
                }
                outData.flags |= IrradianceVolumeData::kFlagHasVisibilityAtlas;
            }

            if (m_ProbeSkyVis.size() == m_ProbeCount)
            {
                outData.skyVisibilityHalfRGBA.resize(totalHalfCount);
                for (uint32_t probeIdx = 0; probeIdx < m_ProbeCount; ++probeIdx)
                {
                    const size_t base = static_cast<size_t>(probeIdx) * 4u;
                    const float vis = std::clamp(m_ProbeSkyVis[probeIdx], 0.0f, 1.0f);
                    const uint16_t h = glm::packHalf1x16(vis);
                    outData.skyVisibilityHalfRGBA[base + 0u] = h;
                    outData.skyVisibilityHalfRGBA[base + 1u] = h;
                    outData.skyVisibilityHalfRGBA[base + 2u] = h;
                    outData.skyVisibilityHalfRGBA[base + 3u] = glm::packHalf1x16(1.0f);
                }
                outData.flags |= IrradianceVolumeData::kFlagHasSkyVisibility;
            }
        }
    }

    bool GIBaker::PrepareBounce(uint32_t bounceIndex)
    {
        if (!m_Scene || !m_CmdAllocator || !m_CmdBuffer)
        {
            return false;
        }

        if (bounceIndex == 0)
        {
            const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
            nri.ResetCommandAllocator(*m_CmdAllocator);
            nri.BeginCommandBuffer(*m_CmdBuffer, nullptr);
            m_Capturer.PrepareScene(m_Scene, m_AssetManager, m_CmdBuffer, nullptr);
            SubmitAndWaitCommandBuffer();
            return true;
        }

        IrradianceVolumeData prevData;
        PackWorkingData(prevData, true);
        if (!m_PrevBounceVolume)
        {
            m_PrevBounceVolume = CreateRef<IrradianceVolume>();
        }
        if (!m_PrevBounceVolume->UploadFromData(prevData))
        {
            return false;
        }
        m_PrevBounceGIData = GI_GPUData{};
        m_PrevBounceVolume->FillGIGPUData(m_PrevBounceGIData, nullptr);
        m_PrevBounceGIData.giMode = 2; // Use LeakResistant mode between bounce captures
        m_PrevBounceGIData.gridMinAndIntensity.w = 1.0f;
        m_PrevBounceGIData.biasAndFade.z = std::max(0.1f, m_Spacing * 0.2f);
        m_PrevBounceGIData.viewportInvSizeAndFlags.w = 1.0f;
        m_Capturer.SetOverrideGIData(&m_PrevBounceGIData);
        return true;
    }

    bool GIBaker::StartBake(Scene *scene, AssetManager *assetManager, Entity volumeEntity, const std::filesystem::path &savePath)
    {
        IGN_PROFILE_FUNCTION();

        CancelBake();
        m_FinishedSuccessfully = false;
        m_BakedData = IrradianceVolumeData{};

        if (!scene || !volumeEntity.IsValid() || !volumeEntity.HasComponent<TransformComponent>() || !volumeEntity.HasComponent<IrradianceVolumeComponent>())
        {
            return false;
        }

        EnsureCommandResources();
        EnsureBatchBuffers();
        if (!m_CmdAllocator || !m_CmdBuffer || !m_Fence
            || !m_BatchSHGpuBuffer || !m_BatchSHReadbackBuffer || !m_BatchSHGpuUAV
            || !m_BatchStatsGpuBuffer || !m_BatchStatsReadbackBuffer || !m_BatchStatsGpuUAV
            || !m_BatchOctGpuBuffer || !m_BatchOctReadbackBuffer || !m_BatchOctGpuUAV)
        {
            return false;
        }

        const auto &tr = volumeEntity.GetComponent<TransformComponent>();
        const auto &vol = volumeEntity.GetComponent<IrradianceVolumeComponent>();

        ComputeVolumeGrid(tr, vol, m_BoundsMin, m_Dims, m_Spacing);
        m_Resolution = static_cast<uint32_t>(std::clamp(vol.captureResolution, 16, 128));
        m_SHOrder = (vol.shOrder == 1) ? 1u : 2u;
        m_SkyVisibilitySplit = vol.skyVisibilitySplit;
        m_ProbeCount = static_cast<uint32_t>(m_Dims.x) * static_cast<uint32_t>(m_Dims.y) * static_cast<uint32_t>(m_Dims.z);
        m_TotalBounces = static_cast<uint32_t>(std::clamp(vol.bounces, 1, 4));
        m_CurrentBounce = 0;
        m_NextProbeIndex = 0;
        m_RelocationCursor = 0;
        m_CompletedProbes = 0;
        m_TotalProbes = m_ProbeCount * m_TotalBounces;

        m_Scene = scene;
        m_AssetManager = assetManager;
        m_SavePath = savePath;
        m_BakeHash = ComputeSceneBakeHash(scene);
        m_BakeVolumeEntity = volumeEntity;

        m_ProbeSH.assign(m_ProbeCount, gi::SHL2{});
        m_ProbeValidity.assign(m_ProbeCount, 1.0f);
        m_ProbeSkyVis.assign(m_ProbeCount, 0.0f);
        m_ProbeOffsets.assign(m_ProbeCount, glm::vec3(0.0f));
        m_ProbeState.assign(m_ProbeCount, 1.0f);
        m_ProbeVisTiles.assign(m_ProbeCount, {});
        m_RelocationQueue.clear();
        m_CandidateFromInvalid.clear();

        if (!PrepareBounce(0))
        {
            CancelBake();
            return false;
        }

        m_Phase = BakePhase::BounceCapture;
        m_IsBaking = true;
        return true;
    }

    bool GIBaker::AdvanceAfterBounce()
    {
        DilateInvalidProbes();

        if (m_CurrentBounce + 1u < m_TotalBounces)
        {
            ++m_CurrentBounce;
            m_NextProbeIndex = 0;
            if (!PrepareBounce(m_CurrentBounce))
            {
                CancelBake();
                return false;
            }
            m_Phase = BakePhase::BounceCapture;
            return true;
        }

        FinalizeBakedData();
        m_PrevBounceVolume.reset();
        m_IsBaking = false;
        m_Phase = BakePhase::Idle;
        m_FinishedSuccessfully = true;
        if (!m_SavePath.empty())
        {
            m_FinishedSuccessfully = gi_io::SaveIrradianceVolume(m_SavePath, m_BakedData);
        }
        if (m_FinishedSuccessfully && m_BakeVolumeEntity.IsValid())
        {
            auto &volume = m_BakeVolumeEntity.GetComponent<IrradianceVolumeComponent>();
            volume.bakeHash = m_BakeHash;
            if (!m_SavePath.empty())
            {
                if (m_Scene && m_Scene->GetProject())
                {
                    std::error_code relEc;
                    const std::filesystem::path relPath = std::filesystem::relative(
                        m_SavePath, m_Scene->GetProject()->GetDirectory(), relEc);
                    const bool insideProj = !relEc && !relPath.empty() && !stringutils::StartsWith(relPath.generic_string(), "..");
                    volume.bakedAssetPath = insideProj ? relPath.generic_string() : m_SavePath.generic_string();
                }
                else
                {
                    volume.bakedAssetPath = m_SavePath.generic_string();
                }
            }
        }
        return false;
    }

    bool GIBaker::StepBake(uint32_t maxProbesThisStep)
    {
        IGN_PROFILE_FUNCTION();

        if (!m_IsBaking)
        {
            return false;
        }

        const uint32_t budget = std::max(1u, maxProbesThisStep);
        uint32_t processed = 0;
        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        while (processed < budget && m_IsBaking)
        {
            if (m_Phase == BakePhase::BounceCapture && m_NextProbeIndex < m_ProbeCount)
            {
                const bool captureVisibility = (m_CurrentBounce == 0u);
                const uint32_t batchCount = std::min({ budget - processed, kMaxBatchProbes, m_ProbeCount - m_NextProbeIndex });

                nri.ResetCommandAllocator(*m_CmdAllocator);
                nri.BeginCommandBuffer(*m_CmdBuffer, nullptr);

                {
                    std::vector<nri::BufferBarrierDesc> preBarriers;
                    if (m_BatchSHWritten)
                    {
                        nri::BufferBarrierDesc b = {};
                        b.buffer = m_BatchSHGpuBuffer;
                        b.before = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                        b.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                        preBarriers.push_back(b);
                    }
                    if (captureVisibility && m_BatchVisWritten)
                    {
                        nri::BufferBarrierDesc sb = {};
                        sb.buffer = m_BatchStatsGpuBuffer;
                        sb.before = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                        sb.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                        preBarriers.push_back(sb);

                        nri::BufferBarrierDesc ob = {};
                        ob.buffer = m_BatchOctGpuBuffer;
                        ob.before = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                        ob.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                        preBarriers.push_back(ob);
                    }
                    if (!preBarriers.empty())
                    {
                        nri::BarrierDesc bDesc = {};
                        bDesc.buffers = preBarriers.data();
                        bDesc.bufferNum = static_cast<uint32_t>(preBarriers.size());
                        nri.CmdBarrier(*m_CmdBuffer, bDesc);
                    }
                }

                m_Capturer.ResetCaptureSlotCursor();

                CaptureSettings capSettings;
                capSettings.resolution = m_Resolution;
                capSettings.nearPlane = 0.05f;
                capSettings.farPlane = 500.0f;
                capSettings.mode = CaptureMode::Irradiance;
                capSettings.includeSky = !m_SkyVisibilitySplit;
                capSettings.enablePreviousGI = (m_CurrentBounce > 0u);
                if (capSettings.enablePreviousGI)
                {
                    capSettings.previousGIData = m_PrevBounceGIData;
                }

                for (uint32_t localIdx = 0; localIdx < batchCount; ++localIdx)
                {
                    const uint32_t probeIdx = m_NextProbeIndex + localIdx;
                    const glm::vec3 probePos = GetProbeBasePosition(probeIdx) + m_ProbeOffsets[probeIdx];

                    CaptureResult cap = m_Capturer.CaptureCubemap(m_CmdBuffer, probePos, capSettings);
                    if (cap.colorCube)
                    {
                        m_Prefilter.ProjectCubeToSH(m_CmdBuffer, cap.colorCube, m_BatchSHGpuUAV, localIdx, true, 6.0f);
                        if (captureVisibility && cap.depthCube)
                        {
                            m_Prefilter.ComputeProbeVisibility(m_CmdBuffer, cap.colorCube, cap.depthCube,
                                m_BatchStatsGpuUAV, m_BatchOctGpuUAV, localIdx,
                                capSettings.nearPlane, capSettings.farPlane, m_Spacing * 2.5f, 50.0f);
                        }
                    }
                }

                {
                    std::vector<nri::BufferBarrierDesc> postBarriers;
                    nri::BufferBarrierDesc shB = {};
                    shB.buffer = m_BatchSHGpuBuffer;
                    shB.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                    shB.after = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                    postBarriers.push_back(shB);

                    if (captureVisibility)
                    {
                        nri::BufferBarrierDesc statsB = {};
                        statsB.buffer = m_BatchStatsGpuBuffer;
                        statsB.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                        statsB.after = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                        postBarriers.push_back(statsB);

                        nri::BufferBarrierDesc octB = {};
                        octB.buffer = m_BatchOctGpuBuffer;
                        octB.before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                        octB.after = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                        postBarriers.push_back(octB);
                    }

                    nri::BarrierDesc bDesc = {};
                    bDesc.buffers = postBarriers.data();
                    bDesc.bufferNum = static_cast<uint32_t>(postBarriers.size());
                    nri.CmdBarrier(*m_CmdBuffer, bDesc);
                }

                const uint64_t shCopySize = static_cast<uint64_t>(batchCount) * sizeof(ProbeSHOutputGPU);
                nri.CmdCopyBuffer(*m_CmdBuffer, *m_BatchSHReadbackBuffer, 0, *m_BatchSHGpuBuffer, 0, shCopySize);
                m_BatchSHWritten = true;

                const uint64_t statsCopySize = static_cast<uint64_t>(batchCount) * sizeof(gi::ProbeVisibilityStatsGPU);
                const uint64_t octCopySize = static_cast<uint64_t>(batchCount) * 100ull * sizeof(glm::vec2);
                if (captureVisibility)
                {
                    nri.CmdCopyBuffer(*m_CmdBuffer, *m_BatchStatsReadbackBuffer, 0, *m_BatchStatsGpuBuffer, 0, statsCopySize);
                    nri.CmdCopyBuffer(*m_CmdBuffer, *m_BatchOctReadbackBuffer, 0, *m_BatchOctGpuBuffer, 0, octCopySize);
                    m_BatchVisWritten = true;
                }

                SubmitAndWaitCommandBuffer();

                if (const void *mappedSH = nri.MapBuffer(*m_BatchSHReadbackBuffer, 0, shCopySize))
                {
                    const auto *results = static_cast<const ProbeSHOutputGPU *>(mappedSH);
                    for (uint32_t localIdx = 0; localIdx < batchCount; ++localIdx)
                    {
                        const uint32_t probeIdx = m_NextProbeIndex + localIdx;
                        for (int c = 0; c < 9; ++c)
                        {
                            m_ProbeSH[probeIdx].c[c] = glm::vec3(results[localIdx].coeff[c]);
                        }
                        if (m_CurrentBounce == 0u)
                        {
                            const bool valid = results[localIdx].backfaceFraction <= 0.25f;
                            m_ProbeValidity[probeIdx] = valid ? 1.0f : 0.0f;
                            m_ProbeState[probeIdx] = valid ? 1.0f : 0.0f;
                        }
                    }
                    nri.UnmapBuffer(*m_BatchSHReadbackBuffer);
                }

                if (captureVisibility)
                {
                    if (const void *mappedOct = nri.MapBuffer(*m_BatchOctReadbackBuffer, 0, octCopySize))
                    {
                        const auto *tiles = static_cast<const glm::vec2 *>(mappedOct);
                        for (uint32_t localIdx = 0; localIdx < batchCount; ++localIdx)
                        {
                            const uint32_t probeIdx = m_NextProbeIndex + localIdx;
                            std::memcpy(m_ProbeVisTiles[probeIdx].data(), tiles + static_cast<size_t>(localIdx) * 100u, 100u * sizeof(glm::vec2));
                        }
                        nri.UnmapBuffer(*m_BatchOctReadbackBuffer);
                    }

                    if (const void *mappedStats = nri.MapBuffer(*m_BatchStatsReadbackBuffer, 0, statsCopySize))
                    {
                        const auto *statsArr = static_cast<const gi::ProbeVisibilityStatsGPU *>(mappedStats);
                        const float maxOffset = 0.45f * m_Spacing;
                        for (uint32_t localIdx = 0; localIdx < batchCount; ++localIdx)
                        {
                            const uint32_t probeIdx = m_NextProbeIndex + localIdx;
                            const auto &st = statsArr[localIdx];
                            const bool valid = (st.backfaceFraction <= 0.25f);
                            m_ProbeValidity[probeIdx] = valid ? 1.0f : 0.0f;
                            m_ProbeState[probeIdx] = valid ? 1.0f : 0.0f;
                            if (probeIdx < m_ProbeSkyVis.size())
                            {
                                m_ProbeSkyVis[probeIdx] = std::clamp(st.skyVisibility, 0.0f, 1.0f);
                            }

                            if (!valid && st.closestBackfaceDirAndDist.w >= 0.0f)
                            {
                                // Probe is inside solid geometry: step outward toward the nearest exiting backface boundary
                                glm::vec3 dir = glm::vec3(st.closestBackfaceDirAndDist);
                                const float len = glm::length(dir);
                                if (len > 1e-4f)
                                {
                                    dir /= len;
                                    const float push = std::min(st.closestBackfaceDirAndDist.w + 0.15f * m_Spacing, maxOffset);
                                    if (push > 1e-4f)
                                    {
                                        m_ProbeOffsets[probeIdx] = dir * push;
                                        m_RelocationQueue.push_back(probeIdx);
                                        m_CandidateFromInvalid.push_back(1);
                                    }
                                }
                            }
                            else if (valid && st.closestFrontfaceDirAndDist.w > 0.0f && st.closestFrontfaceDirAndDist.w < 0.12f * m_Spacing)
                            {
                                // Probe barely grazes a frontface: nudge slightly away from the surface
                                glm::vec3 dir = glm::vec3(st.closestFrontfaceDirAndDist);
                                const float len = glm::length(dir);
                                if (len > 1e-4f)
                                {
                                    dir /= len;
                                    const float push = std::min(0.15f * m_Spacing - st.closestFrontfaceDirAndDist.w, maxOffset);
                                    if (push > 1e-4f)
                                    {
                                        m_ProbeOffsets[probeIdx] = -dir * push;
                                        m_RelocationQueue.push_back(probeIdx);
                                        m_CandidateFromInvalid.push_back(0);
                                    }
                                }
                            }
                        }
                        nri.UnmapBuffer(*m_BatchStatsReadbackBuffer);
                    }
                }

                m_NextProbeIndex += batchCount;
                m_CompletedProbes += batchCount;
                processed += batchCount;

                if (m_NextProbeIndex >= m_ProbeCount)
                {
                    if (m_CurrentBounce == 0u && !m_RelocationQueue.empty())
                    {
                        m_Phase = BakePhase::RelocationCapture;
                        m_RelocationCursor = 0;
                    }
                    else
                    {
                        return AdvanceAfterBounce();
                    }
                }
            }
            else if (m_Phase == BakePhase::RelocationCapture && m_RelocationCursor < m_RelocationQueue.size())
            {
                const uint32_t remaining = static_cast<uint32_t>(m_RelocationQueue.size()) - m_RelocationCursor;
                const uint32_t batchCount = std::min({ budget - processed, kMaxBatchProbes, remaining });

                nri.ResetCommandAllocator(*m_CmdAllocator);
                nri.BeginCommandBuffer(*m_CmdBuffer, nullptr);

                {
                    std::vector<nri::BufferBarrierDesc> preBarriers;
                    if (m_BatchSHWritten)
                    {
                        nri::BufferBarrierDesc b = {};
                        b.buffer = m_BatchSHGpuBuffer;
                        b.before = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                        b.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                        preBarriers.push_back(b);
                    }
                    if (m_BatchVisWritten)
                    {
                        nri::BufferBarrierDesc sb = {};
                        sb.buffer = m_BatchStatsGpuBuffer;
                        sb.before = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                        sb.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                        preBarriers.push_back(sb);

                        nri::BufferBarrierDesc ob = {};
                        ob.buffer = m_BatchOctGpuBuffer;
                        ob.before = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                        ob.after = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                        preBarriers.push_back(ob);
                    }
                    if (!preBarriers.empty())
                    {
                        nri::BarrierDesc bDesc = {};
                        bDesc.buffers = preBarriers.data();
                        bDesc.bufferNum = static_cast<uint32_t>(preBarriers.size());
                        nri.CmdBarrier(*m_CmdBuffer, bDesc);
                    }
                }

                m_Capturer.ResetCaptureSlotCursor();

                CaptureSettings capSettings;
                capSettings.resolution = m_Resolution;
                capSettings.nearPlane = 0.05f;
                capSettings.farPlane = 500.0f;
                capSettings.mode = CaptureMode::Irradiance;
                capSettings.includeSky = !m_SkyVisibilitySplit;
                capSettings.enablePreviousGI = false;

                for (uint32_t localIdx = 0; localIdx < batchCount; ++localIdx)
                {
                    const uint32_t probeIdx = m_RelocationQueue[m_RelocationCursor + localIdx];
                    const glm::vec3 probePos = GetProbeBasePosition(probeIdx) + m_ProbeOffsets[probeIdx];

                    CaptureResult cap = m_Capturer.CaptureCubemap(m_CmdBuffer, probePos, capSettings);
                    if (cap.colorCube)
                    {
                        m_Prefilter.ProjectCubeToSH(m_CmdBuffer, cap.colorCube, m_BatchSHGpuUAV, localIdx, true, 6.0f);
                        if (cap.depthCube)
                        {
                            m_Prefilter.ComputeProbeVisibility(m_CmdBuffer, cap.colorCube, cap.depthCube,
                                m_BatchStatsGpuUAV, m_BatchOctGpuUAV, localIdx,
                                capSettings.nearPlane, capSettings.farPlane, m_Spacing * 2.5f, 50.0f);
                        }
                    }
                }

                {
                    nri::BufferBarrierDesc postBarriers[3] = {};
                    postBarriers[0].buffer = m_BatchSHGpuBuffer;
                    postBarriers[0].before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                    postBarriers[0].after = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                    postBarriers[1].buffer = m_BatchStatsGpuBuffer;
                    postBarriers[1].before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                    postBarriers[1].after = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };
                    postBarriers[2].buffer = m_BatchOctGpuBuffer;
                    postBarriers[2].before = { nri::AccessBits::SHADER_RESOURCE_STORAGE, nri::StageBits::COMPUTE_SHADER };
                    postBarriers[2].after = { nri::AccessBits::COPY_SOURCE, nri::StageBits::COPY };

                    nri::BarrierDesc bDesc = {};
                    bDesc.buffers = postBarriers;
                    bDesc.bufferNum = 3;
                    nri.CmdBarrier(*m_CmdBuffer, bDesc);
                }

                const uint64_t shCopySize = static_cast<uint64_t>(batchCount) * sizeof(ProbeSHOutputGPU);
                const uint64_t statsCopySize = static_cast<uint64_t>(batchCount) * sizeof(gi::ProbeVisibilityStatsGPU);
                const uint64_t octCopySize = static_cast<uint64_t>(batchCount) * 100ull * sizeof(glm::vec2);

                nri.CmdCopyBuffer(*m_CmdBuffer, *m_BatchSHReadbackBuffer, 0, *m_BatchSHGpuBuffer, 0, shCopySize);
                nri.CmdCopyBuffer(*m_CmdBuffer, *m_BatchStatsReadbackBuffer, 0, *m_BatchStatsGpuBuffer, 0, statsCopySize);
                nri.CmdCopyBuffer(*m_CmdBuffer, *m_BatchOctReadbackBuffer, 0, *m_BatchOctGpuBuffer, 0, octCopySize);
                m_BatchSHWritten = true;
                m_BatchVisWritten = true;

                SubmitAndWaitCommandBuffer();

                std::vector<uint8_t> accepted(batchCount, 0);
                if (const void *mappedStats = nri.MapBuffer(*m_BatchStatsReadbackBuffer, 0, statsCopySize))
                {
                    const auto *statsArr = static_cast<const gi::ProbeVisibilityStatsGPU *>(mappedStats);
                    for (uint32_t localIdx = 0; localIdx < batchCount; ++localIdx)
                    {
                        const uint32_t qIdx = m_RelocationCursor + localIdx;
                        const uint32_t probeIdx = m_RelocationQueue[qIdx];
                        const bool relocValid = (statsArr[localIdx].backfaceFraction <= 0.25f);
                        if (relocValid)
                        {
                            accepted[localIdx] = 1;
                            m_ProbeValidity[probeIdx] = 1.0f;
                            m_ProbeState[probeIdx] = 2.0f; // Relocated
                            if (probeIdx < m_ProbeSkyVis.size())
                            {
                                m_ProbeSkyVis[probeIdx] = std::clamp(statsArr[localIdx].skyVisibility, 0.0f, 1.0f);
                            }
                        }
                        else
                        {
                            m_ProbeOffsets[probeIdx] = glm::vec3(0.0f);
                            const bool wasInvalid = (m_CandidateFromInvalid[qIdx] != 0);
                            m_ProbeValidity[probeIdx] = wasInvalid ? 0.0f : 1.0f;
                            m_ProbeState[probeIdx] = wasInvalid ? 0.0f : 1.0f;
                        }
                    }
                    nri.UnmapBuffer(*m_BatchStatsReadbackBuffer);
                }

                if (const void *mappedSH = nri.MapBuffer(*m_BatchSHReadbackBuffer, 0, shCopySize))
                {
                    const auto *results = static_cast<const ProbeSHOutputGPU *>(mappedSH);
                    for (uint32_t localIdx = 0; localIdx < batchCount; ++localIdx)
                    {
                        if (!accepted[localIdx])
                        {
                            continue;
                        }
                        const uint32_t probeIdx = m_RelocationQueue[m_RelocationCursor + localIdx];
                        for (int c = 0; c < 9; ++c)
                        {
                            m_ProbeSH[probeIdx].c[c] = glm::vec3(results[localIdx].coeff[c]);
                        }
                    }
                    nri.UnmapBuffer(*m_BatchSHReadbackBuffer);
                }

                if (const void *mappedOct = nri.MapBuffer(*m_BatchOctReadbackBuffer, 0, octCopySize))
                {
                    const auto *tiles = static_cast<const glm::vec2 *>(mappedOct);
                    for (uint32_t localIdx = 0; localIdx < batchCount; ++localIdx)
                    {
                        if (!accepted[localIdx])
                        {
                            continue;
                        }
                        const uint32_t probeIdx = m_RelocationQueue[m_RelocationCursor + localIdx];
                        std::memcpy(m_ProbeVisTiles[probeIdx].data(), tiles + static_cast<size_t>(localIdx) * 100u, 100u * sizeof(glm::vec2));
                    }
                    nri.UnmapBuffer(*m_BatchOctReadbackBuffer);
                }

                m_RelocationCursor += batchCount;
                processed += batchCount;

                if (m_RelocationCursor >= m_RelocationQueue.size())
                {
                    return AdvanceAfterBounce();
                }
            }
            else
            {
                return AdvanceAfterBounce();
            }
        }

        return m_IsBaking;
    }

    void GIBaker::FinalizeBakedData()
    {
        PackWorkingData(m_BakedData, true);
    }

    bool GIBaker::BakeVolumeSync(Scene *scene, AssetManager *assetManager, Entity volumeEntity,
        IrradianceVolumeData &outData, const std::filesystem::path &savePath)
    {
        if (!StartBake(scene, assetManager, volumeEntity, savePath))
        {
            return false;
        }

        while (StepBake(kMaxBatchProbes))
        {
        }

        if (m_FinishedSuccessfully)
        {
            outData = m_BakedData;
        }
        return m_FinishedSuccessfully;
    }

    void GIBaker::CancelBake()
    {
        m_IsBaking = false;
        m_Phase = BakePhase::Idle;
        m_Scene = nullptr;
        m_AssetManager = nullptr;
        m_BakeVolumeEntity = Entity();
        m_NextProbeIndex = 0;
        m_RelocationCursor = 0;
        m_CompletedProbes = 0;
        m_TotalProbes = 0;
        m_RelocationQueue.clear();
        m_CandidateFromInvalid.clear();
        m_PrevBounceVolume.reset();
    }

    float GIBaker::GetProgress() const
    {
        if (m_TotalProbes == 0)
        {
            return m_FinishedSuccessfully ? 1.0f : 0.0f;
        }
        return std::clamp(static_cast<float>(m_CompletedProbes) / static_cast<float>(m_TotalProbes), 0.0f, 1.0f);
    }

    bool GIBaker::BakeReflectionProbes(Scene *scene, AssetManager *assetManager, const IrradianceVolume *activeVolume,
        const std::filesystem::path &outputDir, std::vector<std::filesystem::path> *outSavedPaths)
    {
        IGN_PROFILE_FUNCTION();

        if (!scene || !scene->registry)
        {
            return false;
        }

        auto view = scene->registry->view<TransformComponent, ReflectionProbeComponent>();
        bool anyProbe = false;
        bool allSucceeded = true;

        std::filesystem::path projectDir;
        if (scene->GetProject())
        {
            projectDir = scene->GetProject()->GetDirectory();
        }

        std::filesystem::path baseOutDir = outputDir;
        if (baseOutDir.empty() && !projectDir.empty())
        {
            baseOutDir = projectDir / "Baked";
        }
        if (baseOutDir.empty())
        {
            baseOutDir = std::filesystem::current_path() / "Baked";
        }

        std::error_code ec;
        std::filesystem::create_directories(baseOutDir, ec);

        for (entt::entity e : view)
        {
            anyProbe = true;
            const auto &tr = view.get<TransformComponent>(e);
            auto &comp = view.get<ReflectionProbeComponent>(e);

            const uint32_t res = static_cast<uint32_t>(std::clamp(comp.resolution, 32, 256));
            const uint32_t mipCount = std::max(1u, std::min(6u, Texture::CalculateMaxMipLevels(res, res, 1)));

            const glm::vec3 worldScale = glm::abs(tr.world.scale);
            const glm::vec3 capturePos = tr.world.translation
                + glm::mat3_cast(tr.world.rotation) * (comp.captureOffset * worldScale);
            const glm::vec3 projExt = glm::max(glm::abs(comp.projectionExtents) * worldScale, glm::vec3(0.05f));

            CaptureSettings capSettings;
            capSettings.resolution = res;
            capSettings.nearPlane = 0.05f;
            capSettings.farPlane = 500.0f;
            capSettings.mode = CaptureMode::Reflection;
            capSettings.includeSky = true;
            capSettings.excludeOriginRadius = std::max(0.0f, comp.excludeRadius);
            capSettings.enablePreviousGI = (activeVolume && activeVolume->IsValid());
            if (capSettings.enablePreviousGI)
            {
                activeVolume->FillGIGPUData(capSettings.previousGIData, nullptr);
                capSettings.previousGIData.giMode = 2;
                capSettings.previousGIData.viewportInvSizeAndFlags.w = 1.0f;
            }

            CaptureResult cap = m_Capturer.CaptureCubemapSync(scene, assetManager, capturePos, capSettings);
            if (!cap.colorCube)
            {
                allSucceeded = false;
                continue;
            }

            gi::SHL2 captureSHL2{};
            if (!m_Prefilter.ProjectCubeToSHSync(cap.colorCube, captureSHL2, nullptr, true, 0.0f))
            {
                allSucceeded = false;
                continue;
            }

            TextureCreateInfo preInfo;
            preInfo.width = res;
            preInfo.height = res;
            preInfo.depth = 1;
            preInfo.mipLevels = mipCount;
            preInfo.arraySize = 6;
            preInfo.sampleCount = 1;
            preInfo.dimension = nri::TextureType::TEXTURE_2D;
            preInfo.format = nri::Format::RGBA16_SFLOAT;
            preInfo.isCubeMap = true;
            preInfo.isRenderTarget = false;
            preInfo.isUAV = true;
            preInfo.bindless = false;

            Ref<Texture> prefilteredCube = Texture::Create(preInfo, "[GIBaker ReflectionPrefilter]");
            if (!prefilteredCube || !m_Prefilter.PrefilterCubeFromMip0Sync(cap.colorCube, prefilteredCube, 64u))
            {
                allSucceeded = false;
                continue;
            }

            comp.bakeHash = ComputeSceneBakeHash(scene);
            ReflectionProbeData rpData;
            rpData.resolution = res;
            rpData.mipCount = mipCount;
            rpData.capturePos = capturePos;
            rpData.boxMin = -projExt;
            rpData.boxMax = projExt;
            rpData.bakeHash = comp.bakeHash;
            for (int k = 0; k < 4; ++k)
            {
                rpData.captureSH[k] = glm::vec4(captureSHL2.c[k], 0.0f);
            }

            rpData.mipFaceBytes.resize(static_cast<size_t>(6u) * mipCount);
            bool readbackOk = true;
            for (uint32_t face = 0; face < 6u && readbackOk; ++face)
            {
                for (uint32_t mip = 0; mip < mipCount; ++mip)
                {
                    uint32_t rowPitch = 0;
                    uint32_t slicePitch = 0;
                    std::vector<uint8_t> &dst = rpData.mipFaceBytes[static_cast<size_t>(face) * mipCount + mip];
                    if (!Texture::ReadbackTextureSubresource(prefilteredCube, mip, face, dst, rowPitch, slicePitch))
                    {
                        readbackOk = false;
                        break;
                    }
                }
            }

            if (!readbackOk)
            {
                allSucceeded = false;
                continue;
            }

            uint64_t uuidVal = static_cast<uint64_t>(entt::to_integral(e));
            std::string safeName = "reflection_probe";
            if (const auto *id = scene->registry->try_get<IDComponent>(e))
            {
                uuidVal = static_cast<uint64_t>(id->uuid);
                if (!id->name.empty())
                {
                    safeName.clear();
                    for (char ch : id->name)
                    {
                        safeName.push_back((std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') ? ch : '_');
                    }
                }
            }

            const std::filesystem::path outFile = baseOutDir / (safeName + "_" + std::to_string(uuidVal) + ".igcube");
            if (!gi_io::SaveReflectionProbe(outFile, rpData))
            {
                allSucceeded = false;
                continue;
            }

            if (!projectDir.empty())
            {
                std::filesystem::path relPath = std::filesystem::relative(outFile, projectDir, ec);
                const bool insideProj = !ec && !relPath.empty() && !stringutils::StartsWith(relPath.generic_string(), "..");
                comp.bakedAssetPath = insideProj ? relPath.generic_string() : outFile.generic_string();
            }
            else
            {
                comp.bakedAssetPath = outFile.generic_string();
            }

            if (outSavedPaths)
            {
                outSavedPaths->push_back(outFile);
            }
        }

        return anyProbe && allSucceeded;
    }
}
