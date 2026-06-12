// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_WIDGET_HPP
#define IGN_WIDGET_HPP

#include "ignite/core/types.hpp"
#include "ignite/math/math.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <vector>
#include <type_traits>
#include <glm/glm.hpp>

#define UI_COLOR_RED { 1.0f, 0.0f, 0.0f, 1.0f }
#define UI_COLOR_BLUE { 0.0f, 0.0f, 1.0f, 1.0f }
#define UI_COLOR_WHITE { 1.0f, 1.0f, 1.0f, 1.0f }
#define UI_COLOR_GRAY { 0.5f, 0.5f, 0.5f, 1.0f }
#define UI_COLOR_DARK_GRAY { 0.25f, 0.25f, 0.25f, 1.0f }
#define UI_COLOR_GREEN { 0.0f, 1.0f, 0.0f, 1.0f }
#define UI_COLOR_YELLOW { 1.0f, 1.0f, 0.0f, 1.0f }
#define UI_COLOR_GRAY { 0.5f, 0.5f, 0.5f, 1.0f }

namespace ignite
{
    class WidgetCanvas;
    class Scene;
    class Texture;
    class Font;
    class WidgetRenderer;
    class WidgetContainer;

    enum class LayoutMode : uint8_t
    {
        Horizontal,
        Vertical,
        Grid,
        Absolute,

        COUNT
    };

    enum class VerticalAlignment : uint8_t
    {
        Top = 0,
        Middle,
        Bottom,
        ExpandVertically,

        COUNT
    };

    enum class HorizontalAlignment : uint8_t
    {
        Left = 0,
        Center,
        Right,
        ExpandHorizontally,

        COUNT
    };

    enum class WidgetType : uint8_t
    {
        Container = 0,
        Button,
        Label,
        Image,
        BoxSizing,
        Overlay,

        COUNT
    };

    typedef int WidgetID;

    // Base widget item
    class IWidgetItem : public std::enable_shared_from_this<IWidgetItem>
    {
    protected:
        std::function<void()> m_OnClick;
        std::function<void()> m_OnPressed;
        std::function<void()> m_OnReleased;
        std::function<void()> m_OnHoverEnter;
        std::function<void()> m_OnHoverExit;

    public:
        IWidgetItem(WidgetID wID) : id(wID) { }

        IWidgetItem *parent = nullptr;
        std::vector<Ref<IWidgetItem>> children;

        std::string name; // widget name
        WidgetID id = -1;

        // Layout info
        glm::vec2 position = glm::vec2(0.0f); // x, y
        glm::vec2 size = glm::vec2(150.0f, 50.0f); // width, height
        float padding = 0.0f;
        float margin = 0.0f;
        int zIndex = 0;

        VerticalAlignment VAlignment = VerticalAlignment::Top;
        HorizontalAlignment HAlignment = HorizontalAlignment::Left;

        Rect worldRect;
        bool visible = true;

    public:
        virtual ~IWidgetItem() = default;

        template<typename T, typename... Args>
        T *AddChild(Args&&... args)
        {
            static_assert(std::is_base_of_v<IWidgetItem, T>, "T must derive from IWidgetItem");
            if constexpr (std::is_abstract_v<T>)
            {
                return nullptr;
            }

            Ref<T> child = CreateRef<T>(std::forward<Args>(args)...);
            child->parent = this;
            children.emplace_back(child);

            auto *ptr = child.get();
            return ptr;
        }

        virtual void Measure() { }
        virtual void Arrange(const Rect &parentArea) { }
        virtual bool HitTest(int px, int py) { return false; }

        void SetVisible(bool isVisible) { visible = isVisible; }
        bool IsVisible() const { return visible; }
        const Rect &GetAlignedRect() const { return worldRect; }

        Rect CalculateAlignedRect(const Rect &parentArea) const
        {
            const glm::vec2 marginVec = glm::vec2(margin);
            const glm::vec2 availableMin = parentArea.min + marginVec;
            const glm::vec2 availableMax = parentArea.max - marginVec;
            const glm::vec2 availableSize = glm::max(availableMax - availableMin, glm::vec2(0.0f));

            glm::vec2 resolvedSize = size;
            glm::vec2 alignedMin = availableMin;

            switch (HAlignment)
            {
                case HorizontalAlignment::Left: break;
                case HorizontalAlignment::Center: alignedMin.x += (availableSize.x - size.x) / 2.0f; break;
                case HorizontalAlignment::Right: alignedMin.x += (availableSize.x - size.x); break;
                case HorizontalAlignment::ExpandHorizontally: resolvedSize.x = availableSize.x; break;
            }

            switch (VAlignment)
            {
                case VerticalAlignment::Top: break;
                case VerticalAlignment::Middle: alignedMin.y += (availableSize.y - size.y) / 2.0f; break;
                case VerticalAlignment::Bottom: alignedMin.y += (availableSize.y - size.y); break;
                case VerticalAlignment::ExpandVertically: resolvedSize.y = availableSize.y; break;
            }

            alignedMin += position;
            return { alignedMin, alignedMin + resolvedSize };
        }

        // Event callbacks
        void SetOnClick(std::function<void()> callback) { m_OnClick = callback; }
        void SetOnPressed(std::function<void()> callback) { m_OnPressed = callback; }
        void SetOnReleased(std::function<void()> callback) { m_OnReleased = callback; }
        void SetOnHoverEnter(std::function<void()> callback) { m_OnHoverEnter = callback; }
        void SetOnHoverExit(std::function<void()> callback) { m_OnHoverExit = callback; }

        template<typename T>
        Ref<T> As() { return std::dynamic_pointer_cast<T>(shared_from_this()); }

        virtual WidgetType GetWidgetType() const = 0;
    };
}

#endif
