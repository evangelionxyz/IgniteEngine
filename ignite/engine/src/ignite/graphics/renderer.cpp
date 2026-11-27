// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "renderer.hpp"
#include "renderer/renderer_2d.hpp"
#include "texture.hpp"
#include "font.hpp"
#include "shader.hpp"
#include "binding_cache.hpp"
#include "bindless_system.hpp"

#include "ignite/graphics/buffers/constant_buffer.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/mesh_builder.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/environment.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include <algorithm>
#include <cctype>


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

            // Immediately bind slot 0 fallback and default textures in the bindless heap
            // so slot 0 is guaranteed valid even before the first Renderer::BeginFrame().
            BindlessSystem::FlushPendingWrites();
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
        m_DefaultFont.reset();

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
        BindingSet::ShutdownPool();
        Texture::ClearRetiredStagingBuffers(0, true);
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

        BindingSet::AdvanceFrame(frameCounter);
        BindlessSystem::FlushPendingWrites();
        Renderer2D::ClearRetiredBuffers(frameCounter);
        Texture::ClearRetiredStagingBuffers(frameCounter);

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

    static std::mutex s_DefaultFontMutex;

    Ref<Font> Renderer::GetDefaultFontIfCreated()
    {
        if (!s_RendererInstance)
            return nullptr;

        std::lock_guard<std::mutex> lock(s_DefaultFontMutex);
        return s_RendererInstance->m_DefaultFont;
    }

    Ref<Font> Renderer::GetDefaultFont()
    {
        if (!s_RendererInstance)
            return nullptr;

        std::lock_guard<std::mutex> lock(s_DefaultFontMutex);
        if (!s_RendererInstance->m_DefaultFont)
        {
            static constexpr const char *kDefaultFontCandidates[] = {
                "example/Assets/Fonts/JetBrainsMono-Bold.ttf",
                "Assets/Fonts/JetBrainsMono-Bold.ttf",
                "resources/fonts/segoeui.ttf",
                "ignite/editor/resources/fonts/segoeui.ttf",
                "C:/Windows/Fonts/consolab.ttf",
                "C:/Windows/Fonts/segoeuib.ttf",
                "C:/Windows/Fonts/segoeui.ttf"
            };

            std::error_code ec;
            for (const char *candidate : kDefaultFontCandidates)
            {
                if (std::filesystem::exists(candidate, ec))
                {
                    s_RendererInstance->m_DefaultFont = Font::Create(candidate, true);
                    break;
                }
            }
        }

        return s_RendererInstance->m_DefaultFont;
    }

    static constexpr uint64_t kDefaultMeshHandleBase = 0x49474E5F4D455348ULL; // "IGN_MESH"
    static std::mutex s_DefaultMeshMutex;

    static Ref<StaticMesh> BuildDefaultMesh(EMeshType type)
    {
        MeshBuilder mb;
        switch (type)
        {
            case EMeshType::CUBE:      mb = MeshBuilder::CreateCube(glm::vec3(1.0f)); break;
            case EMeshType::UV_SPHERE:
            case EMeshType::SPHERE:    mb = MeshBuilder::CreateSphere(0.5f, 24, 36); break;
            case EMeshType::ICOSPHERE: mb = MeshBuilder::CreateIcosphere(0.5f, 2); break;
            case EMeshType::CYLINDER:  mb = MeshBuilder::CreateCylinder(0.5f, 1.0f, 32, true); break;
            case EMeshType::CAPSULE:   mb = MeshBuilder::CreateCapsule(0.5f, 2.0f, 12, 24); break;
            case EMeshType::PLANE:     mb = MeshBuilder::CreatePlane(glm::vec2(10.0f), 1, 1); break;
            case EMeshType::CONE:      mb = MeshBuilder::CreateCone(0.5f, 1.0f, 32, true); break;
            case EMeshType::TORUS:     mb = MeshBuilder::CreateTorus(0.5f, 0.15f, 32, 16); break;
            default:                   mb = MeshBuilder::CreateCube(glm::vec3(1.0f)); break;
        }

        const bool hasDevice = (DeviceManager::GetInstance() && DeviceManager::GetInstance()->GetDevice() != nullptr);
        Ref<StaticMesh> mesh = mb.BuildStaticMesh(Renderer::GetDefaultMeshName(type), AssetHandle(0), hasDevice);
        if (mesh)
        {
            mesh->handle = Renderer::GetDefaultMeshHandle(type);
        }
        return mesh;
    }

    Ref<StaticMesh> Renderer::GetDefaultMesh(EMeshType type)
    {
        if (!s_RendererInstance)
        {
            return BuildDefaultMesh(type);
        }

        std::lock_guard<std::mutex> lock(s_DefaultMeshMutex);
        auto it = s_RendererInstance->m_DefaultMeshes.find(type);
        if (it != s_RendererInstance->m_DefaultMeshes.end() && it->second)
        {
            return it->second;
        }

        Ref<StaticMesh> mesh = BuildDefaultMesh(type);
        s_RendererInstance->m_DefaultMeshes[type] = mesh;
        return mesh;
    }

    AssetHandle Renderer::GetDefaultMeshHandle(EMeshType type)
    {
        return AssetHandle(kDefaultMeshHandleBase + static_cast<uint64_t>(type));
    }

    bool Renderer::IsDefaultMeshHandle(AssetHandle handle, EMeshType *outType)
    {
        const uint64_t raw = static_cast<uint64_t>(handle);
        const uint64_t minVal = kDefaultMeshHandleBase;
        const uint64_t maxVal = kDefaultMeshHandleBase + static_cast<uint64_t>(EMeshType::TORUS);
        if (raw >= minVal && raw <= maxVal)
        {
            if (outType)
            {
                *outType = static_cast<EMeshType>(raw - kDefaultMeshHandleBase);
            }
            return true;
        }
        return false;
    }

    Ref<StaticMesh> Renderer::GetDefaultMeshByHandle(AssetHandle handle)
    {
        EMeshType type = EMeshType::CUBE;
        if (IsDefaultMeshHandle(handle, &type))
        {
            return GetDefaultMesh(type);
        }
        return nullptr;
    }

    const char *Renderer::GetDefaultMeshName(EMeshType type)
    {
        switch (type)
        {
            case EMeshType::UV_SPHERE: return "UVSphere";
            case EMeshType::CUBE:      return "Cube";
            case EMeshType::SPHERE:    return "Sphere";
            case EMeshType::ICOSPHERE: return "Icosphere";
            case EMeshType::CYLINDER:  return "Cylinder";
            case EMeshType::CAPSULE:   return "Capsule";
            case EMeshType::PLANE:     return "Plane";
            case EMeshType::CONE:      return "Cone";
            case EMeshType::TORUS:     return "Torus";
            default:                   return "Primitive";
        }
    }

    bool Renderer::ParseMeshType(std::string_view name, EMeshType &outType)
    {
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower == "cube" || lower == "box") { outType = EMeshType::CUBE; return true; }
        if (lower == "sphere") { outType = EMeshType::SPHERE; return true; }
        if (lower == "uv_sphere" || lower == "uvsphere") { outType = EMeshType::UV_SPHERE; return true; }
        if (lower == "icosphere" || lower == "ico_sphere") { outType = EMeshType::ICOSPHERE; return true; }
        if (lower == "cylinder") { outType = EMeshType::CYLINDER; return true; }
        if (lower == "capsule") { outType = EMeshType::CAPSULE; return true; }
        if (lower == "plane" || lower == "quad") { outType = EMeshType::PLANE; return true; }
        if (lower == "cone") { outType = EMeshType::CONE; return true; }
        if (lower == "torus" || lower == "donut") { outType = EMeshType::TORUS; return true; }
        return false;
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
