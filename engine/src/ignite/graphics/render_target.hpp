#pragma once

#include "ignite/core/types.hpp"
#include "shader.hpp"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

namespace ignite
{
    struct FramebufferAttachments
    {
        nvrhi::Format format;
        nvrhi::ResourceStates state =  nvrhi::ResourceStates::Unknown;
    };

    struct RenderTargetCreateInfo
    {
        std::vector<FramebufferAttachments> attachments;
        uint32_t width = 1280;
        uint32_t height = 720;
    };

    class RenderTarget
    {
    public:
        RenderTarget(const RenderTargetCreateInfo &createInfo);

        void CreateFramebuffer();
        void Resize(const uint32_t width, const uint32_t height);

        uint32_t GetWidth() const { return m_CreateInfo.width; }
        uint32_t GetHeight() const { return m_CreateInfo.height; }

        nvrhi::TextureHandle GetDepthAttachment();
        nvrhi::FramebufferHandle GetFramebuffer();
        nvrhi::TextureHandle GetColorAttachment(uint32_t index);
        std::vector<nvrhi::TextureHandle> &GetColorAttachments();

        void ClearColorAttachmentFloat(nvrhi::CommandListHandle commandList, uint32_t attachmentIndex = 0, const glm::vec3 &clearColor = glm::vec3(0.0f, 0.0f, 0.0f)) const;
        void ClearColorAttachmentUint(nvrhi::CommandListHandle commandList, uint32_t attachmentIndex = 0, uint32_t clearColor = 0) const;
        void ClearDepthAttachment(nvrhi::CommandListHandle commandList, float depth, uint32_t stencil) const;

        static Ref<RenderTarget> Create(const RenderTargetCreateInfo &createInfo);

    private:
        std::vector<nvrhi::TextureHandle> m_ColorAttachments;
        nvrhi::FramebufferHandle m_FramebufferHandle;
        nvrhi::FramebufferDesc m_FramebufferDesc;
        nvrhi::TextureHandle m_DepthAttachment;
        RenderTargetCreateInfo m_CreateInfo;
    };
}