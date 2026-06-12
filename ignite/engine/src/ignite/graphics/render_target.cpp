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

#include "ignite/core/device/device_manager.hpp"
#include "render_target.hpp"
#include "texture.hpp"

#include <cstdint>
#include <vector>
#include <ignite/core/logger.hpp>
#include <ignite/core/types.hpp>
#include <nvrhi/nvrhi.h>
#include <nvrhi/utils.h>
#include <limits>

namespace ignite {

    RenderTarget::RenderTarget(const RenderTargetCreateInfo &createInfo, const std::string &debugName)
        : m_CreateInfo(createInfo)
    {
        m_ClearDepth = { std::numeric_limits<float>::max(), std::numeric_limits<uint32_t>::max() };

        for (auto &attachment : m_CreateInfo.attachments)
        {
            // Depth attachment flag
            const bool isDepthAttachment = attachment.format >= nvrhi::Format::D16 && attachment.format <= nvrhi::Format::X32G8_UINT;

            // Color attachment flag
            const bool isColorAttachment = attachment.format >= nvrhi::Format::R8_UINT && attachment.format <= nvrhi::Format::RGBA32_FLOAT;

            constexpr bool isRenderTarget = true;

            // find depth attachment if framebuffer is not created yet
            if (isDepthAttachment && m_DepthAttachment == nullptr && m_FramebufferHandle == nullptr)
            {
                TextureCreateInfo createInfo = {};
                createInfo.width = m_CreateInfo.width;
                createInfo.height = m_CreateInfo.height;
                createInfo.depth = 1;
                createInfo.isRenderTarget = isRenderTarget;
                createInfo.format = attachment.format;
                createInfo.sampleCount = m_CreateInfo.sampleCount;
                createInfo.sampleQuality = m_CreateInfo.sampleQuality;
                // createInfo.debugName = std::format("{} - {} ", attachment.name, debugName);
				createInfo.initialState = attachment.state != nvrhi::ResourceStates::Unknown
					? attachment.state
					: nvrhi::ResourceStates::DepthWrite;
            	createInfo.keepInitialState = true;

                // Set dimension and array size based on attachment configuration
                if (attachment.arrayLayers > 1)
                {
                    createInfo.dimension = nvrhi::TextureDimension::Texture2DArray;
                    createInfo.arraySize = attachment.arrayLayers;
                }
                else
                {
                    createInfo.dimension = nvrhi::TextureDimension::Texture2D;
                }

                m_DepthAttachment = Texture::Create(createInfo);
            }

            // create a color attachment if color attachments are empty
            if (isColorAttachment)
            {
                // create color attachment texture
                TextureCreateInfo createInfo = {};
                createInfo.width = m_CreateInfo.width;
                createInfo.height = m_CreateInfo.height;
                createInfo.depth = 1;
                createInfo.isRenderTarget = isRenderTarget;
                createInfo.format = attachment.format;
                // createInfo.debugName = std::format("{} - {} ", attachment.name, debugName);
                createInfo.isUAV = false;
				createInfo.initialState = attachment.state != nvrhi::ResourceStates::Unknown
					? attachment.state
					: nvrhi::ResourceStates::RenderTarget;
            	createInfo.keepInitialState = true;
				createInfo.sampleCount = m_CreateInfo.sampleCount;
				createInfo.sampleQuality = m_CreateInfo.sampleQuality;

                createInfo.isNativeObject = attachment.isNativeObject;
                createInfo.nativeObjectPtr = attachment.nativeObjectPtr;
                createInfo.nativeObjectType = attachment.nativeObjectType;

                // Set dimension and array size based on attachment configuration
                if (attachment.arrayLayers > 1)
                {
                    createInfo.dimension = nvrhi::TextureDimension::Texture2DArray;
                    createInfo.arraySize = attachment.arrayLayers;
                }
                else
                {
                    createInfo.dimension = nvrhi::TextureDimension::Texture2D;
                }

                m_ColorAttachments.emplace_back(Texture::Create(createInfo));
            }
        }

        CreateFramebuffer();
    }

    RenderTarget::~RenderTarget()
    {
        m_FramebufferHandle = nullptr;
        m_DepthAttachment = nullptr;
        m_ColorAttachments.clear();
        m_UintClearColors.clear();
    }

    void RenderTarget::CreateFramebuffer()
    {
        if (m_FramebufferHandle != nullptr)
            m_FramebufferHandle.Reset();

        m_FramebufferDesc = nvrhi::FramebufferDesc();
        
        if (m_FramebufferHandle == nullptr)
        {
            if (m_DepthAttachment != nullptr)
            {
                // For array textures, we attach the entire array to the framebuffer
                // Individual layers will be selected via render pass or viewport array index
                m_FramebufferDesc.setDepthAttachment(m_DepthAttachment->GetHandle());
            }

            // add color attachments
            for (auto colorAttachment : m_ColorAttachments)
            {
                m_FramebufferDesc.addColorAttachment(colorAttachment->GetHandle());
            }

            nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
            m_FramebufferHandle = device->createFramebuffer(m_FramebufferDesc);
            LOG_ASSERT(m_FramebufferHandle, "Failed to create render target framebuffer");
        }
    }

    void RenderTarget::Resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        if (m_CreateInfo.width == width && m_CreateInfo.height == height)
        {
            return;
        }

        nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
        device->waitForIdle();

        if (m_FramebufferHandle != nullptr)
            m_FramebufferHandle.Reset();

        m_FramebufferDesc = nvrhi::FramebufferDesc();

        m_CreateInfo.width = width;
        m_CreateInfo.height = height;

        // recreate depth attachment
        if (m_DepthAttachment)
        {
            // copy description
            auto createInfo = m_DepthAttachment->GetCreateInfo();
            createInfo.width = width;
            createInfo.height = height;
            
            m_DepthAttachment = nullptr;

            m_DepthAttachment = Texture::Create(createInfo);
            LOG_ASSERT(m_DepthAttachment, "Failed to create render target depth attachment");
        }

        // recreate color attachments
        std::vector<TextureCreateInfo> colorCIs;
        // copy color descriptions
        for (auto &colorAttachment : m_ColorAttachments)
        {
            auto colorDesc = colorAttachment->GetCreateInfo();
            colorDesc.width = width;
            colorDesc.height = height;

            colorCIs.push_back(colorDesc);
        }

        // create color attachments
        m_ColorAttachments.clear();
        for (auto &colorDesc : colorCIs)
        {
            m_ColorAttachments.push_back(Texture::Create(colorDesc));
        }

        CreateFramebuffer();
    }

    Ref<Texture> RenderTarget::GetDepthAttachment()
    {
        return m_DepthAttachment;
    }

    nvrhi::FramebufferHandle RenderTarget::GetFramebuffer()
    {
        return m_FramebufferHandle;
    }

    Ref<Texture> RenderTarget::GetColorAttachment(uint32_t index)
    {
        if (m_ColorAttachments.size() > index)
            return m_ColorAttachments[index];

        LOG_ASSERT(false, "[Render target] Color attachments index out of bound!");
        return nullptr;
    }

    std::vector<Ref<Texture>> &RenderTarget::GetColorAttachments()
    {
        return m_ColorAttachments;
    }

    void RenderTarget::ClearColorAndDepth(nvrhi::ICommandList *commandList)
    {
        // Float Color
        for (auto &[attachmentIndex, clearColor] : m_FloatClearColors)
        {
            ClearColorAttachmentFloat(commandList, attachmentIndex, clearColor);
        }

        // UINT Color
        for (auto &[attachmentIndex, clearColor] : m_UintClearColors)
        {
            ClearColorAttachmentUint(commandList, attachmentIndex, clearColor);
        }

        auto [depth, stencil] = m_ClearDepth;
        if (depth != std::numeric_limits<float>::max() && stencil != std::numeric_limits<uint32_t>::max() )
        {
            ClearDepthAttachment(commandList, depth, stencil);
        }
    }

    void RenderTarget::SetClearColorAttachmentFloat(const glm::vec4 &clearColor, uint32_t attachmentIndex)
    {
        m_FloatClearColors[attachmentIndex] = clearColor;
    }

    void RenderTarget::SetClearColorAttachmentUint(uint32_t clearColor, uint32_t attachmentIndex)
    {
        m_UintClearColors[attachmentIndex] = clearColor;
    }

    void RenderTarget::SetClearDepthAttachment(float depth, uint32_t stencil)
    {
        m_ClearDepth = { depth, stencil };
    }

    void RenderTarget::ClearColorAttachmentFloat(nvrhi::ICommandList *commandList, uint32_t attachmentIndex, const glm::vec4 &clearColor) const
    {
		if (attachmentIndex >= m_ColorAttachments.size())
		{
			LOG_WARN("[Render Target] Color attachment index {} is out of range", attachmentIndex);
			return;
		}

		nvrhi::TextureHandle texture = m_ColorAttachments[attachmentIndex]->GetHandle();
        nvrhi::utils::ClearColorAttachment(commandList, m_FramebufferHandle, attachmentIndex, nvrhi::Color(clearColor.x, clearColor.y, clearColor.z, clearColor.w));
    }

    void RenderTarget::ClearColorAttachmentUint(nvrhi::ICommandList *commandList, uint32_t attachmentIndex, uint32_t clearColor) const
    {
		if (attachmentIndex >= m_ColorAttachments.size())
		{
			LOG_WARN("[Render Target] Color attachment index {} is out of range", attachmentIndex);
			return;
		}

		nvrhi::TextureHandle texture = m_ColorAttachments[attachmentIndex]->GetHandle();

        const nvrhi::Format format = texture->getDesc().format;
        const bool isUint = format == nvrhi::Format::R32_UINT || format == nvrhi::Format::RGBA8_UINT || format == nvrhi::Format::R8_UINT;
        LOG_ASSERT(isUint, "[Render Target] Color attachment is not UINT type!");

        commandList->clearTextureUInt(texture, nvrhi::AllSubresources, clearColor);
    }

    void RenderTarget::ClearDepthAttachment(nvrhi::ICommandList *commandList, float depth, uint32_t stencil) const
    {
		if (!m_DepthAttachment)
		{
			return;
		}

        nvrhi::utils::ClearDepthStencilAttachment(commandList, m_FramebufferHandle, depth, stencil);
    }

    Ref<RenderTarget> RenderTarget::Create(const RenderTargetCreateInfo &createInfo, const std::string &debugName)
    {
        return CreateRef<RenderTarget>(createInfo, debugName);
    }
}
