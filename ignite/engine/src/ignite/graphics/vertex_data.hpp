// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_VERTEX_DATA_HPP
#define IGN_VERTEX_DATA_HPP

#include "ignite/scene/icamera.hpp"
#include "ignite/core/types.hpp"
#include "gpu_data.hpp"
#include "binding_set.hpp"
#include <NRI.h>
#include <glm/glm.hpp>

#include <array>
#include <concepts>
#include <type_traits>

namespace ignite
{
    template<typename T>
    concept MeshVertex = requires(T v)
    {
        { v.position } -> std::convertible_to<glm::vec3>;
        { v.normal } -> std::convertible_to<glm::vec3>;
        { v.tangent } -> std::convertible_to<glm::vec3>;
        { v.bitangent } -> std::convertible_to<glm::vec3>;
        { v.uv } -> std::convertible_to<glm::vec2>;
        { v.color } -> std::convertible_to<glm::vec4>;
    };

    template<typename T>
    concept AnimatedVertex = requires(T v)
    {
        { v.boneIDs } -> std::convertible_to<std::array<uint32_t, 4>>;
        { v.weights } -> std::convertible_to<std::array<float, 4>>;
    };

    template<typename T>
    concept StaticMeshVertex = MeshVertex<T> && (!AnimatedVertex<T>);

    template<typename T>
    concept SkeletalMeshVertex = MeshVertex<T> && AnimatedVertex<T>;

    struct VertexMeshStatic
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 uv;
        glm::vec4 color = glm::vec4(1.0f);

        static BindingLayoutDesc GetBindingLayoutDesc()
        {
            return BindingLayoutDesc()
                .SetRegisterSpace(0) // set 0
                .SetVisibility(nri::StageBits::ALL)
                .AddItem(BindingLayoutItem::PushConstants(0, sizeof(uint32_t))) // b0: Push Constants - base instance offset
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(1))          // b1: Camera
                .AddItem(BindingLayoutItem::StructuredBufferSRV(2))             // t2: ObjectBuffer (Mesh_GPUData[])
                .AddItem(BindingLayoutItem::StructuredBufferSRV(3))             // t3: InstanceIndexBuffer (uint[])
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(4))          // b4: Scene
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(5))          // b5: CSM
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(6))          // b6: PointLight
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(7));         // b7: SpotLight
        }
    };

    struct VertexMeshAnim
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec2 uv;
        glm::vec4 color = glm::vec4(1.0f);
        std::array<uint32_t, VERTEX_MAX_BONES> boneIDs = { 0 };
        std::array<float, VERTEX_MAX_BONES> weights = { 0.0f };

        static BindingLayoutDesc GetBindingLayoutDesc()
        {
            return BindingLayoutDesc()
                .SetRegisterSpace(0) // set 0
                .SetVisibility(nri::StageBits::ALL)
                .AddItem(BindingLayoutItem::PushConstants(0, sizeof(uint32_t))) // Push Constants
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(1)) // Camera
                .AddItem(BindingLayoutItem::StructuredBufferSRV(2))    // Object
                .AddItem(BindingLayoutItem::StructuredBufferSRV(3))    // Skeleton
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(4)) // Scene
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(5)) // CSM
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(6)) // PointLight
                .AddItem(BindingLayoutItem::VolatileConstantBuffer(7));// SpotLight
        }
    };

    struct VertexScreen
    {
        glm::vec2 position;
        glm::vec2 texCoord;
    };

    struct Vertex2DQuad
    {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec2 tilingFactor;
        glm::vec4 color;
        glm::vec4 additiveColor;
        uint32_t texIndex;
        uint32_t materialType;
        uint32_t objectID;
    };

    struct Vertex2DLine
    {
        glm::vec3 position;
        glm::vec4 color;
    };

    struct Vertex2DCircle
    {
        glm::vec4 position;
        glm::vec2 localPosition;
        glm::vec4 color;
        uint32_t objectID;
    };

    struct VertexText
    {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 texCoord;
        uint32_t texIndex;
        uint32_t objectID;
    };

    struct VertexWidgetQuad
    {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec2 tilingFactor;
        glm::vec4 color;
        uint32_t texIndex;
    };

    struct VertexWidgetText
    {
        glm::vec3 position;
        glm::vec4 color;
        glm::vec2 texCoord;
        uint32_t texIndex;
    };
}

#endif
