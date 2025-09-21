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

#include "renderer.hpp"
#include "renderer_2d.hpp"
#include "texture.hpp"
#include "shader.hpp"

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/environment.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/application.hpp"

#include <ranges>

namespace ignite
{
    Renderer *s_instance = nullptr;

    void ShaderLibrary::Init(nvrhi::GraphicsAPI api)
    {
        m_ShaderMakeOptions.compilerType = ShaderMake::CompilerType_DXC;
        m_ShaderMakeOptions.optimizationLevel = 3;
        m_ShaderMakeOptions.baseDirectory = "resources/shaders/";
        m_ShaderMakeOptions.outputDir = "bin";

        if (api == nvrhi::GraphicsAPI::VULKAN)
            m_ShaderMakeOptions.platformType = ShaderMake::PlatformType_SPIRV;
        else if (api == nvrhi::GraphicsAPI::D3D12)
            m_ShaderMakeOptions.platformType = ShaderMake::PlatformType_DXIL;

        m_ShaderContext = CreateScope<ShaderMake::Context>(&m_ShaderMakeOptions);
    }

    void ShaderLibrary::Compile()
    {
        std::vector<Ref<ShaderMake::ShaderContext>> contexts;
        for (auto &shader : m_Shaders | std::views::values)
        {
            for (auto & [context, handle] : shader | std::views::values)
            {
                contexts.push_back(context);
            }
        }

        // compile at once
        // m_DXILShaderContext->CompileShader(contexts);
        m_ShaderContext->CompileShader(contexts);

        // load to NVRHI Shader handle
        for (auto& shader : m_Shaders | std::views::values)
        {
            for (auto &[type, shader] : shader)
            {
                shader.handle = s_instance->m_Device->createShader(type,
                    shader.context->blob.data.data(), shader.context->blob.dataSize());
            }
        }
    }

    ShaderMake::CompileStatus ShaderLibrary::CompileShaders(const std::vector<Ref<ShaderMake::ShaderContext>> &contexts)
    {
        // m_DXILShaderContext->CompileShader(contexts);
        ShaderMake::CompileStatus status = m_ShaderContext->CompileShader(contexts);
        return status;
    }

    void ShaderLibrary::Load(const std::string &name, const std::string &filepath)
    {
        if (!Exists(name))
        {
            std::unordered_map<nvrhi::ShaderType, ShaderHandleContext> shader;
            shader[nvrhi::ShaderType::Vertex] = { CreateRef<ShaderMake::ShaderContext>(filepath + ".vertex.hlsl", ShaderMake::ShaderType::Vertex), nullptr };
            shader[nvrhi::ShaderType::Pixel] = { CreateRef<ShaderMake::ShaderContext>(filepath + ".pixel.hlsl", ShaderMake::ShaderType::Pixel), nullptr };
            m_Shaders[name] = shader;
        }
    }

    bool ShaderLibrary::Exists(const std::string &name) const
    {
        return m_Shaders.contains(name);
    }

    std::unordered_map<nvrhi::ShaderType, ShaderHandleContext> ShaderLibrary::Get(const std::string &name)
    {
        if (Exists(name))
        {
            return m_Shaders[name];
        }

        return {};
    }

    Renderer::Renderer(DeviceManager *deviceManager, nvrhi::GraphicsAPI api)
    {
        s_instance = this;
        m_GraphicsAPI = api;

        s_instance->m_Device = deviceManager->GetDevice();

		// non volatile constant buffer
		m_EditorCameraConstantBuffer = ConstantBuffer::Create(sizeof(CameraBuffer), false, 1, "Camera Constant Buffer");

        m_CommandList = CommandList::Create();
        auto cmd = m_CommandList->GetActiveHandle();

        {
            TextureCreateInfo textureCreateInfo;
            textureCreateInfo.format = nvrhi::Format::RGBA8_UNORM;
            textureCreateInfo.dimension = nvrhi::TextureDimension::Texture2D;
            textureCreateInfo.samplerMode = nvrhi::SamplerAddressMode::ClampToEdge;
            textureCreateInfo.width = 1;
            textureCreateInfo.height = 1;
            textureCreateInfo.flip = false;

            uint32_t white = 0xFFFFFFFF;
            m_WhiteTexture = Texture::Create(Buffer(&white, sizeof(u32)), textureCreateInfo);

            uint32_t black = 0x00000000;
            m_BlackTexture = Texture::Create(Buffer(&black, sizeof(uint32_t)), textureCreateInfo);

            uint32_t magenta = 0xFFFF00FF;
            m_MagentaTexture = Texture::Create(Buffer(&magenta, sizeof(uint32_t)), textureCreateInfo);
        }

        // Create shaders
        {
            m_ShaderLibrary.Init(m_GraphicsAPI);

            m_ShaderLibrary.Load("batch_2d_quad", "batch_2d_quad");
            m_ShaderLibrary.Load("batch_2d_line", "batch_2d_line");
            m_ShaderLibrary.Load("imgui", "imgui");
            m_ShaderLibrary.Load("skybox", "skybox");

            m_ShaderLibrary.Compile();
        }

        // Create binding layouts
        m_BindingLayouts[GLayoutMap::MESH_ANIM] = s_instance->m_Device->createBindingLayout(VertexMesh_Anim::GetBindingLayoutDesc());
        m_BindingLayouts[GLayoutMap::ENVIRONMENT] = s_instance->m_Device->createBindingLayout(Environment::GetBindingLayoutDesc());
        m_BindingLayouts[GLayoutMap::MATERIAL] = s_instance->m_Device->createBindingLayout(Material::GetBindingLayoutDesc());
    }

    Renderer::~Renderer()
    {
        m_WhiteTexture.reset();
    }

    nvrhi::GraphicsAPI Renderer::GetGraphicsAPI()
    {
        return s_instance->m_GraphicsAPI;
    }

    nvrhi::BindingLayoutHandle Renderer::GetBindingLayout(GLayoutMap type)
    {
        if (s_instance->m_BindingLayouts.contains(type))
            return s_instance->m_BindingLayouts[type];

        return nullptr;
    }

    void Renderer::OnUpdate()
    {
        if (s_instance->m_SubmitFuncs.empty())
            return;

        s_instance->m_CommandList->Begin();
        auto cmd = s_instance->m_CommandList->GetActiveHandle();

        for (const auto &func : s_instance->m_SubmitFuncs)
            func(cmd);
        
        s_instance->m_CommandList->Submit();

        s_instance->m_SubmitFuncs.clear();
    }

    void Renderer::Submit(const std::function<void(nvrhi::ICommandList*)>& func)
    {
        s_instance->m_SubmitFuncs.push_back(func);
    }

    ShaderLibrary &Renderer::GetShaderLibrary()
    {
        return s_instance->m_ShaderLibrary;
    }

	Ref<ConstantBuffer> Renderer::GetCameraConstantBuffer()
	{
		return s_instance->m_EditorCameraConstantBuffer;
	}

    Ref<Texture> Renderer::GetWhiteTexture()
    {
        return s_instance->m_WhiteTexture;
    }

    Ref<Texture> Renderer::GetBlackTexture()
    {
        return s_instance->m_BlackTexture;
    }

    Ref<Texture> Renderer::GetMagentaTexture()
    {
        return s_instance->m_MagentaTexture;
    }

}
