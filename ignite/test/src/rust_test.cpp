// Copyright (c) 2026 Evangelion Manuhutu

#include "rust_test.hpp"
#include "ignite/core/logger.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <string>

// -------------------------------------------------
// Rust FFI Interop & Engine Lifecycle Tests
// -------------------------------------------------

TEST(RustInterop, TestConnection)
{
    int connectionResult = ignite_rust_test_connection();
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
    // Test initializing Rust engine singleton
    bool initResult = ignite_engine_rs_init();
    EXPECT_TRUE(initResult);
    EXPECT_TRUE(ignite_engine_rs_is_initialized());

    // Calling init again should return false (already initialized)
    EXPECT_FALSE(ignite_engine_rs_init());

    // Shutdown
    bool shutdownResult = ignite_engine_rs_shutdown();
    EXPECT_TRUE(shutdownResult);
    EXPECT_FALSE(ignite_engine_rs_is_initialized());
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
