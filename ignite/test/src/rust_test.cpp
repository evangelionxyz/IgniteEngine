// Copyright (c) 2026 Evangelion Manuhutu

#include "rust_test.hpp"
#include <gtest/gtest.h>

// -------------------------------------------------
// Rust FFI Interop & Engine Lifecycle Tests
// -------------------------------------------------
TEST(RustInterop, TestConnection)
{
    int connectionResult = ignite_rust_test_connection();
    EXPECT_EQ(connectionResult, 0x52555354);
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
}

TEST(RustInterop, UUIDPrimitives)
{
    uint64_t randomUuid = ignite_uuid_new();
    EXPECT_NE(randomUuid, 0u);

    uint64_t customUuid = ignite_uuid_from_u64(12345ULL);
    EXPECT_EQ(customUuid, 12345ULL);
}
