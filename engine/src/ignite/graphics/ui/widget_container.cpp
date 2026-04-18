// Copyright (c) 2026 Evangelion Manuhutu

// Created by: Evangelion Manuhutu
// Date      : 18 April 2026

#include "widget_container.hpp"

#include <algorithm>

namespace ignite
{

    void WidgetContainer::Update(float deltaTime, const glm::vec2 &mousePosition)
    {
        if (!visible)
            return;

        for (const Ref<IWidgetItem> &child : children)
        {
            if (!child || !child->IsVisible())
                continue;

            child->Update(deltaTime, mousePosition);
        }
    }

    void WidgetContainer::Draw(Renderer2D *renderer, AssetManager *assetManager)
    {
        if (!visible)
            return;

        for (const Ref<IWidgetItem> &child : children)
        {
            if (!child || !child->IsVisible())
                continue;

            child->Draw(renderer, assetManager);
        }
    }

    void WidgetContainer::Measure()
    {
        IWidgetItem::Measure();

        float measuredWidth = 0.0f;
        float measuredHeight = 0.0f;
        int visibleChildren = 0;

        for (const Ref<IWidgetItem> &child : children)
        {
            if (!child || !child->IsVisible())
            {
                continue;
            }

            const glm::vec2 childSize = child->size + glm::vec2(child->margin * 2.0f);
            ++visibleChildren;

            switch (layout)
            {
                case LayoutMode::Horizontal:
                    measuredWidth += childSize.x;
                    measuredHeight = std::max(measuredHeight, childSize.y);
                    break;
                case LayoutMode::Vertical:
                    measuredWidth = std::max(measuredWidth, childSize.x);
                    measuredHeight += childSize.y;
                    break;
                case LayoutMode::Absolute:
                    measuredWidth = std::max(measuredWidth, child->position.x + childSize.x);
                    measuredHeight = std::max(measuredHeight, child->position.y + childSize.y);
                    break;
                case LayoutMode::Grid:
                default:
                    measuredWidth = std::max(measuredWidth, childSize.x);
                    measuredHeight += childSize.y;
                    break;
            }
        }

        if (visibleChildren > 1)
        {
            switch (layout)
            {
                case LayoutMode::Horizontal:
                    measuredWidth += gap * static_cast<float>(visibleChildren - 1);
                    break;
                case LayoutMode::Vertical:
                case LayoutMode::Grid:
                    measuredHeight += gap * static_cast<float>(visibleChildren - 1);
                    break;
                case LayoutMode::Absolute:
                default:
                    break;
            }
        }

        measuredWidth += padding * 2.0f;
        measuredHeight += padding * 2.0f;

        if (size.x <= 0.0f)
        {
            size.x = measuredWidth;
        }

        if (size.y <= 0.0f)
        {
            size.y = measuredHeight;
        }
    }

    void WidgetContainer::Arrange(const Rect &parentArea)
    {
        worldRect = CalculateAlignedRect(parentArea);

        const glm::vec2 contentMin = worldRect.min + glm::vec2(padding);
        const glm::vec2 contentMax = worldRect.max - glm::vec2(padding);

        float cursorX = contentMin.x;
        float cursorY = contentMin.y;

        for (const Ref<IWidgetItem> &child : children)
        {
            if (!child || !child->IsVisible())
            {
                continue;
            }

            Rect childRect;

            switch (layout)
            {
                case LayoutMode::Horizontal:
                {
                    const glm::vec2 min = { cursorX + child->margin, contentMin.y };
                    const glm::vec2 max = { contentMax.x, contentMax.y };
                    childRect = Rect(min, max);
                    break;
                }
                case LayoutMode::Vertical:
                {
                    const glm::vec2 min = { contentMin.x, cursorY + child->margin };
                    const glm::vec2 max = { contentMax.x, contentMax.y };
                    childRect = Rect(min, max);
                    break;
                }
                case LayoutMode::Absolute:
                {
                    childRect = Rect(contentMin, contentMax);
                    break;
                }
                case LayoutMode::Grid:
                default:
                {
                    const glm::vec2 min = { contentMin.x, cursorY + child->margin };
                    const glm::vec2 max = { contentMax.x, contentMax.y };
                    childRect = Rect(min, max);
                    break;
                }
            }

            child->Arrange(childRect);

            if (layout == LayoutMode::Horizontal)
            {
                cursorX = child->GetAlignedRect().max.x + child->margin + gap;
            }
            else if (layout == LayoutMode::Vertical || layout == LayoutMode::Grid)
            {
                cursorY = child->GetAlignedRect().max.y + child->margin + gap;
            }

            if (cursorX > contentMax.x && layout == LayoutMode::Horizontal)
            {
                break;
            }

            if (cursorY > contentMax.y && (layout == LayoutMode::Vertical || layout == LayoutMode::Grid))
            {
                break;
            }
        }
    }

    bool WidgetContainer::HitTest(int px, int py)
    {
        const glm::vec2 point = glm::vec2(static_cast<float>(px), static_cast<float>(py));
        if (!worldRect.Contains(point))
        {
            return false;
        }

        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            const Ref<IWidgetItem> &child = *it;
            if (child && child->IsVisible() && child->HitTest(px, py))
            {
                return true;
            }
        }

        return true;
    }

}