// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SH_HLSLI
#define IGN_SH_HLSLI

// L2 spherical harmonics basis in Y-up world space (identical ordering to sh.hpp):
//   0: Y00
//   1: Y1-1 (y)    2: Y10 (z)     3: Y11 (x)
//   4: Y2-2 (xy)   5: Y2-1 (yz)   6: Y20 (3z^2 - 1)   7: Y21 (xz)   8: Y22 (x^2 - y^2)
static void SHBasisL2(float3 n, out float b[9])
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

// Coefficients are pre-convolved and pre-divided by PI -> returns outgoing diffuse radiance per unit albedo.
static float3 EvalSHIrradiance(float3 c[9], float3 n)
{
    float b[9];
    SHBasisL2(normalize(n), b);
    float3 e = float3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        e += c[i] * b[i];
    }
    return max(e, float3(0.0f, 0.0f, 0.0f));
}

static float3 EvalSHIrradiance4(float4 c[9], float3 n)
{
    float3 c3[9];
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        c3[i] = c[i].xyz;
    }
    return EvalSHIrradiance(c3, n);
}

static void UnpackSHL2(
    float4 t0, float4 t1, float4 t2, float4 t3, float4 t4, float4 t5, float4 t6,
    out float3 c[9], out float validity)
{
    c[0] = t0.xyz;
    c[1] = float3(t0.w, t1.xy);
    c[2] = float3(t1.zw, t2.x);
    c[3] = t2.yzw;
    c[4] = t3.xyz;
    c[5] = float3(t3.w, t4.xy);
    c[6] = float3(t4.zw, t5.x);
    c[7] = t5.yzw;
    c[8] = t6.xyz;
    validity = t6.w;
}

static float2 OctSignNotZero(float2 v)
{
    return float2((v.x >= 0.0f) ? 1.0f : -1.0f, (v.y >= 0.0f) ? 1.0f : -1.0f);
}

static float2 OctEncode(float3 v)
{
    float l1 = abs(v.x) + abs(v.y) + abs(v.z);
    float2 p = v.xy * (1.0f / max(l1, 1e-6f));
    if (v.z < 0.0f)
    {
        p = (1.0f - abs(p.yx)) * OctSignNotZero(p);
    }
    return p * 0.5f + 0.5f;
}

static float3 OctDecode(float2 uv01)
{
    float2 f = uv01 * 2.0f - 1.0f;
    float3 n = float3(f.x, f.y, 1.0f - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += float2((n.x >= 0.0f) ? -t : t, (n.y >= 0.0f) ? -t : t);
    return normalize(n);
}

#endif

