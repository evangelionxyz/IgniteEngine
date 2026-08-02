// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "batch_builder.hpp"

#include <algorithm>

namespace ignite
{
    void BatchBuilder::Clear()
    {
        m_Map.clear();
        m_Batches.clear();
    }

    void BatchBuilder::Submit(const BatchKey &key, nvrhi::BufferHandle vertexBuffer, nvrhi::BufferHandle indexBuffer,
        nvrhi::BindingSetHandle meshBindingSet, nvrhi::BindingSetHandle materialBindingSet, nvrhi::GraphicsPipelineHandle pipeline,
        uint32_t indexCount, uint32_t objectIndex)
    {
        auto it = m_Map.find(key);
        if (it == m_Map.end())
        {
            DrawBatch batch;
            batch.vertexBuffer = vertexBuffer;
            batch.indexBuffer = indexBuffer;
            batch.meshBindingSet = meshBindingSet;
            batch.materialBindingSet = materialBindingSet;
            batch.pipeline = pipeline;
            batch.indexCount = indexCount;
            batch.objectIndices.push_back(objectIndex);
            m_Map.emplace(key, std::move(batch));
        }
        else
        {
            it->second.objectIndices.push_back(objectIndex);
        }
    }

    void BatchBuilder::Finalize()
    {
        m_Batches.clear();
        m_Batches.reserve(m_Map.size());

        for (auto &[key, batch] : m_Map)
        {
            m_Batches.push_back(std::move(batch));
        }

        // Optional: sort by pipeline to minimize state changes
        std::sort(m_Batches.begin(), m_Batches.end(), [](const DrawBatch &a, const DrawBatch &b)
        {
            return a.pipeline.Get() < b.pipeline.Get();
        });
    }

}
