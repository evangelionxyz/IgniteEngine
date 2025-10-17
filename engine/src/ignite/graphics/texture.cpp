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

#include "texture.hpp"
#include "ignite/core/logger.hpp"

#include <stb_image.h>

#include "ignite/core/application.hpp"

namespace ignite
{
    // Utility function to flip image buffer vertically
    static void FlipImageBuffer(Buffer& buffer, int width, int height, int rowPitch)
    {
        if (!buffer)
            return;

        std::vector<uint8_t> flipped(buffer.size);

        for (int y = 0; y < height; y++)
        {
            // Copy each row from bottom to top
            memcpy(
                flipped.data() + y * rowPitch,
                buffer.data + (height - 1 - y) * rowPitch,
                rowPitch
            );
        }

        // Copy the flipped data back to the original buffer
        memcpy(buffer.data, flipped.data(), flipped.size());
    }


    Texture::Texture(const TextureCreateInfo& createInfo)
        : m_CreateInfo(createInfo)
    {
        CreateTextureHandle();
    }

    Texture::Texture(Buffer buffer, const TextureCreateInfo &createInfo)
        : m_Buffer(buffer), m_CreateInfo(createInfo)
    {
        CreateTextureHandle();

        const int channels = 4;
        int rowPitch = m_CreateInfo.width * channels;
        int depthPitch = rowPitch * m_CreateInfo.height;

        {
            auto device = Application::GetGraphicsDevice();
            auto cmd = device->createCommandList();
            cmd->open();
            SetData(cmd, rowPitch, depthPitch);
            cmd->close();
            device->executeCommandList(cmd);
        }
    }

    Texture::Texture(const std::filesystem::path &filepath, const TextureCreateInfo &createInfo)
        : m_CreateInfo(createInfo), m_Filepath(filepath)
    {
        LOG_ASSERT(std::filesystem::exists(filepath), "File does not found!");

        // always use RGBA
        const int channels = 4;

        switch (m_CreateInfo.format)
        {
            case nvrhi::Format::RGBA8_UNORM:
            {
                int channelsOut;
                uint8_t *pixelData = stbi_load(filepath.generic_string().c_str(), &m_CreateInfo.width, &m_CreateInfo.height, &channelsOut, 4);
                m_Buffer = Buffer(pixelData, m_CreateInfo.width * m_CreateInfo.height * channels);
                LOG_ASSERT(m_Buffer, "Failed to load texture data");
                break;
            }
            case nvrhi::Format::RGBA32_FLOAT:
            {
                int channelsOut;
                float *pixelData = stbi_loadf(filepath.generic_string().c_str(), &m_CreateInfo.width, &m_CreateInfo.height, &channelsOut, 4);
                m_Buffer = Buffer(pixelData, m_CreateInfo.width * m_CreateInfo.height * channels * sizeof(float));
                LOG_ASSERT(m_Buffer, "Failed to load texture data");
                break;
            }
            default:
            {
                LOG_ASSERT(false, "[Texture] Please specify format explicitly!");
                return;
            }
        }

        int rowPitch = m_CreateInfo.width * channels;
        int depthPitch = rowPitch * m_CreateInfo.height;

        CreateTextureHandle();

        {
            auto device = Application::GetGraphicsDevice();
            auto cmd = device->createCommandList();
            cmd->open();
            SetData(cmd, rowPitch, depthPitch);
            cmd->close();
            device->executeCommandList(cmd);
        }
    }

    Texture::~Texture()
    {
    }

    void Texture::SetData(nvrhi::ICommandList *cmd, int rowPitch, int depthPitch)
    {
        LOG_ASSERT(m_Buffer.data, "[Texture] Pixel data is null");

        if (m_CreateInfo.format == nvrhi::Format::RGBA8_UNORM)
        {
            // char = 1 byte, 8 bit
            if (m_CreateInfo.flip)
            {
                FlipImageBuffer(m_Buffer, m_CreateInfo.width, m_CreateInfo.height, rowPitch);
            }

            uint8_t *byteData = static_cast<uint8_t *>(m_Buffer.data);

            auto mipChain = CPUMipGenerator::GenerateMipChain(byteData,
                m_CreateInfo.width, m_CreateInfo.height, rowPitch,
                m_CreateInfo.format, m_CreateInfo.mipLevels);

            // Upload all mip levels
            for (int mip = 0; mip < m_CreateInfo.mipLevels && mip < mipChain.size(); ++mip)
            {
                const auto &mipData = mipChain[mip];
                cmd->writeTexture(m_Handle, 0, mip, mipData.data.data(), mipData.rowPitch);
            }
        }
        else if (m_CreateInfo.format == nvrhi::Format::RGBA32_FLOAT)
        {
            // float = 4 bytes, 32 bit, we need to multiply sizeof(float)
            if (m_CreateInfo.flip)
            {
                FlipImageBuffer(m_Buffer, m_CreateInfo.width, m_CreateInfo.height, rowPitch * sizeof(float));
            }

            float *floatData = reinterpret_cast<float *>(m_Buffer.data);

            auto mipChain = CPUMipGenerator::GenerateMipChain(floatData,
                m_CreateInfo.width, m_CreateInfo.height, rowPitch * sizeof(float),
                m_CreateInfo.format, m_CreateInfo.mipLevels);


            // Upload all mip levels
            for (int mip = 0; mip < m_CreateInfo.mipLevels && mip < mipChain.size(); ++mip)
            {
                const auto &mipData = mipChain[mip];
                cmd->writeTexture(m_Handle, 0, mip, mipData.data.data(), rowPitch * sizeof(float), depthPitch * sizeof(float));
            }
        }
    }

    void Texture::CreateTextureHandle()
    {
        nvrhi::TextureDesc textureDesc = nvrhi::TextureDesc();
        textureDesc.setDimension(m_CreateInfo.dimension);
        textureDesc.setWidth(m_CreateInfo.width);
        textureDesc.setHeight(m_CreateInfo.height);
        textureDesc.setFormat(m_CreateInfo.format);
        textureDesc.setInitialState(nvrhi::ResourceStates::ShaderResource);
        textureDesc.setKeepInitialState(true);
        textureDesc.setMipLevels(m_CreateInfo.mipLevels);
        textureDesc.setDebugName(m_CreateInfo.debugName);
        
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        m_Handle = device->createTexture(textureDesc);
        LOG_ASSERT(m_Handle, "Failed to create texture");

        nvrhi::SamplerDesc samplerDesc = nvrhi::SamplerDesc();
        samplerDesc.setAllAddressModes(m_CreateInfo.samplerMode);
        samplerDesc.setAllFilters(true);

        m_Sampler = device->createSampler(samplerDesc);
        LOG_ASSERT(m_Sampler, "Failed to create texture sampler");
    }

    Ref<Texture> Texture::Create(const TextureCreateInfo& createInfo)
    {
        return CreateRef<Texture>(createInfo);
    }

    Ref<Texture> Texture::Create(Buffer buffer, const TextureCreateInfo &createInfo)
    {
        return CreateRef<Texture>(buffer, createInfo);
    }

    Ref<Texture> Texture::Create(const std::filesystem::path &filepath, const TextureCreateInfo &createInfo)
    {
        return CreateRef<Texture>(filepath, createInfo);
    }
}
