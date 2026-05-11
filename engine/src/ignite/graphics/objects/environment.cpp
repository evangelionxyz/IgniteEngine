/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu
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

#include "environment.hpp"
#include "ignite/graphics/vertex_data.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/device/device_manager.hpp"

#include <stb_image.h>

namespace ignite
{

    // clock wise
    std::array<glm::vec3, 24> vertices =
    {
        glm::vec3( 1.0f,  1.0f,  1.0f), // top right    front  
        glm::vec3( 1.0f,  1.0f, -1.0f), // top right    back
        glm::vec3( 1.0f, -1.0f, -1.0f), // bottom right back
        glm::vec3( 1.0f, -1.0f,  1.0f), // bottom right front

        glm::vec3(-1.0f,  1.0f, -1.0f), // top    left back
        glm::vec3(-1.0f,  1.0f,  1.0f), // top    left front
        glm::vec3(-1.0f, -1.0f,  1.0f), // bottom left front
        glm::vec3(-1.0f, -1.0f, -1.0f), // bottom left back
        
        glm::vec3(-1.0f,  1.0f,  1.0f), // top left  front
        glm::vec3(-1.0f,  1.0f, -1.0f), // top left  back
        glm::vec3( 1.0f,  1.0f, -1.0f), // top right back
        glm::vec3( 1.0f,  1.0f,  1.0f), // top right front

        glm::vec3(-1.0f, -1.0f,  1.0f), // bottom left  front
        glm::vec3( 1.0f, -1.0f,  1.0f), // bottom right front
        glm::vec3( 1.0f, -1.0f, -1.0f), // bottom right back
        glm::vec3(-1.0f, -1.0f, -1.0f), // bottom left  back

        glm::vec3(-1.0f, -1.0f, -1.0f), // bottom left  back
        glm::vec3( 1.0f, -1.0f, -1.0f), // bottom right back
        glm::vec3( 1.0f,  1.0f, -1.0f), // top    right back
        glm::vec3(-1.0f,  1.0f, -1.0f), // top    left  back

        glm::vec3(-1.0f, -1.0f,  1.0f), // bottom left  front
        glm::vec3(-1.0f,  1.0f,  1.0f), // top    left  front
        glm::vec3( 1.0f,  1.0f,  1.0f), // top    right front
        glm::vec3( 1.0f, -1.0f,  1.0f), // bottom right front
    };

    Environment::Environment()
    {
        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();

        // create vertex buffer
        m_VertexBuffer = VertexBuffer::Create(sizeof(vertices), "Environment Vertex Buffer");
        m_IndexBuffer = IndexBuffer::Create(sizeof(uint32_t) * 36, "Environment Index Buffer");

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

        // Clear sampler
        m_Sampler.Reset();

        // Clear texture and buffers
        m_HDRTexture.reset();
        m_VertexBuffer.reset();
        m_IndexBuffer.reset();
    }

    void Environment::Draw(nvrhi::ICommandList *cmd, ICamera *camera, nvrhi::IFramebuffer *fb, const Ref<GraphicsPipeline> &pipeline)
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

        m_BindingSet = device->createBindingSet(bsDesc, Renderer::GetBindingLayout(GLayoutMap::ENVIRONMENT));
        LOG_ASSERT(m_BindingSet, "Failed to create binding set");
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
        m_VertexBuffer->SetData(cmd, Buffer(vertices.data(), sizeof(vertices)));

        // index buffer
		std::vector<uint32_t> indices(36);
        u32 offset = 0;
        for (u32 i = 0; i < 36; i += 6)
        {
            indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
        }

        m_IndexBuffer->SetData(cmd, Buffer(indices.data(), sizeof(uint32_t) * indices.size()));
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
}
