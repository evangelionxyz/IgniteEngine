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

#ifndef SHADER_HPP
#define SHADER_HPP

#include "ignite/core/types.hpp"
#include "ignite/core/logger.hpp"

#include "shader_compiler.hpp"

#include <nvrhi/nvrhi.h>
#include <initializer_list>
#include <filesystem>
#include <string>

namespace ignite
{
    static std::string GetShaderCacheDirectory();
    static void CreateShaderCachedDirectoryIfNeeded();

    static nvrhi::ShaderType GetNVRHIShaderType(ShaderType type)
    {
        switch (type)
        {
            case ShaderType::Vertex: return nvrhi::ShaderType::Vertex;
            case ShaderType::Pixel: return nvrhi::ShaderType::Pixel;
            case ShaderType::Geometry: return nvrhi::ShaderType::Geometry;
            case ShaderType::Compute: return nvrhi::ShaderType::Compute;
        }

        LOG_ASSERT(false, "Invalid shader stage");
        return nvrhi::ShaderType::None;
    }

    static ShaderType GetShaderType(nvrhi::ShaderType type)
    {
        switch (type)
        {
            case nvrhi::ShaderType::Vertex: return ShaderType::Vertex;
            case nvrhi::ShaderType::Pixel: return ShaderType::Pixel;
            case nvrhi::ShaderType::Geometry: return ShaderType::Geometry;
            case nvrhi::ShaderType::Compute: return ShaderType::Compute;
        }

        LOG_ASSERT(false, "Invalid shader stage");
        return ShaderType::Vertex;
    }

    class Shader
    {
    public:
        Shader() = default;
        Shader(const std::filesystem::path &filepath, ShaderType type, bool recompile = false);

        nvrhi::ShaderHandle GetHandle() { return m_Handle; }
        ShaderType GetType() const { return m_Type; }

        static std::vector<uint8_t> CompileOrGetShader(const std::filesystem::path &filepath, ShaderType type, bool recompile);
        static Ref<Shader> Create(const std::filesystem::path &filepath, ShaderType type, bool recompile = false);

        static spirv_cross::ShaderResources SPIRVReflect(ShaderType type, const std::vector<uint8_t> &shaderCode);
        static void DXILReflect(ShaderType type, const std::vector<uint8_t>& shaderCode);
    
    private:
        ShaderType m_Type;
        nvrhi::ShaderHandle m_Handle = nullptr;
    };
}

#endif