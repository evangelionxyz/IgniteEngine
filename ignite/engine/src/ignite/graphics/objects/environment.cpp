// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "environment.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/input/asset_signal.hpp"

#include <stb_image.h>

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

        auto samplerDesc = nvrhi::SamplerDesc();
        samplerDesc.addressU = nvrhi::SamplerAddressMode::Repeat;
        m_Sampler = DeviceManager::GetInstance()->GetDevice()->createSampler(samplerDesc);
        LOG_ASSERT(m_Sampler, "Failed to create sampler");
    }

    Environment::~Environment()
    {
        if (auto *device = DeviceManager::GetInstance()->GetDevice())
        {
            device->waitForIdle();
        }

        // Clear binding set first (it references other resources)
        m_BindingSet.Reset();
        m_Sampler.Reset();

        m_HDRTexture.reset();
        m_VertexBuffer.reset();
        m_IndexBuffer.reset();
    }

    void Environment::Draw(nvrhi::ICommandList *cmd, nvrhi::IFramebuffer *fb, const Ref<GraphicsPipeline> &pipeline)
    {
        LOG_ASSERT(m_BindingSet, "[Environment] Invalid binding set");

        // render
        auto state = nvrhi::GraphicsState();
        state.pipeline = pipeline->GetHandle();
        state.framebuffer = fb;
        state.bindings = { m_BindingSet };
        state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(fb->getFramebufferInfo().getViewport());
        state.addVertexBuffer({ m_VertexBuffer->GetHandle(), 0, 0 });
        state.indexBuffer = { m_IndexBuffer->GetHandle(), nvrhi::Format::R32_UINT };

        cmd->setGraphicsState(state);

        nvrhi::DrawArguments args;
        args.setVertexCount(36);
        args.instanceCount = 1;

        cmd->drawIndexed(args);
    }

    bool Environment::UpdateBindingSet(const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &sceneBuffer)
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        if (!device)
        {
            return false;
        }

        if (!cameraBuffer || !sceneBuffer)
        {
            return false;
        }

        if (!m_HDRTexture)
        {
            m_HDRTexture = Renderer::GetBlackTexture();
        }

        // create binding set after load the texture
        nvrhi::BindingSetDesc bsDesc;
        bsDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(0, cameraBuffer->GetHandle()));
        bsDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, sceneBuffer->GetHandle()));
        bsDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_HDRTexture->GetHandle()));
        bsDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_Sampler));

        m_BindingSet = device->createBindingSet(bsDesc, Renderer::GetBindingLayout(EBindingLayout::ENVIRONMENT));
        LOG_ASSERT(m_BindingSet, "Failed to create binding set");

		// Notify dependents (e.g. AssetManager) that this environment has changed.
		Application::SubmitToMainThread([this]()
		{
			SignalBus::Emit(AssetChangeSignal{ handle, AssetType::Environment });
		});

        return m_BindingSet != nullptr;
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
        m_VertexBuffer->SetData(cmd, Buffer(m_Vertices.data(), sizeof(m_Vertices)));
        m_IndexBuffer->SetData(cmd, Buffer(m_Indices.data(), sizeof(uint32_t) * m_Indices.size()));
    }

    Ref<Environment> Environment::Create()
    {
        return CreateRef<Environment>();
    }

    nvrhi::BindingLayoutDesc Environment::GetBindingLayoutDesc()
    {
        return nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(0)) // camera
            .addItem(nvrhi::BindingLayoutItem::ConstantBuffer(1)) // scene
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
