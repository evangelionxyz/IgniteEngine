// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "imgui_layer.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/graphics/window.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/core/vfs/vfs.hpp"
#include "ignite/core/platform_utils.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <imgui_internal.h>
#include "ignite/graphics/gizmo/gizmo.hpp"

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
#include "ignite/imgui/ui/ui_theme.hpp"

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

	GuiFont::GuiFont(Buffer buffer, bool isCompressed, float size)
        : m_Buffer(std::move(buffer))
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

        if (m_Buffer)
        {
            fontConfig.FontDataOwnedByAtlas = false;
            m_ImFont = m_IsCompressed
                ? io.Fonts->AddFontFromMemoryCompressedTTF(m_Buffer.Data(), static_cast<int>(m_Buffer.Size()), 0.0f, &fontConfig)
                : io.Fonts->AddFontFromMemoryTTF((void *)m_Buffer.Data(), static_cast<int>(m_Buffer.Size()), 0.0f, &fontConfig);
        }
        else if (m_IsDefault)
        {
            m_ImFont = io.Fonts->AddFontDefault(&fontConfig);
        }

        if (m_ImFont)
        {
            io.Fonts->TexRef = ImTextureRef((ImTextureID)0);
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

        UI::FontRegistry::Get().InitFromVFS();

        ImGuiStyle &style = ImGui::GetStyle();
        UI::ApplyShadcnDarkTheme(style);

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
            case nri::GraphicsAPI::VK:
            {
                ImGui_ImplSDL3_InitForVulkan(Application::GetInstance()->GetWindow()->GetWindowHandle());

                break;
            }
            case nri::GraphicsAPI::D3D12:
            {
#ifdef PLATFORM_WINDOWS
                DeviceManager_DX12 &d3d12 = DeviceManager_DX12::GetInstance();
                ImGui_ImplDX12_InitInfo initInfo = {};
                initInfo.Device = d3d12.m_Device12.Get();
                initInfo.CommandQueue = d3d12.m_GraphicsQueue.Get();
                initInfo.NumFramesInFlight = (int)m_DeviceManager->GetDeviceParameters().maxFramesInFlight;
                initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
                initInfo.SrvDescriptorHeap = d3d12.m_SrvDescHeap.Get();
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
        imguiNRI = CreateScope<ImGui_NRI>();
        imguiNRI->Init(m_DeviceManager->GetDevice());
    }

    bool ImGuiLayer::WantCaptureMouse() const
    {
        if (m_BlockEvents)
            return false;

        ImGuiContext *g = ImGui::GetCurrentContext();
        if (!g)
            return false;

        // 1. If an ImGui window is currently being held and dragged (moving window)
        if (g->MovingWindow != nullptr)
        {
            ImGuiWindow *movingRoot = g->MovingWindow->RootWindow ? g->MovingWindow->RootWindow : g->MovingWindow;
            if (movingRoot && strcmp(movingRoot->Name, "##main_dockspace") != 0)
                return true;
        }

        // 2. If an item in an ImGui window is currently active (e.g. dragging a slider, scrollbar, button, etc.)
        if (g->ActiveId != 0 && g->ActiveIdWindow != nullptr)
        {
            ImGuiWindow *activeRoot = g->ActiveIdWindow->RootWindow ? g->ActiveIdWindow->RootWindow : g->ActiveIdWindow;
            if (activeRoot && strcmp(activeRoot->Name, "##main_dockspace") != 0)
                return true;
        }

        // 3. If any ImGui window (other than the background gizmo dockspace) is hovered
        if (g->HoveredWindow != nullptr)
        {
            ImGuiWindow *hoveredRoot = g->HoveredWindow->RootWindow ? g->HoveredWindow->RootWindow : g->HoveredWindow;
            if (hoveredRoot && strcmp(hoveredRoot->Name, "##main_dockspace") != 0)
                return true;
        }

        // 4. On the gizmo dockspace window, pass input through to editor camera unless native Gizmo is actively being manipulated
        if (Gizmo::IsAnyManipulating())
            return true;

        return false;
    }

    bool ImGuiLayer::WantCaptureKeyboard() const
    {
        if (m_BlockEvents)
            return false;

        ImGuiContext *g = ImGui::GetCurrentContext();
        if (!g)
            return false;

        ImGuiIO &io = ImGui::GetIO();
        if (io.WantTextInput)
            return true;

        if (g->ActiveId != 0 && g->ActiveIdWindow != nullptr)
        {
            ImGuiWindow *activeRoot = g->ActiveIdWindow->RootWindow ? g->ActiveIdWindow->RootWindow : g->ActiveIdWindow;
            if (activeRoot && strcmp(activeRoot->Name, "##main_dockspace") != 0)
                return io.WantCaptureKeyboard;
        }

        return false;
    }

    void ImGuiLayer::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<FramebufferResizeEvent>(BIND_CLASS_EVENT_FN(ImGuiLayer::OnFramebufferResize));
        dispatcher.Dispatch<WindowDPIScaleChangedEvent>(BIND_CLASS_EVENT_FN(ImGuiLayer::OnDPIScaleChanged));
    }

    bool ImGuiLayer::OnFramebufferResize(FramebufferResizeEvent &event) const
    {
        if (imguiNRI)
            imguiNRI->BackBufferResizing();

        if (!m_SupportExplicitDisplayScaling)
            return false;

        // Font and style will be updated in the next BeginFrame() call
        // This just clears the current font texture
        ImGuiIO &io = ImGui::GetIO();
        io.Fonts->Clear();
        io.Fonts->TexRef = ImTextureRef((ImTextureID)0);

        UI::FontRegistry::Get().ReleaseScaledFonts();
        if (m_Font)
            m_Font->ReleaseScaledFont();

        return false;
    }

    bool ImGuiLayer::OnDPIScaleChanged(WindowDPIScaleChangedEvent &event)
    {
        if (!m_SupportExplicitDisplayScaling)
            return false;

        UI::FontRegistry::Get().RebuildAtlas(event.GetScaleX());

        // Reset style to original and apply new scaling
        ImGui::GetStyle() = m_OriginalStyle;
        ImGui::GetStyle().ScaleAllSizes(event.GetScaleX());
        m_CurrentDPIScale = event.GetScaleX();
        m_FontsBuilt = true;

        LOG_INFO("ImGui DPI scaling updated to: {}", event.GetScaleX());

        return false;
    }

    void ImGuiLayer::BeginFrame()
    {
        IGN_PROFILE_FUNCTION();

        if (!imguiNRI || m_BeginFrameCalled)
            return;

        float scaleX, scaleY;
        m_DeviceManager->GetDPIScaleInfo(scaleX, scaleY);

        const float targetScale = m_SupportExplicitDisplayScaling ? scaleX : 1.0f;

        // Check if DPI has changed or font atlas needs rebuilding
        if (!m_FontsBuilt || m_DeviceManager->IsUpdateDPIScaleFactor() || m_CurrentDPIScale != scaleX || UI::FontRegistry::Get().GetFont(UI::FontVariant::Regular) == nullptr)
        {
            UI::FontRegistry::Get().RebuildAtlas(targetScale);

            if (m_SupportExplicitDisplayScaling)
            {
                ImGui::GetStyle() = m_OriginalStyle;
                ImGui::GetStyle().ScaleAllSizes(scaleX);
                m_CurrentDPIScale = scaleX;
            }
            m_FontsBuilt = true;
        }

        imguiNRI->UpdateFontTexture();

        {
            const CursorMode cursorMode = InputSystem::GetCursorMode();
            const bool cursorHiddenOrLocked = (cursorMode == CursorMode::Disabled || cursorMode == CursorMode::Hidden);
            const bool cursorLocked = (cursorMode == CursorMode::Disabled);

            ImGuiIO &io = ImGui::GetIO();
            if (cursorHiddenOrLocked)
                io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            else
                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;

            if (cursorLocked)
            {
                if (!(io.ConfigFlags & ImGuiConfigFlags_NoMouse))
                {
                    for (int button = 0; button < 5; ++button)
                        io.AddMouseButtonEvent(button, false);
                    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
                }
                io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
            }
            else
            {
                io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            }
        }

        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        m_BeginFrameCalled = true;
    }

    void ImGuiLayer::EndFrame(RenderTarget* framebuffer, bool clearTarget)
    {
        IGN_PROFILE_FUNCTION();

        {
            IGN_PROFILE_SCOPE("ImGuiLayer::ImGuiRender");
            ImGui::Render();
        }

        {
            IGN_PROFILE_SCOPE("ImGuiLayer::NRIRender");
            imguiNRI->Render(framebuffer, clearTarget);
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

        const bool cursorLocked = (InputSystem::GetCursorMode() == CursorMode::Disabled);
        if (m_BlockEvents || cursorLocked)
        {
            const bool isMouseEvent = (patchedEvent.type == SDL_EVENT_MOUSE_MOTION
                || patchedEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                || patchedEvent.type == SDL_EVENT_MOUSE_BUTTON_UP
                || patchedEvent.type == SDL_EVENT_MOUSE_WHEEL);
            if (isMouseEvent)
                return;

            if (m_BlockEvents)
            {
                const bool isKeyEvent = (patchedEvent.type == SDL_EVENT_KEY_DOWN
                    || patchedEvent.type == SDL_EVENT_KEY_UP
                    || patchedEvent.type == SDL_EVENT_TEXT_INPUT);
                if (isKeyEvent)
                    return;
            }
        }

        ImGui_ImplSDL3_ProcessEvent(&patchedEvent);
    }

    void ImGuiLayer::OnDetach()
    {
        UI::ClearAnimationState();
        UI::FontRegistry::Get().Shutdown();

        imguiNRI->Shutdown();

        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
}
