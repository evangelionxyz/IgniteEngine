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
#include "renderer/renderer_2d.hpp"
#include "texture.hpp"
#include "shader.hpp"

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/core/device/device_manager.hpp"

#include <ranges>
#include <filesystem>

namespace ignite
{
    Renderer *s_instance = nullptr;
    RendererStats Renderer::Stats;

    Renderer::Renderer(DeviceManager *deviceManager, nvrhi::GraphicsAPI api)
    {
        s_instance = this;
        m_GraphicsAPI = api;

        s_instance->m_Device = deviceManager->GetDevice();

        m_DxcInstance = ShaderCompiler::CreateDXCCompiler();

        nvrhi::CommandListHandle cmd = DeviceManager::GetInstance()->GetDevice()->createCommandList();
        cmd->open();

        {
            TextureCreateInfo textureCreateInfo;
            textureCreateInfo.format = nvrhi::Format::RGBA8_UNORM;
            textureCreateInfo.dimension = nvrhi::TextureDimension::Texture2D;
            textureCreateInfo.width = 1;
            textureCreateInfo.height = 1;
            textureCreateInfo.flip = false;
        	textureCreateInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        	textureCreateInfo.keepInitialState = true;

            size_t texSize = sizeof(uint32_t);
            uint32_t white = 0xFFFFFFFF;
            std::vector<uint8_t> whiteData(texSize);
            memcpy(whiteData.data(), &white, texSize);
            m_WhiteTexture = Texture::Create(whiteData, textureCreateInfo, cmd);

            uint32_t black = 0x00000000;
            std::vector<uint8_t> blackData(texSize);
            memcpy(blackData.data(), &black, texSize);
            m_BlackTexture = Texture::Create(blackData, textureCreateInfo, cmd);

            uint32_t magenta = 0xFFFF00FF;
            std::vector<uint8_t> magentaData(texSize);
            memcpy(magentaData.data(), &magenta, texSize);
            m_MagentaTexture = Texture::Create(magentaData, textureCreateInfo, cmd);
        }

        cmd->close();
        Application::SubmitWorkerCommandList(cmd);

        // Create binding layouts
        m_BindingLayouts[GLayoutMap::MESH_ANIM] = s_instance->m_Device->createBindingLayout(VertexMesh_Anim::GetBindingLayoutDesc());
        m_BindingLayouts[GLayoutMap::ENVIRONMENT] = s_instance->m_Device->createBindingLayout(Environment::GetBindingLayoutDesc());
        m_BindingLayouts[GLayoutMap::MATERIAL] = s_instance->m_Device->createBindingLayout(Material::GetBindingLayoutDesc());
    }

    Renderer::~Renderer()
    {
        MeshInstance::ReleaseGlobalResources();
        m_DxcInstance.reset();
        m_WhiteTexture.reset();
        m_MagentaTexture.reset();
        m_BlackTexture.reset();
    }

    void Renderer::BeginStats()
    {
        Stats.quadCount = 0;
        Stats.lineCount = 0;
        Stats.circleCount = 0;
        Stats.textCount = 0;
        Stats.pointLight2dCount = 0;
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

    Ref<DXCInstance> Renderer::GetDXCInstance()
    {
        return s_instance->m_DxcInstance;
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
