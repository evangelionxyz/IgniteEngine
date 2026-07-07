// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "shader.hpp"
#include "renderer.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"

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
            if (!ignite::Path::exists(cachedDirectory))
                ignite::Path::create_directories(cachedDirectory);
        }

        static void ShaderDebugLog(UMBRA_LogType type, const char *message, void *userData)
        {
            switch (type)
            {
                default:
                case UMBRA_LOG_TYPE_INFO: LOG_TRACE("[Shader]\t{}", message); break;
                case UMBRA_LOG_TYPE_WARNING: LOG_WARN("[Shader]\t{}", message); break;
                case UMBRA_LOG_TYPE_ERROR: LOG_ERROR("[Shader]\t{}", message); break;
                case UMBRA_LOG_TYPE_CRITICAL: LOG_ASSERT(false, "[Shader]\t{}", message); break;
            }
        }

        static const char *GetShaderExtension(nvrhi::GraphicsAPI api)
        {
            switch (api)
            {
                case nvrhi::GraphicsAPI::D3D12: return ".dxil";
                case nvrhi::GraphicsAPI::VULKAN: return ".spirv";
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

    Shader::Shader(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile, const char *entryName)
        : m_Filepath(filepath), m_Type(shaderType)
    {
        CreateShaderCachedDirectoryIfNeeded();

        m_Name = filepath.stem().string();

        // Setup the shader desc
        m_ShaderDesc.shaderType = GetNVRHIShaderType(m_Type);
        m_ShaderDesc.entryName = entryName;

        // Compile shader source
        std::vector<uint8_t> shaderCode = CompileOrGetShader(m_Filepath, m_Type, recompile, m_ShaderDesc.entryName.c_str());
        LOG_ASSERT(shaderCode.data() && !shaderCode.empty(), "[Shader] Blob data is not valid {}", m_Filepath);

        // Create the handle and reflect to construct the vertex attributes
        if (shaderCode.data() && !shaderCode.empty())
        {
            nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
            m_Handle = device->createShader(m_ShaderDesc, shaderCode.data(), shaderCode.size());
            LOG_ASSERT(m_Handle, "[Shader] Failed to create {} shader: {}", UMBRA_GetShaderTypeString(m_Type), m_Filepath.generic_string());

            LOG_WARN("[Shader] Reflection for '{}': ", m_Filepath.generic_string());
            Reflect(m_Type, shaderCode, m_VertexAttributes);
        }
    }

    bool Shader::Recompile()
    {
        nvrhi::ShaderHandle newShaderHandle = nullptr;

        // Compile shader source
        std::vector<uint8_t> shaderCode = CompileOrGetShader(m_Filepath, m_Type, true, m_ShaderDesc.entryName.c_str());
        LOG_ASSERT(shaderCode.data() && !shaderCode.empty(), "[Shader] Blob data is not valid {}", m_Filepath);

        // Create the handle and reflect to construct the vertex attributes
        if (shaderCode.data() && !shaderCode.empty())
        {
            nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
            newShaderHandle = device->createShader(m_ShaderDesc, shaderCode.data(), shaderCode.size());
            LOG_ASSERT(newShaderHandle, "[Shader] Failed to create {} shader: {}", UMBRA_GetShaderTypeString(m_Type), m_Filepath.generic_string());

            // Only replace the Main Shader handle if the creation is successful.
            if (newShaderHandle)
            {
                m_Handle = newShaderHandle;
                LOG_WARN("[Shader] Reflection for '{}': ", m_Filepath.generic_string());
                m_ReflectionInfo = Reflect(m_Type, shaderCode, m_VertexAttributes);
            }
        }

        return newShaderHandle != nullptr;
    }

    std::vector<uint8_t> Shader::CompileOrGetShader(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile, const char *entryName)
    {
        LOG_ASSERT(ignite::Path::exists(filepath), "[Shader] File does not exists! '{}'", filepath.generic_string().c_str());
        
        const nvrhi::GraphicsAPI api = DeviceManager::GetInstance()->GetGraphicsAPI();

        umbra::CompilerOptions options = {};
        options.compilerType = UMBRA_SHADER_COMPILER_TYPE_DXC;
#ifdef PLATFORM_WINDOWS
        options.platformType = api == nvrhi::GraphicsAPI::D3D12 ? UMBRA_SHADER_PLATFORM_TYPE_DXIL : UMBRA_SHADER_PLATFORM_TYPE_SPIRV;
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
        options.tRegShift = 0;   // NVRHI Compatible
        options.sRegShift = 128; // NVRHI Compatible
        options.bRegShift = 256; // NVRHI Compatible
        options.uRegShift = 384; // NVRHI Compatible
        
        // Important!!!!
        if (api == nvrhi::GraphicsAPI::VULKAN)
        {
            options.defines = { "SPIRV", "TARGET_VULKAN" };
            options.AddCompilerOptions("-fvk-bind-resource-heap");
            options.AddCompilerOptions("0"); // binding index
            options.AddCompilerOptions("2"); // set index (space2)
        }

        std::vector<uint8_t> shaderCode;
        ignite::Path cacheFilepath = (options.outputFilepath / options.filepath.filename().replace_extension(GetShaderExtension(api))).generic_string();
        if (ignite::Path::exists(cacheFilepath) && !recompile)
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

    umbra::ShaderReflectionInfo Shader::Reflect(UMBRA_ShaderType shaderType, const std::vector<uint8_t> &shaderCode, std::vector<nvrhi::VertexAttributeDesc> &outVertexAttributes)
    {
        if (shaderCode.size() % sizeof(uint32_t) != 0)
        {
            throw std::runtime_error("Shader blob size is not aligned to 4 bytes");
        }

        umbra::ShaderReflectionInfo reflectInfo;

#ifdef PLATFORM_WINDOWS
        const nvrhi::GraphicsAPI api = DeviceManager::GetInstance()->GetGraphicsAPI();
        if (api == nvrhi::GraphicsAPI::D3D12)
        {
            reflectInfo = umbra::ShaderReflection::DXILReflect(shaderType, shaderCode);
        }
        else if (api == nvrhi::GraphicsAPI::VULKAN)
#endif
        {
            reflectInfo = umbra::ShaderReflection::SPIRVReflect(shaderType, shaderCode);
        }

        // Vertex inputs (only for vertex shaders)
        if (shaderType == UMBRA_SHADER_TYPE_VERTEX)
        {
            outVertexAttributes.clear();
            outVertexAttributes.reserve(reflectInfo.vertexAttributes.size());

            for (const auto &vertexAttr : reflectInfo.vertexAttributes)
            {
                // Skip system-value semantics (SV_VertexID, SV_InstanceID, etc.).
                // These are supplied by the GPU and never read from a vertex buffer.
                // Including them would create an input-layout element on slot 0,
                // which forces D3D12 to require a bound vertex buffer even when the
                // shader is purely procedural (e.g. infinite_grid.vertex.hlsl).
                const bool isSysValue = vertexAttr.name.size() >= 3 &&
                    vertexAttr.name[0] == 'S' &&
                    vertexAttr.name[1] == 'V' &&
                    vertexAttr.name[2] == '_';
                if (isSysValue)
                {
                    continue;
                }

                nvrhi::VertexAttributeDesc attr;

                attr.name = vertexAttr.name;
                attr.format = MapUmbraTypeToNVRHIFormat(vertexAttr.format);
                attr.offset = vertexAttr.offset;
                attr.bufferIndex = vertexAttr.bufferIndex;
                attr.isInstanced = false;
                attr.elementStride = vertexAttr.elementStride;
                
                outVertexAttributes.push_back(attr);
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

#if 0
    void Shader::DXILReflect(UMBRA_ShaderType shaderType, const std::vector<uint8_t>& shaderCode, std::vector<nvrhi::VertexAttributeDesc> &vertexAttributes)
    {
#ifdef PLATFORM_WINDOWS
        // Validate the blob first
        if (shaderCode.empty() || shaderCode.size() < 4)
        {
            LOG_ERROR("[Shader Reflect] Invalid blob: empty or too small (size: {})", shaderCode.size());
            return;
        }

        const umbra::ShaderReflectionInfo reflectInfo = umbra::ShaderReflection::DXILReflect(shaderType, shaderCode);

        // Check for DXIL signature - DXIL files should start with "DXBC" header or have specific DXIL markers
        const uint32_t* header = reinterpret_cast<const uint32_t*>(shaderCode.data());
        if (shaderCode.size() >= 4)
        {
            // Check for DXBC signature (0x43425844 = "DXBC" in little endian)
            if (header[0] != 0x43425844)
            {
                LOG_WARN("[Shader Reflect] Blob doesn't appear to be DXBC/DXIL format. First 4 bytes: 0x{:08X}", header[0]);
                
                // Some shader compilers might wrap the DXIL in different containers
                // Try to find DXBC signature within the first few bytes
                bool foundDXBC = false;
                for (size_t offset = 0; offset < std::min(shaderCode.size() - 4, size_t(64)); offset += 4)
                {
                    const uint32_t* searchHeader = reinterpret_cast<const uint32_t*>(shaderCode.data() + offset);
                    if (*searchHeader == 0x43425844)
                    {
                        LOG_INFO("[Shader Reflect] Found DXBC signature at offset {}", offset);
                        foundDXBC = true;
                        break;
                    }
                }
                
                if (!foundDXBC)
                {
                    LOG_ERROR("[Shader Reflect] No DXBC signature found in blob. This might not be a valid DXIL shader.");
                    // Still continue - let D3DReflect decide
                }
            }
        }

        LOG_TRACE("[Shader Reflect] Attempting to reflect {} shader blob of size {} bytes", GetShaderTypeString(type), shaderCode.size());

        ComPtr<ID3D12ShaderReflection> reflection;
        HRESULT result = E_FAIL;

        ComPtr<IDxcUtils> utils;
        ComPtr<IDxcContainerReflection> containerReflection;
        ComPtr<IDxcBlobEncoding> blob;
        result = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf()));
        if (SUCCEEDED(result))
        {
            result = utils->CreateBlob(shaderCode.data(), static_cast<UINT32>(shaderCode.size()), CP_ACP, &blob);
        }
        if (SUCCEEDED(result))
        {
            result = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(containerReflection.GetAddressOf()));
        }
        if (SUCCEEDED(result))
        {
            result = containerReflection->Load(blob.Get());
        }
        if (SUCCEEDED(result))
        {
            UINT32 partIndex = 0;
            if (SUCCEEDED(containerReflection->FindFirstPartKind(DXC_PART_DXIL, &partIndex)))
            {
                result = containerReflection->GetPartReflection(partIndex, IID_PPV_ARGS(reflection.GetAddressOf()));
            }
            else
            {
                result = E_FAIL;
            }
        }

        if (FAILED(result) || !reflection)
        {
            result = D3DReflect(shaderCode.data(), shaderCode.size(), IID_PPV_ARGS(reflection.GetAddressOf()));
        }
        
        if (FAILED(result))
        {
            LOG_ERROR("[Shader Reflect] D3DReflect failed with HRESULT: 0x{:08X}", static_cast<uint32_t>(result));
            
            switch (result)
            {
                case E_INVALIDARG:
                    LOG_ERROR("  - E_INVALIDARG: One or more arguments are invalid");
                    LOG_ERROR("  - This usually means the blob is not valid DXIL/DXBC bytecode");
                    LOG_ERROR("  - Blob size: {} bytes", shaderCode.size());
                    LOG_ERROR("  - First 16 bytes: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
                        shaderCode.size() > 0 ? shaderCode[0] : 0, shaderCode.size() > 1 ? shaderCode[1] : 0,
                        shaderCode.size() > 2 ? shaderCode[2] : 0, shaderCode.size() > 3 ? shaderCode[3] : 0,
                        shaderCode.size() > 4 ? shaderCode[4] : 0, shaderCode.size() > 5 ? shaderCode[5] : 0,
                        shaderCode.size() > 6 ? shaderCode[6] : 0, shaderCode.size() > 7 ? shaderCode[7] : 0,
                        shaderCode.size() > 8 ? shaderCode[8] : 0, shaderCode.size() > 9 ? shaderCode[9] : 0,
                        shaderCode.size() > 10 ? shaderCode[10] : 0, shaderCode.size() > 11 ? shaderCode[11] : 0,
                        shaderCode.size() > 12 ? shaderCode[12] : 0, shaderCode.size() > 13 ? shaderCode[13] : 0,
                        shaderCode.size() > 14 ? shaderCode[14] : 0, shaderCode.size() > 15 ? shaderCode[15] : 0);
                    break;
                case E_OUTOFMEMORY:
                    LOG_ERROR("  - E_OUTOFMEMORY: Out of memory");
                    break;
                case E_FAIL:
                    LOG_ERROR("  - E_FAIL: General failure");
                    break;
                default:
                    LOG_ERROR("  - Unknown error code: 0x{:08X}", static_cast<uint32_t>(result));
                    break;
            }
            
            LOG_ERROR("[Shader Reflect] Possible causes:");
            LOG_ERROR("  1. ShaderMake might be producing a different format than DXIL");
            LOG_ERROR("  2. The shader compilation might have failed silently");
            LOG_ERROR("  3. The blob might be wrapped in an additional container");
            LOG_ERROR("  4. Try checking if the shader files compile successfully first");
            return;
        }

        if (!reflection)
        {
            LOG_ERROR("[Shader Reflect] D3DReflect succeeded but returned null reflection interface");
            return;
        }

        D3D12_SHADER_DESC shaderDesc;
        result = reflection->GetDesc(&shaderDesc);
        if (FAILED(result))
        {
            LOG_ERROR("[Shader Reflect] Failed to get shader description. HRESULT: 0x{:08X}", static_cast<uint32_t>(result));
            return;
        }

        LOG_WARN("[Shader Reflect] {} Shader", GetShaderTypeString(type));
        
        // Extract version info manually - D3D12 uses different version encoding
        UINT majorVersion = (shaderDesc.Version >> 4) & 0xF;
        UINT minorVersion = shaderDesc.Version & 0xF;
        LOG_TRACE("   Shader version: {}.{}", majorVersion, minorVersion);
        LOG_TRACE("   Creator: {}", shaderDesc.Creator ? shaderDesc.Creator : "unknown");

        // --- Constant Buffers (Uniform Buffers) ---
        LOG_TRACE("   {} constant buffers", shaderDesc.ConstantBuffers);
        for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
        {
            ID3D12ShaderReflectionConstantBuffer* cbuffer = reflection->GetConstantBufferByIndex(i);
            if (cbuffer)
            {
                D3D12_SHADER_BUFFER_DESC cbufferDesc;
                result = cbuffer->GetDesc(&cbufferDesc);
                if (SUCCEEDED(result))
                {
                    LOG_TRACE("  [CBV] Name: {}, Size: {}, Variables: {}", 
                        cbufferDesc.Name ? cbufferDesc.Name : "unnamed", 
                        cbufferDesc.Size, 
                        cbufferDesc.Variables);
                }
            }
        }

        // --- Bound Resources (Textures, Samplers, UAVs, etc.) ---
        LOG_TRACE("   {} bound resources", shaderDesc.BoundResources);
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            result = reflection->GetResourceBindingDesc(i, &bindDesc);
            if (SUCCEEDED(result))
            {
                const char* resourceType = "Unknown";
                switch (bindDesc.Type)
                {
                    case D3D_SIT_CBUFFER:
                        resourceType = "CBV (Constant Buffer)";
                        break;
                    case D3D_SIT_TBUFFER:
                        resourceType = "TBuffer";
                        break;
                    case D3D_SIT_TEXTURE:
                        resourceType = "SRV (Texture)";
                        break;
                    case D3D_SIT_SAMPLER:
                        resourceType = "Sampler";
                        break;
                    case D3D_SIT_UAV_RWTYPED:
                        resourceType = "UAV (RW Texture)";
                        break;
                    case D3D_SIT_STRUCTURED:
                        resourceType = "SRV (Structured Buffer)";
                        break;
                    case D3D_SIT_UAV_RWSTRUCTURED:
                        resourceType = "UAV (RW Structured Buffer)";
                        break;
                    case D3D_SIT_BYTEADDRESS:
                        resourceType = "SRV (Byte Address Buffer)";
                        break;
                    case D3D_SIT_UAV_RWBYTEADDRESS:
                        resourceType = "UAV (RW Byte Address Buffer)";
                        break;
                    case D3D_SIT_UAV_APPEND_STRUCTURED:
                        resourceType = "UAV (Append Structured Buffer)";
                        break;
                    case D3D_SIT_UAV_CONSUME_STRUCTURED:
                        resourceType = "UAV (Consume Structured Buffer)";
                        break;
                    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                        resourceType = "UAV (RW Structured Buffer with Counter)";
                        break;
                    case D3D_SIT_RTACCELERATIONSTRUCTURE:
                        resourceType = "SRV (Ray Tracing Acceleration Structure)";
                        break;
                    case D3D_SIT_UAV_FEEDBACKTEXTURE:
                        resourceType = "UAV (Feedback Texture)";
                        break;
                }

                LOG_TRACE("  [{}] Name: {}, Register: {}, Space: {}, Count: {}", 
                    resourceType,
                    bindDesc.Name ? bindDesc.Name : "unnamed",
                    bindDesc.BindPoint,
                    bindDesc.Space,
                    bindDesc.BindCount);
            }
        }

        // --- Input Parameters (Vertex Attributes, etc.) ---
        LOG_TRACE("   {} input parameters", shaderDesc.InputParameters);
        struct InputAttribute
        {
            UINT registerIndex;
            std::string semanticName;
            UINT semanticIndex;
            UINT mask;
            D3D_REGISTER_COMPONENT_TYPE componentType;
        };

        std::vector<InputAttribute> inputs;
        if (type == ShaderType::Vertex)
        {
            inputs.reserve(shaderDesc.InputParameters);
        }

        for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
        {
            D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
            result = reflection->GetInputParameterDesc(i, &paramDesc);
            if (SUCCEEDED(result))
            {
                LOG_TRACE("  [Input] Semantic: {}{}, Register: {}, Mask: 0x{:X}", 
                    paramDesc.SemanticName ? paramDesc.SemanticName : "unknown",
                    paramDesc.SemanticIndex,
                    paramDesc.Register,
                    paramDesc.Mask);

                if (type == ShaderType::Vertex)
                {
                    std::string semantic = paramDesc.SemanticName ? paramDesc.SemanticName : "unknown";
                    inputs.push_back({ paramDesc.Register, std::move(semantic), paramDesc.SemanticIndex, paramDesc.Mask, paramDesc.ComponentType });
                }
            }
        }

        if (type == ShaderType::Vertex && !inputs.empty())
        {
            std::sort(inputs.begin(), inputs.end(), [](const InputAttribute& a, const InputAttribute& b) {
                return a.registerIndex < b.registerIndex;
            });

            auto countMask = [](UINT mask)
            {
                uint32_t count = 0;
                while (mask != 0)
                {
                    count += (mask & 0x1) ? 1u : 0u;
                    mask >>= 1;
                }
                return std::max(count, 1u);
            };

            auto mapFormat = [](D3D_REGISTER_COMPONENT_TYPE componentType, uint32_t elementCount)
            {
                switch (componentType)
                {
                    case D3D_REGISTER_COMPONENT_FLOAT32:
                        switch (elementCount)
                        {
                            case 1: return nvrhi::Format::R32_FLOAT;
                            case 2: return nvrhi::Format::RG32_FLOAT;
                            case 3: return nvrhi::Format::RGB32_FLOAT;
                            case 4: return nvrhi::Format::RGBA32_FLOAT;
                            default: break;
                        }
                        break;
                    case D3D_REGISTER_COMPONENT_SINT32:
                        switch (elementCount)
                        {
                            case 1: return nvrhi::Format::R32_SINT;
                            case 2: return nvrhi::Format::RG32_SINT;
                            case 3: return nvrhi::Format::RGB32_SINT;
                            case 4: return nvrhi::Format::RGBA32_SINT;
                            default: break;
                        }
                        break;
                    case D3D_REGISTER_COMPONENT_UINT32:
                        switch (elementCount)
                        {
                            case 1: return nvrhi::Format::R32_UINT;
                            case 2: return nvrhi::Format::RG32_UINT;
                            case 3: return nvrhi::Format::RGB32_UINT;
                            case 4: return nvrhi::Format::RGBA32_UINT;
                            default: break;
                        }
                        break;
                    default:
                        break;
                }

                return nvrhi::Format::UNKNOWN;
            };

            vertexAttributes.clear();

            uint32_t offset = 0;
            for (const auto& input : inputs)
            {
                const uint32_t elementCount = countMask(input.mask);
                const nvrhi::Format format = mapFormat(input.componentType, elementCount);
                if (format == nvrhi::Format::UNKNOWN)
                {
                    LOG_WARN("  [Vertex Attribute] Unsupported format for input {}{} (mask: 0x{:X})", input.semanticName, input.semanticIndex, input.mask);
                    continue;
                }

                std::string name = input.semanticName;
                if (input.semanticIndex > 0)
                {
                    name += std::to_string(input.semanticIndex);
                }

                nvrhi::VertexAttributeDesc attr;
                attr.name = name;
                attr.format = format;
                attr.offset = offset;
                attr.bufferIndex = 0;
                attr.isInstanced = false;

                const uint32_t componentSize = 4;
                const uint32_t attributeSize = componentSize * elementCount;
                offset += attributeSize;
                vertexAttributes.push_back(attr);
            }

            const uint32_t stride = offset;
            for (auto& attr : vertexAttributes)
            {
                attr.elementStride = stride;
            }
        }

        // --- Output Parameters ---
        LOG_TRACE("   {} output parameters", shaderDesc.OutputParameters);
        for (UINT i = 0; i < shaderDesc.OutputParameters; ++i)
        {
            D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
            result = reflection->GetOutputParameterDesc(i, &paramDesc);
            if (SUCCEEDED(result))
            {
                LOG_TRACE("  [Output] Semantic: {}{}, Register: {}, Mask: 0x{:X}", 
                    paramDesc.SemanticName ? paramDesc.SemanticName : "unknown",
                    paramDesc.SemanticIndex,
                    paramDesc.Register,
                    paramDesc.Mask);
            }
        }

        // Note: D3D12 doesn't have direct equivalent to Vulkan's push constants
        // Push constants in D3D12 are typically implemented as root constants
        // which can be detected by looking for constant buffers with special properties
        // or by examining the root signature (not available through shader reflection alone)
        
        LOG_TRACE("   Root constants would need root signature analysis (not available in shader reflection)");
#else
        LOG_WARN("[Shader Reflect] DXIL reflection is only available on Windows platform");
#endif

    }
#endif

    Ref<Shader> Shader::Create(const ignite::Path &filepath, UMBRA_ShaderType shaderType, bool recompile, const char *entryName)
    {
        // Try to get cached Shader Object
        ShaderKey key = { filepath.filename(), entryName, shaderType };
        auto shaderIt = s_ShaderCache.find(key);
        if (shaderIt != s_ShaderCache.end())
            return shaderIt->second;

        Ref<Shader> returnShader = CreateRef<Shader>(filepath, shaderType, recompile, entryName);
        if (returnShader->GetHandle() == nullptr)
            return nullptr;

        // Cache the shader object
        s_ShaderCache[key] = returnShader;
        return returnShader;
    }
}
