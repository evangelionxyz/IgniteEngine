// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_BATCH_BUILDER_HPP
#define IGN_BATCH_BUILDER_HPP

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>

#include "ignite/core/types.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/binding_set.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/vertex_data.hpp"

namespace ignite
{
    struct BatchKey
    {
        nri::Buffer *vertexBuffer = nullptr;
        nri::Buffer *indexBuffer = nullptr;
        nri::DescriptorSet *meshBindingSet = nullptr;   // camera / object / scene bindings
        nri::DescriptorSet *materialBindingSet = nullptr;
        nri::Pipeline *pipeline = nullptr;

        bool operator==(const BatchKey &o) const noexcept
        {
            return vertexBuffer       == o.vertexBuffer
                && indexBuffer        == o.indexBuffer
                && meshBindingSet     == o.meshBindingSet
                && materialBindingSet == o.materialBindingSet
                && pipeline           == o.pipeline;
        }
    };

    struct BatchKeyHash
    {
        size_t operator()(const BatchKey &k) const noexcept
        {
            size_t h = 0;
            auto combine = [&](const void *ptr)
            {
                h ^= std::hash<const void *>{}(ptr) + 0x9e3779b9u + (h << 6) + (h >> 2);
            };
            combine(k.vertexBuffer);
            combine(k.indexBuffer);
            combine(k.meshBindingSet);
            combine(k.materialBindingSet);
            combine(k.pipeline);
            return h;
        }
    };

    // -----------------------------------------------------------------------
    // DrawBatch — a single instanced draw call with N instances.
    // objectIndices[i] is the index into the global ObjectBuffer for instance i.
    // -----------------------------------------------------------------------
    struct DrawBatch
    {
        nri::Buffer *vertexBuffer = nullptr;
        nri::Buffer *indexBuffer = nullptr;
        Ref<BindingSet> meshBindingSet;
        Ref<BindingSet> materialBindingSet;
        Ref<GraphicsPipeline> pipeline;

        uint32_t vertexStride = sizeof(VertexMeshStatic);
        uint32_t indexCount = 0;

        // Per-instance object indices (indices into the global Mesh_GPUData StructuredBuffer)
        std::vector<uint32_t> objectIndices;

        uint32_t GetInstanceCount() const { return static_cast<uint32_t>(objectIndices.size()); }
    };

    // -----------------------------------------------------------------------
    // BatchBuilder — accumulates render submissions and produces a flat list
    // of DrawBatch objects ready for GPU dispatch.
    // -----------------------------------------------------------------------
    class BatchBuilder
    {
    public:
        void Clear();

        /// Submit one instance to a batch identified by key.
        void Submit(const BatchKey &key, nri::Buffer *vertexBuffer, nri::Buffer *indexBuffer,
            const Ref<BindingSet> &meshBindingSet, const Ref<BindingSet> &materialBindingSet, const Ref<GraphicsPipeline> &pipeline,
            uint32_t indexCount, uint32_t objectIndex, uint32_t vertexStride = sizeof(VertexMeshStatic));

        /// Sorts and flattens the internal map into the final batch vector.
        void Finalize();

        const std::vector<DrawBatch> &GetBatches() const { return m_Batches; }

        bool IsEmpty() const { return m_Map.empty(); }

    private:
        std::unordered_map<BatchKey, DrawBatch, BatchKeyHash> m_Map;
        std::vector<DrawBatch> m_Batches;
    };

}

#endif
