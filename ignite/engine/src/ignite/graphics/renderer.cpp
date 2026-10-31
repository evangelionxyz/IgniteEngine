// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "renderer.hpp"
#include "renderer/renderer_2d.hpp"
#include "texture.hpp"
#include "shader.hpp"
#include "binding_cache.hpp"
#include "bindless_system.hpp"

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/serializer/binary_serializer.hpp"


namespace ignite
{
    static Renderer *s_RendererInstance = nullptr;

    RendererStats Renderer::Stats;

    Renderer::Renderer(DeviceManager *deviceManager, nri::GraphicsAPI api)
    {
        s_RendererInstance = this;

        m_GraphicsAPI = api;

        nri::Device *device = deviceManager->GetDevice();

        BindlessSystem::Initialize(device);
        BindingCache::Init(device);

        Shader::InitShaderData();

        // Create binding layouts
        m_BindingLayouts[EBindingLayout::MESH_STATIC] = VertexMeshStatic::GetBindingLayoutDesc();
        m_BindingLayouts[EBindingLayout::MESH_ANIM] = VertexMeshAnim::GetBindingLayoutDesc();
        m_BindingLayouts[EBindingLayout::ENVIRONMENT] = Environment::GetBindingLayoutDesc();
        m_BindingLayouts[EBindingLayout::MATERIAL] = Material::GetBindingLayoutDesc();

        for (const auto &[type, layoutDesc] : m_BindingLayouts)
        {
            BindingLayoutDesc set0Layout = layoutDesc;
            set0Layout.SetRegisterSpace(0);
            m_PipelineLayouts[type] = set0Layout.CreatePipelineLayout(nri::StageBits::ALL, device);
        }

        // Default textures (Texture::Create with nullptr cmd uses NRI HelperInterface::UploadData synchronously)
        {
            TextureCreateInfo textureCreateInfo;
            textureCreateInfo.format = nri::Format::RGBA8_UNORM;
            textureCreateInfo.dimension = nri::TextureType::TEXTURE_2D;
            textureCreateInfo.width = 1;
            textureCreateInfo.height = 1;
            textureCreateInfo.flip = false;
            textureCreateInfo.initialLayout = nri::Layout::SHADER_RESOURCE;

            size_t texSize = sizeof(uint32_t);
            uint32_t white = 0xFFFFFFFF;
            std::vector<uint8_t> whiteData(texSize);
            memcpy(whiteData.data(), &white, texSize);
            m_WhiteTexture = Texture::Create(whiteData, textureCreateInfo, nullptr);

            uint32_t black = 0x00000000;
            std::vector<uint8_t> blackData(texSize);
            memcpy(blackData.data(), &black, texSize);
            m_BlackTexture = Texture::Create(blackData, textureCreateInfo, nullptr);

            uint32_t magenta = 0xFFFF00FF;
            std::vector<uint8_t> magentaData(texSize);
            memcpy(magentaData.data(), &magenta, texSize);
            m_MagentaTexture = Texture::Create(magentaData, textureCreateInfo, nullptr);

            TextureCreateInfo uintTexCI;
            uintTexCI.format = nri::Format::R32_UINT;
            uintTexCI.dimension = nri::TextureType::TEXTURE_2D;
            uintTexCI.width = 1;
            uintTexCI.height = 1;
            uintTexCI.flip = false;
            uintTexCI.initialLayout = nri::Layout::SHADER_RESOURCE;

            uint32_t blackUInt = 0;
            std::vector<uint8_t> blackUIntData(sizeof(uint32_t));
            memcpy(blackUIntData.data(), &blackUInt, sizeof(uint32_t));
            m_BlackUIntTexture = Texture::Create(blackUIntData, uintTexCI, nullptr);
        }

        // Default materials
        {
            m_DefaultMaterial = CreateRef<Material>();
        }

        // Create frame contexts
        m_MaxFramesInFlight = deviceManager->GetDeviceParameters().maxFramesInFlight;
        m_Frames.clear();
        m_Frames.reserve(m_MaxFramesInFlight);
        for (uint32_t i = 0; i < m_MaxFramesInFlight; ++i)
        {
            m_Frames.emplace_back(device);
            m_Frames.back().InitializeBindingSets(device,
                m_BindingLayouts[EBindingLayout::MESH_STATIC],
                m_BindingLayouts[EBindingLayout::MESH_ANIM]);
        }
    }

    void Renderer::Shutdown()
    {
        LOG_WARN("[Renderer] Shutdown");

        Shader::ShutdownShaderData();
        Shader::s_DXCInstance.reset();

        for (auto &frame : m_Frames)
        {
            frame.Shutdown();
        }
        m_Frames.clear();

        m_WhiteTexture.reset();
        m_MagentaTexture.reset();
        m_BlackTexture.reset();
        m_BlackUIntTexture.reset();

        m_DefaultMaterial.reset();

        m_DefaultMeshes.clear();

        auto *dm = DeviceManager::GetInstance();
        if (dm && dm->GetDevice())
        {
            const auto &nri = dm->GetCoreInterface();
            for (auto &[type, layout] : m_PipelineLayouts)
            {
                if (layout)
                    nri.DestroyPipelineLayout(layout);
            }
        }
        m_PipelineLayouts.clear();
        m_BindingLayouts.clear();

        BindingCache::Shutdown();
        BindlessSystem::Shutdown();
    }

    void Renderer::ResetStatistics()
    {
        // 3D
        Stats.drawCallCount = 0;
        Stats.shadowDrawCallCount = 0;
        Stats.staticMeshCount = 0;
        Stats.skeletalMeshCount = 0;
        Stats.vertexCount3D = 0;
        Stats.indexCount3D = 0;

        // 2D
        Stats.quadCount = 0;
        Stats.lineCount = 0;
        Stats.circleCount = 0;
        Stats.textCount = 0;
        Stats.pointLight2dCount = 0;
    }

    FrameContext *Renderer::GetCurrentFrameContext()
    {
        const uint64_t frameIndex = s_RendererInstance->m_FrameCounter % s_RendererInstance->m_MaxFramesInFlight;
        return &s_RendererInstance->m_Frames[frameIndex];
    }

    void Renderer::BeginFrame(const uint64_t frameCounter)
    {
        ResetStatistics();

        m_FrameCounter = frameCounter;

        BindlessSystem::FlushPendingWrites();

        auto &frame = m_Frames[frameCounter % m_MaxFramesInFlight];
        frame.frameIndexInFlight = static_cast<uint32_t>(frameCounter % m_MaxFramesInFlight);
        frame.objectAllocator.BeginFrame();
        frame.objectAllocator.SetBuffer(frame.objectBuffer);
        frame.boneAllocator.BeginFrame();
        frame.boneAllocator.SetBuffer(frame.boneBuffer);
        frame.instanceIndexAllocator.BeginFrame();
        frame.instanceIndexAllocator.SetBuffer(frame.instanceIndexBuffer);
    }

    nri::GraphicsAPI Renderer::GetGraphicsAPI()
    {
        return s_RendererInstance->m_GraphicsAPI;
    }

    const BindingLayoutDesc &Renderer::GetBindingLayout(EBindingLayout type)
    {
        static const BindingLayoutDesc s_EmptyLayout = {};
        if (s_RendererInstance && s_RendererInstance->m_BindingLayouts.contains(type))
            return s_RendererInstance->m_BindingLayouts[type];

        return s_EmptyLayout;
    }

    nri::PipelineLayout *Renderer::GetPipelineLayout(EBindingLayout type)
    {
        if (s_RendererInstance && s_RendererInstance->m_PipelineLayouts.contains(type))
            return s_RendererInstance->m_PipelineLayouts[type];

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

    Ref<Texture> Renderer::GetBlackUIntTexture()
    {
        return s_RendererInstance->m_BlackUIntTexture;
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

    static std::mutex s_StatsMutex;

    void Renderer::IncrementPipelineCount(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(s_StatsMutex);
        Stats.pipelineCounts[name]++;
    }

    void Renderer::DecrementPipelineCount(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(s_StatsMutex);
        auto it = Stats.pipelineCounts.find(name);
        if (it != Stats.pipelineCounts.end())
        {
            if (it->second > 0)
                it->second--;
            if (it->second == 0)
                Stats.pipelineCounts.erase(it);
        }
    }

    void Renderer::IncrementBindingSetCount(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(s_StatsMutex);
        Stats.bindingSetCounts[name]++;
    }

    void Renderer::DecrementBindingSetCount(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(s_StatsMutex);
        auto it = Stats.bindingSetCounts.find(name);
        if (it != Stats.bindingSetCounts.end())
        {
            if (it->second > 0)
                it->second--;
            if (it->second == 0)
                Stats.bindingSetCounts.erase(it);
        }
    }
}
