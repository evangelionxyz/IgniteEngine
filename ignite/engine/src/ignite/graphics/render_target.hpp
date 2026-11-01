// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RENDER_TARGET_HPP
#define IGN_RENDER_TARGET_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include <NRI.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <string>

namespace ignite
{
    class Texture;

    struct FramebufferAttachments
    {
        std::string name = "[FramebufferAttachment]";
        nri::Format format = nri::Format::UNKNOWN;
        nri::Layout state = nri::Layout::UNDEFINED;
        uint32_t mipLevel = 0;
        uint32_t baseArrayLayer = 0;
        uint32_t arrayLayers = 1;
        void *nativeObjectPtr = nullptr;
        bool isNativeObject = false;
        uint32_t nativeObjectType = 0;
    };

    struct RenderTargetCreateInfo
    {
        std::vector<FramebufferAttachments> attachments;
        uint32_t sampleCount = 1;
        uint32_t sampleQuality = 0;
        uint32_t width = 1280;
        uint32_t height = 720;

        Ref<Texture> depthAttachmentOverride = nullptr;
        std::vector<Ref<Texture>> colorAttachmentOverrides;
    };

    class IGN_API RenderTarget
    {
    public:
        RenderTarget(const RenderTargetCreateInfo &createInfo, const std::string &debugName = "[RenderTarget]");
        ~RenderTarget();

        void CreateFramebuffer();
        void Resize(const uint32_t width, const uint32_t height, bool waitForIdle = true);

        glm::uvec2 GetSize() const { return { m_CreateInfo.width, m_CreateInfo.height }; }

        uint32_t GetWidth() const { return m_CreateInfo.width; }
        uint32_t GetHeight() const { return m_CreateInfo.height; }
        uint32_t GetSampleCount() const { return m_CreateInfo.sampleCount; }

        Ref<Texture> GetDepthAttachment() const { return m_DepthAttachment; }
        RenderTarget *GetFramebuffer() { return this; }
        const RenderTarget *GetFramebuffer() const { return this; }
        Ref<Texture> GetColorAttachment(uint32_t index) const;
        std::vector<Ref<Texture>> &GetColorAttachments() { return m_ColorAttachments; }
        const std::vector<Ref<Texture>> &GetColorAttachments() const { return m_ColorAttachments; }

        uint32_t GetColorAttachmentCount() const { return static_cast<uint32_t>(m_ColorAttachments.size()); }
        nri::Format GetColorFormat(uint32_t index = 0) const;
        nri::Format GetDepthFormat() const;
        bool HasDepthAttachment() const { return m_DepthAttachment != nullptr; }

        nri::Descriptor *GetColorAttachmentView(uint32_t index) const
        {
            return index < m_ColorAttachmentViews.size() ? m_ColorAttachmentViews[index] : nullptr;
        }
        nri::Descriptor *GetDepthAttachmentView() const { return m_DepthAttachmentView; }

        // Dynamic rendering helpers
        void BeginRendering(nri::CommandBuffer *commandBuffer, bool clearColor = false, bool clearDepth = false);
        void EndRendering(nri::CommandBuffer *commandBuffer);
        bool IsRendering() const { return m_IsRendering; }

        void ClearColorAndDepth(nri::CommandBuffer *commandBuffer);

        void SetClearColorAttachmentFloat(const glm::vec4 &clearColor, uint32_t attachmentIndex = 0);
        void SetClearColorAttachmentUint(uint32_t clearColor, uint32_t attachmentIndex = 0);
        void SetClearDepthAttachment(float depth, uint32_t stencil);

        void ClearColorAttachmentFloat(nri::CommandBuffer *commandBuffer, uint32_t attachmentIndex = 0, const glm::vec4 &clearColor = glm::vec4(0.0f));
        void ClearColorAttachmentUint(nri::CommandBuffer *commandBuffer, uint32_t attachmentIndex = 0, uint32_t clearColor = 0);
        void ClearDepthAttachment(nri::CommandBuffer *commandBuffer, float depth, uint32_t stencil);

        RenderTargetCreateInfo &GetCreateInfo() { return m_CreateInfo; }
        const RenderTargetCreateInfo &GetCreateInfo() const { return m_CreateInfo; }

        static Ref<RenderTarget> Create(const RenderTargetCreateInfo &createInfo, const std::string &debugName = "[RenderTarget]");

        operator RenderTarget *() { return this; }
        operator const RenderTarget *() const { return this; }

    private:
        void DestroyAttachmentViews();

        std::vector<Ref<Texture>> m_ColorAttachments;
        std::vector<nri::Descriptor *> m_ColorAttachmentViews;
        Ref<Texture> m_DepthAttachment;
        nri::Descriptor *m_DepthAttachmentView = nullptr;
        RenderTargetCreateInfo m_CreateInfo;
        std::string m_DebugName;

        std::unordered_map<uint32_t, glm::vec4> m_FloatClearColors;
        std::unordered_map<uint32_t, uint32_t> m_UintClearColors;
        std::pair<float, uint32_t> m_ClearDepth;
        bool m_AttachmentsInitialized = false;
        bool m_IsRendering = false;
    };
}

#endif
