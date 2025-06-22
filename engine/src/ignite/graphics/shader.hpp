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
#include "ignite/core/logger.hpp"

#include <filesystem>
#include <string>
#include <vector>
#include <ShaderMake/ShaderMake.h>
#include <fstream>
#include <nvrhi/nvrhi.h>

#include <initializer_list>

namespace ignite
{
    static std::string GetShaderCacheDirectory();
    static void CreateShaderCachedDirectoryIfNeeded();

    static nvrhi::ShaderType GetNVRHIShaderType(ShaderMake::ShaderType type)
    {
        switch (type)
        {
        case ShaderMake::ShaderType::Vertex: return nvrhi::ShaderType::Vertex;
        case ShaderMake::ShaderType::Pixel: return nvrhi::ShaderType::Pixel;
        case ShaderMake::ShaderType::Geometry: return nvrhi::ShaderType::Geometry;
        case ShaderMake::ShaderType::Compute: return nvrhi::ShaderType::Compute;
        }

        LOG_ASSERT(false, "Invalid shader stage");
        return nvrhi::ShaderType::None;
    }

    static ShaderMake::ShaderType GetShaderMakeShaderType(nvrhi::ShaderType type)
    {
        switch (type)
        {
        case nvrhi::ShaderType::Vertex: return ShaderMake::ShaderType::Vertex;
        case nvrhi::ShaderType::Pixel: return ShaderMake::ShaderType::Pixel;
        case nvrhi::ShaderType::Geometry: return ShaderMake::ShaderType::Geometry;
        case nvrhi::ShaderType::Compute: return ShaderMake::ShaderType::Compute;
        }

        LOG_ASSERT(false, "Invalid shader stage");
        return ShaderMake::ShaderType::Vertex;
    }

    class Shader
    {
    public:
        Shader() = default;
        Shader(const std::filesystem::path &filepath, ShaderMake::ShaderType type, bool recompile = false);

        static ShaderMake::ShaderBlob CompileOrGetShader(const std::filesystem::path &filepath, ShaderMake::ShaderType type, bool recompile);
        static Ref<Shader> Create(const std::filesystem::path &filepath, ShaderMake::ShaderType type, bool recompile = false);
        nvrhi::ShaderHandle GetHandle() { return m_Handle; }

        static void SPIRVReflect(ShaderMake::ShaderType type, const ShaderMake::ShaderBlob &blob);
    private:
        nvrhi::ShaderHandle m_Handle = nullptr;
    };
}
