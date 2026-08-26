// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_WINDOW_HPP
#define IGN_WINDOW_HPP

#include <SDL3/SDL.h>

#include "ignite/core/base.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/input/event.hpp"
#include <glm/glm.hpp>

namespace ignite
{
    class IGN_API Window
    {
    public:
        explicit Window(const char *windowTitle, const DeviceParameters &params, nvrhi::GraphicsAPI graphicsApi);

        void PollEvents(const SDL_Event &event);
        void Destroy();

        std::string &GetTitle() { return m_WindowTitle; }

        SDL_Window *GetWindowHandle() const { return m_Window; }
        DeviceManager *GetDeviceManager() const { return m_DeviceManager; }

        bool IsLooping() const { return m_Looping; };
        bool IsVisible() const { return m_IsVisible; }
        bool IsInFocus() const { return m_IsInFocus; }

        void SetEventCallback(const std::function<void(Event&)>& callback);
        void SetTitle(const std::string &title) const;
        void SetIcon(const std::string &filepath);

        void Minimize() const;
        void Maximize() const;
        void Restore() const;

        void Shutdown();

        void Show();
        void Hide();

        glm::ivec2 GetPosition();
        glm::ivec2 GetFramebufferSize();
		glm::ivec2 GetSize();

#ifdef PLATFORM_WINDOWS
		// void SetWindowsIcon(HICON icon) const;
		HWND GetNativeWindow() const;
#endif

    private:
		SDL_Window *m_Window = nullptr;
        DeviceManager *m_DeviceManager;
        std::string m_WindowTitle;
        std::function<void(Event&)> m_Callback;
        bool m_Looping = true;
        bool m_IsVisible = true;
		bool m_IsInFocus = true;

        friend class JoystickManager;
    };
}

#endif
