// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_TERRAIN_RENDERER_HPP
#define IGN_TERRAIN_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "ignite/terrain/terrain.hpp"
#include <nvrhi/nvrhi.h>

namespace ignite
{
    class TerrainComponent;

    class IGN_API TerrainRenderer
    {
    public:
        TerrainRenderer() = default;
        ~TerrainRenderer() = default;

        void Init();
        void RebuildMesh(nvrhi::ICommandList *cmd, TerrainComponent &comp, const glm::mat4 &worldTransform);
    };
}

#endif
