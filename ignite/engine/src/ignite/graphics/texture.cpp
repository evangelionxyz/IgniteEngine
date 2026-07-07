// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "texture.hpp"
#include "bindless_system.hpp"
#include "ignite/core/base.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/imgui/imgui_nvrhi.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/serializer.hpp"
#include <stb_image.h>

namespace ignite
{
	namespace texture_utils
    {
		static ignite::Path BuildMetaPath(Project *project, const AssetMetaData &metadata, const char *extension)
		{
			if (!project)
			{
				return {};
			}

			ignite::Path assetPath = project->GetProjectFilepath(metadata.filepath);
			assetPath += extension;
			return assetPath;
		}
    }

    TextureCreateInfo Texture::GetDefaultCreateInfo(const AssetMetaData &metadata)
    {
        TextureCreateInfo createInfo;
        const std::string extension = metadata.filepath.extension().string();
        const bool isHDR = extension == ".hdr";

        createInfo.format = isHDR ? nvrhi::Format::RGBA32_FLOAT : nvrhi::Format::RGBA8_UNORM;
        createInfo.mipLevels = isHDR ? 1 : 4;
        createInfo.flip = isHDR;
        createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfo.keepInitialState = true;
        createInfo.deferGpuCreate = true;

        return createInfo;
    }

    ignite::Path Texture::GetMetaPath(Project *project, const AssetMetaData &metadata)
    {
        return texture_utils::BuildMetaPath(project, metadata, ".meta");
    }

    ignite::Path Texture::GetLegacyMetaPath(Project *project, const AssetMetaData &metadata)
    {
        return texture_utils::BuildMetaPath(project, metadata, ".ixtex");
    }

    bool Texture::LoadCreateInfoFile(const ignite::Path &filepath, TextureCreateInfo &outCreateInfo)
    {
        if (filepath.empty() || !ignite::Path::exists(filepath))
        {
            return false;
        }

        YAML::Node root = Serializer::Deserialize(filepath);
        YAML::Node node = root["DATA"];
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

        if (node["Format"]) outCreateInfo.format = static_cast<nvrhi::Format>(node["Format"].as<uint32_t>());
        if (node["InitialState"]) outCreateInfo.initialState = static_cast<nvrhi::ResourceStates>(node["InitialState"].as<uint32_t>());
        if (node["Dimension"]) outCreateInfo.dimension = static_cast<nvrhi::TextureDimension>(node["Dimension"].as<uint32_t>());
        if (node["SamplerAddressU"]) outCreateInfo.samplerAddressU = static_cast<nvrhi::SamplerAddressMode>(node["SamplerAddressU"].as<uint32_t>());
        if (node["SamplerAddressV"]) outCreateInfo.samplerAddressV = static_cast<nvrhi::SamplerAddressMode>(node["SamplerAddressV"].as<uint32_t>());
        if (node["SamplerAddressW"]) outCreateInfo.samplerAddressW = static_cast<nvrhi::SamplerAddressMode>(node["SamplerAddressW"].as<uint32_t>());

        return true;
    }

    bool Texture::SerializeMetaFile(const ignite::Path &filepath, AssetHandle handle, const TextureCreateInfo &createInfo)
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
        sr.AddKeyValue("InitialState", static_cast<uint32_t>(createInfo.initialState));
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
            (m_CreateInfo.format == nvrhi::Format::RGBA32_FLOAT) ? sizeof(float) * 4u : 4u;

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
        : m_CreateInfo(createInfo), m_DebugName(debugName)
    {
        if (!m_CreateInfo.deferGpuCreate)
        {
            CreateTextureHandle();
        }
    }

    Texture::Texture(const std::vector<uint8_t> &data, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName)
        : m_Buffer(data), m_CreateInfo(createInfo), m_DebugName(debugName)
    {
        if (!m_CreateInfo.deferGpuCreate)
        {
            CreateTextureHandle();
        }

        if (cmd)
        {
			const uint32_t channels = 4;
			const uint32_t rowPitch = m_CreateInfo.width * channels;
			uint32_t depthPitch = rowPitch * m_CreateInfo.height;

            SetData(cmd, rowPitch, depthPitch);
        }
    }

    Texture::Texture(const ignite::Path &filepath, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName)
        : m_CreateInfo(createInfo), m_Filepath(filepath), m_DebugName(debugName)
    {
        if (!ignite::Path::exists(filepath))
        {
            LOG_ERROR("[Texture] File does not found! {}", filepath.generic_string());
            return;
        }

        if (texture_utils::IsExrFile(filepath))
        {
            if (m_CreateInfo.format == nvrhi::Format::UNKNOWN)
            {
                m_CreateInfo.format = nvrhi::Format::RGBA32_FLOAT;
            }

            int w = 0, h = 0;
            if (!texture_utils::LoadEXRTexture(filepath, w, h, m_CreateInfo.format, m_Buffer))
            {
                LOG_ERROR("[Texture] Failed to load EXR texture {}", filepath.generic_string());
                return;
            }

            if (!m_CreateInfo.deferGpuCreate)
            {
                CreateTextureHandle();
            }

            if (cmd)
            {
                m_CreateInfo.width = (uint32_t)w;
                m_CreateInfo.height = (uint32_t)h;

                const uint32_t channelCount = 4;
                const uint32_t rowPitch = m_CreateInfo.width * channelCount;
                const uint32_t depthPitch = rowPitch * m_CreateInfo.height;
                SetData(cmd, rowPitch, depthPitch);
            }

            return;
        }

        // always use RGBA
        const int channels = 4;

        switch (m_CreateInfo.format)
        {
            case nvrhi::Format::RGBA8_UNORM:
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
            case nvrhi::Format::RGBA32_FLOAT:
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
        }

        if (cmd)
        {
			const uint32_t rowPitch = m_CreateInfo.width * channels;
			const uint32_t depthPitch = rowPitch * m_CreateInfo.height;
            SetData(cmd, rowPitch, depthPitch);
        }
    }

	Texture::~Texture()
    {
        IGN_PROFILE_FUNCTION();

        // Evict this texture's nvrhi handle from the ImGui binding set cache
        // BEFORE releasing the handle. If we don't do this, the cache keeps
        // a BindingSetHandle that holds a ref to the old nvrhi texture, preventing
        // the GPU memory from being freed. On the next resize a new texture may
        // get the same address, causing an incorrect cache hit with a stale binding.
        if (m_Handle)
        {
            ImGui_NVRHI::InvalidateTextureCache(m_Handle.Get());
        }

        if (m_BindlessIndex != 0xFFFFFFFF)
        {
            BindlessSystem::UnregisterTexture(m_BindlessIndex);
            m_BindlessIndex = 0xFFFFFFFF;
        }

        if (m_Handle && m_TracyAllocationTracked)
        {
            IGN_PROFILE_FREE_N(m_Handle.Get(), "GPU Texture");
            m_TracyAllocationTracked = false;
        }

        m_Buffer.clear();

        m_Sampler = nullptr;
        m_Handle = nullptr;
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

        if (m_CreateInfo.format == nvrhi::Format::RGBA8_UNORM)
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
        else if (m_CreateInfo.format == nvrhi::Format::RGBA32_FLOAT)
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
    }

    void Texture::PrepareUploadData(uint32_t channelCount)
    {
        const uint32_t rowPitch = m_CreateInfo.width * channelCount;
        const uint32_t depthPitch = rowPitch * m_CreateInfo.height;
        PrepareUploadData(rowPitch, depthPitch);
    }

    void Texture::SetData(nvrhi::ICommandList *cmd, uint32_t rowPitch, uint32_t depthPitch)
    {
        IGN_PROFILE_FUNCTION();
        (void)depthPitch;
        if (!m_Buffer.data())
        {
            return;
        }

        EnsureTextureHandle();

        PrepareUploadData(rowPitch, depthPitch);

        if (!m_PreparedMipChain.empty())
        {
            for (uint32_t mip = 0; mip < m_CreateInfo.mipLevels && mip < m_PreparedMipChain.size(); ++mip)
            {
                const auto &mipData = m_PreparedMipChain[mip];
                cmd->writeTexture(m_Handle, 0, mip, mipData.data.data(), mipData.rowPitch, mipData.slicePitch);
            }
        }

        if (!m_CreateInfo.keepCpuData)
        {
            // Release mip chain capacity, not just the elements.
            std::vector<MipLevelData>().swap(m_PreparedMipChain);
            m_UploadDataPrepared = false;
            m_Buffer.clear();
        }
    }

	void Texture::SetData(nvrhi::ICommandList *cmd, uint32_t channelCount)
	{
		const uint32_t rowPitch = m_CreateInfo.width * channelCount;
		const uint32_t depthPitch = rowPitch * m_CreateInfo.height;
		SetData(cmd, rowPitch, depthPitch);
	}

    void *Texture::GetPixelData(Ref<Texture> texture, size_t *outRowPitch, nvrhi::ICommandList *cmd, nvrhi::IDevice *device)
    {
        cmd->open();
        nvrhi::TextureDesc stagingDesc = texture->GetHandle()->getDesc();
        stagingDesc.initialState = nvrhi::ResourceStates::CopyDest;
        nvrhi::StagingTextureHandle stagingTexture = device->createStagingTexture(stagingDesc, nvrhi::CpuAccessMode::Read);
        cmd->copyTexture(stagingTexture, nvrhi::TextureSlice(), texture->GetHandle(), nvrhi::TextureSlice());
        cmd->close();
        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            device->executeCommandList(cmd);
        }

        // Map and read the pixel data
        void *pixelData = device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, outRowPitch);
        return pixelData;
    }

	void Texture::CreateTextureHandle()
    {
        IGN_PROFILE_FUNCTION();
		if (m_Handle)
		{
			return;
		}

    	LOG_ASSERT(m_CreateInfo.dimension != nvrhi::TextureDimension::Unknown, "[Texture] Dimension must be set");
    	LOG_ASSERT(m_CreateInfo.initialState != nvrhi::ResourceStates::Unknown, "[Texture] State must be set");

        auto textureDesc = nvrhi::TextureDesc();
        textureDesc.setDimension(m_CreateInfo.dimension);
        textureDesc.setWidth(m_CreateInfo.width);
        textureDesc.setHeight(m_CreateInfo.height);
        textureDesc.setFormat(m_CreateInfo.format);
        textureDesc.setInitialState(m_CreateInfo.initialState);
        textureDesc.setKeepInitialState(m_CreateInfo.keepInitialState);
        textureDesc.setMipLevels(m_CreateInfo.mipLevels);
        textureDesc.setDebugName(m_DebugName);
        textureDesc.setArraySize(m_CreateInfo.arraySize);
        textureDesc.setSampleQuality(m_CreateInfo.sampleQuality);
        textureDesc.setSampleCount(m_CreateInfo.sampleCount);
        textureDesc.setDepth(m_CreateInfo.depth);
        textureDesc.setIsUAV(m_CreateInfo.isUAV);
        textureDesc.setIsRenderTarget(m_CreateInfo.isRenderTarget);
		textureDesc.setIsTypeless(m_CreateInfo.isTypeless);
        textureDesc.isShadingRateSurface = m_CreateInfo.isShadingRateSurface;

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        if (m_CreateInfo.isNativeObject)
        {
            LOG_ASSERT(m_CreateInfo.nativeObjectPtr, "[Texture] Should non null object");
            LOG_ASSERT(m_CreateInfo.nativeObjectType != 0x0, "[Texture] Should set the native object type");
            m_Handle = device->createHandleForNativeTexture(m_CreateInfo.nativeObjectType, nvrhi::Object(m_CreateInfo.nativeObjectPtr), textureDesc);
        }
        else
        {
            m_Handle = device->createTexture(textureDesc);
        }

        if (m_Handle)
        {
            IGN_PROFILE_ALLOC_N(m_Handle.Get(), GetApproxSizeBytes(), "GPU Texture");
            m_TracyAllocationTracked = true;
            if (m_CreateInfo.bindless && !m_CreateInfo.isRenderTarget && !m_CreateInfo.isNativeObject)
            {
                m_BindlessIndex = BindlessSystem::RegisterTexture(m_Handle.Get());
            }
        }

        nvrhi::SamplerDesc samplerDesc;
        samplerDesc.addressU = m_CreateInfo.samplerAddressU;
        samplerDesc.addressV = m_CreateInfo.samplerAddressV;
        samplerDesc.addressW = m_CreateInfo.samplerAddressW;
        samplerDesc.setAllFilters(m_CreateInfo.samplerLinearFiltering);
        m_Sampler = device->createSampler(samplerDesc);
       
        LOG_ASSERT(m_Handle, "Failed to create texture");
        LOG_ASSERT(m_Sampler, "Failed to create texture sampler");
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

    Ref<Texture> Texture::Create(const std::vector<uint8_t> &data, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName)
    {
        return CreateRef<Texture>(data, createInfo, cmd, debugName);
    }

    Ref<Texture> Texture::Create(const ignite::Path &filepath, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName)
    {
        return CreateRef<Texture>(filepath, createInfo, cmd, debugName);
    }

    bool texture_utils::LoadEXRTexture(const ignite::Path &filepath, int &outWidth, int &outHeight, nvrhi::Format &outFormat, std::vector<uint8_t> &data)
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
                if (exr_get_tile_counts(ctx, 0, 0, 0, &tileCountX, &tileCountY) != EXR_ERR_SUCCESS || tileCountX <= 0 || tileCountY <= 0)
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

            if (outFormat == nvrhi::Format::UNKNOWN)
            {
                outFormat = nvrhi::Format::RGBA32_FLOAT;
            }

            if (outFormat == nvrhi::Format::RGBA32_FLOAT)
            {
                data.resize(pixelCount * 4u * sizeof(float));
                std::memcpy(data.data(), floatPixels.data(), data.size());
            }
            else if (outFormat == nvrhi::Format::RGBA8_UNORM)
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

}
