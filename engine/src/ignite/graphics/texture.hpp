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
#include <filesystem>

namespace ignite
{
    struct TextureCreateInfo
    {
        std::string debugName = "[Texture Class]";

        uint32_t width = 1;
        uint32_t height = 1;
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

        bool keepInitialState = false;
        nvrhi::Format format = nvrhi::Format::UNKNOWN;
        nvrhi::ResourceStates initialState = nvrhi::ResourceStates::Unknown;
        nvrhi::TextureDimension dimension = nvrhi::TextureDimension::Texture2D;
    };

    class Texture : public Asset
    {
    public:
        Texture() = default;
        Texture(const TextureCreateInfo &createInfo);
        Texture(Buffer buffer, const TextureCreateInfo &createInfo, nvrhi::ICommandList *cmd);
        Texture(const std::filesystem::path &filepath, const TextureCreateInfo &createInfo, nvrhi::ICommandList *cmd);

        ~Texture() override;

        static Ref<Texture> Create(const TextureCreateInfo& createInfo);
        static Ref<Texture> Create(Buffer buffer, const TextureCreateInfo &createInfo, nvrhi::ICommandList *cmd);
        static Ref<Texture> Create(const std::filesystem::path &filepath, const TextureCreateInfo &createInfo, nvrhi::ICommandList *cmd);

        void SetData(nvrhi::ICommandList *cmd, uint32_t rowPitch, uint32_t depthPitch);

        const TextureCreateInfo &GetCreateInfo() const { return m_CreateInfo; }
        nvrhi::TextureHandle GetHandle() { return m_Handle; }

        int GetWidth() const { return m_CreateInfo.width; }
        int GetHeight() const { return m_CreateInfo.height; }
        int GetChannels() const { return 4; }
        int GetMipLevel() const { return m_CreateInfo.mipLevels; }
        nvrhi::Format GetFormat() const { return m_CreateInfo.format; }

        const std::filesystem::path &GetFilepath() { return m_Filepath; }

        bool operator ==(const Texture &other) const 
        { 
            return m_Handle.Get() == other.m_Handle.Get();
        }

        const Buffer &GetBuffer() { return m_Buffer; }
        static AssetType GetStaticType() { return AssetType::Texture; }
        virtual AssetType GetType() override { return GetStaticType(); }

    private:
        void CreateTextureHandle();

        Buffer m_Buffer;
        TextureCreateInfo m_CreateInfo;
        std::filesystem::path m_Filepath;
        nvrhi::TextureHandle m_Handle;
    };

}

#endif
