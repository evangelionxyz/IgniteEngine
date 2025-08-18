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

#include "ignite/core/application.hpp"
#include "render_target.hpp"

#include <cstdint>
#include <vector>
#include <ignite/core/logger.hpp>
#include <ignite/core/types.hpp>
#include <nvrhi/nvrhi.h>
#include <nvrhi/utils.h>


namespace ignite {

    RenderTarget::RenderTarget(const RenderTargetCreateInfo &createInfo)
        : m_CreateInfo(createInfo)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();
        
        for (auto &attachment : m_CreateInfo.attachments)
        {
            const bool isDepthAttachment = attachment.format == nvrhi::Format::D32S8 || attachment.format == nvrhi::Format::D16 || attachment.format == nvrhi::Format::D24S8 || attachment.format == nvrhi::Format::D32;
            const bool isColorAttachment = attachment.format == nvrhi::Format::RGBA8_UNORM || attachment.format == nvrhi::Format::SRGBA8_UNORM || attachment.format == nvrhi::Format::R32_UINT;
            const bool isRenderTarget = true;

            // find depth attachment if framebuffer is not created yet
            if (isDepthAttachment && m_DepthAttachment == nullptr && m_FramebufferHandle == nullptr)
            {
                nvrhi::TextureDesc depthDesc = nvrhi::TextureDesc();
                depthDesc.setWidth(m_CreateInfo.width);
                depthDesc.setHeight(m_CreateInfo.height);
                depthDesc.setFormat(attachment.format);
                depthDesc.setDebugName("Render target depth attachment");
                depthDesc.setInitialState(attachment.state);
                depthDesc.setIsRenderTarget(true);
                depthDesc.setKeepInitialState(true);
                depthDesc.setClearValue(nvrhi::Color(1.f));
                depthDesc.setUseClearValue(true);
                depthDesc.setDimension(nvrhi::TextureDimension::Texture2D);

                m_DepthAttachment = device->createTexture(depthDesc);
                LOG_ASSERT(m_DepthAttachment, "Failed to create render target depth attachment");
            }

            // create color attachment if color attachments are empty
            if (isColorAttachment)
            {
                // create color attachment texture
                nvrhi::TextureDesc colorDesc;
                colorDesc.setWidth(m_CreateInfo.width);
                colorDesc.setHeight(m_CreateInfo.height);
                colorDesc.setFormat(attachment.format);
                colorDesc.setDebugName("Render target color attachment texture");
                colorDesc.setInitialState(attachment.state);
                colorDesc.setKeepInitialState(true);
                colorDesc.setIsUAV(false);
                colorDesc.setIsRenderTarget(isRenderTarget);
                colorDesc.setIsTypeless(false);
                colorDesc.setUseClearValue(true);

                nvrhi::TextureHandle colorAttachment = device->createTexture(colorDesc);
                LOG_ASSERT(colorAttachment, "Failed to create render target color attachment texture");

                m_ColorAttachments.push_back(colorAttachment);
            }
        }

        CreateFramebuffer();
    }

    void RenderTarget::CreateFramebuffer()
    {
        if (m_FramebufferHandle != nullptr)
            m_FramebufferHandle.Reset();
        
        if (m_FramebufferHandle == nullptr)
        {
            if (m_DepthAttachment != nullptr)
            {
                m_FramebufferDesc.setDepthAttachment(m_DepthAttachment);
            }

            // add color attachments
            for (auto &colorAttachment : m_ColorAttachments)
            {
                m_FramebufferDesc.addColorAttachment(colorAttachment);
            }

            nvrhi::IDevice *device = Application::GetGraphicsDevice();
            m_FramebufferHandle = device->createFramebuffer(m_FramebufferDesc);
            LOG_ASSERT(m_FramebufferHandle, "Failed to create render target framebuffer");
        }
    }

    void RenderTarget::Resize(const uint32_t width, const uint32_t height)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        m_CreateInfo.width = width;
        m_CreateInfo.height = height;

        // recreate depth attachment
        if (m_DepthAttachment)
        {
            // copy description
            auto depthDesc = m_DepthAttachment->getDesc();
            depthDesc.width = width;
            depthDesc.height = height;
            
            m_DepthAttachment.Reset();
            m_DepthAttachment = device->createTexture(depthDesc);
            LOG_ASSERT(m_DepthAttachment, "Failed to create render target depth attachment");
        }

        // recreate color attachments
        std::vector<nvrhi::TextureDesc> colorDescs;

        // copy color descriptions
        for (auto &colorAttachment : m_ColorAttachments)
        {
            auto colorDesc = colorAttachment->getDesc();
            colorDesc.width = width;
            colorDesc.height = height;

            colorDescs.push_back(colorDesc);
        }

        // create color attachments
        m_ColorAttachments.clear();
        for (auto &colorDesc : colorDescs)
        {
            nvrhi::TextureHandle colorAttachment = device->createTexture(colorDesc);
            LOG_ASSERT(colorAttachment, "Failed to create render target color attachment texture");

            m_ColorAttachments.push_back(colorAttachment);
        }

        // reset create desc
        m_FramebufferDesc = nvrhi::FramebufferDesc();

        CreateFramebuffer();
    }

    nvrhi::TextureHandle RenderTarget::GetDepthAttachment()
    {
        return m_DepthAttachment;
    }

    nvrhi::FramebufferHandle RenderTarget::GetFramebuffer()
    {
        return m_FramebufferHandle;
    }

    nvrhi::TextureHandle RenderTarget::GetColorAttachment(uint32_t index)
    {
        if (m_ColorAttachments.size() > index)
            return m_ColorAttachments[index];

        LOG_ASSERT(false, "[Render target] Color attachments index out of bound!");
        return nullptr;
    }

    std::vector<nvrhi::TextureHandle> &RenderTarget::GetColorAttachments()
    {
        return m_ColorAttachments;
    }

    void RenderTarget::ClearColorAttachmentFloat(nvrhi::CommandListHandle commandList, uint32_t attachmentIndex, const glm::vec4 &clearColor) const
    {
        nvrhi::TextureHandle texture = m_ColorAttachments[attachmentIndex];
        nvrhi::utils::ClearColorAttachment(commandList, m_FramebufferHandle, attachmentIndex, nvrhi::Color(clearColor.x, clearColor.y, clearColor.z, clearColor.w));
    }

    void RenderTarget::ClearColorAttachmentUint(nvrhi::CommandListHandle commandList, uint32_t attachmentIndex, uint32_t clearColor) const
    {
        if (attachmentIndex >= m_ColorAttachments.size())
        {
            attachmentIndex = glm::max(static_cast<int>(m_ColorAttachments.size()) - 1, 0);
            LOG_ASSERT(false, "[Render target] Color attachments index out of bound!");
        }

        nvrhi::TextureHandle texture = m_ColorAttachments[attachmentIndex];
        const nvrhi::Format format = texture->getDesc().format;

        bool isUint = format == nvrhi::Format::R32_UINT || format == nvrhi::Format::RGBA8_UINT || format == nvrhi::Format::R8_UINT;
        LOG_ASSERT(isUint, "[Render Target] Color attachment is not UINT type!");

        commandList->clearTextureUInt(texture, nvrhi::AllSubresources, clearColor);
    }

    void RenderTarget::ClearDepthAttachment(nvrhi::CommandListHandle commandList, float depth, uint32_t stencil) const
    {
        nvrhi::utils::ClearDepthStencilAttachment(commandList, m_FramebufferHandle, depth, stencil);
    }

    Ref<RenderTarget> RenderTarget::Create(const RenderTargetCreateInfo &createInfo)
    {
        return CreateRef<RenderTarget>(createInfo);
    }
}
