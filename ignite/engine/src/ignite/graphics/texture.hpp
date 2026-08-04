/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
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

#pragma once
#ifndef IGN_TEXTURE_HPP
#define IGN_TEXTURE_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/buffer.hpp"
#include "ignite/core/path.hpp"
#include "mip_generator.hpp"

#include <openexr.h>
#include <openexr_errors.h>

namespace ignite
{
    struct ImageData
    {
        std::vector<uint8_t> pixels;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct TextureCreateInfo
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arraySize = 1;
        uint32_t sampleCount = 1;
        uint32_t sampleQuality = 0;

        bool flip = false;
        bool isRenderTarget = false;
        bool isTypeless = false;
        bool isUAV = false;
        bool isShadingRateSurface = false;
        bool keepCpuData = false;
        bool deferGpuCreate = false;
		bool bindless = true;

        bool keepInitialState = false;
        bool isNativeObject = false;

        void *nativeObjectPtr = nullptr;
        nvrhi::ObjectType nativeObjectType = 0;

        nvrhi::Format format = nvrhi::Format::UNKNOWN;
        nvrhi::ResourceStates initialState = nvrhi::ResourceStates::Unknown;
        nvrhi::TextureDimension dimension = nvrhi::TextureDimension::Texture2D;

        nvrhi::SamplerAddressMode samplerAddressU = nvrhi::SamplerAddressMode::ClampToEdge;
        nvrhi::SamplerAddressMode samplerAddressV = nvrhi::SamplerAddressMode::ClampToEdge;
        nvrhi::SamplerAddressMode samplerAddressW = nvrhi::SamplerAddressMode::ClampToEdge;
        bool samplerLinearFiltering = true;
    };

    namespace texture_utils
    {
        static std::string ToLowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        static bool IsExrFile(const ignite::Path &filepath)
        {
            return ToLowerCopy(filepath.extension().string()) == ".exr";
        }

        static int FindExrChannelIndex(exr_decode_pipeline_t &decode, const char *name)
        {
            for (int i = 0; i < decode.channel_count; ++i)
            {
                const char *channelName = decode.channels[i].channel_name;
                if (channelName && ToLowerCopy(channelName) == ToLowerCopy(name))
                {
                    return i;
                }
            }
            return -1;
        }

        static bool ConfigureExrChannel(exr_decode_pipeline_t &decode, int channelIndex, std::vector<float> &plane, uint32_t width, uint32_t height)
        {
            if (channelIndex < 0)
            {
                return false;
            }

            plane.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
            exr_coding_channel_info_t &channel = decode.channels[channelIndex];
            channel.user_data_type = EXR_PIXEL_FLOAT;
            channel.user_bytes_per_element = sizeof(float);
            channel.user_pixel_stride = sizeof(float);
            channel.user_line_stride = static_cast<int32_t>(sizeof(float) * width);
            channel.decode_to_ptr = reinterpret_cast<unsigned char *>(plane.data());
            return true;
        }

        static uint8_t FloatToByte(float value)
        {
            if (!std::isfinite(value))
            {
                value = 0.0f;
            }

            value = std::clamp(value, 0.0f, 1.0f);
            return static_cast<uint8_t>(value * 255.0f + 0.5f);
        }

        static void DownSample(uint8_t *srcPixels, uint8_t *destPixels, int srcWidth, int srcHeight, int destWidth, int destHeight, int channels)
        {
            // Simple box - filter downsample — good enough for thumbnails and avoids
            // pulling in stb_image_resize as an additional dependency.
            const float xScale = static_cast<float>(srcWidth) / static_cast<float>(destWidth);
            const float yScale = static_cast<float>(srcHeight) / static_cast<float>(destHeight);

            for (int dy = 0; dy < destHeight; ++dy)
            {
                const int srcY0 = static_cast<int>(dy * yScale);
                const int srcY1 = static_cast<int>((dy + 1) * yScale);
                const int clampedSrcY1 = std::min(srcY1, srcHeight - 1);

                for (int dx = 0; dx < destWidth; ++dx)
                {
                    const int srcX0 = static_cast<int>(dx * xScale);
                    const int srcX1 = static_cast<int>((dx + 1) * xScale);
                    const int clampedSrcX1 = std::min(srcX1, srcWidth - 1);

                    uint32_t r = 0, g = 0, b = 0, a = 0, count = 0;
                    for (int sy = srcY0; sy <= clampedSrcY1; ++sy)
                    {
                        for (int sx = srcX0; sx <= clampedSrcX1; ++sx)
                        {
                            const uint8_t *px = srcPixels + (sy * srcWidth + sx) * channels;
                            r += px[0]; g += px[1]; b += px[2]; a += px[3];
                            ++count;
                        }
                    }
                    if (count == 0) count = 1;
                    uint8_t *p = destPixels + (dy * destWidth + dx) * channels;
                    p[0] = static_cast<uint8_t>(r / count);
                    p[1] = static_cast<uint8_t>(g / count);
                    p[2] = static_cast<uint8_t>(b / count);
                    p[3] = static_cast<uint8_t>(a / count);
                }

            }
        }

        IGN_API bool LoadEXRTexture(const ignite::Path &filepath, int &outWidth, int &outHeight, nvrhi::Format &outFormat, std::vector<uint8_t> &data);

        // Utility function to flip image buffer vertically
        void FlipImageBuffer(std::vector<uint8_t> &data, int width, int height, int rowPitch);
    }

    class IGN_API Texture : public Asset
    {
    public:
        Texture() = default;
        Texture(TextureCreateInfo createInfo, const std::string &debugName = "Texture Class");
        Texture(const std::vector<uint8_t> &data, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName = "Texture Class");
        Texture(const ignite::Path &filepath, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName = "Texture Class");

        ~Texture() override;

        static Ref<Texture> Create();
        static Ref<Texture> Create(TextureCreateInfo createInfo, const std::string &debugName = "Texture Class");
        static Ref<Texture> Create(const std::vector<uint8_t> &data, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName = "Texture Class");
        static Ref<Texture> Create(const ignite::Path &filepath, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName = "Texture Class");

        static TextureCreateInfo GetDefaultCreateInfo(const AssetMetaData &metadata);
        static ignite::Path GetMetaPath(Project *project, const AssetMetaData &metadata);
        static ignite::Path GetLegacyMetaPath(Project *project, const AssetMetaData &metadata);
        static bool LoadCreateInfoFile(const ignite::Path &filepath, TextureCreateInfo &outCreateInfo);
        static bool SerializeMetaFile(const ignite::Path &filepath, AssetHandle handle, const TextureCreateInfo &createInfo);

        void SetData(nvrhi::ICommandList *cmd, uint32_t channelCount);
        void SetData(nvrhi::ICommandList *cmd, uint32_t rowPitch, uint32_t depthPitch);
        void PrepareUploadData(uint32_t channelCount);
        void PrepareUploadData(uint32_t rowPitch, uint32_t depthPitch);

        // Asynchronous GPU upload queue management
        static void SubmitAsyncUpload(Ref<Texture> texture, std::function<void()> onComplete = nullptr);
        static void ProcessAsyncUploads(uint32_t maxUploadsPerFrame = 4, uint64_t maxBytesPerFrame = 16 * 1024 * 1024);
        static bool HasPendingUploads();

        TextureCreateInfo GetCreateInfo() const { return m_CreateInfo; }
        nvrhi::TextureHandle GetHandle() { return m_Handle; }
        nvrhi::SamplerHandle GetSampler() const { return m_Sampler; }
        uint32_t GetBindlessIndex() const;

        static void *GetPixelData(Ref<Texture> texture, size_t *outRowPitch, nvrhi::ICommandList *cmd, nvrhi::IDevice *device);
        static uint32_t CalculateMaxMipLevels(uint32_t width, uint32_t height, uint32_t depth = 1);

        int GetWidth() const { return m_CreateInfo.width; }
        int GetHeight() const { return m_CreateInfo.height; }
        int GetChannels() const { return 4; }
        int GetMipLevel() const { return m_CreateInfo.mipLevels; }
        nvrhi::Format GetFormat() const { return m_CreateInfo.format; }

        const std::string &GetDebugName() const { return m_DebugName; }
        const ignite::Path &GetFilepath() { return m_Filepath; }

        const std::vector<uint8_t> &GetBuffer() { return m_Buffer; }
        static AssetType GetStaticType() { return AssetType::Texture; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

        bool operator ==(const Texture &other) const  { return m_Handle.Get() == other.m_Handle.Get(); }

        operator nvrhi::TextureHandle() const { return m_Handle; }
        operator nvrhi::ITexture *() const { return m_Handle; }

    private:
        void CreateTextureHandle();
        void EnsureTextureHandle();
        size_t GetApproxSizeBytes() const;

        std::vector<uint8_t> m_Buffer;
        TextureCreateInfo m_CreateInfo;
        ignite::Path m_Filepath;
        nvrhi::TextureHandle m_Handle;
        nvrhi::SamplerHandle m_Sampler;
        std::string m_DebugName;
        uint32_t m_BindlessIndex = 0xFFFFFFFF;
        bool m_TracyAllocationTracked = false;
        bool m_UploadDataPrepared = false;
        std::vector<MipLevelData> m_PreparedMipChain;
    };

}

#endif
