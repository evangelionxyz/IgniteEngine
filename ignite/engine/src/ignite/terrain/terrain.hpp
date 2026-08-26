// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_TERRAIN_HPP
#define IGN_TERRAIN_HPP

#include "ignite/core/base.hpp"
#include "ignite/graphics/brush.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/math/aabb.hpp"
#include "ignite/asset/asset.hpp"

#include <vector>
#include <cstdint>

namespace ignite
{
    struct TerrainBrush
    {
        float radius = 10.0f;
        float strength = 1.0f;
        float fallOff = 0.5f;
        BrushShape shape = BrushShape::Circle;
        BrushMode mode = BrushMode::Raise;
    };

    class IGN_API TerrainData : public Asset
    {
    public:
        TerrainData() = default;
        virtual ~TerrainData() override = default;

        uint32_t resolution = 64;
        float worldSize = 100.0f;
        float maxHeight = 50.0f;
        std::vector<float> heightmap;
        AssetHandle heightmapTextureHandle = AssetHandle(0);
        bool dirty = true;

        void InitFlat(uint32_t res = 64, float size = 100.0f, float maxH = 50.0f);
        float GetHeight(uint32_t x, uint32_t z) const;
        void SetHeight(uint32_t x, uint32_t z, float val);

        bool LoadHeightmap(const std::string &filepath);
        bool LoadFromTexture(AssetHandle textureHandle);
        float SampleHeight(float worldX, float worldZ) const;
        glm::vec3 SampleNormal(float worldX, float worldZ) const;

        static AssetType GetStaticType() { return AssetType::Terrain; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

        virtual bool Serialize(const std::filesystem::path &filepath) override;
        static Ref<TerrainData> Deserialize(const std::filesystem::path &filepath);
    };

    class IGN_API TerrainChunk
    {
    public:
        uint32_t gridX = 0;
        uint32_t gridZ = 0;
        uint32_t chunkResolution = 64;
        uint32_t lodLevel = 0;
        bool gpuBuffersDirty = true;

        AABB bounds;
        Ref<MeshPrimitive<VertexMeshStatic>> primitive;
    };

    class TerrainRenderer;

    class IGN_API TerrainEditor
    {
    public:

    private:
    };
}

#endif
