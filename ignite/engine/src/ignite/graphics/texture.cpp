// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "texture.hpp"
#include "bindless_system.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/serializer.hpp"

#include <Extensions/NRIHelper.h>
#include <stb_image.h>

namespace ignite
{
    namespace texture_utils
    {
        static std::filesystem::path BuildMetaPath(Project *project, const AssetMetaData &metadata, const char *extension)
        {
            if (!project)
            {
                return {};
            }

            std::filesystem::path assetPath = project->GetProjectFilepath(metadata.filepath);
            assetPath += extension;
            return assetPath;
        }

        static bool IsDepthFormat(nri::Format format)
        {
            switch (format)
            {
            case nri::Format::D16_UNORM:
            case nri::Format::D32_SFLOAT:
            case nri::Format::D24_UNORM_S8_UINT:
            case nri::Format::D32_SFLOAT_S8_UINT:
                return true;
            default:
                return false;
            }
        }
    }

    TextureCreateInfo Texture::GetDefaultCreateInfo(const AssetMetaData &metadata)
    {
        TextureCreateInfo createInfo;
        const std::string extension = metadata.filepath.extension().string();
        const bool isHDR = extension == ".hdr";

        createInfo.format = isHDR ? nri::Format::RGBA32_SFLOAT : nri::Format::RGBA8_UNORM;
        createInfo.mipLevels = isHDR ? 1 : 4;
        createInfo.flip = isHDR;
        createInfo.initialAccess = nri::AccessBits::SHADER_RESOURCE;
        createInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfo.initialStages = nri::StageBits::ALL;
        createInfo.keepInitialState = true;
        createInfo.deferGpuCreate = true;

        return createInfo;
    }

    std::filesystem::path Texture::GetMetaPath(Project *project, const AssetMetaData &metadata)
    {
        return texture_utils::BuildMetaPath(project, metadata, ".meta");
    }

    std::filesystem::path Texture::GetLegacyMetaPath(Project *project, const AssetMetaData &metadata)
    {
        return texture_utils::BuildMetaPath(project, metadata, ".ixtex");
    }

    bool Texture::LoadCreateInfoFile(const std::filesystem::path &filepath, TextureCreateInfo &outCreateInfo)
    {
        if (filepath.empty() || !std::filesystem::exists(filepath))
        {
            return false;
        }

        JsonNode root = Serializer::Deserialize(filepath);
        JsonNode node = root["DATA"];
        if (!node)
        {
            node = root["TextureImportSettings"];
        }
        if (!node)
        {
            return false;
        }

        if (node["Width"]) outCreateInfo.width = node["Width"].as<uint32_t>();
        if (node["Height"]) outCreateInfo.height = node["Height"].as<uint32_t>();
        if (node["Depth"]) outCreateInfo.depth = node["Depth"].as<uint32_t>();
        if (node["MipLevels"]) outCreateInfo.mipLevels = node["MipLevels"].as<uint32_t>();
        if (node["ArraySize"]) outCreateInfo.arraySize = node["ArraySize"].as<uint32_t>();
        if (node["SampleCount"]) outCreateInfo.sampleCount = node["SampleCount"].as<uint32_t>();
        if (node["SampleQuality"]) outCreateInfo.sampleQuality = node["SampleQuality"].as<uint32_t>();

        if (node["Flip"]) outCreateInfo.flip = node["Flip"].as<bool>();
        if (node["IsRenderTarget"]) outCreateInfo.isRenderTarget = node["IsRenderTarget"].as<bool>();
        if (node["IsTypeless"]) outCreateInfo.isTypeless = node["IsTypeless"].as<bool>();
        if (node["IsUAV"]) outCreateInfo.isUAV = node["IsUAV"].as<bool>();
        if (node["IsShadingRateSurface"]) outCreateInfo.isShadingRateSurface = node["IsShadingRateSurface"].as<bool>();
        if (node["KeepCpuData"]) outCreateInfo.keepCpuData = node["KeepCpuData"].as<bool>();
        if (node["DeferGpuCreate"]) outCreateInfo.deferGpuCreate = node["DeferGpuCreate"].as<bool>();
        if (node["KeepInitialState"]) outCreateInfo.keepInitialState = node["KeepInitialState"].as<bool>();
        if (node["SamplerLinearFiltering"]) outCreateInfo.samplerLinearFiltering = node["SamplerLinearFiltering"].as<bool>();

        if (node["Format"]) outCreateInfo.format = static_cast<nri::Format>(node["Format"].as<uint32_t>());
        if (node["InitialState"]) outCreateInfo.initialLayout = static_cast<nri::Layout>(node["InitialState"].as<uint32_t>());
        if (node["Dimension"]) outCreateInfo.dimension = static_cast<nri::TextureType>(node["Dimension"].as<uint32_t>());
        if (node["SamplerAddressU"]) outCreateInfo.samplerAddressU = static_cast<nri::AddressMode>(node["SamplerAddressU"].as<uint32_t>());
        if (node["SamplerAddressV"]) outCreateInfo.samplerAddressV = static_cast<nri::AddressMode>(node["SamplerAddressV"].as<uint32_t>());
        if (node["SamplerAddressW"]) outCreateInfo.samplerAddressW = static_cast<nri::AddressMode>(node["SamplerAddressW"].as<uint32_t>());

        return true;
    }

    bool Texture::SerializeMetaFile(const std::filesystem::path &filepath, AssetHandle handle, const TextureCreateInfo &createInfo)
    {
        if (filepath.empty())
        {
            return false;
        }

        Serializer sr(filepath);
        sr.BeginMap();
        sr.AddKeyValue("Version", Application::GetVersion());
        sr.AddKeyValue("ASSET_HANDLE", static_cast<uint64_t>(handle));
        sr.AddKeyValue("ASSET_TYPE", AssetTypeToString(AssetType::Texture));

        sr.BeginMap("DATA");
        sr.AddKeyValue("Width", createInfo.width);
        sr.AddKeyValue("Height", createInfo.height);
        sr.AddKeyValue("Depth", createInfo.depth);
        sr.AddKeyValue("MipLevels", createInfo.mipLevels);
        sr.AddKeyValue("ArraySize", createInfo.arraySize);
        sr.AddKeyValue("SampleCount", createInfo.sampleCount);
        sr.AddKeyValue("SampleQuality", createInfo.sampleQuality);

        sr.AddKeyValue("Flip", createInfo.flip);
        sr.AddKeyValue("IsRenderTarget", createInfo.isRenderTarget);
        sr.AddKeyValue("IsTypeless", createInfo.isTypeless);
        sr.AddKeyValue("IsUAV", createInfo.isUAV);
        sr.AddKeyValue("IsShadingRateSurface", createInfo.isShadingRateSurface);
        sr.AddKeyValue("KeepCpuData", createInfo.keepCpuData);
        sr.AddKeyValue("DeferGpuCreate", createInfo.deferGpuCreate);
        sr.AddKeyValue("KeepInitialState", createInfo.keepInitialState);
        sr.AddKeyValue("SamplerLinearFiltering", createInfo.samplerLinearFiltering);

        sr.AddKeyValue("Format", static_cast<uint32_t>(createInfo.format));
        sr.AddKeyValue("InitialState", static_cast<uint32_t>(createInfo.initialLayout));
        sr.AddKeyValue("Dimension", static_cast<uint32_t>(createInfo.dimension));
        sr.AddKeyValue("SamplerAddressU", static_cast<uint32_t>(createInfo.samplerAddressU));
        sr.AddKeyValue("SamplerAddressV", static_cast<uint32_t>(createInfo.samplerAddressV));
        sr.AddKeyValue("SamplerAddressW", static_cast<uint32_t>(createInfo.samplerAddressW));
        sr.EndMap();

        sr.EndMap();
        sr.Serialize();
        return true;
    }

    size_t Texture::GetApproxSizeBytes() const
    {
        const size_t bytesPerPixel =
            (m_CreateInfo.format == nri::Format::RGBA32_SFLOAT) ? sizeof(float) * 4u : 4u;

        const size_t width = static_cast<size_t>(std::max(m_CreateInfo.width, 1u));
        const size_t height = static_cast<size_t>(std::max(m_CreateInfo.height, 1u));
        const size_t depth = static_cast<size_t>(std::max(m_CreateInfo.depth, 1u));
        const size_t arraySize = static_cast<size_t>(std::max(m_CreateInfo.arraySize, 1u));
        const size_t sampleCount = static_cast<size_t>(std::max(m_CreateInfo.sampleCount, 1u));
        const size_t mipLevels = static_cast<size_t>(std::max(m_CreateInfo.mipLevels, 1u));

        size_t total = 0;
        size_t mipWidth = width;
        size_t mipHeight = height;
        size_t mipDepth = depth;
        for (size_t mip = 0; mip < mipLevels; ++mip)
        {
            total += mipWidth * mipHeight * mipDepth * bytesPerPixel;
            mipWidth = std::max<size_t>(1, mipWidth / 2);
            mipHeight = std::max<size_t>(1, mipHeight / 2);
            mipDepth = std::max<size_t>(1, mipDepth / 2);
        }

        return total * arraySize * sampleCount;
    }

    Texture::Texture(TextureCreateInfo createInfo, const std::string &debugName)
        : m_CreateInfo(createInfo), m_DebugName(debugName),
          m_CurrentAccess(createInfo.initialAccess),
          m_CurrentLayout(createInfo.initialLayout),
          m_CurrentStages(createInfo.initialStages)
    {
        m_Ready = true;
        if (!m_CreateInfo.deferGpuCreate)
        {
            CreateTextureHandle();
        }
    }

    Texture::Texture(const std::vector<uint8_t> &data, TextureCreateInfo createInfo, nri::CommandBuffer *cmd, const std::string &debugName)
        : m_Buffer(data), m_CreateInfo(createInfo), m_DebugName(debugName),
          m_CurrentAccess(createInfo.initialAccess),
          m_CurrentLayout(createInfo.initialLayout),
          m_CurrentStages(createInfo.initialStages)
    {
        m_Ready = (cmd != nullptr);
        if (!m_CreateInfo.deferGpuCreate)
        {
            CreateTextureHandle();

            const uint32_t channels = 4;
            const uint32_t rowPitch = m_CreateInfo.width * channels;
            uint32_t depthPitch = rowPitch * m_CreateInfo.height;

            SetData(cmd, rowPitch, depthPitch);
            m_Ready = true;
        }
    }

    Texture::Texture(const std::filesystem::path &filepath, TextureCreateInfo createInfo, nri::CommandBuffer *cmd, const std::string &debugName)
        : m_CreateInfo(createInfo), m_Filepath(filepath), m_DebugName(debugName),
          m_CurrentAccess(createInfo.initialAccess),
          m_CurrentLayout(createInfo.initialLayout),
          m_CurrentStages(createInfo.initialStages)
    {
        m_Ready = (cmd != nullptr);
        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("[Texture] File does not found! {}", filepath.generic_string());
            return;
        }

        if (texture_utils::IsExrFile(filepath))
        {
            if (m_CreateInfo.format == nri::Format::UNKNOWN)
            {
                m_CreateInfo.format = nri::Format::RGBA32_SFLOAT;
            }

            int w = 0, h = 0;
            if (!texture_utils::LoadEXRTexture(filepath, w, h, m_CreateInfo.format, m_Buffer))
            {
                LOG_ERROR("[Texture] Failed to load EXR texture {}", filepath.generic_string());
                return;
            }

            m_CreateInfo.width = static_cast<uint32_t>(w);
            m_CreateInfo.height = static_cast<uint32_t>(h);

            if (!m_CreateInfo.deferGpuCreate)
            {
                CreateTextureHandle();

                const uint32_t channelCount = 4;
                const uint32_t rowPitch = m_CreateInfo.width * channelCount;
                const uint32_t depthPitch = rowPitch * m_CreateInfo.height;
                SetData(cmd, rowPitch, depthPitch);
                m_Ready = true;
            }

            return;
        }

        // always use RGBA
        const int channels = 4;

        switch (m_CreateInfo.format)
        {
            case nri::Format::RGBA8_UNORM:
            case nri::Format::RGBA8_SRGB:
            {
                int width, height, channelsOut;
                uint8_t *pixelData = stbi_load(filepath.generic_string().c_str(), &width, &height, &channelsOut, channels);
                const uint64_t dataSize = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * channels;
                m_Buffer.resize(dataSize);
                memcpy(m_Buffer.data(), pixelData, dataSize);
                stbi_image_free(pixelData);

                m_CreateInfo.width = static_cast<uint32_t>(width);
                m_CreateInfo.height = static_cast<uint32_t>(height);
                LOG_ASSERT(m_Buffer.data(), "Failed to load texture data");
                break;
            }
            case nri::Format::RGBA32_SFLOAT:
            {
                int width, height, channelsOut;
                float *pixelData = stbi_loadf(filepath.generic_string().c_str(), &width, &height, &channelsOut, channels);
                const uint64_t dataSize = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * channels * sizeof(float);
                m_Buffer.resize(dataSize);
                memcpy(m_Buffer.data(), pixelData, dataSize);
                stbi_image_free(pixelData);

                m_CreateInfo.width = static_cast<uint32_t>(width);
                m_CreateInfo.height = static_cast<uint32_t>(height);
                LOG_ASSERT(m_Buffer.data(), "Failed to load texture data");
                break;
            }
            default:
            {
                LOG_ASSERT(false, "[Texture] Please specify format explicitly!");
                return;
            }
        }

        if (!m_CreateInfo.deferGpuCreate)
        {
            CreateTextureHandle();

            const uint32_t rowPitch = m_CreateInfo.width * channels;
            const uint32_t depthPitch = rowPitch * m_CreateInfo.height;
            SetData(cmd, rowPitch, depthPitch);
            m_Ready = true;
        }
    }

    Texture::~Texture()
    {
        IGN_PROFILE_FUNCTION();

        if (m_BindlessIndex != 0xFFFFFFFF)
        {
            BindlessSystem::UnregisterTexture(m_BindlessIndex);
            m_BindlessIndex = 0xFFFFFFFF;
        }

        if (m_Handle && m_TracyAllocationTracked)
        {
            IGN_PROFILE_FREE_N(m_Handle, "GPU Texture");
            m_TracyAllocationTracked = false;
        }

        auto *deviceManager = DeviceManager::GetInstance();
        if (deviceManager)
        {
            const auto &nri = deviceManager->GetCoreInterface();
            for (nri::Descriptor *mipSrv : m_MipSRVs)
            {
                if (mipSrv) nri.DestroyDescriptor(mipSrv);
            }
            for (nri::Descriptor *uav : m_StorageViews)
            {
                if (uav) nri.DestroyDescriptor(uav);
            }
            if (m_ShaderResourceView)
            {
                nri.DestroyDescriptor(m_ShaderResourceView);
            }
            if (m_Sampler)
            {
                nri.DestroyDescriptor(m_Sampler);
            }
            if (m_StagingBuffer)
            {
                nri.DestroyBuffer(m_StagingBuffer);
            }
            if (m_Handle && !m_CreateInfo.isNativeObject)
            {
                nri.DestroyTexture(m_Handle);
            }
        }

        m_MipSRVs.clear();
        m_StorageViews.clear();
        m_ShaderResourceView = nullptr;
        m_Sampler = nullptr;
        m_StagingBuffer = nullptr;
        m_Handle = nullptr;
        m_Buffer.clear();
    }

    void Texture::PrepareUploadData(uint32_t rowPitch, uint32_t depthPitch)
    {
        IGN_PROFILE_FUNCTION();
        (void)depthPitch;
        if (!m_Buffer.data())
        {
            return;
        }

        if (m_UploadDataPrepared)
        {
            return;
        }

        if (m_CreateInfo.width > 0 && m_CreateInfo.height > 0)
        {
            const uint32_t maxMips = CalculateMaxMipLevels(m_CreateInfo.width, m_CreateInfo.height, m_CreateInfo.depth);
            m_CreateInfo.mipLevels = (m_CreateInfo.mipLevels == 0) ? 1u : std::min(m_CreateInfo.mipLevels, maxMips);
        }

        if (m_CreateInfo.format == nri::Format::RGBA8_UNORM || m_CreateInfo.format == nri::Format::RGBA8_SRGB)
        {
            if (m_CreateInfo.flip)
            {
                texture_utils::FlipImageBuffer(m_Buffer, m_CreateInfo.width, m_CreateInfo.height, rowPitch);
            }

            uint8_t *byteData = static_cast<uint8_t *>(m_Buffer.data());
            m_PreparedMipChain = CPUMipGenerator::GenerateMipChain(byteData,
                m_CreateInfo.width, m_CreateInfo.height, rowPitch,
                m_CreateInfo.format, m_CreateInfo.mipLevels);
            m_UploadDataPrepared = true;
        }
        else if (m_CreateInfo.format == nri::Format::RGBA32_SFLOAT)
        {
            if (m_CreateInfo.flip)
            {
                texture_utils::FlipImageBuffer(m_Buffer, m_CreateInfo.width, m_CreateInfo.height, rowPitch * sizeof(float));
            }

            float *floatData = reinterpret_cast<float *>(m_Buffer.data());
            m_PreparedMipChain = CPUMipGenerator::GenerateMipChain(floatData,
                m_CreateInfo.width, m_CreateInfo.height, rowPitch * sizeof(float),
                m_CreateInfo.format, m_CreateInfo.mipLevels);
            m_UploadDataPrepared = true;
        }
        else
        {
            MipLevelData baseMip;
            baseMip.width = m_CreateInfo.width;
            baseMip.height = m_CreateInfo.height;
            baseMip.rowPitch = rowPitch;
            baseMip.slicePitch = rowPitch * m_CreateInfo.height;
            baseMip.data = m_Buffer;
            m_PreparedMipChain.clear();
            m_PreparedMipChain.push_back(std::move(baseMip));
            m_UploadDataPrepared = true;
        }
    }

    void Texture::PrepareUploadData(uint32_t channelCount)
    {
        const uint32_t rowPitch = m_CreateInfo.width * channelCount;
        const uint32_t depthPitch = rowPitch * m_CreateInfo.height;
        PrepareUploadData(rowPitch, depthPitch);
    }

    void Texture::SetData(nri::CommandBuffer *cmd, uint32_t rowPitch, uint32_t depthPitch)
    {
        IGN_PROFILE_FUNCTION();
        (void)depthPitch;
        if (!m_Buffer.data())
        {
            return;
        }

        EnsureTextureHandle();
        PrepareUploadData(rowPitch, depthPitch);

        auto *deviceManager = DeviceManager::GetInstance();
        if (!deviceManager || !m_Handle || m_PreparedMipChain.empty())
        {
            return;
        }

        const auto &nri = deviceManager->GetCoreInterface();
        const uint32_t mipCount = std::min<uint32_t>(m_CreateInfo.mipLevels, static_cast<uint32_t>(m_PreparedMipChain.size()));

        if (cmd != nullptr)
        {
            if (m_StagingBuffer)
            {
                nri.DestroyBuffer(m_StagingBuffer);
                m_StagingBuffer = nullptr;
            }

            struct MipPlacement
            {
                uint64_t offset = 0;
                uint32_t alignedRowPitch = 0;
                uint32_t alignedSlicePitch = 0;
            };
            std::vector<MipPlacement> placements(mipCount);
            uint64_t totalSize = 0;

            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                const auto &mipData = m_PreparedMipChain[mip];
                totalSize = (totalSize + 511ULL) & ~511ULL;
                placements[mip].offset = totalSize;
                placements[mip].alignedRowPitch = (mipData.rowPitch + 255u) & ~255u;
                placements[mip].alignedSlicePitch = placements[mip].alignedRowPitch * mipData.height;
                totalSize += placements[mip].alignedSlicePitch;
            }

            nri::BufferDesc stagingDesc = {};
            stagingDesc.size = totalSize;
            stagingDesc.usage = nri::BufferUsageBits::NONE;

            nri::Result res = nri.CreateCommittedBuffer(*deviceManager->GetDevice(), nri::MemoryLocation::HOST_UPLOAD, 1.0f, stagingDesc, m_StagingBuffer);
            LOG_ASSERT(res == nri::Result::SUCCESS && m_StagingBuffer, "[Texture] Failed to create staging buffer!");

            if (m_StagingBuffer)
            {
                uint8_t *mapped = static_cast<uint8_t *>(nri.MapBuffer(*m_StagingBuffer, 0, totalSize));
                if (mapped)
                {
                    for (uint32_t mip = 0; mip < mipCount; ++mip)
                    {
                        const auto &mipData = m_PreparedMipChain[mip];
                        const auto &placement = placements[mip];
                        uint8_t *dstMipPtr = mapped + placement.offset;
                        for (uint32_t row = 0; row < mipData.height; ++row)
                        {
                            memcpy(dstMipPtr + row * placement.alignedRowPitch,
                                mipData.data.data() + row * mipData.rowPitch,
                                mipData.rowPitch);
                        }
                    }
                    nri.UnmapBuffer(*m_StagingBuffer);
                }

                nri::TextureBarrierDesc preBarrier = {};
                preBarrier.texture = m_Handle;
                preBarrier.before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
                preBarrier.after = { nri::AccessBits::COPY_DESTINATION, nri::Layout::COPY_DESTINATION, nri::StageBits::COPY };
                preBarrier.mipOffset = 0;
                preBarrier.mipNum = nri::REMAINING;
                preBarrier.layerOffset = 0;
                preBarrier.layerNum = nri::REMAINING;
                preBarrier.planes = nri::PlaneBits::ALL;

                nri::BarrierDesc barrierDesc = {};
                barrierDesc.textures = &preBarrier;
                barrierDesc.textureNum = 1;
                nri.CmdBarrier(*cmd, barrierDesc);

                for (uint32_t mip = 0; mip < mipCount; ++mip)
                {
                    const auto &mipData = m_PreparedMipChain[mip];
                    const auto &placement = placements[mip];

                    nri::TextureRegionDesc dstRegion = {};
                    dstRegion.x = 0;
                    dstRegion.y = 0;
                    dstRegion.z = 0;
                    dstRegion.width = static_cast<nri::Dim_t>(mipData.width);
                    dstRegion.height = static_cast<nri::Dim_t>(mipData.height);
                    dstRegion.depth = 1;
                    dstRegion.mipOffset = static_cast<nri::Dim_t>(mip);
                    dstRegion.layerOffset = 0;
                    dstRegion.planes = nri::PlaneBits::ALL;

                    nri::TextureDataLayoutDesc srcLayout = {};
                    srcLayout.offset = placement.offset;
                    srcLayout.rowPitch = placement.alignedRowPitch;
                    srcLayout.slicePitch = placement.alignedSlicePitch;

                    nri.CmdUploadBufferToTexture(*cmd, *m_Handle, dstRegion, *m_StagingBuffer, srcLayout);
                }

                nri::TextureBarrierDesc postBarrier = preBarrier;
                postBarrier.before = preBarrier.after;
                postBarrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                barrierDesc.textures = &postBarrier;
                nri.CmdBarrier(*cmd, barrierDesc);

                SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
            }
        }
        else
        {
            std::vector<nri::TextureSubresourceUploadDesc> subresources(mipCount);
            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                const auto &mipData = m_PreparedMipChain[mip];
                subresources[mip].slices = mipData.data.data();
                subresources[mip].sliceNum = 1;
                subresources[mip].rowPitch = mipData.rowPitch;
                subresources[mip].slicePitch = mipData.slicePitch;
            }

            nri::TextureUploadDesc uploadDesc = {};
            uploadDesc.subresources = subresources.data();
            uploadDesc.texture = m_Handle;
            uploadDesc.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE };
            uploadDesc.planes = nri::PlaneBits::ALL;

            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
            deviceManager->GetHelperInterface().UploadData(*deviceManager->GetGraphicsQueue(), &uploadDesc, 1, nullptr, 0);

            SetCurrentState(nri::Layout::SHADER_RESOURCE, nri::AccessBits::SHADER_RESOURCE, nri::StageBits::ALL);
        }

        if (!m_CreateInfo.keepCpuData)
        {
            std::vector<MipLevelData>().swap(m_PreparedMipChain);
            m_UploadDataPrepared = false;
            m_Buffer.clear();
        }

        m_Ready = true;
    }

    void Texture::SetData(nri::CommandBuffer *cmd, uint32_t channelCount)
    {
        const uint32_t rowPitch = m_CreateInfo.width * channelCount;
        const uint32_t depthPitch = rowPitch * m_CreateInfo.height;
        SetData(cmd, rowPitch, depthPitch);
    }

    uint32_t Texture::CalculateMaxMipLevels(uint32_t width, uint32_t height, uint32_t depth)
    {
        const uint32_t maxDim = std::max({ width, height, depth, 1u });
        return 1u + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(maxDim))));
    }

    void Texture::CreateTextureHandle()
    {
        IGN_PROFILE_FUNCTION();
        if (m_Handle)
        {
            return;
        }

        if (m_CreateInfo.sampleCount > 1)
        {
            m_CreateInfo.mipLevels = 1;
        }
        else if (m_CreateInfo.width > 0 && m_CreateInfo.height > 0)
        {
            const uint32_t maxMips = CalculateMaxMipLevels(m_CreateInfo.width, m_CreateInfo.height, m_CreateInfo.depth);
            m_CreateInfo.mipLevels = (m_CreateInfo.mipLevels == 0) ? 1u : std::min(m_CreateInfo.mipLevels, maxMips);
        }

        auto *deviceManager = DeviceManager::GetInstance();
        if (!deviceManager)
        {
            return;
        }

        nri::Device *device = deviceManager->GetDevice();
        const auto &nri = deviceManager->GetCoreInterface();

        const bool isDepth = texture_utils::IsDepthFormat(m_CreateInfo.format);

        nri::TextureUsageBits usage = nri::TextureUsageBits::SHADER_RESOURCE;
        if (m_CreateInfo.isUAV)
        {
            usage |= nri::TextureUsageBits::SHADER_RESOURCE_STORAGE;
        }
        if (m_CreateInfo.isRenderTarget)
        {
            usage |= isDepth ? nri::TextureUsageBits::DEPTH_STENCIL_ATTACHMENT : nri::TextureUsageBits::COLOR_ATTACHMENT;
        }
        if (m_CreateInfo.isShadingRateSurface)
        {
            usage |= nri::TextureUsageBits::SHADING_RATE_ATTACHMENT;
        }

        nri::TextureDesc textureDesc = {};
        textureDesc.type = m_CreateInfo.dimension;
        textureDesc.usage = usage;
        textureDesc.format = m_CreateInfo.format;
        textureDesc.width = static_cast<nri::Dim_t>(m_CreateInfo.width);
        textureDesc.height = static_cast<nri::Dim_t>(m_CreateInfo.height);
        textureDesc.depth = static_cast<nri::Dim_t>(m_CreateInfo.depth);
        textureDesc.mipNum = static_cast<nri::Dim_t>(m_CreateInfo.mipLevels);
        textureDesc.layerNum = static_cast<nri::Dim_t>(m_CreateInfo.arraySize);
        textureDesc.sampleNum = static_cast<nri::Sample_t>(m_CreateInfo.sampleCount);

        if (m_CreateInfo.isNativeObject)
        {
            m_Handle = static_cast<nri::Texture *>(m_CreateInfo.nativeObjectPtr);
        }
        else
        {
            nri::Result res = nri.CreateCommittedTexture(*device, nri::MemoryLocation::DEVICE, 1.0f, textureDesc, m_Handle);
            LOG_ASSERT(res == nri::Result::SUCCESS && m_Handle, "Failed to create texture");
        }

        if (m_Handle)
        {
            m_CurrentAccess = m_CreateInfo.initialAccess;
            m_CurrentLayout = m_CreateInfo.initialLayout;
            m_CurrentStages = m_CreateInfo.initialStages;

            if (!m_CreateInfo.isNativeObject)
            {
                nri.SetDebugName((nri::Object *)m_Handle, m_DebugName.c_str());
            }

            IGN_PROFILE_ALLOC_N(m_Handle, GetApproxSizeBytes(), "GPU Texture");
            m_TracyAllocationTracked = true;

            const auto &desc = nri.GetTextureDesc(*m_Handle);
            if ((desc.usage & nri::TextureUsageBits::SHADER_RESOURCE) != 0)
            {
                // Create full-chain SRV
                nri::TextureViewDesc srvDesc = {};
                srvDesc.texture = m_Handle;
                if (m_CreateInfo.isCubeMap)
                {
                    srvDesc.type = (m_CreateInfo.arraySize > 6) ? nri::TextureView::TEXTURE_CUBE_ARRAY : nri::TextureView::TEXTURE_CUBE;
                }
                else if (m_CreateInfo.arraySize > 1)
                {
                    srvDesc.type = nri::TextureView::TEXTURE_ARRAY;
                }
                else
                {
                    srvDesc.type = nri::TextureView::TEXTURE;
                }
                srvDesc.format = m_CreateInfo.format;
                srvDesc.mipOffset = 0;
                srvDesc.mipNum = nri::REMAINING;
                srvDesc.layerOffset = 0;
                srvDesc.layerNum = nri::REMAINING;
                srvDesc.planes = isDepth ? nri::PlaneBits::DEPTH : nri::PlaneBits::ALL;

                nri.CreateTextureView(srvDesc, m_ShaderResourceView);
            }

            if (m_CreateInfo.isUAV && ((desc.usage & nri::TextureUsageBits::SHADER_RESOURCE_STORAGE) != 0))
            {
                m_StorageViews.resize(m_CreateInfo.mipLevels, nullptr);
                for (uint32_t mip = 0; mip < m_CreateInfo.mipLevels; ++mip)
                {
                    nri::TextureViewDesc uavDesc = {};
                    uavDesc.texture = m_Handle;
                    uavDesc.type = (m_CreateInfo.arraySize > 1) ? nri::TextureView::STORAGE_TEXTURE_ARRAY : nri::TextureView::STORAGE_TEXTURE;
                    uavDesc.format = m_CreateInfo.format;
                    uavDesc.mipOffset = static_cast<nri::Dim_t>(mip);
                    uavDesc.mipNum = 1;
                    uavDesc.layerOffset = 0;
                    uavDesc.layerNum = nri::REMAINING;
                    nri.CreateTextureView(uavDesc, m_StorageViews[mip]);
                }
            }

            if (m_CreateInfo.bindless && !m_CreateInfo.isRenderTarget && !m_CreateInfo.isNativeObject && m_ShaderResourceView)
            {
                m_BindlessIndex = BindlessSystem::RegisterTexture(m_ShaderResourceView);
            }
        }

        if (m_Handle)
        {
            const auto &desc = nri.GetTextureDesc(*m_Handle);
            if ((desc.usage & nri::TextureUsageBits::SHADER_RESOURCE) != 0)
            {
                nri::SamplerDesc samplerDesc = {};
                samplerDesc.filters.min = m_CreateInfo.samplerLinearFiltering ? nri::Filter::LINEAR : nri::Filter::NEAREST;
                samplerDesc.filters.mag = m_CreateInfo.samplerLinearFiltering ? nri::Filter::LINEAR : nri::Filter::NEAREST;
                samplerDesc.filters.mip = m_CreateInfo.samplerLinearFiltering ? nri::Filter::LINEAR : nri::Filter::NEAREST;
                samplerDesc.addressModes.u = m_CreateInfo.samplerAddressU;
                samplerDesc.addressModes.v = m_CreateInfo.samplerAddressV;
                samplerDesc.addressModes.w = m_CreateInfo.samplerAddressW;
                samplerDesc.mipMax = 16.0f;

                nri::Result samplerRes = nri.CreateSampler(*device, samplerDesc, m_Sampler);
                LOG_ASSERT(samplerRes == nri::Result::SUCCESS && m_Sampler, "Failed to create texture sampler");
            }
        }
    }

    nri::Descriptor *Texture::GetMipShaderResourceView(uint32_t mipLevel)
    {
        EnsureTextureHandle();
        if (!m_Handle)
        {
            return nullptr;
        }

        auto *deviceManager = DeviceManager::GetInstance();
        if (!deviceManager)
        {
            return nullptr;
        }

        const auto &nri = deviceManager->GetCoreInterface();
        const auto &desc = nri.GetTextureDesc(*m_Handle);
        if ((desc.usage & nri::TextureUsageBits::SHADER_RESOURCE) == 0)
        {
            return nullptr;
        }
        if (mipLevel == 0 && m_CreateInfo.mipLevels <= 1)
        {
            return m_ShaderResourceView;
        }
        if (m_MipSRVs.size() < m_CreateInfo.mipLevels)
        {
            m_MipSRVs.resize(m_CreateInfo.mipLevels, nullptr);
        }
        if (mipLevel >= m_MipSRVs.size())
        {
            return m_ShaderResourceView;
        }
        if (!m_MipSRVs[mipLevel])
        {
            auto *deviceManager = DeviceManager::GetInstance();
            const auto &nri = deviceManager->GetCoreInterface();
            const bool isDepth = texture_utils::IsDepthFormat(m_CreateInfo.format);

            nri::TextureViewDesc srvDesc = {};
            srvDesc.texture = m_Handle;
            srvDesc.type = (m_CreateInfo.arraySize > 1) ? nri::TextureView::TEXTURE_ARRAY : nri::TextureView::TEXTURE;
            srvDesc.format = m_CreateInfo.format;
            srvDesc.mipOffset = static_cast<nri::Dim_t>(mipLevel);
            srvDesc.mipNum = 1;
            srvDesc.layerOffset = 0;
            srvDesc.layerNum = nri::REMAINING;
            srvDesc.planes = isDepth ? nri::PlaneBits::DEPTH : nri::PlaneBits::ALL;
            nri.CreateTextureView(srvDesc, m_MipSRVs[mipLevel]);
        }
        return m_MipSRVs[mipLevel];
    }

    nri::Descriptor *Texture::GetStorageView(uint32_t mipLevel)
    {
        EnsureTextureHandle();
        if (!m_Handle || mipLevel >= m_StorageViews.size())
        {
            return nullptr;
        }
        return m_StorageViews[mipLevel];
    }

    void Texture::EnsureTextureHandle()
    {
        if (!m_Handle)
        {
            CreateTextureHandle();
        }
    }

    Ref<Texture> Texture::Create()
    {
        return CreateRef<Texture>();
    }

    Ref<Texture> Texture::Create(TextureCreateInfo createInfo, const std::string &debugName)
    {
        return CreateRef<Texture>(createInfo, debugName);
    }

    Ref<Texture> Texture::Create(const std::vector<uint8_t> &data, TextureCreateInfo createInfo, nri::CommandBuffer *cmd, const std::string &debugName)
    {
        return CreateRef<Texture>(data, createInfo, cmd, debugName);
    }

    Ref<Texture> Texture::Create(const std::filesystem::path &filepath, TextureCreateInfo createInfo, nri::CommandBuffer *cmd, const std::string &debugName)
    {
        return CreateRef<Texture>(filepath, createInfo, cmd, debugName);
    }

    bool texture_utils::LoadEXRTexture(const std::filesystem::path &filepath, int &outWidth, int &outHeight, nri::Format &outFormat, std::vector<uint8_t> &data)
    {
        exr_context_t ctx = nullptr;
        exr_context_initializer_t cinit = EXR_DEFAULT_CONTEXT_INITIALIZER;
        exr_result_t rv = exr_start_read(&ctx, filepath.string().c_str(), &cinit);
        if (rv != EXR_ERR_SUCCESS)
        {
            LOG_ERROR("[Texture] Failed to open EXR '{}': {}", filepath.generic_string(), exr_get_default_error_message(rv));
            return false;
        }

        exr_decode_pipeline_t decode = EXR_DECODE_PIPELINE_INITIALIZER;
        bool decodeInitialized = false;
        bool success = false;

        do
        {
            int partCount = 0;
            rv = exr_get_count(ctx, &partCount);
            if (rv != EXR_ERR_SUCCESS || partCount <= 0)
            {
                LOG_ERROR("[Texture] EXR has no readable parts: {}", filepath.generic_string());
                break;
            }

            exr_attr_box2i_t dataWindow {};
            rv = exr_get_data_window(ctx, 0, &dataWindow);
            if (rv != EXR_ERR_SUCCESS)
            {
                LOG_ERROR("[Texture] Failed to read EXR data window '{}': {}", filepath.generic_string(), exr_get_default_error_message(rv));
                break;
            }

            const uint32_t width = static_cast<uint32_t>(dataWindow.max.x - dataWindow.min.x + 1);
            const uint32_t height = static_cast<uint32_t>(dataWindow.max.y - dataWindow.min.y + 1);
            if (width == 0 || height == 0)
            {
                LOG_ERROR("[Texture] Invalid EXR dimensions '{}': {}x{}", filepath.generic_string(), width, height);
                break;
            }

            exr_storage_t storage = EXR_STORAGE_SCANLINE;
            rv = exr_get_storage(ctx, 0, &storage);
            if (rv != EXR_ERR_SUCCESS)
            {
                LOG_ERROR("[Texture] Failed to read EXR storage '{}': {}", filepath.generic_string(), exr_get_default_error_message(rv));
                break;
            }

            exr_chunk_info_t chunk {};
            if (storage == EXR_STORAGE_SCANLINE)
            {
                rv = exr_read_scanline_chunk_info(ctx, 0, dataWindow.min.y, &chunk);
            }
            else if (storage == EXR_STORAGE_TILED)
            {
                rv = exr_read_tile_chunk_info(ctx, 0, 0, 0, 0, 0, &chunk);
            }
            else
            {
                LOG_ERROR("[Texture] Unsupported EXR storage for texture loading: {}", filepath.generic_string());
                break;
            }

            if (rv != EXR_ERR_SUCCESS)
            {
                LOG_ERROR("[Texture] Failed to initialize EXR chunk info '{}': {}", filepath.generic_string(), exr_get_default_error_message(rv));
                break;
            }

            rv = exr_decoding_initialize(ctx, 0, &chunk, &decode);
            if (rv != EXR_ERR_SUCCESS)
            {
                LOG_ERROR("[Texture] Failed to initialize EXR decode pipeline '{}': {}", filepath.generic_string(), exr_get_default_error_message(rv));
                break;
            }

            decodeInitialized = true;

            const int rIndex = FindExrChannelIndex(decode, "R");
            const int gIndex = FindExrChannelIndex(decode, "G");
            const int bIndex = FindExrChannelIndex(decode, "B");
            const int aIndex = FindExrChannelIndex(decode, "A");
            const int yIndex = FindExrChannelIndex(decode, "Y");

            std::vector<float> redPlane;
            std::vector<float> greenPlane;
            std::vector<float> bluePlane;
            std::vector<float> alphaPlane;
            std::vector<float> luminancePlane;

            ConfigureExrChannel(decode, rIndex, redPlane, width, height);
            ConfigureExrChannel(decode, gIndex, greenPlane, width, height);
            ConfigureExrChannel(decode, bIndex, bluePlane, width, height);
            ConfigureExrChannel(decode, aIndex, alphaPlane, width, height);
            ConfigureExrChannel(decode, yIndex, luminancePlane, width, height);

            auto setChunkDecodePointers = [&](const exr_chunk_info_t &cinfo)
            {
                const int32_t xOffset = cinfo.start_x - dataWindow.min.x;
                const int32_t yOffset = cinfo.start_y - dataWindow.min.y;
                if (xOffset < 0 || yOffset < 0)
                {
                    return false;
                }

                const size_t pixelOffset = static_cast<size_t>(yOffset) * static_cast<size_t>(width) + static_cast<size_t>(xOffset);

                auto setChannelPtr = [&](int channelIndex, std::vector<float> &plane)
                {
                    if (channelIndex < 0)
                    {
                        return true;
                    }

                    if (pixelOffset >= plane.size())
                    {
                        return false;
                    }

                    exr_coding_channel_info_t &channel = decode.channels[channelIndex];
                    channel.decode_to_ptr = reinterpret_cast<unsigned char *>(plane.data() + pixelOffset);
                    channel.user_pixel_stride = sizeof(float);
                    channel.user_line_stride = static_cast<int32_t>(sizeof(float) * width);
                    return true;
                };

                if (!setChannelPtr(rIndex, redPlane)) return false;
                if (!setChannelPtr(gIndex, greenPlane)) return false;
                if (!setChannelPtr(bIndex, bluePlane)) return false;
                if (!setChannelPtr(aIndex, alphaPlane)) return false;
                if (!setChannelPtr(yIndex, luminancePlane)) return false;
                return true;
            };

            rv = exr_decoding_choose_default_routines(ctx, 0, &decode);
            if (rv != EXR_ERR_SUCCESS)
            {
                LOG_ERROR("[Texture] Failed to select EXR decode routines '{}': {}", filepath.generic_string(), exr_get_default_error_message(rv));
                break;
            }

            if (storage == EXR_STORAGE_SCANLINE)
            {
                int scanlinesPerChunk = 1;
                if (exr_get_scanlines_per_chunk(ctx, 0, &scanlinesPerChunk) != EXR_ERR_SUCCESS || scanlinesPerChunk <= 0)
                {
                    scanlinesPerChunk = 1;
                }

                for (int y = dataWindow.min.y; y <= dataWindow.max.y; y += scanlinesPerChunk)
                {
                    rv = exr_read_scanline_chunk_info(ctx, 0, y, &chunk);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        continue;
                    }

                    rv = exr_decoding_update(ctx, 0, &chunk, &decode);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        continue;
                    }

                    if (!setChunkDecodePointers(chunk))
                    {
                        continue;
                    }

                    rv = exr_decoding_run(ctx, 0, &decode);
                    if (rv != EXR_ERR_SUCCESS)
                    {
                        continue;
                    }
                }
            }
            else if (storage == EXR_STORAGE_TILED)
            {
                int32_t tileCountX = 0;
                int32_t tileCountY = 0;
                int32_t levelWidth = 0;
                int32_t levelHeight = 0;
                int32_t tileWidth = 0;
                int32_t tileHeight = 0;
                if (exr_get_level_sizes(ctx, 0, 0, 0, &levelWidth, &levelHeight) != EXR_ERR_SUCCESS ||
                    exr_get_tile_sizes(ctx, 0, 0, 0, &tileWidth, &tileHeight) != EXR_ERR_SUCCESS ||
                    tileWidth <= 0 || tileHeight <= 0)
                {
                    LOG_ERROR("[Texture] Failed to query EXR level/tile sizes for '{}'", filepath.generic_string());
                    break;
                }
                tileCountX = (levelWidth + tileWidth - 1) / tileWidth;
                tileCountY = (levelHeight + tileHeight - 1) / tileHeight;
                if (tileCountX <= 0 || tileCountY <= 0)
                {
                    LOG_ERROR("[Texture] Invalid EXR tile counts '{}': {}x{}", filepath.generic_string(), tileCountX, tileCountY);
                    break;
                }

                for (int32_t tileY = 0; tileY < tileCountY; ++tileY)
                {
                    for (int32_t tileX = 0; tileX < tileCountX; ++tileX)
                    {
                        rv = exr_read_tile_chunk_info(ctx, 0, tileX, tileY, 0, 0, &chunk);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            continue;
                        }

                        rv = exr_decoding_update(ctx, 0, &chunk, &decode);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            continue;
                        }

                        if (!setChunkDecodePointers(chunk))
                        {
                            continue;
                        }

                        rv = exr_decoding_run(ctx, 0, &decode);
                        if (rv != EXR_ERR_SUCCESS)
                        {
                            continue;
                        }
                    }
                }
            }

            const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
            const bool hasRGB = rIndex >= 0 || gIndex >= 0 || bIndex >= 0;
            const bool hasY = yIndex >= 0;
            const bool hasA = aIndex >= 0;

            std::vector<float> floatPixels(pixelCount * 4u, 0.0f);
            for (size_t i = 0; i < pixelCount; ++i)
            {
                const float y = hasY ? luminancePlane[i] : 0.0f;
                const float fallback = hasRGB
                    ? (rIndex >= 0 ? redPlane[i] : (gIndex >= 0 ? greenPlane[i] : bluePlane[i]))
                    : y;

                const float r = rIndex >= 0 ? redPlane[i] : (hasY ? y : fallback);
                const float g = gIndex >= 0 ? greenPlane[i] : (hasY ? y : fallback);
                const float b = bIndex >= 0 ? bluePlane[i] : (hasY ? y : fallback);
                const float a = hasA ? alphaPlane[i] : 1.0f;

                floatPixels[i * 4u + 0u] = r;
                floatPixels[i * 4u + 1u] = g;
                floatPixels[i * 4u + 2u] = b;
                floatPixels[i * 4u + 3u] = a;
            }

            outWidth = width;
            outHeight = height;

            if (outFormat == nri::Format::UNKNOWN)
            {
                outFormat = nri::Format::RGBA32_SFLOAT;
            }

            if (outFormat == nri::Format::RGBA32_SFLOAT)
            {
                data.resize(pixelCount * 4u * sizeof(float));
                std::memcpy(data.data(), floatPixels.data(), data.size());
            }
            else if (outFormat == nri::Format::RGBA8_UNORM || outFormat == nri::Format::RGBA8_SRGB)
            {
                data.resize(pixelCount * 4u);
                for (size_t i = 0; i < pixelCount * 4; ++i)
                    data[i] = FloatToByte(floatPixels[i]);
            }
            else
            {
                LOG_ERROR("[Texture] Unsupported texture format for EXR '{}': {}", filepath.generic_string(), static_cast<int>(outFormat));
                break;
            }

            success = (data.data() != nullptr) && !data.empty();
        }
        while (false);

        if (decodeInitialized)
        {
            exr_decoding_destroy(ctx, &decode);
        }

        exr_finish(&ctx);
        return success;
    }

    void texture_utils::FlipImageBuffer(std::vector<uint8_t> &data, int width, int height, int rowPitch)
    {
        if (!data.data())
            return;

        std::vector<uint8_t> flipped(data.size());

        for (int y = 0; y < height; y++)
        {
            // Copy each row from bottom to top
            memcpy(
                flipped.data() + y * rowPitch,
                data.data() + (height - 1 - y) * rowPitch,
                rowPitch
            );
        }

        // Copy the flipped data back to the original buffer
        memcpy(data.data(), flipped.data(), flipped.size());
    }

    uint32_t Texture::GetBindlessIndex() const
    {
        return (m_BindlessIndex == 0xFFFFFFFF) ? 0 : m_BindlessIndex;
    }

    struct TextureUploadTask
    {
        Ref<Texture> texture;
        std::function<void()> onComplete;
    };

    static std::mutex s_UploadQueueMutex;
    static std::queue<TextureUploadTask> s_UploadQueue;

    void Texture::SubmitAsyncUpload(Ref<Texture> texture, std::function<void()> onComplete)
    {
        if (!texture)
            return;

        auto *app = Application::GetInstance();
        if (!app || !Application::IsRenderThreadRunning())
        {
            if (DeviceManager::GetInstance() && DeviceManager::GetInstance()->GetDevice())
            {
                texture->SetReadyFlag(false);
                texture->SetData(nullptr, 4);
                texture->SetReadyFlag(true);
                if (BindlessSystem::HasPendingWrites())
                {
                    BindlessSystem::FlushPendingWrites();
                }
                if (onComplete) onComplete();
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lock(s_UploadQueueMutex);
            s_UploadQueue.push({ texture, onComplete });
        }

        Application::SubmitToRenderThread([]() {});
    }

    bool Texture::HasPendingUploads()
    {
        std::lock_guard<std::mutex> lock(s_UploadQueueMutex);
        return !s_UploadQueue.empty();
    }

    void Texture::ClearPendingUploads()
    {
        std::lock_guard<std::mutex> lock(s_UploadQueueMutex);
        while (!s_UploadQueue.empty())
        {
            s_UploadQueue.pop();
        }
    }

    void Texture::ProcessAsyncUploads(uint32_t maxUploadsPerFrame, uint64_t maxBytesPerFrame)
    {
        IGN_PROFILE_FUNCTION();

        std::vector<TextureUploadTask> batch;
        {
            std::lock_guard<std::mutex> lock(s_UploadQueueMutex);
            if (s_UploadQueue.empty())
                return;

            uint32_t count = 0;
            uint64_t accumulatedBytes = 0;

            while (!s_UploadQueue.empty() && count < maxUploadsPerFrame)
            {
                auto task = s_UploadQueue.front();
                s_UploadQueue.pop();

                if (task.texture)
                {
                    uint64_t texBytes = static_cast<uint64_t>(task.texture->GetApproxSizeBytes());
                    batch.push_back(task);
                    accumulatedBytes += texBytes;
                    count++;

                    if (accumulatedBytes >= maxBytesPerFrame && count > 1)
                    {
                        break;
                    }
                }
            }
        }

        if (batch.empty())
            return;

        if (!DeviceManager::GetInstance() || !DeviceManager::GetInstance()->GetDevice())
            return;

        for (auto &task : batch)
        {
            if (!task.texture)
                continue;

            task.texture->SetReadyFlag(false);
            task.texture->SetData(nullptr, 4);
            task.texture->SetReadyFlag(true);
            if (task.onComplete)
            {
                task.onComplete();
            }
        }

        if (BindlessSystem::HasPendingWrites())
        {
            BindlessSystem::FlushPendingWrites();
        }
    }
}
