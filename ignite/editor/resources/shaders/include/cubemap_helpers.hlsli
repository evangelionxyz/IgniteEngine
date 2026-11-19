// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CUBEMAP_HELPERS_HLSLI
#define IGN_CUBEMAP_HELPERS_HLSLI

static float3 CubemapUVToDir(uint face, float2 uv)
{
    // Vulkan/D3D12 cubemap orientation (uv in [0,1]^2, v=0 top -> st in [-1,1]^2)
    float2 st = uv * 2.0f - 1.0f;
    float3 dir = float3(0.0f, 0.0f, 0.0f);
    if (face == 0u)      dir = float3( 1.0f, -st.y, -st.x); // +X
    else if (face == 1u) dir = float3(-1.0f, -st.y,  st.x); // -X
    else if (face == 2u) dir = float3( st.x,  1.0f,  st.y); // +Y
    else if (face == 3u) dir = float3( st.x, -1.0f, -st.y); // -Y
    else if (face == 4u) dir = float3( st.x, -st.y,  1.0f); // +Z
    else                 dir = float3(-st.x, -st.y, -1.0f); // -Z
    return normalize(dir);
}

// Solid angle dOmega subtended by texel at cubemap plane coordinates st in [-1, 1]^2 for size N
static float CubemapTexelSolidAngle(float2 uv, uint size)
{
    float2 st = uv * 2.0f - 1.0f;
    float texelSpan = 2.0f / float(max(size, 1u));
    float d2 = 1.0f + st.x * st.x + st.y * st.y;
    return (texelSpan * texelSpan) / (d2 * sqrt(d2));
}

#endif

