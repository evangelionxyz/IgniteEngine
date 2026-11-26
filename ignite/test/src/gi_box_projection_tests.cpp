// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gi/gi_asset_io.hpp"
#include "ignite/graphics/gi/gi_baker.hpp"
#include "ignite/graphics/gi/irradiance_volume.hpp"
#include "ignite/graphics/gi/reflection_probe_set.hpp"
#include "ignite/graphics/gi/sh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>
#include <string>

using namespace ignite;

namespace
{
    AssetHandle MakeBoxMat(AssetManager *assetManager, uint64_t id, const glm::vec3 &albedo)
    {
        Ref<Material> mat = CreateRef<Material>();
        mat->name = "ReflBoxMat_" + std::to_string(id);
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

    Entity AddRoomBox(Scene *scene, const std::string &name, const glm::vec3 &pos, const glm::vec3 &scale, AssetHandle matHandle)
    {
        Entity e = SceneManager::CreateEntity(scene, name, EntityType_Node);
        auto &tr = e.GetComponent<TransformComponent>();
        tr.local.translation = pos;
        tr.local.scale = scale;
        tr.world = tr.local;

        auto &smc = e.AddComponent<StaticMeshComponent>();
        const AssetHandle baseCubeHandle = Renderer::GetDefaultMeshHandle(EMeshType::CUBE);
        auto baseCube = AssetManager::GetInstance()->GetAsset<StaticMesh>(baseCubeHandle);
        Ref<StaticMesh> uniqueCube = StaticMesh::Create();
        if (baseCube)
        {
            for (const auto &inst : baseCube->GetMeshInstances())
            {
                auto copyInst = StaticMeshInstance::Create(inst->GetName(), inst->GetPrimitive());
                copyInst->local = inst->local;
                copyInst->global = inst->global;
                copyInst->localAABB = inst->localAABB;
                copyInst->SetMaterial(matHandle);
                uniqueCube->AddMeshInstance(copyInst);
            }
            uniqueCube->CalculateLocalAABB();
        }
        static uint64_t s_TestMeshId = 900200000ULL;
        const AssetHandle meshHandle(++s_TestMeshId);
        AssetManager::GetInstance()->AssignAsset(meshHandle, uniqueCube);
        smc.handle = meshHandle;
        return e;
    }
}

TEST(GIBoxProjectionTests, AnalyticAxisAlignedAndRotatedOBBParallax)
{
    ReflectionProbe_GPUData axisProbe{};
    axisProbe.worldToLocal = glm::mat4(1.0f);
    axisProbe.localToWorld = glm::mat4(1.0f);
    axisProbe.boxMinAndSlice = glm::vec4(-4.0f, -2.0f, -4.0f, 0.0f);
    axisProbe.boxMaxAndBlend = glm::vec4( 4.0f,  2.0f,  4.0f, 1.0f);
    axisProbe.capturePosAndNorm = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

    const glm::vec3 P(2.0f, -2.0f, 0.0f);
    const glm::vec3 R = glm::normalize(glm::vec3(1.0f, 2.0f, 0.0f));
    const glm::vec3 projected = gi::BoxProjectOBB(P, R, axisProbe);

    // Ray hits ceiling y=+2 at t = 4 / R.y, so x = 2 + 4 * (1/2) = 4 -> worldHit = (4, 2, 0).
    // Vector from capturePos(0,1,0) is normalize(4, 1, 0).
    const glm::vec3 expected = glm::normalize(glm::vec3(4.0f, 1.0f, 0.0f));
    EXPECT_NEAR(projected.x, expected.x, 1e-4f);
    EXPECT_NEAR(projected.y, expected.y, 1e-4f);
    EXPECT_NEAR(projected.z, expected.z, 1e-4f);

    // 45-degree Y-rotated OBB probe centered at (10, 0, 0) with half-extents (4, 2, 4)
    const glm::quat rot45 = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 probeCenter(10.0f, 0.0f, 0.0f);
    const glm::mat4 localToWorld = glm::translate(glm::mat4(1.0f), probeCenter) * glm::mat4_cast(rot45);

    ReflectionProbe_GPUData rotProbe{};
    rotProbe.localToWorld = localToWorld;
    rotProbe.worldToLocal = glm::inverse(localToWorld);
    rotProbe.boxMinAndSlice = glm::vec4(-4.0f, -2.0f, -4.0f, 0.0f);
    rotProbe.boxMaxAndBlend = glm::vec4( 4.0f,  2.0f,  4.0f, 1.0f);
    rotProbe.capturePosAndNorm = glm::vec4(probeCenter, 1.0f);

    const glm::vec3 localRight = glm::normalize(glm::mat3_cast(rot45) * glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 rotOrigin = probeCenter + glm::mat3_cast(rot45) * glm::vec3(1.0f, -1.0f, 0.0f);
    const glm::vec3 rotProj = gi::BoxProjectOBB(rotOrigin, localRight, rotProbe);
    const glm::vec3 expectedRot = glm::normalize(glm::mat3_cast(rot45) * glm::vec3(4.0f, -1.0f, 0.0f));

    EXPECT_NEAR(rotProj.x, expectedRot.x, 1e-4f);
    EXPECT_NEAR(rotProj.y, expectedRot.y, 1e-4f);
    EXPECT_NEAR(rotProj.z, expectedRot.z, 1e-4f);
}

TEST(GIBoxProjectionTests, InfluenceBlendNormalizationAndSpecularOcclusion)
{
    ReflectionProbe_GPUData probe{};
    probe.worldToLocal = glm::mat4(1.0f);
    probe.localToWorld = glm::mat4(1.0f);
    probe.boxMaxAndBlend = glm::vec4(5.0f, 3.0f, 5.0f, 1.0f);
    probe.influenceMinAndPrio = glm::vec4(-5.0f, -3.0f, -5.0f, 0.0f);
    probe.influenceMaxAndInt = glm::vec4( 5.0f,  3.0f,  5.0f, 1.0f);

    EXPECT_FLOAT_EQ(gi::ComputeProbeInfluenceWeight(glm::vec3(0.0f, 0.0f, 0.0f), probe), 1.0f);
    EXPECT_FLOAT_EQ(gi::ComputeProbeInfluenceWeight(glm::vec3(5.5f, 0.0f, 0.0f), probe), 0.0f);
    const float borderW = gi::ComputeProbeInfluenceWeight(glm::vec3(4.5f, 0.0f, 0.0f), probe);
    EXPECT_GT(borderW, 0.1f);
    EXPECT_LT(borderW, 0.9f);

    // Lagarde normalization suppresses glowing reflections in dark corners
    EXPECT_FLOAT_EQ(gi::ComputeProbeNormalizationFactor(glm::vec3(0.0f), glm::vec3(1.0f), 1.0f), 0.0f);
    EXPECT_NEAR(gi::ComputeProbeNormalizationFactor(glm::vec3(0.5f), glm::vec3(0.5f), 1.0f), 1.0f, 1e-5f);
    EXPECT_NEAR(gi::ComputeProbeNormalizationFactor(glm::vec3(10.0f), glm::vec3(1.0f), 1.0f), 2.0f, 1e-5f);

    // Specular occlusion stays in [0,1] and increases monotonically with AO
    const float so0 = gi::ComputeSpecularOcclusion(0.0f, 0.0f, 0.3f);
    const float soMid = gi::ComputeSpecularOcclusion(0.5f, 0.4f, 0.3f);
    const float so1 = gi::ComputeSpecularOcclusion(0.5f, 1.0f, 0.3f);
    EXPECT_FLOAT_EQ(so0, 0.0f);
    EXPECT_FLOAT_EQ(so1, 1.0f);
    EXPECT_GE(soMid, 0.0f);
    EXPECT_LE(soMid, 1.0f);
    EXPECT_LT(so0, soMid);
    EXPECT_LT(soMid, so1);
}

TEST(GIBoxProjectionTests, CornellRoomReflectionProbeBakeAndGPUCubeArray)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteReflectionProbeTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ProjectInfo info;
    info.name = "ReflectionProbeTest";
    info.rootDirectory = tempDir;
    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    AssetManager *assetManager = AssetManager::GetInstance();
    Ref<Scene> scene = CreateRef<Scene>(project.get());
    project->SetActiveScene(scene);

    const AssetHandle redMat   = MakeBoxMat(assetManager, 9500001ULL, glm::vec3(0.85f, 0.05f, 0.05f));
    const AssetHandle greenMat = MakeBoxMat(assetManager, 9500002ULL, glm::vec3(0.05f, 0.85f, 0.05f));
    const AssetHandle whiteMat = MakeBoxMat(assetManager, 9500003ULL, glm::vec3(0.8f, 0.8f, 0.8f));

    AddRoomBox(scene.get(), "Left",    glm::vec3(-2.6f, 0.0f, 0.0f), glm::vec3(0.4f, 5.6f, 5.6f), redMat);
    AddRoomBox(scene.get(), "Right",   glm::vec3( 2.6f, 0.0f, 0.0f), glm::vec3(0.4f, 5.6f, 5.6f), greenMat);
    AddRoomBox(scene.get(), "Floor",   glm::vec3( 0.0f,-2.6f, 0.0f), glm::vec3(5.6f, 0.4f, 5.6f), whiteMat);
    AddRoomBox(scene.get(), "Ceiling", glm::vec3( 0.0f, 2.6f, 0.0f), glm::vec3(5.6f, 0.4f, 5.6f), whiteMat);
    AddRoomBox(scene.get(), "Back",    glm::vec3( 0.0f, 0.0f, 2.6f), glm::vec3(5.6f, 5.6f, 0.4f), whiteMat);
    AddRoomBox(scene.get(), "Front",   glm::vec3( 0.0f, 0.0f,-2.6f), glm::vec3(5.6f, 5.6f, 0.4f), whiteMat);

    {
        Entity pt = SceneManager::CreateEntity(scene.get(), "Light", EntityType_Node);
        pt.GetComponent<TransformComponent>().local.translation = glm::vec3(0.0f, 1.8f, 0.0f);
        pt.GetComponent<TransformComponent>().world = pt.GetComponent<TransformComponent>().local;
        auto &pl = pt.AddComponent<PointLightComponent>();
        pl.color = glm::vec4(1.0f);
        pl.intensity = 16.0f;
        pl.range = 12.0f;
        pl.enabled = true;
        pl.bakeIndirect = true;
    }

    Entity volEntity = SceneManager::CreateEntity(scene.get(), "RoomGIVolume", EntityType_Node);
    auto &vol = volEntity.AddComponent<IrradianceVolumeComponent>();
    vol.extents = glm::vec3(2.8f);
    vol.spacing = 1.4f;
    vol.bounces = 1;
    vol.captureResolution = 32;

    GIBaker baker;
    IrradianceVolumeData giData;
    ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), assetManager, volEntity, giData));
    Ref<IrradianceVolume> gpuVol = CreateRef<IrradianceVolume>();
    ASSERT_TRUE(gpuVol->UploadFromData(giData));

    Entity rpEntity = SceneManager::CreateEntity(scene.get(), "RoomReflectionProbe", EntityType_Node);
    auto &rpComp = rpEntity.AddComponent<ReflectionProbeComponent>();
    rpComp.influenceExtents = glm::vec3(3.0f);
    rpComp.projectionExtents = glm::vec3(2.4f);
    rpComp.resolution = 64;
    rpComp.bakeHash = 0xDEADBEEF1234ULL;

    std::vector<std::filesystem::path> savedPaths;
    const std::filesystem::path bakedDir = tempDir / "Baked" / "Reflection";
    ASSERT_TRUE(baker.BakeReflectionProbes(scene.get(), assetManager, gpuVol.get(), bakedDir, &savedPaths));
    ASSERT_EQ(savedPaths.size(), 1u);
    EXPECT_TRUE(std::filesystem::exists(savedPaths[0]));
    EXPECT_FALSE(rpComp.bakedAssetPath.empty());

    ReflectionProbeData loadedProbe;
    ASSERT_TRUE(gi_io::LoadReflectionProbe(savedPaths[0], loadedProbe));
    EXPECT_EQ(loadedProbe.resolution, 64u);
    EXPECT_EQ(loadedProbe.mipCount, 6u);
    EXPECT_GT(loadedProbe.captureSH[0].r + loadedProbe.captureSH[0].g + loadedProbe.captureSH[0].b, 0.1f);

    ReflectionProbeSet probeSet;
    ASSERT_TRUE(probeSet.BuildFromScene(scene.get(), project->GetDirectory()));
    EXPECT_TRUE(probeSet.IsValid());
    EXPECT_EQ(probeSet.GetProbeCount(), 1u);
    EXPECT_EQ(probeSet.GetResolution(), 64u);
    EXPECT_EQ(probeSet.GetMipCount(), 6u);

    GI_GPUData gpuGI{};
    probeSet.FillGIGPUData(gpuGI);
    EXPECT_GE(gpuGI.reflectionCubeArrayIndex, 0);
    EXPECT_EQ(gpuGI.reflectionProbeCount, 1);
    EXPECT_EQ(gpuGI.reflectionMipCount, 6);

    probeSet.Clear();
    gpuVol.reset();
    project->SetActiveScene(nullptr);
    scene.reset();
    assetManager->Reset();
    Project::CloseActive();
    std::filesystem::remove_all(tempDir, ec);
}

