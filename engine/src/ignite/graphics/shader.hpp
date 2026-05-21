// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef SHADER_HPP
#define SHADER_HPP

#include "Umbra/ShaderCompiler.h"

#include "ignite/core/types.hpp"
#include "ignite/core/logger.hpp"

#include <nvrhi/nvrhi.h>
#include <initializer_list>
#include "ignite/core/path.hpp"
#include <string>

namespace ignite
{
    static nvrhi::Format MapUmbraTypeToNVRHIFormat(const UMBRA_VertexElementFormat &format)
    {
        switch (format)
        {
            case UMBRA_VERTEX_ELEMENT_FORMAT_FLOAT: return nvrhi::Format::R32_FLOAT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_FLOAT2: return nvrhi::Format::RG32_FLOAT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_FLOAT3: return nvrhi::Format::RGB32_FLOAT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_FLOAT4: return nvrhi::Format::RGBA32_FLOAT;

            case UMBRA_VERTEX_ELEMENT_FORMAT_INT: return nvrhi::Format::R32_SINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_INT2: return nvrhi::Format::RG32_SINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_INT3: return nvrhi::Format::RGB32_SINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_INT4: return nvrhi::Format::RGBA32_SINT;

            case UMBRA_VERTEX_ELEMENT_FORMAT_UINT: return nvrhi::Format::R32_UINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_UINT2: return nvrhi::Format::RG32_UINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_UINT3: return nvrhi::Format::RGB32_UINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_UINT4: return nvrhi::Format::RGBA32_UINT;
            default:
                LOG_ASSERT(false, "[Shader] Unreachable, Invalid Vertex Format");
                return nvrhi::Format::UNKNOWN;
        }
    }

    static uint32_t GetVertexStride(nvrhi::Format format)
    {
        constexpr uint32_t BASE_SIZE = 4;
        switch (format)
        {
            case nvrhi::Format::R32_FLOAT:
            case nvrhi::Format::R32_SINT:
            case nvrhi::Format::R32_UINT: return BASE_SIZE * 1;

            case nvrhi::Format::RG32_FLOAT:
            case nvrhi::Format::RG32_SINT:
            case nvrhi::Format::RG32_UINT: return BASE_SIZE * 2;

            case nvrhi::Format::RGB32_FLOAT:
            case nvrhi::Format::RGB32_SINT:
            case nvrhi::Format::RGB32_UINT: return BASE_SIZE * 3;

            case nvrhi::Format::RGBA32_FLOAT:
            case nvrhi::Format::RGBA32_SINT:
            case nvrhi::Format::RGBA32_UINT: return BASE_SIZE * 4;
            default:
                LOG_ASSERT(false, "[Shader] Unreachable, Invalid Vertex Format!");
                return 0;
        }
    }

    static nvrhi::ShaderType GetNVRHIShaderType(UMBRA_ShaderType shaderType)
    {
        switch (shaderType)
        {
            case UMBRA_SHADER_TYPE_VERTEX: return nvrhi::ShaderType::Vertex;
            case UMBRA_SHADER_TYPE_PIXEL: return nvrhi::ShaderType::Pixel;
            case UMBRA_SHADER_TYPE_GEOMETRY: return nvrhi::ShaderType::Geometry;
            case UMBRA_SHADER_TYPE_COMPUTE: return nvrhi::ShaderType::Compute;
            default:
                LOG_ASSERT(false, "[Shader] Unreachable, Invalid Shader Type");
                return nvrhi::ShaderType::None;
        }
    }

    static UMBRA_ShaderType GetUmbraShaderType(nvrhi::ShaderType shaderType)
    {
        switch (shaderType)
        {
            case nvrhi::ShaderType::Vertex: return UMBRA_SHADER_TYPE_VERTEX;
            case nvrhi::ShaderType::Pixel: return UMBRA_SHADER_TYPE_PIXEL;
            case nvrhi::ShaderType::Geometry: return UMBRA_SHADER_TYPE_GEOMETRY;
            case nvrhi::ShaderType::Compute: return UMBRA_SHADER_TYPE_COMPUTE;
        }

        LOG_ASSERT(false, "[Shader] Unreachable, Invalid Shader Type");
        return UMBRA_SHADER_TYPE_VERTEX;
    }

    class Shader
    {
    public:
        Shader() = default;
        Shader(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile = false);


		const std::vector<nvrhi::VertexAttributeDesc> &GetVertexAttributes() { return m_VertexAttributes; }
        inline nvrhi::ShaderHandle GetHandle() { return m_Handle; }
        inline UMBRA_ShaderType GetType() const { return m_Type; }

        static std::vector<uint8_t> CompileOrGetShader(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile);
        static Ref<Shader> Create(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile = false);

        void Reflect(UMBRA_ShaderType shaderType, const std::vector<uint8_t> &shaderCode, std::vector<nvrhi::VertexAttributeDesc> &vertexAttributes);

        static Ref<umbra::DXCInstance> GetDXCInstance();
    
    private:
        static void SetupLogCallback();
        static void ClearLogCallback();
    
    private:
        UMBRA_ShaderType m_Type;
		std::vector<nvrhi::VertexAttributeDesc> m_VertexAttributes;
        nvrhi::ShaderHandle m_Handle = nullptr;

        static Ref<umbra::DXCInstance> s_DXCInstance;

        friend class Renderer;
    };
}

#endif