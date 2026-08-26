// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_RENDER_TARGET_HPP
#define IGN_RENDER_TARGET_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

namespace ignite
{
    class Texture;

    struct FramebufferAttachments
    {
        std::string name = "[FramebufferAttachment]";
        nvrhi::Format format;
        nvrhi::ResourceStates state =  nvrhi::ResourceStates::Unknown;
        uint32_t arrayLayers = 1;
        void *nativeObjectPtr = nullptr;
        bool isNativeObject = false;
        nvrhi::ObjectType nativeObjectType = 0;
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
        RenderTarget(const RenderTargetCreateInfo &createInfo, const std::string& debugName = "[RenderTarget]");
        ~RenderTarget();

        void CreateFramebuffer();
        void Resize(const uint32_t width, const uint32_t height);

        glm::uvec2 GetSize() { return { m_CreateInfo.width, m_CreateInfo.height }; }

        uint32_t GetWidth() const { return m_CreateInfo.width; }
        uint32_t GetHeight() const { return m_CreateInfo.height; }

        Ref<Texture> GetDepthAttachment();
        nvrhi::FramebufferHandle GetFramebuffer();
        Ref<Texture> GetColorAttachment(uint32_t index);
        std::vector<Ref<Texture>> &GetColorAttachments();

        void ClearColorAndDepth(nvrhi::ICommandList *commandList);

        void SetClearColorAttachmentFloat(const glm::vec4 &clearColor, uint32_t attachmentIndex = 0);
        void SetClearColorAttachmentUint(uint32_t clearColor, uint32_t attachmentIndex = 0);
        void SetClearDepthAttachment(float depth, uint32_t stencil);

        void ClearColorAttachmentFloat(nvrhi::ICommandList *commandList, uint32_t attachmentIndex = 0, const glm::vec4 &clearColor = glm::vec4(0.0f)) const;
        void ClearColorAttachmentUint(nvrhi::ICommandList *commandList, uint32_t attachmentIndex = 0, uint32_t clearColor = 0) const;
        void ClearDepthAttachment(nvrhi::ICommandList *commandList, float depth, uint32_t stencil) const;

        RenderTargetCreateInfo& GetCreateInfo() { return m_CreateInfo; }

        static Ref<RenderTarget> Create(const RenderTargetCreateInfo &createInfo, const std::string& debugName = "[RenderTarget]");

        operator nvrhi::FramebufferHandle() const { return m_FramebufferHandle; }
        operator nvrhi::IFramebuffer *() const { return m_FramebufferHandle; }

    private:
        std::vector<Ref<Texture>> m_ColorAttachments;
        nvrhi::FramebufferHandle m_FramebufferHandle;
        nvrhi::FramebufferDesc m_FramebufferDesc;
        Ref<Texture> m_DepthAttachment;
        RenderTargetCreateInfo m_CreateInfo;

        std::unordered_map<uint32_t, glm::vec4> m_FloatClearColors;
        std::unordered_map<uint32_t, uint32_t> m_UintClearColors;
        std::pair<float, uint32_t> m_ClearDepth;
    };
}

#endif
