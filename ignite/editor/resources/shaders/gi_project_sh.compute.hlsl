// Copyright (c) 2026 Evangelion Manuhutu

#include "include/cubemap_helpers.hlsli"
#include "include/sh.hlsli"

static const float kPI = 3.14159265359f;

struct ProbeSHOutput
{
    float4 coeff[9];
    float backfaceFraction;
    float totalSolidAngle;
    float2 _pad;
};

cbuffer ProjectSHParams : register(b0)
{
    uint faceSize;
    uint probeIndex;
    uint convolveOverPi;
    float hanningWindow;
};

Texture2DArray<float4> srcCube : register(t0);
RWStructuredBuffer<ProbeSHOutput> outSH : register(u0);

groupshared float gs_Accum[64][29];

[numthreads(8, 8, 1)]
void main(uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    float3 accum[9];
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        accum[i] = float3(0.0f, 0.0f, 0.0f);
    }

    float totalOmega = 0.0f;
    float backfaceOmega = 0.0f;

    uint faceTexels = faceSize * faceSize;
    uint totalTexels = 6u * faceTexels;

    for (uint idx = groupIndex; idx < totalTexels; idx += 64u)
    {
        uint face = idx / faceTexels;
        uint rem = idx - face * faceTexels;
        uint y = rem / faceSize;
        uint x = rem - y * faceSize;

        float2 uv = (float2(x, y) + 0.5f) / float(faceSize);
        float3 dir = CubemapUVToDir(face, uv);
        float dOmega = CubemapTexelSolidAngle(uv, faceSize);
        float4 texel = srcCube.Load(int4(int(x), int(y), int(face), 0));

        float b[9];
        SHBasisL2(dir, b);

        float3 rad = max(texel.rgb, float3(0.0f, 0.0f, 0.0f));
        [unroll]
        for (int k = 0; k < 9; ++k)
        {
            accum[k] += rad * (b[k] * dOmega);
        }

        totalOmega += dOmega;
        if (texel.a < 0.5f)
        {
            backfaceOmega += dOmega;
        }
    }

    [unroll]
    for (int j = 0; j < 9; ++j)
    {
        gs_Accum[groupIndex][j * 3 + 0] = accum[j].r;
        gs_Accum[groupIndex][j * 3 + 1] = accum[j].g;
        gs_Accum[groupIndex][j * 3 + 2] = accum[j].b;
    }
    gs_Accum[groupIndex][27] = totalOmega;
    gs_Accum[groupIndex][28] = backfaceOmega;

    GroupMemoryBarrierWithGroupSync();

    for (uint stride = 32u; stride > 0u; stride >>= 1u)
    {
        if (groupIndex < stride)
        {
            for (uint c = 0u; c < 29u; ++c)
            {
                gs_Accum[groupIndex][c] += gs_Accum[groupIndex + stride][c];
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (groupIndex == 0u)
    {
        float sumOmega = gs_Accum[0][27];
        float sumBackface = gs_Accum[0][28];
        float normScale = (sumOmega > 1e-8f) ? (4.0f * kPI / sumOmega) : 0.0f;

        float3 coeff[9];
        [unroll]
        for (int m = 0; m < 9; ++m)
        {
            coeff[m] = float3(
                gs_Accum[0][m * 3 + 0],
                gs_Accum[0][m * 3 + 1],
                gs_Accum[0][m * 3 + 2]) * normScale;
        }

        if (convolveOverPi != 0u)
        {
            const float a1 = 2.0f / 3.0f;
            const float a2 = 0.25f;
            coeff[1] *= a1;
            coeff[2] *= a1;
            coeff[3] *= a1;
            [unroll]
            for (int n = 4; n < 9; ++n)
            {
                coeff[n] *= a2;
            }
        }

        if (hanningWindow > 0.0f)
        {
            float w1 = 0.5f * (1.0f + cos(kPI * 1.0f / hanningWindow));
            float w2 = 0.5f * (1.0f + cos(kPI * 2.0f / hanningWindow));
            coeff[1] *= w1;
            coeff[2] *= w1;
            coeff[3] *= w1;
            [unroll]
            for (int n = 4; n < 9; ++n)
            {
                coeff[n] *= w2;
            }
        }

        [unroll]
        for (int idxOut = 0; idxOut < 9; ++idxOut)
        {
            outSH[probeIndex].coeff[idxOut] = float4(coeff[idxOut], 0.0f);
        }
        outSH[probeIndex].backfaceFraction = (sumOmega > 1e-8f) ? (sumBackface / sumOmega) : 0.0f;
        outSH[probeIndex].totalSolidAngle = sumOmega;
        outSH[probeIndex]._pad = float2(0.0f, 0.0f);
    }
}

