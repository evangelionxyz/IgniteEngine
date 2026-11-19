// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "gpu_timer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
    GPUTimer::~GPUTimer()
    {
        Shutdown();
    }

    bool GPUTimer::Initialize(nri::Device *device, uint32_t maxZones)
    {
        Shutdown();

        auto *dm = DeviceManager::GetInstance();
        if (!device && dm)
            device = dm->GetDevice();
        if (!device || !dm || maxZones == 0)
            return false;

        const auto &nri = dm->GetCoreInterface();
        const auto &devDesc = nri.GetDeviceDesc(*device);
        if (!devDesc.features.timestamp || devDesc.other.timestampFrequencyHz == 0)
        {
            m_Supported = false;
            return false;
        }

        m_Device = device;
        m_MaxZones = maxZones;
        m_TimestampFrequencyHz = devDesc.other.timestampFrequencyHz;

        const uint32_t queryCount = maxZones * 2u;

        nri::QueryPoolDesc qpDesc = {};
        qpDesc.queryType = nri::QueryType::TIMESTAMP;
        qpDesc.capacity = queryCount;
        if (nri.CreateQueryPool(*m_Device, qpDesc, m_QueryPool) != nri::Result::SUCCESS || !m_QueryPool)
        {
            Shutdown();
            return false;
        }

        const uint64_t bufferSize = static_cast<uint64_t>(queryCount) * sizeof(uint64_t);
        nri::BufferDesc bufDesc = {};
        bufDesc.size = bufferSize;
        bufDesc.usage = nri::BufferUsageBits::NONE;
        if (nri.CreateBuffer(*m_Device, bufDesc, m_ReadbackBuffer) != nri::Result::SUCCESS || !m_ReadbackBuffer)
        {
            Shutdown();
            return false;
        }
        nri.SetDebugName((nri::Object *)m_ReadbackBuffer, "[GPUTimer Readback]");

        nri::MemoryDesc memDesc = {};
        nri.GetBufferMemoryDesc(*m_ReadbackBuffer, nri::MemoryLocation::HOST_READBACK, memDesc);
        nri::AllocateMemoryDesc allocDesc = {};
        allocDesc.size = memDesc.size;
        allocDesc.type = memDesc.type;
        if (nri.AllocateMemory(*m_Device, allocDesc, m_ReadbackMemory) != nri::Result::SUCCESS || !m_ReadbackMemory)
        {
            Shutdown();
            return false;
        }

        nri::BindBufferMemoryDesc bindDesc = {};
        bindDesc.buffer = m_ReadbackBuffer;
        bindDesc.memory = m_ReadbackMemory;
        bindDesc.offset = 0;
        if (nri.BindBufferMemory(&bindDesc, 1) != nri::Result::SUCCESS)
        {
            Shutdown();
            return false;
        }

        m_MappedReadback = static_cast<uint64_t *>(nri.MapBuffer(*m_ReadbackBuffer, 0, bufferSize));
        if (!m_MappedReadback)
        {
            Shutdown();
            return false;
        }

        m_Supported = true;
        return true;
    }

    void GPUTimer::Shutdown()
    {
        auto *dm = DeviceManager::GetInstance();
        if (dm && m_Device)
        {
            const auto &nri = dm->GetCoreInterface();
            if (m_QueryPool)
            {
                nri.DestroyQueryPool(m_QueryPool);
                m_QueryPool = nullptr;
            }
            if (m_ReadbackBuffer)
            {
                if (m_MappedReadback)
                {
                    nri.UnmapBuffer(*m_ReadbackBuffer);
                    m_MappedReadback = nullptr;
                }
                nri.DestroyBuffer(m_ReadbackBuffer);
                m_ReadbackBuffer = nullptr;
            }
            if (m_ReadbackMemory)
            {
                nri.FreeMemory(m_ReadbackMemory);
                m_ReadbackMemory = nullptr;
            }
        }

        m_Device = nullptr;
        m_MaxZones = 0;
        m_TimestampFrequencyHz = 0;
        m_Supported = false;
        m_HasPendingResults = false;
        m_Zones.clear();
        m_PreviousZoneNames.clear();
        m_LastResultsMs.clear();
    }

    void GPUTimer::BeginFrame(nri::CommandBuffer *cmd)
    {
        if (!m_Supported || !cmd)
            return;

        if (m_HasPendingResults && m_MappedReadback && m_TimestampFrequencyHz > 0)
        {
            m_LastResultsMs.clear();
            const double ticksToMs = 1000.0 / static_cast<double>(m_TimestampFrequencyHz);
            for (size_t i = 0; i < m_PreviousZoneNames.size(); ++i)
            {
                if (m_PreviousZoneNames[i].empty())
                    continue;

                const uint64_t t0 = m_MappedReadback[i * 2u + 0u];
                const uint64_t t1 = m_MappedReadback[i * 2u + 1u];
                if (t1 >= t0)
                {
                    m_LastResultsMs[m_PreviousZoneNames[i]] = static_cast<double>(t1 - t0) * ticksToMs;
                }
            }
            m_HasPendingResults = false;
        }

        m_Zones.clear();
        m_PreviousZoneNames.clear();

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdResetQueries(*cmd, *m_QueryPool, 0, m_MaxZones * 2u);
    }

    uint32_t GPUTimer::BeginZone(nri::CommandBuffer *cmd, const char *name)
    {
        if (!m_Supported || !cmd || !name)
            return UINT32_MAX;

        if (m_Zones.size() >= m_MaxZones)
            return UINT32_MAX;

        const uint32_t zoneIndex = static_cast<uint32_t>(m_Zones.size());
        m_Zones.push_back({ name, false });

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdEndQuery(*cmd, *m_QueryPool, zoneIndex * 2u);
        return zoneIndex;
    }

    void GPUTimer::EndZone(nri::CommandBuffer *cmd, uint32_t zoneIndex)
    {
        if (!m_Supported || !cmd || zoneIndex >= m_Zones.size() || m_Zones[zoneIndex].closed)
            return;

        m_Zones[zoneIndex].closed = true;
        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdEndQuery(*cmd, *m_QueryPool, zoneIndex * 2u + 1u);
    }

    void GPUTimer::Resolve(nri::CommandBuffer *cmd)
    {
        if (!m_Supported || !cmd || m_Zones.empty())
            return;

        m_PreviousZoneNames.clear();
        m_PreviousZoneNames.reserve(m_Zones.size());
        for (const auto &z : m_Zones)
        {
            m_PreviousZoneNames.push_back(z.closed ? z.name : std::string());
        }

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
        nri.CmdCopyQueries(*cmd, *m_QueryPool, 0, static_cast<uint32_t>(m_Zones.size()) * 2u, *m_ReadbackBuffer, 0);
        m_HasPendingResults = true;
    }

    double GPUTimer::GetZoneMs(const std::string &name) const
    {
        auto it = m_LastResultsMs.find(name);
        return it != m_LastResultsMs.end() ? it->second : 0.0;
    }
}

