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
                depthDesc.setDebugName(std::format("{} - {} ", attachment.name, debugName));
                depthDesc.setInitialState(attachment.state);
                depthDesc.setIsRenderTarget(true);
                depthDesc.setKeepInitialState(true);
                depthDesc.setClearValue(nvrhi::Color(1.f));
                depthDesc.setUseClearValue(true);
                
                // Set dimension and array size based on attachment configuration
                if (attachment.arrayLayers > 1)
                {
                    depthDesc.setDimension(nvrhi::TextureDimension::Texture2DArray);
                    depthDesc.setArraySize(attachment.arrayLayers);
                }
                else
                {
                    depthDesc.setDimension(nvrhi::TextureDimension::Texture2D);
                }

                m_DepthAttachment = Texture::Create(depthDesc);
            }

            // create color attachment if color attachments are empty
            if (isColorAttachment)
            {
                // create color attachment texture
                nvrhi::TextureDesc colorDesc;
                colorDesc.setWidth(m_CreateInfo.width);
                colorDesc.setHeight(m_CreateInfo.height);
                colorDesc.setFormat(attachment.format);
                colorDesc.setDebugName(std::format("{} - {} ", attachment.name, debugName));
                colorDesc.setInitialState(attachment.state);
                colorDesc.setKeepInitialState(true);
                colorDesc.setIsUAV(false);
                colorDesc.setIsRenderTarget(isRenderTarget);
                colorDesc.setIsTypeless(false);
                colorDesc.setUseClearValue(true);

                if (attachment.arrayLayers > 1)
                {
                    colorDesc.setDimension(nvrhi::TextureDimension::Texture2DArray);
                    colorDesc.setArraySize(attachment.arrayLayers);
                }
                else
                {
                    colorDesc.setDimension(nvrhi::TextureDimension::Texture2D);
                }

                Ref<Texture> colorAttachment = Texture::Create(colorDesc);
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
                // For array textures, we attach the entire array to the framebuffer
                // Individual layers will be selected via render pass or viewport array index
                m_FramebufferDesc.setDepthAttachment(m_DepthAttachment->GetHandle());
            }

            // add color attachments
            for (auto &colorAttachment : m_ColorAttachments)
            {
                m_FramebufferDesc.addColorAttachment(colorAttachment->GetHandle());
            }

            nvrhi::IDevice *device = Application::GetGraphicsDevice();
            m_FramebufferHandle = device->createFramebuffer(m_FramebufferDesc);
            LOG_ASSERT(m_FramebufferHandle, "Failed to create render target framebuffer");
        }
    }

    void RenderTarget::Resize(const uint32_t width, const uint32_t height)
    {
        nvrhi::IDevice *device = Application::GetGraphicsDevice();

        device->waitForIdle();

        m_CreateInfo.width = width;
        m_CreateInfo.height = height;

        // recreate depth attachment
        if (m_DepthAttachment)
        {
            // copy description
            auto depthDesc = m_DepthAttachment->GetHandle()->getDesc();
            depthDesc.width = width;
            depthDesc.height = height;
            
            m_DepthAttachment.reset();

            m_DepthAttachment = Texture::Create(depthDesc);
            LOG_ASSERT(m_DepthAttachment, "Failed to create render target depth attachment");
        }

        // recreate color attachments
        std::vector<nvrhi::TextureDesc> colorDescs;

        // copy color descriptions
        for (auto &colorAttachment : m_ColorAttachments)
        {
            auto colorDesc = colorAttachment->GetHandle()->getDesc();
            colorDesc.width = width;
            colorDesc.height = height;

            colorDescs.push_back(colorDesc);
        }

        // create color attachments
        m_ColorAttachments.clear();
        for (auto &colorDesc : colorDescs)
        {
            Ref<Texture> colorAttachment = Texture::Create(colorDesc);
            m_ColorAttachments.push_back(colorAttachment);
        }

        // reset create desc
        m_FramebufferDesc = nvrhi::FramebufferDesc();

        CreateFramebuffer();
    }

    bool RenderTarget::ShouldResize(const uint32_t width, const uint32_t height)
    {
        return m_CreateInfo.width != width || m_CreateInfo.height != height;
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
        nvrhi::TextureHandle texture = m_ColorAttachments[attachmentIndex]->GetHandle();
        nvrhi::utils::ClearColorAttachment(commandList, m_FramebufferHandle, attachmentIndex, nvrhi::Color(clearColor.x, clearColor.y, clearColor.z, clearColor.w));
    }

    void RenderTarget::ClearColorAttachmentUint(nvrhi::ICommandList *commandList, uint32_t attachmentIndex, uint32_t clearColor) const
    {
        nvrhi::TextureHandle texture = m_ColorAttachments[attachmentIndex]->GetHandle();

        const nvrhi::Format format = texture->getDesc().format;
        const bool isUint = format == nvrhi::Format::R32_UINT || format == nvrhi::Format::RGBA8_UINT || format == nvrhi::Format::R8_UINT;
        LOG_ASSERT(isUint, "[Render Target] Color attachment is not UINT type!");

        commandList->clearTextureUInt(texture, nvrhi::AllSubresources, clearColor);
    }

    void RenderTarget::ClearDepthAttachment(nvrhi::ICommandList *commandList, float depth, uint32_t stencil) const
    {
        nvrhi::utils::ClearDepthStencilAttachment(commandList, m_FramebufferHandle, depth, stencil);
    }

    Ref<RenderTarget> RenderTarget::Create(const RenderTargetCreateInfo &createInfo, const std::string &debugName)
    {
        return CreateRef<RenderTarget>(createInfo, debugName);
    }
}
