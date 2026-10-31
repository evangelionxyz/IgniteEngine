// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SHADER_HPP
#define IGN_SHADER_HPP

#include "Umbra/ShaderCompiler.h"

#ifdef GetLastError
#undef GetLastError
#endif

#include "ignite/asset/asset.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/logger.hpp"

#include <NRI.h>
#include "ignite/core/file_watcher.hpp"

#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace ignite
{
    inline nri::Format MapUmbraTypeToNRIFormat(const UMBRA_VertexElementFormat &format)
    {
        switch (format)
        {
            case UMBRA_VERTEX_ELEMENT_FORMAT_FLOAT: return nri::Format::R32_SFLOAT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_FLOAT2: return nri::Format::RG32_SFLOAT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_FLOAT3: return nri::Format::RGB32_SFLOAT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_FLOAT4: return nri::Format::RGBA32_SFLOAT;

            case UMBRA_VERTEX_ELEMENT_FORMAT_INT: return nri::Format::R32_SINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_INT2: return nri::Format::RG32_SINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_INT3: return nri::Format::RGB32_SINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_INT4: return nri::Format::RGBA32_SINT;

            case UMBRA_VERTEX_ELEMENT_FORMAT_UINT: return nri::Format::R32_UINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_UINT2: return nri::Format::RG32_UINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_UINT3: return nri::Format::RGB32_UINT;
            case UMBRA_VERTEX_ELEMENT_FORMAT_UINT4: return nri::Format::RGBA32_UINT;
            default:
                LOG_ASSERT(false, "[Shader] Unreachable, Invalid Vertex Format");
                return nri::Format::UNKNOWN;
        }
    }

    inline uint32_t GetVertexStride(nri::Format format)
    {
        constexpr uint32_t BASE_SIZE = 4;
        switch (format)
        {
            case nri::Format::R32_SFLOAT:
            case nri::Format::R32_SINT:
            case nri::Format::R32_UINT: return BASE_SIZE * 1;

            case nri::Format::RG32_SFLOAT:
            case nri::Format::RG32_SINT:
            case nri::Format::RG32_UINT: return BASE_SIZE * 2;

            case nri::Format::RGB32_SFLOAT:
            case nri::Format::RGB32_SINT:
            case nri::Format::RGB32_UINT: return BASE_SIZE * 3;

            case nri::Format::RGBA32_SFLOAT:
            case nri::Format::RGBA32_SINT:
            case nri::Format::RGBA32_UINT: return BASE_SIZE * 4;
            default:
                LOG_ASSERT(false, "[Shader] Unreachable, Invalid Vertex Format!");
                return 0;
        }
    }

    inline nri::StageBits GetNRIStageBits(UMBRA_ShaderType shaderType)
    {
        switch (shaderType)
        {
            case UMBRA_SHADER_TYPE_VERTEX: return nri::StageBits::VERTEX_SHADER;
            case UMBRA_SHADER_TYPE_PIXEL: return nri::StageBits::FRAGMENT_SHADER;
            case UMBRA_SHADER_TYPE_GEOMETRY: return nri::StageBits::GEOMETRY_SHADER;
            case UMBRA_SHADER_TYPE_COMPUTE: return nri::StageBits::COMPUTE_SHADER;
            default:
                LOG_ASSERT(false, "[Shader] Unreachable, Invalid Shader Type");
                return nri::StageBits::NONE;
        }
    }

    inline UMBRA_ShaderType GetUmbraShaderType(nri::StageBits stage)
    {
        switch (stage)
        {
            case nri::StageBits::VERTEX_SHADER: return UMBRA_SHADER_TYPE_VERTEX;
            case nri::StageBits::FRAGMENT_SHADER: return UMBRA_SHADER_TYPE_PIXEL;
            case nri::StageBits::GEOMETRY_SHADER: return UMBRA_SHADER_TYPE_GEOMETRY;
            case nri::StageBits::COMPUTE_SHADER: return UMBRA_SHADER_TYPE_COMPUTE;
            default: break;
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

    using ShaderReloadToken = uint32_t;
    using ShaderReloadCallback = std::function<void(Shader &)>;

    class IGN_API Shader : public Asset
    {
    public:
        Shader() = default;
        Shader(const std::filesystem::path &filepath, UMBRA_ShaderType shaderType, bool recompile = false, const char *entryName = "main");
        ~Shader();

        Shader(const Shader &) = delete;
        Shader &operator=(const Shader &) = delete;

        // Called (main thread) after this shader was successfully recompiled, e.g. by the file watcher.
        // Returns a token for RemoveReloadListener. Safe to call from inside a listener.
        ShaderReloadToken AddReloadListener(ShaderReloadCallback callback);
        void RemoveReloadListener(ShaderReloadToken token);

        const std::filesystem::path &GetFilepath() const { return m_Filepath; }

        const std::vector<nri::VertexAttributeDesc> &GetVertexAttributes() const { return m_VertexAttributes; }
        const std::vector<nri::VertexStreamDesc> &GetVertexStreams() const { return m_VertexStreams; }
        uint32_t GetVertexStride() const { return m_VertexStride; }

        nri::ShaderDesc GetShaderDesc() const
        {
            nri::ShaderDesc desc = {};
            desc.stage = m_Stage;
            desc.bytecode = m_Bytecode.data();
            desc.size = m_Bytecode.size();
            desc.entryPointName = m_EntryName.c_str();
            return desc;
        }

        const std::vector<uint8_t> &GetBytecode() const { return m_Bytecode; }
        bool IsValid() const { return !m_Bytecode.empty(); }
        UMBRA_ShaderType GetType() const { return m_Type; }
        nri::StageBits GetStage() const { return m_Stage; }

        // Returns true if recompile successful.
        bool Recompile();

        // Compile or Get shader code from cached binary data (.spirv/.dxil).
        static std::vector<uint8_t> CompileOrGetShader(const std::filesystem::path &filepath, UMBRA_ShaderType shaderType, bool recompile, const char *entryPoint);

        // Helper for reflecting shader code and constructing NRI Vertex Attributes
        // Returns UMBRA Shader Reflection Info
        static umbra::ShaderReflectionInfo Reflect(
            UMBRA_ShaderType shaderType,
            const std::vector<uint8_t> &shaderCode,
            std::vector<std::string> &outSemanticNames,
            std::vector<nri::VertexAttributeDesc> &outVertexAttributes,
            std::vector<nri::VertexStreamDesc> &outVertexStreams,
            uint32_t &outVertexStride);

        // Create interface
        static Ref<Shader> Create(const std::filesystem::path &filepath, UMBRA_ShaderType shaderType, bool recompile = false, const char *entryName = "main");

        // DX Compiler Instance, automatically create if not created yet.
        static Ref<umbra::DXCInstance> GetDXCInstance();

        static ShaderMap &GetShaderCache();

        const umbra::ShaderReflectionInfo &GetReflectionInfo() const { return m_ReflectionInfo; }
        const std::string &GetName() const { return m_Name; }

        static AssetType GetStaticAssetType() { return AssetType::Shader; }
        virtual AssetType GetAssetType() override { return GetStaticAssetType(); }

    private:
        static void InitShaderData();
        static void ShutdownShaderData();

    private:
        UMBRA_ShaderType m_Type = UMBRA_SHADER_TYPE_VERTEX;
        nri::StageBits m_Stage = nri::StageBits::NONE;
        std::string m_EntryName = "main";
        std::vector<uint8_t> m_Bytecode;
        std::filesystem::path m_Filepath;
        std::string m_Name;

        umbra::ShaderReflectionInfo m_ReflectionInfo;

        FileWatchToken m_FileWatchToken = kInvalidFileWatchToken;
        std::vector<std::pair<ShaderReloadToken, ShaderReloadCallback>> m_ReloadListeners;
        ShaderReloadToken m_LastReloadToken = 0;

        std::vector<std::string> m_SemanticNames;
        std::vector<nri::VertexAttributeDesc> m_VertexAttributes;
        std::vector<nri::VertexStreamDesc> m_VertexStreams;
        uint32_t m_VertexStride = 0;

        static Ref<umbra::DXCInstance> s_DXCInstance;
        static ShaderMap s_ShaderCache;

        friend class Renderer;
    };
}

#endif
