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

#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/buffer.hpp"
#include "mip_generator.hpp"
#include <filesystem>

namespace ignite
{
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

    class Texture : public Asset
    {
    public:
        Texture() = default;
        Texture(TextureCreateInfo createInfo, const std::string &debugName = "Texture Class");
        Texture(Buffer buffer, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName = "Texture Class");
        Texture(const std::filesystem::path &filepath, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName = "Texture Class");

        ~Texture() override;

        static Ref<Texture> Create();
        static Ref<Texture> Create(TextureCreateInfo createInfo, const std::string &debugName = "Texture Class");
        static Ref<Texture> Create(Buffer buffer, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName = "Texture Class");
        static Ref<Texture> Create(const std::filesystem::path &filepath, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName = "Texture Class");

        void SetData(nvrhi::ICommandList *cmd, uint32_t channelCount);
        void SetData(nvrhi::ICommandList *cmd, uint32_t rowPitch, uint32_t depthPitch);
        void PrepareUploadData(uint32_t channelCount);
        void PrepareUploadData(uint32_t rowPitch, uint32_t depthPitch);

        TextureCreateInfo GetCreateInfo() const { return m_CreateInfo; }
        nvrhi::TextureHandle GetHandle() { return m_Handle; }
        nvrhi::SamplerHandle GetSampler() const { return m_Sampler; }

        static void *GetPixelData(Ref<Texture> texture, size_t *outRowPitch, nvrhi::ICommandList *cmd, nvrhi::IDevice *device);

        int GetWidth() const { return m_CreateInfo.width; }
        int GetHeight() const { return m_CreateInfo.height; }
        int GetChannels() const { return 4; }
        int GetMipLevel() const { return m_CreateInfo.mipLevels; }
        nvrhi::Format GetFormat() const { return m_CreateInfo.format; }

        const std::string &GetDebugName() const { return m_DebugName; }
        const std::filesystem::path &GetFilepath() { return m_Filepath; }

        const Buffer &GetBuffer() { return m_Buffer; }
        static AssetType GetStaticType() { return AssetType::Texture; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

        bool operator ==(const Texture &other) const  { return m_Handle.Get() == other.m_Handle.Get(); }

    private:
        void CreateTextureHandle();
        void EnsureTextureHandle();
        size_t GetApproxSizeBytes() const;

        Buffer m_Buffer;
        TextureCreateInfo m_CreateInfo;
        std::filesystem::path m_Filepath;
        nvrhi::TextureHandle m_Handle;
        nvrhi::SamplerHandle m_Sampler;
        std::string m_DebugName;
        bool m_TracyAllocationTracked = false;
        bool m_UploadDataPrepared = false;
        std::vector<MipLevelData> m_PreparedMipChain;
    };

}

#endif
