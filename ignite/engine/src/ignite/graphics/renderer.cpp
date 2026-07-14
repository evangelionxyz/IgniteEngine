// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "renderer.hpp"
#include "renderer/renderer_2d.hpp"
#include "texture.hpp"
#include "shader.hpp"
#include "bindless_system.hpp"

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/core/path.hpp"

namespace ignite
{
    static Renderer *s_RendererInstance = nullptr;

    RendererStats Renderer::Stats;

    Renderer::Renderer(DeviceManager *deviceManager, nvrhi::GraphicsAPI api)
    {
        s_RendererInstance = this;

        m_GraphicsAPI = api;

        m_Device = deviceManager->GetDevice();

        Shader::InitShaderData();

		// Create binding layouts
		m_BindingLayouts[EBindingLayout::MESH_STATIC] = m_Device->createBindingLayout(VertexMeshStatic::GetBindingLayoutDesc());
		m_BindingLayouts[EBindingLayout::MESH_ANIM] = m_Device->createBindingLayout(VertexMeshAnim::GetBindingLayoutDesc());
		m_BindingLayouts[EBindingLayout::ENVIRONMENT] = m_Device->createBindingLayout(Environment::GetBindingLayoutDesc());
		m_BindingLayouts[EBindingLayout::MATERIAL] = m_Device->createBindingLayout(Material::GetBindingLayoutDesc());


        nvrhi::CommandListHandle cmd = m_Device->createCommandList();

        // Default textures
        {
            TextureCreateInfo textureCreateInfo;
            textureCreateInfo.format = nvrhi::Format::RGBA8_UNORM;
            textureCreateInfo.dimension = nvrhi::TextureDimension::Texture2D;
            textureCreateInfo.width = 1;
            textureCreateInfo.height = 1;
            textureCreateInfo.flip = false;
        	textureCreateInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        	textureCreateInfo.keepInitialState = true;

			cmd->open();

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

            cmd->close();
            m_Device->executeCommandList(cmd);
        }

		// Default materials
		{
			m_DefaultMaterial = CreateRef<Material>();
		}
    }

	void Renderer::Shutdown()
	{
        LOG_WARN("[Renderer] Shutdown");

		Shader::ShutdownShaderData();
		Shader::s_DXCInstance.reset();

		m_WhiteTexture.reset();
		m_MagentaTexture.reset();
		m_BlackTexture.reset();

        m_DefaultMaterial.reset();

        m_DefaultMeshes.clear();
		m_BindingLayouts.clear();

		BindlessSystem::Shutdown();
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
        return s_RendererInstance->m_GraphicsAPI;
    }

    nvrhi::BindingLayoutHandle Renderer::GetBindingLayout(EBindingLayout type)
    {
        if (s_RendererInstance->m_BindingLayouts.contains(type))
            return s_RendererInstance->m_BindingLayouts[type];

        return nullptr;
    }

    Ref<Texture> Renderer::GetWhiteTexture()
    {
        return s_RendererInstance->m_WhiteTexture;
    }

    Ref<Texture> Renderer::GetBlackTexture()
    {
        return s_RendererInstance->m_BlackTexture;
    }

    Ref<Texture> Renderer::GetMagentaTexture()
    {
        return s_RendererInstance->m_MagentaTexture;
    }

	Ref<Material> Renderer::GetDefaultMaterial()
	{
        return s_RendererInstance->m_DefaultMaterial;
	}

	Ref<StaticMesh> Renderer::GetDefaultMesh(EMeshType type)
	{
		auto it = s_RendererInstance->m_DefaultMeshes.find(type);
        if (it != s_RendererInstance->m_DefaultMeshes.end())
            return s_RendererInstance->m_DefaultMeshes[type];

        s_RendererInstance->m_DefaultMeshes[type] = StaticMesh::Deserialize("resources/staticmeshes/uvsphere.mesh");
        return s_RendererInstance->m_DefaultMeshes[type];
	}

}
