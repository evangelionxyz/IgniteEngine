// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/file_watcher.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"
#include "ignite/core/vfs/vfs.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/objects/mesh_builder.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/thumbnail_generator.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/binary_serializer.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace ignite;

namespace
{
    Ref<Project> CreateTempProject(const std::string &name, std::filesystem::path &outDir)
    {
        const std::filesystem::path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
        outDir = testResourcesRoot / "temp" / name;

        std::error_code ec;
        if (std::filesystem::exists(outDir, ec))
        {
            std::filesystem::remove_all(outDir, ec);
        }
        std::filesystem::create_directories(outDir, ec);

        ProjectInfo info{};
        info.name = name;
        info.filepath = outDir / (name + ".ixproj");
        info.rootDirectory = outDir;
        info.assetDirectory = "Assets";
        info.scriptsDirectory = "Scripts";
        info.assetRegistryFilepath = "AssetRegistry.ixreg";
        info.configuration = ProjectConfiguration::Debug;

        Ref<Project> project = Project::Create(info);
        if (project)
        {
            AssetManager::GetInstance()->SetActiveProject(project);
        }
        return project;
    }

    void PumpEngineQueuesOnce(uint64_t &frameIdx, ThumbnailGenerator *thumbGen = nullptr)
    {
        FileWatcher::Update();
        Application::GetInstance()->ProcessMainThreadSubmissions();
        Texture::ProcessAsyncUploads(8, 32 * 1024 * 1024);

        if (auto *am = AssetManager::GetInstance())
        {
            am->OnUpdate(0.016f);
        }

        if (thumbGen)
        {
            if (auto *renderer = Application::GetInstance()->GetRenderer())
            {
                renderer->BeginFrame(frameIdx++);
                thumbGen->ProcessFrame(Renderer::GetCurrentFrameContext());
            }
        }
    }

    bool HasVisiblePixelVariation(const std::vector<uint8_t> &rgba)
    {
        if (rgba.size() < 8)
            return false;

        uint8_t minR = 255, maxR = 0, minG = 255, maxG = 0, minB = 255, maxB = 0;
        for (size_t i = 0; i + 3 < rgba.size(); i += 4)
        {
            minR = std::min(minR, rgba[i + 0]);
            maxR = std::max(maxR, rgba[i + 0]);
            minG = std::min(minG, rgba[i + 1]);
            maxG = std::max(maxG, rgba[i + 1]);
            minB = std::min(minB, rgba[i + 2]);
            maxB = std::max(maxB, rgba[i + 2]);
        }

        return (maxR - minR) > 5 || (maxG - minG) > 5 || (maxB - minB) > 5;
    }
}

TEST(ThumbnailsAndWatcher, BinaryThumbnailCacheRoundtripAndValidation)
{
    std::filesystem::path projectDir;
    Ref<Project> project = CreateTempProject("ThumbCacheRoundtrip", projectDir);
    ASSERT_NE(project, nullptr);

    const AssetHandle handle(0x123456789ABCDEF0ULL);
    const std::filesystem::path cachePath = ThumbnailGenerator::GetCacheFilepath(project.get(), handle, project->GetAssetDirectory() / "Hero.mesh");
    EXPECT_EQ(cachePath.parent_path(), project->GetCacheDirectory() / "thumbnails");
    EXPECT_EQ(cachePath.extension(), ".bin");

    ThumbnailBinaryData src{};
    src.width = 72;
    src.height = 72;
    src.channels = 4;
    src.sourceTimestamp = 9876543210LL;
    src.assetHandle = static_cast<uint64_t>(handle);
    src.pixels.resize(72u * 72u * 4u);
    for (size_t i = 0; i < src.pixels.size(); ++i)
    {
        src.pixels[i] = static_cast<uint8_t>((i * 37u + 13u) & 0xFFu);
    }

    ASSERT_TRUE(BinarySerializer::SerializeThumbnail(src, cachePath));
    EXPECT_TRUE(std::filesystem::exists(cachePath));

    ThumbnailBinaryData dst{};
    ASSERT_TRUE(BinarySerializer::DeserializeThumbnail(cachePath, dst));
    EXPECT_EQ(dst.width, src.width);
    EXPECT_EQ(dst.height, src.height);
    EXPECT_EQ(dst.channels, src.channels);
    EXPECT_EQ(dst.sourceTimestamp, src.sourceTimestamp);
    EXPECT_EQ(dst.assetHandle, src.assetHandle);
    EXPECT_EQ(dst.pixels, src.pixels);

    // Corrupt magic must be rejected cleanly
    const std::filesystem::path badPath = project->GetCacheDirectory() / "thumbnails" / "corrupt.bin";
    {
        std::ofstream bad(badPath, std::ios::binary | std::ios::trunc);
        bad << "BAD!1234567890";
    }
    ThumbnailBinaryData badOut{};
    EXPECT_FALSE(BinarySerializer::DeserializeThumbnail(badPath, badOut));

    AssetManager::GetInstance()->Reset();
}

TEST(ThumbnailsAndWatcher, MaterialSphereAndGltfMeshThumbnailGenerationAndCaching)
{
    std::filesystem::path projectDir;
    Ref<Project> project = CreateTempProject("ThumbGltfGeneration", projectDir);
    ASSERT_NE(project, nullptr);

    AssetManager *am = AssetManager::GetInstance();
    ASSERT_NE(am, nullptr);

    // 1. Create a Material asset (.ixmat)
    Ref<Material> testMat = CreateRef<Material>();
    testMat->name = "RedMetallicMat";
    testMat->gpuData.baseColorFactor = glm::vec4(0.95f, 0.15f, 0.12f, 1.0f);
    testMat->gpuData.metallicFactor = 0.8f;
    testMat->gpuData.roughnessFactor = 0.25f;

    const std::filesystem::path matPath = project->GetAssetDirectory() / "RedMetallicMat.ixmat";
    ASSERT_TRUE(testMat->Serialize(matPath));

    const AssetHandle matHandle = AssetHandle();
    testMat->handle = matHandle;
    am->AssignMetaData(matHandle, { project->GetProjectRelativeFilepath(matPath), AssetType::Material });
    am->AssignAsset(matHandle, testMat);

    // 2. Import a real glTF model (Avocado if available, otherwise procedural cube mesh)
    AssetHandle meshHandle = AssetHandle();
    std::filesystem::path meshPath;
    const std::filesystem::path avocadoGltf = "E:/3D Models/glTF-Sample-Models/2.0/Avocado/glTF/Avocado.gltf";
    if (std::filesystem::exists(avocadoGltf))
    {
        AssetMetaData srcMeta{ avocadoGltf, AssetType::StaticMesh };
        StaticMeshImportPayload payload{};
        payload.targetDirectory = project->GetAssetDirectory() / "Avocado";
        payload.importMaterials = true;
        payload.forceRebuild = true;

        Ref<StaticMesh> avocadoMesh = AssetImporter::ImportStaticMesh(meshHandle, srcMeta, am, payload);
        ASSERT_NE(avocadoMesh, nullptr);
        ASSERT_FALSE(avocadoMesh->GetMeshInstances().empty());
        meshPath = project->GetProjectFilepath(am->GetMetaData(meshHandle).filepath);
        ASSERT_TRUE(std::filesystem::exists(meshPath));
        EXPECT_TRUE(am->IsMeshSourceWatched(meshHandle));
    }
    else
    {
        Ref<StaticMesh> cubeMesh = MeshBuilder::CreateCube(glm::vec3(1.5f)).BuildStaticMesh("FallbackCube", matHandle, true);
        ASSERT_NE(cubeMesh, nullptr);
        meshPath = project->GetAssetDirectory() / "FallbackCube.mesh";
        ASSERT_TRUE(cubeMesh->Serialize(meshPath));
        cubeMesh->handle = meshHandle;
        am->AssignMetaData(meshHandle, { project->GetProjectRelativeFilepath(meshPath), AssetType::StaticMesh });
        am->AssignAsset(meshHandle, cubeMesh);
    }

    ThumbnailGenerator thumbGen;
    thumbGen.SetRenderIntervalFrames(1);

    constexpr uint32_t kThumbSize = 72;
    Ref<Texture> matThumb;
    Ref<Texture> meshThumb;
    std::atomic<bool> matDone{ false };
    std::atomic<bool> meshDone{ false };

    const std::filesystem::path matCachePath = ThumbnailGenerator::GetCacheFilepath(project.get(), matHandle, matPath);
    const std::filesystem::path meshCachePath = ThumbnailGenerator::GetCacheFilepath(project.get(), meshHandle, meshPath);

    ThumbnailRequest matReq{};
    matReq.handle = matHandle;
    matReq.type = AssetType::Material;
    matReq.assetPath = matPath;
    matReq.cachePath = matCachePath;
    matReq.size = kThumbSize;
    matReq.sourceTimestamp = ThumbnailGenerator::GetFileTimestamp(matPath);
    matReq.onComplete = [&](Ref<Texture> tex) { matThumb = std::move(tex); matDone.store(true); };
    thumbGen.RequestThumbnail(std::move(matReq));

    ThumbnailRequest meshReq{};
    meshReq.handle = meshHandle;
    meshReq.type = AssetType::StaticMesh;
    meshReq.assetPath = meshPath;
    meshReq.cachePath = meshCachePath;
    meshReq.size = kThumbSize;
    meshReq.sourceTimestamp = ThumbnailGenerator::GetFileTimestamp(meshPath);
    meshReq.onComplete = [&](Ref<Texture> tex) { meshThumb = std::move(tex); meshDone.store(true); };
    thumbGen.RequestThumbnail(std::move(meshReq));

    uint64_t frameIdx = 1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while ((!matDone.load() || !meshDone.load() || !std::filesystem::exists(matCachePath) || !std::filesystem::exists(meshCachePath))
        && std::chrono::steady_clock::now() < deadline)
    {
        PumpEngineQueuesOnce(frameIdx, &thumbGen);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    thumbGen.FlushPendingReadbacks(3000);

    ASSERT_TRUE(matDone.load());
    ASSERT_TRUE(meshDone.load());
    ASSERT_NE(matThumb, nullptr);
    ASSERT_NE(meshThumb, nullptr);
    EXPECT_EQ(matThumb->GetWidth(), static_cast<int>(kThumbSize));
    EXPECT_EQ(matThumb->GetHeight(), static_cast<int>(kThumbSize));
    EXPECT_EQ(meshThumb->GetWidth(), static_cast<int>(kThumbSize));
    EXPECT_EQ(meshThumb->GetHeight(), static_cast<int>(kThumbSize));

    // Verify .cache/thumbnails/*.bin files were written and contain real non-uniform rendered pixels
    ASSERT_TRUE(std::filesystem::exists(matCachePath));
    ASSERT_TRUE(std::filesystem::exists(meshCachePath));

    ThumbnailBinaryData matBin{};
    ThumbnailBinaryData meshBin{};
    ASSERT_TRUE(BinarySerializer::DeserializeThumbnail(matCachePath, matBin));
    ASSERT_TRUE(BinarySerializer::DeserializeThumbnail(meshCachePath, meshBin));
    EXPECT_EQ(matBin.width, kThumbSize);
    EXPECT_EQ(meshBin.width, kThumbSize);
    EXPECT_TRUE(HasVisiblePixelVariation(matBin.pixels));
    EXPECT_TRUE(HasVisiblePixelVariation(meshBin.pixels));

    // Verify disk cache hit path loads directly without queueing a GPU render
    Ref<Texture> cachedMatThumb;
    std::atomic<bool> cacheHitDone{ false };
    ThumbnailRequest cacheHitReq{};
    cacheHitReq.handle = matHandle;
    cacheHitReq.type = AssetType::Material;
    cacheHitReq.assetPath = matPath;
    cacheHitReq.cachePath = matCachePath;
    cacheHitReq.size = kThumbSize;
    cacheHitReq.sourceTimestamp = ThumbnailGenerator::GetFileTimestamp(matPath);
    cacheHitReq.onComplete = [&](Ref<Texture> tex) { cachedMatThumb = std::move(tex); cacheHitDone.store(true); };
    thumbGen.RequestThumbnail(std::move(cacheHitReq));

    const auto cacheDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!cacheHitDone.load() && std::chrono::steady_clock::now() < cacheDeadline)
    {
        // Note: intentionally NOT calling thumbGen.ProcessFrame() here — proves disk cache hit works without GPU render!
        PumpEngineQueuesOnce(frameIdx, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT_TRUE(cacheHitDone.load());
    ASSERT_NE(cachedMatThumb, nullptr);
    EXPECT_EQ(thumbGen.GetPendingRenderCount(), 0u);

    // Verify invalidation removes .bin cache file
    thumbGen.InvalidateThumbnail(project.get(), matHandle, matPath);
    EXPECT_FALSE(std::filesystem::exists(matCachePath));

    thumbGen.Shutdown();
    am->Reset();
}

TEST(ThumbnailsAndWatcher, AssetJsonHotReloadViaFileWatcher)
{
    std::filesystem::path projectDir;
    Ref<Project> project = CreateTempProject("AssetJsonHotReload", projectDir);
    ASSERT_NE(project, nullptr);

    AssetManager *am = AssetManager::GetInstance();
    ASSERT_NE(am, nullptr);

    FileWatcher::SetDebounceMs(60);

    // Create and register a JSON material (.ixmat)
    Ref<Material> mat = CreateRef<Material>();
    mat->name = "WatchedMat";
    mat->gpuData.baseColorFactor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    mat->gpuData.roughnessFactor = 0.2f;

    const std::filesystem::path matPath = project->GetAssetDirectory() / "WatchedMat.ixmat";
    ASSERT_TRUE(mat->Serialize(matPath));

    const AssetHandle matHandle = AssetHandle();
    mat->handle = matHandle;
    am->AssignMetaData(matHandle, { project->GetProjectRelativeFilepath(matPath), AssetType::Material });
    am->AssignAsset(matHandle, mat);

    std::atomic<int> changeCount{ 0 };
    SignalToken token = SignalBus::Subscribe<AssetChangeSignal>([&](const AssetChangeSignal &sig)
    {
        if (sig.handle == matHandle && sig.type == AssetType::Material)
        {
            changeCount.fetch_add(1);
        }
    });

    // Give watcher thread a brief moment and ensure mtime advances
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    // Modify the .ixmat JSON on disk externally
    Ref<Material> modified = CreateRef<Material>();
    modified->name = "WatchedMatUpdated";
    modified->gpuData.baseColorFactor = glm::vec4(0.1f, 0.9f, 0.2f, 1.0f);
    modified->gpuData.roughnessFactor = 0.75f;
    ASSERT_TRUE(modified->Serialize(matPath));

    uint64_t frameIdx = 1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (changeCount.load() == 0 && std::chrono::steady_clock::now() < deadline)
    {
        PumpEngineQueuesOnce(frameIdx, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Drain one extra cycle for render-thread in-place update
    PumpEngineQueuesOnce(frameIdx, nullptr);

    SignalBus::Unsubscribe<AssetChangeSignal>(token);

    EXPECT_GT(changeCount.load(), 0);
    // Verify the EXISTING Ref<Material> pointer was updated in-place!
    EXPECT_EQ(mat->name, "WatchedMatUpdated");
    EXPECT_NEAR(mat->gpuData.baseColorFactor.r, 0.1f, 0.01f);
    EXPECT_NEAR(mat->gpuData.baseColorFactor.g, 0.9f, 0.01f);
    EXPECT_NEAR(mat->gpuData.baseColorFactor.b, 0.2f, 0.01f);
    EXPECT_NEAR(mat->gpuData.roughnessFactor, 0.75f, 0.01f);

    FileWatcher::SetDebounceMs(150);
    am->Reset();
}

TEST(ThumbnailsAndWatcher, MeshSourceTrackingOnlyIfExistsAndAutoReload)
{
    std::filesystem::path projectDir;
    Ref<Project> project = CreateTempProject("MeshSourceTracking", projectDir);
    ASSERT_NE(project, nullptr);

    AssetManager *am = AssetManager::GetInstance();
    ASSERT_NE(am, nullptr);

    FileWatcher::SetDebounceMs(60);

    // Part A: Mesh whose source path does NOT exist must NOT be watched.
    {
        Ref<StaticMesh> orphanMesh = MeshBuilder::CreateCube(glm::vec3(1.0f)).BuildStaticMesh("Orphan", AssetHandle(0), false);
        const std::filesystem::path orphanPath = project->GetAssetDirectory() / "Orphan.mesh";
        ASSERT_TRUE(orphanMesh->Serialize(orphanPath));

        const AssetHandle orphanHandle = AssetHandle();
        orphanMesh->handle = orphanHandle;
        orphanMesh->SetSourcePath(projectDir / "DoesNotExist" / "missing_model.gltf");
        ASSERT_TRUE(orphanMesh->SerializeMetaFile(Asset::GetMetaFilePath(orphanPath)));

        am->AssignMetaData(orphanHandle, { project->GetProjectRelativeFilepath(orphanPath), AssetType::StaticMesh });
        EXPECT_FALSE(am->IsMeshSourceWatched(orphanHandle));
    }

    // Part B: Mesh whose source glTF DOES exist IS watched, and editing the glTF reloads mesh + material!
    const std::filesystem::path sampleBoxDir = "E:/3D Models/glTF-Sample-Models/2.0/Box/glTF";
    if (!std::filesystem::exists(sampleBoxDir / "Box.gltf"))
    {
        GTEST_SKIP() << "Box.gltf sample not found at E:/3D Models/glTF-Sample-Models/2.0/Box/glTF";
    }

    const std::filesystem::path externalSourceDir = projectDir / "ExternalSources" / "Box";
    std::filesystem::create_directories(externalSourceDir);
    std::filesystem::copy_file(sampleBoxDir / "Box.gltf", externalSourceDir / "Box.gltf", std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(sampleBoxDir / "Box0.bin", externalSourceDir / "Box0.bin", std::filesystem::copy_options::overwrite_existing);

    const std::filesystem::path copiedGltf = externalSourceDir / "Box.gltf";
    const AssetHandle boxHandle = AssetHandle();

    AssetMetaData importMeta{ copiedGltf, AssetType::StaticMesh };
    StaticMeshImportPayload payload{};
    payload.targetDirectory = project->GetAssetDirectory() / "Box";
    payload.importMaterials = true;
    payload.forceRebuild = true;

    Ref<StaticMesh> boxMesh = AssetImporter::ImportStaticMesh(boxHandle, importMeta, am, payload);
    ASSERT_NE(boxMesh, nullptr);
    ASSERT_FALSE(boxMesh->GetMeshInstances().empty());
    EXPECT_TRUE(am->IsMeshSourceWatched(boxHandle));

    const AssetHandle boxMatHandle = boxMesh->GetMeshInstances()[0]->GetMaterialAssetHandle();
    ASSERT_NE(boxMatHandle, AssetHandle(0));

    Ref<Material> boxMat = am->GetAssetImmediate<Material>(boxMatHandle);
    ASSERT_NE(boxMat, nullptr);
    EXPECT_GT(boxMat->gpuData.baseColorFactor.r, 0.7f);
    EXPECT_LT(boxMat->gpuData.baseColorFactor.g, 0.1f);

    std::atomic<bool> meshChanged{ false };
    std::atomic<bool> matChanged{ false };
    SignalToken token = SignalBus::Subscribe<AssetChangeSignal>([&](const AssetChangeSignal &sig)
    {
        if (sig.handle == boxHandle && sig.type == AssetType::StaticMesh)
            meshChanged.store(true);
        if (sig.handle == boxMatHandle && sig.type == AssetType::Material)
            matChanged.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    // Modify Box.gltf material color from red [0.8, 0.0, 0.0, 1.0] to green [0.0, 0.85, 0.15, 1.0]
    {
        std::ifstream in(copiedGltf);
        std::stringstream ss;
        ss << in.rdbuf();
        std::string gltfText = ss.str();
        in.close();

        const std::string targetStr = "0.800000011920929,\n                    0.0,\n                    0.0,";
        const size_t pos = gltfText.find("0.800000011920929");
        ASSERT_NE(pos, std::string::npos);

        // Replace the baseColorFactor block cleanly
        const size_t arrStart = gltfText.find('[', gltfText.find("\"baseColorFactor\""));
        const size_t arrEnd = gltfText.find(']', arrStart);
        ASSERT_NE(arrStart, std::string::npos);
        ASSERT_NE(arrEnd, std::string::npos);
        gltfText.replace(arrStart, arrEnd - arrStart + 1, "[ 0.05, 0.85, 0.15, 1.0 ]");

        std::ofstream out(copiedGltf, std::ios::trunc);
        out << gltfText;
        out.close();
    }

    uint64_t frameIdx = 1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while ((!meshChanged.load() || !matChanged.load()) && std::chrono::steady_clock::now() < deadline)
    {
        PumpEngineQueuesOnce(frameIdx, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    PumpEngineQueuesOnce(frameIdx, nullptr);

    SignalBus::Unsubscribe<AssetChangeSignal>(token);

    EXPECT_TRUE(meshChanged.load());
    EXPECT_TRUE(matChanged.load());
    EXPECT_NEAR(boxMat->gpuData.baseColorFactor.r, 0.05f, 0.02f);
    EXPECT_NEAR(boxMat->gpuData.baseColorFactor.g, 0.85f, 0.02f);
    EXPECT_NEAR(boxMat->gpuData.baseColorFactor.b, 0.15f, 0.02f);

    FileWatcher::SetDebounceMs(150);
    am->Reset();
}

