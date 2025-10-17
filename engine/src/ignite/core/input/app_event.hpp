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

#include "event.hpp"

#include <vector>
#include <sstream>
#include <filesystem>

namespace ignite
{
    class WindowResizeEvent final : public Event
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height)
            : m_Width(width), m_Height(height) {}
        u32 GetWidth() const { return m_Width; }
        u32 GetHeight() const { return m_Height; }
        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowResizeEvent: " << m_Width << ", " << m_Height;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowResize);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);

    private:
        unsigned int m_Width, m_Height;
    };

    class WindowCloseEvent final : public Event
    {
    public:
        WindowCloseEvent() = default;
        [[nodiscard]] std::string ToString() const override
        {
            return "WindowCloseEvent: Window Closed!";
        }

        EVENT_CLASS_TYPE(WindowClose);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);
    };

    class FramebufferResizeEvent final : public Event
    {
    public:
        FramebufferResizeEvent(int width, int height)
            : m_Width(width), m_Height(height) {}
        [[nodiscard]] i32 GetWidth() const { return m_Width; }
        [[nodiscard]] i32 GetHeight() const { return m_Height; }
        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << "FramebufferResizeEvent: " << m_Width << ", " << m_Height;
            return ss.str();
        }
        EVENT_CLASS_TYPE(FramebufferResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication);
    private:
        int m_Width, m_Height;
    };

    class WindowDropEvent final : public Event
    {
    public:
        explicit WindowDropEvent(const std::vector<std::filesystem::path> &paths)
            : m_Paths(paths) {}
        explicit WindowDropEvent(std::vector <std::filesystem::path> &&paths)
            : m_Paths(std::move(paths)) {}

        [[nodiscard]] const std::vector<std::filesystem::path> &GetPaths() const { return m_Paths; }

        EVENT_CLASS_TYPE(WindowDrop);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);
    private:
        std::vector<std::filesystem::path> m_Paths;
    };

    class WindowMaximizedEvent final : public Event
    {
    public:
        explicit WindowMaximizedEvent(bool maximized)
            : m_Maximized(maximized) {}

        [[nodiscard]] std::string ToString() const override
        {
            return "WindowMaximizedEvent: " + m_Maximized ? "True" : "False";
        }

        [[nodiscard]] bool IsMaximized() const { return m_Maximized; }

        EVENT_CLASS_TYPE(WindowMaximized);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);

    private:
        bool m_Maximized = false;
    };

    class WindowMinimizedEvent final : public Event
    {
    public:
        explicit WindowMinimizedEvent(bool minimized)
            : m_Minimized(minimized) {}

        [[nodiscard]] std::string ToString() const override
        {
            return "WindowMinimizedEvent: " + m_Minimized ? "True" : "False";
        }

        [[nodiscard]] bool IsMinimized() const { return m_Minimized; }

        EVENT_CLASS_TYPE(WindowMinimized);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);
    private:
        bool m_Minimized = false;
    };

    class WindowDPIScaleChangedEvent final : public Event
    {
    public:
        WindowDPIScaleChangedEvent(float scaleX, float scaleY)
            : m_ScaleX(scaleX), m_ScaleY(scaleY) {}

        [[nodiscard]] float GetScaleX() const { return m_ScaleX; }
        [[nodiscard]] float GetScaleY() const { return m_ScaleY; }

        [[nodiscard]] std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowDPIScaleChangedEvent: " << m_ScaleX << "x" << m_ScaleY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowDPIScaleChanged);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);

    private:
        float m_ScaleX, m_ScaleY;
    };
}
