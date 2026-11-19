// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GPU_TIMER_HPP
#define IGN_GPU_TIMER_HPP

#include "ignite/core/base.hpp"
#include <NRI.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace ignite
{
    class IGN_API GPUTimer
    {
    public:
        GPUTimer() = default;
        ~GPUTimer();

        GPUTimer(const GPUTimer &) = delete;
        GPUTimer &operator=(const GPUTimer &) = delete;

        bool Initialize(nri::Device *device, uint32_t maxZones = 32);
        void Shutdown();

        bool IsSupported() const { return m_Supported; }

        // Call at the start of a command buffer whose previous submission has finished on the GPU.
        // Reads back the previous resolved timestamps, updates results, and resets the query pool.
        void BeginFrame(nri::CommandBuffer *cmd);

        uint32_t BeginZone(nri::CommandBuffer *cmd, const char *name);
        void EndZone(nri::CommandBuffer *cmd, uint32_t zoneIndex);

        // Call at the end of a command buffer (outside rendering) to copy timestamp queries to readback memory.
        void Resolve(nri::CommandBuffer *cmd);

        double GetZoneMs(const std::string &name) const;
        const std::unordered_map<std::string, double> &GetResultsMs() const { return m_LastResultsMs; }

    private:
        struct ZoneRecord
        {
            std::string name;
            bool closed = false;
        };

        nri::Device *m_Device = nullptr;
        nri::QueryPool *m_QueryPool = nullptr;
        nri::Buffer *m_ReadbackBuffer = nullptr;
        nri::Memory *m_ReadbackMemory = nullptr;
        uint64_t *m_MappedReadback = nullptr;

        uint64_t m_TimestampFrequencyHz = 0;
        uint32_t m_MaxZones = 0;
        bool m_Supported = false;
        bool m_HasPendingResults = false;

        std::vector<ZoneRecord> m_Zones;
        std::vector<std::string> m_PreviousZoneNames;
        std::unordered_map<std::string, double> m_LastResultsMs;
    };
}

#endif

