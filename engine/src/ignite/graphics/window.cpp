/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu
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

#include "ignite/core/input/input.hpp"

#include <SDL3/SDL_video.h>

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
    Window::Window(const char *windowTitle, const DeviceParameters &params, nvrhi::GraphicsAPI graphicsApi)
        : m_WindowTitle(windowTitle)
    {
        m_DeviceManager = DeviceManager::Create(this, params, graphicsApi);

		DeviceParameters& deviceParams = m_DeviceManager->GetDeviceParameters();

#ifdef _DEBUG
        deviceParams.enableDebugRuntime = true;
        deviceParams.enableNvrhiValidationLayer = true;
#endif

        // Create device instance
        bool result = m_DeviceManager->CreateInstance(params);
        LOG_ASSERT(result, "Failed to create Instance");

        m_DeviceManager->SetHeadLessDevice(false);

        result = false;
        for (const auto& info : formatInfo)
        {
            if (info.format == deviceParams.swapChainFormat)
            {
                result = true;
                break;
            }
        }
        LOG_ASSERT(result, "SDL3 format not found\n");

        SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
        if (graphicsApi == nvrhi::GraphicsAPI::VULKAN)
        {
		    windowFlags |= SDL_WINDOW_VULKAN;
        }

        m_Window = SDL_CreateWindow(windowTitle, params.windowWidth, params.windowHeight, windowFlags);
        LOG_ASSERT(m_Window, "Failed to create SDL3 window\n");

        if (params.startMaximized)
        {
            SDL_MaximizeWindow(m_Window);
        }

        if (params.startFullscreen)
        {
            SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN);
        }
        else
        {
            int width, height;
            SDL_GetWindowSize(m_Window, &width, &height);
            deviceParams.backBufferWidth = width;
            deviceParams.backBufferHeight = height;
        }

	    SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

#if PLATFORM_WINDOWS
		HWND hwnd = GetNativeWindow();
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

        m_DeviceManager->CreateBackBuffers();

        if (params.enablePerMonitorDPI)
        {
#ifdef PLATFORM_WINDOWS
			HWND hwnd = GetNativeWindow();
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            uint32_t dpiX, dpiY;
            GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
			m_DeviceManager->SetDPISacaleFactors(dpiX / 96.f, dpiY / 96.f);
#else
#endif
        }

        JoystickManager::Init(this);
    }

    void Window::PollEvents(const SDL_Event &event)
    {
		DeviceParameters &deviceParams = m_DeviceManager->GetDeviceParameters();
        const SDL_WindowID mainWindowId = SDL_GetWindowID(m_Window);
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        {
			m_Looping = false;
            break;
        }

        case SDL_EVENT_WINDOW_RESIZED:
        {
            if (event.window.windowID != mainWindowId)
                break;

            WindowResizeEvent e(event.window.data1, event.window.data2);
            m_Callback(e);

            if (event.window.data1 == 0 || event.window.data2 == 0)
            {
                m_IsVisible = false;
                break;
            }

			SDL_WindowFlags flags = SDL_GetWindowFlags(m_Window);
            // m_DeviceManager->m_WindowIsInFocus = (flags & SDL_WINDOW_INPUT_FOCUS) == 0;
            m_IsVisible = true;

            deviceParams.windowWidth = event.window.data1;
            deviceParams.windowHeight = event.window.data2;
            break;
        }
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
            if (event.window.windowID != mainWindowId)
                break;

            FramebufferResizeEvent e(event.window.data1, event.window.data2);
            m_Callback(e);

            // window is not minimized, and the size has changed
            if (event.window.data1 > 0 && event.window.data2 > 0)
            {
                deviceParams.backBufferWidth = event.window.data1;
                deviceParams.backBufferHeight = event.window.data2;

                m_DeviceManager->ResizeSwapChain();
                m_DeviceManager->CreateBackBuffers();
            }
            break;
        }
        case SDL_EVENT_WINDOW_MAXIMIZED:
        {
            if (event.window.windowID != mainWindowId)
                break;

            WindowMaximizedEvent e(true);
            m_Callback(e);
			break;
        }
        case SDL_EVENT_WINDOW_MINIMIZED:
        {
            if (event.window.windowID != mainWindowId)
                break;

            WindowMinimizedEvent e(true);
            m_Callback(e);
            break;
        }
        case SDL_EVENT_WINDOW_RESTORED:
        {
            if (event.window.windowID != mainWindowId)
                break;

            WindowMinimizedEvent event(false);
            m_Callback(event);
            break;
		}
        case SDL_EVENT_WINDOW_DESTROYED:
        {
            if (event.window.windowID != mainWindowId)
                break;

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
            if (event.text.windowID != mainWindowId)
                break;

			/*KeyTypedEvent e(std::string(event.text.text));
			m_Callback(e);*/
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        {
            if (event.key.windowID != mainWindowId)
                break;

            Input::SetModifier(KeyMod::Shift, event.key.mod & SDL_KMOD_SHIFT);
            Input::SetModifier(KeyMod::Control, event.key.mod & SDL_KMOD_CTRL);
            Input::SetModifier(KeyMod::LeftAlt, event.key.mod & SDL_KMOD_LALT);
            Input::SetModifier(KeyMod::RightAlt, event.key.mod & SDL_KMOD_RALT);
            Input::SetModifier(KeyMod::LeftShift, event.key.mod & SDL_KMOD_LSHIFT);
            Input::SetModifier(KeyMod::RightShift, event.key.mod & SDL_KMOD_RSHIFT);
            Input::SetModifier(KeyMod::LeftControl, event.key.mod & SDL_KMOD_LCTRL);
            Input::SetModifier(KeyMod::RightControl, event.key.mod & SDL_KMOD_RCTRL);

			Input::SetKey(event.key.key, true);

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
            if (event.key.windowID != mainWindowId)
                break;

            Input::SetModifier(KeyMod::Shift, event.key.mod& SDL_KMOD_SHIFT);
            Input::SetModifier(KeyMod::Control, event.key.mod& SDL_KMOD_CTRL);
            Input::SetModifier(KeyMod::LeftAlt, event.key.mod& SDL_KMOD_LALT);
            Input::SetModifier(KeyMod::RightAlt, event.key.mod& SDL_KMOD_RALT);
            Input::SetModifier(KeyMod::LeftShift, event.key.mod& SDL_KMOD_LSHIFT);
            Input::SetModifier(KeyMod::RightShift, event.key.mod& SDL_KMOD_RSHIFT);
            Input::SetModifier(KeyMod::LeftControl, event.key.mod& SDL_KMOD_LCTRL);
            Input::SetModifier(KeyMod::RightControl, event.key.mod& SDL_KMOD_RCTRL);

            Input::SetKey(event.key.key, false);

            KeyReleasedEvent e(event.key.key);
            m_Callback(e);
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
           if (event.button.windowID != mainWindowId)
                break;

			Input::SetMouseButton(event.button.button, true);
            MouseButtonPressedEvent e(event.button.button);
            m_Callback(e);
			break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            if (event.button.windowID != mainWindowId)
                break;

            Input::SetMouseButton(event.button.button, false);
            MouseButtonReleasedEvent e(event.button.button);
            m_Callback(e);
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            if (event.wheel.windowID != mainWindowId)
                break;

            MouseScrolledEvent e(event.wheel.x, event.wheel.y);
            m_Callback(e);
            break;
		}
        case SDL_EVENT_MOUSE_MOTION:
        {
          if (event.motion.windowID != mainWindowId)
                break;

			Input::SetMousePosition((int)event.motion.x, (int)event.motion.y);
            MouseMovedEvent e((int)event.motion.x, (int)event.motion.y);
            m_Callback(e);
            break;
		}
        case SDL_EVENT_DROP_FILE:
        {
			// TODO: implement multiple file drop
#if 0
            std::vector<ignite::Path> filepaths(event.drop.reserved);

            LOG_INFO("Paths: ");
            for (uint32_t i = 0; i < static_cast<uint32_t>(filepaths.size()); i++)
            {
                filepaths[i] = ignite::Path(std::string(event.drop.data));
                LOG_INFO(" {}", filepaths[i].generic_string().c_str());
            }

            WindowDropEvent e(std::move(filepaths));
            m_Callback(e);
#endif
            break;
        }
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        {
            if (deviceParams.enablePerMonitorDPI)
            {
                WindowDPIScaleChangedEvent e(static_cast<float>(event.display.data1), static_cast<float>(event.display.data2));
                m_Callback(e);
#ifdef _WIN32
				HWND hwnd = GetNativeWindow();
                HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                uint32_t dpiX, dpiY;
                GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
				m_DeviceManager->SetDPISacaleFactors(dpiX / 96.f, dpiY / 96.f);
#else
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
        if (m_Window)
        {
			SDL_DestroyWindow(m_Window);
            m_Window = nullptr;
        }

		SDL_Quit();
    }

    void Window::SetTitle(const std::string &title) const
    {
		SDL_SetWindowTitle(m_Window, title.c_str());
    }

    void Window::SetIcon(const std::string &filepath)
    {
        int width, height, channels;
        uint8_t *pixels = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

        if (pixels)
        {
            SDL_Surface *iconSurface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
            SDL_SetWindowIcon(m_Window, nullptr);
        }
        else
        {
            LOG_ERROR("Window icon file not found: {}", filepath);
        }
    }

    void Window::Minimize() const
    {
		SDL_MinimizeWindow(m_Window);
    }

    void Window::Maximize() const
    {
        SDL_MaximizeWindow(m_Window);
    }

    void Window::Restore() const
    {
		SDL_RestoreWindow(m_Window);
    }
    
    void Window::Shutdown()
    {
		m_Looping = false;
    }

    void Window::Show()
    {
        if (!m_DeviceManager->GetDeviceParameters().startMaximized)
        {
			int width, height;
			SDL_GetWindowSize(m_Window, &width, &height);

            SDL_Rect rect;
			SDL_GetDisplayBounds(0, &rect);
            SDL_SetWindowPosition(m_Window, rect.w / 2 - width / 2, rect.h / 2 - height / 2);
        }

		m_IsVisible = true;
		SDL_ShowWindow(m_Window);
    }

    void Window::Hide()
    {
		m_IsVisible = false;
		SDL_HideWindow(m_Window);
    }

    glm::ivec2 Window::GetPosition()
    {
        int x, y;
        SDL_GetWindowPosition(m_Window, &x, &y);
        return { x, y };
    }

    glm::ivec2 Window::GetFramebufferSize()
    {
		const auto &deviceParams = m_DeviceManager->GetDeviceParameters();
        return { deviceParams.backBufferWidth, deviceParams.backBufferHeight };
    }

    glm::ivec2 Window::GetSize()
    {
		int width, height;
		SDL_GetWindowSize(m_Window, &width, &height);
        return { width, height };
    }

    HWND Window::GetNativeWindow() const
    {
        // Retrieve HWND
        SDL_PropertiesID props = SDL_GetWindowProperties(m_Window);
        HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        return hwnd;
    }

    void Window::SetEventCallback(const std::function<void(Event &)> &callback)
    {
        m_Callback = callback;
    }
}
