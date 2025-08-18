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

#pragma once

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/input/event.hpp"

#include "ignite/core/types.hpp"

#include <glm/glm.hpp>
#include <list>

namespace ignite
{
    class Window
    {
    public:
        explicit Window(const char *windowTitle, const DeviceCreationParameters &createInfo, nvrhi::GraphicsAPI graphicsApi);

        [[nodiscard]] GLFWwindow *GetWindowHandle() const { return m_DeviceManager->m_Window; }
        [[nodiscard]] bool IsLooping() const { return glfwWindowShouldClose(m_DeviceManager->m_Window) == 0; };

        void PollEvents();
        void Destroy();

        std::string &GetTitle() { return m_WindowTitle; }
        void SetEventCallback(const std::function<void(Event&)>& callback);
        [[nodiscard]] DeviceManager *GetDeviceManager() const { return m_DeviceManager; }

        [[nodiscard]] bool IsVisible() const { return m_DeviceManager->m_WindowVisible; }
        [[nodiscard]] bool IsInFocus() const { return m_DeviceManager->m_WindowIsInFocus; }

        void SetTitle(const std::string &title) const;
        void SetIcon(const std::string &filepath);

        void Iconify() const;
        void Maximize() const;
        void Restore() const;

        void Show();
        void Hide();

        glm::vec2 GetPosition();
        glm::vec2 GetFramebufferSize();

    private:
        void SetCallbacks() const;
        DeviceManager *m_DeviceManager;
        std::string m_WindowTitle;
        
        std::function<void(Event&)> m_Callback;

        friend class JoystickManager;
    };
}
