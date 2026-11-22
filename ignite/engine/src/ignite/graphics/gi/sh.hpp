// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_GI_SH_HPP
#define IGN_GI_SH_HPP

#include <glm/glm.hpp>
#include <array>
#include <cmath>
#include <cstdint>

namespace ignite::gi
{
    inline constexpr float kPI = 3.14159265359f;

    // L2 spherical harmonics coefficients in RGB (9 basis functions, Y-up world space).
    // Ordering matches sh.hlsli:
    //   0: Y00
    //   1: Y1-1 (y)    2: Y10 (z)     3: Y11 (x)
    //   4: Y2-2 (xy)   5: Y2-1 (yz)   6: Y20 (3z^2 - 1)   7: Y21 (xz)   8: Y22 (x^2 - y^2)
    struct SHL2
    {
        glm::vec3 c[9] = {};
    };

    struct DebugProbeGPU
    {
        glm::vec4 posAndRadius = glm::vec4(0.0f, 0.0f, 0.0f, 0.35f);
        glm::vec4 sh[9] = {};
        glm::vec4 stateAndFlags = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); // x: 0=invalid, 1=valid, 2=relocated
    };

    inline void SHBasisL2(const glm::vec3 &n, float b[9])
    {
        b[0] = 0.28209479f;
        b[1] = 0.48860251f * n.y;
        b[2] = 0.48860251f * n.z;
        b[3] = 0.48860251f * n.x;
        b[4] = 1.09254843f * n.x * n.y;
        b[5] = 1.09254843f * n.y * n.z;
        b[6] = 0.31539157f * (3.0f * n.z * n.z - 1.0f);
        b[7] = 1.09254843f * n.x * n.z;
        b[8] = 0.54627422f * (n.x * n.x - n.y * n.y);
    }

    inline glm::vec3 CubemapUVToDir(uint32_t face, const glm::vec2 &uv)
    {
        const glm::vec2 st = uv * 2.0f - 1.0f;
        glm::vec3 dir(0.0f);
        switch (face)
        {
            case 0u: dir = glm::vec3( 1.0f, -st.y, -st.x); break; // +X
            case 1u: dir = glm::vec3(-1.0f, -st.y,  st.x); break; // -X
            case 2u: dir = glm::vec3( st.x,  1.0f,  st.y); break; // +Y
            case 3u: dir = glm::vec3( st.x, -1.0f, -st.y); break; // -Y
            case 4u: dir = glm::vec3( st.x, -st.y,  1.0f); break; // +Z
            default: dir = glm::vec3(-st.x, -st.y, -1.0f); break; // -Z
        }
        return glm::normalize(dir);
    }

    inline float CubemapTexelSolidAngle(const glm::vec2 &uv, uint32_t size)
    {
        const glm::vec2 st = uv * 2.0f - 1.0f;
        const float texelSpan = 2.0f / static_cast<float>(size > 0u ? size : 1u);
        const float d2 = 1.0f + st.x * st.x + st.y * st.y;
        return (texelSpan * texelSpan) / (d2 * std::sqrt(d2));
    }

    // Cosine lobe convolution factors divided by pi: {1, 2/3, 1/4} for bands l = 0, 1, 2.
    inline void ConvolveIrradianceOverPi(SHL2 &sh)
    {
        constexpr float a0 = 1.0f;
        constexpr float a1 = 2.0f / 3.0f;
        constexpr float a2 = 1.0f / 4.0f;
        sh.c[0] *= a0;
        sh.c[1] *= a1;
        sh.c[2] *= a1;
        sh.c[3] *= a1;
        for (int i = 4; i < 9; ++i)
        {
            sh.c[i] *= a2;
        }
    }

    // Applies Hanning de-ringing filter on bands 1 and 2 when windowWidth > 0.
    inline void ApplyHanningWindow(SHL2 &sh, float windowWidth)
    {
        if (windowWidth <= 0.0f)
        {
            return;
        }
        const float w1 = 0.5f * (1.0f + std::cos(kPI * 1.0f / windowWidth));
        const float w2 = 0.5f * (1.0f + std::cos(kPI * 2.0f / windowWidth));
        sh.c[1] *= w1;
        sh.c[2] *= w1;
        sh.c[3] *= w1;
        for (int i = 4; i < 9; ++i)
        {
            sh.c[i] *= w2;
        }
    }

    // Evaluates pre-convolved (and /pi pre-divided) irradiance along unit normal n.
    // Returns outgoing diffuse radiance per unit albedo, clamped >= 0.
    inline glm::vec3 EvalSHIrradiance(const SHL2 &sh, const glm::vec3 &n)
    {
        float b[9];
        SHBasisL2(glm::normalize(n), b);
        glm::vec3 sum(0.0f);
        for (int i = 0; i < 9; ++i)
        {
            sum += sh.c[i] * b[i];
        }
        return glm::max(sum, glm::vec3(0.0f));
    }

    // Integrates 6 HDR cubemap faces (row-major faceSize x faceSize vec4 texels each) into raw L2 SH.
    // Texels with alpha < 0.5 are counted toward backfaceFraction.
    inline SHL2 ProjectCubemapCPU(const glm::vec4 * const faces[6], uint32_t faceSize,
        float *outBackfaceFraction = nullptr, float *outTotalSolidAngle = nullptr)
    {
        SHL2 result{};
        if (faceSize == 0u)
        {
            return result;
        }

        double accum[9][3] = {};
        double totalOmega = 0.0;
        double backfaceOmega = 0.0;

        for (uint32_t face = 0; face < 6u; ++face)
        {
            const glm::vec4 *facePixels = faces[face];
            if (!facePixels)
            {
                continue;
            }
            for (uint32_t y = 0; y < faceSize; ++y)
            {
                for (uint32_t x = 0; x < faceSize; ++x)
                {
                    const glm::vec2 uv(
                        (static_cast<float>(x) + 0.5f) / static_cast<float>(faceSize),
                        (static_cast<float>(y) + 0.5f) / static_cast<float>(faceSize));
                    const glm::vec3 dir = CubemapUVToDir(face, uv);
                    const double dOmega = static_cast<double>(CubemapTexelSolidAngle(uv, faceSize));
                    const glm::vec4 texel = facePixels[static_cast<size_t>(y) * faceSize + x];

                    float b[9];
                    SHBasisL2(dir, b);

                    const glm::vec3 rad = glm::max(glm::vec3(texel), glm::vec3(0.0f));
                    for (int i = 0; i < 9; ++i)
                    {
                        const double w = dOmega * static_cast<double>(b[i]);
                        accum[i][0] += static_cast<double>(rad.r) * w;
                        accum[i][1] += static_cast<double>(rad.g) * w;
                        accum[i][2] += static_cast<double>(rad.b) * w;
                    }

                    totalOmega += dOmega;
                    if (texel.a < 0.5f)
                    {
                        backfaceOmega += dOmega;
                    }
                }
            }
        }

        const double normScale = (totalOmega > 1e-8) ? (4.0 * static_cast<double>(kPI) / totalOmega) : 0.0;
        for (int i = 0; i < 9; ++i)
        {
            result.c[i].r = static_cast<float>(accum[i][0] * normScale);
            result.c[i].g = static_cast<float>(accum[i][1] * normScale);
            result.c[i].b = static_cast<float>(accum[i][2] * normScale);
        }

        if (outBackfaceFraction)
        {
            *outBackfaceFraction = (totalOmega > 1e-8) ? static_cast<float>(backfaceOmega / totalOmega) : 0.0f;
        }
        if (outTotalSolidAngle)
        {
            *outTotalSolidAngle = static_cast<float>(totalOmega);
        }
        return result;
    }

    inline void PackSHL2To7RGBA(const SHL2 &sh, float validity, glm::vec4 out[7])
    {
        float f[28] = {};
        for (int i = 0; i < 9; ++i)
        {
            f[i * 3 + 0] = sh.c[i].r;
            f[i * 3 + 1] = sh.c[i].g;
            f[i * 3 + 2] = sh.c[i].b;
        }
        f[27] = validity;
        for (int t = 0; t < 7; ++t)
        {
            out[t] = glm::vec4(f[t * 4 + 0], f[t * 4 + 1], f[t * 4 + 2], f[t * 4 + 3]);
        }
    }

    inline void UnpackSHL2From7RGBA(const glm::vec4 in[7], SHL2 &out, float &outValidity)
    {
        out.c[0] = glm::vec3(in[0]);
        out.c[1] = glm::vec3(in[0].w, in[1].x, in[1].y);
        out.c[2] = glm::vec3(in[1].z, in[1].w, in[2].x);
        out.c[3] = glm::vec3(in[2].y, in[2].z, in[2].w);
        out.c[4] = glm::vec3(in[3]);
        out.c[5] = glm::vec3(in[3].w, in[4].x, in[4].y);
        out.c[6] = glm::vec3(in[4].z, in[4].w, in[5].x);
        out.c[7] = glm::vec3(in[5].y, in[5].z, in[5].w);
        out.c[8] = glm::vec3(in[6]);
        outValidity = in[6].w;
    }

    struct alignas(16) ProbeVisibilityStatsGPU
    {
        glm::vec4 closestBackfaceDirAndDist = glm::vec4(0.0f, 1.0f, 0.0f, -1.0f);
        glm::vec4 closestFrontfaceDirAndDist = glm::vec4(0.0f, 1.0f, 0.0f, -1.0f);
        float backfaceFraction = 0.0f;
        uint32_t hitCount = 0;
        uint32_t backfaceCount = 0;
        uint32_t _pad = 0;
    };
    static_assert(sizeof(ProbeVisibilityStatsGPU) == 48, "ProbeVisibilityStatsGPU must match HLSL layout");

    inline glm::vec2 OctSignNotZero(const glm::vec2 &v)
    {
        return glm::vec2(v.x >= 0.0f ? 1.0f : -1.0f, v.y >= 0.0f ? 1.0f : -1.0f);
    }

    inline glm::vec2 OctEncode(const glm::vec3 &v)
    {
        const float l1 = std::abs(v.x) + std::abs(v.y) + std::abs(v.z);
        glm::vec2 p(v.x / std::max(l1, 1e-6f), v.y / std::max(l1, 1e-6f));
        if (v.z < 0.0f)
        {
            p = (glm::vec2(1.0f) - glm::abs(glm::vec2(p.y, p.x))) * OctSignNotZero(p);
        }
        return p * 0.5f + 0.5f;
    }

    inline glm::vec3 OctDecode(const glm::vec2 &uv01)
    {
        const glm::vec2 f = uv01 * 2.0f - 1.0f;
        glm::vec3 n(f.x, f.y, 1.0f - std::abs(f.x) - std::abs(f.y));
        const float t = std::clamp(-n.z, 0.0f, 1.0f);
        n.x += (n.x >= 0.0f) ? -t : t;
        n.y += (n.y >= 0.0f) ? -t : t;
        return glm::normalize(n);
    }
}

#endif

