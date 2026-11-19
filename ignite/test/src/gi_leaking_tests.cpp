// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gi/gi_asset_io.hpp"
#include "ignite/graphics/gi/gi_baker.hpp"
#include "ignite/graphics/gi/irradiance_volume.hpp"
#include "ignite/graphics/gi/sh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <glm/gtc/packing.hpp>
#include <filesystem>
#include <string>

using namespace ignite;

namespace
{
    AssetHandle CreateMat(AssetManager *assetManager, uint64_t id, const glm::vec3 &albedo)
    {
        Ref<Material> mat = CreateRef<Material>();
        mat->name = "LeakTestMat_" + std::to_string(id);
        mat->gpuData.baseColorFactor = glm::vec4(albedo, 1.0f);
        mat->gpuData.baseWeight = 1.0f;
        mat->gpuData.metallicFactor = 0.0f;
        mat->gpuData.roughnessFactor = 1.0f;
        mat->gpuData.specularWeight = 0.0f;
        mat->SetType(MaterialType::Opaque);
        mat->SetReadyFlag(true);
        mat->SetDirtyFlag(true);
        mat->InvalidateBindingSet();

        const AssetHandle handle(id);
        assetManager->AssignAsset(handle, mat);
        return handle;
    }

    Entity AddBox(Scene *scene, const std::string &name, const glm::vec3 &pos, const glm::vec3 &scale, AssetHandle matHandle)
    {
        Entity e = SceneManager::CreateEntity(scene, name, EntityType_Node);
        auto &tr = e.GetComponent<TransformComponent>();
        tr.local.translation = pos;
        tr.local.scale = scale;
        tr.world = tr.local;

        auto &smc = e.AddComponent<StaticMeshComponent>();
        smc.handle = Renderer::GetDefaultMeshHandle(EMeshType::CUBE);
        smc.overrideMaterials[0] = matHandle;
        return e;
    }

    gi::SHL2 UnpackSHAt(const IrradianceVolumeData &data, int32_t ix, int32_t iy, int32_t iz, float &outValidity)
    {
        const size_t idx = (static_cast<size_t>(iz) * data.dims.y + iy) * data.dims.x + ix;
        const size_t base = idx * 4u;
        glm::vec4 packed[7]{};
        for (int t = 0; t < 7; ++t)
        {
            packed[t] = glm::vec4(
                glm::unpackHalf1x16(data.shHalfRGBA[t][base + 0u]),
                glm::unpackHalf1x16(data.shHalfRGBA[t][base + 1u]),
                glm::unpackHalf1x16(data.shHalfRGBA[t][base + 2u]),
                glm::unpackHalf1x16(data.shHalfRGBA[t][base + 3u]));
        }
        gi::SHL2 sh{};
        gi::UnpackSHL2From7RGBA(packed, sh, outValidity);
        return sh;
    }

    glm::vec4 UnpackOffsetAndState(const IrradianceVolumeData &data, int32_t ix, int32_t iy, int32_t iz)
    {
        const size_t idx = (static_cast<size_t>(iz) * data.dims.y + iy) * data.dims.x + ix;
        const size_t base = idx * 4u;
        if (base + 3u >= data.probeOffsetsHalfRGBA.size())
        {
            return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }
        return glm::vec4(
            glm::unpackHalf1x16(data.probeOffsetsHalfRGBA[base + 0u]),
            glm::unpackHalf1x16(data.probeOffsetsHalfRGBA[base + 1u]),
            glm::unpackHalf1x16(data.probeOffsetsHalfRGBA[base + 2u]),
            glm::unpackHalf1x16(data.probeOffsetsHalfRGBA[base + 3u]));
    }
}

TEST(GILeakingTests, MultiBounceIncreasesIndirectEnergy)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteMultiBounceTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ProjectInfo info;
    info.name = "MultiBounceTest";
    info.rootDirectory = tempDir;
    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    AssetManager *assetManager = AssetManager::GetInstance();
    Ref<Scene> scene = CreateRef<Scene>(project.get());
    project->SetActiveScene(scene);

    const AssetHandle whiteMat = CreateMat(assetManager, 9400001ULL, glm::vec3(0.85f, 0.85f, 0.85f));
    // Inner room walls sit at +-2.4m (center +-2.6m, thickness 0.4m) so they lie inside the [-2.8..+2.8]^3 volume
    AddBox(scene.get(), "Left",    glm::vec3(-2.6f, 0.0f, 0.0f), glm::vec3(0.4f, 5.6f, 5.6f), whiteMat);
    AddBox(scene.get(), "Right",   glm::vec3( 2.6f, 0.0f, 0.0f), glm::vec3(0.4f, 5.6f, 5.6f), whiteMat);
    AddBox(scene.get(), "Floor",   glm::vec3( 0.0f,-2.6f, 0.0f), glm::vec3(5.6f, 0.4f, 5.6f), whiteMat);
    AddBox(scene.get(), "Ceiling", glm::vec3( 0.0f, 2.6f, 0.0f), glm::vec3(5.6f, 0.4f, 5.6f), whiteMat);
    AddBox(scene.get(), "Back",    glm::vec3( 0.0f, 0.0f, 2.6f), glm::vec3(5.6f, 5.6f, 0.4f), whiteMat);
    AddBox(scene.get(), "Front",   glm::vec3( 0.0f, 0.0f,-2.6f), glm::vec3(5.6f, 5.6f, 0.4f), whiteMat);

    {
        Entity pt = SceneManager::CreateEntity(scene.get(), "RoomLight", EntityType_Node);
        pt.GetComponent<TransformComponent>().local.translation = glm::vec3(0.0f, 1.2f, 0.0f);
        pt.GetComponent<TransformComponent>().world = pt.GetComponent<TransformComponent>().local;
        auto &pl = pt.AddComponent<PointLightComponent>();
        pl.color = glm::vec4(1.0f, 0.95f, 0.9f, 1.0f);
        pl.intensity = 15.0f;
        pl.range = 12.0f;
        pl.enabled = true;
        pl.bakeIndirect = true;
    }

    Entity volEntity = SceneManager::CreateEntity(scene.get(), "GIVolume", EntityType_Node);
    auto &vol = volEntity.AddComponent<IrradianceVolumeComponent>();
    vol.extents = glm::vec3(2.8f);
    vol.spacing = 1.4f; // 5x5x5 probes at {-2.8, -1.4, 0, +1.4, +2.8}^3; indices 1..3 are strictly interior
    vol.captureResolution = 32;

    GIBaker baker;
    IrradianceVolumeData data1Bounce;
    vol.bounces = 1;
    ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), assetManager, volEntity, data1Bounce));

    IrradianceVolumeData data3Bounce;
    vol.bounces = 3;
    ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), assetManager, volEntity, data3Bounce));

    auto avgInteriorL0 = [](const IrradianceVolumeData &data)
    {
        float sum = 0.0f;
        uint32_t count = 0;
        for (int32_t z = 1; z < data.dims.z - 1; ++z)
        {
            for (int32_t y = 1; y < data.dims.y - 1; ++y)
            {
                for (int32_t x = 1; x < data.dims.x - 1; ++x)
                {
                    float val = 0.0f;
                    gi::SHL2 sh = UnpackSHAt(data, x, y, z, val);
                    if (val > 0.5f)
                    {
                        sum += (sh.c[0].r + sh.c[0].g + sh.c[0].b);
                        ++count;
                    }
                }
            }
        }
        return count > 0 ? sum / static_cast<float>(count) : 0.0f;
    };

    const float e1 = avgInteriorL0(data1Bounce);
    const float e3 = avgInteriorL0(data3Bounce);
    EXPECT_GT(e1, 0.05f);
    EXPECT_GT(e3, e1 * 1.15f);

    project->SetActiveScene(nullptr);
    scene.reset();
    assetManager->Reset();
    Project::CloseActive();
    std::filesystem::remove_all(tempDir, ec);
}

TEST(GILeakingTests, ProbeRelocationEscapesSolidWall)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteRelocationTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ProjectInfo info;
    info.name = "RelocationTest";
    info.rootDirectory = tempDir;
    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    AssetManager *assetManager = AssetManager::GetInstance();
    Ref<Scene> scene = CreateRef<Scene>(project.get());
    project->SetActiveScene(scene);

    const AssetHandle wallMat = CreateMat(assetManager, 9400002ULL, glm::vec3(0.75f, 0.75f, 0.75f));
    // Solid block around origin: half-extent = (0.35, 3.0, 3.0) -> overlaps center probe (0,0,0) with nearest exit at 0.35m < 0.45*2.0
    AddBox(scene.get(), "CenterPillar", glm::vec3(0.05f, 0.0f, 0.0f), glm::vec3(0.7f, 6.0f, 6.0f), wallMat);
    // Floor so surrounding probes have grounded geometry
    AddBox(scene.get(), "Floor", glm::vec3(0.0f, -3.2f, 0.0f), glm::vec3(10.0f, 0.4f, 10.0f), wallMat);

    {
        Entity sun = SceneManager::CreateEntity(scene.get(), "Sun", EntityType_Node);
        sun.GetComponent<TransformComponent>().local.rotation = glm::quat(glm::vec3(-0.7f, 0.3f, 0.0f));
        sun.GetComponent<TransformComponent>().world = sun.GetComponent<TransformComponent>().local;
        auto &dl = sun.AddComponent<DirectionalLightComponent>();
        dl.color = glm::vec4(1.0f);
        dl.intensity = 3.0f;
        dl.bakeIndirect = true;
    }

    Entity volEntity = SceneManager::CreateEntity(scene.get(), "RelocVolume", EntityType_Node);
    auto &vol = volEntity.AddComponent<IrradianceVolumeComponent>();
    vol.extents = glm::vec3(2.0f);
    vol.spacing = 2.0f; // Center probe (1,1,1) is at (0,0,0), inside CenterPillar [-0.30..+0.40]
    vol.bounces = 1;
    vol.captureResolution = 32;

    GIBaker baker;
    IrradianceVolumeData baked;
    ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), assetManager, volEntity, baked));

    const glm::vec4 centerOffState = UnpackOffsetAndState(baked, 1, 1, 1);
    EXPECT_NEAR(centerOffState.w, 2.0f, 0.05f); // State == 2 (Relocated)
    const float offLen = glm::length(glm::vec3(centerOffState));
    EXPECT_GT(offLen, 0.25f);
    EXPECT_LE(offLen, 0.45f * baked.spacing + 0.02f);

    uint32_t validOrRelocated = 0;
    const uint32_t total = baked.GetProbeCount();
    for (int32_t z = 0; z < baked.dims.z; ++z)
    {
        for (int32_t y = 0; y < baked.dims.y; ++y)
        {
            for (int32_t x = 0; x < baked.dims.x; ++x)
            {
                float val = 0.0f;
                UnpackSHAt(baked, x, y, z, val);
                if (val > 0.5f)
                {
                    ++validOrRelocated;
                }
            }
        }
    }
    EXPECT_EQ(validOrRelocated, total);

    project->SetActiveScene(nullptr);
    scene.reset();
    assetManager->Reset();
    Project::CloseActive();
    std::filesystem::remove_all(tempDir, ec);
}

TEST(GILeakingTests, ThinWallVisibilityAtlasAndLeakResistantSampling)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteThinWallTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ProjectInfo info;
    info.name = "ThinWallTest";
    info.rootDirectory = tempDir;
    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    AssetManager *assetManager = AssetManager::GetInstance();
    Ref<Scene> scene = CreateRef<Scene>(project.get());
    project->SetActiveScene(scene);

    const AssetHandle matteWhite = CreateMat(assetManager, 9400003ULL, glm::vec3(0.8f, 0.8f, 0.8f));

    // Outer closed building spanning [-4.2..+4.2, -3.2..+3.2, -3.2..+3.2]
    AddBox(scene.get(), "OuterLeft",    glm::vec3(-4.2f, 0.0f, 0.0f), glm::vec3(0.4f, 6.4f, 6.4f), matteWhite);
    AddBox(scene.get(), "OuterRight",   glm::vec3( 4.2f, 0.0f, 0.0f), glm::vec3(0.4f, 6.4f, 6.4f), matteWhite);
    AddBox(scene.get(), "OuterFloor",   glm::vec3( 0.0f,-3.2f, 0.0f), glm::vec3(8.8f, 0.4f, 6.4f), matteWhite);
    AddBox(scene.get(), "OuterCeiling", glm::vec3( 0.0f, 3.2f, 0.0f), glm::vec3(8.8f, 0.4f, 6.4f), matteWhite);
    AddBox(scene.get(), "OuterBack",    glm::vec3( 0.0f, 0.0f, 3.2f), glm::vec3(8.8f, 6.4f, 0.4f), matteWhite);
    AddBox(scene.get(), "OuterFront",   glm::vec3( 0.0f, 0.0f,-3.2f), glm::vec3(8.8f, 6.4f, 0.4f), matteWhite);

    // Thin 25cm interior dividing wall at x = 0 overlapping outer shell slightly so zero shadow-map corner cracks occur
    AddBox(scene.get(), "ThinDivider",  glm::vec3( 0.0f, 0.0f, 0.0f), glm::vec3(0.25f, 6.8f, 6.8f), matteWhite);

    // Bright point light ONLY in the left room (-2, 1, 0), range 10m overlaps the right room
    {
        Entity pt = SceneManager::CreateEntity(scene.get(), "BrightRoomLight", EntityType_Node);
        pt.GetComponent<TransformComponent>().local.translation = glm::vec3(-2.0f, 1.0f, 0.0f);
        pt.GetComponent<TransformComponent>().world = pt.GetComponent<TransformComponent>().local;
        auto &pl = pt.AddComponent<PointLightComponent>();
        pl.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        pl.intensity = 25.0f;
        pl.range = 10.0f;
        pl.enabled = true;
        pl.bakeIndirect = true;
    }

    Entity volEntity = SceneManager::CreateEntity(scene.get(), "TwoRoomVolume", EntityType_Node);
    auto &vol = volEntity.AddComponent<IrradianceVolumeComponent>();
    vol.extents = glm::vec3(2.25f, 1.5f, 1.5f);
    vol.spacing = 1.5f; // dims = 4x3x3 -> ix=0 (-2.25) & ix=1 (-0.75) bright room, ix=2 (+0.75) & ix=3 (+2.25) dark room
    vol.bounces = 2;
    const std::filesystem::path igiPath = tempDir / "Baked" / "thin_wall.igi";
    GIBaker baker;
    IrradianceVolumeData baked;
    ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), assetManager, volEntity, baked, igiPath));

    EXPECT_TRUE((baked.flags & IrradianceVolumeData::kFlagHasVisibilityAtlas) != 0u);
    EXPECT_TRUE((baked.flags & IrradianceVolumeData::kFlagHasProbeOffsets) != 0u);
    EXPECT_GT(baked.GetVisibilityAtlasWidth(), 0u);
    EXPECT_GT(baked.GetVisibilityAtlasHeight(), 0u);
    EXPECT_EQ(baked.visibilityAtlasHalfRG.size(),
        static_cast<size_t>(baked.GetVisibilityAtlasWidth()) * baked.GetVisibilityAtlasHeight() * 2u);

    Ref<IrradianceVolume> gpuVol = CreateRef<IrradianceVolume>();
    ASSERT_TRUE(gpuVol->LoadFromFile(igiPath));
    EXPECT_TRUE(gpuVol->IsValid());

    GI_GPUData giData{};
    gpuVol->FillGIGPUData(giData, &vol);
    EXPECT_GE(giData.shTextureIndices1.w, 0);
    EXPECT_GE(giData.visibilityParams.x, 0);
    EXPECT_EQ(giData.giMode, 2);

    // Bright room probe (ix=0, x=-2.25) should have strong irradiance;
    // dark room probes (ix=2 at +0.75, ix=3 at +2.25) must remain nearly dark (local bake shadows block light across thin wall).
    float brightVal = 0.0f;
    float darkNearVal = 0.0f;
    float darkFarVal = 0.0f;
    const gi::SHL2 shBright = UnpackSHAt(gpuVol->GetData(), 0, 1, 1, brightVal);
    const gi::SHL2 shDarkNear = UnpackSHAt(gpuVol->GetData(), 2, 1, 1, darkNearVal);
    const gi::SHL2 shDarkFar = UnpackSHAt(gpuVol->GetData(), 3, 1, 1, darkFarVal);

    EXPECT_GT(brightVal, 0.5f);
    EXPECT_GT(darkNearVal, 0.5f);
    EXPECT_GT(darkFarVal, 0.5f);

    const float brightLum = glm::dot(gi::EvalSHIrradiance(shBright, glm::vec3(1.0f, 0.0f, 0.0f)), glm::vec3(1.0f / 3.0f));
    const float darkNearLum = glm::dot(gi::EvalSHIrradiance(shDarkNear, glm::vec3(-1.0f, 0.0f, 0.0f)), glm::vec3(1.0f / 3.0f));
    const float darkFarLum = glm::dot(gi::EvalSHIrradiance(shDarkFar, glm::vec3(-1.0f, 0.0f, 0.0f)), glm::vec3(1.0f / 3.0f));

    EXPECT_GT(brightLum, 0.2f);
    EXPECT_LT(darkNearLum, brightLum * 0.08f);
    EXPECT_LT(darkFarLum, brightLum * 0.05f);

    gpuVol.reset();
    project->SetActiveScene(nullptr);
    scene.reset();
    assetManager->Reset();
    Project::CloseActive();
    std::filesystem::remove_all(tempDir, ec);
}

