// Copyright (c) 2026 Evangelion Manuhutu

#include "rust_test.hpp"
#include "ignite/core/logger.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <filesystem>

// -------------------------------------------------
// Rust FFI Interop & Engine Lifecycle Tests
// -------------------------------------------------

TEST(RustInterop, TestConnection)
{
    int connectionResult = ignite_rs_test_connection();
    EXPECT_EQ(connectionResult, 0x52555354); // RUST magic number
}

TEST(RustInterop, ResultProtocol)
{
    EXPECT_STREQ(ignite_result_to_string(IgniteResult_Ok), "Ok");
    EXPECT_STREQ(ignite_result_to_string(IgniteResult_ErrNullPointer), "ErrNullPointer");
    EXPECT_STREQ(ignite_result_to_string(IgniteResult_ErrInvalidHandle), "ErrInvalidHandle");
    EXPECT_STREQ(ignite_result_to_string(IgniteResult_ErrInvalidParam), "ErrInvalidParam");
    EXPECT_STREQ(ignite_result_to_string(IgniteResult_ErrNotFound), "ErrNotFound");
    EXPECT_STREQ(ignite_result_to_string(IgniteResult_ErrAlreadyExists), "ErrAlreadyExists");
    EXPECT_STREQ(ignite_result_to_string(IgniteResult_ErrOperationFailed), "ErrOperationFailed");
    EXPECT_STREQ(ignite_result_to_string(IgniteResult_ErrUnknown), "ErrUnknown");
}

TEST(RustInterop, EngineLifecycle)
{
    // The engine is initialized by Application startup
    bool isInit = ignite_rs_engine_is_initialized();
    if (!isInit)
    {
        EXPECT_TRUE(ignite_rs_engine_init());
        EXPECT_TRUE(ignite_rs_engine_is_initialized());
    }

    // Calling init again when already initialized should return false
    EXPECT_FALSE(ignite_rs_engine_init());

    // Test shutdown and re-initialization
    EXPECT_TRUE(ignite_rs_engine_shutdown());
    EXPECT_FALSE(ignite_rs_engine_is_initialized());

    // Re-initialize for Application lifetime
    EXPECT_TRUE(ignite_rs_engine_init());
    EXPECT_TRUE(ignite_rs_engine_is_initialized());
}

TEST(RustInterop, AssetHandleAndTypes)
{
    // Test Rust AssetHandle creation and validation
    uint64_t nullHandle = ignite_asset_handle_create(0);
    EXPECT_EQ(nullHandle, 0u);
    EXPECT_FALSE(ignite_asset_handle_is_valid(nullHandle));

    uint64_t validHandle = ignite_asset_handle_create(0x123456789ABCDEF0ULL);
    EXPECT_EQ(validHandle, 0x123456789ABCDEF0ULL);
    EXPECT_TRUE(ignite_asset_handle_is_valid(validHandle));

    // Test Rust AssetType enum to string FFI conversion
    const char *textureStr = ignite_asset_type_to_string(AssetType_RS_Texture);
    ASSERT_NE(textureStr, nullptr);
    EXPECT_STREQ(textureStr, "Texture");

    const char *sceneStr = ignite_asset_type_to_string(AssetType_RS_Scene);
    ASSERT_NE(sceneStr, nullptr);
    EXPECT_STREQ(sceneStr, "Scene");

    const char *meshStr = ignite_asset_type_to_string(AssetType_RS_Mesh);
    ASSERT_NE(meshStr, nullptr);
    EXPECT_STREQ(meshStr, "Mesh");

    const char *prefabStr = ignite_asset_type_to_string(AssetType_RS_Prefab);
    ASSERT_NE(prefabStr, nullptr);
    EXPECT_STREQ(prefabStr, "Prefab");
}

TEST(RustInterop, UUIDPrimitives)
{
    uint64_t randomUuid = ignite_uuid_new();
    EXPECT_NE(randomUuid, 0u);

    uint64_t customUuid = ignite_uuid_from_u64(12345ULL);
    EXPECT_EQ(customUuid, 12345ULL);
}

// -------------------------------------------------
// Logging Bridge Test
// -------------------------------------------------

static std::vector<std::pair<IgniteLogLevel, std::string>> g_CapturedLogs;

static void TestLogCallback(IgniteLogLevel level, const char* message)
{
    if (message)
    {
        g_CapturedLogs.push_back({ level, std::string(message) });
    }
}

TEST(RustInterop, LoggingBridge)
{
    g_CapturedLogs.clear();

    // Register log callback
    IgniteResult regResult = ignite_rs_log_register_callback(TestLogCallback);
    EXPECT_EQ(regResult, IgniteResult_Ok);

    // Send log messages across FFI
    EXPECT_EQ(ignite_rs_log(IgniteLogLevel_Info, "Hello from C++ FFI Test!"), IgniteResult_Ok);
    EXPECT_EQ(ignite_rs_log(IgniteLogLevel_Warn, "Warning event from Rust FFI!"), IgniteResult_Ok);
    EXPECT_EQ(ignite_rs_log(IgniteLogLevel_Error, "Error event from Rust FFI!"), IgniteResult_Ok);

    // Test null message error safety
    EXPECT_EQ(ignite_rs_log(IgniteLogLevel_Info, nullptr), IgniteResult_ErrNullPointer);

    // Verify received log calls
    ASSERT_EQ(g_CapturedLogs.size(), 3u);
    EXPECT_EQ(g_CapturedLogs[0].first, IgniteLogLevel_Info);
    EXPECT_STREQ(g_CapturedLogs[0].second.c_str(), "Hello from C++ FFI Test!");

    EXPECT_EQ(g_CapturedLogs[1].first, IgniteLogLevel_Warn);
    EXPECT_STREQ(g_CapturedLogs[1].second.c_str(), "Warning event from Rust FFI!");

    EXPECT_EQ(g_CapturedLogs[2].first, IgniteLogLevel_Error);
    EXPECT_STREQ(g_CapturedLogs[2].second.c_str(), "Error event from Rust FFI!");

    // Unregister callback
    EXPECT_EQ(ignite_rs_log_unregister_callback(), IgniteResult_Ok);
    
    // Calling log after unregistering should return ErrNotFound
    EXPECT_EQ(ignite_rs_log(IgniteLogLevel_Info, "Should fail"), IgniteResult_ErrNotFound);
}

// -------------------------------------------------
// Memory Allocation Safety Test
// -------------------------------------------------

TEST(RustInterop, MemoryBoundary)
{
    uint64_t handle = 0;
    const uint8_t* ptr = nullptr;
    const size_t bufferSize = 1024;
    const uint8_t fillValue = 0xAB;

    // Test allocating buffer in Rust (Rust owns memory)
    IgniteResult allocResult = ignite_test_alloc_buffer(bufferSize, fillValue, &handle, &ptr);
    EXPECT_EQ(allocResult, IgniteResult_Ok);
    EXPECT_NE(handle, 0u);
    ASSERT_NE(ptr, nullptr);

    // Verify C++ can read the memory allocated by Rust
    EXPECT_EQ(ptr[0], fillValue);
    EXPECT_EQ(ptr[512], fillValue);
    EXPECT_EQ(ptr[1023], fillValue);

    // Free buffer via Rust handle
    IgniteResult freeResult = ignite_test_free_buffer(handle);
    EXPECT_EQ(freeResult, IgniteResult_Ok);

    // Double free should fail safely with ErrNotFound
    EXPECT_EQ(ignite_test_free_buffer(handle), IgniteResult_ErrNotFound);

    // Invalid handle (0) should fail with ErrInvalidHandle
    EXPECT_EQ(ignite_test_free_buffer(0), IgniteResult_ErrInvalidHandle);

    // Null output parameters error safety
    EXPECT_EQ(ignite_test_alloc_buffer(bufferSize, fillValue, nullptr, &ptr), IgniteResult_ErrNullPointer);
    EXPECT_EQ(ignite_test_alloc_buffer(bufferSize, fillValue, &handle, nullptr), IgniteResult_ErrNullPointer);
    EXPECT_EQ(ignite_test_alloc_buffer(0, fillValue, &handle, &ptr), IgniteResult_ErrInvalidParam);
}

// -------------------------------------------------
// Phase 1: Core Utilities FFI Tests
// -------------------------------------------------

TEST(RustInterop, Phase1StringUtils)
{
    EXPECT_TRUE(ignite_rs_string_ends_with("texture.png", ".png"));
    EXPECT_FALSE(ignite_rs_string_ends_with("texture.png", ".jpg"));
    EXPECT_FALSE(ignite_rs_string_ends_with(nullptr, ".png"));
    EXPECT_FALSE(ignite_rs_string_ends_with("texture.png", nullptr));

    char lowerBuf[64] = {0};
    size_t lowerLen = ignite_rs_string_to_lower("IGNITE ENGINE 2026", lowerBuf, sizeof(lowerBuf));
    EXPECT_EQ(lowerLen, 18u);
    EXPECT_STREQ(lowerBuf, "ignite engine 2026");

    char trimBuf[64] = {0};
    size_t trimLen = ignite_rs_string_trim("   Hello Ignite   \t", trimBuf, sizeof(trimBuf));
    EXPECT_EQ(trimLen, 12u);
    EXPECT_STREQ(trimBuf, "Hello Ignite");
}

TEST(RustInterop, Phase1Hashing)
{
    uint64_t hash1 = ignite_rs_hash_string("Ignite");
    uint64_t hash2 = ignite_rs_hash_string("Ignite");
    uint64_t hash3 = ignite_rs_hash_string("Engine");

    EXPECT_NE(hash1, 0u);
    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
    EXPECT_EQ(ignite_rs_hash_string(nullptr), 0u);

    uint64_t combined = ignite_rs_hash_combine(100, 200);
    EXPECT_NE(combined, 100u);
    EXPECT_NE(combined, 200u);
}

TEST(RustInterop, Phase1Timers)
{
    uint64_t timerId = ignite_rs_timer_create();
    EXPECT_NE(timerId, 0u);

    float elapsed = ignite_rs_timer_elapsed_seconds(timerId);
    EXPECT_GE(elapsed, 0.0f);

    EXPECT_TRUE(ignite_rs_timer_reset(timerId));
    EXPECT_TRUE(ignite_rs_timer_destroy(timerId));
    EXPECT_FALSE(ignite_rs_timer_destroy(timerId)); // Already destroyed
}

TEST(RustInterop, Phase1SignalBus)
{
    EXPECT_EQ(ignite_rs_signal_publish("on_test_signal", nullptr, 0), IgniteResult_Ok);
    EXPECT_EQ(ignite_rs_signal_publish(nullptr, nullptr, 0), IgniteResult_ErrNullPointer);
}

// -------------------------------------------------
// Phase 2: Asset System & Pinning FFI Tests
// -------------------------------------------------

TEST(RustInterop, Phase2AssetRegistryAndMetadata)
{
    const uint64_t handle = 0xABCD12345678ULL;
    const char* path = "Assets/Textures/player.png";

    // Assign metadata in Rust asset registry
    IgniteResult assignRes = ignite_rs_asset_assign_metadata(handle, path, AssetType_RS_Texture);
    EXPECT_EQ(assignRes, IgniteResult_Ok);

    // Retrieve metadata
    char outBuf[128] = {0};
    AssetType_RS outType = AssetType_RS_Invalid;
    IgniteResult getRes = ignite_rs_asset_get_metadata(handle, outBuf, sizeof(outBuf), &outType);
    EXPECT_EQ(getRes, IgniteResult_Ok);
    EXPECT_STREQ(outBuf, path);
    EXPECT_EQ(outType, AssetType_RS_Texture);

    // Remove metadata
    IgniteResult removeRes = ignite_rs_asset_remove_metadata(handle);
    EXPECT_EQ(removeRes, IgniteResult_Ok);

    // Get after remove should return ErrNotFound
    EXPECT_EQ(ignite_rs_asset_get_metadata(handle, outBuf, sizeof(outBuf), &outType), IgniteResult_ErrNotFound);
}

TEST(RustInterop, Phase2AssetPinningRefCounting)
{
    const uint64_t validHandle = 0x999988887777ULL;
    const uint64_t invalidHandle = 0ULL; // AssetHandle(0) must never be pinned (Rule 13)

    // Rule 13: Pinning handle 0 should return ErrInvalidHandle
    EXPECT_EQ(ignite_rs_asset_pin(invalidHandle), IgniteResult_ErrInvalidHandle);
    EXPECT_FALSE(ignite_rs_asset_is_pinned(invalidHandle));
    EXPECT_EQ(ignite_rs_asset_get_pin_count(invalidHandle), 0u);

    // Pinning valid handle
    EXPECT_EQ(ignite_rs_asset_pin(validHandle), IgniteResult_Ok);
    EXPECT_TRUE(ignite_rs_asset_is_pinned(validHandle));
    EXPECT_EQ(ignite_rs_asset_get_pin_count(validHandle), 1u);

    // Increment pin count
    EXPECT_EQ(ignite_rs_asset_pin(validHandle), IgniteResult_Ok);
    EXPECT_EQ(ignite_rs_asset_get_pin_count(validHandle), 2u);

    // Decrement pin count
    EXPECT_EQ(ignite_rs_asset_unpin(validHandle), IgniteResult_Ok);
    EXPECT_EQ(ignite_rs_asset_get_pin_count(validHandle), 1u);

    // Final unpin removes pin
    EXPECT_EQ(ignite_rs_asset_unpin(validHandle), IgniteResult_Ok);
    EXPECT_FALSE(ignite_rs_asset_is_pinned(validHandle));
    EXPECT_EQ(ignite_rs_asset_get_pin_count(validHandle), 0u);

    // Unpin when not pinned returns ErrNotFound
    EXPECT_EQ(ignite_rs_asset_unpin(validHandle), IgniteResult_ErrNotFound);
}

// -------------------------------------------------
// Phase 3: Serialization FFI Tests (serde & bincode)
// -------------------------------------------------

TEST(RustInterop, Phase3YamlSerialization)
{
    const uint64_t handle = 0x555544443333ULL;
    const char* path = "Assets/Scenes/Level1.ixscene";

    char yamlBuf[256] = {0};
    IgniteResult serRes = ignite_rs_serialize_metadata_yaml(handle, path, AssetType_RS_Scene, yamlBuf, sizeof(yamlBuf));
    EXPECT_EQ(serRes, IgniteResult_Ok);
    EXPECT_NE(strstr(yamlBuf, "filepath"), nullptr);
    EXPECT_NE(strstr(yamlBuf, "Scene"), nullptr);

    // Deserialize back from YAML
    char outPathBuf[256] = {0};
    AssetType_RS outType = AssetType_RS_Invalid;
    IgniteResult deserRes = ignite_rs_deserialize_metadata_yaml(yamlBuf, outPathBuf, sizeof(outPathBuf), &outType);
    EXPECT_EQ(deserRes, IgniteResult_Ok);
    EXPECT_STREQ(outPathBuf, path);
    EXPECT_EQ(outType, AssetType_RS_Scene);
}

TEST(RustInterop, Phase3BinarySerialization)
{
    const uint64_t handle = 0x777766665555ULL;
    const char* path = "Assets/Models/knight.mesh";

    uint64_t bufHandle = 0;
    const uint8_t* ptr = nullptr;
    size_t len = 0;

    IgniteResult serRes = ignite_rs_serialize_metadata_binary(handle, path, AssetType_RS_Mesh, &bufHandle, &ptr, &len);
    EXPECT_EQ(serRes, IgniteResult_Ok);
    EXPECT_NE(bufHandle, 0u);
    ASSERT_NE(ptr, nullptr);
    EXPECT_GT(len, 0u);
}

// -------------------------------------------------
// Phase 4: ECS & Scene FFI Tests
// -------------------------------------------------

TEST(RustInterop, Phase4EcsSceneLifecycle)
{
    uint64_t sceneHandle = ignite_rs_scene_create("TestScene_Rust");
    EXPECT_NE(sceneHandle, 0u);

    uint64_t entityId = ignite_rs_scene_create_entity(sceneHandle, "PlayerEntity");
    EXPECT_NE(entityId, 0u);

    char nameBuf[64] = {0};
    IgniteResult nameRes = ignite_rs_entity_get_name(sceneHandle, entityId, nameBuf, sizeof(nameBuf));
    EXPECT_EQ(nameRes, IgniteResult_Ok);
    EXPECT_STREQ(nameBuf, "PlayerEntity");

    float pos[3] = { 1.0f, 2.0f, 3.0f };
    float rot[3] = { 0.0f, 0.0f, 0.0f };
    float scale[3] = { 1.0f, 1.0f, 1.0f };

    IgniteResult setTransformRes = ignite_rs_entity_set_transform(sceneHandle, entityId, pos, rot, scale);
    EXPECT_EQ(setTransformRes, IgniteResult_Ok);

    float outPos[3] = {0};
    float outRot[3] = {0};
    float outScale[3] = {0};
    IgniteResult getTransformRes = ignite_rs_entity_get_transform(sceneHandle, entityId, outPos, outRot, outScale);
    EXPECT_EQ(getTransformRes, IgniteResult_Ok);
    EXPECT_FLOAT_EQ(outPos[0], 1.0f);
    EXPECT_FLOAT_EQ(outPos[1], 2.0f);
    EXPECT_FLOAT_EQ(outPos[2], 3.0f);

    EXPECT_EQ(ignite_rs_scene_destroy_entity(sceneHandle, entityId), IgniteResult_Ok);
    EXPECT_EQ(ignite_rs_scene_destroy(sceneHandle), IgniteResult_Ok);
}

// -------------------------------------------------
// Phase 5: Project Management & MochiSharp Scripting Bridge FFI Tests
// -------------------------------------------------

static int g_ScriptTickCount = 0;
static void MockMochiSharpScriptTick(float dt)
{
    g_ScriptTickCount++;
}

TEST(RustInterop, Phase5ProjectAndScriptingBridge)
{
    // Test Rust Project Creation FFI
    uint64_t projHandle = ignite_rs_project_create("DemoProject", "D:/Projects/Demo");
    EXPECT_NE(projHandle, 0u);

    char projName[64] = {0};
    EXPECT_EQ(ignite_rs_project_get_name(projHandle, projName, sizeof(projName)), IgniteResult_Ok);
    EXPECT_STREQ(projName, "DemoProject");

    char assetDir[256] = {0};
    EXPECT_EQ(ignite_rs_project_get_asset_directory(projHandle, assetDir, sizeof(assetDir)), IgniteResult_Ok);
    EXPECT_NE(strstr(assetDir, "Assets"), nullptr);

    EXPECT_EQ(ignite_rs_project_destroy(projHandle), IgniteResult_Ok);

    // Test MochiSharp C# Scripting Bridge Callback FFI
    g_ScriptTickCount = 0;
    EXPECT_EQ(ignite_rs_script_register_tick_callback(MockMochiSharpScriptTick), IgniteResult_Ok);
    EXPECT_TRUE(ignite_rs_script_trigger_tick(0.016f));
    EXPECT_EQ(g_ScriptTickCount, 1);

    EXPECT_EQ(ignite_rs_script_unregister_tick_callback(), IgniteResult_Ok);
    EXPECT_FALSE(ignite_rs_script_trigger_tick(0.016f));
}

// -------------------------------------------------
// FPS Game Test Case (.ixreg) Asset Registry Deserialization
// -------------------------------------------------

TEST(RustInterop, FpsGameAssetRegistryRoundtrip)
{
    std::filesystem::path targetFile = "fps_game_test_case/AssetRegistry.ixreg";
    if (!std::filesystem::exists(targetFile))
    {
        targetFile = "D:/Dev/Ignite/Bin/Debug/fps_game_test_case/AssetRegistry.ixreg";
    }
    if (!std::filesystem::exists(targetFile))
    {
        targetFile = "../../Bin/Debug/fps_game_test_case/AssetRegistry.ixreg";
    }

    std::filesystem::path absPath = std::filesystem::absolute(targetFile);
    std::string pathStr = absPath.generic_string();

    // Load and deserialize the entire 28.6KB FPS Game AssetRegistry.ixreg in Rust
    size_t loadedCount = ignite_rs_load_asset_registry_file(pathStr.c_str());
    EXPECT_GT(loadedCount, 0u);

    // Verify specific entries deserialized accurately from fps_game_test_case
    // Material 18 handle: 42972961298889726
    const uint64_t material18Handle = 42972961298889726ULL;
    char outPath[256] = {0};
    AssetType_RS outType = AssetType_RS_Invalid;
    EXPECT_EQ(ignite_rs_asset_get_metadata(material18Handle, outPath, sizeof(outPath), &outType), IgniteResult_Ok);
    EXPECT_STREQ(outPath, "Assets/StaticMeshes/Sponza/Material_18.ixmat");
    EXPECT_EQ(outType, AssetType_RS_Material);

    // StaticMesh Sponza handle: 1801218881103323832
    const uint64_t sponzaMeshHandle = 1801218881103323832ULL;
    memset(outPath, 0, sizeof(outPath));
    outType = AssetType_RS_Invalid;
    EXPECT_EQ(ignite_rs_asset_get_metadata(sponzaMeshHandle, outPath, sizeof(outPath), &outType), IgniteResult_Ok);
    EXPECT_STREQ(outPath, "Assets/StaticMeshes/Sponza/Sponza.mesh");
    EXPECT_EQ(outType, AssetType_RS_StaticMesh);
}
