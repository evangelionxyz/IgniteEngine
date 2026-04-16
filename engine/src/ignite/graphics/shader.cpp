/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "shader.hpp"

#include "renderer.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"

#include <fstream>
#include <iterator>
#include <string>

#ifdef PLATFORM_WINDOWS
    #include <dxcapi.h>
    #include <d3d12shader.h>
    #include <wrl/client.h>
using Microsoft::WRL::ComPtr;

    #ifndef DXC_PART_DXIL
        #define DXC_PART_DXIL (('D') | ('X' << 8) | ('I' << 16) | ('L' << 24))
    #endif
    #ifndef DXC_PART_DXBC
        #define DXC_PART_DXBC (('D') | ('X' << 8) | ('B' << 16) | ('C' << 24))
    #endif
#endif

#ifndef SHADER_REFLECT_VERBOSE
// #define SHADER_REFLECT_VERBOSE
#endif

namespace ignite
{
    
    std::string GetShaderCacheDirectory()
    {
        return "resources/shaders/bin/";
    }

    void CreateShaderCachedDirectoryIfNeeded()
    {
        static std::string cachedDirectory = GetShaderCacheDirectory();
        if (!std::filesystem::exists(cachedDirectory))
            std::filesystem::create_directories(cachedDirectory);
    }

    const char* GetShaderTypeString(ShaderType type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return "Vertex";
            case ShaderType::Pixel: return "Pixel";
            case ShaderType::Geometry: return "Geometry";
            case ShaderType::Compute: return "Compute";
        }

        LOG_ASSERT(false, "Invalid shader type");
        return "Invalid shader type";
    }

    static const char *GetShaderExtension(nvrhi::GraphicsAPI api)
    {
        switch (api)
        {
            case nvrhi::GraphicsAPI::D3D12: return ".dxil";
            case nvrhi::GraphicsAPI::VULKAN: return ".spirv";
        }

        LOG_ASSERT(false, "Invalid Graphics API");
        return "Invalid Graphics API";
    }


    Shader::Shader(const std::filesystem::path &filepath, ShaderType type, bool recompile)
        : m_Type(type)
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        const nvrhi::GraphicsAPI api = device->getGraphicsAPI();

        CreateShaderCachedDirectoryIfNeeded();

        std::vector<uint8_t> shaderCode = CompileOrGetShader(filepath, type, recompile);
        LOG_ASSERT(shaderCode.data(), "[Shader] Blob data is not valid {}", filepath);

        nvrhi::ShaderDesc shaderDesc;
        shaderDesc.shaderType = GetNVRHIShaderType(type);

        m_Handle = device->createShader(shaderDesc, shaderCode.data(), shaderCode.size());
        LOG_ASSERT(m_Handle, "Failed to create {} shader: {}", GetShaderTypeString(type), filepath.generic_string());

        if (!shaderCode.empty())
        {
            if (api == nvrhi::GraphicsAPI::VULKAN)
            {
                SPIRVReflect(type, shaderCode, m_VertexAttributes);
            }
            else if (api == nvrhi::GraphicsAPI::D3D12)
            {
                DXILReflect(type, shaderCode, m_VertexAttributes);
            }
        }
    }

    std::vector<uint8_t> Shader::CompileOrGetShader(const std::filesystem::path &filepath, ShaderType type, bool recompile)
    {
        LOG_WARN("{}", std::filesystem::current_path().string());
        LOG_ASSERT(std::filesystem::exists(filepath), "[Shader] File does not exists! '{}'", filepath.generic_string().c_str());
        
        const nvrhi::GraphicsAPI api = DeviceManager::GetInstance()->GetGraphicsAPI();

        CompilerOptions opt = {};
        opt.filepath = filepath;
        opt.outputFilepath = filepath.parent_path() / "bin";
        opt.platformType = api == nvrhi::GraphicsAPI::D3D12 ? ShaderPlatformType::DXIL : ShaderPlatformType::SPIRV;
        opt.compilerType = CompilerType::DXC;
        opt.shaderDesc.shaderType = type;
        
        // Important!!!!
        if (api == nvrhi::GraphicsAPI::VULKAN)
        {
            opt.defines = { "SPIRV", "TARGET_VULKAN" };
        }

        std::vector<uint8_t> shaderCode;
        std::filesystem::path cacheFilepath = opt.outputFilepath / filepath.filename().replace_extension(GetShaderExtension(api));
        if (std::filesystem::exists(cacheFilepath) && !recompile)
        {
            std::ifstream file(cacheFilepath, std::ios::binary);
            file.seekg(0, std::ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            shaderCode.resize(fileSize);
            file.read(reinterpret_cast<char*>(shaderCode.data()), fileSize);
        }
        else
        {
            shaderCode = ShaderCompiler::CompileDXC(Renderer::GetDXCInstance(), opt);
        }

        return shaderCode;
    }

    void Shader::SPIRVReflect(ShaderType type, const std::vector<uint8_t> &shaderCode, std::vector<nvrhi::VertexAttributeDesc> &vertexAttributes)
    {
        if (shaderCode.size() % sizeof(uint32_t) != 0)
        {
            throw std::runtime_error("Shader blob size is not aligned to 4 bytes");
        }

        const uint32_t *ptr = reinterpret_cast<const uint32_t *>(shaderCode.data());
        size_t wordCount = shaderCode.size() / sizeof(uint32_t);
        std::vector<uint32_t> dataBlob(ptr, ptr + wordCount);

        spirv_cross::Compiler compiler(dataBlob);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

#if defined(SHADER_REFLECT_VERBOSE)
        LOG_WARN("[Shader Reflect] {} Shader", GetShaderTypeString(type));

        // --- Uniform Buffers ---
        LOG_TRACE("   {} uniform buffers", resources.uniform_buffers.size());
        for (const auto &ubo : resources.uniform_buffers)
        {
            const auto &type = compiler.get_type(ubo.base_type_id);
            uint32_t size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
            uint32_t binding = compiler.get_decoration(ubo.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(ubo.id, spv::DecorationDescriptorSet);
            size_t memberCount = type.member_types.size();

            LOG_TRACE("  [UBO] Name: {}, Set: {}, Binding: {}, Size: {}, Members: {}", ubo.name, set, binding, size, memberCount);
        }

        // --- Sampled Images (combined or separate textures) ---
        LOG_TRACE("   {} sampled images", resources.sampled_images.size());
        for (const auto &image : resources.sampled_images)
        {
            uint32_t binding = compiler.get_decoration(image.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);

            LOG_TRACE("  [Texture] Name: {}, Set: {}, Binding: {}", image.name, set, binding);
        }

        // --- Separate Samplers ---
        LOG_TRACE("   {} separate samplers", resources.separate_samplers.size());
        for (const auto &sampler : resources.separate_samplers)
        {
            uint32_t binding = compiler.get_decoration(sampler.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(sampler.id, spv::DecorationDescriptorSet);

            LOG_TRACE("  [Sampler] Name: {}, Set: {}, Binding: {}", sampler.name, set, binding);
        }

        // --- Separate Images (non-combined, i.e., texture2D) ---
        LOG_TRACE("   {} separate images", resources.separate_images.size());
        for (const auto &image : resources.separate_images)
        {
            uint32_t binding = compiler.get_decoration(image.id, spv::DecorationBinding);
            uint32_t set = compiler.get_decoration(image.id, spv::DecorationDescriptorSet);

            LOG_TRACE("  [Separate Image] Name: {}, Set: {}, Binding: {}", image.name, set, binding);
        }

        // --- Push Constants ---
        LOG_TRACE("   {} push constants", resources.push_constant_buffers.size());
        for (const auto &pcb : resources.push_constant_buffers)
        {
            const auto &type = compiler.get_type(pcb.base_type_id);
            uint32_t size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));

            LOG_TRACE("  [PushConstant] Name: {}, Size: {}", pcb.name, size);
        }

#endif
		// Vertex inputs (only for vertex shaders)
        if (type == ShaderType::Vertex)
        {
            vertexAttributes.clear();
            
            // Sort inputs by location to compute offsets consistently
            struct InAttribute
            {
                uint32_t location; 
                spirv_cross::ID id;
            };

            std::vector<InAttribute> inputs;
            inputs.reserve(resources.stage_inputs.size());
            for (const auto& in : resources.stage_inputs)
            {
				uint32_t location = compiler.get_decoration(in.id, spv::DecorationLocation);
				inputs.push_back({ location, in.id });
            }
            
            std::sort(inputs.begin(), inputs.end(), [](const InAttribute& a, const InAttribute& b) {
                return a.location < b.location;
			});

            uint32_t offset = 0;
            for (const auto& it : inputs)
            {
				const spirv_cross::SPIRType &type = compiler.get_type(compiler.get_type_from_variable(it.id).self);
				nvrhi::Format format = MapSPIRVTypeToNVRHIFormat(type);
                if (format == nvrhi::Format::UNKNOWN)
                {
                    LOG_WARN("  [Vertex Attribute] Unsupported format for input at location {}", it.location);
                    continue;
				}

				const uint32_t componentSize = 4; // 32-bit float assumed
				const uint32_t elementCount = std::max(type.vecsize, 1u);
				const uint32_t attributeSize = componentSize * elementCount;

                nvrhi::VertexAttributeDesc attr;
                attr.name = compiler.get_name(it.id);
                attr.format = format;
                attr.offset = offset;
                attr.bufferIndex = 0; // Assuming single vertex buffer for simplicity
                attr.isInstanced = false;
                // attr.elementStride; // calculated later

				offset += attributeSize;
                vertexAttributes.push_back(attr);

				// LOG_TRACE("  [Vertex Attribute] Name: {}, Location: {}, Format: {}, Offset: {}",  attr.name, it.location, static_cast<uint32_t>(attr.format), attr.offset);
            }

			const uint32_t stride = offset;
            for (auto& attr : vertexAttributes)
            {
                attr.elementStride = stride;
			}
        }
    }

    void Shader::DXILReflect(ShaderType type, const std::vector<uint8_t>& shaderCode, std::vector<nvrhi::VertexAttributeDesc> &vertexAttributes)
    {
#ifdef PLATFORM_WINDOWS
        // Validate the blob first
        if (shaderCode.empty() || shaderCode.size() < 4)
        {
            LOG_ERROR("[Shader Reflect] Invalid blob: empty or too small (size: {})", shaderCode.size());
            return;
        }

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

        LOG_TRACE("[Shader Reflect] Attempting to reflect {} shader blob of size {} bytes", 
            GetShaderTypeString(type), shaderCode.size());

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

    Ref<Shader> Shader::Create(const std::filesystem::path &filepath, ShaderType type, bool recompile)
    {
        Ref<Shader> returnShader = CreateRef<Shader>(filepath, type, recompile);
        if (returnShader->GetHandle() == nullptr)
        {
            return nullptr;
        }

        return returnShader;
    }
}
