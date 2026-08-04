// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_TERRAIN_HPP
#define IGN_TERRAIN_HPP

#include "ignite/core/base.hpp"
#include "ignite/graphics/brush.hpp"

namespace ignite
{
    struct TerrainBrush
    {
        float radius;
        float strength;
        float fallOff;
        BrushShape shape;
        BrushMode mode;
    };

    struct IGN_API TerrainData
    {
        // - Heightmap
        // - Splatmap
        // - Holes
        // - Vegetation
        // - Detail layers
    };

    class IGN_API TerrainRenderer
    {
    public:

    private:

    };

    class IGN_API TerrainChunk
    {
    public:

    };

    class IGN_API TerrainEditor
    {
    public:

    private:
    };
}

#endif
