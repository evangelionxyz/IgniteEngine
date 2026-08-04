// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_BRUSH_HPP
#define IGN_BRUSH_HPP

namespace ignite
{
    enum class BrushShape
    {
        Circle = 0,
        Square,
        CustomAlpha,
        Noise
    };

    enum class BrushMode
    {
        Raise = 0,
        Lower,
        Flatten,
        Smooth,
        Paint,
        Erase,
        Noise
    };
}

#endif
