// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_VERTEX_DATA_HPP
#define IGN_VERTEX_DATA_HPP

#include "ignite/scene/icamera.hpp"
#include "ignite/core/types.hpp"
#include "gpu_data.hpp"
#include <nvrhi/nvrhi.h>
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

		static nvrhi::BindingLayoutDesc GetBindingLayoutDesc()
		{
			return nvrhi::BindingLayoutDesc()
				.setRegisterSpace(0) // set 0
				.setRegisterSpaceIsDescriptorSet(true)
				.setVisibility(nvrhi::ShaderType::All)
				.addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(uint32_t)))  // Push Constants - Object Index
				.addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(1))  // Camera
                .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2))  // Object
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(3))  // Scene
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(4))  // CSM
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(5))  // PointLight
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(6)); // SpotLight
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

        static nvrhi::BindingLayoutDesc GetBindingLayoutDesc()
        {
            return nvrhi::BindingLayoutDesc()
                .setRegisterSpace(0) // set 0
                .setRegisterSpaceIsDescriptorSet(true)
                .setVisibility(nvrhi::ShaderType::All)
                .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(uint32_t)))  // Push Constants - Object Index
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(1))  // Camera
                .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(2))  // Object
                .addItem(nvrhi::BindingLayoutItem::StructuredBuffer_SRV(3))  // Skeleton
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(4))  // Scene
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(5))  // CSM
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(6))  // PointLight
                .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(7)); // SpotLight
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
