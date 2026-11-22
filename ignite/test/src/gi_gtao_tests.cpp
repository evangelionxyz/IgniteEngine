// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/agent_controller.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gi/gi_baker.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/gtao.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"

#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vector>

namespace ignite::tests
{
    namespace
    {
        glm::vec4 SampleRGBA16F(const std::vector<uint8_t> &bytes, uint32_t rowPitch, uint32_t x, uint32_t y)
        {
            const size_t byteOffset = static_cast<size_t>(y) * static_cast<size_t>(rowPitch) + static_cast<size_t>(x) * 4u * sizeof(uint16_t);
            const auto *px = reinterpret_cast<const uint16_t *>(bytes.data() + byteOffset);
            return glm::vec4(
                glm::unpackHalf1x16(px[0]),
                glm::unpackHalf1x16(px[1]),
                glm::unpackHalf1x16(px[2]),
                glm::unpackHalf1x16(px[3]));
        }

        glm::vec4 SampleRGBA16FAvg3x3(const std::vector<uint8_t> &bytes, uint32_t rowPitch, uint32_t width, uint32_t height, uint32_t cx, uint32_t cy)
        {
            glm::vec4 sum(0.0f);
            float count = 0.0f;
            for (int dy = -1; dy <= 1; ++dy)
            {
                const int py = static_cast<int>(cy) + dy;
                if (py < 0 || py >= static_cast<int>(height))
                    continue;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int px = static_cast<int>(cx) + dx;
                    if (px < 0 || px >= static_cast<int>(width))
                        continue;
                    sum += SampleRGBA16F(bytes, rowPitch, static_cast<uint32_t>(px), static_cast<uint32_t>(py));
                    count += 1.0f;
                }
            }
            return count > 0.0f ? (sum / count) : glm::vec4(0.0f);
        }

        bool ProjectWorldToPixel(ICamera &cam, const glm::vec3 &worldPos, uint32_t width, uint32_t height, uint32_t &outX, uint32_t &outY)
        {
            const glm::vec4 clip = cam.GetProjection() * cam.GetView() * glm::vec4(worldPos, 1.0f);
            if (clip.w <= 1e-4f)
                return false;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            const float u = ndc.x * 0.5f + 0.5f;
            const float v = 0.5f - ndc.y * 0.5f;
            if (u <= 0.02f || u >= 0.98f || v <= 0.02f || v >= 0.98f)
                return false;

            outX = std::clamp(static_cast<uint32_t>(std::floor(u * static_cast<float>(width))), 0u, width - 1u);
            outY = std::clamp(static_cast<uint32_t>(std::floor(v * static_cast<float>(height))), 0u, height - 1u);
            return true;
        }

        AssetHandle RegisterTestMaterial(AssetManager *am, uint64_t id, const glm::vec3 &baseColor, float baseWeight, const glm::vec3 &emissive)
        {
            Ref<Material> mat = CreateRef<Material>();
            mat->name = "GTAOTestMat_" + std::to_string(id);
            mat->gpuData.baseColorFactor = glm::vec4(baseColor, 1.0f);
            mat->gpuData.baseWeight = baseWeight;
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
            am->AssignAsset(handle, mat);
            return handle;
        }

        Entity AddTestBox(Scene *scene, const std::string &name, const glm::vec3 &pos, const glm::vec3 &scale, AssetHandle matHandle)
        {
            Entity e = SceneManager::CreateEntity(scene, name, EntityType_Node);
            auto &tr = e.GetComponent<TransformComponent>();
            tr.local.translation = pos;
            tr.local.scale = scale;
            tr.world = tr.local;
            tr.dirty = true;

            auto &smc = e.AddComponent<StaticMeshComponent>();
            smc.handle = Renderer::GetDefaultMeshHandle(EMeshType::CUBE);
            smc.overrideMaterials[0] = matHandle;
            return e;
        }
    }

    TEST(GTAOTests, MultiBounceAOPreservesEndpoints)
    {
        const glm::vec3 albedo(0.8f, 0.45f, 0.15f);
        const glm::vec3 dark = gi::MultiBounceAO(0.0f, albedo);
        const glm::vec3 full = gi::MultiBounceAO(1.0f, albedo);

        EXPECT_NEAR(dark.r, 0.0f, 1e-6f);
        EXPECT_NEAR(dark.g, 0.0f, 1e-6f);
        EXPECT_NEAR(dark.b, 0.0f, 1e-6f);
        EXPECT_NEAR(full.r, 1.0f, 1e-6f);
        EXPECT_NEAR(full.g, 1.0f, 1e-6f);
        EXPECT_NEAR(full.b, 1.0f, 1e-6f);
    }

    TEST(GTAOTests, MultiBounceAOIsIndirectOnlyFriendly)
    {
        const glm::vec3 white = gi::MultiBounceAO(0.5f, glm::vec3(1.0f));
        const glm::vec3 black = gi::MultiBounceAO(0.5f, glm::vec3(0.0f));

        // Jimenez's polynomial restores some bounce energy for bright albedo,
        // while a black surface remains the ordinary scalar AO term.
        EXPECT_GT(white.r, 0.5f);
        EXPECT_NEAR(black.r, 0.5f, 1e-5f);
        EXPECT_NEAR(white.r, white.g, 1e-6f);
        EXPECT_NEAR(white.g, white.b, 1e-6f);
    }

    TEST(GTAOTests, MultiBounceAOClampsInputs)
    {
        const glm::vec3 result = gi::MultiBounceAO(4.0f, glm::vec3(2.0f, -1.0f, 0.5f));
        EXPECT_GE(result.r, 0.0f);
        EXPECT_LE(result.r, 1.0f);
        EXPECT_GE(result.g, 0.0f);
        EXPECT_LE(result.g, 1.0f);
        EXPECT_GE(result.b, 0.0f);
        EXPECT_LE(result.b, 1.0f);
        EXPECT_FLOAT_EQ(result.r, 1.0f);
    }

    TEST(GTAOTests, PostProcessingDefaultsToGTAO)
    {
        const PostProcessing settings{};
        EXPECT_EQ(settings.aoMode, AOMode::GTAO);
        EXPECT_GT(settings.aoRadius, 0.0f);
    }

    TEST(GTAOTests, DepthPrepassAndGTAOEndToEndResizeAndIndirectOnly)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGTAOEndToEndTest";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir / "Assets", ec);

        ProjectInfo info;
        info.name = "GTAOEndToEndTest";
        info.rootDirectory = tempDir;
        info.filepath = tempDir / "GTAOEndToEndTest.ixproj";
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *assetManager = AssetManager::GetInstance();
        ASSERT_NE(assetManager, nullptr);
        assetManager->SetActiveProject(project);

        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        const AssetHandle whiteMat = RegisterTestMaterial(assetManager, 9600001ULL, glm::vec3(0.85f, 0.85f, 0.85f), 1.0f, glm::vec3(0.0f));
        const AssetHandle emissiveMat = RegisterTestMaterial(assetManager, 9600002ULL, glm::vec3(0.0f), 0.0f, glm::vec3(2.2f, 0.7f, 0.3f));

        // 90-degree concave floor-wall corner plus an emissive cube
        AddTestBox(scene.get(), "Floor", glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(12.0f, 0.5f, 12.0f), whiteMat);
        AddTestBox(scene.get(), "BackWall", glm::vec3(0.0f, 2.0f, -1.25f), glm::vec3(12.0f, 4.0f, 0.5f), whiteMat);
        AddTestBox(scene.get(), "EmissiveCube", glm::vec3(2.0f, 0.6f, 0.2f), glm::vec3(0.8f, 0.8f, 0.8f), emissiveMat);

        {
            Entity lightEnt = SceneManager::CreateEntity(scene.get(), "SunLight", EntityType_Node);
            auto &ltr = lightEnt.GetComponent<TransformComponent>();
            ltr.local.rotation = glm::angleAxis(glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            ltr.world = ltr.local;
            ltr.dirty = true;

            auto &dl = lightEnt.AddComponent<DirectionalLightComponent>();
            dl.color = glm::vec4(1.0f);
            dl.intensity = 3.0f;
            dl.cascadeShadow = false;
            dl.bakeIndirect = true;
        }

        Entity envEnt = SceneManager::CreateWorldEnvironment(scene.get(), "Env");
        auto &envComp = envEnt.GetComponent<WorldEnvironment>();
        envComp.skyType = SkyType::ProceduralSky;
        envComp.enableIBL = true;
        envComp.dirtyEnvironment = true;

        scene->UpdateTransforms(0.016f);

        ICamera camera;
        camera.fov = 55.0f;
        camera.nearPlane = 0.1f;
        camera.farPlane = 100.0f;
        camera.position = glm::vec3(0.0f, 2.4f, 3.2f);
        camera.SetView(glm::lookAt(camera.position, glm::vec3(0.0f, 0.35f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f)));
        camera.postProcessing.enableSSAO = true;
        camera.postProcessing.aoMode = AOMode::GTAO;
        camera.postProcessing.aoRadius = 1.0f;
        camera.postProcessing.aoIntensity = 1.5f;
        camera.postProcessing.aoPower = 1.0f;
        camera.postProcessing.taaProperties.enable = false;

        Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>();
        sceneRenderer->SetActiveScene(scene);
        sceneRenderer->sceneRenderSettings.enableDepthPrepass = true;
        sceneRenderer->sceneRenderSettings.enableFXAA = true;
        sceneRenderer->sceneRenderSettings.aoModeOverride = static_cast<int>(AOMode::GTAO);

        Renderer *renderer = Application::GetInstance()->GetRenderer();
        ASSERT_NE(renderer, nullptr);
        uint64_t frameIdx = 1;

        auto renderFrames = [&](uint32_t w, uint32_t h, int count)
        {
            camera.UpdateProjection(w, h);
            sceneRenderer->ResizeFramebuffer(&camera, w, h);
            for (int i = 0; i < count; ++i)
            {
                renderer->BeginFrame(frameIdx++);
                sceneRenderer->BeginFrame();
                sceneRenderer->Render(&camera, Renderer::GetCurrentFrameContext(), false);
            }
        };

        // Phase A: Render AO debug mode across a viewport resize
        sceneRenderer->SetRenderMode(11 /* RENDER_MODE_AO */);
        renderFrames(320, 240, 2);
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        Ref<CameraRenderTarget> target = sceneRenderer->GetRenderTarget(&camera);
        ASSERT_NE(target, nullptr);
        ASSERT_NE(target->sceneRT, nullptr);
        ASSERT_NE(target->compositeRT, nullptr);

        uint32_t cornerX = 0, cornerY = 0;
        uint32_t flatX = 0, flatY = 0;
        uint32_t emissiveX = 0, emissiveY = 0;
        const glm::vec3 cornerWorld(0.0f, 0.02f, -0.92f);
        const glm::vec3 flatFloorWorld(-1.8f, 0.02f, 1.2f);
        const glm::vec3 emissiveWorld(2.0f, 0.6f, 0.6f);

        ASSERT_TRUE(ProjectWorldToPixel(camera, cornerWorld, 384, 256, cornerX, cornerY));
        ASSERT_TRUE(ProjectWorldToPixel(camera, flatFloorWorld, 384, 256, flatX, flatY));
        ASSERT_TRUE(ProjectWorldToPixel(camera, emissiveWorld, 384, 256, emissiveX, emissiveY));

        {
            std::vector<uint8_t> bytes;
            uint32_t rowPitch = 0, slicePitch = 0;
            Ref<Texture> sceneColor = target->sceneRT->GetColorAttachment(0);
            ASSERT_NE(sceneColor, nullptr);
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(sceneColor, 0, 0, bytes, rowPitch, slicePitch));

            const float cornerAO = SampleRGBA16FAvg3x3(bytes, rowPitch, 384, 256, cornerX, cornerY).r;
            const float flatAO = SampleRGBA16FAvg3x3(bytes, rowPitch, 384, 256, flatX, flatY).r;

            EXPECT_GT(flatAO, 0.70f);
            EXPECT_GE(cornerAO, 0.0f);
            EXPECT_LT(cornerAO, flatAO - 0.05f);
        }

        {
            std::vector<uint8_t> compBytes;
            uint32_t rowPitch = 0, slicePitch = 0;
            Ref<Texture> compColor = target->compositeRT->GetColorAttachment(0);
            ASSERT_NE(compColor, nullptr);
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(compColor, 0, 0, compBytes, rowPitch, slicePitch));
            ASSERT_FALSE(compBytes.empty());

            uint8_t minVal = 255;
            uint8_t maxVal = 0;
            for (uint32_t y = 0; y < 256; ++y)
            {
                const uint8_t *row = compBytes.data() + static_cast<size_t>(y) * rowPitch;
                for (uint32_t x = 0; x < 384; ++x)
                {
                    const uint8_t r = row[x * 4u];
                    minVal = std::min(minVal, r);
                    maxVal = std::max(maxVal, r);
                }
            }
            EXPECT_GT(maxVal - minVal, 0);
        }

        const auto &timings = sceneRenderer->GetLastGPUTimingsMs();
        if (!timings.empty())
        {
            EXPECT_TRUE(timings.contains("DepthNormalPrepass"));
            EXPECT_TRUE(timings.contains("GTAO"));
        }

        // Phase B: Verify GTAO modulates indirect lighting only (direct + emissive unchanged when IBL is disabled)
        envComp.enableIBL = false;
        sceneRenderer->SetRenderMode(0 /* RENDER_MODE_COLOR */);

        sceneRenderer->sceneRenderSettings.aoModeOverride = 0;
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        std::vector<uint8_t> bytesNoAO;
        uint32_t pitchNoAO = 0, sliceNoAO = 0;
        ASSERT_TRUE(Texture::ReadbackTextureSubresource(target->sceneRT->GetColorAttachment(0), 0, 0, bytesNoAO, pitchNoAO, sliceNoAO));

        sceneRenderer->sceneRenderSettings.aoModeOverride = 2;
        renderFrames(384, 256, 2);
        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());

        std::vector<uint8_t> bytesWithAO;
        uint32_t pitchWithAO = 0, sliceWithAO = 0;
        ASSERT_TRUE(Texture::ReadbackTextureSubresource(target->sceneRT->GetColorAttachment(0), 0, 0, bytesWithAO, pitchWithAO, sliceWithAO));

        auto checkDirectUnchanged = [&](uint32_t px, uint32_t py, const char *label)
        {
            const glm::vec3 c0 = glm::vec3(SampleRGBA16FAvg3x3(bytesNoAO, pitchNoAO, 384, 256, px, py));
            const glm::vec3 c1 = glm::vec3(SampleRGBA16FAvg3x3(bytesWithAO, pitchWithAO, 384, 256, px, py));
            EXPECT_GT(c0.r, 0.1f) << label;
            EXPECT_LT(glm::length(c0 - c1), 0.02f) << label;
        };

        checkDirectUnchanged(cornerX, cornerY, "corner direct lighting");
        checkDirectUnchanged(flatX, flatY, "flat floor direct lighting");
        checkDirectUnchanged(emissiveX, emissiveY, "emissive cube");

        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());
        sceneRenderer.reset();
        project->SetActiveScene(nullptr);
        scene.reset();
        assetManager->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }

    TEST(GIToolingTests, ComputeSceneBakeHashDeterminismAndInvalidation)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGIBakeHashTest";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir / "Assets", ec);

        ProjectInfo info;
        info.name = "GIBakeHashTest";
        info.rootDirectory = tempDir;
        info.filepath = tempDir / "GIBakeHashTest.ixproj";
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *assetManager = AssetManager::GetInstance();
        ASSERT_NE(assetManager, nullptr);
        assetManager->SetActiveProject(project);

        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        const AssetHandle mat1 = RegisterTestMaterial(assetManager, 9700001ULL, glm::vec3(0.7f), 1.0f, glm::vec3(0.0f));
        const AssetHandle mat2 = RegisterTestMaterial(assetManager, 9700002ULL, glm::vec3(0.2f, 0.8f, 0.3f), 1.0f, glm::vec3(0.0f));

        Entity cube = AddTestBox(scene.get(), "StaticCube", glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), mat1);
        auto &rc = cube.GetComponent<RenderingComponent>();
        rc.contributeGI = true;

        Entity lightEnt = SceneManager::CreateEntity(scene.get(), "Sun", EntityType_Node);
        auto &dl = lightEnt.AddComponent<DirectionalLightComponent>();
        dl.intensity = 2.5f;
        dl.bakeIndirect = true;

        Entity envEnt = SceneManager::CreateWorldEnvironment(scene.get(), "Env");
        auto &envComp = envEnt.GetComponent<WorldEnvironment>();
        envComp.exposure = 1.0f;

        Entity volEnt = SceneManager::CreateEntity(scene.get(), "GIVolume", EntityType_Node);
        auto &vol = volEnt.AddComponent<IrradianceVolumeComponent>();

        scene->UpdateTransforms(0.016f);

        const uint64_t h0 = ComputeSceneBakeHash(scene.get());
        EXPECT_NE(h0, 0ULL);
        EXPECT_EQ(h0, ComputeSceneBakeHash(scene.get()));
        EXPECT_TRUE(IsSceneGIOutOfDate(scene.get()));

        vol.bakeHash = h0;
        vol.bakedAssetPath = "Baked/test.igivol";
        EXPECT_FALSE(IsSceneGIOutOfDate(scene.get()));
        EXPECT_EQ(ComputeSceneBakeHash(scene.get()), h0);

        // 1. Static mesh transform change invalidates bake hash
        auto &cubeTr = cube.GetComponent<TransformComponent>();
        cubeTr.local.translation.x += 1.0f;
        cubeTr.dirty = true;
        scene->UpdateTransforms(0.016f);
        const uint64_t h1 = ComputeSceneBakeHash(scene.get());
        EXPECT_NE(h1, h0);
        EXPECT_TRUE(IsSceneGIOutOfDate(scene.get()));

        // 2. Material override change invalidates bake hash
        vol.bakeHash = h1;
        EXPECT_FALSE(IsSceneGIOutOfDate(scene.get()));
        cube.GetComponent<StaticMeshComponent>().overrideMaterials[0] = mat2;
        const uint64_t h2 = ComputeSceneBakeHash(scene.get());
        EXPECT_NE(h2, h1);
        EXPECT_TRUE(IsSceneGIOutOfDate(scene.get()));

        // 3. Bake-enabled light intensity change invalidates bake hash
        vol.bakeHash = h2;
        dl.intensity = 6.0f;
        const uint64_t h3 = ComputeSceneBakeHash(scene.get());
        EXPECT_NE(h3, h2);
        EXPECT_TRUE(IsSceneGIOutOfDate(scene.get()));

        // 4. WorldEnvironment exposure change invalidates bake hash
        vol.bakeHash = h3;
        envComp.exposure = 1.75f;
        const uint64_t h4 = ComputeSceneBakeHash(scene.get());
        EXPECT_NE(h4, h3);
        EXPECT_TRUE(IsSceneGIOutOfDate(scene.get()));

        project->SetActiveScene(nullptr);
        scene.reset();
        assetManager->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }

    TEST(GIToolingTests, AgentControllerGICommandsAndRenderSettings)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteAgentGITest";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir / "Assets", ec);

        ProjectInfo info;
        info.name = "AgentGITest";
        info.rootDirectory = tempDir;
        info.filepath = tempDir / "AgentGITest.ixproj";
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *assetManager = AssetManager::GetInstance();
        ASSERT_NE(assetManager, nullptr);
        assetManager->SetActiveProject(project);

        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        Entity volEnt = SceneManager::CreateEntity(scene.get(), "GIVolume", EntityType_Node);
        auto &vol = volEnt.AddComponent<IrradianceVolumeComponent>();
        vol.bakedAssetPath = "Baked/v.igivol";
        vol.bakeHash = 123ULL;

        Entity probeEnt = SceneManager::CreateEntity(scene.get(), "ReflProbe", EntityType_Node);
        auto &rp = probeEnt.AddComponent<ReflectionProbeComponent>();
        rp.bakedAssetPath = "Baked/p.igirp";
        rp.bakeHash = 456ULL;

        Ref<SceneRenderer> sceneRenderer = CreateRef<SceneRenderer>();
        sceneRenderer->SetActiveScene(scene);
        GIBaker giBaker;

        AgentExecutionContext ctx;
        ctx.scene = scene.get();
        ctx.project = project.get();
        ctx.sceneRenderer = sceneRenderer.get();
        ctx.giBaker = &giBaker;

        // set_render_settings string + integer modes
        {
            const auto res = AgentController::ExecuteCommand({
                { "command", "set_render_settings" },
                { "gi_mode", "fast" },
                { "ao_mode", "gtao" },
                { "depth_prepass", false },
                { "show_gi_probes", true }
            }, ctx);
            ASSERT_TRUE(res.value("ok", false));
            EXPECT_EQ(sceneRenderer->sceneRenderSettings.giModeOverride, 1);
            EXPECT_EQ(sceneRenderer->sceneRenderSettings.aoModeOverride, 2);
            EXPECT_FALSE(sceneRenderer->sceneRenderSettings.enableDepthPrepass);
            EXPECT_TRUE(sceneRenderer->sceneRenderSettings.showGIProbes);

            const auto resInt = AgentController::ExecuteCommand({
                { "command", "set_render_settings" },
                { "gi_mode", 2 },
                { "ao_mode", 0 },
                { "depth_prepass", true }
            }, ctx);
            ASSERT_TRUE(resInt.value("ok", false));
            EXPECT_EQ(sceneRenderer->sceneRenderSettings.giModeOverride, 2);
            EXPECT_EQ(sceneRenderer->sceneRenderSettings.aoModeOverride, 0);
            EXPECT_TRUE(sceneRenderer->sceneRenderSettings.enableDepthPrepass);
        }

        // set_gi_debug_mode valid and out-of-range
        {
            EXPECT_TRUE(AgentController::ExecuteCommand({ { "command", "set_gi_debug_mode" }, { "mode", 9 } }, ctx).value("ok", false));
            EXPECT_EQ(sceneRenderer->GetRenderMode(), 9);
            EXPECT_TRUE(AgentController::ExecuteCommand({ { "command", "set_gi_debug_mode" }, { "mode", 11 } }, ctx).value("ok", false));
            EXPECT_EQ(sceneRenderer->GetRenderMode(), 11);
            EXPECT_TRUE(AgentController::ExecuteCommand({ { "command", "set_gi_debug_mode" }, { "mode", 0 } }, ctx).value("ok", false));
            EXPECT_EQ(sceneRenderer->GetRenderMode(), 0);
            EXPECT_FALSE(AgentController::ExecuteCommand({ { "command", "set_gi_debug_mode" }, { "mode", 15 } }, ctx).value("ok", true));
        }

        // get_gi_status
        {
            const auto res = AgentController::ExecuteCommand({ { "command", "get_gi_status" } }, ctx);
            ASSERT_TRUE(res.value("ok", false));
            EXPECT_NE(res["data"].value("scene_bake_hash", 0ULL), 0ULL);
            EXPECT_TRUE(res["data"].value("out_of_date", false));
            EXPECT_EQ(res["data"].value("volume_count", 0u), 1u);
            EXPECT_EQ(res["data"].value("reflection_probe_count", 0u), 1u);
        }

        // get_gpu_timings
        {
            const auto res = AgentController::ExecuteCommand({ { "command", "get_gpu_timings" } }, ctx);
            ASSERT_TRUE(res.value("ok", false));
            EXPECT_TRUE(res["data"].contains("timings_ms"));
        }

        // clear_baked_gi
        {
            const auto res = AgentController::ExecuteCommand({ { "command", "clear_baked_gi" } }, ctx);
            ASSERT_TRUE(res.value("ok", false));
            EXPECT_EQ(res["data"].value("cleared_volumes", 0u), 1u);
            EXPECT_EQ(res["data"].value("cleared_probes", 0u), 1u);
            EXPECT_TRUE(vol.bakedAssetPath.empty());
            EXPECT_EQ(vol.bakeHash, 0ULL);
            EXPECT_TRUE(rp.bakedAssetPath.empty());
            EXPECT_EQ(rp.bakeHash, 0ULL);
        }

        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());
        sceneRenderer.reset();
        project->SetActiveScene(nullptr);
        scene.reset();
        assetManager->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }
}
