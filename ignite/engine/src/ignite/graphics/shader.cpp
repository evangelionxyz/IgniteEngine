// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "shader.hpp"
#include "renderer.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/file_watcher.hpp"

#include <cctype>

namespace ignite
{
    Ref<umbra::DXCInstance> Shader::s_DXCInstance = nullptr;
    ShaderMap Shader::s_ShaderCache;

    namespace
    {
        static std::string GetShaderCacheDirectory()
        {
            return "resources/shaders/bin/";
        }

        static void CreateShaderCachedDirectoryIfNeeded()
        {
            static std::string cachedDirectory = GetShaderCacheDirectory();
            if (!std::filesystem::exists(cachedDirectory))
                std::filesystem::create_directories(cachedDirectory);
        }

        static void ShaderDebugLog(UMBRA_LogType type, const char *message, void *userData)
        {
            (void)userData;
            switch (type)
            {
                default:
                case UMBRA_LOG_TYPE_INFO: LOG_TRACE("[Shader]\t{}", message); break;
                case UMBRA_LOG_TYPE_WARNING: LOG_WARN("[Shader]\t{}", message); break;
                case UMBRA_LOG_TYPE_ERROR: LOG_ERROR("[Shader]\t{}", message); break;
                case UMBRA_LOG_TYPE_CRITICAL: LOG_ASSERT(false, "[Shader]\t{}", message); break;
            }
        }

        static const char *GetShaderExtension(nri::GraphicsAPI api)
        {
            switch (api)
            {
                case nri::GraphicsAPI::D3D12: return ".dxil";
                case nri::GraphicsAPI::VK: return ".spirv";
                default: break;
            }

            LOG_ASSERT(false, "[Shader] Unreachable, Invalid Graphics API");
            return "[Shader] Unreachable, Invalid Graphics API";
        }
    }

    void Shader::InitShaderData()
    {
        umbra::ShaderCompiler::SetLogCallback(ShaderDebugLog, nullptr);
        Shader::s_ShaderCache.clear();
    }

    void Shader::ShutdownShaderData()
    {
        Shader::s_ShaderCache.clear();
        umbra::ShaderCompiler::ClearLogCallback();
    }

    Shader::Shader(const std::filesystem::path &filepath, UMBRA_ShaderType shaderType, bool recompile, const char *entryName)
        : m_Type(shaderType), m_EntryName(entryName ? entryName : "main"), m_Filepath(filepath)
    {
        CreateShaderCachedDirectoryIfNeeded();

        m_Name = filepath.stem().string();
        m_Stage = GetNRIStageBits(m_Type);

        // Compile shader source
        m_Bytecode = CompileOrGetShader(m_Filepath, m_Type, recompile, m_EntryName.c_str());
        LOG_ASSERT(m_Bytecode.data() && !m_Bytecode.empty(), "[Shader] Blob data is not valid {}", m_Filepath);

        if (!m_Bytecode.empty())
        {
            LOG_WARN("[Shader] Reflection for '{}': ", m_Filepath.generic_string());
            m_ReflectionInfo = Reflect(m_Type, m_Bytecode, m_SemanticNames, m_VertexAttributes, m_VertexStreams, m_VertexStride);
        }

        // Hot reload: recompile when the source file changes on disk.
        // FileWatcher invokes this on the main thread from FileWatcher::Update().
        m_FileWatchToken = FileWatcher::WatchFile(m_Filepath, [this](const FileChangeEvent &event)
        {
            if (event.type == FileChangeType::Removed)
                return;

            LOG_INFO("[Shader] Source changed, recompiling '{}'", event.path.generic_string());
            Recompile();
        });
    }

    Shader::~Shader()
    {
        FileWatcher::Unwatch(m_FileWatchToken);
    }

    bool Shader::Recompile()
    {
        // Compile shader source. A failed compile (e.g. a syntax error while editing) must not
        // crash or drop the working shader, so only log and keep the previous bytecode.
        std::vector<uint8_t> shaderCode = CompileOrGetShader(m_Filepath, m_Type, true, m_EntryName.c_str());
        if (shaderCode.empty())
        {
            LOG_ERROR("[Shader] Recompile failed, keeping previous shader: {}", m_Filepath.generic_string());
            return false;
        }

        m_Bytecode = std::move(shaderCode);
        LOG_WARN("[Shader] Reflection for '{}': ", m_Filepath.generic_string());
        m_ReflectionInfo = Reflect(m_Type, m_Bytecode, m_SemanticNames, m_VertexAttributes, m_VertexStreams, m_VertexStride);

        NotifyChange();

        // Notify only the objects that depend on this specific shader (e.g. its PSOs).
        // Copy so listeners can add/remove listeners while being notified.
        const auto listeners = m_ReloadListeners;
        for (const auto &[token, callback] : listeners)
        {
            if (callback)
                callback(*this);
        }

        return true;
    }

    ShaderReloadToken Shader::AddReloadListener(ShaderReloadCallback callback)
    {
        if (!callback)
            return 0;

        const ShaderReloadToken token = ++m_LastReloadToken;
        m_ReloadListeners.emplace_back(token, std::move(callback));
        return token;
    }

    void Shader::RemoveReloadListener(ShaderReloadToken token)
    {
        std::erase_if(m_ReloadListeners, [token](const auto &entry) { return entry.first == token; });
    }

    std::vector<uint8_t> Shader::CompileOrGetShader(const std::filesystem::path &filepath, UMBRA_ShaderType shaderType, bool recompile, const char *entryName)
    {
        LOG_ASSERT(std::filesystem::exists(filepath), "[Shader] File does not exists! '{}'", filepath.generic_string().c_str());

        const nri::GraphicsAPI api = DeviceManager::GetInstance()->GetGraphicsAPI();

        umbra::CompilerOptions options = {};
        options.compilerType = UMBRA_SHADER_COMPILER_TYPE_DXC;
#ifdef PLATFORM_WINDOWS
        options.platformType = api == nri::GraphicsAPI::D3D12 ? UMBRA_SHADER_PLATFORM_TYPE_DXIL : UMBRA_SHADER_PLATFORM_TYPE_SPIRV;
#elif PLATFORM_LINUX
        options.platformType = UMBRA_SHADER_PLATFORM_TYPE_SPIRV;
#endif
        options.filepath = filepath.generic_string();
        options.outputFilepath = (filepath.parent_path() / "bin").generic_string();
        options.shaderDesc.entryPoint = entryName;
        options.shaderDesc.shaderModel = "6_6";
        options.shaderDesc.vulkanVersion = "1.3";
        options.shaderDesc.shaderType = shaderType;
        options.shaderDesc.optLevel = UMBRA_OPT_LEVEL_3;
        options.tRegShift = 0;   // Matches DeviceCreationVKDesc::vkBindingOffsets.tRegister
        options.sRegShift = 128; // Matches DeviceCreationVKDesc::vkBindingOffsets.sRegister
        options.bRegShift = 256; // Matches DeviceCreationVKDesc::vkBindingOffsets.bRegister
        options.uRegShift = 384; // Matches DeviceCreationVKDesc::vkBindingOffsets.uRegister

        options.AddCompilerOptions("-Zpc"); // Force column-major matrix packing matching GLM

        if (api == nri::GraphicsAPI::VK)
        {
            options.defines = { "SPIRV", "TARGET_VULKAN" };
            options.AddCompilerOptions("-fvk-bind-resource-heap");
            options.AddCompilerOptions("0"); // binding index
            options.AddCompilerOptions("2"); // set index (space2)
        }

        std::vector<uint8_t> shaderCode;
        std::filesystem::path cacheFilepath = (options.outputFilepath / options.filepath.filename().replace_extension(GetShaderExtension(api))).generic_string();
        if (std::filesystem::exists(cacheFilepath) && !recompile)
        {
            std::ifstream file(cacheFilepath, std::ios::binary);
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            shaderCode.resize(fileSize);
            file.read((char *)(shaderCode.data()), fileSize);
        }
        else
        {
            shaderCode = umbra::ShaderCompiler::CompileDXC(GetDXCInstance(), options);
        }

        return shaderCode;
    }

    umbra::ShaderReflectionInfo Shader::Reflect(
        UMBRA_ShaderType shaderType,
        const std::vector<uint8_t> &shaderCode,
        std::vector<std::string> &outSemanticNames,
        std::vector<nri::VertexAttributeDesc> &outVertexAttributes,
        std::vector<nri::VertexStreamDesc> &outVertexStreams,
        uint32_t &outVertexStride)
    {
        if (shaderCode.size() % sizeof(uint32_t) != 0)
        {
            throw std::runtime_error("Shader blob size is not aligned to 4 bytes");
        }

        umbra::ShaderReflectionInfo reflectInfo;

#ifdef PLATFORM_WINDOWS
        const nri::GraphicsAPI api = DeviceManager::GetInstance()->GetGraphicsAPI();
        if (api == nri::GraphicsAPI::D3D12)
        {
            reflectInfo = umbra::ShaderReflection::DXILReflect(shaderType, shaderCode);
        }
        else if (api == nri::GraphicsAPI::VK)
#endif
        {
            reflectInfo = umbra::ShaderReflection::SPIRVReflect(shaderType, shaderCode);
        }

        // Vertex inputs (only for vertex shaders)
        if (shaderType == UMBRA_SHADER_TYPE_VERTEX)
        {
            outSemanticNames.clear();
            outVertexAttributes.clear();
            outVertexStreams.clear();
            outVertexStride = 0;

            outSemanticNames.reserve(reflectInfo.vertexAttributes.size());
            outVertexAttributes.reserve(reflectInfo.vertexAttributes.size());

            for (const auto &vertexAttr : reflectInfo.vertexAttributes)
            {
                // Skip system-value semantics (SV_VertexID, SV_InstanceID, etc.).
                const bool isSysValue = vertexAttr.name.size() >= 3 &&
                    (vertexAttr.name[0] == 'S' || vertexAttr.name[0] == 's') &&
                    (vertexAttr.name[1] == 'V' || vertexAttr.name[1] == 'v') &&
                    vertexAttr.name[2] == '_';
                if (isSysValue)
                {
                    continue;
                }

                // Split trailing semantic index if present (e.g. "TEXCOORD1" -> "TEXCOORD", 1)
                std::string semanticBase = vertexAttr.name;
                uint32_t semanticIndex = 0;
                size_t digitPos = semanticBase.size();
                while (digitPos > 0 && std::isdigit(static_cast<unsigned char>(semanticBase[digitPos - 1])))
                {
                    --digitPos;
                }
                if (digitPos < semanticBase.size() && digitPos > 0)
                {
                    semanticIndex = static_cast<uint32_t>(std::stoul(semanticBase.substr(digitPos)));
                    semanticBase.resize(digitPos);
                }

                outSemanticNames.push_back(std::move(semanticBase));

                nri::VertexAttributeDesc attr = {};
                attr.d3d.semanticName = outSemanticNames.back().c_str();
                attr.d3d.semanticIndex = semanticIndex;
                attr.vk.location = vertexAttr.location;
                attr.offset = vertexAttr.offset;
                attr.format = MapUmbraTypeToNRIFormat(vertexAttr.format);
                attr.streamIndex = static_cast<uint16_t>(vertexAttr.bufferIndex);

                outVertexAttributes.push_back(attr);
            }

            // Recalculate offsets and strides per vertex buffer stream
            std::unordered_map<uint16_t, uint32_t> bufferStrides;
            for (auto &attr : outVertexAttributes)
            {
                uint32_t &currentOffset = bufferStrides[attr.streamIndex];
                attr.offset = currentOffset;
                currentOffset += ignite::GetVertexStride(attr.format);
            }

            for (const auto &[streamIndex, stride] : bufferStrides)
            {
                nri::VertexStreamDesc stream = {};
                stream.bindingSlot = streamIndex;
                stream.stepRate = nri::VertexStreamStepRate::PER_VERTEX;
                stream.stride = static_cast<uint16_t>(stride);
                outVertexStreams.push_back(stream);

                if (streamIndex == 0)
                {
                    outVertexStride = stride;
                }
            }
        }

        return reflectInfo;
    }

    Ref<umbra::DXCInstance> Shader::GetDXCInstance()
    {
        if (!s_DXCInstance)
        {
            s_DXCInstance = umbra::ShaderCompiler::CreateDXCCompiler();
        }

        return s_DXCInstance;
    }

    ShaderMap &Shader::GetShaderCache()
    {
        return s_ShaderCache;
    }

    Ref<Shader> Shader::Create(const std::filesystem::path &filepath, UMBRA_ShaderType shaderType, bool recompile, const char *entryName)
    {
        // Try to get cached Shader Object
        ShaderKey key = { filepath.filename().string(), entryName, shaderType };
        auto shaderIt = s_ShaderCache.find(key);
        if (shaderIt != s_ShaderCache.end())
            return shaderIt->second;

        Ref<Shader> returnShader = CreateRef<Shader>(filepath, shaderType, recompile, entryName);
        if (!returnShader->IsValid())
            return nullptr;

        // Cache the shader object
        s_ShaderCache[key] = returnShader;
        return returnShader;
    }
}
