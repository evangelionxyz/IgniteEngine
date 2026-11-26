// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/gi/scene_capture_renderer.hpp"
#include "ignite/graphics/gi/ibl_prefilter.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <glm/gtc/packing.hpp>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>

using namespace ignite;

namespace
{
    glm::vec4 SampleRGBA16FCenter(const std::vector<uint8_t> &bytes, uint32_t rowPitch, uint32_t width, uint32_t height)
    {
        const uint32_t cx = width / 2u;
        const uint32_t cy = height / 2u;
        const size_t byteOffset = static_cast<size_t>(cy) * static_cast<size_t>(rowPitch) + static_cast<size_t>(cx) * 4u * sizeof(uint16_t);
        const auto *px = reinterpret_cast<const uint16_t *>(bytes.data() + byteOffset);
        return glm::vec4(
            glm::unpackHalf1x16(px[0]),
            glm::unpackHalf1x16(px[1]),
            glm::unpackHalf1x16(px[2]),
            glm::unpackHalf1x16(px[3]));
    }

    AssetHandle RegisterEmissiveMaterial(AssetManager *assetManager, uint64_t rawHandle, const glm::vec3 &emissiveRgb)
    {
        Ref<Material> mat = CreateRef<Material>();
        mat->name = "EmissiveTestMat";
        mat->gpuData.baseColorFactor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        mat->gpuData.baseWeight = 0.0f;
        mat->gpuData.emissiveFactor = glm::vec4(emissiveRgb, 1.0f);
        mat->gpuData.emissionLuminance = 1.0f;
        mat->gpuData.metallicFactor = 0.0f;
        mat->gpuData.roughnessFactor = 1.0f;
        mat->SetType(MaterialType::Opaque);
        mat->SetReadyFlag(true);
        mat->SetDirtyFlag(true);
        mat->InvalidateBindingSet();

        const AssetHandle handle(rawHandle);
        assetManager->AssignAsset(handle, mat);
        return handle;
    }

    Entity CreateBoxEntity(Scene *scene, const std::string &name, const glm::vec3 &pos, const glm::vec3 &scale, AssetHandle matHandle)
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
        static uint64_t s_TestMeshId = 900300000ULL;
        const AssetHandle meshHandle(++s_TestMeshId);
        AssetManager::GetInstance()->AssignAsset(meshHandle, uniqueCube);
        smc.handle = meshHandle;
        return e;
    }
}

TEST(GICaptureTests, CubemapCaptureOrientationAndBackface)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGICaptureOrientationTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ProjectInfo info;
    info.name = "GICaptureOrientationTest";
    info.rootDirectory = tempDir;
    Ref<Project> project = Project::Create(info);
    ASSERT_NE(project, nullptr);

    AssetManager *assetManager = AssetManager::GetInstance();
    ASSERT_NE(assetManager, nullptr);

    Ref<Scene> scene = CreateRef<Scene>(project.get());
    project->SetActiveScene(scene);

    // 1. Place 6 colored emissive walls facing the origin along +X, -X, +Y, -Y, +Z, -Z
    const std::array<glm::vec3, 6> expectedColors = {
        glm::vec3(3.0f, 0.2f, 0.2f), // +X: bright red (tests linear HDR > 1.0)
        glm::vec3(0.2f, 2.5f, 2.5f), // -X: cyan
        glm::vec3(0.2f, 3.0f, 0.2f), // +Y: green
        glm::vec3(2.5f, 0.2f, 2.5f), // -Y: magenta
        glm::vec3(0.2f, 0.2f, 3.0f), // +Z: blue
        glm::vec3(2.5f, 2.5f, 0.2f)  // -Z: yellow
    };
    const std::array<glm::vec3, 6> wallPositions = {
        glm::vec3( 4.0f,  0.0f,  0.0f),
        glm::vec3(-4.0f,  0.0f,  0.0f),
        glm::vec3( 0.0f,  4.0f,  0.0f),
        glm::vec3( 0.0f, -4.0f,  0.0f),
        glm::vec3( 0.0f,  0.0f,  4.0f),
        glm::vec3( 0.0f,  0.0f, -4.0f)
    };

    for (uint32_t face = 0; face < 6; ++face)
    {
        const AssetHandle matHandle = RegisterEmissiveMaterial(assetManager, 9100001ULL + face, expectedColors[face]);
        CreateBoxEntity(scene.get(), "Wall_" + std::to_string(face), wallPositions[face], glm::vec3(3.0f), matHandle);
    }

    SceneCaptureRenderer capturer;
    CaptureSettings settings;
    settings.resolution = 16;
    settings.nearPlane = 0.05f;
    settings.farPlane = 50.0f;
    settings.mode = CaptureMode::Irradiance;
    settings.includeSky = false;

    CaptureResult orientationResult = capturer.CaptureCubemapSync(scene.get(), assetManager, glm::vec3(0.0f), settings);
    ASSERT_NE(orientationResult.colorCube, nullptr);
    ASSERT_NE(orientationResult.depthCube, nullptr);

    for (uint32_t face = 0; face < 6; ++face)
    {
        std::vector<uint8_t> bytes;
        uint32_t rowPitch = 0;
        uint32_t slicePitch = 0;
        ASSERT_TRUE(Texture::ReadbackTextureSubresource(orientationResult.colorCube, 0, face, bytes, rowPitch, slicePitch));
        const glm::vec4 c = SampleRGBA16FCenter(bytes, rowPitch, 16, 16);

        EXPECT_NEAR(c.r, expectedColors[face].r, 0.15f) << "face " << face << " red mismatch";
        EXPECT_NEAR(c.g, expectedColors[face].g, 0.15f) << "face " << face << " green mismatch";
        EXPECT_NEAR(c.b, expectedColors[face].b, 0.15f) << "face " << face << " blue mismatch";
        EXPECT_NEAR(c.a, 1.0f, 0.02f) << "face " << face << " front-face alpha must be 1.0";
    }

    // 2. Closed cube front-face vs back-face discrimination (SV_IsFrontFace polarity check)
    Ref<Scene> backfaceScene = CreateRef<Scene>(project.get());
    project->SetActiveScene(backfaceScene);
    const AssetHandle enclosureMat = RegisterEmissiveMaterial(assetManager, 9200001ULL, glm::vec3(2.0f, 1.0f, 0.5f));
    CreateBoxEntity(backfaceScene.get(), "ClosedBox", glm::vec3(0.0f), glm::vec3(4.0f), enclosureMat);

    // Capture from OUTSIDE the closed cube looking at its +Z wall (-Z face of probe at (0,0,-4) or +Z face looking at origin)
    CaptureResult outsideResult = capturer.CaptureCubemapSync(backfaceScene.get(), assetManager, glm::vec3(0.0f, 0.0f, -4.0f), settings);
    ASSERT_NE(outsideResult.colorCube, nullptr);
    {
        std::vector<uint8_t> bytes;
        uint32_t rowPitch = 0;
        uint32_t slicePitch = 0;
        ASSERT_TRUE(Texture::ReadbackTextureSubresource(outsideResult.colorCube, 0, 4u /* +Z looks from z=-4 toward z=-2 */, bytes, rowPitch, slicePitch));
        const glm::vec4 c = SampleRGBA16FCenter(bytes, rowPitch, 16, 16);
        EXPECT_NEAR(c.r, 2.0f, 0.15f);
        EXPECT_NEAR(c.g, 1.0f, 0.15f);
        EXPECT_NEAR(c.b, 0.5f, 0.15f);
        EXPECT_NEAR(c.a, 1.0f, 0.02f);
    }

    // Capture from INSIDE the closed cube: all 6 faces hit back-faces and must output (0,0,0,0)
    CaptureResult insideResult = capturer.CaptureCubemapSync(backfaceScene.get(), assetManager, glm::vec3(0.0f), settings);
    ASSERT_NE(insideResult.colorCube, nullptr);
    for (uint32_t face = 0; face < 6; ++face)
    {
        std::vector<uint8_t> bytes;
        uint32_t rowPitch = 0;
        uint32_t slicePitch = 0;
        ASSERT_TRUE(Texture::ReadbackTextureSubresource(insideResult.colorCube, 0, face, bytes, rowPitch, slicePitch));
        const glm::vec4 c = SampleRGBA16FCenter(bytes, rowPitch, 16, 16);
        EXPECT_NEAR(c.r, 0.0f, 0.02f) << "inside cube backface face " << face;
        EXPECT_NEAR(c.g, 0.0f, 0.02f) << "inside cube backface face " << face;
        EXPECT_NEAR(c.b, 0.0f, 0.02f) << "inside cube backface face " << face;
        EXPECT_NEAR(c.a, 0.0f, 0.02f) << "inside cube backface alpha face " << face;
    }

    project->SetActiveScene(nullptr);
    backfaceScene.reset();
    scene.reset();
    assetManager->Reset();
    Project::CloseActive();
    std::filesystem::remove_all(tempDir, ec);
}

TEST(GICaptureTests, IBLPrefilterLinearEnergyConservation)
{
    auto *dm = DeviceManager::GetInstance();
    ASSERT_NE(dm, nullptr);
    ASSERT_NE(dm->GetDevice(), nullptr);
    ASSERT_NE(dm->GetGraphicsQueue(), nullptr);

    constexpr uint32_t kEnvW = 32;
    constexpr uint32_t kEnvH = 16;
    constexpr uint32_t kCubeSize = 32;
    constexpr uint32_t kCubeMips = 5;

    const glm::vec3 kTargetRadiance(0.5f, 1.0f, 2.0f);

    // Upload a constant linear HDR equirectangular 2D texture
    TextureCreateInfo env2DInfo;
    env2DInfo.width = kEnvW;
    env2DInfo.height = kEnvH;
    env2DInfo.depth = 1;
    env2DInfo.mipLevels = 1;
    env2DInfo.arraySize = 1;
    env2DInfo.sampleCount = 1;
    env2DInfo.dimension = nri::TextureType::TEXTURE_2D;
    env2DInfo.format = nri::Format::RGBA16_SFLOAT;
    env2DInfo.isCubeMap = false;
    env2DInfo.isRenderTarget = false;
    env2DInfo.isUAV = false;
    env2DInfo.bindless = false;
    env2DInfo.samplerLinearFiltering = true;
    Ref<Texture> srcEnv2D = Texture::Create(env2DInfo, "[GICaptureTest Env2D]");

    std::vector<uint16_t> envPixels(static_cast<size_t>(kEnvW) * kEnvH * 4u);
    const uint16_t hr = glm::packHalf1x16(kTargetRadiance.r);
    const uint16_t hg = glm::packHalf1x16(kTargetRadiance.g);
    const uint16_t hb = glm::packHalf1x16(kTargetRadiance.b);
    const uint16_t ha = glm::packHalf1x16(1.0f);
    for (size_t i = 0; i < static_cast<size_t>(kEnvW) * kEnvH; ++i)
    {
        envPixels[i * 4u + 0u] = hr;
        envPixels[i * 4u + 1u] = hg;
        envPixels[i * 4u + 2u] = hb;
        envPixels[i * 4u + 3u] = ha;
    }
    const uint32_t envRowPitch = kEnvW * 4u * static_cast<uint32_t>(sizeof(uint16_t));
    ASSERT_TRUE(srcEnv2D->UploadRawData(envPixels.data(), envRowPitch, envRowPitch * kEnvH, 0, 0));

    TextureCreateInfo rawCubeInfo;
    rawCubeInfo.width = kCubeSize;
    rawCubeInfo.height = kCubeSize;
    rawCubeInfo.depth = 1;
    rawCubeInfo.mipLevels = kCubeMips;
    rawCubeInfo.arraySize = 6;
    rawCubeInfo.sampleCount = 1;
    rawCubeInfo.dimension = nri::TextureType::TEXTURE_2D;
    rawCubeInfo.format = nri::Format::RGBA16_SFLOAT;
    rawCubeInfo.isCubeMap = true;
    rawCubeInfo.isRenderTarget = false;
    rawCubeInfo.isUAV = true;
    rawCubeInfo.bindless = false;
    rawCubeInfo.samplerLinearFiltering = true;
    Ref<Texture> rawCube = Texture::Create(rawCubeInfo, "[GICaptureTest RawCube]");

    TextureCreateInfo prefilterInfo = rawCubeInfo;
    prefilterInfo.bindless = true;
    prefilterInfo.forceBindless = true;
    Ref<Texture> prefilterCube = Texture::Create(prefilterInfo, "[GICaptureTest PrefilterCube]");

    IBLPrefilter prefilter;

    const auto &nri = dm->GetCoreInterface();
    nri::CommandAllocator *cmdAlloc = nullptr;
    nri::CommandBuffer *cmd = nullptr;
    nri::Fence *fence = nullptr;
    ASSERT_EQ(nri.CreateCommandAllocator(*dm->GetGraphicsQueue(), cmdAlloc), nri::Result::SUCCESS);
    ASSERT_EQ(nri.CreateCommandBuffer(*cmdAlloc, cmd), nri::Result::SUCCESS);
    nri::FenceDesc fenceDesc = {};
    ASSERT_EQ(nri.CreateFence(*dm->GetDevice(), fenceDesc, fence), nri::Result::SUCCESS);

    ASSERT_EQ(nri.BeginCommandBuffer(*cmd, nullptr), nri::Result::SUCCESS);
    prefilter.ConvertEnv2DToCube(cmd, srcEnv2D, rawCube, 0 /* HDRI */, 0.0f);
    prefilter.GenerateCubeMips(cmd, rawCube);
    prefilter.PrefilterGGX(cmd, rawCube, prefilterCube, 64);
    ASSERT_EQ(nri.EndCommandBuffer(*cmd), nri::Result::SUCCESS);

    {
        std::lock_guard<std::mutex> lock(GPUUploadSync::GetQueueMutex());
        nri::FenceSubmitDesc signalDesc = { fence, 1 };
        nri::QueueSubmitDesc submitDesc = {};
        submitDesc.commandBuffers = &cmd;
        submitDesc.commandBufferNum = 1;
        submitDesc.signalFences = &signalDesc;
        submitDesc.signalFenceNum = 1;
        nri.QueueSubmit(*dm->GetGraphicsQueue(), submitDesc);
        nri.Wait(*fence, 1);
    }

    nri.DestroyCommandBuffer(cmd);
    nri.DestroyCommandAllocator(cmdAlloc);
    nri.DestroyFence(fence);

    for (uint32_t mip = 0; mip < kCubeMips; ++mip)
    {
        const uint32_t mipSize = std::max(1u, kCubeSize >> mip);
        for (uint32_t face : { 0u, 2u, 4u })
        {
            std::vector<uint8_t> bytes;
            uint32_t rowPitch = 0;
            uint32_t slicePitch = 0;
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(prefilterCube, mip, face, bytes, rowPitch, slicePitch));
            const glm::vec4 c = SampleRGBA16FCenter(bytes, rowPitch, mipSize, mipSize);
            EXPECT_NEAR(c.r, kTargetRadiance.r, 0.03f) << "mip " << mip << " face " << face;
            EXPECT_NEAR(c.g, kTargetRadiance.g, 0.05f) << "mip " << mip << " face " << face;
            EXPECT_NEAR(c.b, kTargetRadiance.b, 0.10f) << "mip " << mip << " face " << face;
        }
    }
}
