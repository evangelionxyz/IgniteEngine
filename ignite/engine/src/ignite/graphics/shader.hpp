// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SHADER_HPP
#define IGN_SHADER_HPP

#include "Umbra/ShaderCompiler.h"

#ifdef GetLastError
#undef GetLastError
#endif

#include "ignite/core/types.hpp"
#include "ignite/core/logger.hpp"

#include <nvrhi/nvrhi.h>
#include <initializer_list>
#include "ignite/core/path.hpp"
#include <string>

namespace ignite
{
    inline nvrhi::Format MapUmbraTypeToNVRHIFormat(const UMBRA_VertexElementFormat &format)
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

    inline uint32_t GetVertexStride(nvrhi::Format format)
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

    inline nvrhi::ShaderType GetNVRHIShaderType(UMBRA_ShaderType shaderType)
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

    inline UMBRA_ShaderType GetUmbraShaderType(nvrhi::ShaderType shaderType)
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

    struct IGN_API ShaderKey
    {
        std::string filename;
        std::string entryName;
        UMBRA_ShaderType shaderType;

        bool operator==(const ShaderKey &other) const noexcept
        {
            return filename == other.filename
                && entryName == other.entryName
                && shaderType == other.shaderType;
        }
    };

    struct IGN_API ShaderKeyHasher
    {
        size_t operator()(const ShaderKey &k) const noexcept
        {
            size_t h1 = std::hash<std::string> {}(k.filename);
            size_t h2 = std::hash<std::string> {}(k.entryName);
            size_t h3 = std::hash<uint32_t> {}(static_cast<uint32_t>(k.shaderType));

            // Combine hashes (boost::hash_combine style)
            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    class Shader;
    using ShaderMap = std::unordered_map<ShaderKey, Ref<Shader>, ShaderKeyHasher>;

    class IGN_API Shader
    {
    public:
        Shader() = default;
        Shader(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile = false, const char *entryName = "main");

		const inline std::vector<nvrhi::VertexAttributeDesc> &GetVertexAttributes() { return m_VertexAttributes; }
        inline nvrhi::ShaderHandle GetHandle() { return m_Handle; }
        inline UMBRA_ShaderType GetType() const { return m_Type; }

        // Returns true if recompile successful.
        bool Recompile();

        // Compile or Get shader code from cached binary data (.spirv/.dxil).
        static std::vector<uint8_t> CompileOrGetShader(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile, const char *entryPoint);
        
        // Helper for reflecting shader code and construct the NVRHI Vertex Attributes
        // Returns UMBRA Shader Reflection Info
        static umbra::ShaderReflectionInfo Reflect(UMBRA_ShaderType shaderType, const std::vector<uint8_t> &shaderCode, std::vector<nvrhi::VertexAttributeDesc> &vertexAttributes);
        
        // Create interface
        static Ref<Shader> Create(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile = false, const char *entryName = "main");

        // DX Compiler Instance, automatically create if not created yet.
        static Ref<umbra::DXCInstance> GetDXCInstance();

        static ShaderMap &GetShaderCache();

        inline const umbra::ShaderReflectionInfo &GetReflectionInfo() { return m_ReflectionInfo; }
        inline const std::string &GetName() { return m_Name; }

    private:
        static void InitShaderData();
        static void ShutdownShaderData();
    
    private:
        UMBRA_ShaderType m_Type;
        nvrhi::ShaderDesc m_ShaderDesc;
        nvrhi::ShaderHandle m_Handle = nullptr;
        ignite::Path m_Filepath;
        std::string m_Name;

        umbra::ShaderReflectionInfo m_ReflectionInfo;
		std::vector<nvrhi::VertexAttributeDesc> m_VertexAttributes;

        static Ref<umbra::DXCInstance> s_DXCInstance;
        static ShaderMap s_ShaderCache;

        friend class Renderer;
    };
}

#endif