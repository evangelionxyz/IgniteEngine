// Copyright (c) 2026 Evangelion Manuhutu
#include "ignite_pch.hpp"
#include "terrain_builder.hpp"

#include "ignite/core/logger.hpp"
#include "FastNoise/FastNoise.h"

#include "ignite/scene/component.hpp"

namespace ignite
{
    void TerrainBuilder::ApplyBrush()
    {

    }

    void TerrainBuilder::Raise()
    {

    }

    void TerrainBuilder::Flatten()
    {

    }

    void TerrainBuilder::Smooth()
    {

    }

    void TerrainBuilder::Paint()
    {

    }

    void TerrainBuilder::Generate()
    {
        auto simplex = FastNoise::New<FastNoise::Simplex>();
        if (simplex)
        {
            float noiseVal = simplex->GenSingle2D(0.5f, 0.5f, 1337);
            LOG_INFO("FastNoise2 Simplex test sample at (0.5, 0.5): {}", noiseVal);
        }
    }

    void TerrainBuilder::GenerateProcedural(TerrainData &data, float frequency, int seed)
    {
        uint32_t res = std::max(2u, data.resolution);
        data.heightmap.resize(static_cast<size_t>(res) * static_cast<size_t>(res));

        auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
        auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();
        if (fnFractal && fnSimplex)
        {
            fnFractal->SetSource(fnSimplex);
            fnFractal->SetOctaveCount(5);
            fnFractal->SetGain(0.5f);
            fnFractal->SetLacunarity(2.0f);

            FastNoise::OutputMinMax minMax = fnFractal->GenUniformGrid2D(
                data.heightmap.data(),
                0, 0,
                static_cast<int>(res), static_cast<int>(res),
                frequency, frequency,
                seed
            );

            float range = minMax.max - minMax.min;
            if (range > 0.00001f)
            {
                for (float &val : data.heightmap)
                {
                    val = (val - minMax.min) / range;
                }
            }
            LOG_INFO("[TerrainBuilder] Generated FastNoise2 fractal terrain ({}x{}, min: {}, max: {})", res, res, minMax.min, minMax.max);
        }
        else
        {
            for (uint32_t z = 0; z < res; ++z)
            {
                for (uint32_t x = 0; x < res; ++x)
                {
                    float fx = static_cast<float>(x) * frequency;
                    float fz = static_cast<float>(z) * frequency;
                    float val = std::sin(fx) * std::cos(fz) * 0.5f + 0.5f;
                    data.SetHeight(x, z, val);
                }
            }
            LOG_INFO("[TerrainBuilder] Generated fallback procedural terrain ({}x{})", res, res);
        }

        data.dirty = true;
    }

    void TerrainBuilder::GenerateProcedural(TerrainComponent &comp, float frequency, int seed)
    {
        if (!comp.data)
        {
            comp.data = CreateRef<TerrainData>();
            comp.data->InitFlat(comp.resolution, comp.worldSize, comp.maxHeight);
        }

        comp.data->resolution = comp.resolution;
        comp.data->worldSize = comp.worldSize;
        comp.data->maxHeight = comp.maxHeight;

        GenerateProcedural(*comp.data, frequency, seed);
        comp.gpuInitialized = false;
    }
}
