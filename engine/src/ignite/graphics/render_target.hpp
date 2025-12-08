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

#ifndef RENDER_TARGET_HPP
#define RENDER_TARGET_HPP

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
        RenderTarget(const RenderTargetCreateInfo &createInfo, const std::string& debugName = "[RenderTarget]");

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

        static Ref<RenderTarget> Create(const RenderTargetCreateInfo &createInfo, const std::string& debugName = "[RenderTarget]");

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
