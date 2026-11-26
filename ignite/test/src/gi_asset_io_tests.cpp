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
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace ignite;

namespace
{
    AssetHandle AssignSolidMaterial(AssetManager *assetManager, uint64_t id,
        const glm::vec3 &albedo, const glm::vec3 &emissive = glm::vec3(0.0f))
    {
        Ref<Material> mat = CreateRef<Material>();
        mat->name = "CornellMat_" + std::to_string(id);
        mat->gpuData.baseColorFactor = glm::vec4(albedo, 1.0f);
        mat->gpuData.baseWeight = 1.0f;
        mat->gpuData.emissiveFactor = glm::vec4(emissive, 1.0f);
        mat->gpuData.emissionLuminance = 1.0f;
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

    Entity AddCornellWall(Scene *scene, const std::string &name,
        const glm::vec3 &pos, const glm::vec3 &scale, AssetHandle matHandle)
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
        static uint64_t s_TestMeshId = 900100000ULL;
        const AssetHandle meshHandle(++s_TestMeshId);
        AssetManager::GetInstance()->AssignAsset(meshHandle, uniqueCube);
        smc.handle = meshHandle;
        return e;
    }

    gi::SHL2 UnpackProbeSH(const IrradianceVolumeData &data, int32_t ix, int32_t iy, int32_t iz, float &outValidity)
    {
        const size_t probeIdx = (static_cast<size_t>(iz) * data.dims.y + iy) * data.dims.x + ix;
        const size_t base = probeIdx * 4u;
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
}

TEST(GIAssetIOTests, IGIRoundTripAndValidation)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteIGIIOTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    IrradianceVolumeData src;
    src.dims = glm::ivec3(4, 3, 2);
    src.boundsMin = glm::vec3(-3.0f, 0.5f, -1.5f);
    src.spacing = 1.5f;
    src.shOrder = 2;
    src.visOctRes = 8;
    src.visAtlasCols = 6;
    src.visAtlasRows = 4;
    src.bakeHash = 0xCAFEBABE12345678ULL;

    const size_t probeCount = src.GetProbeCount();
    const size_t shFloats = probeCount * 4u;
    for (int t = 0; t < 7; ++t)
    {
        src.shHalfRGBA[t].resize(shFloats);
        for (size_t i = 0; i < shFloats; ++i)
        {
            src.shHalfRGBA[t][i] = glm::packHalf1x16(0.05f * static_cast<float>((t * 17 + i) % 31));
        }
    }

    src.probeOffsetsHalfRGBA.resize(shFloats);
    for (size_t i = 0; i < probeCount; ++i)
    {
        src.probeOffsetsHalfRGBA[i * 4u + 0u] = glm::packHalf1x16(0.1f);
        src.probeOffsetsHalfRGBA[i * 4u + 1u] = glm::packHalf1x16(-0.2f);
        src.probeOffsetsHalfRGBA[i * 4u + 2u] = glm::packHalf1x16(0.05f);
        src.probeOffsetsHalfRGBA[i * 4u + 3u] = glm::packHalf1x16((i % 3 == 0) ? 2.0f : 1.0f);
    }

    const size_t atlasTexels = static_cast<size_t>(src.GetVisibilityAtlasWidth()) * src.GetVisibilityAtlasHeight();
    src.visibilityAtlasHalfRG.resize(atlasTexels * 2u);
    for (size_t i = 0; i < atlasTexels; ++i)
    {
        src.visibilityAtlasHalfRG[i * 2u + 0u] = glm::packHalf1x16(2.5f);
        src.visibilityAtlasHalfRG[i * 2u + 1u] = glm::packHalf1x16(6.25f);
    }

    const std::filesystem::path igiPath = tempDir / "test_volume.igi";
    ASSERT_TRUE(gi_io::SaveIrradianceVolume(igiPath, src));

    IrradianceVolumeData dst;
    ASSERT_TRUE(gi_io::LoadIrradianceVolume(igiPath, dst));

    EXPECT_EQ(dst.dims, src.dims);
    EXPECT_EQ(dst.boundsMin, src.boundsMin);
    EXPECT_FLOAT_EQ(dst.spacing, src.spacing);
    EXPECT_EQ(dst.shOrder, src.shOrder);
    EXPECT_EQ(dst.visOctRes, src.visOctRes);
    EXPECT_EQ(dst.visAtlasCols, src.visAtlasCols);
    EXPECT_EQ(dst.visAtlasRows, src.visAtlasRows);
    EXPECT_EQ(dst.bakeHash, src.bakeHash);

    for (int t = 0; t < 7; ++t)
    {
        EXPECT_EQ(dst.shHalfRGBA[t], src.shHalfRGBA[t]);
    }
    EXPECT_EQ(dst.probeOffsetsHalfRGBA, src.probeOffsetsHalfRGBA);
    EXPECT_EQ(dst.visibilityAtlasHalfRG, src.visibilityAtlasHalfRG);

    // Corrupt header check
    {
        std::ofstream bad(igiPath, std::ios::binary | std::ios::in | std::ios::out);
        bad.seekp(0);
        bad.write("BAD!", 4);
    }
    IrradianceVolumeData invalidDst;
    EXPECT_FALSE(gi_io::LoadIrradianceVolume(igiPath, invalidDst));

    std::filesystem::remove_all(tempDir, ec);
}

TEST(GIAssetIOTests, IGCubeRoundTripAndValidation)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteIGCubeIOTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ReflectionProbeData src;
    src.resolution = 16;
    src.mipCount = 3;
    src.capturePos = glm::vec3(1.0f, 2.5f, -3.0f);
    src.boxMin = glm::vec3(-4.0f, 0.0f, -6.0f);
    src.boxMax = glm::vec3(6.0f, 5.0f, 2.0f);
    src.bakeHash = 0xDEADBEEF87654321ULL;
    for (int i = 0; i < 4; ++i)
    {
        src.captureSH[i] = glm::vec4(0.25f * static_cast<float>(i + 1), 0.5f, 0.75f, 1.0f);
    }

    src.mipFaceBytes.resize(6u * src.mipCount);
    for (uint32_t f = 0; f < 6u; ++f)
    {
        for (uint32_t m = 0; m < src.mipCount; ++m)
        {
            const uint32_t sz = std::max(1u, src.resolution >> m);
            auto &buf = src.mipFaceBytes[static_cast<size_t>(f) * src.mipCount + m];
            buf.resize(static_cast<size_t>(sz) * sz * 8u);
            for (size_t k = 0; k < buf.size(); ++k)
            {
                buf[k] = static_cast<uint8_t>((f * 31u + m * 13u + k) & 0xFFu);
            }
        }
    }

    const std::filesystem::path cubePath = tempDir / "test_probe.igcube";
    ASSERT_TRUE(gi_io::SaveReflectionProbe(cubePath, src));

    ReflectionProbeData dst;
    ASSERT_TRUE(gi_io::LoadReflectionProbe(cubePath, dst));

    EXPECT_EQ(dst.resolution, src.resolution);
    EXPECT_EQ(dst.mipCount, src.mipCount);
    EXPECT_EQ(dst.capturePos, src.capturePos);
    EXPECT_EQ(dst.boxMin, src.boxMin);
    EXPECT_EQ(dst.boxMax, src.boxMax);
    EXPECT_EQ(dst.bakeHash, src.bakeHash);
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(dst.captureSH[i], src.captureSH[i]);
    }
    EXPECT_EQ(dst.mipFaceBytes, src.mipFaceBytes);

    std::filesystem::remove_all(tempDir, ec);
}

TEST(GIAssetIOTests, CornellBoxBakeColorBleedingAndReload)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteCornellBakeTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ProjectInfo info;
    info.name = "CornellBakeTest";
    info.rootDirectory = tempDir;
    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    AssetManager *assetManager = AssetManager::GetInstance();
    ASSERT_NE(assetManager, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    project->SetActiveScene(scene);

    // Classic Cornell Box [-3..+3]^3 illuminated by a bright white interior point light
    // Left (-X): saturated Red wall, Right (+X): saturated Green wall, Top/Bottom/Back/Front: White walls
    const AssetHandle redMat   = AssignSolidMaterial(assetManager, 9300001ULL, glm::vec3(0.9f, 0.05f, 0.05f));
    const AssetHandle greenMat = AssignSolidMaterial(assetManager, 9300002ULL, glm::vec3(0.05f, 0.9f, 0.05f));
    const AssetHandle whiteMat = AssignSolidMaterial(assetManager, 9300003ULL, glm::vec3(0.85f, 0.85f, 0.85f));
    const AssetHandle lightMat = AssignSolidMaterial(assetManager, 9300004ULL, glm::vec3(1.0f), glm::vec3(8.0f, 8.0f, 8.0f));

    AddCornellWall(scene.get(), "LeftWallRed",    glm::vec3(-3.2f, 0.0f,  0.0f), glm::vec3(0.4f, 6.4f, 6.4f), redMat);
    AddCornellWall(scene.get(), "RightWallGreen", glm::vec3( 3.2f, 0.0f,  0.0f), glm::vec3(0.4f, 6.4f, 6.4f), greenMat);
    AddCornellWall(scene.get(), "FloorWhite",     glm::vec3( 0.0f,-3.2f,  0.0f), glm::vec3(6.4f, 0.4f, 6.4f), whiteMat);
    AddCornellWall(scene.get(), "CeilingWhite",   glm::vec3( 0.0f, 3.2f,  0.0f), glm::vec3(6.4f, 0.4f, 6.4f), whiteMat);
    AddCornellWall(scene.get(), "BackWallWhite",  glm::vec3( 0.0f, 0.0f,  3.2f), glm::vec3(6.4f, 6.4f, 0.4f), whiteMat);
    AddCornellWall(scene.get(), "FrontWallWhite", glm::vec3( 0.0f, 0.0f, -3.2f), glm::vec3(6.4f, 6.4f, 0.4f), whiteMat);
    AddCornellWall(scene.get(), "CeilingPanel",   glm::vec3( 0.0f, 2.95f, 0.0f), glm::vec3(1.5f, 0.1f, 1.5f), lightMat);

    // Add interior point light to drive strong diffuse bounce off colored walls
    {
        Entity ptEntity = SceneManager::CreateEntity(scene.get(), "CeilingLight", EntityType_Node);
        auto &ptTr = ptEntity.GetComponent<TransformComponent>();
        ptTr.local.translation = glm::vec3(0.0f, 2.2f, 0.0f);
        ptTr.world = ptTr.local;

        auto &pt = ptEntity.AddComponent<PointLightComponent>();
        pt.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        pt.intensity = 18.0f;
        pt.range = 15.0f;
        pt.enabled = true;
        pt.bakeIndirect = true;
    }

    // Add 3x3x3 Irradiance Volume spanning [-2..+2]^3 inside the Cornell Box (all probes strictly inside room)
    Entity volEntity = SceneManager::CreateEntity(scene.get(), "CornellGIVolume", EntityType_Node);
    {
        auto &vol = volEntity.AddComponent<IrradianceVolumeComponent>();
        vol.extents = glm::vec3(2.0f, 2.0f, 2.0f);
        vol.spacing = 2.0f; // 3x3x3 probes at {-2, 0, +2}^3
        vol.bounces = 1;
        vol.captureResolution = 32;
        vol.runtimeMode = 1;
        vol.bakeHash = 0x11223344ULL;
    }

    const std::filesystem::path igiPath = tempDir / "Baked" / "Cornell" / "cornell.igi";
    IrradianceVolumeData baked;
    GIBaker baker;
    ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), assetManager, volEntity, baked, igiPath));
    EXPECT_EQ(baked.dims, glm::ivec3(3, 3, 3));
    EXPECT_TRUE(std::filesystem::exists(igiPath));

    // Reload from .igi into GPU IrradianceVolume and verify bindless textures & GI_GPUData
    Ref<IrradianceVolume> gpuVolume = CreateRef<IrradianceVolume>();
    ASSERT_TRUE(gpuVolume->LoadFromFile(igiPath));
    EXPECT_TRUE(gpuVolume->IsValid());

    GI_GPUData gpuGI{};
    gpuVolume->FillGIGPUData(gpuGI, &volEntity.GetComponent<IrradianceVolumeComponent>());
    EXPECT_EQ(gpuGI.gridDimsAndOrder, glm::ivec4(3, 3, 3, 2));
    EXPECT_FLOAT_EQ(gpuGI.biasAndFade.w, 1.0f);
    EXPECT_GE(gpuGI.shTextureIndices0.x, 0);
    EXPECT_GE(gpuGI.shTextureIndices1.z, 0);

    // Check color bleeding on probes near left red wall (ix=0, looking along +X) vs right green wall (ix=2, looking along -X)
    float leftVal = 0.0f;
    float centerVal = 0.0f;
    float rightVal = 0.0f;
    const gi::SHL2 shLeft = UnpackProbeSH(gpuVolume->GetData(), 0, 1, 1, leftVal);
    const gi::SHL2 shCenter = UnpackProbeSH(gpuVolume->GetData(), 1, 1, 1, centerVal);
    const gi::SHL2 shRight = UnpackProbeSH(gpuVolume->GetData(), 2, 1, 1, rightVal);

    EXPECT_GT(leftVal, 0.5f);
    EXPECT_GT(centerVal, 0.5f);
    EXPECT_GT(rightVal, 0.5f);

    // At left probe (-2, 0, 0), surface facing -X (toward the red left wall) receives red bounced light;
    // at right probe (+2, 0, 0), surface facing +X (toward the green right wall) receives green bounced light.
    const glm::vec3 irrLeftFromRedWall = gi::EvalSHIrradiance(shLeft, glm::vec3(-1.0f, 0.0f, 0.0f));
    const glm::vec3 irrRightFromGreenWall = gi::EvalSHIrradiance(shRight, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 irrCenterDown = gi::EvalSHIrradiance(shCenter, glm::vec3(0.0f, -1.0f, 0.0f));

    EXPECT_GT(irrLeftFromRedWall.r, 0.05f);
    EXPECT_GT(irrLeftFromRedWall.r, irrLeftFromRedWall.g * 1.3f);

    EXPECT_GT(irrRightFromGreenWall.g, 0.05f);
    EXPECT_GT(irrRightFromGreenWall.g, irrRightFromGreenWall.r * 1.3f);

    EXPECT_GT(irrCenterDown.r, 0.05f);
    EXPECT_GT(irrCenterDown.g, 0.05f);
    EXPECT_GT(irrCenterDown.b, 0.05f);

    gpuVolume.reset();
    project->SetActiveScene(nullptr);
    scene.reset();
    assetManager->Reset();
    Project::CloseActive();
    std::filesystem::remove_all(tempDir, ec);
}
