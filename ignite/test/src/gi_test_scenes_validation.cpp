// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/asset/asset_importer.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gi/gi_asset_io.hpp"
#include "ignite/graphics/gi/gi_baker.hpp"
#include "ignite/graphics/gi/irradiance_volume.hpp"
#include "ignite/graphics/gi/reflection_probe_set.hpp"
#include "ignite/graphics/gi/sh.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/renderer/scene_renderer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/icamera.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/serializer/scene_serializer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace ignite::tests
{
    namespace
    {
        const std::filesystem::path kValidationSceneDir = std::filesystem::path("D:/Dev/Ignite/example/Assets/Scenes/GIValidation");

        AssetHandle CreateValidationMaterial(AssetManager *am, uint64_t id, const std::string &name,
            const glm::vec3 &baseColor, float metallic = 0.0f, float roughness = 0.85f,
            float specularWeight = 0.5f, const glm::vec3 &emissive = glm::vec3(0.0f))
        {
            Ref<Material> mat = CreateRef<Material>();
            mat->name = name;
            mat->gpuData.baseColorFactor = glm::vec4(baseColor, 1.0f);
            mat->gpuData.baseWeight = 1.0f;
            mat->gpuData.metallicFactor = metallic;
            mat->gpuData.roughnessFactor = roughness;
            mat->gpuData.specularWeight = specularWeight;
            mat->gpuData.emissiveFactor = glm::vec4(emissive, 1.0f);
            mat->gpuData.emissionLuminance = (glm::length(emissive) > 0.001f) ? 1.0f : 1.0f;
            mat->SetType(MaterialType::Opaque);
            mat->SetReadyFlag(true);
            mat->SetDirtyFlag(true);
            mat->InvalidateBindingSet();

            const AssetHandle handle(id);
            am->AssignAsset(handle, mat);
            return handle;
        }

        Entity AddValidationMesh(Scene *scene, const std::string &name, EMeshType meshType,
            const glm::vec3 &pos, const glm::vec3 &scale, AssetHandle matHandle,
            EntityMobility mobility = EntityMobility::Static, bool castShadow = true, bool receiveShadow = true)
        {
            Entity e = SceneManager::CreateEntity(scene, name, EntityType_Node);
            auto &tr = e.GetComponent<TransformComponent>();
            tr.local.translation = pos;
            tr.local.scale = scale;
            tr.world = tr.local;
            tr.dirty = true;

            auto &rc = e.GetComponent<RenderingComponent>();
            rc.mobility = mobility;
            rc.castShadow = castShadow;
            rc.receiveShadow = receiveShadow;
            rc.contributeGI = (mobility == EntityMobility::Static);
            rc.receiveGI = true;

            auto &smc = e.AddComponent<StaticMeshComponent>();
            const AssetHandle baseMeshHandle = Renderer::GetDefaultMeshHandle(meshType);
            auto baseMesh = AssetManager::GetInstance()->GetAsset<StaticMesh>(baseMeshHandle);
            Ref<StaticMesh> uniqueMesh = StaticMesh::Create();
            if (baseMesh)
            {
                for (const auto &inst : baseMesh->GetMeshInstances())
                {
                    auto copyInst = StaticMeshInstance::Create(inst->GetName(), inst->GetPrimitive());
                    copyInst->local = inst->local;
                    copyInst->global = inst->global;
                    copyInst->localAABB = inst->localAABB;
                    copyInst->SetMaterial(matHandle);
                    uniqueMesh->AddMeshInstance(copyInst);
                }
                uniqueMesh->CalculateLocalAABB();
            }
            static uint64_t s_TestMeshId = 900600000ULL;
            const AssetHandle meshHandle(++s_TestMeshId);
            AssetManager::GetInstance()->AssignAsset(meshHandle, uniqueMesh);
            smc.handle = meshHandle;
            return e;
        }

        gi::SHL2 UnpackSHAt(const IrradianceVolumeData &data, int32_t ix, int32_t iy, int32_t iz, float *outValidity = nullptr)
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
            float validity = 1.0f;
            gi::UnpackSHL2From7RGBA(packed, sh, validity);
            if (outValidity)
            {
                *outValidity = validity;
            }
            return sh;
        }

        float UnpackSkyVisAt(const IrradianceVolumeData &data, int32_t ix, int32_t iy, int32_t iz)
        {
            const size_t idx = (static_cast<size_t>(iz) * data.dims.y + iy) * data.dims.x + ix;
            const size_t base = idx * 4u;
            if (base >= data.skyVisibilityHalfRGBA.size())
            {
                return -1.0f;
            }
            return glm::unpackHalf1x16(data.skyVisibilityHalfRGBA[base]);
        }

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

        glm::vec4 SampleRGBA16FAvg3x3(const std::vector<uint8_t> &bytes, uint32_t rowPitch, uint32_t w, uint32_t h, uint32_t cx, uint32_t cy)
        {
            glm::vec4 sum(0.0f);
            float count = 0.0f;
            for (int dy = -1; dy <= 1; ++dy)
            {
                const int py = static_cast<int>(cy) + dy;
                if (py < 0 || py >= static_cast<int>(h)) continue;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int px = static_cast<int>(cx) + dx;
                    if (px < 0 || px >= static_cast<int>(w)) continue;
                    sum += SampleRGBA16F(bytes, rowPitch, static_cast<uint32_t>(px), static_cast<uint32_t>(py));
                    count += 1.0f;
                }
            }
            return count > 0.0f ? (sum / count) : glm::vec4(0.0f);
        }

        bool ProjectWorldToPixel(ICamera &cam, const glm::vec3 &worldPos, uint32_t w, uint32_t h, uint32_t &outX, uint32_t &outY)
        {
            const glm::vec4 clip = cam.GetProjection() * cam.GetView() * glm::vec4(worldPos, 1.0f);
            if (clip.w <= 1e-4f) return false;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            const float u = ndc.x * 0.5f + 0.5f;
            const float v = 0.5f - ndc.y * 0.5f;
            if (u <= 0.02f || u >= 0.98f || v <= 0.02f || v >= 0.98f) return false;
            outX = std::clamp(static_cast<uint32_t>(std::floor(u * static_cast<float>(w))), 0u, w - 1u);
            outY = std::clamp(static_cast<uint32_t>(std::floor(v * static_cast<float>(h))), 0u, h - 1u);
            return true;
        }

        void SaveValidationScene(const Ref<Scene> &scene, Project *project, const std::string &filename)
        {
            std::error_code ec;
            std::filesystem::create_directories(kValidationSceneDir, ec);
            SceneSerializer serializer(scene, project);
            EXPECT_TRUE(serializer.Serialize(kValidationSceneDir / filename));
            EXPECT_TRUE(std::filesystem::exists(kValidationSceneDir / filename));
        }

        // Imports a real glTF model via AssetImporter::ImportStaticMesh and collects resolved Material assets.
        Ref<StaticMesh> BuildStaticMeshFromGLTF(const std::string &gltfPath, const std::filesystem::path &targetDir,
            AssetManager *am, uint64_t meshId, std::vector<Ref<Material>> &outMaterials)
        {
            std::error_code ec;
            std::filesystem::create_directories(targetDir, ec);

            AssetMetaData meta;
            meta.filepath = gltfPath;
            meta.type = AssetType::StaticMesh;

            StaticMeshImportPayload payload;
            payload.type = ImportType::Import;
            payload.targetDirectory = targetDir;
            payload.importMaterials = true;
            payload.forceRebuild = true;

            Ref<StaticMesh> mesh = AssetImporter::ImportStaticMesh(AssetHandle(meshId), meta, am, payload);
            outMaterials.clear();
            if (!mesh)
            {
                return nullptr;
            }

            for (const auto &inst : mesh->GetMeshInstances())
            {
                if (!inst) continue;
                const AssetHandle matH = inst->GetMaterialAssetHandle();
                if (matH == AssetHandle(0)) continue;
                if (auto mat = am->GetAsset<Material>(matH))
                {
                    outMaterials.push_back(mat);
                }
            }
            return mesh;
        }
    }

    // 1. Cornell Box: red/green wall color bleed, multi-bounce gain, L1 vs L2 SH, Sky-Visibility split, and baked point light shadow
    TEST(GITestScenes, CornellBox_BounceColorBleedAndM8Features)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGICornellBox";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir / "Baked", ec);

        ProjectInfo info;
        info.name = "GIValidation_CornellBox";
        info.rootDirectory = tempDir;
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *am = AssetManager::GetInstance();
        am->SetActiveProject(project);
        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        const AssetHandle whiteMat = CreateValidationMaterial(am, 9701001ULL, "CB_White", glm::vec3(0.8f));
        const AssetHandle redMat   = CreateValidationMaterial(am, 9701002ULL, "CB_Red",   glm::vec3(0.85f, 0.08f, 0.08f));
        const AssetHandle greenMat = CreateValidationMaterial(am, 9701003ULL, "CB_Green", glm::vec3(0.08f, 0.85f, 0.08f));

        // 5x5x5 closed Cornell box (interior x in [-2.5..+2.5], y in [0..5], z in [-2.5..+2.7]) with open front (+Z)
        AddValidationMesh(scene.get(), "Floor",       EMeshType::CUBE, {  0.0f, -0.2f,  0.0f }, { 2.9f, 0.2f, 2.7f }, whiteMat);
        AddValidationMesh(scene.get(), "Ceiling",     EMeshType::CUBE, {  0.0f,  5.2f,  0.0f }, { 2.9f, 0.2f, 2.7f }, whiteMat);
        AddValidationMesh(scene.get(), "BackWall",    EMeshType::CUBE, {  0.0f,  2.5f, -2.7f }, { 2.9f, 2.7f, 0.2f }, whiteMat);
        AddValidationMesh(scene.get(), "LeftWall",    EMeshType::CUBE, { -2.7f,  2.5f,  0.0f }, { 0.2f, 2.7f, 2.7f }, redMat);
        AddValidationMesh(scene.get(), "RightWall",   EMeshType::CUBE, {  2.7f,  2.5f,  0.0f }, { 0.2f, 2.7f, 2.7f }, greenMat);
        AddValidationMesh(scene.get(), "OccluderBox", EMeshType::CUBE, {  0.0f,  0.8f, -0.3f }, { 0.6f, 0.8f, 0.6f }, whiteMat);

        {
            Entity pt = SceneManager::CreateEntity(scene.get(), "CeilingPointLight", EntityType_Node);
            auto &tr = pt.GetComponent<TransformComponent>();
            tr.local.translation = glm::vec3(0.0f, 4.4f, -0.2f);
            tr.world = tr.local;
            tr.dirty = true;
            auto &pl = pt.AddComponent<PointLightComponent>();
            pl.color = glm::vec4(1.0f, 0.96f, 0.88f, 1.0f);
            pl.intensity = 18.0f;
            pl.range = 10.0f;
            pl.bakeIndirect = true;
            pl.castShadow = true;
            pl.bakeStaticShadow = true;
        }

        // L2 multi-bounce bake (extents.z = 3.4 so front probe sits outside open +Z entrance)
        Entity volL2 = SceneManager::CreateEntity(scene.get(), "CornellVolume", EntityType_Node);
        auto &vtr = volL2.GetComponent<TransformComponent>();
        vtr.local.translation = glm::vec3(0.0f, 2.5f, 0.0f);
        vtr.world = vtr.local;
        vtr.dirty = true;
        auto &vc = volL2.AddComponent<IrradianceVolumeComponent>();
        vc.extents = glm::vec3(2.2f, 2.0f, 3.4f);
        vc.spacing = 1.4f;
        vc.bounces = 2;
        vc.captureResolution = 32;
        vc.shOrder = 2;
        vc.runtimeMode = 2;
        vc.skyVisibilitySplit = true;

        GIBaker baker;
        IrradianceVolumeData data2;
        const std::filesystem::path bakePath = tempDir / "Baked" / "cornell_l2.igi";
        ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), am, volL2, data2, bakePath));
        EXPECT_EQ(data2.shOrder, 2u);
        EXPECT_NE(data2.flags & IrradianceVolumeData::kFlagHasSkyVisibility, 0u);

        // Interior probes near red/green walls: normal pointing toward each wall receives that wall's reflected color
        const int32_t midZ = data2.dims.z / 2;
        const gi::SHL2 leftProbe  = UnpackSHAt(data2, 1, data2.dims.y / 2, midZ);
        const gi::SHL2 rightProbe = UnpackSHAt(data2, data2.dims.x - 2, data2.dims.y / 2, midZ);
        const glm::vec3 leftIrr  = gi::EvalSHIrradiance(leftProbe, glm::vec3(-1.0f, 0.0f, 0.0f));
        const glm::vec3 rightIrr = gi::EvalSHIrradiance(rightProbe, glm::vec3(1.0f, 0.0f, 0.0f));
        EXPECT_GT(leftIrr.r, leftIrr.g);
        EXPECT_GT(rightIrr.g, rightIrr.r);

        // Interior probe has lower sky visibility than front (+Z) probe outside the open ceiling
        const float backSkyVis  = UnpackSkyVisAt(data2, data2.dims.x / 2, data2.dims.y / 2, 1);
        const float frontSkyVis = UnpackSkyVisAt(data2, data2.dims.x / 2, data2.dims.y / 2, data2.dims.z - 1);
        EXPECT_GE(backSkyVis, 0.0f);
        EXPECT_LE(backSkyVis, 1.0f);
        EXPECT_GT(frontSkyVis, backSkyVis);

        // Verify L1 SH mode (shOrder = 1) zeros L2 bands and fills GPU metadata cleanly
        vc.shOrder = 1;
        IrradianceVolumeData dataL1;
        ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), am, volL2, dataL1));
        EXPECT_EQ(dataL1.shOrder, 1u);
        const gi::SHL2 probeL1 = UnpackSHAt(dataL1, 1, dataL1.dims.y / 2, midZ);
        EXPECT_NEAR(glm::length(probeL1.c[4]), 0.0f, 1e-4f);
        EXPECT_NEAR(glm::length(probeL1.c[8]), 0.0f, 1e-4f);
        Ref<IrradianceVolume> runtimeVol = CreateRef<IrradianceVolume>();
        ASSERT_TRUE(runtimeVol->UploadFromData(dataL1));
        GI_GPUData gpuGI{};
        runtimeVol->FillGIGPUData(gpuGI, &vc);
        EXPECT_EQ(gpuGI.gridDimsAndOrder.w, 1);
        EXPECT_EQ(gpuGI.skyVisibilitySplit, 1);
        EXPECT_GE(gpuGI.skyVisibilityTexIndex, 0);

        SaveValidationScene(scene, project.get(), "GI_CornellBox.ixscene");

        project->SetActiveScene(nullptr);
        scene.reset();
        am->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }

    // 2. Thin-wall Leak Regression Room: bright exterior vs sealed dark interior across a 0.2m wall
    TEST(GITestScenes, ThinWallLeakTest_DarkInteriorIsolated)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGIThinWallValidation";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir / "Baked", ec);

        ProjectInfo info;
        info.name = "GIValidation_ThinWall";
        info.rootDirectory = tempDir;
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *am = AssetManager::GetInstance();
        am->SetActiveProject(project);
        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        const AssetHandle wallMat = CreateValidationMaterial(am, 9702001ULL, "TW_Wall", glm::vec3(0.75f));
        const AssetHandle redEmissive = CreateValidationMaterial(am, 9702002ULL, "TW_BrightOutdoor",
            glm::vec3(1.0f, 0.3f, 0.1f), 0.0f, 0.9f, 0.0f, glm::vec3(8.0f, 4.0f, 2.0f));

        // 0.2m thin separating wall at x = 0, plus closed dark chamber at x = [0..5]
        AddValidationMesh(scene.get(), "ThinWall",    EMeshType::CUBE, { 0.0f, 2.0f,  0.0f }, { 0.2f, 4.4f, 6.0f }, wallMat);
        AddValidationMesh(scene.get(), "DarkBack",    EMeshType::CUBE, { 5.2f, 2.0f,  0.0f }, { 0.4f, 4.4f, 6.0f }, wallMat);
        AddValidationMesh(scene.get(), "DarkFloor",   EMeshType::CUBE, { 2.5f,-0.2f,  0.0f }, { 5.4f, 0.4f, 6.0f }, wallMat);
        AddValidationMesh(scene.get(), "DarkCeiling", EMeshType::CUBE, { 2.5f, 4.2f,  0.0f }, { 5.4f, 0.4f, 6.0f }, wallMat);
        AddValidationMesh(scene.get(), "DarkSideN",   EMeshType::CUBE, { 2.5f, 2.0f, -2.9f }, { 5.4f, 4.4f, 0.4f }, wallMat);
        AddValidationMesh(scene.get(), "DarkSideS",   EMeshType::CUBE, { 2.5f, 2.0f,  2.9f }, { 5.4f, 4.4f, 0.4f }, wallMat);
        AddValidationMesh(scene.get(), "BrightFloor", EMeshType::CUBE, {-3.0f,-0.2f,  0.0f }, { 6.0f, 0.4f, 6.0f }, wallMat);
        AddValidationMesh(scene.get(), "SunBox",      EMeshType::CUBE, {-2.5f, 1.5f,  0.0f }, { 1.5f, 2.5f, 2.5f }, redEmissive);

        {
            Entity sun = SceneManager::CreateEntity(scene.get(), "Sun", EntityType_Node);
            auto &tr = sun.GetComponent<TransformComponent>();
            tr.local.rotation = glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(35.0f), 0.0f));
            tr.world = tr.local;
            tr.dirty = true;
            auto &dl = sun.AddComponent<DirectionalLightComponent>();
            dl.color = glm::vec4(1.0f, 0.96f, 0.90f, 1.0f);
            dl.intensity = 4.5f;
            dl.bakeIndirect = true;
        }

        Entity vol = SceneManager::CreateEntity(scene.get(), "ThinWallVolume", EntityType_Node);
        auto &vtr = vol.GetComponent<TransformComponent>();
        vtr.local.translation = glm::vec3(0.0f, 2.0f, 0.0f);
        vtr.world = vtr.local;
        vtr.dirty = true;
        auto &vc = vol.AddComponent<IrradianceVolumeComponent>();
        vc.extents = glm::vec3(4.0f, 1.5f, 2.0f);
        vc.spacing = 1.0f;
        vc.bounces = 2;
        vc.captureResolution = 32;
        vc.runtimeMode = 2; // LeakResistant

        GIBaker baker;
        IrradianceVolumeData baked;
        const std::filesystem::path bakePath = tempDir / "Baked" / "thin_wall.igi";
        ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), am, vol, baked, bakePath));

        const gi::SHL2 outsideProbe = UnpackSHAt(baked, 1, baked.dims.y / 2, baked.dims.z / 2);
        const gi::SHL2 darkProbe    = UnpackSHAt(baked, baked.dims.x - 2, baked.dims.y / 2, baked.dims.z / 2);
        const float outsideLum = glm::dot(gi::EvalSHIrradiance(outsideProbe, glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(0.2126f, 0.7152f, 0.0722f));
        const float darkLum    = glm::dot(gi::EvalSHIrradiance(darkProbe, glm::vec3(-1.0f, 0.0f, 0.0f)), glm::vec3(0.2126f, 0.7152f, 0.0722f));
        EXPECT_GT(outsideLum, 0.4f);
        EXPECT_LT(darkLum, outsideLum * 0.05f);

        SaveValidationScene(scene, project.get(), "GI_ThinWallLeakTest.ixscene");

        project->SetActiveScene(nullptr);
        scene.reset();
        am->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }

    // 3. Sunlit Corridor / Atrium: indirect multi-bounce falloff into an unlit recessed hallway
    TEST(GITestScenes, SunlitCorridorAtrium_IndirectFalloff)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGISunlitAtrium";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir / "Baked", ec);

        ProjectInfo info;
        info.name = "GIValidation_Atrium";
        info.rootDirectory = tempDir;
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *am = AssetManager::GetInstance();
        am->SetActiveProject(project);
        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        const AssetHandle plasterMat = CreateValidationMaterial(am, 9703001ULL, "AtriumPlaster", glm::vec3(0.85f, 0.80f, 0.72f));
        const AssetHandle warmBounce = CreateValidationMaterial(am, 9703002ULL, "AtriumWarmSunFloor", glm::vec3(0.95f, 0.75f, 0.45f));

        // Open-roof sunlit atrium at z in [-0.5 .. +6.0], roofed dark corridor at z in [-8.5 .. -0.5]
        AddValidationMesh(scene.get(), "Floor",           EMeshType::CUBE, {  0.0f, -0.2f, -1.0f }, { 2.5f, 0.2f, 7.0f }, warmBounce);
        AddValidationMesh(scene.get(), "CorridorRoof",    EMeshType::CUBE, {  0.0f,  3.8f, -4.5f }, { 2.5f, 0.2f, 4.0f }, plasterMat);
        AddValidationMesh(scene.get(), "LeftWall",        EMeshType::CUBE, { -2.7f,  1.8f, -1.0f }, { 0.2f, 2.0f, 7.0f }, plasterMat);
        AddValidationMesh(scene.get(), "RightWall",       EMeshType::CUBE, {  2.7f,  1.8f, -1.0f }, { 0.2f, 2.0f, 7.0f }, plasterMat);
        AddValidationMesh(scene.get(), "CorridorEndWall", EMeshType::CUBE, {  0.0f,  1.8f, -8.2f }, { 2.7f, 2.0f, 0.2f }, plasterMat);
        AddValidationMesh(scene.get(), "AtriumBackWall",  EMeshType::CUBE, {  0.0f,  1.8f,  5.8f }, { 2.7f, 2.0f, 0.2f }, plasterMat);

        {
            Entity sun = SceneManager::CreateEntity(scene.get(), "AtriumSun", EntityType_Node);
            auto &tr = sun.GetComponent<TransformComponent>();
            // X = -115 deg points sunDirection toward (+Y, -Z), directly illuminating the atrium floor and AtriumBackWall's -Z inner face
            tr.local.rotation = glm::quat(glm::vec3(glm::radians(-115.0f), 0.0f, 0.0f));
            tr.world = tr.local;
            tr.dirty = true;
            auto &dl = sun.AddComponent<DirectionalLightComponent>();
            dl.color = glm::vec4(1.0f, 0.94f, 0.82f, 1.0f);
            dl.intensity = 6.0f;
            dl.bakeIndirect = true;
        }

        Entity vol = SceneManager::CreateEntity(scene.get(), "AtriumVolume", EntityType_Node);
        auto &vtr = vol.GetComponent<TransformComponent>();
        vtr.local.translation = glm::vec3(0.0f, 1.8f, -1.5f);
        vtr.world = vtr.local;
        vtr.dirty = true;
        auto &vc = vol.AddComponent<IrradianceVolumeComponent>();
        vc.extents = glm::vec3(1.6f, 1.2f, 5.5f);
        vc.spacing = 1.5f;
        vc.bounces = 3;
        vc.captureResolution = 32;
        vc.runtimeMode = 2;

        GIBaker baker;
        IrradianceVolumeData baked;
        ASSERT_TRUE(baker.BakeVolumeSync(scene.get(), am, vol, baked));
        ASSERT_GE(baked.dims.z, 7);

        const auto lumAtZ = [&](int32_t iz)
        {
            const gi::SHL2 sh = UnpackSHAt(baked, baked.dims.x / 2, baked.dims.y / 2, iz);
            const glm::vec3 floorBounce = gi::EvalSHIrradiance(sh, glm::vec3(0.0f, -1.0f, 0.0f));
            const glm::vec3 wallBounce  = gi::EvalSHIrradiance(sh, glm::vec3(0.0f, 0.0f, 1.0f));
            return glm::dot(0.5f * (floorBounce + wallBounce), glm::vec3(0.2126f, 0.7152f, 0.0722f));
        };

        const float deepCorridorLum = lumAtZ(1);
        const float midCorridorLum  = lumAtZ(baked.dims.z / 2);
        const float atriumLum       = lumAtZ(baked.dims.z - 3); // z = +1.5, inside sunlit atrium opening
        EXPECT_GT(atriumLum, midCorridorLum);
        EXPECT_GT(midCorridorLum, deepCorridorLum);
        EXPECT_GT(deepCorridorLum, 0.005f);

        SaveValidationScene(scene, project.get(), "GI_SunlitCorridorAtrium.ixscene");

        project->SetActiveScene(nullptr);
        scene.reset();
        am->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }

    // 4. Reflection Probes Parallax & Local Self-Occlusion Exclusion: validates side Chrome/Gold spheres
    TEST(GITestScenes, ReflectionProbesParallax_LocalSideSpheresDistinct)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGIReflectionParallax";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir / "Baked", ec);

        ProjectInfo info;
        info.name = "GIValidation_ReflectionProbes";
        info.rootDirectory = tempDir;
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *am = AssetManager::GetInstance();
        am->SetActiveProject(project);
        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        const AssetHandle roomFloorMat = CreateValidationMaterial(am, 9704001ULL, "RP_Floor", glm::vec3(0.25f));
        const AssetHandle centerBoxMat = CreateValidationMaterial(am, 9704002ULL, "RP_CenterBox", glm::vec3(0.75f));
        const AssetHandle leftRedWall  = CreateValidationMaterial(am, 9704003ULL, "RP_LeftRed", glm::vec3(0.95f, 0.08f, 0.08f), 0.0f, 0.7f, 0.3f, glm::vec3(3.0f, 0.2f, 0.2f));
        const AssetHandle rightGrnWall = CreateValidationMaterial(am, 9704004ULL, "RP_RightGreen", glm::vec3(0.08f, 0.95f, 0.12f), 0.0f, 0.7f, 0.3f, glm::vec3(0.2f, 3.0f, 0.2f));
        const AssetHandle chromeMat    = CreateValidationMaterial(am, 9704005ULL, "RP_Chrome", glm::vec3(0.92f, 0.92f, 0.94f), 1.0f, 0.04f, 1.0f);
        const AssetHandle goldMat      = CreateValidationMaterial(am, 9704006ULL, "RP_Gold", glm::vec3(1.0f, 0.84f, 0.32f), 1.0f, 0.06f, 1.0f);

        AddValidationMesh(scene.get(), "Floor", EMeshType::CUBE, { 0.0f, -0.5f, 0.0f }, { 16.0f, 1.0f, 12.0f }, roomFloorMat);
        AddValidationMesh(scene.get(), "CenterBox", EMeshType::CUBE, { 0.0f, 1.0f, -2.0f }, { 2.2f, 2.2f, 2.2f }, centerBoxMat);
        AddValidationMesh(scene.get(), "LeftLandmark", EMeshType::CUBE, { -6.0f, 1.5f, 0.0f }, { 0.6f, 4.0f, 6.0f }, leftRedWall);
        AddValidationMesh(scene.get(), "RightLandmark", EMeshType::CUBE, {  6.0f, 1.5f, 0.0f }, { 0.6f, 4.0f, 6.0f }, rightGrnWall);
        AddValidationMesh(scene.get(), "LeftChromeSphere", EMeshType::SPHERE, { -3.5f, 1.2f, 0.0f }, { 1.0f, 1.0f, 1.0f }, chromeMat);
        AddValidationMesh(scene.get(), "RightGoldSphere",  EMeshType::SPHERE, {  3.5f, 1.2f, 0.0f }, { 1.0f, 1.0f, 1.0f }, goldMat);

        auto addProbe = [&](const std::string &name, const glm::vec3 &pos, const glm::vec3 &infExt, int prio, float exclRadius)
        {
            Entity p = SceneManager::CreateEntity(scene.get(), name, EntityType_Node);
            auto &tr = p.GetComponent<TransformComponent>();
            tr.local.translation = pos;
            tr.world = tr.local;
            tr.dirty = true;
            auto &rp = p.AddComponent<ReflectionProbeComponent>();
            rp.influenceExtents = infExt;
            rp.projectionExtents = infExt;
            rp.resolution = 64;
            rp.priority = prio;
            rp.excludeRadius = exclRadius;
            return p;
        };

        addProbe("GlobalCenterProbe", { 0.0f, 1.2f, 0.0f }, { 9.0f, 5.0f, 7.0f }, 0, 0.0f);
        Entity leftProbe  = addProbe("ChromeSideProbe", { -3.5f, 1.2f, 0.0f }, { 2.0f, 2.0f, 2.0f }, 10, 0.65f);
        Entity rightProbe = addProbe("GoldSideProbe",   {  3.5f, 1.2f, 0.0f }, { 2.0f, 2.0f, 2.0f }, 10, 0.65f);

        GIBaker baker;
        std::vector<std::filesystem::path> savedProbes;
        ASSERT_TRUE(baker.BakeReflectionProbes(scene.get(), am, nullptr, tempDir / "Baked", &savedProbes));
        ASSERT_EQ(savedProbes.size(), 3u);

        ReflectionProbeData leftData, rightData;
        ASSERT_TRUE(gi_io::LoadReflectionProbe(tempDir / "Baked" / leftProbe.GetComponent<ReflectionProbeComponent>().bakedAssetPath.substr(6), leftData));
        ASSERT_TRUE(gi_io::LoadReflectionProbe(tempDir / "Baked" / rightProbe.GetComponent<ReflectionProbeComponent>().bakedAssetPath.substr(6), rightData));

        // Left chrome probe is next to the red landmark -> R > G in its captured irradiance SH
        // Right gold probe is next to the green landmark -> G > R in its captured irradiance SH
        const glm::vec3 leftMean  = glm::vec3(leftData.captureSH[0]);
        const glm::vec3 rightMean = glm::vec3(rightData.captureSH[0]);
        EXPECT_GT(leftMean.r, leftMean.g);
        EXPECT_GT(rightMean.g, rightMean.r);

        // Build runtime ReflectionProbeSet and test live Transform update without re-bake
        Ref<ReflectionProbeSet> probeSet = CreateRef<ReflectionProbeSet>();
        ASSERT_TRUE(probeSet->BuildFromScene(scene.get(), tempDir));
        ASSERT_EQ(probeSet->GetProbesGPU().size(), 3u);

        auto &rtr = rightProbe.GetComponent<TransformComponent>();
        rtr.local.translation = glm::vec3(4.2f, 1.5f, 0.0f);
        rtr.world = rtr.local;
        rtr.dirty = true;
        EXPECT_TRUE(probeSet->UpdateProbeTransformsFromScene(scene.get()));

        bool foundUpdatedCapturePos = false;
        for (const auto &gpu : probeSet->GetProbesGPU())
        {
            if (std::abs(gpu.capturePosAndNorm.x - 4.2f) < 1e-3f)
            {
                foundUpdatedCapturePos = true;
            }
        }
        EXPECT_TRUE(foundUpdatedCapturePos);

        SaveValidationScene(scene, project.get(), "GI_ReflectionProbesParallax.ixscene");

        project->SetActiveScene(nullptr);
        probeSet.reset();
        scene.reset();
        am->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }

    // 5. OpenPBR + Real glTF 2.0 Models + Static/Movable Point & Spot Light Runtime Shadows
    TEST(GITestScenes, OpenPBRShowcase_RealGLTFAndLocalMobilityShadows)
    {
        const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteGIOpenPBRShowcase";
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        std::filesystem::create_directories(tempDir, ec);

        ProjectInfo info;
        info.name = "GIValidation_OpenPBRShowcase";
        info.rootDirectory = tempDir;
        Ref<Project> project = Project::Create(info);
        ASSERT_NE(project, nullptr);

        AssetManager *am = AssetManager::GetInstance();
        am->SetActiveProject(project);
        Ref<Scene> scene = CreateRef<Scene>(project.get());
        project->SetActiveScene(scene);

        std::vector<Ref<Material>> retainedGLTFMats;
        std::vector<Ref<StaticMesh>> retainedGLTFMeshes;
        auto tryLoadGLTF = [&](const char *relPath, const char *name, const glm::vec3 &pos, float scale, uint64_t matIdBase) -> std::vector<Ref<Material>>
        {
            const std::filesystem::path fullPath = std::filesystem::path("E:/3D Models/glTF-Sample-Models/2.0") / relPath;
            std::vector<Ref<Material>> mats;
            if (!std::filesystem::exists(fullPath))
            {
                return mats;
            }
            if (Ref<StaticMesh> mesh = BuildStaticMeshFromGLTF(fullPath.string(), tempDir / "Assets" / name, am, matIdBase + 50000ULL, mats))
            {
                const AssetHandle h = mesh->handle;
                retainedGLTFMeshes.push_back(mesh);

                Entity e = SceneManager::CreateEntity(scene.get(), name, EntityType_Node);
                auto &tr = e.GetComponent<TransformComponent>();
                tr.local.translation = pos;
                tr.local.scale = glm::vec3(scale);
                tr.world = tr.local;
                tr.dirty = true;
                auto &rc = e.GetComponent<RenderingComponent>();
                rc.mobility = EntityMobility::Static;
                rc.castShadow = true;
                rc.receiveShadow = true;
                auto &smc = e.AddComponent<StaticMeshComponent>();
                smc.handle = h;
                for (auto &m : mats) retainedGLTFMats.push_back(m);
            }
            return mats;
        };

        auto helmetMats  = tryLoadGLTF("DamagedHelmet/glTF/DamagedHelmet.gltf",         "DamagedHelmet",     { -4.0f, 1.0f, 0.0f }, 0.8f, 9801000ULL);
        auto coatMats    = tryLoadGLTF("ClearCoatCarPaint/glTF/ClearCoatCarPaint.gltf", "ClearCoatCarPaint", { -2.0f, 0.8f, 0.0f }, 0.8f, 9802000ULL);
        auto sheenMats   = tryLoadGLTF("SheenChair/glTF/SheenChair.gltf",               "SheenChair",        {  0.0f, 0.0f, 0.0f }, 1.0f, 9803000ULL);
        auto spheresMats = tryLoadGLTF("MetalRoughSpheres/glTF/MetalRoughSpheres.gltf", "MetalRoughSpheres", {  2.0f, 0.8f, 0.0f }, 0.3f, 9804000ULL);
        auto dragonMats  = tryLoadGLTF("DragonAttenuation/glTF/DragonAttenuation.gltf", "DragonAttenuation", {  4.0f, 0.0f, 0.0f }, 0.4f, 9805000ULL);
        auto carbonMats  = tryLoadGLTF("CarbonFibre/glTF/CarbonFibre.gltf",             "CarbonFibre",       {  6.0f, 0.5f, 0.0f }, 0.8f, 9806000ULL);

        if (!coatMats.empty())
        {
            bool anyCoat = false;
            for (const auto &m : coatMats) if (m->gpuData.coatWeight > 0.25f) anyCoat = true;
            EXPECT_TRUE(anyCoat);
        }
        if (!sheenMats.empty())
        {
            bool anyFuzz = false;
            for (const auto &m : sheenMats) if (m->gpuData.fuzzWeight > 0.25f) anyFuzz = true;
            EXPECT_TRUE(anyFuzz);
        }
        if (!dragonMats.empty())
        {
            bool anyTrans = false;
            for (const auto &m : dragonMats) if (m->gpuData.transmissionWeight > 0.25f) anyTrans = true;
            EXPECT_TRUE(anyTrans);
        }

        // Local Shadow Stage at Z = 6.0 (Static pedestal + Movable blocker under Point Light + Spot Light)
        const AssetHandle stageFloorMat = CreateValidationMaterial(am, 9705001ULL, "StageFloor", glm::vec3(0.85f), 0.0f, 0.9f, 0.2f);
        const AssetHandle blockerMat    = CreateValidationMaterial(am, 9705002ULL, "ShadowBlocker", glm::vec3(0.35f), 0.0f, 0.8f, 0.2f);

        Entity shadowFloor = AddValidationMesh(scene.get(), "ShadowFloor", EMeshType::CUBE, { 0.0f, -0.25f, 6.0f }, { 14.0f, 0.5f, 10.0f }, stageFloorMat,
            EntityMobility::Static, true, true);
        Entity movableBlocker = AddValidationMesh(scene.get(), "MovableOccluder", EMeshType::CUBE,
            { 0.0f, 1.5f, 6.0f }, { 1.4f, 0.4f, 1.4f }, blockerMat, EntityMobility::Movable, true, true);

        Entity pointLight = SceneManager::CreateEntity(scene.get(), "BakedStaticAndMovablePointLight", EntityType_Node);
        {
            auto &tr = pointLight.GetComponent<TransformComponent>();
            tr.local.translation = glm::vec3(0.0f, 3.2f, 6.0f);
            tr.world = tr.local;
            tr.dirty = true;
            auto &pl = pointLight.AddComponent<PointLightComponent>();
            pl.color = glm::vec4(1.0f, 0.95f, 0.85f, 1.0f);
            pl.intensity = 24.0f;
            pl.range = 12.0f;
            pl.castShadow = true;
            pl.bakeStaticShadow = true;
            pl.softShadows = false;
            pl.shadowStrength = 1.0f;
        }

        Entity spotLight = SceneManager::CreateEntity(scene.get(), "DynamicSpotLight", EntityType_Node);
        {
            auto &tr = spotLight.GetComponent<TransformComponent>();
            tr.local.translation = glm::vec3(-4.0f, 3.5f, 8.0f);
            tr.local.rotation = glm::quat(glm::vec3(glm::radians(40.0f), glm::radians(35.0f), 0.0f));
            tr.world = tr.local;
            tr.dirty = true;
            auto &sl = spotLight.AddComponent<SpotLightComponent>();
            sl.color = glm::vec4(0.75f, 0.88f, 1.0f, 1.0f);
            sl.intensity = 15.0f;
            sl.range = 14.0f;
            sl.innerConeAngle = 15.0f;
            sl.outerConeAngle = 35.0f;
            sl.castShadow = true;
            sl.bakeStaticShadow = false;
            sl.softShadows = true;
        }

        // Render offscreen and verify movable occluder casts a real runtime local point shadow on Static floor
        ICamera cam;
        cam.fov = 55.0f;
        cam.nearPlane = 0.1f;
        cam.farPlane = 80.0f;
        cam.position = glm::vec3(0.0f, 5.2f, 12.2f);
        cam.SetView(glm::lookAt(cam.position, glm::vec3(0.0f, 0.0f, 6.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
        cam.postProcessing.enableSSAO = false;
        cam.postProcessing.taaProperties.enable = false;
        cam.UpdateProjection(320, 240);

        Ref<SceneRenderer> sr = CreateRef<SceneRenderer>();
        sr->SetActiveScene(scene);
        sr->sceneRenderSettings.enableDepthPrepass = true;
        sr->sceneRenderSettings.aoModeOverride = 0;
        sr->ResizeFramebuffer(&cam, 320, 240);

        Renderer *renderer = Application::GetInstance()->GetRenderer();
        ASSERT_NE(renderer, nullptr);
        uint64_t frameIdx = 1;
        auto renderTwoFrames = [&]()
        {
            scene->UpdateTransforms(0.016f);
            for (int i = 0; i < 2; ++i)
            {
                renderer->BeginFrame(frameIdx++);
                sr->BeginFrame();
                sr->Render(&cam, Renderer::GetCurrentFrameContext(), false);
            }
            GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());
        };

        renderTwoFrames();

        Ref<CameraRenderTarget> rt = sr->GetRenderTarget(&cam);
        ASSERT_NE(rt, nullptr);
        ASSERT_NE(rt->sceneRT, nullptr);
        Ref<Texture> sceneCol = rt->sceneRT->GetColorAttachment(0);
        ASSERT_NE(sceneCol, nullptr);

        uint32_t sx = 0, sy = 0, lx = 0, ly = 0;
        ASSERT_TRUE(ProjectWorldToPixel(cam, { 0.0f, 0.0f, 6.0f }, 320, 240, sx, sy));
        ASSERT_TRUE(ProjectWorldToPixel(cam, { 2.5f, 0.0f, 6.0f }, 320, 240, lx, ly));

        {
            std::vector<uint8_t> bytes;
            uint32_t rowPitch = 0, slicePitch = 0;
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(sceneCol, 0, 0, bytes, rowPitch, slicePitch));
            const glm::vec4 shadowPx = SampleRGBA16FAvg3x3(bytes, rowPitch, 320, 240, sx, sy);
            const glm::vec4 litPx    = SampleRGBA16FAvg3x3(bytes, rowPitch, 320, 240, lx, ly);
            EXPECT_GT(litPx.r + litPx.g + litPx.b, shadowPx.r + shadowPx.g + shadowPx.b + 0.05f);
        }

        // Toggle receiveShadow = false on ShadowFloor -> previously shadowed spot becomes unshadowed
        auto &floorRC = shadowFloor.GetComponent<RenderingComponent>();
        floorRC.receiveShadow = false;
        renderTwoFrames();
        {
            std::vector<uint8_t> bytes;
            uint32_t rowPitch = 0, slicePitch = 0;
            ASSERT_TRUE(Texture::ReadbackTextureSubresource(sceneCol, 0, 0, bytes, rowPitch, slicePitch));
            const glm::vec4 unshadowedPx = SampleRGBA16FAvg3x3(bytes, rowPitch, 320, 240, sx, sy);
            EXPECT_GT(unshadowedPx.r + unshadowedPx.g + unshadowedPx.b, 0.3f);
        }
        floorRC.receiveShadow = true;

        SaveValidationScene(scene, project.get(), "GI_OpenPBRShowcase.ixscene");

        GPUUploadSync::DeviceWaitIdle(DeviceManager::GetInstance()->GetDevice());
        sr->SetActiveScene(nullptr);
        sr.reset();
        project->SetActiveScene(nullptr);
        scene.reset();
        retainedGLTFMeshes.clear();
        retainedGLTFMats.clear();
        am->Reset();
        Project::CloseActive();
        std::filesystem::remove_all(tempDir, ec);
    }
}
