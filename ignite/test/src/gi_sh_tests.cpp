// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gi/sh.hpp"
#include "ignite/graphics/gi/ibl_prefilter.hpp"
#include "ignite/graphics/texture.hpp"

#include <glm/gtc/packing.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

using namespace ignite;

TEST(GISHTests, ConstantRadianceUnityIrradiance)
{
    constexpr uint32_t kFaceSize = 32;
    std::array<std::vector<glm::vec4>, 6> storage;
    const glm::vec4 *faces[6]{};
    for (uint32_t f = 0; f < 6; ++f)
    {
        storage[f].assign(kFaceSize * kFaceSize, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        faces[f] = storage[f].data();
    }

    float backfaceFrac = -1.0f;
    gi::SHL2 sh = gi::ProjectCubemapCPU(faces, kFaceSize, &backfaceFrac);
    gi::ConvolveIrradianceOverPi(sh);

    EXPECT_NEAR(backfaceFrac, 0.0f, 1e-6f);

    constexpr int kSamples = 100;
    const float goldenAngle = gi::kPI * (3.0f - std::sqrt(5.0f));
    for (int i = 0; i < kSamples; ++i)
    {
        const float y = 1.0f - (2.0f * (static_cast<float>(i) + 0.5f)) / static_cast<float>(kSamples);
        const float r = std::sqrt(std::max(0.0f, 1.0f - y * y));
        const float phi = goldenAngle * static_cast<float>(i);
        const glm::vec3 n(std::cos(phi) * r, y, std::sin(phi) * r);

        const glm::vec3 irr = gi::EvalSHIrradiance(sh, n);
        EXPECT_NEAR(irr.r, 1.0f, 1e-4f) << "direction " << i;
        EXPECT_NEAR(irr.g, 1.0f, 1e-4f) << "direction " << i;
        EXPECT_NEAR(irr.b, 1.0f, 1e-4f) << "direction " << i;
    }
}

TEST(GISHTests, CosineLobeAnalyticMatch)
{
    constexpr uint32_t kFaceSize = 64;
    std::array<std::vector<glm::vec4>, 6> storage;
    const glm::vec4 *faces[6]{};
    for (uint32_t f = 0; f < 6; ++f)
    {
        storage[f].resize(kFaceSize * kFaceSize);
        for (uint32_t y = 0; y < kFaceSize; ++y)
        {
            for (uint32_t x = 0; x < kFaceSize; ++x)
            {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(kFaceSize),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(kFaceSize));
                const glm::vec3 dir = gi::CubemapUVToDir(f, uv);
                const float l = std::max(0.0f, dir.y);
                storage[f][y * kFaceSize + x] = glm::vec4(l, l, l, 1.0f);
            }
        }
        faces[f] = storage[f].data();
    }

    gi::SHL2 sh = gi::ProjectCubemapCPU(faces, kFaceSize);
    gi::ConvolveIrradianceOverPi(sh);

    const glm::vec3 topIrr = gi::EvalSHIrradiance(sh, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 sideIrr = gi::EvalSHIrradiance(sh, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 botIrr = gi::EvalSHIrradiance(sh, glm::vec3(0.0f, -1.0f, 0.0f));

    const float analyticTop = 2.0f / 3.0f;
    const float analyticSide = 2.0f / (3.0f * gi::kPI);
    EXPECT_NEAR(topIrr.r, analyticTop, analyticTop * 0.03f);
    EXPECT_NEAR(sideIrr.r, analyticSide, analyticTop * 0.03f);
    EXPECT_LT(botIrr.r, 0.02f);
}

TEST(GISHTests, DeltaLightPlusYWithHanningWindow)
{
    constexpr uint32_t kFaceSize = 32;
    std::array<std::vector<glm::vec4>, 6> storage;
    const glm::vec4 *faces[6]{};
    const float cosHalfAngle = std::cos(glm::radians(15.0f));

    for (uint32_t f = 0; f < 6; ++f)
    {
        storage[f].resize(kFaceSize * kFaceSize);
        for (uint32_t y = 0; y < kFaceSize; ++y)
        {
            for (uint32_t x = 0; x < kFaceSize; ++x)
            {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(kFaceSize),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(kFaceSize));
                const glm::vec3 dir = gi::CubemapUVToDir(f, uv);
                const float val = (dir.y >= cosHalfAngle) ? 20.0f : 0.0f;
                storage[f][y * kFaceSize + x] = glm::vec4(val, val * 0.5f, 0.0f, 1.0f);
            }
        }
        faces[f] = storage[f].data();
    }

    gi::SHL2 sh = gi::ProjectCubemapCPU(faces, kFaceSize);
    gi::ConvolveIrradianceOverPi(sh);
    gi::ApplyHanningWindow(sh, 6.0f);

    const glm::vec3 irrUp = gi::EvalSHIrradiance(sh, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 irrDown = gi::EvalSHIrradiance(sh, glm::vec3(0.0f, -1.0f, 0.0f));
    EXPECT_GT(irrUp.r, irrUp.g);
    EXPECT_GT(irrUp.r, 0.5f);
    EXPECT_LT(irrDown.r, irrUp.r * 0.05f);
}

TEST(GISHTests, PackUnpack7RGBARoundTrip)
{
    gi::SHL2 src{};
    for (int i = 0; i < 9; ++i)
    {
        src.c[i] = glm::vec3(
            0.1f * static_cast<float>(i + 1),
            -0.05f * static_cast<float>(i),
            0.3f * static_cast<float>(i - 4));
    }

    glm::vec4 packed[7]{};
    gi::PackSHL2To7RGBA(src, 0.875f, packed);

    gi::SHL2 dst{};
    float dstValidity = 0.0f;
    gi::UnpackSHL2From7RGBA(packed, dst, dstValidity);

    EXPECT_NEAR(dstValidity, 0.875f, 1e-6f);
    for (int i = 0; i < 9; ++i)
    {
        EXPECT_NEAR(dst.c[i].r, src.c[i].r, 1e-6f);
        EXPECT_NEAR(dst.c[i].g, src.c[i].g, 1e-6f);
        EXPECT_NEAR(dst.c[i].b, src.c[i].b, 1e-6f);
    }
}

TEST(GISHTests, GPUComputeMatchesCPUProjection)
{
    auto *dm = DeviceManager::GetInstance();
    ASSERT_NE(dm, nullptr);
    ASSERT_NE(dm->GetDevice(), nullptr);

    constexpr uint32_t kFaceSize = 32;
    std::array<std::vector<glm::vec4>, 6> cpuFaces;
    const glm::vec4 *cpuPtrs[6]{};
    std::vector<std::vector<uint8_t>> fp16Subresources(6);

    for (uint32_t f = 0; f < 6; ++f)
    {
        cpuFaces[f].resize(kFaceSize * kFaceSize);
        fp16Subresources[f].resize(kFaceSize * kFaceSize * 4u * sizeof(uint16_t));
        auto *dstHalf = reinterpret_cast<uint16_t *>(fp16Subresources[f].data());

        for (uint32_t y = 0; y < kFaceSize; ++y)
        {
            for (uint32_t x = 0; x < kFaceSize; ++x)
            {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(kFaceSize),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(kFaceSize));
                const glm::vec3 dir = gi::CubemapUVToDir(f, uv);
                const float r = 0.4f + std::max(0.0f, dir.x) * 1.5f;
                const float g = 0.2f + std::max(0.0f, dir.y) * 2.2f;
                const float b = 0.1f + std::max(0.0f, -dir.z) * 3.0f;
                const float a = (f == 5u && y < kFaceSize / 2u) ? 0.0f : 1.0f;

                const size_t idx = static_cast<size_t>(y) * kFaceSize + x;
                const uint16_t hr = glm::packHalf1x16(r);
                const uint16_t hg = glm::packHalf1x16(g);
                const uint16_t hb = glm::packHalf1x16(b);
                const uint16_t ha = glm::packHalf1x16(a);

                dstHalf[idx * 4u + 0u] = hr;
                dstHalf[idx * 4u + 1u] = hg;
                dstHalf[idx * 4u + 2u] = hb;
                dstHalf[idx * 4u + 3u] = ha;

                // Use the exact fp16-quantized values for CPU reference so comparison tests math, not quantization
                cpuFaces[f][idx] = glm::vec4(
                    glm::unpackHalf1x16(hr),
                    glm::unpackHalf1x16(hg),
                    glm::unpackHalf1x16(hb),
                    glm::unpackHalf1x16(ha));
            }
        }
        cpuPtrs[f] = cpuFaces[f].data();
    }

    float cpuBackfaceFrac = 0.0f;
    gi::SHL2 cpuSH = gi::ProjectCubemapCPU(cpuPtrs, kFaceSize, &cpuBackfaceFrac);
    gi::ConvolveIrradianceOverPi(cpuSH);
    gi::ApplyHanningWindow(cpuSH, 6.0f);

    TextureCreateInfo cubeInfo;
    cubeInfo.width = kFaceSize;
    cubeInfo.height = kFaceSize;
    cubeInfo.depth = 1;
    cubeInfo.mipLevels = 1;
    cubeInfo.arraySize = 6;
    cubeInfo.sampleCount = 1;
    cubeInfo.dimension = nri::TextureType::TEXTURE_2D;
    cubeInfo.format = nri::Format::RGBA16_SFLOAT;
    cubeInfo.isCubeMap = true;
    cubeInfo.isRenderTarget = false;
    cubeInfo.isUAV = false;
    cubeInfo.bindless = false;
    Ref<Texture> cubeTex = Texture::Create(cubeInfo, "[GISHTest SyntheticCube]");
    ASSERT_TRUE(cubeTex->UploadCubeMipChain(fp16Subresources, 0));

    IBLPrefilter prefilter;
    gi::SHL2 gpuSH{};
    float gpuBackfaceFrac = -1.0f;
    ASSERT_TRUE(prefilter.ProjectCubeToSHSync(cubeTex, gpuSH, &gpuBackfaceFrac, true, 6.0f));

    EXPECT_NEAR(gpuBackfaceFrac, cpuBackfaceFrac, 1e-3f);
    for (int i = 0; i < 9; ++i)
    {
        EXPECT_NEAR(gpuSH.c[i].r, cpuSH.c[i].r, 1e-3f) << "coeff " << i;
        EXPECT_NEAR(gpuSH.c[i].g, cpuSH.c[i].g, 1e-3f) << "coeff " << i;
        EXPECT_NEAR(gpuSH.c[i].b, cpuSH.c[i].b, 1e-3f) << "coeff " << i;
    }
}

