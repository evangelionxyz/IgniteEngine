// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_TERRAIN_BUILDER_HPP
#define IGN_TERRAIN_BUILDER_HPP

#include "ignite/core/base.hpp"
#include "terrain.hpp"

namespace ignite
{
    class IGN_API TerrainBuilder
    {
    public:

        void ApplyBrush();
        void Raise();
        void Flatten();
        void Smooth();
        void Paint();

        void Generate();

    private:

    };
}

#endif
