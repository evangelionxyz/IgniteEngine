// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>
#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/terrain/terrain.hpp"
#include "ignite/terrain/terrain_builder.hpp"
#include "ignite/terrain/terrain_renderer.hpp"
#include "ignite/serializer/entity_serializer.hpp"
#include "ignite/serializer/serializer.hpp"

using namespace ignite;

TEST(TerrainSystem, FlatInitializationAndSampling)
{
    TerrainComponent comp;
    comp.resolution = 32;
    comp.worldSize = 100.0f;
    comp.maxHeight = 40.0f;

    comp.data = CreateRef<TerrainData>();
    comp.data->InitFlat(comp.resolution, comp.worldSize, comp.maxHeight);

    EXPECT_EQ(comp.data->resolution, 32u);
    EXPECT_EQ(comp.data->heightmap.size(), 32u * 32u);
    EXPECT_FLOAT_EQ(comp.data->GetHeight(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(comp.data->SampleHeight(0.0f, 0.0f), 0.0f);

    glm::vec3 normal = comp.data->SampleNormal(0.0f, 0.0f);
    EXPECT_NEAR(normal.x, 0.0f, 1e-4f);
    EXPECT_NEAR(normal.y, 1.0f, 1e-4f);
    EXPECT_NEAR(normal.z, 0.0f, 1e-4f);
}

TEST(TerrainSystem, ProceduralNoiseGeneration)
{
    TerrainComponent comp;
    comp.resolution = 64;
    comp.worldSize = 100.0f;
    comp.maxHeight = 50.0f;

    TerrainBuilder::GenerateProcedural(comp, 0.05f, 1337);

    ASSERT_NE(comp.data, nullptr);
    EXPECT_EQ(comp.data->heightmap.size(), 64u * 64u);
    EXPECT_TRUE(comp.data->dirty);
    EXPECT_FALSE(comp.gpuInitialized);

    float minVal = 1e9f;
    float maxVal = -1e9f;

    for (float h : comp.data->heightmap)
    {
        minVal = std::min(minVal, h);
        maxVal = std::max(maxVal, h);
        EXPECT_GE(h, 0.0f);
        EXPECT_LE(h, 1.0f);
    }

    // Verify terrain has actual height variation (not flat)
    EXPECT_GT(maxVal - minVal, 0.1f);

    float centerHeight = comp.data->SampleHeight(0.0f, 0.0f);
    EXPECT_GE(centerHeight, 0.0f);
    EXPECT_LE(centerHeight, 50.0f);

    glm::vec3 normal = comp.data->SampleNormal(0.0f, 0.0f);
    EXPECT_NEAR(glm::length(normal), 1.0f, 1e-3f);
}

TEST(TerrainSystem, ChunkedMeshBuilding)
{
    TerrainComponent comp;
    comp.resolution = 65;
    comp.worldSize = 100.0f;
    comp.maxHeight = 50.0f;
    comp.chunkCount = 2;

    TerrainBuilder::GenerateProcedural(comp, 0.03f, 42);

    TerrainRenderer renderer;
    renderer.RebuildMesh(nullptr, comp, glm::mat4(1.0f));

    EXPECT_TRUE(comp.gpuInitialized);
    EXPECT_EQ(comp.chunks.size(), 4u);

    for (const auto &chunk : comp.chunks)
    {
        EXPECT_FALSE(chunk.gpuBuffersDirty);
        EXPECT_NE(chunk.primitive, nullptr);
        EXPECT_GT(chunk.primitive->vertices.size(), 0u);
        EXPECT_GT(chunk.primitive->indices.size(), 0u);
        EXPECT_LE(chunk.bounds.min.x, chunk.bounds.max.x);
        EXPECT_LE(chunk.bounds.min.y, chunk.bounds.max.y);
        EXPECT_LE(chunk.bounds.min.z, chunk.bounds.max.z);
    }
}

TEST(TerrainSystem, GPUResourceRelease)
{
    TerrainComponent comp;
    comp.resolution = 16;
    comp.chunkCount = 1;

    comp.data = CreateRef<TerrainData>();
    comp.data->InitFlat(16, 50.0f, 10.0f);

    TerrainRenderer renderer;
    renderer.RebuildMesh(nullptr, comp, glm::mat4(1.0f));

    EXPECT_EQ(comp.chunks.size(), 1u);
    EXPECT_NE(comp.chunks[0].primitive, nullptr);

    comp.ReleaseGPU();

    EXPECT_EQ(comp.chunks.size(), 0u);
    EXPECT_FALSE(comp.gpuInitialized);
}

TEST(TerrainSystem, SerializationRoundtrip)
{
    TerrainComponent comp;
    comp.resolution = 64;
    comp.worldSize = 200.0f;
    comp.maxHeight = 75.0f;
    comp.chunkCount = 4;
    comp.lodLevels = 2;

    TerrainBuilder::GenerateProcedural(comp, 0.02f, 999);

    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Terrain" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Resolution" << YAML::Value << comp.resolution;
    out << YAML::Key << "WorldSize" << YAML::Value << comp.worldSize;
    out << YAML::Key << "MaxHeight" << YAML::Value << comp.maxHeight;
    out << YAML::Key << "ChunkCount" << YAML::Value << comp.chunkCount;
    out << YAML::Key << "LodLevels" << YAML::Value << comp.lodLevels;
    out << YAML::Key << "HeightmapHandle" << YAML::Value << static_cast<uint64_t>(comp.heightmapHandle);
    out << YAML::EndMap;
    out << YAML::EndMap;

    std::string yamlStr = out.c_str();
    YAML::Node rootNode = YAML::Load(yamlStr);
    ASSERT_TRUE(rootNode.IsDefined());

    YAML::Node terrainNode = rootNode["Terrain"];
    ASSERT_TRUE(terrainNode.IsDefined());

    EXPECT_EQ(terrainNode["Resolution"].as<uint32_t>(), 64u);
    EXPECT_FLOAT_EQ(terrainNode["WorldSize"].as<float>(), 200.0f);
    EXPECT_FLOAT_EQ(terrainNode["MaxHeight"].as<float>(), 75.0f);
    EXPECT_EQ(terrainNode["ChunkCount"].as<uint32_t>(), 4u);
    EXPECT_EQ(terrainNode["LodLevels"].as<uint32_t>(), 2u);
}
