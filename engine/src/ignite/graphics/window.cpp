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

#include "window.hpp"
#include "stb_image.h"
#include "ignite/core/logger.hpp"
#include "ignite/core/input/app_event.hpp"
#include "ignite/core/input/event.hpp"
#include "ignite/core/input/key_event.hpp"
#include "ignite/core/input/mouse_event.hpp"
#include "ignite/core/input/joystick_event.hpp"

#ifdef _WIN32
    #include <dwmapi.h>
    #include <ShellScalingApi.h>
    #pragma comment(lib, "Dwmapi.lib") // Link to DWM API
    #pragma comment(lib, "shcore.lib")
#endif

#include <ignite/core/input/joystick_codes.hpp>

namespace ignite
{
    static const struct
    {
        nvrhi::Format format;
        u32 redBits;
        u32 greenBits;
        u32 blueBits;
        u32 alphaBits;
        u32 depthBits;
        u32 stencilBits;
    } formatInfo[] =
    {
        { nvrhi::Format::UNKNOWN,            0,  0,  0,  0,  0,  0, },
        { nvrhi::Format::R8_UINT,            8,  0,  0,  0,  0,  0, },
        { nvrhi::Format::RG8_UINT,           8,  8,  0,  0,  0,  0, },
        { nvrhi::Format::RG8_UNORM,          8,  8,  0,  0,  0,  0, },
        { nvrhi::Format::R16_UINT,          16,  0,  0,  0,  0,  0, },
        { nvrhi::Format::R16_UNORM,         16,  0,  0,  0,  0,  0, },
        { nvrhi::Format::R16_FLOAT,         16,  0,  0,  0,  0,  0, },
        { nvrhi::Format::RGBA8_UNORM,        8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::RGBA8_SNORM,        8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::BGRA8_UNORM,        8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::SRGBA8_UNORM,       8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::SBGRA8_UNORM,       8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::R10G10B10A2_UNORM, 10, 10, 10,  2,  0,  0, },
        { nvrhi::Format::R11G11B10_FLOAT,   11, 11, 10,  0,  0,  0, },
        { nvrhi::Format::RG16_UINT,         16, 16,  0,  0,  0,  0, },
        { nvrhi::Format::RG16_FLOAT,        16, 16,  0,  0,  0,  0, },
        { nvrhi::Format::R32_UINT,          32,  0,  0,  0,  0,  0, },
        { nvrhi::Format::R32_FLOAT,         32,  0,  0,  0,  0,  0, },
        { nvrhi::Format::RGBA16_FLOAT,      16, 16, 16, 16,  0,  0, },
        { nvrhi::Format::RGBA16_UNORM,      16, 16, 16, 16,  0,  0, },
        { nvrhi::Format::RGBA16_SNORM,      16, 16, 16, 16,  0,  0, },
        { nvrhi::Format::RG32_UINT,         32, 32,  0,  0,  0,  0, },
        { nvrhi::Format::RG32_FLOAT,        32, 32,  0,  0,  0,  0, },
        { nvrhi::Format::RGB32_UINT,        32, 32, 32,  0,  0,  0, },
        { nvrhi::Format::RGB32_FLOAT,       32, 32, 32,  0,  0,  0, },
        { nvrhi::Format::RGBA32_UINT,       32, 32, 32, 32,  0,  0, },
        { nvrhi::Format::RGBA32_FLOAT,      32, 32, 32, 32,  0,  0, }
    };

    static void GLFW_ErrorCallback(i32 error, const char *description)
    {
        LOG_ERROR("GLFW error: {}", description);
        exit(1);
    }

    Window::Window(const char *windowTitle, const DeviceCreationParameters &deviceParams, nvrhi::GraphicsAPI graphicsApi)
        : m_WindowTitle(windowTitle)
    {
        m_DeviceManager = DeviceManager::Create(graphicsApi);

        m_DeviceManager->m_DeviceParams = deviceParams;
#ifdef _DEBUG
        m_DeviceManager->m_DeviceParams.enableDebugRuntime = true;
        m_DeviceManager->m_DeviceParams.enableNvrhiValidationLayer= true;
#endif

        // Create device instance
        bool result = m_DeviceManager->CreateInstance(m_DeviceManager->m_DeviceParams);
        LOG_ASSERT(result, "Failed to create Instance");

        m_DeviceManager->m_DeviceParams.headlessDevice = false;

        result = false;
        for (const auto &info : formatInfo)
        {
            if (info.format == m_DeviceManager->m_DeviceParams.swapChainFormat)
            {
                result = true;
                break;
            }
        }
        LOG_ASSERT(result, "SDL3 format not found\n");

        // glfwWindowHint(GLFW_SAMPLES, m_DeviceManager->m_DeviceParams.swapChainSampleCount);
        // glfwWindowHint(GLFW_REFRESH_RATE, m_DeviceManager->m_DeviceParams.refreshRate);
        // glfwWindowHint(GLFW_SCALE_TO_MONITOR, m_DeviceManager->m_DeviceParams.resizeWindowWithDisplayScale);
        // glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // ignored for full screen
        // glfwWindowHint(GLFW_DECORATED, !m_DeviceManager->m_DeviceParams.startBorderless); // borderless window

        SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;

        switch (graphicsApi)
        {
        case nvrhi::GraphicsAPI::VULKAN:
			windowFlags |= SDL_WINDOW_VULKAN;
            break;
        }

        m_DeviceManager->m_Window = SDL_CreateWindow(
            windowTitle,
            m_DeviceManager->m_DeviceParams.backBufferWidth,
            m_DeviceManager->m_DeviceParams.backBufferHeight,
            windowFlags
        );
        LOG_ASSERT(m_DeviceManager->m_Window, "Failed to create GLFW window\n");

        SDL_SetWindowSurfaceVSync(m_DeviceManager->m_Window, m_DeviceManager->m_DeviceParams.vsyncEnable);

        if (m_DeviceManager->m_DeviceParams.startMaximized)
        {
			SDL_MaximizeWindow(m_DeviceManager->m_Window);
        }

        if (m_DeviceManager->m_DeviceParams.startFullscreen)
        {
			SDL_SetWindowFullscreen(m_DeviceManager->m_Window, SDL_WINDOW_FULLSCREEN);
        }
        else
        {
            i32 fbWidth = 0, fbHeight = 0;
			SDL_GetWindowSize(m_DeviceManager->m_Window, &fbWidth, &fbHeight);
            m_DeviceManager->m_DeviceParams.backBufferWidth = fbWidth;
            m_DeviceManager->m_DeviceParams.backBufferHeight = fbHeight;
        }


        if (m_DeviceManager->m_DeviceParams.windowPosX != -1 && m_DeviceManager->m_DeviceParams.windowPosY != -1)
        {
			SDL_SetWindowPosition(m_DeviceManager->m_Window, m_DeviceManager->m_DeviceParams.windowPosX, m_DeviceManager->m_DeviceParams.windowPosY);
        }

#if PLATFORM_WINDOWS
		HWND hwnd = m_DeviceManager->GetNativeWindow();
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

        // 7160E8 visual studio purple
        COLORREF rgbRed = 0x00E86071;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));

        // DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUNDSMALL;
        // DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
#endif

        result = m_DeviceManager->CreateDevice();
        LOG_ASSERT(result, "Failed to create Device Instance\n");

        result = m_DeviceManager->CreateSwapChain();
        LOG_ASSERT(result, "Failed to create Swap Chain\n");

        m_DeviceManager->m_WindowVisible = true;
        m_DeviceManager->m_WindowIsInFocus = true;

        m_DeviceManager->CreateBackBuffers();

        if (m_DeviceManager->m_DeviceParams.enablePerMonitorDPI)
        {
#ifdef PLATFORM_WINDOWS
			HWND hwnd = m_DeviceManager->GetNativeWindow();
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            u32 dpiX, dpiY;
            GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
            m_DeviceManager->m_DPIScaleFactorX = dpiX / 96.f;
            m_DeviceManager->m_DPIScaleFactorY = dpiY / 96.f;
#else
            GLFWmonitor *monitor = glfwGetWindowMonitor(window);
            if (!monitor)
                monitor = glfwGetPrimaryMonitor();
            glfwGetMonitorContentScale(monitor, &m_DeviceManager->m_DPIScaleFactorX, &m_DeviceManager->m_DPIScaleFactorY);
#endif
        }

        JoystickManager::Init(this);
    }

    void Window::PollEvents(const SDL_Event &event)
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        {
			m_Looping = false;
            break;
        }

        case SDL_EVENT_WINDOW_RESIZED:
        {
            WindowResizeEvent e(event.window.data1, event.window.data2);
            m_Callback(e);

            if (event.window.data1 == 0 || event.window.data2 == 0)
            {
                m_DeviceManager->m_WindowVisible = false;
                break;
            }

			SDL_WindowFlags flags = SDL_GetWindowFlags(m_DeviceManager->m_Window);
            // m_DeviceManager->m_WindowIsInFocus = (flags & SDL_WINDOW_INPUT_FOCUS) == 0;
            m_DeviceManager->m_WindowVisible = true;

            m_DeviceManager->m_DeviceParams.windowPosX = event.window.data1;
            m_DeviceManager->m_DeviceParams.windowPosY = event.window.data2;
            break;
        }
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            FramebufferResizeEvent e(event.window.data1, event.window.data2);
            m_Callback(e);

            // window is not minimized, and the size has changed
            if (event.window.data1 > 0 && event.window.data2 > 0)
            {
                m_DeviceManager->m_DeviceParams.backBufferWidth = event.window.data1;
                m_DeviceManager->m_DeviceParams.backBufferHeight = event.window.data2;

                m_DeviceManager->ResizeSwapChain();
                m_DeviceManager->CreateBackBuffers();
            }
            break;
        }
        case SDL_EVENT_WINDOW_MAXIMIZED:
        {
            WindowMaximizedEvent e(true);
            m_Callback(e);
			break;
        }
        case SDL_EVENT_WINDOW_MINIMIZED:
        {
            WindowMinimizedEvent e(true);
            m_Callback(e);
            break;
        }
        case SDL_EVENT_WINDOW_RESTORED:
        {
            WindowMinimizedEvent event(false);
            m_Callback(event);
            break;
		}
        case SDL_EVENT_WINDOW_DESTROYED:
        {
            WindowCloseEvent e;
            m_Callback(e);
            break;
        }
        case SDL_EVENT_JOYSTICK_ADDED:
        {
            SDL_JoystickID jID = event.jdevice.which;
			JoystickManager::ConnectJoystick(jID);

            break;
        }
        case SDL_EVENT_JOYSTICK_REMOVED:
        {
			SDL_JoystickID jID = event.jdevice.which;
            JoystickManager::DisconnectJoystick(jID);
			break;
        }
        case SDL_EVENT_TEXT_INPUT:
        {
			/*KeyTypedEvent e(std::string(event.text.text));
			m_Callback(e);*/
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        {
            if (event.key.repeat)
            {
                KeyPressedEvent e(event.key.key, 1);
                m_Callback(e);
            }
            else
            {
                KeyPressedEvent e(event.key.key, 0);
                m_Callback(e);
            }
            break;
        }
        case SDL_EVENT_KEY_UP:
        {
            KeyReleasedEvent e(event.key.key);
            m_Callback(e);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            MouseButtonPressedEvent e(event.button.button);
            m_Callback(e);
			break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            MouseButtonReleasedEvent e(event.button.button);
            m_Callback(e);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            MouseScrolledEvent e(static_cast<float>(event.wheel.x), static_cast<float>(event.wheel.y));
            m_Callback(e);
            break;
		}
        case SDL_EVENT_MOUSE_MOTION:
        {
            MouseMovedEvent e(static_cast<float>(event.motion.x), static_cast<float>(event.motion.y));
            m_Callback(e);
            break;
		}
        case SDL_EVENT_DROP_FILE:
        {
			// TODO: implement multiple file drop
#if 0
            std::vector<std::filesystem::path> filepaths(event.drop.reserved);

            LOG_INFO("Paths: ");
            for (uint32_t i = 0; i < static_cast<uint32_t>(filepaths.size()); i++)
            {
                filepaths[i] = std::filesystem::path(std::string(event.drop.data));
                LOG_INFO(" {}", filepaths[i].generic_string().c_str());
            }

            WindowDropEvent e(std::move(filepaths));
            m_Callback(e);
#endif
            break;
        }
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        {
            if (m_DeviceManager->m_DeviceParams.enablePerMonitorDPI)
            {
                WindowDPIScaleChangedEvent e(static_cast<float>(event.display.data1), static_cast<float>(event.display.data2));
                m_Callback(e);
#ifdef _WIN32
				HWND hwnd = m_DeviceManager->GetNativeWindow();
                HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                u32 dpiX, dpiY;
                GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
                m_DeviceManager->m_DPIScaleFactorX = dpiX / 96.f;
                m_DeviceManager->m_DPIScaleFactorY = dpiY / 96.f;
#else
                GLFWmonitor* monitor = glfwGetWindowMonitor(window);
                if (!monitor) monitor = glfwGetPrimaryMonitor();
                glfwGetMonitorContentScale(monitor, &win.m_DeviceManager->m_DPIScaleFactorX, &win.m_DeviceManager->m_DPIScaleFactorY);
#endif
            }

            break;
        }
            
        }

        for (const Ref<Joystick>& j : JoystickManager::GetConnectedJoystick())
        {
            j->Update();
        }
    }

    void Window::Destroy()
    {
        if (m_DeviceManager->m_Window)
        {
			SDL_DestroyWindow(m_DeviceManager->m_Window);
            m_DeviceManager->m_Window = nullptr;
        }

		SDL_Quit();
    }

    void Window::SetTitle(const std::string &title) const
    {
		SDL_SetWindowTitle(m_DeviceManager->m_Window, title.c_str());
    }

    void Window::SetIcon(const std::string &filepath)
    {
        int width, height, channels;
        uint8_t *pixels = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

        if (pixels)
        {
            SDL_Surface *iconSurface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
            SDL_SetWindowIcon(m_DeviceManager->m_Window, nullptr);
        }
        else
        {
            LOG_ERROR("Window icon file not found: {}", filepath);
        }
    }

    void Window::Minimize() const
    {
		SDL_MinimizeWindow(m_DeviceManager->m_Window);
    }

    void Window::Maximize() const
    {
		SDL_MaximizeWindow(m_DeviceManager->m_Window);
    }

    void Window::Restore() const
    {
		SDL_RestoreWindow(m_DeviceManager->m_Window);
    }
    
    void Window::Shutdown()
    {
		m_Looping = false;
    }

    void Window::Show()
    {
        if (!m_DeviceManager->m_DeviceParams.startMaximized)
        {
			int width, height;
			SDL_GetWindowSize(m_DeviceManager->m_Window, &width, &height);

#if 0
            int displayCount;
            SDL_DisplayID *displayId = SDL_GetDisplays(&displayCount);
#endif
            SDL_Rect rect;
			SDL_GetDisplayBounds(0, &rect);
            
            SDL_SetWindowPosition(m_DeviceManager->m_Window,
                rect.w / 2 - width / 2,
                rect.h / 2 - height / 2
            );
        }

		m_DeviceManager->m_WindowVisible = true;
		SDL_ShowWindow(m_DeviceManager->m_Window);
    }

    void Window::Hide()
    {
		m_DeviceManager->m_WindowVisible = false;
		SDL_HideWindow(m_DeviceManager->m_Window);
    }

    glm::vec2 Window::GetPosition()
    {
        int xPos, yPos;
		SDL_GetWindowPosition(m_DeviceManager->m_Window, &xPos, &yPos);
        return { static_cast<float>(xPos), static_cast<float>(yPos) };
    }

    glm::vec2 Window::GetFramebufferSize()
    {
        float width = static_cast<float>(m_DeviceManager->m_DeviceParams.backBufferWidth);
        float height = static_cast<float>(m_DeviceManager->m_DeviceParams.backBufferHeight);
        return { width, height };
    }

    void Window::SetEventCallback(const std::function<void(Event &)> &callback)
    {
        m_Callback = callback;
    }
}
