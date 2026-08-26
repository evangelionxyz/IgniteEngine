// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "widget_container.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{

    WidgetContainer::WidgetContainer(WidgetID wID)
        : IWidgetItem(wID)
    {
        layout.widthMode = SizeMode::Auto;
        layout.heightMode = SizeMode::Auto;
    }

    WidgetContainer::~WidgetContainer()
    {
    }

    void WidgetContainer::Measure()
    {
        float measuredWidth = 0.0f;
        float measuredHeight = 0.0f;
        int visibleChildren = 0;

        const bool isRow = (layout.flex.direction == FlexDirection::Row);

        for (const Ref<IWidgetItem> &child : children)
        {
            if (!child || child->IsCollapsed())
                continue;

            child->Measure();

            if (child->layout.positionType == PositionType::Absolute)
                continue;

            ++visibleChildren;

            float childW = (child->layout.widthMode == SizeMode::Fixed) ? child->layout.width : child->GetSize().x;
            float childH = (child->layout.heightMode == SizeMode::Fixed) ? child->layout.height : child->GetSize().y;

            childW = std::clamp(childW, child->layout.minWidth, child->layout.maxWidth);
            childH = std::clamp(childH, child->layout.minHeight, child->layout.maxHeight);

            const float childMarginX = child->layout.margin.w + child->layout.margin.y;
            const float childMarginY = child->layout.margin.x + child->layout.margin.z;

            const float totalChildW = childW + childMarginX;
            const float totalChildH = childH + childMarginY;

            if (isRow)
            {
                measuredWidth += totalChildW;
                measuredHeight = std::max(measuredHeight, totalChildH);
            }
            else
            {
                measuredWidth = std::max(measuredWidth, totalChildW);
                measuredHeight += totalChildH;
            }
        }

        if (visibleChildren > 1)
        {
            const float totalGap = layout.flex.gap * static_cast<float>(visibleChildren - 1);
            if (isRow)
                measuredWidth += totalGap;
            else
                measuredHeight += totalGap;
        }

        const float paddingX = layout.padding.w + layout.padding.y;
        const float paddingY = layout.padding.x + layout.padding.z;

        measuredWidth += paddingX;
        measuredHeight += paddingY;

        if (layout.widthMode == SizeMode::Auto || layout.width <= 0.0f)
            layout.width = std::clamp(measuredWidth, layout.minWidth, layout.maxWidth);

        if (layout.heightMode == SizeMode::Auto || layout.height <= 0.0f)
            layout.height = std::clamp(measuredHeight, layout.minHeight, layout.maxHeight);
    }

    void WidgetContainer::Arrange(const Rect &parentArea)
    {
        IWidgetItem::Arrange(parentArea);

        const glm::vec2 contentSize = glm::max(box.content.max - box.content.min, glm::vec2(0.0f));
        const bool isRow = (layout.flex.direction == FlexDirection::Row);

        std::vector<Ref<IWidgetItem>> flexChildren;
        std::vector<Ref<IWidgetItem>> absoluteChildren;

        for (const Ref<IWidgetItem> &child : children)
        {
            if (!child || child->IsCollapsed())
                continue;

            if (child->layout.positionType == PositionType::Absolute)
                absoluteChildren.push_back(child);
            else
                flexChildren.push_back(child);
        }

        for (const Ref<IWidgetItem> &child : absoluteChildren)
        {
            child->Arrange(box.content);
        }

        if (flexChildren.empty())
            return;

        const float containerMainSize = isRow ? contentSize.x : contentSize.y;
        const float containerCrossSize = isRow ? contentSize.y : contentSize.x;
        const float containerMainMin = isRow ? box.content.min.x : box.content.min.y;
        const float containerCrossMin = isRow ? box.content.min.y : box.content.min.x;
        const float containerCrossMax = isRow ? box.content.max.y : box.content.max.x;

        struct ChildLayoutInfo
        {
            Ref<IWidgetItem> item;
            float baseMain = 0.0f;
            float mainSize = 0.0f;
            float crossSize = 0.0f;
            float marginMainStart = 0.0f;
            float marginMainEnd = 0.0f;
            float marginCrossStart = 0.0f;
            float marginCrossEnd = 0.0f;
        };

        std::vector<ChildLayoutInfo> layoutInfos;
        layoutInfos.reserve(flexChildren.size());

        float totalBaseMain = 0.0f;
        float totalGrow = 0.0f;
        float totalShrink = 0.0f;

        for (const Ref<IWidgetItem> &child : flexChildren)
        {
            ChildLayoutInfo info;
            info.item = child;

            info.marginMainStart  = isRow ? child->layout.margin.w : child->layout.margin.x;
            info.marginMainEnd    = isRow ? child->layout.margin.y : child->layout.margin.z;
            info.marginCrossStart = isRow ? child->layout.margin.x : child->layout.margin.w;
            info.marginCrossEnd   = isRow ? child->layout.margin.z : child->layout.margin.y;

            const SizeMode mainMode  = isRow ? child->layout.widthMode : child->layout.heightMode;
            const float rawMainSize  = isRow ? child->layout.width : child->layout.height;

            if (child->layout.flex.basis >= 0.0f)
            {
                info.baseMain = child->layout.flex.basis;
            }
            else if (mainMode == SizeMode::Percent)
            {
                info.baseMain = (rawMainSize / 100.0f) * containerMainSize;
            }
            else
            {
                info.baseMain = rawMainSize;
            }

            info.mainSize = info.baseMain;

            totalBaseMain += info.baseMain + info.marginMainStart + info.marginMainEnd;
            totalGrow += child->layout.flex.grow;
            totalShrink += child->layout.flex.shrink;

            layoutInfos.push_back(info);
        }

        const float totalGaps = (flexChildren.size() > 1) ? layout.flex.gap * static_cast<float>(flexChildren.size() - 1) : 0.0f;
        const float remainingSpace = containerMainSize - (totalBaseMain + totalGaps);

        if (remainingSpace > 0.0f && totalGrow > 0.0f)
        {
            for (auto &info : layoutInfos)
            {
                if (info.item->layout.flex.grow > 0.0f)
                {
                    const float extra = remainingSpace * (info.item->layout.flex.grow / totalGrow);
                    info.mainSize = info.baseMain + extra;
                }
            }
        }
        else if (remainingSpace < 0.0f && totalShrink > 0.0f)
        {
            const float deficit = -remainingSpace;
            for (auto &info : layoutInfos)
            {
                if (info.item->layout.flex.shrink > 0.0f)
                {
                    const float reduction = deficit * (info.item->layout.flex.shrink / totalShrink);
                    info.mainSize = std::max(0.0f, info.baseMain - reduction);
                }
            }
        }

        for (auto &info : layoutInfos)
        {
            const float minM = isRow ? info.item->layout.minWidth : info.item->layout.minHeight;
            const float maxM = isRow ? info.item->layout.maxWidth : info.item->layout.maxHeight;
            info.mainSize = std::clamp(info.mainSize, minM, maxM);
        }

        auto ResolveChildAlign = [&](const Ref<IWidgetItem> &item) -> AlignItems
        {
            if (item->layout.flex.alignSelf != AlignSelf::Auto)
            {
                switch (item->layout.flex.alignSelf)
                {
                    case AlignSelf::Start:   return AlignItems::Start;
                    case AlignSelf::Center:  return AlignItems::Center;
                    case AlignSelf::End:     return AlignItems::End;
                    case AlignSelf::Stretch: return AlignItems::Stretch;
                    default: break;
                }
            }

            return layout.flex.alignItems;
        };

        for (auto &info : layoutInfos)
        {
            AlignItems align = ResolveChildAlign(info.item);
            const SizeMode crossMode = isRow ? info.item->layout.heightMode : info.item->layout.widthMode;
            const float rawCrossSize = isRow ? info.item->layout.height : info.item->layout.width;

            if (align == AlignItems::Stretch && crossMode != SizeMode::Fixed)
            {
                info.crossSize = std::max(0.0f, containerCrossSize - (info.marginCrossStart + info.marginCrossEnd));
            }
            else if (crossMode == SizeMode::Percent)
            {
                info.crossSize = (rawCrossSize / 100.0f) * containerCrossSize;
            }
            else
            {
                info.crossSize = rawCrossSize;
            }

            const float minC = isRow ? info.item->layout.minHeight : info.item->layout.minWidth;
            const float maxC = isRow ? info.item->layout.maxHeight : info.item->layout.maxWidth;
            info.crossSize = std::clamp(info.crossSize, minC, maxC);
        }

        float sumFinalMain = 0.0f;
        for (const auto &info : layoutInfos)
        {
            sumFinalMain += info.mainSize + info.marginMainStart + info.marginMainEnd;
        }

        const float freeMainSpace = containerMainSize - (sumFinalMain + totalGaps);
        const size_t count = layoutInfos.size();

        float mainCursor = containerMainMin;
        float stepGap = layout.flex.gap;

        switch (layout.flex.justifyContent)
        {
            case JustifyContent::Start:
                mainCursor = containerMainMin;
                break;
            case JustifyContent::Center:
                mainCursor = containerMainMin + (freeMainSpace / 2.0f);
                break;
            case JustifyContent::End:
                mainCursor = containerMainMin + freeMainSpace;
                break;
            case JustifyContent::SpaceBetween:
                mainCursor = containerMainMin;
                if (count > 1)
                    stepGap = layout.flex.gap + (freeMainSpace / static_cast<float>(count - 1));
                break;
            case JustifyContent::SpaceAround:
                if (count > 0)
                {
                    const float spacePerChild = freeMainSpace / static_cast<float>(count);
                    mainCursor = containerMainMin + (spacePerChild / 2.0f);
                    stepGap = layout.flex.gap + spacePerChild;
                }
                break;
        }

        for (auto &info : layoutInfos)
        {
            AlignItems align = ResolveChildAlign(info.item);

            const float childMainPos = mainCursor + info.marginMainStart;
            float childCrossPos = containerCrossMin + info.marginCrossStart;
            const float availableCross = containerCrossSize - (info.marginCrossStart + info.marginCrossEnd);

            switch (align)
            {
                case AlignItems::Start:
                case AlignItems::Stretch:
                    childCrossPos = containerCrossMin + info.marginCrossStart;
                    break;
                case AlignItems::Center:
                    childCrossPos = containerCrossMin + info.marginCrossStart + (availableCross - info.crossSize) / 2.0f;
                    break;
                case AlignItems::End:
                    childCrossPos = containerCrossMax - info.marginCrossEnd - info.crossSize;
                    break;
            }

            Rect childRect;
            if (isRow)
            {
                childRect.min = glm::vec2(childMainPos, childCrossPos);
                childRect.max = childRect.min + glm::vec2(info.mainSize, info.crossSize);
                info.item->layout.width = info.mainSize;
                info.item->layout.height = info.crossSize;
            }
            else
            {
                childRect.min = glm::vec2(childCrossPos, childMainPos);
                childRect.max = childRect.min + glm::vec2(info.crossSize, info.mainSize);
                info.item->layout.width = info.crossSize;
                info.item->layout.height = info.mainSize;
            }

            info.item->Arrange(childRect);

            mainCursor = childMainPos + info.mainSize + info.marginMainEnd + stepGap;
        }
    }

    bool WidgetContainer::HitTest(int px, int py)
    {
        const glm::vec2 point = glm::vec2(static_cast<float>(px), static_cast<float>(py));
        if (!worldRect.Contains(point))
            return false;

        for (auto it = children.rbegin(); it != children.rend(); ++it)
        {
            const Ref<IWidgetItem> &child = *it;
            if (child && child->IsVisible() && child->HitTest(px, py))
                return true;
        }

        return true;
    }

    // =========================================================================
    // WidgetBoxSizing
    // =========================================================================

    WidgetBoxSizing::WidgetBoxSizing(WidgetID wID)
        : WidgetContainer(wID)
    {
    }

    WidgetBoxSizing::~WidgetBoxSizing()
    {
    }

    void WidgetBoxSizing::Measure()
    {
        WidgetContainer::Measure();
        layout.width = std::clamp(layout.width, minSize.x, maxSize.x);
        layout.height = std::clamp(layout.height, minSize.y, maxSize.y);
    }

    // =========================================================================
    // WidgetOverlay
    // =========================================================================

    WidgetOverlay::WidgetOverlay(WidgetID wID)
        : WidgetContainer(wID)
    {
    }

    WidgetOverlay::~WidgetOverlay()
    {
    }

    void WidgetOverlay::Arrange(const Rect &parentArea)
    {
        IWidgetItem::Arrange(parentArea);

        for (const Ref<IWidgetItem> &child : children)
        {
            if (!child || child->IsCollapsed())
                continue;

            child->Arrange(box.content);
        }
    }

}
