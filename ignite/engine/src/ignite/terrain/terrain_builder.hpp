// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_TERRAIN_BUILDER_HPP
#define IGN_TERRAIN_BUILDER_HPP

#include "ignite/core/base.hpp"
#include "terrain.hpp"

namespace ignite
{
    class TerrainComponent;

    class IGN_API TerrainBuilder
    {
    public:
        void ApplyBrush();
        void Raise();
        void Flatten();
        void Smooth();
        void Paint();

        void Generate();
        static void GenerateProcedural(TerrainData &data, float frequency = 0.2f, int seed = 1337);
        static void GenerateProcedural(TerrainComponent &comp, float frequency = 0.2f, int seed = 1337);

    private:

    };
}

#endif
