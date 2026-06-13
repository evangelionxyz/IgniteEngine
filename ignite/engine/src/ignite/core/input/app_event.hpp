// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_APP_EVENT_HPP
#define IGN_APP_EVENT_HPP

#include "event.hpp"
#include "ignite/asset/asset.hpp"

#include <vector>
#include <sstream>
#include "ignite/core/path.hpp"

namespace ignite
{
    class WindowResizeEvent final : public Event
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height)
            : m_Width(width), m_Height(height) {}
        u32 GetWidth() const { return m_Width; }
        u32 GetHeight() const { return m_Height; }
        std::string ToString() const override
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
        std::string ToString() const override
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
        i32 GetWidth() const { return m_Width; }
        i32 GetHeight() const { return m_Height; }
        std::string ToString() const override
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
        explicit WindowDropEvent(const std::vector<ignite::Path> &paths)
            : m_Paths(paths) {}
        explicit WindowDropEvent(std::vector <ignite::Path> &&paths)
            : m_Paths(std::move(paths)) {}

        const std::vector<ignite::Path> &GetPaths() const { return m_Paths; }

        EVENT_CLASS_TYPE(WindowDrop);
        EVENT_CLASS_CATEGORY(EventCategoryApplication);
    private:
        std::vector<ignite::Path> m_Paths;
    };

    class WindowMaximizedEvent final : public Event
    {
    public:
        explicit WindowMaximizedEvent(bool maximized)
            : m_Maximized(maximized) {}

        std::string ToString() const override
        {
            return "WindowMaximizedEvent";
        }

        bool IsMaximized() const { return m_Maximized; }

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

        std::string ToString() const override
        {
            return "WindowMinimizedEvent";
        }

        bool IsMinimized() const { return m_Minimized; }

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

        float GetScaleX() const { return m_ScaleX; }
        float GetScaleY() const { return m_ScaleY; }

        std::string ToString() const override
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

    class AssetEditorOpenEvent final : public Event
    {
    public:
        AssetEditorOpenEvent(AssetHandle handle, AssetMetaData metadata)
            : m_Handle(handle), m_AssetMetaData(metadata)
        {
        }

        AssetHandle GetAssetHandle() { return m_Handle; }
        AssetMetaData &GetAssetMetaData() { return m_AssetMetaData; }

		EVENT_CLASS_TYPE(AssetEditorOpen);
		EVENT_CLASS_CATEGORY(EventCategoryApplication);

    private:
        AssetMetaData m_AssetMetaData;
        AssetHandle m_Handle;
    };

    class AssetEditorCreateEvent final : public Event
    {
    public:
        AssetEditorCreateEvent(AssetType type, ignite::Path targetDirectory)
            : m_Type(type), m_TargetDirectory(std::move(targetDirectory))
        {
        }

        AssetType GetAssetType() const { return m_Type; }
        const ignite::Path &GetTargetDirectory() const { return m_TargetDirectory; }

        EVENT_CLASS_TYPE(AssetEditorCreate);
		EVENT_CLASS_CATEGORY(EventCategoryApplication);

    private:
        AssetType m_Type = AssetType::Invalid;
        ignite::Path m_TargetDirectory;
    };
}

#endif
