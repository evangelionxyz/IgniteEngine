// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "imgui_nri.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/graphics_pipeline.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/bindless_system.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_set>

#ifdef PLATFORM_WINDOWS
#include "ignite/core/device/device_manager_dx12.hpp"
#include <dxgi1_5.h>
#include <wrl/client.h>
#endif

#ifdef IGNITE_WITH_VULKAN
#include "ignite/core/device/device_manager_vk.hpp"
#endif

namespace
{
#ifdef PLATFORM_WINDOWS
    struct ImGuiViewportRendererData_DX12
    {
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> backBuffers;
        std::vector<nri::Texture *> nriTextures;
        std::vector<std::shared_ptr<ignite::RenderTarget>> renderTargets;
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        uint32_t pendingWidth = 0;
        uint32_t pendingHeight = 0;
        bool hasPendingResize = false;
    };
#endif

#ifdef IGNITE_WITH_VULKAN
    struct ImGuiViewportRendererData_VK
    {
        vk::SurfaceKHR surface;
        vk::SwapchainKHR swapChain;
        std::vector<vk::Image> swapChainImages;
        std::vector<nri::Texture *> nriTextures;
        std::vector<std::shared_ptr<ignite::RenderTarget>> renderTargets;
        vk::SurfaceFormatKHR surfaceFormat{};
        vk::Extent2D extent{};
        vk::Fence acquireFence;
        vk::Semaphore presentSemaphore;
        uint32_t currentImageIndex = 0;
        uint32_t pendingWidth = 0;
        uint32_t pendingHeight = 0;
        bool hasPendingResize = false;
        bool hasAcquiredImage = false;
    };
#endif
}

namespace ignite
{
    ImGui_NRI *ImGui_NRI::s_Instance = nullptr;

    ImGui_NRI::~ImGui_NRI()
    {
    }

    bool ImGui_NRI::UpdateFontTexture()
    {
        IGN_PROFILE_FUNCTION();

        ImGuiIO &io = ImGui::GetIO();
        if (m_FontTexture && io.Fonts->TexRef.GetTexID())
            return true;

        uint8_t *pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (!pixels || width <= 0 || height <= 0)
            return false;

        TextureCreateInfo createInfo = {};
        createInfo.width = static_cast<uint32_t>(width);
        createInfo.height = static_cast<uint32_t>(height);
        createInfo.format = nri::Format::RGBA8_UNORM;
        createInfo.initialAccess = nri::AccessBits::SHADER_RESOURCE;
        createInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfo.initialStages = nri::StageBits::ALL;

        std::vector<uint8_t> data(pixels, pixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        m_FontTexture = Texture::Create(data, createInfo, nullptr, "ImGui font texture");
        LOG_ASSERT(m_FontTexture && m_FontTexture->GetHandle(), "[ImGui NRI] Failed to create font texture");

        io.Fonts->TexID = (ImTextureID)m_FontTexture->GetShaderResourceView();
        return true;
    }

    bool ImGui_NRI::Init(nri::Device *device)
    {
        m_Device = device;
        s_Instance = this;
        m_IsShuttingDown = false;

        auto *deviceManager = DeviceManager::GetInstance();
        LOG_ASSERT(deviceManager && m_Device, "[ImGui NRI] Device manager or device is null");
        const auto &nri = deviceManager->GetCoreInterface();
        nri::Queue *graphicsQueue = deviceManager->GetGraphicsQueue();

        nri::FenceDesc fenceDesc = {};
        fenceDesc.initialValue = 0;
        nri.CreateFence(*m_Device, fenceDesc, m_CommandFence);

        for (auto &slot : m_CommandSlots)
        {
            nri.CreateCommandAllocator(*graphicsQueue, slot.allocator);
            nri.CreateCommandBuffer(*slot.allocator, slot.commandBuffer);
            slot.fenceValue = 0;
        }

        ImGuiIO &io = ImGui::GetIO();
        io.BackendRendererUserData = this;
        io.BackendRendererName = "imgui_nri";
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

#ifdef PLATFORM_WINDOWS
        if (Renderer::GetGraphicsAPI() == nri::GraphicsAPI::D3D12)
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
        if (Renderer::GetGraphicsAPI() == nri::GraphicsAPI::VK)
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

        nri::SamplerDesc samplerDesc = {};
        samplerDesc.filters.min = nri::Filter::LINEAR;
        samplerDesc.filters.mag = nri::Filter::LINEAR;
        samplerDesc.filters.mip = nri::Filter::LINEAR;
        samplerDesc.addressModes.u = nri::AddressMode::REPEAT;
        samplerDesc.addressModes.v = nri::AddressMode::REPEAT;
        samplerDesc.addressModes.w = nri::AddressMode::REPEAT;
        samplerDesc.mipMax = 1.0f;

        nri::Result samplerRes = nri.CreateSampler(*m_Device, samplerDesc, m_FontSampler);
        LOG_ASSERT(samplerRes == nri::Result::SUCCESS && m_FontSampler, "Failed to create ImGui font sampler");

        m_BindingLayoutDesc = BindingLayoutDesc();
        m_BindingLayoutDesc->SetRegisterSpace(0);
        m_BindingLayoutDesc->SetVisibility(nri::StageBits::ALL);
        m_BindingLayoutDesc->AddItem(BindingLayoutItem::PushConstants(0, sizeof(ImGuiPushConstants)));
        m_BindingLayoutDesc->AddItem(BindingLayoutItem::Texture_SRV(0));
        m_BindingLayoutDesc->AddItem(BindingLayoutItem::Sampler(0));

        m_PipelineLayout = m_BindingLayoutDesc->CreatePipelineLayout(nri::StageBits::ALL, m_Device);
        LOG_ASSERT(m_PipelineLayout, "Failed to create ImGui pipeline layout");

        return true;
    }

    nri::CommandBuffer *ImGui_NRI::BeginCommandBuffer()
    {
        auto *deviceManager = DeviceManager::GetInstance();
        if (!deviceManager)
            return nullptr;

        const auto &nri = deviceManager->GetCoreInterface();
        auto &slot = m_CommandSlots[m_CurrentSlotIndex];
        m_CurrentSlotIndex = (m_CurrentSlotIndex + 1) % static_cast<uint32_t>(m_CommandSlots.size());

        if (m_CommandFence && slot.fenceValue > 0 && nri.GetFenceValue(*m_CommandFence) < slot.fenceValue)
        {
            nri.Wait(*m_CommandFence, slot.fenceValue);
        }

        nri.ResetCommandAllocator(*slot.allocator);
        nri.BeginCommandBuffer(*slot.commandBuffer, BindlessSystem::GetDescriptorPool());
        return slot.commandBuffer;
    }

    void ImGui_NRI::SubmitCommandBuffer(nri::CommandBuffer *cmd)
    {
        if (!cmd)
            return;

        auto *deviceManager = DeviceManager::GetInstance();
        if (!deviceManager)
            return;

        const auto &nri = deviceManager->GetCoreInterface();
        nri.EndCommandBuffer(*cmd);

        const uint64_t signalValue = m_NextFenceValue++;
        for (auto &slot : m_CommandSlots)
        {
            if (slot.commandBuffer == cmd)
            {
                slot.fenceValue = signalValue;
                break;
            }
        }

        nri::FenceSubmitDesc signalFence = {};
        signalFence.fence = m_CommandFence;
        signalFence.value = signalValue;
        signalFence.stages = nri::StageBits::ALL;

        nri::QueueSubmitDesc submitDesc = {};
        submitDesc.commandBuffers = &cmd;
        submitDesc.commandBufferNum = 1;
        if (m_CommandFence)
        {
            submitDesc.signalFences = &signalFence;
            submitDesc.signalFenceNum = 1;
        }

        {
            std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
            nri.QueueSubmit(*deviceManager->GetGraphicsQueue(), submitDesc);
        }
    }

    bool ImGui_NRI::Render(RenderTarget *renderTarget)
    {
        ImDrawData *drawData = ImGui::GetDrawData();
        return Render(drawData, renderTarget);
    }

    bool ImGui_NRI::Render(ImDrawData *drawData, RenderTarget *renderTarget)
    {
        IGN_PROFILE_FUNCTION();

        if (!drawData || drawData->CmdListsCount <= 0 || !renderTarget)
            return true;

        nri::CommandBuffer *cmd = BeginCommandBuffer();
        if (!cmd)
            return false;

        const bool result = Render(cmd, drawData, renderTarget);

        SubmitCommandBuffer(cmd);
        return result;
    }

    bool ImGui_NRI::Render(nri::CommandBuffer *cmd, ImDrawData *drawData, RenderTarget *renderTarget)
    {
        IGN_PROFILE_FUNCTION();

        if (!cmd || !drawData || drawData->CmdListsCount <= 0 || !renderTarget)
            return true;

        int fbWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
        int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
        if (fbWidth <= 0 || fbHeight <= 0)
            return true;

        if (!UpdateGeometry(drawData))
        {
            return false;
        }

        Ref<GraphicsPipeline> pipeline = GetPSO(renderTarget);
        if (!pipeline || !pipeline->GetHandle())
            return false;

        if (!m_PipelineLayout)
            m_PipelineLayout = pipeline->GetPipelineLayout();

        ImGuiPushConstants pushConstants = {};
        pushConstants.invDisplaySize[0] = 1.0f / drawData->DisplaySize.x;
        pushConstants.invDisplaySize[1] = 1.0f / drawData->DisplaySize.y;
        pushConstants.displayPos[0] = drawData->DisplayPos.x;
        pushConstants.displayPos[1] = drawData->DisplayPos.y;
        ImVec2 clipOff = drawData->DisplayPos;
        ImVec2 clipScale = drawData->FramebufferScale;

        const auto &nri = DeviceManager::GetInstance()->GetCoreInterface();

        renderTarget->BeginRendering(cmd, false /* clearColor */, false /* clearDepth */);

        nri.CmdSetPipelineLayout(*cmd, nri::BindPoint::GRAPHICS, *pipeline->GetPipelineLayout());
        nri.CmdSetPipeline(*cmd, *pipeline->GetHandle());

        nri::Viewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(fbWidth);
        viewport.height = static_cast<float>(fbHeight);
        viewport.depthMin = 0.0f;
        viewport.depthMax = 1.0f;
        nri.CmdSetViewports(*cmd, &viewport, 1);

        nri::VertexBufferDesc vbDesc = {};
        vbDesc.buffer = m_VertexBuffer->GetHandle();
        vbDesc.offset = 0;
        vbDesc.stride = sizeof(ImGuiVertexData);
        nri.CmdSetVertexBuffers(*cmd, 0, &vbDesc, 1);

        nri.CmdSetIndexBuffer(*cmd, *m_IndexBuffer->GetHandle(), 0,
            sizeof(ImDrawIdx) == 2 ? nri::IndexType::UINT16 : nri::IndexType::UINT32);

        nri::SetRootConstantsDesc rootConstDesc = {};
        rootConstDesc.rootConstantIndex = 0;
        rootConstDesc.data = &pushConstants;
        rootConstDesc.size = sizeof(pushConstants);
        rootConstDesc.offset = 0;
        rootConstDesc.bindPoint = nri::BindPoint::GRAPHICS;
        nri.CmdSetRootConstants(*cmd, rootConstDesc);

        nri::Descriptor *lastTexture = nullptr;
        Ref<BindingSet> lastBindingSet = nullptr;

        int vtxOffset = 0;
        int idxOffset = 0;
        for (int n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList *cmdList = drawData->CmdLists[n];

            for (int i = 0; i < cmdList->CmdBuffer.Size; ++i)
            {
                const ImDrawCmd *pCmd = &cmdList->CmdBuffer[i];

                if (pCmd->UserCallback)
                {
                    pCmd->UserCallback(cmdList, pCmd);
                    lastTexture = nullptr;
                    lastBindingSet = nullptr;
                }
                else
                {
                    nri::Descriptor *textureSRV = (nri::Descriptor *)pCmd->TexRef.GetTexID();
                    if (!textureSRV && m_FontTexture)
                    {
                        textureSRV = m_FontTexture->GetShaderResourceView();
                    }

                    if (textureSRV != lastTexture)
                    {
                        lastTexture = textureSRV;
                        lastBindingSet = GetBindingSet(textureSRV);
                    }

                    if (lastBindingSet && lastBindingSet->GetHandle())
                    {
                        nri::SetDescriptorSetDesc setDesc = {};
                        setDesc.setIndex = 0;
                        setDesc.descriptorSet = lastBindingSet->GetHandle();
                        setDesc.bindPoint = nri::BindPoint::GRAPHICS;
                        nri.CmdSetDescriptorSet(*cmd, setDesc);
                    }

                    ImVec2 clipMin((pCmd->ClipRect.x - clipOff.x) * clipScale.x, (pCmd->ClipRect.y - clipOff.y) * clipScale.y);
                    ImVec2 clipMax((pCmd->ClipRect.z - clipOff.x) * clipScale.x, (pCmd->ClipRect.w - clipOff.y) * clipScale.y);

                    if (clipMin.x < 0.0f) clipMin.x = 0.0f;
                    if (clipMin.y < 0.0f) clipMin.y = 0.0f;
                    if (clipMax.x > static_cast<float>(fbWidth)) clipMax.x = static_cast<float>(fbWidth);
                    if (clipMax.y > static_cast<float>(fbHeight)) clipMax.y = static_cast<float>(fbHeight);

                    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                        continue;

                    nri::Rect scissor = {};
                    scissor.x = static_cast<nri::Dim_t>(clipMin.x);
                    scissor.y = static_cast<nri::Dim_t>(clipMin.y);
                    scissor.width = static_cast<nri::Dim_t>(clipMax.x - clipMin.x);
                    scissor.height = static_cast<nri::Dim_t>(clipMax.y - clipMin.y);
                    nri.CmdSetScissors(*cmd, &scissor, 1);

                    nri::DrawIndexedDesc drawDesc = {};
                    drawDesc.indexNum = pCmd->ElemCount;
                    drawDesc.instanceNum = 1;
                    drawDesc.baseIndex = static_cast<uint32_t>(idxOffset + pCmd->IdxOffset);
                    drawDesc.baseVertex = static_cast<int32_t>(vtxOffset + pCmd->VtxOffset);
                    drawDesc.baseInstance = 0;

                    nri.CmdDrawIndexed(*cmd, drawDesc);
                }
            }
            idxOffset += cmdList->IdxBuffer.Size;
            vtxOffset += cmdList->VtxBuffer.Size;
        }

        renderTarget->EndRendering(cmd);
        return true;
    }

    void ImGui_NRI::BackBufferResizing()
    {
        m_GraphicsPipelines.clear();
        std::scoped_lock lock(m_BindingsCacheMutex);
        m_BindingsCache.clear();
    }

    Ref<GraphicsPipeline> ImGui_NRI::GetPSO(RenderTarget *renderTarget)
    {
        IGN_PROFILE_FUNCTION();

        if (!renderTarget)
            return nullptr;

        FramebufferKey fbKey = MakeFramebufferKey(renderTarget, nri::FillMode::SOLID, nri::CullMode::NONE);
        auto pipelineIter = m_GraphicsPipelines.find(fbKey);
        if (pipelineIter != m_GraphicsPipelines.end())
        {
            return pipelineIter->second;
        }

        if (!m_BindingLayoutDesc.has_value())
        {
            m_BindingLayoutDesc = BindingLayoutDesc();
            m_BindingLayoutDesc->SetRegisterSpace(0);
            m_BindingLayoutDesc->SetVisibility(nri::StageBits::ALL);
            m_BindingLayoutDesc->AddItem(BindingLayoutItem::PushConstants(0, sizeof(ImGuiPushConstants)));
            m_BindingLayoutDesc->AddItem(BindingLayoutItem::Texture_SRV(0));
            m_BindingLayoutDesc->AddItem(BindingLayoutItem::Sampler(0));
        }

        auto vertexShader = Shader::Create("resources/shaders/imgui.vertex.hlsl", UMBRA_SHADER_TYPE_VERTEX);
        auto pixelShader = Shader::Create("resources/shaders/imgui.pixel.hlsl", UMBRA_SHADER_TYPE_PIXEL);

        GraphicsPipelineParams params;
        params.fillMode = nri::FillMode::SOLID;
        params.cullMode = nri::CullMode::NONE;
        params.primitiveType = nri::Topology::TRIANGLE_LIST;

        params.enableBlend = true;
        params.srcBlend = nri::BlendFactor::SRC_ALPHA;
        params.destBlend = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.srcBlendAlpha = nri::BlendFactor::ONE_MINUS_SRC_ALPHA;
        params.destBlendAlpha = nri::BlendFactor::ZERO;
        params.blendOp = nri::BlendOp::ADD;
        params.blendOpAlpha = nri::BlendOp::ADD;

        params.enableDepthClip = true;
        params.enableScissor = true;

        params.enableDepthTest = false;
        params.enableDepthWrite = false;
        params.enableDepthStencil = false;

        Ref<GraphicsPipeline> pipeline = GraphicsPipeline::Create("ImGui Pipeline");
        pipeline->AddBindingLayout(*m_BindingLayoutDesc);
        pipeline->SetShaders({ vertexShader, pixelShader });
        pipeline->Build(fbKey, params);

        m_GraphicsPipelines[fbKey] = pipeline;
        if (!m_PipelineLayout)
        {
            m_PipelineLayout = pipeline->GetPipelineLayout();
        }

        return pipeline;
    }

    Ref<BindingSet> ImGui_NRI::GetBindingSet(nri::Descriptor *textureSRV)
    {
        IGN_PROFILE_FUNCTION();

        if (!textureSRV && m_FontTexture)
            textureSRV = m_FontTexture->GetShaderResourceView();

        if (!textureSRV || !m_PipelineLayout)
            return nullptr;

        std::scoped_lock lock(m_BindingsCacheMutex);

        auto iter = m_BindingsCache.find(textureSRV);
        if (iter != m_BindingsCache.end())
            return iter->second;

        BindingSetDesc desc;
        desc.AddItem(BindingSetItem::TextureSRV(0, textureSRV));
        desc.AddItem(BindingSetItem::Sampler(0, m_FontSampler));

        Ref<BindingSet> binding = BindingSet::Create("ImGui Binding Set",
            BindingSet::AllocateAndUpdate(m_PipelineLayout, 0, desc));
        LOG_ASSERT(binding && binding->GetHandle(), "Failed to create ImGui binding set");

        m_BindingsCache[textureSRV] = binding;
        return binding;
    }

    void ImGui_NRI::InvalidateTextureCache(void *textureHandle)
    {
        if (!s_Instance || !textureHandle)
            return;
        std::scoped_lock lock(s_Instance->m_BindingsCacheMutex);
        s_Instance->m_BindingsCache.erase(static_cast<nri::Descriptor *>(textureHandle));
    }

    void ImGui_NRI::InvalidateTextureCache(nri::Descriptor *textureSRV)
    {
        if (!s_Instance || !textureSRV)
            return;
        std::scoped_lock lock(s_Instance->m_BindingsCacheMutex);
        s_Instance->m_BindingsCache.erase(textureSRV);
    }

    void ImGui_NRI::ClearTextureCache()
    {
        if (!s_Instance)
            return;

        std::scoped_lock lock(s_Instance->m_BindingsCacheMutex);
        s_Instance->m_BindingsCache.clear();
    }

    bool ImGui_NRI::UpdateGeometry(ImDrawData *drawData)
    {
        IGN_PROFILE_FUNCTION();

        if (drawData->TotalVtxCount <= 0 || drawData->TotalIdxCount <= 0)
            return true;

        const size_t actualVtxBytes = static_cast<size_t>(drawData->TotalVtxCount) * sizeof(ImGuiVertexData);
        const size_t actualIdxBytes = static_cast<size_t>(drawData->TotalIdxCount) * sizeof(ImDrawIdx);

        const size_t allocVtxBytes = (static_cast<size_t>(drawData->TotalVtxCount) + 5000) * sizeof(ImGuiVertexData);
        const size_t allocIdxBytes = (static_cast<size_t>(drawData->TotalIdxCount) + 5000) * sizeof(ImDrawIdx);

        if (!m_VertexBuffer || m_VertexBuffer->GetByteSize() < actualVtxBytes)
        {
            m_VertexBuffer = VertexBuffer::Create(allocVtxBytes, "ImGui vertex buffer");
            if (!m_VertexBuffer)
                return false;
        }

        if (!m_IndexBuffer || m_IndexBuffer->GetByteSize() < actualIdxBytes)
        {
            m_IndexBuffer = IndexBuffer::Create(allocIdxBytes, "ImGui index buffer");
            if (!m_IndexBuffer)
                return false;
        }

        m_VertexBufferCPU.resize(m_VertexBuffer->GetByteSize() / sizeof(ImGuiVertexData));
        m_IndexBufferCPU.resize(m_IndexBuffer->GetByteSize() / sizeof(ImDrawIdx));

        ImGuiVertexData *vtxDst = m_VertexBufferCPU.data();
        ImDrawIdx *idxDst = m_IndexBufferCPU.data();

        for (int n = 0; n < drawData->CmdListsCount; ++n)
        {
            const ImDrawList *cmdList = drawData->CmdLists[n];

            for (int i = 0; i < cmdList->VtxBuffer.Size; ++i)
            {
                const ImDrawVert &src = cmdList->VtxBuffer[i];
                ImGuiVertexData &dst = vtxDst[i];

                dst.position = glm::vec2(src.pos.x, src.pos.y);
                dst.texCoord = glm::vec2(src.uv.x, src.uv.y);

                dst.color.r = ((src.col >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f;
                dst.color.g = ((src.col >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f;
                dst.color.b = ((src.col >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f;
                dst.color.a = ((src.col >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f;
            }

            std::memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));

            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }

        m_VertexBuffer->SetData(m_VertexBufferCPU.data(), actualVtxBytes);
        m_IndexBuffer->SetData(m_IndexBufferCPU.data(), actualIdxBytes);

        return true;
    }

    void ImGui_NRI::Shutdown()
    {
        m_IsShuttingDown = true;

#ifdef PLATFORM_WINDOWS
        if (Renderer::GetGraphicsAPI() == nri::GraphicsAPI::D3D12)
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
        if (Renderer::GetGraphicsAPI() == nri::GraphicsAPI::VK)
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

        {
            std::scoped_lock lock(m_BindingsCacheMutex);
            m_BindingsCache.clear();
        }
        m_GraphicsPipelines.clear();
        m_BindingLayoutDesc.reset();
        m_PipelineLayout = nullptr;

        m_FontTexture = nullptr;
        auto *deviceManager = DeviceManager::GetInstance();
        if (deviceManager && m_FontSampler)
        {
            deviceManager->GetCoreInterface().DestroyDescriptor(m_FontSampler);
            m_FontSampler = nullptr;
        }

        m_VertexBuffer = nullptr;
        m_IndexBuffer = nullptr;

        if (deviceManager)
        {
            const auto &nri = deviceManager->GetCoreInterface();
            for (auto &slot : m_CommandSlots)
            {
                if (slot.commandBuffer)
                {
                    nri.DestroyCommandBuffer(slot.commandBuffer);
                    slot.commandBuffer = nullptr;
                }
                if (slot.allocator)
                {
                    nri.DestroyCommandAllocator(slot.allocator);
                    slot.allocator = nullptr;
                }
            }
            if (m_CommandFence)
            {
                nri.DestroyFence(m_CommandFence);
                m_CommandFence = nullptr;
            }
        }

        s_Instance = nullptr;
    }

#ifdef IGNITE_WITH_VULKAN
    static ImGuiViewportRendererData_VK *GetViewportDataVK(ImGuiViewport *viewport)
    {
        return static_cast<ImGuiViewportRendererData_VK *>(viewport->RendererUserData);
    }

    static nri::Format GetNRIFormatFromVkFormat(vk::Format format)
    {
        switch (format)
        {
            case vk::Format::eR8G8B8A8Unorm: return nri::Format::RGBA8_UNORM;
            case vk::Format::eR8G8B8A8Srgb: return nri::Format::RGBA8_SRGB;
            case vk::Format::eB8G8R8A8Unorm: return nri::Format::BGRA8_UNORM;
            case vk::Format::eB8G8R8A8Srgb: return nri::Format::BGRA8_SRGB;
            default: return nri::Format::UNKNOWN;
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

    template <typename T>
    static void RemoveViewportPipelines(ImGui_NRI *renderer, T *viewportData)
    {
        if (!renderer || !viewportData)
            return;

        for (const auto &rt : viewportData->renderTargets)
        {
            if (!rt)
                continue;

            FramebufferKey key = MakeFramebufferKey(rt.get(), nri::FillMode::SOLID, nri::CullMode::NONE);
            renderer->RemoveGraphicsPipeline(key);
        }
    }

    static bool CreateViewportSwapChainAndRenderTargetsVK(ImGuiViewportRendererData_VK *viewportData, DeviceManager_VK &deviceManager, uint32_t width, uint32_t height)
    {
        if (!viewportData || !viewportData->surface || !deviceManager.m_VulkanDevice)
            return false;

        const vk::SurfaceCapabilitiesKHR capabilities = deviceManager.m_VulkanPhysicalDevice.getSurfaceCapabilitiesKHR(viewportData->surface);
        const std::vector<vk::SurfaceFormatKHR> surfaceFormats = deviceManager.m_VulkanPhysicalDevice.getSurfaceFormatsKHR(viewportData->surface);
        const std::vector<vk::PresentModeKHR> presentModes = deviceManager.m_VulkanPhysicalDevice.getSurfacePresentModesKHR(viewportData->surface);

        if (surfaceFormats.empty() || presentModes.empty())
            return false;

        vk::Extent2D extent{};
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            extent = capabilities.currentExtent;
        }
        else
        {
            extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        viewportData->surfaceFormat = surfaceFormats.front();
        for (const vk::SurfaceFormatKHR &surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == vk::Format::eB8G8R8A8Unorm || surfaceFormat.format == vk::Format::eR8G8B8A8Unorm)
            {
                viewportData->surfaceFormat = surfaceFormat;
                break;
            }
        }

        const vk::PresentModeKHR presentMode = SelectViewportPresentMode(presentModes, deviceManager.GetDeviceParameters().vsyncEnable);

        uint32_t minImageCount = std::max(capabilities.minImageCount, deviceManager.GetDeviceParameters().swapChainBufferCount);
        if (capabilities.maxImageCount > 0)
            minImageCount = std::min(minImageCount, capabilities.maxImageCount);

        std::unordered_set<uint32_t> uniqueQueues =
        {
            static_cast<uint32_t>(deviceManager.m_GraphicsQueueFamily),
            static_cast<uint32_t>(deviceManager.m_PresentQueueFamily)
        };
        std::vector<uint32_t> queueFamilies(uniqueQueues.begin(), uniqueQueues.end());
        const bool sharedSwapChain = queueFamilies.size() > 1;

        if (viewportData->swapChain)
        {
            const auto &nri = deviceManager.GetCoreInterface();
            for (auto *tex : viewportData->nriTextures)
            {
                if (tex)
                    nri.DestroyTexture(tex);
            }
            viewportData->nriTextures.clear();
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
            .setQueueFamilyIndexCount(sharedSwapChain ? static_cast<uint32_t>(queueFamilies.size()) : 0)
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
        viewportData->nriTextures.resize(viewportData->swapChainImages.size(), nullptr);
        viewportData->renderTargets.reserve(viewportData->swapChainImages.size());

        nri::Format colorFormat = GetNRIFormatFromVkFormat(viewportData->surfaceFormat.format);
        if (colorFormat == nri::Format::UNKNOWN)
            colorFormat = deviceManager.GetDeviceParameters().swapChainFormat;

        for (size_t n = 0; n < viewportData->swapChainImages.size(); ++n)
        {
            nri::TextureVKDesc texVKDesc = {};
            texVKDesc.vkImage = (uint64_t)(VkImage)viewportData->swapChainImages[n];
            texVKDesc.vkFormat = (uint32_t)(VkFormat)viewportData->surfaceFormat.format;
            texVKDesc.vkImageType = (uint32_t)VK_IMAGE_TYPE_2D;
            texVKDesc.vkImageUsageFlags = (uint32_t)(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
            texVKDesc.width = static_cast<nri::Dim_t>(extent.width);
            texVKDesc.height = static_cast<nri::Dim_t>(extent.height);
            texVKDesc.depth = 1;
            texVKDesc.mipNum = 1;
            texVKDesc.layerNum = 1;
            texVKDesc.sampleNum = 1;

            if (deviceManager.m_WrapperVK.CreateTextureVK(*deviceManager.GetDevice(), texVKDesc, viewportData->nriTextures[n]) != nri::Result::SUCCESS)
            {
                return false;
            }

            RenderTargetCreateInfo createInfo;
            createInfo.width = extent.width;
            createInfo.height = extent.height;
            createInfo.attachments =
            {
                FramebufferAttachments {
                    .name = "ImGui viewport",
                    .format = colorFormat,
                    .state = nri::Layout::PRESENT,
                    .nativeObjectPtr = viewportData->nriTextures[n],
                    .isNativeObject = true,
                    .nativeObjectType = 2
                }
            };

            viewportData->renderTargets.emplace_back(RenderTarget::Create(createInfo, "ImGui viewport"));
        }

        viewportData->currentImageIndex = 0;
        viewportData->hasAcquiredImage = false;
        return true;
    }

    void ImGui_NRI::RendererCreateWindowVK(ImGuiViewport *viewport)
    {
        if (Renderer::GetGraphicsAPI() != nri::GraphicsAPI::VK)
            return;

        DeviceManager_VK *deviceManager = DeviceManager_VK::GetInstance();
        if (!deviceManager || !deviceManager->m_VulkanDevice || !deviceManager->m_VulkanInstance)
            return;

        ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();
        if (!platformIO.Platform_CreateVkSurface)
            return;

        Ref<ImGuiViewportRendererData_VK> viewportData = CreateRef<ImGuiViewportRendererData_VK>();

        ImU64 surface = 0;
        const int surfaceResult = platformIO.Platform_CreateVkSurface(viewport, static_cast<ImU64>((uintptr_t)(VkInstance)deviceManager->m_VulkanInstance), nullptr, &surface);
        if (surfaceResult != 0 || surface == 0)
            return;

        viewportData->surface = vk::SurfaceKHR((VkSurfaceKHR)static_cast<uintptr_t>(surface));
        viewportData->acquireFence = deviceManager->m_VulkanDevice.createFence(vk::FenceCreateInfo());
        viewportData->presentSemaphore = deviceManager->m_VulkanDevice.createSemaphore(vk::SemaphoreCreateInfo());

        if (!viewportData->acquireFence || !viewportData->presentSemaphore)
        {
            if (viewportData->acquireFence)
                deviceManager->m_VulkanDevice.destroyFence(viewportData->acquireFence);
            if (viewportData->presentSemaphore)
                deviceManager->m_VulkanDevice.destroySemaphore(viewportData->presentSemaphore);
            deviceManager->m_VulkanInstance.destroySurfaceKHR(viewportData->surface);
            return;
        }

        if (!CreateViewportSwapChainAndRenderTargetsVK(viewportData.get(), *deviceManager,
            static_cast<uint32_t>(std::max(viewport->Size.x, 1.0f)), static_cast<uint32_t>(std::max(viewport->Size.y, 1.0f))))
        {
            deviceManager->m_VulkanDevice.destroyFence(viewportData->acquireFence);
            deviceManager->m_VulkanDevice.destroySemaphore(viewportData->presentSemaphore);
            deviceManager->m_VulkanInstance.destroySurfaceKHR(viewportData->surface);
            return;
        }

        viewport->RendererUserData = viewportData.get();
    }

    void ImGui_NRI::RendererDestroyWindowVK(ImGuiViewport *viewport)
    {
        auto *viewportData = GetViewportDataVK(viewport);
        if (!viewportData)
            return;

        auto *renderer = static_cast<ImGui_NRI *>(ImGui::GetIO().BackendRendererUserData);
        DeviceManager_VK *deviceManager = DeviceManager_VK::GetInstance();

        if (deviceManager && deviceManager->m_VulkanDevice)
        {
            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

            if (renderer)
                RemoveViewportPipelines(renderer, viewportData);

            const auto &nri = deviceManager->GetCoreInterface();
            for (auto *tex : viewportData->nriTextures)
            {
                if (tex)
                    nri.DestroyTexture(tex);
            }
            viewportData->nriTextures.clear();
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

    void ImGui_NRI::RendererSetWindowSizeVK(ImGuiViewport *viewport, ImVec2 size)
    {
        auto *viewportData = GetViewportDataVK(viewport);
        auto *renderer = static_cast<ImGui_NRI *>(ImGui::GetIO().BackendRendererUserData);
        if (!viewportData || !viewportData->swapChain || !renderer || !renderer->m_Device)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (size.x <= 0.0f || size.y <= 0.0f)
            return;

        const uint32_t requestedWidth = static_cast<uint32_t>(size.x);
        const uint32_t requestedHeight = static_cast<uint32_t>(size.y);

        if (viewportData->hasPendingResize
            && viewportData->pendingWidth == requestedWidth
            && viewportData->pendingHeight == requestedHeight)
            return;

        if (!viewportData->hasPendingResize
            && viewportData->extent.width == requestedWidth
            && viewportData->extent.height == requestedHeight)
            return;

        viewportData->pendingWidth = requestedWidth;
        viewportData->pendingHeight = requestedHeight;
        viewportData->hasPendingResize = true;
    }

    void ImGui_NRI::RendererRenderWindowVK(ImGuiViewport *viewport, void *)
    {
        auto *renderer = static_cast<ImGui_NRI *>(ImGui::GetIO().BackendRendererUserData);
        auto *viewportData = GetViewportDataVK(viewport);
        DeviceManager_VK *deviceManager = DeviceManager_VK::GetInstance();
        if (!renderer || !viewportData || !viewportData->swapChain || !deviceManager)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (viewportData->hasPendingResize)
        {
            const uint32_t width = viewportData->pendingWidth;
            const uint32_t height = viewportData->pendingHeight;

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

        const vk::Result resetFencesResult = deviceManager->m_VulkanDevice.resetFences(1, &viewportData->acquireFence);
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

        const vk::Result waitFencesResult = deviceManager->m_VulkanDevice.waitForFences(1, &viewportData->acquireFence, VK_TRUE, std::numeric_limits<uint64_t>::max());

        viewportData->hasAcquiredImage = true;

        if (viewportData->currentImageIndex >= viewportData->renderTargets.size())
            return;

        renderer->Render(viewport->DrawData, viewportData->renderTargets[viewportData->currentImageIndex].get());
    }

    void ImGui_NRI::RendererSwapBuffersVK(ImGuiViewport *viewport, void *)
    {
        auto *renderer = static_cast<ImGui_NRI *>(ImGui::GetIO().BackendRendererUserData);
        auto *viewportData = GetViewportDataVK(viewport);
        DeviceManager_VK *deviceManager = DeviceManager_VK::GetInstance();
        if (!renderer || !viewportData || !viewportData->swapChain || !deviceManager)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (!viewportData->hasAcquiredImage)
            return;

        vk::SubmitInfo submitInfo{};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &viewportData->presentSemaphore;
        {
            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
            deviceManager->m_GraphicsQueue.submit(1, &submitInfo, nullptr);
        }

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

    static bool CreateViewportRenderTargets(ImGuiViewportRendererData_DX12 *viewportData, uint32_t width, uint32_t height)
    {
        if (!viewportData || !viewportData->swapChain)
            return false;

        DeviceManager_DX12 &deviceManager = DeviceManager_DX12::GetInstance();
        const auto &nri = deviceManager.GetCoreInterface();

        for (auto *tex : viewportData->nriTextures)
        {
            if (tex)
                nri.DestroyTexture(tex);
        }
        viewportData->nriTextures.clear();
        viewportData->renderTargets.clear();
        viewportData->backBuffers.clear();

        viewportData->backBuffers.resize(viewportData->swapChainDesc.BufferCount);
        viewportData->nriTextures.resize(viewportData->swapChainDesc.BufferCount, nullptr);
        viewportData->renderTargets.reserve(viewportData->swapChainDesc.BufferCount);

        for (uint32_t i = 0; i < viewportData->swapChainDesc.BufferCount; ++i)
        {
            HRESULT hr = viewportData->swapChain->GetBuffer(i, IID_PPV_ARGS(&viewportData->backBuffers[i]));
            if (FAILED(hr))
                return false;

            nri::TextureD3D12Desc texDesc{};
            texDesc.d3d12Resource = viewportData->backBuffers[i].Get();
            if (deviceManager.m_WrapperD3D12.CreateTextureD3D12(*deviceManager.GetDevice(), texDesc, viewportData->nriTextures[i]) != nri::Result::SUCCESS)
            {
                return false;
            }

            RenderTargetCreateInfo createInfo;
            createInfo.width = width;
            createInfo.height = height;
            createInfo.attachments =
            {
                FramebufferAttachments {
                    .name = "ImGui viewport",
                    .format = deviceManager.GetDeviceParameters().swapChainFormat,
                    .state = nri::Layout::PRESENT,
                    .nativeObjectPtr = viewportData->nriTextures[i],
                    .isNativeObject = true,
                    .nativeObjectType = 1
                }
            };

            viewportData->renderTargets.emplace_back(RenderTarget::Create(createInfo, "ImGui viewport"));
        }

        return true;
    }

    void ImGui_NRI::RendererCreateWindow(ImGuiViewport *viewport)
    {
        if (Renderer::GetGraphicsAPI() != nri::GraphicsAPI::D3D12)
            return;

        DeviceManager_DX12 &deviceManager = DeviceManager_DX12::GetInstance();
        HWND hwnd = reinterpret_cast<HWND>(viewport->PlatformHandleRaw);
        if (!hwnd)
            return;

        Ref<ImGuiViewportRendererData_DX12> viewportData = CreateRef<ImGuiViewportRendererData_DX12>();
        viewportData->swapChainDesc = deviceManager.m_SwapChainDesc;
        viewportData->swapChainDesc.Width = static_cast<uint32_t>(std::max(viewport->Size.x, 1.0f));
        viewportData->swapChainDesc.Height = static_cast<uint32_t>(std::max(viewport->Size.y, 1.0f));

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
        HRESULT hr = deviceManager.m_DxgiFactory2->CreateSwapChainForHwnd(
            deviceManager.m_GraphicsQueue.Get(),
            hwnd,
            &viewportData->swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1);

        LOG_ASSERT(SUCCEEDED(hr), "Failed to create ImGui viewport swap chain");
        if (FAILED(hr))
            return;

        hr = swapChain1.As(&viewportData->swapChain);
        LOG_ASSERT(SUCCEEDED(hr), "Failed to query IDXGISwapChain3 for ImGui viewport");
        if (FAILED(hr))
            return;

        if (!CreateViewportRenderTargets(viewportData.get(), static_cast<uint32_t>(std::max(viewport->Size.x, 1.0f)), static_cast<uint32_t>(std::max(viewport->Size.y, 1.0f))))
            return;

        viewport->RendererUserData = viewportData.get();
    }

    void ImGui_NRI::RendererDestroyWindow(ImGuiViewport *viewport)
    {
        auto *viewportData = GetViewportData(viewport);
        if (!viewportData)
            return;

        DeviceManager_DX12 &deviceManager = DeviceManager_DX12::GetInstance();
        const auto &nri = deviceManager.GetCoreInterface();

        for (auto *tex : viewportData->nriTextures)
        {
            if (tex)
                nri.DestroyTexture(tex);
        }
        viewportData->nriTextures.clear();
        viewportData->renderTargets.clear();
        viewportData->backBuffers.clear();
        viewportData->swapChain = nullptr;

        delete viewportData;
        viewport->RendererUserData = nullptr;
    }

    void ImGui_NRI::RendererSetWindowSize(ImGuiViewport *viewport, ImVec2 size)
    {
        auto *viewportData = GetViewportData(viewport);
        auto *renderer = static_cast<ImGui_NRI *>(ImGui::GetIO().BackendRendererUserData);
        if (!viewportData || !viewportData->swapChain || !renderer || !renderer->m_Device)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (size.x <= 0.0f || size.y <= 0.0f)
            return;

        const uint32_t requestedWidth = static_cast<uint32_t>(size.x);
        const uint32_t requestedHeight = static_cast<uint32_t>(size.y);

        if (viewportData->hasPendingResize
            && viewportData->pendingWidth == requestedWidth
            && viewportData->pendingHeight == requestedHeight)
            return;

        if (!viewportData->hasPendingResize
            && viewportData->swapChainDesc.Width == requestedWidth
            && viewportData->swapChainDesc.Height == requestedHeight)
            return;

        viewportData->pendingWidth = requestedWidth;
        viewportData->pendingHeight = requestedHeight;
        viewportData->hasPendingResize = true;
    }

    void ImGui_NRI::RendererRenderWindow(ImGuiViewport *viewport, void *)
    {
        auto *renderer = static_cast<ImGui_NRI *>(ImGui::GetIO().BackendRendererUserData);
        auto *viewportData = GetViewportData(viewport);
        if (!renderer || !viewportData || !viewportData->swapChain)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        if (viewportData->hasPendingResize)
        {
            const uint32_t width = viewportData->pendingWidth;
            const uint32_t height = viewportData->pendingHeight;

            if (width > 0 && height > 0)
            {
                HRESULT hr = S_OK;
                {
                    std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

                    RemoveViewportPipelines(renderer, viewportData);

                    DeviceManager_DX12 &deviceManager = DeviceManager_DX12::GetInstance();
                    for (auto *tex : viewportData->nriTextures)
                    {
                        if (tex)
                            deviceManager.GetCoreInterface().DestroyTexture(tex);
                    }
                    viewportData->nriTextures.clear();
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
                    LOG_WARN("Failed to resize ImGui viewport swap chain (HRESULT=0x{:08X}, size={}x{})", static_cast<uint32_t>(hr), width, height);
                    viewportData->hasPendingResize = false;
                    return;
                }
            }
        }

        const uint32_t backBufferIndex = viewportData->swapChain->GetCurrentBackBufferIndex();
        if (backBufferIndex >= viewportData->renderTargets.size())
            return;

        renderer->Render(viewport->DrawData, viewportData->renderTargets[backBufferIndex].get());
    }

    void ImGui_NRI::RendererSwapBuffers(ImGuiViewport *viewport, void *)
    {
        auto *renderer = static_cast<ImGui_NRI *>(ImGui::GetIO().BackendRendererUserData);
        auto *viewportData = GetViewportData(viewport);
        if (!renderer || !viewportData || !viewportData->swapChain)
            return;

        if (renderer->m_IsShuttingDown)
            return;

        std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());

        const HRESULT hr = viewportData->swapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);

        if (hr == DXGI_ERROR_WAS_STILL_DRAWING || hr == DXGI_STATUS_OCCLUDED)
            return;

        if (hr == DXGI_ERROR_INVALID_CALL || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            LOG_WARN("ImGui viewport present skipped during teardown/reset (HRESULT=0x{:08X})", static_cast<uint32_t>(hr));
            return;
        }

        LOG_ASSERT(SUCCEEDED(hr), "Failed to present ImGui viewport swap chain");
    }
#endif
}
