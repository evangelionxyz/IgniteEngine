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

#include "mip_generator.hpp"

#include "texture.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/core/application.hpp"
#include <stb_image.h>

namespace ignite
{
    // Utility function to flip image buffer vertically
    static void FlipImageBuffer(Buffer &buffer, int width, int height, int rowPitch)
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

    Texture::Texture(TextureCreateInfo createInfo, const std::string &debugName)
        : m_CreateInfo(createInfo), m_DebugName(debugName)
    {
        if (!m_CreateInfo.deferGpuCreate)
        {
            CreateTextureHandle();
        }
    }

    Texture::Texture(Buffer buffer, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName)
        : m_CreateInfo(createInfo), m_DebugName(debugName)
    {
        if (buffer.data && buffer.size)
        {
            m_Buffer = Buffer::Copy(buffer);
        }

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

    Texture::Texture(const std::filesystem::path &filepath, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName)
        : m_CreateInfo(createInfo), m_Filepath(filepath)
    {
        LOG_ASSERT(std::filesystem::exists(filepath), "File does not found!");

        // always use RGBA
        const int channels = 4;

        switch (m_CreateInfo.format)
        {
            case nvrhi::Format::RGBA8_UNORM:
            {
                int width, height, channelsOut;
                uint8_t *pixelData = stbi_load(filepath.generic_string().c_str(), &width, &height, &channelsOut, channels);
                const uint64_t dataSize = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * channels;
                Buffer buffer(dataSize);
                memcpy(buffer.data, pixelData, dataSize);
                stbi_image_free(pixelData);
                m_Buffer = buffer;

                m_CreateInfo.width = static_cast<uint32_t>(width);
                m_CreateInfo.height = static_cast<uint32_t>(height);
                LOG_ASSERT(m_Buffer, "Failed to load texture data");
                break;
            }
            case nvrhi::Format::RGBA32_FLOAT:
            {
                int width, height, channelsOut;
                float *pixelData = stbi_loadf(filepath.generic_string().c_str(), &width, &height, &channelsOut, channels);
                const uint64_t dataSize = static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * channels * sizeof(float);
                Buffer buffer(dataSize);
                memcpy(buffer.data, pixelData, dataSize);
                stbi_image_free(pixelData);
                m_Buffer = buffer;

                m_CreateInfo.width = static_cast<uint32_t>(width);
                m_CreateInfo.height = static_cast<uint32_t>(height);
                LOG_ASSERT(m_Buffer, "Failed to load texture data");
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
    }

    void Texture::SetData(nvrhi::ICommandList *cmd, uint32_t rowPitch, uint32_t depthPitch)
    {
        if (m_HasUploaded || !m_Buffer.data)
        {
            return;
        }

        EnsureTextureHandle();

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
            for (uint32_t mip = 0; mip < m_CreateInfo.mipLevels && mip < mipChain.size(); ++mip)
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
            for (uint32_t mip = 0; mip < m_CreateInfo.mipLevels && mip < mipChain.size(); ++mip)
            {
                const auto &mipData = mipChain[mip];
                cmd->writeTexture(m_Handle, 0, mip, mipData.data.data(), rowPitch * sizeof(float), depthPitch * sizeof(float));
            }
        }

        m_HasUploaded = true;
        SetReadyFlag(m_HasUploaded);

        if (!m_CreateInfo.keepCpuData)
        {
            m_Buffer.Release();
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
		device->executeCommandList(cmd);

		// Map and read the pixel data
		void *pixelData = device->mapStagingTexture(stagingTexture, nvrhi::TextureSlice(), nvrhi::CpuAccessMode::Read, outRowPitch);
        return pixelData;
	}

	void Texture::CreateTextureHandle()
    {
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
        
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        m_Handle = device->createTexture(textureDesc);
        LOG_ASSERT(m_Handle, "Failed to create texture");
    }

    void Texture::EnsureTextureHandle()
    {
        if (!m_Handle)
        {
            CreateTextureHandle();
        }
    }

    Ref<Texture> Texture::Create(TextureCreateInfo createInfo, const std::string &debugName)
    {
        return CreateRef<Texture>(createInfo, debugName);
    }

    Ref<Texture> Texture::Create(Buffer buffer, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName)
    {
        return CreateRef<Texture>(buffer, createInfo, cmd, debugName);
    }

    Ref<Texture> Texture::Create(const std::filesystem::path &filepath, TextureCreateInfo createInfo, nvrhi::ICommandList *cmd, const std::string &debugName)
    {
        return CreateRef<Texture>(filepath, createInfo, cmd, debugName);
    }
}
