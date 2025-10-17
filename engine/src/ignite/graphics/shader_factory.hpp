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

#pragma once

#include "ignite/core/types.hpp"
#include "ignite/core/vfs/vfs.hpp"

#include <nvrhi/nvrhi.h>
#include <memory>

#include <functional>
#include <filesystem>

namespace ignite
{
    static std::string GetShaderFolder(nvrhi::GraphicsAPI api)
    {
        if (api == nvrhi::GraphicsAPI::D3D12)
        {
            return "/shaders/dxil";
        }
        else if (api == nvrhi::GraphicsAPI::VULKAN)
        {
            return "/shaders/spirv";
        }
        assert(false);
        return "Unsupported graphics API";
    }

    static std::string GetShaderExtension(nvrhi::GraphicsAPI api)
    {
        if (api == nvrhi::GraphicsAPI::D3D12)
        {
            return ".dxil";
        }
        else if (api == nvrhi::GraphicsAPI::VULKAN)
        {
            return ".spirv";
        }
        assert(false);
        return "Unsupported graphics API";
    }

    struct ShaderMacro
    {
        std::string name;
        std::string definition;

        ShaderMacro(const std::string &_name, const std::string &_definition)
            : name(_name), definition(_definition)
        {
        }
    };

    struct StaticShader
    {
        void const *pByteCode = nullptr;
        size_t size = 0;
    };

#if IGNITE_WITH_DX12 && IGNITE_WITH_STATIC_SHADERS
#define IGNITE_MAKE_DXIL_SHADER(symbol) StaticShader{symbol,sizeof(symbol)}
#else
#define IGNITE_MAKE_DXIL_SHADER(symbol) StaticShader()
#endif

#if IGNITE_WITH_VULKAN && IGNITE_WITH_STATIC_SHADERS
#define IGNITE_MAKE_SPIRV_SHADER(symbol) StaticShader{symbol,sizeof(symbol)}
#else
#define IGNITE_MAKE_SPIRV_SHADER(symbol) StaticShader()
#endif

    // Macro to use with ShaderFactory::CreateStaticPlatformShader.
    // If there are symbols g_MyShader_dxbc, g_MyShader_dxil, g_MyShader_spirv - just use:
    //      CreateStaticPlatformShader(IGNITE_MAKE_PLATFORM_SHADER(g_MyShader), defines, shaderDesc);
    // and all available platforms will be resolved automatically.
#define IGNITE_MAKE_PLATFORM_SHADER(basename) IGNITE_MAKE_DXIL_SHADER(basename##_dxil), IGNITE_MAKE_SPIRV_SHADER(basename##_spirv)

    // Similar to IGNITE_MAKE_PLATFORM_SHADER but for libraries - they are not available on DX11/DXBC.
    //      CreateStaticPlatformShaderLibrary(IGNITE_MAKE_PLATFORM_SHADER_LIBRARY(g_MyShaderLibrary), defines);
#define IGNITE_MAKE_PLATFORM_SHADER_LIBRARY(basename) IGNITE_MAKE_DXIL_SHADER(basename##_dxil), IGNITE_MAKE_SPIRV_SHADER(basename##_spirv)

    class ShaderFactory
    {
    public:
        ShaderFactory(nvrhi::DeviceHandle device, Ref<vfs::IFileSystem> fs, const std::filesystem::path &basePath);

        virtual ~ShaderFactory();
        void ClearCache();

        Ref<vfs::IBlob> GetByteCode(const char *filename, const char *entryName);
        nvrhi::ShaderHandle CreateShader(const char *filename, const char *entryName, const std::vector<ShaderMacro> *pDefines, const nvrhi::ShaderDesc &desc);
        nvrhi::ShaderLibraryHandle CreateShaderLibrary(const char *filename, const std::vector<ShaderMacro> *pDefines);
        nvrhi::ShaderHandle CreateStaticShader(StaticShader shader, const std::vector<ShaderMacro> *pDefines, const nvrhi::ShaderDesc &desc);
        nvrhi::ShaderHandle CreateStaticPlatformShader(StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro> *pDefines, const nvrhi::ShaderDesc &desc);
        nvrhi::ShaderLibraryHandle CreateStaticShaderLibrary(StaticShader shader, const std::vector<ShaderMacro> *pDefines);
        nvrhi::ShaderLibraryHandle CreateStaticPlatformShaderLibrary(StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro> *pDefines);
        nvrhi::ShaderHandle CreateAutoShader(const char *filename, const char *entryName, StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro> *pDefines, const nvrhi::ShaderDesc &desc);
        nvrhi::ShaderLibraryHandle CreateAutoShaderLibrary(const char *filename, StaticShader dxil, StaticShader spirv, const std::vector<ShaderMacro> *pDefines);
        std::pair<const void *, size_t> FindShaderFromHash(u64 hash, std::function<u64(std::pair<const void *, size_t>, nvrhi::GraphicsAPI)> hashGenerator);
    private:
        nvrhi::DeviceHandle m_Device;
        std::unordered_map<std::string, Ref<vfs::IBlob>> m_ByteCodeCache;
        Ref<vfs::IFileSystem> m_FS;
        std::filesystem::path m_BasePath;
    };
}
