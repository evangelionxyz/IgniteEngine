// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "environment.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/binding_cache.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"

#include <stb_image.h>

#include "procedural_sky.hpp"

namespace ignite
{
    Environment::Environment()
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        CreateVerticesIndices();

        // create vertex buffer
        m_VertexBuffer = VertexBuffer::Create(sizeof(m_Vertices), "Environment Vertex Buffer");
        m_IndexBuffer = IndexBuffer::Create(sizeof(uint32_t) * m_Indices.size(), "Environment Index Buffer");

        m_HDRTexture = Renderer::GetBlackTexture();
        m_ProceduralSky = CreateRef<ProceduralSky>();
        m_SkyType = SkyType::HDRI;

        auto samplerDesc = nvrhi::SamplerDesc();
        samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
        m_Sampler = DeviceManager::GetInstance()->GetDevice()->createSampler(samplerDesc);
        LOG_ASSERT(m_Sampler, "Failed to create sampler");
    }

    Environment::~Environment()
    {
        m_Sampler.Reset();

        m_HDRTexture.reset();
        m_ProceduralSky.reset();
        m_VertexBuffer.reset();
        m_IndexBuffer.reset();
    }

    void Environment::Draw(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb, const Ref<GraphicsPipeline> &pipeline,
        const nvrhi::BufferHandle &cameraBuffer, const nvrhi::BufferHandle &sceneBuffer)
    {
        Ref<Texture> tex = nullptr;
        if (m_SkyType == SkyType::ProceduralSky && m_ProceduralSky && m_ProceduralSky->GetSkyViewLUT())
        {
            tex = m_ProceduralSky->GetSkyViewLUT();
        }
        else
        {
            tex = m_HDRTexture ? m_HDRTexture : Renderer::GetBlackTexture();
        }

        nvrhi::BindingSetDesc bsDesc;
        bsDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer));
        bsDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, sceneBuffer));
        bsDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, tex->GetHandle()));
        bsDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_Sampler));

        nvrhi::BindingSetHandle bindingSet = BindingCache::GetOrCreateBindingSet(bsDesc, Renderer::GetBindingLayout(EBindingLayout::ENVIRONMENT));
        LOG_ASSERT(bindingSet, "[Environment] Failed to get or create binding set");

        // render
        auto state = nvrhi::GraphicsState();
        state.pipeline = pipeline->GetHandle();
        state.framebuffer = fb;
        state.bindings = { bindingSet };
        state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(fb->getFramebufferInfo().getViewport());
        state.addVertexBuffer({ m_VertexBuffer->GetHandle(), 0, 0 });
        state.indexBuffer = { m_IndexBuffer->GetHandle(), nvrhi::Format::R32_UINT };

        cmd->setGraphicsState(state);

        nvrhi::DrawArguments args;
        args.setVertexCount(36);
        args.instanceCount = 1;

        cmd->drawIndexed(args);
    }

    void Environment::LoadTexture(const std::string& filepath)
    {
        TextureCreateInfo textureCI;
        textureCI.dimension = nvrhi::TextureDimension::Texture2D;
        textureCI.format = nvrhi::Format::RGBA32_FLOAT;
        textureCI.flip = true;
    	textureCI.keepInitialState = true;
    	textureCI.initialState = nvrhi::ResourceStates::ShaderResource;
        m_HDRTexture = Texture::Create(filepath, textureCI, nullptr, "Environment HDR");
    }

    void Environment::SetTexture(const Ref<Texture> &texture)
    {
        m_HDRTexture = texture ? texture : Renderer::GetBlackTexture();
    }

	void Environment::WriteBuffer(nvrhi::ICommandList *cmd)
    {
        // write buffers
        m_VertexBuffer->SetData(cmd, m_Vertices.data(), sizeof(m_Vertices));
        m_IndexBuffer->SetData(cmd, m_Indices.data(), sizeof(uint32_t) * m_Indices.size());
    }

    Ref<Environment> Environment::Create()
    {
        return CreateRef<Environment>();
    }

    nvrhi::BindingLayoutDesc Environment::GetBindingLayoutDesc()
    {
        return nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(0)) // camera
            .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(1)) // scene
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0)) // texture
            .addItem(nvrhi::BindingLayoutItem::Sampler(0));
    }

	void Environment::CreateVerticesIndices()
	{
		// clock wise Vertices
		m_Vertices =
		{
			glm::vec3(1.0f,  1.0f,  1.0f), // top right    front  
			glm::vec3(1.0f,  1.0f, -1.0f), // top right    back
			glm::vec3(1.0f, -1.0f, -1.0f), // bottom right back
			glm::vec3(1.0f, -1.0f,  1.0f), // bottom right front

			glm::vec3(-1.0f,  1.0f, -1.0f), // top    left back
			glm::vec3(-1.0f,  1.0f,  1.0f), // top    left front
			glm::vec3(-1.0f, -1.0f,  1.0f), // bottom left front
			glm::vec3(-1.0f, -1.0f, -1.0f), // bottom left back

			glm::vec3(-1.0f,  1.0f,  1.0f), // top left  front
			glm::vec3(-1.0f,  1.0f, -1.0f), // top left  back
			glm::vec3(1.0f,  1.0f, -1.0f), // top right back
			glm::vec3(1.0f,  1.0f,  1.0f), // top right front

			glm::vec3(-1.0f, -1.0f,  1.0f), // bottom left  front
			glm::vec3(1.0f, -1.0f,  1.0f), // bottom right front
			glm::vec3(1.0f, -1.0f, -1.0f), // bottom right back
			glm::vec3(-1.0f, -1.0f, -1.0f), // bottom left  back

			glm::vec3(-1.0f, -1.0f, -1.0f), // bottom left  back
			glm::vec3(1.0f, -1.0f, -1.0f), // bottom right back
			glm::vec3(1.0f,  1.0f, -1.0f), // top    right back
			glm::vec3(-1.0f,  1.0f, -1.0f), // top    left  back

			glm::vec3(-1.0f, -1.0f,  1.0f), // bottom left  front
			glm::vec3(-1.0f,  1.0f,  1.0f), // top    left  front
			glm::vec3(1.0f,  1.0f,  1.0f), // top    right front
			glm::vec3(1.0f, -1.0f,  1.0f), // bottom right front
		};


        // Indices
		size_t offset = 0;
		for (size_t i = 0; i < m_Indices.size(); i += 6)
		{
			m_Indices[i + 0] = static_cast<uint32_t>(offset + 0zu);
			m_Indices[i + 1] = static_cast<uint32_t>(offset + 1zu);
			m_Indices[i + 2] = static_cast<uint32_t>(offset + 2zu);
			m_Indices[i + 3] = static_cast<uint32_t>(offset + 2zu);
			m_Indices[i + 4] = static_cast<uint32_t>(offset + 3zu);
			m_Indices[i + 5] = static_cast<uint32_t>(offset + 0zu);
			offset += 4;
		}
	}
}
