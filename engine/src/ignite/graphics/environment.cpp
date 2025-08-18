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

#include "environment.hpp"
#include "vertex_data.hpp"
#include "graphics_pipeline.hpp"
#include "renderer.hpp"

#include "ignite/scene/icamera.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/core/application.hpp"

#include <stb_image.h>

namespace ignite {

    // clock wise
    std::array<glm::vec3, 24> vertices = {

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
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        // create vertex buffer
        m_VertexBuffer = VertexBuffer::Create(sizeof(vertices), "[Environment] Vertex Buffer");
        m_IndexBuffer = IndexBuffer::Create(sizeof(uint32_t) * 36, "[Environment] Index Buffer");

        m_ParamsConstantBuffer = ConstantBuffer::Create(sizeof(EnvironmentParams), true, 16, "[Environment] EnvironmentParams constant buffer!");
        m_DirLightConstantBuffer = ConstantBuffer::Create(sizeof(DirLight), true, 16, "[Environment] DirLight constant buffer!");
    }

    void Environment::Begin(nvrhi::ICommandList *commandList, ICamera *camera, nvrhi::IFramebuffer *framebuffer, const Ref<GraphicsPipeline> &pipeline)
    {
        CameraConstants cameraConstants = { camera->GetViewProjectionMatrix(), glm::vec4(camera->position, 1.0f) };

        // write params buffer
        m_ParamsConstantBuffer->SetData(commandList, Buffer(&params, sizeof(EnvironmentParams)));
        m_DirLightConstantBuffer->SetData(commandList, Buffer(&dirLight, sizeof(DirLight)));

        // render
        auto state = nvrhi::GraphicsState();
        state.pipeline = pipeline->GetHandle();
        state.framebuffer = framebuffer;
        state.viewport = nvrhi::ViewportState().addViewportAndScissorRect(framebuffer->getFramebufferInfo().getViewport());
        state.addVertexBuffer({ m_VertexBuffer->GetHandle(), 0, 0 });
        state.indexBuffer = { m_IndexBuffer->GetHandle(), nvrhi::Format::R32_UINT };

        if (m_BindingSet != nullptr)
        {
            state.addBindingSet(m_BindingSet);
        }

        commandList->setGraphicsState(state);

        // push camera constants
        commandList->setPushConstants(&cameraConstants, sizeof(CameraConstants));

        nvrhi::DrawArguments args;
        args.setVertexCount(36);
        args.instanceCount = 1;

        commandList->drawIndexed(args);
    }

    void Environment::End()
    {
        m_Invalidating = false;
    }

    void Environment::LoadTexture(const std::string &filepath)
    {
        m_Invalidating = true;
        
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        TextureCreateInfo textureCI;
        textureCI.dimension = nvrhi::TextureDimension::Texture2D;
        textureCI.format = nvrhi::Format::RGBA32_FLOAT;
        textureCI.flip = true; // usually HDR textures are flipped

        m_HDRTexture = Texture::Create(filepath, textureCI);

        // create binding set after load the texture
        nvrhi::BindingSetDesc bsDesc;
        bsDesc.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(CameraConstants)));
        bsDesc.addItem(nvrhi::BindingSetItem::ConstantBuffer(1, m_ParamsConstantBuffer->GetHandle()));
        bsDesc.addItem(nvrhi::BindingSetItem::Texture_SRV(0, m_HDRTexture->GetHandle()));
        bsDesc.addItem(nvrhi::BindingSetItem::Sampler(0, m_HDRTexture->GetSampler()));

        m_BindingSet = device->createBindingSet(bsDesc, Renderer::GetBindingLayout(GLayoutMap::ENVIRONMENT));
        LOG_ASSERT(m_BindingSet, "Failed to create binding set");
    }

    void Environment::WriteBuffer(nvrhi::ICommandList *commandList)
    {
        // write buffers
        m_HDRTexture->Write(commandList);
        m_VertexBuffer->SetData(commandList, Buffer(vertices.data(), sizeof(vertices)));

        // index buffer
        u32 *indices = new u32[36];
        u32 Offset = 0;
        for (u32 i = 0; i < 36; i += 6)
        {
            indices[i + 0] = Offset + 0;
            indices[i + 1] = Offset + 1;
            indices[i + 2] = Offset + 2;

            indices[i + 3] = Offset + 2;
            indices[i + 4] = Offset + 3;
            indices[i + 5] = Offset + 0;

            Offset += 4;
        }

        m_IndexBuffer->SetData(commandList, Buffer(indices, sizeof(uint32_t) * 36));
        delete[] indices;
    }

    void Environment::SetSunDirection(float pitch, float yaw)
    {
        float pitchR = glm::radians(pitch); // elevation
        float yawR = glm::radians(yaw); // azimuth

        glm::vec3 dir;
        dir.x = cos(pitchR) * sin(yawR);
        dir.y = sin(pitchR);
        dir.z = cos(pitchR) * cos(yawR);

        dirLight.direction = glm::vec4(glm::normalize(dir), 0.0f);
    }

    Ref<Environment> Environment::Create()
    {
        return CreateRef<Environment>();
    }

    nvrhi::VertexAttributeDesc Environment::GetAttribute()
    {
        return nvrhi::VertexAttributeDesc()
            .setName("POSITION")
            .setFormat(nvrhi::Format::RGB32_FLOAT)
            .setOffset(0)
            .setElementStride(sizeof(glm::vec3));
    }

    nvrhi::BindingLayoutDesc Environment::GetBindingLayoutDesc()
    {
        return nvrhi::BindingLayoutDesc()
            .setVisibility(nvrhi::ShaderType::All)
            .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(CameraConstants)))
            .addItem(nvrhi::BindingLayoutItem::VolatileConstantBuffer(1))
            .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
            .addItem(nvrhi::BindingLayoutItem::Sampler(0));
    }
}
