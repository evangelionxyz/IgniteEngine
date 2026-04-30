// Copyright (c) 2026 Evangelion Manuhutu

#include "widget_label.hpp"
#include "ignite/graphics/font.hpp"

namespace ignite
{
    WidgetLabel::WidgetLabel(const std::string &text, WidgetID wID)
        : text(text), IWidgetItem(wID)
    {
        style.color = UI_COLOR_WHITE;
    }

    WidgetLabel::~WidgetLabel()
    {
        fontHandle = 0;
        font = nullptr;
    }

    Rect WidgetLabel::GetTextBounds() const
    {
        if (!font || !font->IsReady() || text.empty())
        {
            return { glm::vec2(0.0f), glm::vec2(0.0f) };
        }

        const auto &fontGeometry = font->GetGeometry();
        const auto &metrics = fontGeometry.getMetrics();
        const double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
        const double spaceGlyphAdvance = fontGeometry.getGlyph(' ') ? fontGeometry.getGlyph(' ')->getAdvance() : 0.0;

        double x = 0.0;
        double y = 0.0;
        bool hasBounds = false;
        glm::vec2 boundsMin(0.0f);
        glm::vec2 boundsMax(0.0f);

        for (size_t i = 0; i < text.size(); ++i)
        {
            const char character = text[i];
            if (character == '\r')
            {
                continue;
            }

            if (character == '\n')
            {
                x = 0.0;
                y += fsScale * metrics.lineHeight + style.lineSpacing;
                continue;
            }

            if (character == ' ')
            {
                double advance = spaceGlyphAdvance;
                if (i < text.size() - 1)
                {
                    fontGeometry.getAdvance(advance, character, text[i + 1]);
                }
                x += fsScale * advance + style.kerning;
                continue;
            }

            if (character == '\t')
            {
                x += 4.0 * (fsScale * spaceGlyphAdvance + style.kerning);
                continue;
            }

            auto glyph = fontGeometry.getGlyph(character);
            if (!glyph)
            {
                glyph = fontGeometry.getGlyph('?');
            }
            if (!glyph)
            {
                continue;
            }

            double planeLeft, planeBottom, planeRight, planeTop;
            glyph->getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);

            glm::vec2 quadMin(static_cast<float>(planeLeft), static_cast<float>(planeBottom));
            glm::vec2 quadMax(static_cast<float>(planeRight), static_cast<float>(planeTop));

            quadMin *= static_cast<float>(fsScale);
            quadMax *= static_cast<float>(fsScale);

            quadMin.y = -quadMin.y;
            quadMax.y = -quadMax.y;

            quadMin += glm::vec2(static_cast<float>(x), static_cast<float>(y));
            quadMax += glm::vec2(static_cast<float>(x), static_cast<float>(y));

            const glm::vec2 glyphMin(std::min(quadMin.x, quadMax.x), std::min(quadMin.y, quadMax.y));
            const glm::vec2 glyphMax(std::max(quadMin.x, quadMax.x), std::max(quadMin.y, quadMax.y));

            if (!hasBounds)
            {
                boundsMin = glyphMin;
                boundsMax = glyphMax;
                hasBounds = true;
            }
            else
            {
                boundsMin = glm::min(boundsMin, glyphMin);
                boundsMax = glm::max(boundsMax, glyphMax);
            }

            double advance = glyph->getAdvance();
            if (i < text.size() - 1)
            {
                fontGeometry.getAdvance(advance, character, text[i + 1]);
            }
            x += fsScale * advance + style.kerning;
        }

        if (!hasBounds)
        {
            return { glm::vec2(0.0f), glm::vec2(0.0f) };
        }

        return { boundsMin * style.fontSize, boundsMax * style.fontSize };
    }

    void WidgetLabel::Measure()
    {
        if (!font || text.empty())
        {
            size = glm::vec2(0.0f);
            return;
        }
        size = GetTextBounds().GetSize();
    }

    void WidgetLabel::Arrange(const Rect &parentRect)
    {
        worldRect = CalculateAlignedRect(parentRect);
    }

    bool WidgetLabel::HitTest(int px, int py)
    {
        return worldRect.Contains(glm::vec2(static_cast<float>(px), static_cast<float>(py)));
    }
}
