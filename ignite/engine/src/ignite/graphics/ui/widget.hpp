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

    enum class SizeMode : uint8_t
    {
        Auto = 0,
        Fixed,
        Fill,
        Percent
    };

    enum class FlexDirection : uint8_t
    {
        Row = 0,
        Column
    };

    enum class JustifyContent : uint8_t
    {
        Start = 0,
        Center,
        End,
        SpaceBetween,
        SpaceAround
    };

    enum class AlignItems : uint8_t
    {
        Start = 0,
        Center,
        End,
        Stretch
    };

    enum class AlignSelf : uint8_t
    {
        Auto = 0,
        Start,
        Center,
        End,
        Stretch
    };

    enum class FlexWrap : uint8_t
    {
        NoWrap = 0,
        Wrap
    };

    enum class PositionType : uint8_t
    {
        Relative = 0,
        Absolute,
        Fixed
    };

    enum class OverflowMode : uint8_t
    {
        Visible = 0,
        Hidden,
        Clip,
        Scroll
    };

    enum class VisibilityMode : uint8_t
    {
        Visible = 0,
        Hidden,
        Collapsed
    };

    struct FlexProperties
    {
        FlexDirection direction = FlexDirection::Column;
        JustifyContent justifyContent = JustifyContent::Start;
        AlignItems alignItems = AlignItems::Start;
        AlignSelf alignSelf = AlignSelf::Auto;
        FlexWrap wrap = FlexWrap::NoWrap;
        float grow = 0.0f;
        float shrink = 1.0f;
        float basis = -1.0f; // -1 = auto
        float gap = 0.0f;
    };

    struct UILayout
    {
        glm::vec2 position = glm::vec2(0.0f);
        SizeMode widthMode = SizeMode::Fixed;
        SizeMode heightMode = SizeMode::Fixed;
        float width = 150.0f;
        float height = 50.0f;
        float minWidth = 0.0f;
        float minHeight = 0.0f;
        float maxWidth = 10000.0f;
        float maxHeight = 10000.0f;
        glm::vec4 margin = glm::vec4(0.0f);  // top, right, bottom, left
        glm::vec4 padding = glm::vec4(0.0f); // top, right, bottom, left
        HorizontalAlignment horizontalAlignment = HorizontalAlignment::Left;
        VerticalAlignment verticalAlignment = VerticalAlignment::Top;
        FlexProperties flex;
        PositionType positionType = PositionType::Relative;
        OverflowMode overflow = OverflowMode::Visible;
        VisibilityMode visibility = VisibilityMode::Visible;
    };

    struct Box
    {
        Rect content;
        Rect padding;
        Rect border;
        Rect margin;
    };

    struct UIStyle
    {
        glm::vec4 backgroundColor = glm::vec4(0.0f);
        glm::vec4 borderColor = glm::vec4(0.0f);
        float borderWidth = 0.0f;
        float cornerRadius = 0.0f;
        float opacity = 1.0f;
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
    class IGN_API IWidgetItem : public std::enable_shared_from_this<IWidgetItem>
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

        // Layout & Style info
        UILayout layout;
        UIStyle baseStyle;
        Box box;

        int zIndex = 0;
        Rect worldRect;

        bool m_DirtyLayout = true;
        bool m_DirtyPaint = true;

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
            MarkLayoutDirty();
            return ptr;
        }

        virtual void Measure() { }
        virtual void Arrange(const Rect &parentArea)
        {
            worldRect = CalculateAlignedRect(parentArea);

            box.margin  = worldRect;
            box.border  = Rect(box.margin.min + glm::vec2(layout.margin.w, layout.margin.x),
                               box.margin.max - glm::vec2(layout.margin.y, layout.margin.z));
            box.padding = Rect(box.border.min + glm::vec2(baseStyle.borderWidth),
                               box.border.max - glm::vec2(baseStyle.borderWidth));
            box.content = Rect(box.padding.min + glm::vec2(layout.padding.w, layout.padding.x),
                               box.padding.max - glm::vec2(layout.padding.y, layout.padding.z));
        }
        virtual bool HitTest(int px, int py) { return false; }

        void MarkLayoutDirty()
        {
            m_DirtyLayout = true;
            if (parent)
            {
                parent->MarkLayoutDirty();
            }
        }

        void MarkPaintDirty()
        {
            m_DirtyPaint = true;
        }

        void SetVisible(bool isVisible)
        {
            layout.visibility = isVisible ? VisibilityMode::Visible : VisibilityMode::Hidden;
            MarkLayoutDirty();
            MarkPaintDirty();
        }

        bool IsVisible() const
        {
            return layout.visibility == VisibilityMode::Visible;
        }

        bool IsCollapsed() const
        {
            return layout.visibility == VisibilityMode::Collapsed;
        }

        const Rect &GetAlignedRect() const { return worldRect; }

        glm::vec2 GetPosition() const { return layout.position; }
        void SetPosition(const glm::vec2 &pos) { layout.position = pos; MarkLayoutDirty(); }
        glm::vec2 GetSize() const { return glm::vec2(layout.width, layout.height); }
        void SetSize(const glm::vec2 &sz) { layout.width = sz.x; layout.height = sz.y; MarkLayoutDirty(); }

        Rect CalculateAlignedRect(const Rect &parentArea) const
        {
            // top = margin.x, right = margin.y, bottom = margin.z, left = margin.w
            const glm::vec2 marginMin = glm::vec2(layout.margin.w, layout.margin.x);
            const glm::vec2 marginMax = glm::vec2(layout.margin.y, layout.margin.z);
            const glm::vec2 availableMin = parentArea.min + marginMin;
            const glm::vec2 availableMax = parentArea.max - marginMax;
            const glm::vec2 availableSize = glm::max(availableMax - availableMin, glm::vec2(0.0f));

            glm::vec2 resolvedSize = glm::vec2(layout.width, layout.height);
            glm::vec2 alignedMin = availableMin;

            switch (layout.horizontalAlignment)
            {
                case HorizontalAlignment::Left: break;
                case HorizontalAlignment::Center: alignedMin.x += (availableSize.x - resolvedSize.x) / 2.0f; break;
                case HorizontalAlignment::Right: alignedMin.x += (availableSize.x - resolvedSize.x); break;
                case HorizontalAlignment::ExpandHorizontally: resolvedSize.x = availableSize.x; break;
            }

            switch (layout.verticalAlignment)
            {
                case VerticalAlignment::Top: break;
                case VerticalAlignment::Middle: alignedMin.y += (availableSize.y - resolvedSize.y) / 2.0f; break;
                case VerticalAlignment::Bottom: alignedMin.y += (availableSize.y - resolvedSize.y); break;
                case VerticalAlignment::ExpandVertically: resolvedSize.y = availableSize.y; break;
            }

            alignedMin += layout.position;
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
