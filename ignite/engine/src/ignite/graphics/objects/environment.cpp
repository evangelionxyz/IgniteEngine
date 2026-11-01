// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "environment.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/binding_cache.hpp"
#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/device/device_manager.hpp"

#include "procedural_sky.hpp"

#include <stb_image.h>

namespace ignite
{
    Environment::Environment()
    {
        auto &nri = DeviceManager::GetInstance()->GetNRI();
        nri::Device *device = DeviceManager::GetInstance()->GetDevice();

        CreateVerticesIndices();

        // create vertex buffer
        m_VertexBuffer = VertexBuffer::Create(sizeof(glm::vec3) * m_Vertices.size(), "Environment Vertex Buffer");
        m_IndexBuffer = IndexBuffer::Create(sizeof(uint32_t) * m_Indices.size(), "Environment Index Buffer");

        m_HDRTexture = Renderer::GetBlackTexture();
        m_ProceduralSky = CreateRef<ProceduralSky>();
        m_SkyType = SkyType::HDRI;

        nri::SamplerDesc samplerDesc = {};
        samplerDesc.filters = { nri::Filter::LINEAR, nri::Filter::LINEAR, nri::Filter::LINEAR };
        samplerDesc.addressModes = { nri::AddressMode::REPEAT, nri::AddressMode::CLAMP_TO_EDGE, nri::AddressMode::CLAMP_TO_EDGE };
        samplerDesc.mipMax = 16.0f;
        nri.CreateSampler(*device, samplerDesc, m_Sampler);
        LOG_ASSERT(m_Sampler, "Failed to create sampler");
    }

    Environment::~Environment()
    {
        if (m_Sampler)
        {
            DeviceManager::GetInstance()->GetNRI().DestroyDescriptor(m_Sampler);
            m_Sampler = nullptr;
        }

        m_HDRTexture.reset();
        m_ProceduralSky.reset();
        m_VertexBuffer.reset();
        m_IndexBuffer.reset();
    }

    void Environment::Draw(nri::CommandBuffer *cmd, RenderTarget *rt, const Ref<GraphicsPipeline> &pipeline,
        const ConstantBuffer &cameraBuffer, const ConstantBuffer &sceneBuffer)
    {
        if (!cmd || !pipeline || !pipeline->GetHandle())
            return;

        auto &nri = DeviceManager::GetInstance()->GetNRI();

        Ref<Texture> tex = nullptr;
        if (m_SkyType == SkyType::ProceduralSky && m_ProceduralSky && m_ProceduralSky->GetSkyViewLUT())
        {
            tex = m_ProceduralSky->GetSkyViewLUT();
        }
        else
        {
            tex = m_HDRTexture ? m_HDRTexture : Renderer::GetBlackTexture();
        }

        BindingSetDesc bsDesc;
        bsDesc.AddItem(BindingSetItem::ConstantBuffer(0, cameraBuffer.GetDescriptor()));
        bsDesc.AddItem(BindingSetItem::ConstantBuffer(1, sceneBuffer.GetDescriptor()));
        bsDesc.AddItem(BindingSetItem::TextureSRV(0, tex->GetShaderResourceView()));
        bsDesc.AddItem(BindingSetItem::Sampler(0, m_Sampler));

        Ref<BindingSet> bindingSet = BindingCache::GetOrCreateBindingSet(bsDesc, pipeline->GetPipelineLayout(), 0, "Environment");
        LOG_ASSERT(bindingSet, "[Environment] Failed to get or create binding set");

        const bool startedRendering = rt && !rt->IsRendering();
        if (startedRendering)
            rt->BeginRendering(cmd, false, false);

        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *pipeline->GetPipelineLayout());
        nri.CmdSetPipeline(*cmd, *pipeline->GetHandle());

        nri::SetDescriptorSetDesc setDesc = {};
        setDesc.setIndex = 0;
        setDesc.descriptorSet = bindingSet->GetHandle();
        setDesc.bindPoint = nri::BindPoint::GRAPHICS;
        nri.CmdSetDescriptorSet(*cmd, setDesc);

        nri::VertexBufferDesc vbDesc = {};
        vbDesc.buffer = m_VertexBuffer->GetHandle();
        vbDesc.offset = 0;
        vbDesc.stride = sizeof(glm::vec3);
        nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);
        nri.CmdSetIndexBuffer(*cmd, *m_IndexBuffer->GetHandle(), 0, nri::IndexType::UINT32);

        nri::DrawIndexedDesc drawDesc = {};
        drawDesc.indexNum = 36;
        drawDesc.instanceNum = 1;
        nri.CmdDrawIndexed(*cmd, drawDesc);

        if (startedRendering)
            rt->EndRendering(cmd);
    }

    void Environment::Draw(nri::CommandBuffer *cmd, RenderTarget *rt, const Ref<GraphicsPipeline> &pipeline,
        const Ref<ConstantBuffer> &cameraBuffer, const Ref<ConstantBuffer> &sceneBuffer)
    {
        if (!cameraBuffer || !sceneBuffer)
            return;
        Draw(cmd, rt, pipeline, *cameraBuffer, *sceneBuffer);
    }

    void Environment::LoadTexture(const std::string& filepath)
    {
        TextureCreateInfo textureCI;
        textureCI.dimension = nri::TextureType::TEXTURE_2D;
        textureCI.format = nri::Format::RGBA32_SFLOAT;
        textureCI.flip = true;
        textureCI.keepInitialState = true;
        textureCI.initialLayout = nri::Layout::SHADER_RESOURCE;
        textureCI.initialAccess = nri::AccessBits::SHADER_RESOURCE;
        m_HDRTexture = Texture::Create(filepath, textureCI, nullptr, "Environment HDR");
    }

    void Environment::SetTexture(const Ref<Texture> &texture)
    {
        m_HDRTexture = texture ? texture : Renderer::GetBlackTexture();
    }

    void Environment::WriteBuffer(nri::CommandBuffer *cmd)
    {
        // write buffers
        if (cmd)
        {
            m_VertexBuffer->SetData(cmd, m_Vertices.data(), sizeof(glm::vec3) * m_Vertices.size());
            m_IndexBuffer->SetData(cmd, m_Indices.data(), sizeof(uint32_t) * m_Indices.size());
        }
        else
        {
            m_VertexBuffer->SetData(m_Vertices.data(), sizeof(glm::vec3) * m_Vertices.size());
            m_IndexBuffer->SetData(m_Indices.data(), sizeof(uint32_t) * m_Indices.size());
        }
    }

    Ref<Environment> Environment::Create()
    {
        return CreateRef<Environment>();
    }

    BindingLayoutDesc Environment::GetBindingLayoutDesc()
    {
        return BindingLayoutDesc()
            .SetVisibility(nri::StageBits::ALL)
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(0)) // camera
            .AddItem(BindingLayoutItem::VolatileConstantBuffer(1)) // scene
            .AddItem(BindingLayoutItem::TextureSRV(0)) // texture
            .AddItem(BindingLayoutItem::Sampler(0));
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
