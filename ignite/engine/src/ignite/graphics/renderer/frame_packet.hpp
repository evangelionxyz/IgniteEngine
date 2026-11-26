// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_FRAME_PACKET_HPP
#define IGN_FRAME_PACKET_HPP

#include "ignite/core/base.hpp"
#include "ignite/graphics/gpu_data.hpp"
#include "ignite/math/aabb.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>

namespace ignite
{
    class StaticMesh;
    class SkeletalMesh;
    class Material;
    struct TransformComponent;
    struct RenderingComponent;
    struct StaticMeshComponent;
    struct SkeletalMeshComponent;
    struct TerrainComponent;

    struct LocalShadowLightItem
    {
        uint32_t slot = 0; // 0..7 point lights, 8..11 spot lights
        glm::vec3 position{ 0.0f };
        float range = 1.0f;
        bool isSpot = false;
        glm::vec3 direction{ 0.0f, 0.0f, 1.0f };
        float cosOuterAngle = -1.0f;
        bool castShadow = true;
        bool bakeStaticShadow = true;
    };

    struct StaticMeshPacketItem
    {
        entt::entity entity = entt::null;
        uint32_t objectID = 0;
        glm::mat4 worldMatrix{ 1.0f };
        glm::mat4 normalMatrix{ 1.0f };
        StaticMesh *mesh = nullptr;
        const StaticMeshComponent *component = nullptr;
        uint32_t firstObjectIndexOffset = 0;
        uint32_t instanceCount = 0;
        bool isStatic = true;
        bool castShadow = true;
        bool receiveShadow = true;
    };

    struct SkeletalMeshPacketItem
    {
        entt::entity entity = entt::null;
        uint32_t objectID = 0;
        glm::mat4 worldMatrix{ 1.0f };
        glm::mat4 normalMatrix{ 1.0f };
        SkeletalMesh *mesh = nullptr;
        const SkeletalMeshComponent *component = nullptr;
        uint32_t boneOffset = 0;
        bool isStatic = false;
        bool castShadow = true;
        bool receiveShadow = true;
    };

    struct TerrainPacketItem
    {
        entt::entity entity = entt::null;
        uint32_t objectID = 0;
        glm::mat4 worldMatrix{ 1.0f };
        glm::mat4 normalMatrix{ 1.0f };
        TerrainComponent *component = nullptr;
        Material *material = nullptr;
        bool isStatic = true;
        bool castShadow = true;
        bool receiveShadow = true;
    };

    struct FramePacketStats
    {
        uint32_t totalStaticMeshes = 0;
        uint32_t visibleStaticMeshes = 0;
        uint32_t totalSkeletalMeshes = 0;
        uint32_t visibleSkeletalMeshes = 0;
        uint32_t totalTerrains = 0;
        uint32_t pointLightCount = 0;
        uint32_t spotLightCount = 0;
        uint32_t shadowDrawCalls = 0;
        uint32_t colorDrawCalls = 0;
        bool hasDirectionalShadow = false;
    };

    // Optimal per-frame render packet extracted once per camera/frame.
    // Keeps contiguous SoA arrays for SIMD frustum culling and reuses heap capacity across frames.
    struct FramePacket
    {
        // Camera & view state
        glm::mat4 view{ 1.0f };
        glm::mat4 projection{ 1.0f };
        glm::mat4 viewProjection{ 1.0f };
        glm::vec3 cameraPosition{ 0.0f };
        uint32_t viewportWidth = 1;
        uint32_t viewportHeight = 1;
        bool isGameCamera = false;
        bool drawDebug = false;

        // Lighting & shadow state
        PointLightBufferData pointLights{};
        SpotLightBufferData spotLights{};
        std::vector<LocalShadowLightItem> localShadowLights;
        glm::vec3 sunDirection{ 0.0f, 1.0f, 0.0f };
        float shadowDistance = 200.0f;
        bool cascadeShadow = false;

        // Extracted renderables (parallel with *AABBs for cache-friendly SIMD culling)
        std::vector<StaticMeshPacketItem> staticMeshes;
        std::vector<AABB> staticMeshAABBs;
        std::vector<uint32_t> staticMeshObjectIndices;
        std::vector<AABB> staticSubmeshAABBs;

        std::vector<SkeletalMeshPacketItem> skeletalMeshes;
        std::vector<AABB> skeletalMeshAABBs;

        std::vector<TerrainPacketItem> terrains;

        // Reusable visibility scratch buffer (avoids per-pass heap allocations)
        std::vector<uint8_t> visibilityScratch;

        FramePacketStats stats{};

        void Clear()
        {
            staticMeshes.clear();
            staticMeshAABBs.clear();
            staticMeshObjectIndices.clear();
            staticSubmeshAABBs.clear();
            skeletalMeshes.clear();
            skeletalMeshAABBs.clear();
            terrains.clear();
            visibilityScratch.clear();
            stats = {};
        }

        void Reserve(size_t staticCount = 256, size_t skelCount = 64)
        {
            if (staticMeshes.capacity() < staticCount)
            {
                staticMeshes.reserve(staticCount);
                staticMeshAABBs.reserve(staticCount);
                staticMeshObjectIndices.reserve(staticCount * 2);
                staticSubmeshAABBs.reserve(staticCount * 2);
            }
            if (skeletalMeshes.capacity() < skelCount)
            {
                skeletalMeshes.reserve(skelCount);
                skeletalMeshAABBs.reserve(skelCount);
            }
            if (visibilityScratch.capacity() < std::max(staticCount, skelCount))
            {
                visibilityScratch.reserve(std::max(staticCount, skelCount));
            }
        }
    };
}

#endif
