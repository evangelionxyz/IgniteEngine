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

#include "imgui_nvrhi.hpp"
#include "ignite/core/logger.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_set>

// ImGui color channel shifts for ImU32 RGBA packed format
#define IM_COL32_R_SHIFT    0
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    16
#define IM_COL32_A_SHIFT    24

#ifdef PLATFORM_WINDOWS
    #include "ignite/core/device/device_manager_dx12.hpp"
    #include <dxgi1_5.h>
#endif

#ifdef IGNITE_WITH_VULKAN
    #include "ignite/core/device/device_manager_vk.hpp"
#endif

namespace
{
#ifdef PLATFORM_WINDOWS
    struct ImGuiViewportRendererData_DX12
    {
        nvrhi::RefCountPtr<IDXGISwapChain3> swapChain;
        std::vector<nvrhi::RefCountPtr<ID3D12Resource>> backBuffers;
        std::vector<std::shared_ptr<ignite::RenderTarget>> renderTargets;
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        u32 pendingWidth = 0;
        u32 pendingHeight = 0;
        bool hasPendingResize = false;
    };
#endif

#ifdef IGNITE_WITH_VULKAN
    struct ImGuiViewportRendererData_VK
    {
        vk::SurfaceKHR surface;
        vk::SwapchainKHR swapChain;
        std::vector<vk::Image> swapChainImages;
        std::vector<std::shared_ptr<ignite::RenderTarget>> renderTargets;
        vk::SurfaceFormatKHR surfaceFormat{};
        vk::Extent2D extent{};
        vk::Fence acquireFence;
        vk::Semaphore presentSemaphore;
        u32 currentImageIndex = 0;
        u32 pendingWidth = 0;
        u32 pendingHeight = 0;
        bool hasPendingResize = false;
        bool hasAcquiredImage = false;
    };
#endif
}

namespace ignite
{
    struct ImGuiPushConstants
    {
        f32 invDisplaySize[2];
        f32 displayPos[2];
    };

    bool ImGui_NVRHI::UpdateFontTexture()
    {
        std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

        ImGuiIO &io = ImGui::GetIO();

        // If the font texture exists and is bound to ImGui, we're done.
        // Note: ImGui_Renderer will reset io.Fonts->TexID when new fonts are added.
        if (fontTexture && io.Fonts->TexID)
            return true;

        unsigned char *pixels;
        i32 width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!pixels)
            return false;

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = width;
        textureDesc.height = height;
        textureDesc.format = nvrhi::Format::RGBA8_UNORM;
        textureDesc.debugName = "ImGui font texture";

        fontTexture = m_Device->createTexture(textureDesc);
        LOG_ASSERT(fontTexture, "Failed to create imgui font texture");

        commandList->open();

        commandList->beginTrackingTextureState(fontTexture, nvrhi::AllSubresources, nvrhi::ResourceStates::Common);
        commandList->writeTexture(fontTexture, 0, 0, pixels, width * 4);
        commandList->setPermanentTextureState(fontTexture, nvrhi::ResourceStates::ShaderResource);
        commandList->commitBarriers();

        commandList->close();
        m_Device->executeCommandList(commandList);

        io.Fonts->TexID = (ImTextureID)fontTexture.Get();

        return true;
    }

    bool ImGui_NVRHI::Init(nvrhi::IDevice *device)
    {
        m_Device = device;
        commandList = device->createCommandList(
            nvrhi::CommandListParameters()
                .setEnableImmediateExecution(false)
                .setQueueType(nvrhi::CommandQueue::Graphics));
        m_IsShuttingDown = false;

        ImGuiIO &io = ImGui::GetIO();
        io.BackendRendererUserData = this;
        io.BackendRendererName = "imgui_nvrhi";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

#ifdef PLATFORM_WINDOWS
        if (Renderer::GetGraphicsAPI() == nvrhi::GraphicsAPI::D3D12)
        {
            io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

            ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();
            platformIO.Renderer_CreateWindow = RendererCreateWindow;
            platformIO.Renderer_DestroyWindow = RendererDestroyWindow;
            platformIO.Renderer_SetWindowSize = RendererSetWindowSize;
            platformIO.Renderer_RenderWindow = RendererRenderWindow;
            platformIO.Renderer_SwapBuffers = RendererSwapBuffers;
        }
#endif

#ifdef IGNITE_WITH_VULKAN
        if (Renderer::GetGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
        {
            io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

            ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();
            platformIO.Renderer_CreateWindow = RendererCreateWindowVK;
            platformIO.Renderer_DestroyWindow = RendererDestroyWindowVK;
            platformIO.Renderer_SetWindowSize = RendererSetWindowSizeVK;
            platformIO.Renderer_RenderWindow = RendererRenderWindowVK;
            platformIO.Renderer_SwapBuffers = RendererSwapBuffersVK;
        }
#endif

        const auto desc = nvrhi::SamplerDesc()
            .setAllAddressModes(nvrhi::SamplerAddressMode::Wrap)
            .setAllFilters(true);

        fontSampler = device->createSampler(desc);

        LOG_ASSERT(fontSampler, "Failed to create ImGui font sampler");
        if (!fontSampler)
            return false;

        return true;
    }

    bool ImGui_NVRHI::Render(nvrhi::IFramebuffer *framebuffer)
    {
        ImDrawData *drawData = ImGui::GetDrawData();
        return Render(drawData, framebuffer);
    }

    bool ImGui_NVRHI::Render(ImDrawData *drawData, nvrhi::IFramebuffer *framebuffer)
    {
        std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

        if (!drawData || drawData->CmdListsCount <= 0)
            return true;

        int fbWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
        int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
        if (fbWidth <= 0 || fbHeight <= 0)
            return true;

        commandList->open();
        commandList->beginMarker("ImGui");

        if (!UpdateGeometry(commandList, drawData))
        {
            commandList->close();
            return false;
        }

        ImGuiPushConstants pushConstants = {};
        pushConstants.invDisplaySize[0] = 1.0f / drawData->DisplaySize.x;
        pushConstants.invDisplaySize[1] = 1.0f / drawData->DisplaySize.y;
        pushConstants.displayPos[0] = drawData->DisplayPos.x;
        pushConstants.displayPos[1] = drawData->DisplayPos.y;
        ImVec2 clipOff = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        // setup graphics state
        nvrhi::GraphicsState drawState;
        drawState.framebuffer = framebuffer;
        LOG_ASSERT(drawState.framebuffer, "Invalid framebuffer");

        Ref<GraphicsPipeline> pipeline = GetPSO(drawState.framebuffer);
        drawState.pipeline = pipeline->GetHandle();

        drawState.viewport.viewports.push_back(
            nvrhi::Viewport(
                drawData->DisplaySize.x * drawData->FramebufferScale.x,
                drawData->DisplaySize.y * drawData->FramebufferScale.y
        ));

        drawState.viewport.scissorRects.resize(1);

        drawState.vertexBuffers = { { vertexBuffer, 0, 0 } };
        drawState.indexBuffer.buffer = indexBuffer;
        drawState.indexBuffer.format = sizeof(ImDrawIdx) == 2 ? nvrhi::Format::R16_UINT : nvrhi::Format::R32_UINT;
        drawState.indexBuffer.offset = 0;

        // render command list
        i32 vtxOffset = 0;
        i32 idxOffset = 0;
        for (i32 n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList *cmdList = drawData->CmdLists[n];

            for (i32 i = 0; i < cmdList->CmdBuffer.Size; ++i)
            {
                const ImDrawCmd *pCmd = &cmdList->CmdBuffer[i];

                if (pCmd->UserCallback)
                {
                    pCmd->UserCallback(cmdList, pCmd);
                }
                else
                {
                    drawState.bindings = { GetBindingSet((nvrhi::ITexture *)pCmd->TextureId, pipeline->GetBindingLayout(0)) };
                    LOG_ASSERT(drawState.bindings[0], "Invalid draw state binding");

                    ImVec2 clipMin((pCmd->ClipRect.x - clipOff.x) * clipScale.x, (pCmd->ClipRect.y - clipOff.y) * clipScale.y);
                    ImVec2 clipMax((pCmd->ClipRect.z - clipOff.x) * clipScale.x, (pCmd->ClipRect.w - clipOff.y) * clipScale.y);

                    if (clipMin.x < 0.0f) clipMin.x = 0.0f;
                    if (clipMin.y < 0.0f) clipMin.y = 0.0f;
                    if (clipMax.x > static_cast<float>(fbWidth)) clipMax.x = static_cast<float>(fbWidth);
                    if (clipMax.y > static_cast<float>(fbHeight)) clipMax.y = static_cast<float>(fbHeight);

                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;

                    drawState.viewport.scissorRects[0] = nvrhi::Rect(
                        int(clipMin.x),
                        int(clipMax.x),
                        int(clipMin.y),
                        int(clipMax.y)
                    );

                    nvrhi::DrawArguments drawArguments;
                    drawArguments.vertexCount = pCmd->ElemCount;
                    drawArguments.startVertexLocation = vtxOffset + pCmd->VtxOffset;
                    drawArguments.startIndexLocation = idxOffset + pCmd->IdxOffset;

                    commandList->setGraphicsState(drawState);
                    commandList->setPushConstants(&pushConstants, sizeof(pushConstants));
                    commandList->drawIndexed(drawArguments);
                }
            }
            idxOffset += cmdList->IdxBuffer.Size;
            vtxOffset += cmdList->VtxBuffer.Size;
        }

        commandList->endMarker();
        commandList->close();
        m_Device->executeCommandList(commandList);

        return true;
    }

    void ImGui_NVRHI::BackBufferResizing()
    {
        graphicsPipelines.clear();
        bindingsCache.clear();
    }

    bool ImGui_NVRHI::ReallocateBuffer(nvrhi::BufferHandle &buffer, size_t requiredSize, size_t reallocateSize, bool isIndexBuffer)
    {
        if (buffer == nullptr || size_t(buffer->getDesc().byteSize) < requiredSize)
        {
            nvrhi::BufferDesc desc;
            desc.byteSize = static_cast<u32>(reallocateSize);
            desc.debugName = isIndexBuffer ? "ImGui index buffer" : "ImGui vertex buffer";
            desc.canHaveUAVs = false;
            desc.isVertexBuffer = !isIndexBuffer;
            desc.isIndexBuffer = isIndexBuffer;
            desc.isDrawIndirectArgs = false;
            desc.isVolatile = false;
            desc.initialState = isIndexBuffer ? nvrhi::ResourceStates::IndexBuffer : nvrhi::ResourceStates::VertexBuffer;
            desc.keepInitialState = true;

            buffer = m_Device->createBuffer(desc);

            if (!buffer)
                return false;
        }

        return true;
    }

    Ref<GraphicsPipeline> ImGui_NVRHI::GetPSO(nvrhi::IFramebuffer *framebuffer)
    {
        auto pipelineIter = graphicsPipelines.find(framebuffer);
        if (pipelineIter != graphicsPipelines.end())
        {
            return pipelineIter->second;
        }

        if (!bindingLayout)
        {
            nvrhi::BindingLayoutDesc layoutDesc;
            layoutDesc.visibility = nvrhi::ShaderType::All;
            layoutDesc.bindings =
            {
                nvrhi::BindingLayoutItem::PushConstants(0, sizeof(float) * 4),
                nvrhi::BindingLayoutItem::Texture_SRV(0),
                nvrhi::BindingLayoutItem::Sampler(0)
            };

            bindingLayout = m_Device->createBindingLayout(layoutDesc);
        }

        auto vertexShader = Shader::Create("resources/shaders/imgui.vertex.hlsl", ShaderType::Vertex);
        auto pixelShader = Shader::Create("resources/shaders/imgui.pixel.hlsl", ShaderType::Pixel);

        GraphicsPipelineParams params;
        params.fillMode = nvrhi::RasterFillMode::Solid;
        params.cullMode = nvrhi::RasterCullMode::None;
        
        params.enableBlend = true; // Explicitly enable blending for ImGui
        params.srcBlend = nvrhi::BlendFactor::SrcAlpha;
        params.destBlend = nvrhi::BlendFactor::InvSrcAlpha;
        params.srcBlendAlpha = nvrhi::BlendFactor::InvSrcAlpha;
        params.destBlendAlpha = nvrhi::BlendFactor::Zero;

        params.enableDepthClip = true;
        params.enableDepthClip = true;

        params.enableDepthTest = false;
        params.enableDepthWrite = true;
        params.enableDepthStencil = false;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create();
        pipeline->SetShaders({ vertexShader, pixelShader })
            .AddBindingLayout(bindingLayout)
            .Build(framebuffer, params);

        graphicsPipelines[framebuffer] = pipeline;
        return pipeline;
    }

    nvrhi::IBindingSet *ImGui_NVRHI::GetBindingSet(nvrhi::ITexture *texture, nvrhi::BindingLayoutHandle bindingLayout)
    {
        auto iter = bindingsCache.find(texture);
        if (iter != bindingsCache.end())
            return iter->second;

        nvrhi::BindingSetDesc desc;
        desc.bindings =
        {
            nvrhi::BindingSetItem::PushConstants(0, sizeof(float) * 4),
            nvrhi::BindingSetItem::Texture_SRV(0, texture),
            nvrhi::BindingSetItem::Sampler(0, fontSampler)
        };

        nvrhi::BindingSetHandle binding;
        binding = m_Device->createBindingSet(desc, bindingLayout);
        LOG_ASSERT(binding, "Failed to create ImGui binding set");

        bindingsCache[texture] = binding;
        return binding;
    }

    bool ImGui_NVRHI::UpdateGeometry(nvrhi::ICommandList *commandList, ImDrawData *drawData)
    {
        // Calculate size needed for expanded vertices
        size_t expandedVertexSize = drawData->TotalVtxCount * sizeof(ImGuiVertexData);
        
        if (!ReallocateBuffer(vertexBuffer, expandedVertexSize,
            (drawData->TotalVtxCount + 5000) * sizeof(ImGuiVertexData),
            false))
        {
            return false;
        }

        if (!ReallocateBuffer(indexBuffer, drawData->TotalIdxCount * sizeof(ImDrawIdx),
            (drawData->TotalIdxCount + 5000) * sizeof(ImDrawIdx),
            true))
        {
            return false;
        }

        // Resize buffers to match expanded vertex format
        imguiVertexBuffer.resize(vertexBuffer->getDesc().byteSize / sizeof(ImGuiVertexData));
        imguiIndexBuffer.resize(indexBuffer->getDesc().byteSize / sizeof(ImDrawIdx));

        ImGuiVertexData *vtxDst = imguiVertexBuffer.data();
        ImDrawIdx *idxDst = imguiIndexBuffer.data();

        for (i32 n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList *cmdList = drawData->CmdLists[n];
            
            // Convert ImDrawVert to ImGuiVertexData (expand ImU32 color to float4)
            for (i32 i = 0; i < cmdList->VtxBuffer.Size; ++i)
            {
                const ImDrawVert& src = cmdList->VtxBuffer[i];
                ImGuiVertexData& dst = vtxDst[i];
                
                dst.position = glm::vec2(src.pos.x, src.pos.y);
                dst.texCoord = glm::vec2(src.uv.x, src.uv.y);
                
                // Convert packed RGBA ImU32 to float4
                dst.color.r = ((src.col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                dst.color.g = ((src.col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                dst.color.b = ((src.col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                dst.color.a = ((src.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
            }
            
            std::memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }

        commandList->writeBuffer(vertexBuffer, imguiVertexBuffer.data(), vertexBuffer->getDesc().byteSize);
        commandList->writeBuffer(indexBuffer, imguiIndexBuffer.data(), indexBuffer->getDesc().byteSize);

        return true;
    }

    void ImGui_NVRHI::Shutdown()
    {
        m_IsShuttingDown = true;

#ifdef PLATFORM_WINDOWS
        if (Renderer::GetGraphicsAPI() == nvrhi::GraphicsAPI::D3D12)
        {
            ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();

            ImGui::DestroyPlatformWindows();

            platformIO.Renderer_CreateWindow = nullptr;
            platformIO.Renderer_DestroyWindow = nullptr;
            platformIO.Renderer_SetWindowSize = nullptr;
            platformIO.Renderer_RenderWindow = nullptr;
            platformIO.Renderer_SwapBuffers = nullptr;
        }
#endif

#ifdef IGNITE_WITH_VULKAN
        if (Renderer::GetGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
        {
            ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();

            ImGui::DestroyPlatformWindows();

            platformIO.Renderer_CreateWindow = nullptr;
            platformIO.Renderer_DestroyWindow = nullptr;
            platformIO.Renderer_SetWindowSize = nullptr;
            platformIO.Renderer_RenderWindow = nullptr;
            platformIO.Renderer_SwapBuffers = nullptr;
        }
#endif

        ImGuiIO &io = ImGui::GetIO();
        io.BackendRendererUserData = nullptr;
        io.BackendRendererName = nullptr;

        bindingsCache.clear();
        graphicsPipelines.clear();
        bindingLayout = nullptr;

        fontTexture = nullptr;
        fontSampler = nullptr;

        vertexBuffer = nullptr;
        indexBuffer = nullptr;

        commandList = nullptr;
    }

#ifdef IGNITE_WITH_VULKAN
    static ImGuiViewportRendererData_VK *GetViewportDataVK(ImGuiViewport *viewport)
    {
        return static_cast<ImGuiViewportRendererData_VK *>(viewport->RendererUserData);
    }

    static nvrhi::Format GetNvrhiFormatFromVkFormat(vk::Format format)
    {
        switch (format)
        {
            case vk::Format::eR8G8B8A8Unorm: return nvrhi::Format::RGBA8_UNORM;
            case vk::Format::eR8G8B8A8Srgb: return nvrhi::Format::SRGBA8_UNORM;
            case vk::Format::eB8G8R8A8Unorm: return nvrhi::Format::BGRA8_UNORM;
            case vk::Format::eB8G8R8A8Srgb: return nvrhi::Format::SBGRA8_UNORM;
            default: return nvrhi::Format::UNKNOWN;
        }
    }

    static vk::PresentModeKHR SelectViewportPresentMode(const std::vector<vk::PresentModeKHR> &presentModes, bool vsyncEnabled)
    {
        if (vsyncEnabled)
            return vk::PresentModeKHR::eFifo;

        if (std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eMailbox) != presentModes.end())
            return vk::PresentModeKHR::eMailbox;

        if (std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eImmediate) != presentModes.end())
            return vk::PresentModeKHR::eImmediate;

        return vk::PresentModeKHR::eFifo;
    }

    static void RemoveViewportPipelines(ImGui_NVRHI *renderer, ImGuiViewportRendererData_VK *viewportData)
    {
        if (!renderer || !viewportData)
            return;

        for (const auto &rt : viewportData->renderTargets)
        {
            if (!rt)
                continue;

            nvrhi::IFramebuffer *fb = rt->GetFramebuffer().Get();
            auto pipelineIter = renderer->graphicsPipelines.find(fb);
            if (pipelineIter != renderer->graphicsPipelines.end())
                renderer->graphicsPipelines.erase(pipelineIter);
        }
    }

    static bool CreateViewportSwapChainAndRenderTargetsVK(ImGuiViewportRendererData_VK *viewportData, DeviceManager_VK &deviceManager, u32 width, u32 height)
    {
        if (!viewportData || !viewportData->surface || !deviceManager.m_VulkanDevice)
            return false;

        const vk::SurfaceCapabilitiesKHR capabilities = deviceManager.m_VulkanPhysicalDevice.getSurfaceCapabilitiesKHR(viewportData->surface);
        const std::vector<vk::SurfaceFormatKHR> surfaceFormats = deviceManager.m_VulkanPhysicalDevice.getSurfaceFormatsKHR(viewportData->surface);
        const std::vector<vk::PresentModeKHR> presentModes = deviceManager.m_VulkanPhysicalDevice.getSurfacePresentModesKHR(viewportData->surface);

        if (surfaceFormats.empty() || presentModes.empty())
            return false;

        vk::Extent2D extent{};
        if (capabilities.currentExtent.width != std::numeric_limits<u32>::max())
        {
            extent = capabilities.currentExtent;
        }
        else
        {
            extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        viewportData->surfaceFormat = surfaceFormats.front();
        const vk::Format preferredFormat = vk::Format(nvrhi::vulkan::convertFormat(deviceManager.GetDeviceParameters().swapChainFormat));
        for (const vk::SurfaceFormatKHR &surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == preferredFormat)
            {
                viewportData->surfaceFormat = surfaceFormat;
                break;
            }
        }

        const vk::PresentModeKHR presentMode = SelectViewportPresentMode(presentModes, deviceManager.GetDeviceParameters().vsyncEnable);

        u32 minImageCount = std::max(capabilities.minImageCount, deviceManager.GetDeviceParameters().swapChainBufferCount);
        if (capabilities.maxImageCount > 0)
            minImageCount = std::min(minImageCount, capabilities.maxImageCount);

        std::unordered_set<u32> uniqueQueues =
        {
            static_cast<u32>(deviceManager.m_GraphicsQueueFamily),
            static_cast<u32>(deviceManager.m_PresentQueueFamily)
        };
        std::vector<u32> queueFamilies(uniqueQueues.begin(), uniqueQueues.end());
        const bool sharedSwapChain = queueFamilies.size() > 1;

        if (viewportData->swapChain)
        {
            viewportData->renderTargets.clear();
            viewportData->swapChainImages.clear();
            deviceManager.m_VulkanDevice.destroySwapchainKHR(viewportData->swapChain);
            viewportData->swapChain = nullptr;
        }

        const vk::SwapchainCreateInfoKHR swapChainDesc = vk::SwapchainCreateInfoKHR()
            .setSurface(viewportData->surface)
            .setMinImageCount(minImageCount)
            .setImageFormat(viewportData->surfaceFormat.format)
            .setImageColorSpace(viewportData->surfaceFormat.colorSpace)
            .setImageExtent(extent)
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
            .setImageSharingMode(sharedSwapChain ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
            .setQueueFamilyIndexCount(sharedSwapChain ? static_cast<u32>(queueFamilies.size()) : 0)
            .setPQueueFamilyIndices(sharedSwapChain ? queueFamilies.data() : nullptr)
            .setPreTransform(capabilities.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(presentMode)
            .setClipped(VK_TRUE)
            .setOldSwapchain(VK_NULL_HANDLE);

        const vk::Result createResult = deviceManager.m_VulkanDevice.createSwapchainKHR(&swapChainDesc, nullptr, &viewportData->swapChain);
        if (createResult != vk::Result::eSuccess)
            return false;

        viewportData->extent = extent;
        viewportData->swapChainImages = deviceManager.m_VulkanDevice.getSwapchainImagesKHR(viewportData->swapChain);
        viewportData->renderTargets.reserve(viewportData->swapChainImages.size());

        nvrhi::Format colorFormat = GetNvrhiFormatFromVkFormat(viewportData->surfaceFormat.format);
        if (colorFormat == nvrhi::Format::UNKNOWN)
            colorFormat = deviceManager.GetDeviceParameters().swapChainFormat;

        for (const vk::Image &image : viewportData->swapChainImages)
        {
            RenderTargetCreateInfo createInfo;
            createInfo.width = extent.width;
            createInfo.height = extent.height;
            createInfo.attachments =
            {
                FramebufferAttachments {
                    .name = "ImGui viewport",
                    .format = colorFormat,
                    .state = nvrhi::ResourceStates::Present,
                    .nativeObjectPtr = reinterpret_cast<void *>(static_cast<VkImage>(image)),
                    .isNativeObject = true,
                    .nativeObjectType = nvrhi::ObjectTypes::VK_Image
                }
            };

            viewportData->renderTargets.emplace_back(RenderTarget::Create(createInfo, "ImGui viewport"));
        }

        viewportData->currentImageIndex = 0;
        viewportData->hasAcquiredImage = false;
        if (viewportData->renderTargets.empty())
        {
            deviceManager.m_VulkanDevice.destroySwapchainKHR(viewportData->swapChain);
            viewportData->swapChain = nullptr;
            return false;
        }

        return true;
    }

    void ImGui_NVRHI::RendererCreateWindowVK(ImGuiViewport *viewport)
    {
        if (Renderer::GetGraphicsAPI() != nvrhi::GraphicsAPI::VULKAN)
            return;

        DeviceManager_VK *deviceManager = DeviceManager_VK::GetInstance();
        if (!deviceManager || !deviceManager->m_VulkanDevice || !deviceManager->m_VulkanInstance)
            return;

        ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();
        if (!platformIO.Platform_CreateVkSurface)
            return;

        auto *viewportData = new ImGuiViewportRendererData_VK();

        ImU64 surface = 0;
        const int surfaceResult = platformIO.Platform_CreateVkSurface(
            viewport,
            static_cast<ImU64>(reinterpret_cast<uintptr_t>(static_cast<VkInstance>(deviceManager->m_VulkanInstance))),
            nullptr,
            &surface);

        if (surfaceResult != 0 || surface == 0)
        {
            delete viewportData;
            return;
        }

        viewportData->surface = vk::SurfaceKHR(reinterpret_cast<VkSurfaceKHR>(static_cast<uintptr_t>(surface)));

        viewportData->acquireFence = deviceManager->m_VulkanDevice.createFence(vk::FenceCreateInfo());
        viewportData->presentSemaphore = deviceManager->m_VulkanDevice.createSemaphore(vk::SemaphoreCreateInfo());

        if (!viewportData->acquireFence || !viewportData->presentSemaphore)
        {
            if (viewportData->acquireFence)
                deviceManager->m_VulkanDevice.destroyFence(viewportData->acquireFence);
            if (viewportData->presentSemaphore)
                deviceManager->m_VulkanDevice.destroySemaphore(viewportData->presentSemaphore);
            deviceManager->m_VulkanInstance.destroySurfaceKHR(viewportData->surface);
            delete viewportData;
            return;
        }

        if (!CreateViewportSwapChainAndRenderTargetsVK(
            viewportData,
            *deviceManager,
            static_cast<u32>(std::max(viewport->Size.x, 1.0f)),
            static_cast<u32>(std::max(viewport->Size.y, 1.0f))))
        {
            deviceManager->m_VulkanDevice.destroyFence(viewportData->acquireFence);
            deviceManager->m_VulkanDevice.destroySemaphore(viewportData->presentSemaphore);
            deviceManager->m_VulkanInstance.destroySurfaceKHR(viewportData->surface);
            delete viewportData;
            return;
        }

        viewport->RendererUserData = viewportData;
    }

    void ImGui_NVRHI::RendererDestroyWindowVK(ImGuiViewport *viewport)
    {
        auto *viewportData = GetViewportDataVK(viewport);
        if (!viewportData)
            return;

        auto *renderer = static_cast<ImGui_NVRHI *>(ImGui::GetIO().BackendRendererUserData);
        DeviceManager_VK *deviceManager = DeviceManager_VK::GetInstance();

        if (deviceManager && deviceManager->m_VulkanDevice)
        {
            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

            if (renderer)
                RemoveViewportPipelines(renderer, viewportData);

            viewportData->renderTargets.clear();
            viewportData->swapChainImages.clear();

            if (viewportData->swapChain)
            {
                deviceManager->m_VulkanDevice.destroySwapchainKHR(viewportData->swapChain);
                viewportData->swapChain = nullptr;
            }

            if (viewportData->acquireFence)
            {
                deviceManager->m_VulkanDevice.destroyFence(viewportData->acquireFence);
                viewportData->acquireFence = nullptr;
            }

            if (viewportData->presentSemaphore)
            {
                deviceManager->m_VulkanDevice.destroySemaphore(viewportData->presentSemaphore);
                viewportData->presentSemaphore = nullptr;
            }

            if (viewportData->surface)
            {
                if (deviceManager->m_VulkanInstance)
                    deviceManager->m_VulkanInstance.destroySurfaceKHR(viewportData->surface);
                viewportData->surface = VK_NULL_HANDLE;
            }
        }
        else if (deviceManager && viewportData->surface && deviceManager->m_VulkanInstance)
        {
            deviceManager->m_VulkanInstance.destroySurfaceKHR(viewportData->surface);
            viewportData->surface = VK_NULL_HANDLE;
        }

        delete viewportData;
        viewport->RendererUserData = nullptr;
    }

    void ImGui_NVRHI::RendererSetWindowSizeVK(ImGuiViewport *viewport, ImVec2 size)
    {
        auto *viewportData = GetViewportDataVK(viewport);
        auto *renderer = static_cast<ImGui_NVRHI *>(ImGui::GetIO().BackendRendererUserData);
        if (!viewportData || !viewportData->swapChain || !renderer || !renderer->m_Device)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (size.x <= 0.0f || size.y <= 0.0f)
            return;

        const u32 requestedWidth = static_cast<u32>(size.x);
        const u32 requestedHeight = static_cast<u32>(size.y);

        if (viewportData->hasPendingResize
            && viewportData->pendingWidth == requestedWidth
            && viewportData->pendingHeight == requestedHeight)
        {
            return;
        }

        if (!viewportData->hasPendingResize
            && viewportData->extent.width == requestedWidth
            && viewportData->extent.height == requestedHeight)
        {
            return;
        }

        viewportData->pendingWidth = requestedWidth;
        viewportData->pendingHeight = requestedHeight;
        viewportData->hasPendingResize = true;
    }

    void ImGui_NVRHI::RendererRenderWindowVK(ImGuiViewport *viewport, void *)
    {
        auto *renderer = static_cast<ImGui_NVRHI *>(ImGui::GetIO().BackendRendererUserData);
        auto *viewportData = GetViewportDataVK(viewport);
        DeviceManager_VK *deviceManager = DeviceManager_VK::GetInstance();
        if (!renderer || !viewportData || !viewportData->swapChain || !deviceManager)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (viewportData->hasPendingResize)
        {
            const u32 width = viewportData->pendingWidth;
            const u32 height = viewportData->pendingHeight;

            if (width > 0 && height > 0)
            {
                std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

                RemoveViewportPipelines(renderer, viewportData);

                if (!CreateViewportSwapChainAndRenderTargetsVK(viewportData, *deviceManager, width, height))
                {
                    LOG_WARN("Failed to recreate ImGui Vulkan viewport swap chain (size={}x{})", width, height);
                    return;
                }

                viewportData->hasPendingResize = false;
            }
        }

        deviceManager->m_VulkanDevice.resetFences(1, &viewportData->acquireFence);

        const vk::Result acquireResult = deviceManager->m_VulkanDevice.acquireNextImageKHR(
            viewportData->swapChain,
            0,
            vk::Semaphore(),
            viewportData->acquireFence,
            &viewportData->currentImageIndex);

        viewportData->hasAcquiredImage = false;

        if (acquireResult == vk::Result::eErrorOutOfDateKHR)
        {
            viewportData->hasPendingResize = true;
            return;
        }

        if (acquireResult == vk::Result::eTimeout || acquireResult == vk::Result::eNotReady)
            return;

        if (!(acquireResult == vk::Result::eSuccess || acquireResult == vk::Result::eSuboptimalKHR))
            return;

        deviceManager->m_VulkanDevice.waitForFences(1, &viewportData->acquireFence, VK_TRUE, std::numeric_limits<uint64_t>::max());

        viewportData->hasAcquiredImage = true;

        if (viewportData->currentImageIndex >= viewportData->renderTargets.size())
            return;

        renderer->Render(viewport->DrawData, viewportData->renderTargets[viewportData->currentImageIndex]->GetFramebuffer().Get());
    }

    void ImGui_NVRHI::RendererSwapBuffersVK(ImGuiViewport *viewport, void *)
    {
        auto *renderer = static_cast<ImGui_NVRHI *>(ImGui::GetIO().BackendRendererUserData);
        auto *viewportData = GetViewportDataVK(viewport);
        DeviceManager_VK *deviceManager = DeviceManager_VK::GetInstance();
        if (!renderer || !viewportData || !viewportData->swapChain || !deviceManager)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (!viewportData->hasAcquiredImage)
            return;

        deviceManager->m_NvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, viewportData->presentSemaphore, 0);
        deviceManager->m_NvrhiDevice->executeCommandLists(nullptr, 0);

        const vk::PresentInfoKHR presentInfo = vk::PresentInfoKHR()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&viewportData->presentSemaphore)
            .setSwapchainCount(1)
            .setPSwapchains(&viewportData->swapChain)
            .setPImageIndices(&viewportData->currentImageIndex);

        const vk::Result presentResult = deviceManager->m_PresentQueue.presentKHR(&presentInfo);
        if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR)
        {
            viewportData->hasPendingResize = true;
            viewportData->hasAcquiredImage = false;
            return;
        }

        if (presentResult != vk::Result::eSuccess)
        {
            LOG_WARN("Failed to present ImGui Vulkan viewport swap chain, result={}", static_cast<int>(presentResult));
        }

        viewportData->hasAcquiredImage = false;
    }
#endif

#ifdef PLATFORM_WINDOWS
    static ImGuiViewportRendererData_DX12 *GetViewportData(ImGuiViewport *viewport)
    {
        return static_cast<ImGuiViewportRendererData_DX12 *>(viewport->RendererUserData);
    }

    static bool CreateViewportRenderTargets(ImGuiViewportRendererData_DX12 *viewportData, u32 width, u32 height)
    {
        if (!viewportData || !viewportData->swapChain)
            return false;

        viewportData->backBuffers.resize(viewportData->swapChainDesc.BufferCount);
        viewportData->renderTargets.clear();
        viewportData->renderTargets.reserve(viewportData->swapChainDesc.BufferCount);

        DeviceManager_DX12 &deviceManager = DeviceManager_DX12::GetInstance();
        for (u32 i = 0; i < viewportData->swapChainDesc.BufferCount; ++i)
        {
            HRESULT hr = viewportData->swapChain->GetBuffer(i, IID_PPV_ARGS(&viewportData->backBuffers[i]));
            if (FAILED(hr))
                return false;

            RenderTargetCreateInfo createInfo;
            createInfo.width = width;
            createInfo.height = height;
            createInfo.attachments =
            {
                FramebufferAttachments {
                    .name = "ImGui viewport",
                    .format = deviceManager.GetDeviceParameters().swapChainFormat,
                    .state = nvrhi::ResourceStates::Present,
                    .nativeObjectPtr = viewportData->backBuffers[i],
                    .isNativeObject = true,
                    .nativeObjectType = nvrhi::ObjectTypes::D3D12_Resource
                }
            };

            viewportData->renderTargets.emplace_back(RenderTarget::Create(createInfo, "ImGui viewport"));
        }

        return true;
    }

    void ImGui_NVRHI::RendererCreateWindow(ImGuiViewport *viewport)
    {
        if (Renderer::GetGraphicsAPI() != nvrhi::GraphicsAPI::D3D12)
            return;

        DeviceManager_DX12 &deviceManager = DeviceManager_DX12::GetInstance();
        HWND hwnd = reinterpret_cast<HWND>(viewport->PlatformHandleRaw);
        if (!hwnd)
            return;

        auto *viewportData = new ImGuiViewportRendererData_DX12();
        viewportData->swapChainDesc = deviceManager.m_SwapChainDesc;
        viewportData->swapChainDesc.Width = static_cast<u32>(std::max(viewport->Size.x, 1.0f));
        viewportData->swapChainDesc.Height = static_cast<u32>(std::max(viewport->Size.y, 1.0f));

        nvrhi::RefCountPtr<IDXGISwapChain1> swapChain1;
        HRESULT hr = deviceManager.m_DxgiFactory2->CreateSwapChainForHwnd(
            deviceManager.m_GraphicsQueue,
            hwnd,
            &viewportData->swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1);

        LOG_ASSERT(SUCCEEDED(hr), "Failed to create ImGui viewport swap chain");
        if (FAILED(hr))
        {
            delete viewportData;
            return;
        }

        hr = swapChain1->QueryInterface(IID_PPV_ARGS(&viewportData->swapChain));
        LOG_ASSERT(SUCCEEDED(hr), "Failed to query IDXGISwapChain3 for ImGui viewport");
        if (FAILED(hr))
        {
            delete viewportData;
            return;
        }

        if (!CreateViewportRenderTargets(
            viewportData,
            static_cast<u32>(std::max(viewport->Size.x, 1.0f)),
            static_cast<u32>(std::max(viewport->Size.y, 1.0f))))
        {
            delete viewportData;
            return;
        }

        viewport->RendererUserData = viewportData;
    }

    void ImGui_NVRHI::RendererDestroyWindow(ImGuiViewport *viewport)
    {
        auto *viewportData = GetViewportData(viewport);
        if (!viewportData)
            return;

        viewportData->renderTargets.clear();
        viewportData->backBuffers.clear();
        viewportData->swapChain = nullptr;

        delete viewportData;
        viewport->RendererUserData = nullptr;
    }

    void ImGui_NVRHI::RendererSetWindowSize(ImGuiViewport *viewport, ImVec2 size)
    {
        auto *viewportData = GetViewportData(viewport);
        auto *renderer = static_cast<ImGui_NVRHI *>(ImGui::GetIO().BackendRendererUserData);
        if (!viewportData || !viewportData->swapChain || !renderer || !renderer->m_Device)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (size.x <= 0.0f || size.y <= 0.0f)
            return;

        const u32 requestedWidth = static_cast<u32>(size.x);
        const u32 requestedHeight = static_cast<u32>(size.y);

        if (viewportData->hasPendingResize
            && viewportData->pendingWidth == requestedWidth
            && viewportData->pendingHeight == requestedHeight)
        {
            return;
        }

        if (!viewportData->hasPendingResize
            && viewportData->swapChainDesc.Width == requestedWidth
            && viewportData->swapChainDesc.Height == requestedHeight)
        {
            return;
        }

        viewportData->pendingWidth = requestedWidth;
        viewportData->pendingHeight = requestedHeight;
        viewportData->hasPendingResize = true;
    }

    void ImGui_NVRHI::RendererRenderWindow(ImGuiViewport *viewport, void *)
    {
        auto *renderer = static_cast<ImGui_NVRHI *>(ImGui::GetIO().BackendRendererUserData);
        auto *viewportData = GetViewportData(viewport);
        if (!renderer || !viewportData || !viewportData->swapChain)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (viewportData->hasPendingResize)
        {
            const u32 width = viewportData->pendingWidth;
            const u32 height = viewportData->pendingHeight;

            if (width > 0 && height > 0)
            {
                HRESULT hr = S_OK;
                {
                    std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

                    for (const auto &rt : viewportData->renderTargets)
                    {
                        if (!rt)
                            continue;

                        nvrhi::IFramebuffer *fb = rt->GetFramebuffer().Get();
                        auto pipelineIter = renderer->graphicsPipelines.find(fb);
                        if (pipelineIter != renderer->graphicsPipelines.end())
                        {
                            renderer->graphicsPipelines.erase(pipelineIter);
                        }
                    }

                    viewportData->renderTargets.clear();
                    viewportData->backBuffers.clear();

                    hr = viewportData->swapChain->ResizeBuffers(
                        0,
                        width,
                        height,
                        DXGI_FORMAT_UNKNOWN,
                        viewportData->swapChainDesc.Flags);

                    if (hr == DXGI_ERROR_INVALID_CALL)
                    {
                        renderer->m_Device->waitForIdle();
                        renderer->m_Device->runGarbageCollection();

                        hr = viewportData->swapChain->ResizeBuffers(
                            0,
                            width,
                            height,
                            DXGI_FORMAT_UNKNOWN,
                            viewportData->swapChainDesc.Flags);
                    }
                }

                if (SUCCEEDED(hr))
                {
                    viewportData->swapChain->GetDesc1(&viewportData->swapChainDesc);

                    if (!CreateViewportRenderTargets(viewportData, width, height))
                    {
                        LOG_WARN("Failed to recreate ImGui viewport render targets after resize");
                    }

                    viewportData->hasPendingResize = false;
                }
                else if (hr == DXGI_ERROR_INVALID_CALL || hr == DXGI_ERROR_WAS_STILL_DRAWING)
                {
                    return;
                }
                else
                {
                    LOG_WARN("Failed to resize ImGui viewport swap chain (HRESULT=0x{:08X}, size={}x{})", static_cast<u32>(hr), width, height);
                    viewportData->hasPendingResize = false;
                    return;
                }
            }
        }

        const u32 backBufferIndex = viewportData->swapChain->GetCurrentBackBufferIndex();
        if (backBufferIndex >= viewportData->renderTargets.size())
            return;

        renderer->Render(viewport->DrawData, viewportData->renderTargets[backBufferIndex]->GetFramebuffer().Get());
    }

    void ImGui_NVRHI::RendererSwapBuffers(ImGuiViewport *viewport, void *)
    {
        auto *renderer = static_cast<ImGui_NVRHI *>(ImGui::GetIO().BackendRendererUserData);
        auto *viewportData = GetViewportData(viewport);
        if (!renderer || !viewportData || !viewportData->swapChain)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

        // Keep platform viewport presents non-blocking to avoid stalling the main loop
        // (which also drives FMOD updates).
        const HRESULT hr = viewportData->swapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);

        if (hr == DXGI_ERROR_WAS_STILL_DRAWING || hr == DXGI_STATUS_OCCLUDED)
            return;

        if (hr == DXGI_ERROR_INVALID_CALL || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            LOG_WARN("ImGui viewport present skipped during teardown/reset (HRESULT=0x{:08X})", static_cast<u32>(hr));
            return;
        }

        LOG_ASSERT(SUCCEEDED(hr), "Failed to present ImGui viewport swap chain");
    }
#endif
}
