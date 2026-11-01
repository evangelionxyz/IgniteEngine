// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_TERRAIN_RENDERER_HPP
#define IGN_TERRAIN_RENDERER_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/terrain/terrain.hpp"

namespace ignite
{
    class TerrainComponent;

    class IGN_API TerrainRenderer
    {
    public:
        TerrainRenderer() = default;
        ~TerrainRenderer() = default;

        void Init();
        void RebuildMesh(nri::CommandBuffer *cmd, TerrainComponent &comp, const glm::mat4 &worldTransform);
    };
}

#endif
