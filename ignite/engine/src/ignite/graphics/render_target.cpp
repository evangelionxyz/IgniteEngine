// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "render_target.hpp"
#include "texture.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/core/types.hpp"

namespace ignite
{
    namespace
    {
        static bool IsDepthStencilFormat(nri::Format format)
        {
            switch (format)
            {
            case nri::Format::D16_UNORM:
            case nri::Format::D32_SFLOAT:
            case nri::Format::D24_UNORM_S8_UINT:
            case nri::Format::D32_SFLOAT_S8_UINT:
                return true;
            default:
                return false;
            }
        }

        static bool HasStencilComponent(nri::Format format)
        {
            return format == nri::Format::D24_UNORM_S8_UINT || format == nri::Format::D32_SFLOAT_S8_UINT;
        }
    }

    RenderTarget::RenderTarget(const RenderTargetCreateInfo &createInfo, const std::string &debugName)
        : m_CreateInfo(createInfo), m_DebugName(debugName)
    {
        m_ClearDepth = { std::numeric_limits<float>::max(), std::numeric_limits<uint32_t>::max() };

        if (m_CreateInfo.depthAttachmentOverride != nullptr)
        {
            m_DepthAttachment = m_CreateInfo.depthAttachmentOverride;
        }

        size_t overrideColorIdx = 0;

        for (auto &attachment : m_CreateInfo.attachments)
        {
            const bool isDepthAttachment = IsDepthStencilFormat(attachment.format);
            const bool isColorAttachment = !isDepthAttachment && attachment.format != nri::Format::UNKNOWN;
            constexpr bool isRenderTarget = true;

            if (isDepthAttachment && m_DepthAttachment == nullptr)
            {
                TextureCreateInfo texCI = {};
                texCI.width = m_CreateInfo.width;
                texCI.height = m_CreateInfo.height;
                texCI.depth = 1;
                texCI.isRenderTarget = isRenderTarget;
                texCI.format = attachment.format;
                texCI.sampleCount = m_CreateInfo.sampleCount;
                texCI.sampleQuality = m_CreateInfo.sampleQuality;
                texCI.initialAccess = nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE;
                texCI.initialLayout = attachment.state != nri::Layout::UNDEFINED
                    ? attachment.state
                    : nri::Layout::DEPTH_STENCIL_ATTACHMENT;
                texCI.keepInitialState = true;
                texCI.dimension = nri::TextureType::TEXTURE_2D;
                texCI.arraySize = std::max(attachment.arrayLayers, 1u);
                texCI.isNativeObject = attachment.isNativeObject;
                texCI.nativeObjectType = attachment.nativeObjectType;
                texCI.nativeObjectPtr = attachment.nativeObjectPtr;

                m_DepthAttachment = Texture::Create(texCI, attachment.name);
            }

            if (isColorAttachment)
            {
                if (overrideColorIdx < m_CreateInfo.colorAttachmentOverrides.size())
                {
                    m_ColorAttachments.push_back(m_CreateInfo.colorAttachmentOverrides[overrideColorIdx++]);
                }
                else
                {
                    TextureCreateInfo texCI = {};
                    texCI.width = m_CreateInfo.width;
                    texCI.height = m_CreateInfo.height;
                    texCI.depth = 1;
                    texCI.isRenderTarget = isRenderTarget;
                    texCI.format = attachment.format;
                    texCI.isUAV = false;
                    texCI.initialAccess = nri::AccessBits::COLOR_ATTACHMENT;
                    texCI.initialLayout = attachment.state != nri::Layout::UNDEFINED
                        ? attachment.state
                        : nri::Layout::COLOR_ATTACHMENT;
                    texCI.keepInitialState = true;
                    texCI.sampleCount = m_CreateInfo.sampleCount;
                    texCI.sampleQuality = m_CreateInfo.sampleQuality;
                    texCI.isNativeObject = attachment.isNativeObject;
                    texCI.nativeObjectPtr = attachment.nativeObjectPtr;
                    texCI.nativeObjectType = attachment.nativeObjectType;
                    texCI.dimension = nri::TextureType::TEXTURE_2D;
                    texCI.arraySize = std::max(attachment.arrayLayers, 1u);

                    m_ColorAttachments.emplace_back(Texture::Create(texCI, attachment.name));
                }
            }
        }

        // If colorAttachmentOverrides were supplied without matching attachment entries, add them
        while (overrideColorIdx < m_CreateInfo.colorAttachmentOverrides.size())
        {
            m_ColorAttachments.push_back(m_CreateInfo.colorAttachmentOverrides[overrideColorIdx++]);
        }

        CreateFramebuffer();
    }

    RenderTarget::~RenderTarget()
    {
        DestroyAttachmentViews();
        m_DepthAttachment = nullptr;
        m_ColorAttachments.clear();
        m_UintClearColors.clear();
        m_FloatClearColors.clear();
    }

    void RenderTarget::DestroyAttachmentViews()
    {
        auto *deviceManager = DeviceManager::GetInstance();
        if (deviceManager)
        {
            const auto &nri = deviceManager->GetCoreInterface();
            for (nri::Descriptor *view : m_ColorAttachmentViews)
            {
                if (view)
                {
                    nri.DestroyDescriptor(view);
                }
            }
            if (m_DepthAttachmentView)
            {
                nri.DestroyDescriptor(m_DepthAttachmentView);
            }
        }
        m_ColorAttachmentViews.clear();
        m_DepthAttachmentView = nullptr;
    }

    void RenderTarget::CreateFramebuffer()
    {
        auto *deviceManager = DeviceManager::GetInstance();
        if (!deviceManager || !deviceManager->GetDevice())
        {
            return;
        }

        GPUUploadSync::DeviceWaitIdle(deviceManager->GetDevice());
        DestroyAttachmentViews();
        m_AttachmentsInitialized = false;

        const auto &nri = deviceManager->GetCoreInterface();

        // Find depth attachment config if any
        uint32_t depthMipLevel = 0;
        uint32_t depthBaseLayer = 0;
        uint32_t depthLayerCount = nri::REMAINING;
        size_t colorConfigIdx = 0;
        std::vector<FramebufferAttachments> colorConfigs;

        for (const auto &att : m_CreateInfo.attachments)
        {
            if (IsDepthStencilFormat(att.format))
            {
                depthMipLevel = att.mipLevel;
                depthBaseLayer = att.baseArrayLayer;
                depthLayerCount = att.arrayLayers;
            }
            else if (att.format != nri::Format::UNKNOWN)
            {
                colorConfigs.push_back(att);
            }
        }

        if (m_DepthAttachment && m_DepthAttachment->GetHandle())
        {
            nri::TextureViewDesc dsvDesc = {};
            dsvDesc.texture = m_DepthAttachment->GetHandle();
            dsvDesc.type = nri::TextureView::DEPTH_STENCIL_ATTACHMENT;
            dsvDesc.format = m_DepthAttachment->GetFormat();
            dsvDesc.mipOffset = static_cast<nri::Dim_t>(depthMipLevel);
            dsvDesc.mipNum = 1;
            dsvDesc.layerOffset = static_cast<nri::Dim_t>(depthBaseLayer);
            dsvDesc.layerNum = static_cast<nri::Dim_t>(depthLayerCount);
            dsvDesc.planes = HasStencilComponent(dsvDesc.format)
                ? (nri::PlaneBits::DEPTH | nri::PlaneBits::STENCIL)
                : nri::PlaneBits::DEPTH;

            nri::Result res = nri.CreateTextureView(dsvDesc, m_DepthAttachmentView);
            LOG_ASSERT(res == nri::Result::SUCCESS && m_DepthAttachmentView, "Failed to create depth attachment view");
        }

        m_ColorAttachmentViews.resize(m_ColorAttachments.size(), nullptr);
        for (size_t i = 0; i < m_ColorAttachments.size(); ++i)
        {
            if (!m_ColorAttachments[i] || !m_ColorAttachments[i]->GetHandle())
                continue;

            uint32_t mipLevel = (colorConfigIdx < colorConfigs.size()) ? colorConfigs[colorConfigIdx].mipLevel : 0;
            uint32_t baseLayer = (colorConfigIdx < colorConfigs.size()) ? colorConfigs[colorConfigIdx].baseArrayLayer : 0;
            uint32_t layerCount = (colorConfigIdx < colorConfigs.size()) ? colorConfigs[colorConfigIdx].arrayLayers : 1;
            ++colorConfigIdx;

            nri::TextureViewDesc rtvDesc = {};
            rtvDesc.texture = m_ColorAttachments[i]->GetHandle();
            rtvDesc.type = nri::TextureView::COLOR_ATTACHMENT;
            rtvDesc.format = m_ColorAttachments[i]->GetFormat();
            rtvDesc.mipOffset = static_cast<nri::Dim_t>(mipLevel);
            rtvDesc.mipNum = 1;
            rtvDesc.layerOffset = static_cast<nri::Dim_t>(baseLayer);
            rtvDesc.layerNum = static_cast<nri::Dim_t>(layerCount);
            rtvDesc.planes = nri::PlaneBits::ALL;

            nri::Result res = nri.CreateTextureView(rtvDesc, m_ColorAttachmentViews[i]);
            LOG_ASSERT(res == nri::Result::SUCCESS && m_ColorAttachmentViews[i], "Failed to create color attachment view");
        }
    }

    void RenderTarget::Resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0 || (m_CreateInfo.width == width && m_CreateInfo.height == height))
            return;

        if (auto *dm = DeviceManager::GetInstance())
        {
            if (dm->GetDevice())
            {
                GPUUploadSync::DeviceWaitIdle(dm->GetDevice());
            }
        }

        DestroyAttachmentViews();

        m_CreateInfo.width = width;
        m_CreateInfo.height = height;

        // recreate depth attachment
        if (m_DepthAttachment)
        {
            if (m_CreateInfo.depthAttachmentOverride != nullptr)
            {
                m_DepthAttachment = m_CreateInfo.depthAttachmentOverride;
            }
            else
            {
                auto createInfo = m_DepthAttachment->GetCreateInfo();
                createInfo.width = width;
                createInfo.height = height;
                m_DepthAttachment = nullptr;
                m_DepthAttachment = Texture::Create(createInfo, m_DebugName + " Depth");
                LOG_ASSERT(m_DepthAttachment, "Failed to create render target depth attachment");
            }
        }

        // recreate color attachments
        std::vector<Ref<Texture>> newColorAttachments;
        size_t overrideColorIdx = 0;

        for (size_t i = 0; i < m_ColorAttachments.size(); ++i)
        {
            if (overrideColorIdx < m_CreateInfo.colorAttachmentOverrides.size())
            {
                newColorAttachments.push_back(m_CreateInfo.colorAttachmentOverrides[overrideColorIdx++]);
            }
            else
            {
                auto colorDesc = m_ColorAttachments[i]->GetCreateInfo();
                colorDesc.width = width;
                colorDesc.height = height;
                newColorAttachments.push_back(Texture::Create(colorDesc, m_DebugName + " Color"));
            }
        }
        m_ColorAttachments = std::move(newColorAttachments);

        CreateFramebuffer();
    }

    Ref<Texture> RenderTarget::GetColorAttachment(uint32_t index) const
    {
        if (index < m_ColorAttachments.size())
            return m_ColorAttachments[index];

        LOG_ASSERT(false, "[Render target] Color attachments index out of bound!");
        return nullptr;
    }

    nri::Format RenderTarget::GetColorFormat(uint32_t index) const
    {
        if (index < m_ColorAttachments.size() && m_ColorAttachments[index])
        {
            return m_ColorAttachments[index]->GetFormat();
        }
        return nri::Format::UNKNOWN;
    }

    nri::Format RenderTarget::GetDepthFormat() const
    {
        if (m_DepthAttachment)
        {
            return m_DepthAttachment->GetFormat();
        }
        return nri::Format::UNKNOWN;
    }

    void RenderTarget::BeginRendering(nri::CommandBuffer *commandBuffer, bool clearColor, bool clearDepth)
    {
        if (!commandBuffer || m_IsRendering)
            return;

        auto *deviceManager = DeviceManager::GetInstance();
        if (!deviceManager)
            return;

        const auto &nri = deviceManager->GetCoreInterface();

        // Transition attachments into attachment layouts
        std::vector<nri::TextureBarrierDesc> barriers;
        barriers.reserve(m_ColorAttachments.size() + (m_DepthAttachment ? 1 : 0));

        for (size_t i = 0; i < m_ColorAttachments.size(); ++i)
        {
            if (!m_ColorAttachments[i] || !m_ColorAttachments[i]->GetHandle())
                continue;

            nri::TextureBarrierDesc b = {};
            b.texture = m_ColorAttachments[i]->GetHandle();
            if (m_ColorAttachments[i]->GetCurrentLayout() != nri::Layout::UNDEFINED)
            {
                b.before = { m_ColorAttachments[i]->GetCurrentAccess(), m_ColorAttachments[i]->GetCurrentLayout(), m_ColorAttachments[i]->GetCurrentStages() };
            }
            else if (m_AttachmentsInitialized && m_CreateInfo.colorAttachmentOverrides.empty())
            {
                if (m_ColorAttachments[i]->GetCreateInfo().initialLayout == nri::Layout::PRESENT)
                {
                    b.before = { nri::AccessBits::NONE, nri::Layout::PRESENT, nri::StageBits::NONE };
                }
                else
                {
                    const auto &colorDesc = nri.GetTextureDesc(*m_ColorAttachments[i]->GetHandle());
                    if ((colorDesc.usage & nri::TextureUsageBits::SHADER_RESOURCE) != 0)
                    {
                        b.before = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                    }
                    else
                    {
                        b.before = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT };
                    }
                }
            }
            else
            {
                b.before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
            }
            b.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT };
            b.mipOffset = 0;
            b.mipNum = nri::REMAINING;
            b.layerOffset = 0;
            b.layerNum = nri::REMAINING;
            b.planes = nri::PlaneBits::ALL;
            barriers.push_back(b);
            m_ColorAttachments[i]->SetCurrentState(b.after.layout, b.after.access, b.after.stages);
        }

        if (m_DepthAttachment && m_DepthAttachment->GetHandle())
        {
            const auto &depthDesc = nri.GetTextureDesc(*m_DepthAttachment->GetHandle());
            const bool depthSupportsShaderResource = (depthDesc.usage & nri::TextureUsageBits::SHADER_RESOURCE) != 0;

            nri::TextureBarrierDesc b = {};
            b.texture = m_DepthAttachment->GetHandle();
            if (m_DepthAttachment->GetCurrentLayout() != nri::Layout::UNDEFINED)
            {
                b.before = { m_DepthAttachment->GetCurrentAccess(), m_DepthAttachment->GetCurrentLayout(), m_DepthAttachment->GetCurrentStages() };
            }
            else if (m_AttachmentsInitialized && !m_CreateInfo.depthAttachmentOverride)
            {
                if (depthSupportsShaderResource)
                {
                    b.before = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                }
                else
                {
                    b.before = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT, nri::StageBits::DEPTH_STENCIL_ATTACHMENT };
                }
            }
            else
            {
                b.before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
            }
            b.after = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT, nri::StageBits::DEPTH_STENCIL_ATTACHMENT };
            b.mipOffset = 0;
            b.mipNum = nri::REMAINING;
            b.layerOffset = 0;
            b.layerNum = nri::REMAINING;
            b.planes = HasStencilComponent(m_DepthAttachment->GetFormat())
                ? (nri::PlaneBits::DEPTH | nri::PlaneBits::STENCIL)
                : nri::PlaneBits::DEPTH;
            barriers.push_back(b);
            m_DepthAttachment->SetCurrentState(b.after.layout, b.after.access, b.after.stages);
        }

        if (!barriers.empty())
        {
            nri::BarrierDesc barrierDesc = {};
            barrierDesc.textures = barriers.data();
            barrierDesc.textureNum = static_cast<uint32_t>(barriers.size());
            nri.CmdBarrier(*commandBuffer, barrierDesc);
        }

        std::vector<nri::AttachmentDesc> colorAttachments(m_ColorAttachmentViews.size());
        for (size_t i = 0; i < m_ColorAttachmentViews.size(); ++i)
        {
            colorAttachments[i] = {};
            colorAttachments[i].descriptor = m_ColorAttachmentViews[i];
            colorAttachments[i].loadOp = clearColor ? nri::LoadOp::CLEAR : nri::LoadOp::LOAD;
            colorAttachments[i].storeOp = nri::StoreOp::STORE;

            uint32_t idx = static_cast<uint32_t>(i);
            if (auto itFloat = m_FloatClearColors.find(idx); itFloat != m_FloatClearColors.end())
            {
                colorAttachments[i].clearValue.color.f = { itFloat->second.x, itFloat->second.y, itFloat->second.z, itFloat->second.w };
            }
            else if (auto itUint = m_UintClearColors.find(idx); itUint != m_UintClearColors.end())
            {
                colorAttachments[i].clearValue.color.ui = { itUint->second, itUint->second, itUint->second, itUint->second };
            }
        }

        nri::RenderingDesc renderingDesc = {};
        renderingDesc.colors = colorAttachments.empty() ? nullptr : colorAttachments.data();
        renderingDesc.colorNum = static_cast<uint32_t>(colorAttachments.size());

        if (m_DepthAttachmentView)
        {
            renderingDesc.depth.descriptor = m_DepthAttachmentView;
            renderingDesc.depth.loadOp = clearDepth ? nri::LoadOp::CLEAR : nri::LoadOp::LOAD;
            renderingDesc.depth.storeOp = nri::StoreOp::STORE;
            if (m_ClearDepth.first != std::numeric_limits<float>::max())
            {
                renderingDesc.depth.clearValue.depthStencil.depth = m_ClearDepth.first;
                renderingDesc.depth.clearValue.depthStencil.stencil = static_cast<uint8_t>(m_ClearDepth.second);
            }
            else
            {
                renderingDesc.depth.clearValue.depthStencil.depth = 1.0f;
                renderingDesc.depth.clearValue.depthStencil.stencil = 0;
            }
        }

        nri.CmdBeginRendering(*commandBuffer, renderingDesc);

        nri::Viewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_CreateInfo.width);
        viewport.height = static_cast<float>(m_CreateInfo.height);
        viewport.depthMin = 0.0f;
        viewport.depthMax = 1.0f;
        nri.CmdSetViewports(*commandBuffer, &viewport, 1);

        nri::Rect scissor = {};
        scissor.x = 0;
        scissor.y = 0;
        scissor.width = static_cast<nri::Dim_t>(m_CreateInfo.width);
        scissor.height = static_cast<nri::Dim_t>(m_CreateInfo.height);
        nri.CmdSetScissors(*commandBuffer, &scissor, 1);

        m_IsRendering = true;
    }

    void RenderTarget::EndRendering(nri::CommandBuffer *commandBuffer)
    {
        if (!commandBuffer || !m_IsRendering)
            return;

        auto *deviceManager = DeviceManager::GetInstance();
        if (!deviceManager)
            return;

        const auto &nri = deviceManager->GetCoreInterface();
        nri.CmdEndRendering(*commandBuffer);

        std::vector<nri::TextureBarrierDesc> barriers;
        barriers.reserve(m_ColorAttachments.size() + (m_DepthAttachment ? 1 : 0));

        for (size_t i = 0; i < m_ColorAttachments.size(); ++i)
        {
            if (!m_ColorAttachments[i] || !m_ColorAttachments[i]->GetHandle())
                continue;

            nri::TextureBarrierDesc b = {};
            b.texture = m_ColorAttachments[i]->GetHandle();
            b.before = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT };
            if (m_ColorAttachments[i]->GetCreateInfo().initialLayout == nri::Layout::PRESENT)
            {
                b.after = { nri::AccessBits::NONE, nri::Layout::PRESENT, nri::StageBits::NONE };
            }
            else
            {
                const auto &colorDesc = nri.GetTextureDesc(*m_ColorAttachments[i]->GetHandle());
                if ((colorDesc.usage & nri::TextureUsageBits::SHADER_RESOURCE) != 0)
                {
                    b.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                }
                else
                {
                    b.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT };
                }
            }
            b.mipOffset = 0;
            b.mipNum = nri::REMAINING;
            b.layerOffset = 0;
            b.layerNum = nri::REMAINING;
            b.planes = nri::PlaneBits::ALL;
            barriers.push_back(b);
            m_ColorAttachments[i]->SetCurrentState(b.after.layout, b.after.access, b.after.stages);
        }

        if (m_DepthAttachment && m_DepthAttachment->GetHandle())
        {
            const auto &depthDesc = nri.GetTextureDesc(*m_DepthAttachment->GetHandle());
            const bool depthSupportsShaderResource = (depthDesc.usage & nri::TextureUsageBits::SHADER_RESOURCE) != 0;

            if (depthSupportsShaderResource)
            {
                nri::TextureBarrierDesc b = {};
                b.texture = m_DepthAttachment->GetHandle();
                b.before = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT, nri::StageBits::DEPTH_STENCIL_ATTACHMENT };
                b.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
                b.mipOffset = 0;
                b.mipNum = nri::REMAINING;
                b.layerOffset = 0;
                b.layerNum = nri::REMAINING;
                b.planes = HasStencilComponent(m_DepthAttachment->GetFormat())
                    ? (nri::PlaneBits::DEPTH | nri::PlaneBits::STENCIL)
                    : nri::PlaneBits::DEPTH;
                barriers.push_back(b);
                m_DepthAttachment->SetCurrentState(b.after.layout, b.after.access, b.after.stages);
            }
            else
            {
                m_DepthAttachment->SetCurrentState(nri::Layout::DEPTH_STENCIL_ATTACHMENT, nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::StageBits::DEPTH_STENCIL_ATTACHMENT);
            }
        }

        if (!barriers.empty())
        {
            nri::BarrierDesc barrierDesc = {};
            barrierDesc.textures = barriers.data();
            barrierDesc.textureNum = static_cast<uint32_t>(barriers.size());
            nri.CmdBarrier(*commandBuffer, barrierDesc);
        }

        m_AttachmentsInitialized = true;
        m_IsRendering = false;
    }

    void RenderTarget::ClearColorAndDepth(nri::CommandBuffer *commandBuffer)
    {
        if (!commandBuffer)
            return;

        const bool hasColorClear = !m_FloatClearColors.empty() || !m_UintClearColors.empty();
        const bool hasDepthClear = m_DepthAttachmentView != nullptr
            && m_ClearDepth.first != std::numeric_limits<float>::max()
            && m_ClearDepth.second != std::numeric_limits<uint32_t>::max();

        BeginRendering(commandBuffer, hasColorClear, hasDepthClear);
        EndRendering(commandBuffer);
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

    void RenderTarget::ClearColorAttachmentFloat(nri::CommandBuffer *commandBuffer, uint32_t attachmentIndex, const glm::vec4 &clearColor)
    {
        if (!commandBuffer)
            return;

        if (attachmentIndex >= m_ColorAttachmentViews.size() || !m_ColorAttachmentViews[attachmentIndex])
        {
            LOG_WARN("[Render Target] Color attachment index {} is out of range", attachmentIndex);
            return;
        }

        m_FloatClearColors[attachmentIndex] = clearColor;

        if (m_IsRendering)
        {
            const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
            nri::ClearAttachmentDesc clearDesc = {};
            clearDesc.value.color.f = { clearColor.x, clearColor.y, clearColor.z, clearColor.w };
            clearDesc.planes = nri::PlaneBits::COLOR;
            clearDesc.colorAttachmentIndex = static_cast<uint8_t>(attachmentIndex);

            nri::Rect rect = { 0, 0, static_cast<nri::Dim_t>(m_CreateInfo.width), static_cast<nri::Dim_t>(m_CreateInfo.height) };
            nri.CmdClearAttachments(*commandBuffer, &clearDesc, 1, &rect, 1);
            return;
        }

        auto *deviceManager = DeviceManager::GetInstance();
        const auto &nri = deviceManager->GetCoreInterface();

        nri::TextureBarrierDesc barrier = {};
        barrier.texture = m_ColorAttachments[attachmentIndex]->GetHandle();
        if (m_ColorAttachments[attachmentIndex]->GetCurrentLayout() != nri::Layout::UNDEFINED)
        {
            barrier.before = { m_ColorAttachments[attachmentIndex]->GetCurrentAccess(), m_ColorAttachments[attachmentIndex]->GetCurrentLayout(), m_ColorAttachments[attachmentIndex]->GetCurrentStages() };
        }
        else
        {
            barrier.before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
        }
        barrier.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT };
        barrier.mipOffset = 0;
        barrier.mipNum = nri::REMAINING;
        barrier.layerOffset = 0;
        barrier.layerNum = nri::REMAINING;
        barrier.planes = nri::PlaneBits::ALL;

        nri::BarrierDesc barrierDesc = {};
        barrierDesc.textures = &barrier;
        barrierDesc.textureNum = 1;
        nri.CmdBarrier(*commandBuffer, barrierDesc);

        nri::AttachmentDesc colorAtt = {};
        colorAtt.descriptor = m_ColorAttachmentViews[attachmentIndex];
        colorAtt.clearValue.color.f = { clearColor.x, clearColor.y, clearColor.z, clearColor.w };
        colorAtt.loadOp = nri::LoadOp::CLEAR;
        colorAtt.storeOp = nri::StoreOp::STORE;

        nri::RenderingDesc renderingDesc = {};
        renderingDesc.colors = &colorAtt;
        renderingDesc.colorNum = 1;
        nri.CmdBeginRendering(*commandBuffer, renderingDesc);
        nri.CmdEndRendering(*commandBuffer);

        barrier.before = barrier.after;
        if (m_ColorAttachments[attachmentIndex]->GetCreateInfo().initialLayout == nri::Layout::PRESENT)
        {
            barrier.after = { nri::AccessBits::NONE, nri::Layout::PRESENT, nri::StageBits::NONE };
        }
        else
        {
            const auto &colorDesc = nri.GetTextureDesc(*m_ColorAttachments[attachmentIndex]->GetHandle());
            if ((colorDesc.usage & nri::TextureUsageBits::SHADER_RESOURCE) != 0)
            {
                barrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
            }
            else
            {
                barrier.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT };
            }
        }
        nri.CmdBarrier(*commandBuffer, barrierDesc);
        m_ColorAttachments[attachmentIndex]->SetCurrentState(barrier.after.layout, barrier.after.access, barrier.after.stages);
        m_AttachmentsInitialized = true;
    }

    void RenderTarget::ClearColorAttachmentUint(nri::CommandBuffer *commandBuffer, uint32_t attachmentIndex, uint32_t clearColor)
    {
        if (!commandBuffer)
            return;

        if (attachmentIndex >= m_ColorAttachmentViews.size() || !m_ColorAttachmentViews[attachmentIndex])
        {
            LOG_WARN("[Render Target] Color attachment index {} is out of range", attachmentIndex);
            return;
        }

        const nri::Format format = m_ColorAttachments[attachmentIndex]->GetFormat();
        const bool isUint = format == nri::Format::R32_UINT || format == nri::Format::RGBA8_UINT || format == nri::Format::R8_UINT;
        LOG_ASSERT(isUint, "[Render Target] Color attachment is not UINT type!");

        m_UintClearColors[attachmentIndex] = clearColor;

        if (m_IsRendering)
        {
            const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
            nri::ClearAttachmentDesc clearDesc = {};
            clearDesc.value.color.ui = { clearColor, clearColor, clearColor, clearColor };
            clearDesc.planes = nri::PlaneBits::COLOR;
            clearDesc.colorAttachmentIndex = static_cast<uint8_t>(attachmentIndex);

            nri::Rect rect = { 0, 0, static_cast<nri::Dim_t>(m_CreateInfo.width), static_cast<nri::Dim_t>(m_CreateInfo.height) };
            nri.CmdClearAttachments(*commandBuffer, &clearDesc, 1, &rect, 1);
            return;
        }

        auto *deviceManager = DeviceManager::GetInstance();
        const auto &nri = deviceManager->GetCoreInterface();

        nri::TextureBarrierDesc barrier = {};
        barrier.texture = m_ColorAttachments[attachmentIndex]->GetHandle();
        if (m_ColorAttachments[attachmentIndex]->GetCurrentLayout() != nri::Layout::UNDEFINED)
        {
            barrier.before = { m_ColorAttachments[attachmentIndex]->GetCurrentAccess(), m_ColorAttachments[attachmentIndex]->GetCurrentLayout(), m_ColorAttachments[attachmentIndex]->GetCurrentStages() };
        }
        else
        {
            barrier.before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
        }
        barrier.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT };
        barrier.mipOffset = 0;
        barrier.mipNum = nri::REMAINING;
        barrier.layerOffset = 0;
        barrier.layerNum = nri::REMAINING;
        barrier.planes = nri::PlaneBits::ALL;

        nri::BarrierDesc barrierDesc = {};
        barrierDesc.textures = &barrier;
        barrierDesc.textureNum = 1;
        nri.CmdBarrier(*commandBuffer, barrierDesc);

        nri::AttachmentDesc colorAtt = {};
        colorAtt.descriptor = m_ColorAttachmentViews[attachmentIndex];
        colorAtt.clearValue.color.ui = { clearColor, clearColor, clearColor, clearColor };
        colorAtt.loadOp = nri::LoadOp::CLEAR;
        colorAtt.storeOp = nri::StoreOp::STORE;

        nri::RenderingDesc renderingDesc = {};
        renderingDesc.colors = &colorAtt;
        renderingDesc.colorNum = 1;
        nri.CmdBeginRendering(*commandBuffer, renderingDesc);
        nri.CmdEndRendering(*commandBuffer);

        barrier.before = barrier.after;
        if (m_ColorAttachments[attachmentIndex]->GetCreateInfo().initialLayout == nri::Layout::PRESENT)
        {
            barrier.after = { nri::AccessBits::NONE, nri::Layout::PRESENT, nri::StageBits::NONE };
        }
        else
        {
            const auto &colorDesc = nri.GetTextureDesc(*m_ColorAttachments[attachmentIndex]->GetHandle());
            if ((colorDesc.usage & nri::TextureUsageBits::SHADER_RESOURCE) != 0)
            {
                barrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
            }
            else
            {
                barrier.after = { nri::AccessBits::COLOR_ATTACHMENT, nri::Layout::COLOR_ATTACHMENT, nri::StageBits::COLOR_ATTACHMENT };
            }
        }
        nri.CmdBarrier(*commandBuffer, barrierDesc);
        m_ColorAttachments[attachmentIndex]->SetCurrentState(barrier.after.layout, barrier.after.access, barrier.after.stages);
        m_AttachmentsInitialized = true;
    }

    void RenderTarget::ClearDepthAttachment(nri::CommandBuffer *commandBuffer, float depth, uint32_t stencil)
    {
        if (!commandBuffer || !m_DepthAttachment || !m_DepthAttachmentView)
        {
            return;
        }

        m_ClearDepth = { depth, stencil };

        if (m_IsRendering)
        {
            const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();
            nri::ClearAttachmentDesc clearDesc = {};
            clearDesc.value.depthStencil.depth = depth;
            clearDesc.value.depthStencil.stencil = static_cast<uint8_t>(stencil);
            clearDesc.planes = HasStencilComponent(m_DepthAttachment->GetFormat())
                ? (nri::PlaneBits::DEPTH | nri::PlaneBits::STENCIL)
                : nri::PlaneBits::DEPTH;

            nri::Rect rect = { 0, 0, static_cast<nri::Dim_t>(m_CreateInfo.width), static_cast<nri::Dim_t>(m_CreateInfo.height) };
            nri.CmdClearAttachments(*commandBuffer, &clearDesc, 1, &rect, 1);
            return;
        }

        auto *deviceManager = DeviceManager::GetInstance();
        const auto &nri = deviceManager->GetCoreInterface();

        nri::TextureBarrierDesc barrier = {};
        barrier.texture = m_DepthAttachment->GetHandle();
        if (m_DepthAttachment->GetCurrentLayout() != nri::Layout::UNDEFINED)
        {
            barrier.before = { m_DepthAttachment->GetCurrentAccess(), m_DepthAttachment->GetCurrentLayout(), m_DepthAttachment->GetCurrentStages() };
        }
        else
        {
            barrier.before = { nri::AccessBits::NONE, nri::Layout::UNDEFINED, nri::StageBits::NONE };
        }
        barrier.after = { nri::AccessBits::DEPTH_STENCIL_ATTACHMENT_WRITE, nri::Layout::DEPTH_STENCIL_ATTACHMENT, nri::StageBits::DEPTH_STENCIL_ATTACHMENT };
        barrier.mipOffset = 0;
        barrier.mipNum = nri::REMAINING;
        barrier.layerOffset = 0;
        barrier.layerNum = nri::REMAINING;
        barrier.planes = HasStencilComponent(m_DepthAttachment->GetFormat())
            ? (nri::PlaneBits::DEPTH | nri::PlaneBits::STENCIL)
            : nri::PlaneBits::DEPTH;

        nri::BarrierDesc barrierDesc = {};
        barrierDesc.textures = &barrier;
        barrierDesc.textureNum = 1;
        nri.CmdBarrier(*commandBuffer, barrierDesc);

        nri::RenderingDesc renderingDesc = {};
        renderingDesc.depth.descriptor = m_DepthAttachmentView;
        renderingDesc.depth.clearValue.depthStencil.depth = depth;
        renderingDesc.depth.clearValue.depthStencil.stencil = static_cast<uint8_t>(stencil);
        renderingDesc.depth.loadOp = nri::LoadOp::CLEAR;
        renderingDesc.depth.storeOp = nri::StoreOp::STORE;

        nri.CmdBeginRendering(*commandBuffer, renderingDesc);
        nri.CmdEndRendering(*commandBuffer);

        barrier.before = barrier.after;
        nri.CmdBarrier(*commandBuffer, barrierDesc);
        m_DepthAttachment->SetCurrentState(barrier.after.layout, barrier.after.access, barrier.after.stages);
        m_AttachmentsInitialized = true;
    }

    Ref<RenderTarget> RenderTarget::Create(const RenderTargetCreateInfo &createInfo, const std::string &debugName)
    {
        return CreateRef<RenderTarget>(createInfo, debugName);
    }
}
