// Copyright (c) 2026 Evangelion Manuhutu

#include "imgui_layer.hpp"
#include "ignite/core/application.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <ImGuizmo.h>

#ifdef PLATFORM_WINDOWS
    #include <backends/imgui_impl_dx12.h>
    #include <backends/imgui_impl_win32.h>
    #include "ignite/core/device/device_manager_dx12.hpp"
#endif

#ifdef IGNITE_WITH_VULKAN
    #include <backends/imgui_impl_vulkan.h>
    #include "ignite/core/device/device_manager_vk.hpp"
#endif

#include "ignite/graphics/renderer.hpp"

#include <fstream>

namespace ignite
{

    GuiFont::GuiFont()
        : m_IsDefault(false)
        , m_IsCompressed(false)
        , m_SizeAtDefaultScale(0.0f)
    {
    }

    GuiFont::GuiFont(float size)
        : m_IsDefault(true)
        , m_IsCompressed(false)
        , m_SizeAtDefaultScale(size)
    {
    }

    GuiFont::GuiFont(std::vector<uint8_t> data, bool isCompressed, float size)
        : m_Data(std::move(data))
        , m_IsDefault(false)
        , m_IsCompressed(isCompressed)
        , m_SizeAtDefaultScale(size)
    {
    }

    void GuiFont::CreateScaledFont(float displayScale)
    {
        ImFontConfig fontConfig;
        fontConfig.SizePixels = m_SizeAtDefaultScale * displayScale;

        m_ImFont = nullptr;

        auto &io = ImGui::GetIO();

        if (m_Data.data() && !m_Data.empty())
        {
            fontConfig.FontDataOwnedByAtlas = false;
            m_ImFont = m_IsCompressed 
                ? io.Fonts->AddFontFromMemoryCompressedTTF(m_Data.data(), static_cast<int>(m_Data.size()), 0.0f, &fontConfig)
                : io.Fonts->AddFontFromMemoryTTF(m_Data.data(), static_cast<int>(m_Data.size()), 0.0f, &fontConfig);
        }
        else if (m_IsDefault)
        {
            m_ImFont = io.Fonts->AddFontDefault(&fontConfig);
        }

        if (m_ImFont)
        {
            io.Fonts->TexID = 0;
        }
    }

    void GuiFont::ReleaseScaledFont()
    {
        m_ImFont = nullptr;
    }

    ImGuiLayer::ImGuiLayer(DeviceManager *deviceManager)
        : Layer("ImGuiLayer")
        , m_DeviceManager(deviceManager)
        , m_SupportExplicitDisplayScaling(deviceManager->GetDeviceParameters().supportExplicitDisplayScaling)
    {

        LOG_ASSERT(m_DeviceManager, "Invalid device manager");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ignite::Path fontPath = "resources/fonts/segoeui.ttf";
        
        LOG_ASSERT(ignite::Path::exists(fontPath), "[ImGui Layer] font does not found");

        std::ifstream file(fontPath, std::ios::binary);
        if (file.is_open())
        {
            file.seekg(0, std::ios::end);
            const uint64_t size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> dataArray;
            dataArray.resize(size);

            file.read(reinterpret_cast<char *>(dataArray.data()), static_cast<std::streamsize>(dataArray.size()));
            
            if (file.good())
            {
                constexpr float fontSize = 16.0f;
                m_Font = CreateRef<GuiFont>(dataArray, false, fontSize);
            }

            file.close();
        }

        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = ImGui::GetStyle().Colors;
        // Text
        colors[ImGuiCol_Text] = ImVec4(1.00f, 0.95f, 0.80f, 1.00f); // Soft cream-yellow
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.45f, 0.30f, 1.00f);

        // Backgrounds
        colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.06f, 1.00f); // Near black
        colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.08f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.06f, 0.96f);

        // Borders
        colors[ImGuiCol_Border] = ImVec4(0.30f, 0.25f, 0.10f, 0.80f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // Frames (Inputs, Checkboxes, etc.)
        colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.14f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.22f, 0.12f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.30f, 0.15f, 1.00f);

        // Title Bars
        colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.11f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.18f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.04f, 1.00f);

        // Menus
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.11f, 0.08f, 1.00f);

        // Scrollbars
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.04f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.30f, 0.10f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.40f, 0.15f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.50f, 0.20f, 1.00f);

        // Interactables (The High-Vis Amber)
        colors[ImGuiCol_CheckMark] = ImVec4(0.95f, 0.80f, 0.10f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.60f, 0.10f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.95f, 0.80f, 0.10f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.30f, 0.25f, 0.05f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.45f, 0.38f, 0.10f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.60f, 0.50f, 0.15f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.30f, 0.25f, 0.05f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.45f, 0.38f, 0.10f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.50f, 0.15f, 1.00f);

        // Tabs
        colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.14f, 0.10f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.45f, 0.38f, 0.10f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.35f, 0.30f, 0.10f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.07f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.14f, 0.10f, 1.00f);

        // Tables
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.16f, 0.10f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.35f, 0.30f, 0.15f, 1.00f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.25f, 0.20f, 0.10f, 1.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

        // Misc
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.95f, 0.80f, 0.10f, 0.25f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.85f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.95f, 0.80f, 0.10f, 1.00f);

#ifdef IMGUI_HAS_DOCK
        colors[ImGuiCol_DockingPreview] = ImVec4(0.95f, 0.80f, 0.10f, 0.40f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.07f, 0.07f, 0.06f, 1.00f);
#endif

        style.WindowPadding = ImVec2{ 5.0f, 5.0f };
        style.FramePadding = ImVec2{ 4.0f, 4.0f };
        style.ItemSpacing = ImVec2{ 4.0f, 3.0f };
        style.ItemInnerSpacing = ImVec2{ 5.0f, 5.0f };
        style.CellPadding = ImVec2{ 2.0f, 4.0f };
        style.TouchExtraPadding = ImVec2{ 0.0f, 0.0f };
        style.WindowTitleAlign = ImVec2 { 0.5f, 0.5f };
        style.WindowBorderHoverPadding = 10.0f;
        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;

        style.IndentSpacing = 8;
        style.ScrollbarSize = 14.0f;
        style.GrabMinSize = 14.0f;
        style.FrameBorderSize = 1.0f;
        style.WindowRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.TabBorderSize = 1.0f;
        style.TabBarBorderSize = 1.0f;
        style.TabBarOverlineSize = 2.0f;
        style.TabCloseButtonMinWidthSelected = 1.0f;
        style.TabCloseButtonMinWidthUnselected = 1.0f;
        style.DockingSeparatorSize = 1.0f;

        style.WindowMenuButtonPosition = ImGuiDir_Right;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.AntiAliasedFill = true;
        style.AntiAliasedLines = true;
        style.AntiAliasedLinesUseTex = true;

        // Store the original style for proper scaling
        m_OriginalStyle = style;

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoAutoMerge = false;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.ConfigViewportsNoDecoration = false;
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        switch (Renderer::GetGraphicsAPI())
        {
            case nvrhi::GraphicsAPI::VULKAN:
            {
				io.BackendRendererName = "NVRHI Vulkan";
                ImGui_ImplSDL3_InitForVulkan(Application::GetInstance()->GetWindow()->GetWindowHandle());

                break;
            }
            case nvrhi::GraphicsAPI::D3D12:
            {
#ifdef PLATFORM_WINDOWS
				io.BackendRendererName = "NVRHI DirectX12";

                DeviceManager_DX12 &d3d12 = DeviceManager_DX12::GetInstance();
                ImGui_ImplDX12_InitInfo initInfo = {};
                initInfo.Device = d3d12.m_Device12;
                initInfo.CommandQueue = d3d12.m_GraphicsQueue;
                initInfo.NumFramesInFlight = m_DeviceManager->GetDeviceParameters().maxFramesInFlight;
                initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
                initInfo.SrvDescriptorHeap = d3d12.m_SrvDescHeap;
                initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle)
                {
                    return DeviceManager_DX12::GetInstance().m_SrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle);
                };
                initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
                {
                    return DeviceManager_DX12::GetInstance().m_SrvDescHeapAlloc.Free(cpu_handle, gpu_handle);
                };
				ImGui_ImplSDL3_InitForD3D(Application::GetInstance()->GetWindow()->GetWindowHandle());
                // ImGui_ImplDX12_Init(&initInfo);
                break;
#endif
            }
        }
    }

    void ImGuiLayer::OnAttach()
    {
        imguiNVRHI = CreateScope<ImGui_NVRHI>();
        imguiNVRHI->Init(m_DeviceManager->GetDevice());
    }

    void ImGuiLayer::OnEvent(Event &event)
    {
		if (m_BlockEvents)
		{
            // ImGuiIO &io = ImGui::GetIO();
            // event.Handled |= event.IsInCategory(EventCategoryMouse) && io.WantCaptureMouse;
            // event.Handled |= event.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard;
		}

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<FramebufferResizeEvent>(BIND_CLASS_EVENT_FN(ImGuiLayer::OnFramebufferResize));
        dispatcher.Dispatch<WindowDPIScaleChangedEvent>(BIND_CLASS_EVENT_FN(ImGuiLayer::OnDPIScaleChanged));
    }

    bool ImGuiLayer::OnFramebufferResize(FramebufferResizeEvent &event) const
    {
        if (imguiNVRHI)
            imguiNVRHI->BackBufferResizing();

        if (!m_SupportExplicitDisplayScaling)
            return false;

        // Font and style will be updated in the next BeginFrame() call
        // This just clears the current font texture
        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->Clear();
        io.Fonts->TexID = 0;

        m_Font->ReleaseScaledFont();

        return false;
    }

    bool ImGuiLayer::OnDPIScaleChanged(WindowDPIScaleChangedEvent &event)
    {
        if (!m_SupportExplicitDisplayScaling)
            return false;

        // Force immediate font and style update
        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->Clear();
        io.Fonts->TexID = 0;

        m_Font->ReleaseScaledFont();
        m_Font->CreateScaledFont(event.GetScaleX());

        // Reset style to original and apply new scaling
        ImGui::GetStyle() = m_OriginalStyle;
        ImGui::GetStyle().ScaleAllSizes(event.GetScaleX());
        m_CurrentDPIScale = event.GetScaleX();

        LOG_INFO("ImGui DPI scaling updated to: {}", event.GetScaleX());

        return false;
    }

    void ImGuiLayer::BeginFrame()
    {
        IGN_PROFILE_FUNCTION();

        if (!imguiNVRHI || m_BeginFrameCalled)
            return;

        float scaleX, scaleY;
        m_DeviceManager->GetDPIScaleInfo(scaleX, scaleY);

        // Check if DPI has changed and recreate font if necessary
        if (m_DeviceManager->IsUpdateDPIScaleFactor() || m_CurrentDPIScale != scaleX)
        {
            if (m_Font->GetScaledFont())
            {
                // Clear existing font and recreate with new scale
                ImGuiIO &io = ImGui::GetIO();
                io.Fonts->Clear();
                io.Fonts->TexID = 0;
                
                m_Font->ReleaseScaledFont();
            }
            
            // Always recreate font with new scale
            m_Font->CreateScaledFont(m_SupportExplicitDisplayScaling ? scaleX : 1.0f);
            
            // Reset style to original and apply new scaling
            if (m_SupportExplicitDisplayScaling)
            {
                ImGui::GetStyle() = m_OriginalStyle;
                ImGui::GetStyle().ScaleAllSizes(scaleX);
                m_CurrentDPIScale = scaleX;
            }
        }

        if (!m_Font->GetScaledFont())
        {
            m_Font->CreateScaledFont(m_SupportExplicitDisplayScaling ? scaleX : 1.0f);
            if (m_SupportExplicitDisplayScaling)
            {
                m_CurrentDPIScale = scaleX;
            }
        }

        imguiNVRHI->UpdateFontTexture();

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        m_BeginFrameCalled = true;
    }

    void ImGuiLayer::EndFrame(nvrhi::IFramebuffer* framebuffer)
    {
        IGN_PROFILE_FUNCTION();

        {
            IGN_PROFILE_SCOPE("ImGuiLayer::ImGuiRender");
            ImGui::Render();
        }

        {
            IGN_PROFILE_SCOPE("ImGuiLayer::NVRHIRender");
            imguiNVRHI->Render(framebuffer);
        }

        m_BeginFrameCalled = false;
    }

    void ImGuiLayer::RenderPlatformWindows()
    {
		IGN_PROFILE_FUNCTION();

        ImGuiIO &io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void ImGuiLayer::PollEvent(const SDL_Event& event)
    {
        SDL_Event patchedEvent = event;

        if ((patchedEvent.type == SDL_EVENT_MOUSE_MOTION && patchedEvent.motion.windowID == 0)
            || (patchedEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN && patchedEvent.button.windowID == 0)
            || (patchedEvent.type == SDL_EVENT_MOUSE_BUTTON_UP && patchedEvent.button.windowID == 0)
            || (patchedEvent.type == SDL_EVENT_MOUSE_WHEEL && patchedEvent.wheel.windowID == 0))
        {
            if (SDL_Window *mouseFocus = SDL_GetMouseFocus())
            {
                const SDL_WindowID focusedWindowId = SDL_GetWindowID(mouseFocus);
                if (patchedEvent.type == SDL_EVENT_MOUSE_MOTION)
                    patchedEvent.motion.windowID = focusedWindowId;
                else if (patchedEvent.type == SDL_EVENT_MOUSE_WHEEL)
                    patchedEvent.wheel.windowID = focusedWindowId;
                else
                    patchedEvent.button.windowID = focusedWindowId;
            }
        }

        if (patchedEvent.type == SDL_EVENT_WINDOW_FOCUS_LOST)
        {
            const SDL_MouseButtonFlags pressedButtons = SDL_GetMouseState(nullptr, nullptr);
            if (pressedButtons == 0)
            {
                ImGuiIO &io = ImGui::GetIO();
                for (int button = 0; button < 5; ++button)
                {
                    io.AddMouseButtonEvent(button, false);
                }
            }
        }

        ImGui_ImplSDL3_ProcessEvent(&patchedEvent);
    }

    void ImGuiLayer::OnDetach()
    {
        imguiNVRHI->Shutdown();

        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
}
