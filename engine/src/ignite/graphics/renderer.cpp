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

#include "environment.hpp"

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
        m_ShaderContext->CompileShader(contexts);

        // load to NVRHI Shader handle
        for (auto& shader : m_Shaders | std::views::values)
        {
            for (auto &[type, shader] : shader)
            {
                shader.handle = s_instance->m_Device->createShader(type,
                    shader.context->blob.data.data(),
                    shader.context->blob.dataSize());
            }
        }
    }

    void ShaderLibrary::CompileShaders(const std::vector<Ref<ShaderMake::ShaderContext>> &contexts)
    {
        m_ShaderContext->CompileShader(contexts);
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
            return m_Shaders[name];
        return {};
    }

    ShaderMake::Context *ShaderLibrary::GetContext() const
    {
        return m_ShaderContext.get();
    }

    Renderer::Renderer(DeviceManager *deviceManager, nvrhi::GraphicsAPI api)
    {
        s_instance = this;
        m_GraphicsAPI = api;

        s_instance->m_Device = deviceManager->GetDevice();

        m_CommandList = CommandList::Create();
        auto cmd = m_CommandList->GetActiveHandle();

        {
            TextureCreateInfo textureCI;
            textureCI.format = nvrhi::Format::RGBA8_UNORM;
            textureCI.dimension = nvrhi::TextureDimension::Texture2D;
            textureCI.samplerMode = nvrhi::SamplerAddressMode::ClampToBorder;
            textureCI.width = 1;
            textureCI.height = 1;
            textureCI.flip = false;

            m_CommandList->Begin();

            u32 white = 0xFFFFFFFF;
            m_WhiteTexture = Texture::Create(Buffer(&white, sizeof(u32)), textureCI);
            m_WhiteTexture->Write(cmd);

            u32 black = 0x00000000;
            m_BlackTexture = Texture::Create(Buffer(&black, sizeof(u32)), textureCI);
            m_BlackTexture->Write(cmd);
            m_CommandList->Submit();
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
        m_BindingLayouts[GLayoutMap::MATERIAL] = s_instance->m_Device->createBindingLayout(VertexMesh_Anim::GetMaterialBindingLayoutDesc());
        m_BindingLayouts[GLayoutMap::ENVIRONMENT] = s_instance->m_Device->createBindingLayout(Environment::GetBindingLayoutDesc());

        Renderer2D::Init();
    }

    Renderer::~Renderer()
    {
        m_WhiteTexture.reset();
        Renderer2D::Shutdown();
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

    Ref<Texture> Renderer::GetWhiteTexture()
    {
        return s_instance->m_WhiteTexture;
    }

    Ref<Texture> Renderer::GetBlackTexture()
    {
        return s_instance->m_BlackTexture;
    }
}
