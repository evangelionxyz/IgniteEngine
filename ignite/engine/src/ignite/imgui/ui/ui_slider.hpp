// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_UI_SLIDER_HPP
#define IGN_UI_SLIDER_HPP

#include "ui_theme.hpp"

namespace ignite::UI
{
    IGN_API bool SliderFloat(
        const char *label,
        float *value,
        float minVal,
        float maxVal,
        const char *format = "%.2f",
        float width = 0.0f);

    IGN_API bool SliderInt(
        const char *label,
        int *value,
        int minVal,
        int maxVal,
        const char *format = "%d",
        float width = 0.0f);

    IGN_API bool RangeSliderFloat(
        const char *label,
        float *minVal,
        float *maxVal,
        float rangeMin,
        float rangeMax,
        const char *format = "%.1f",
        float width = 0.0f);
}

#endif

