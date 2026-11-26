// Copyright (c) 2026 Evangelion Manuhutu

cbuffer GTAOPrefilterParams : register(b0)
{
    float4x4 projectionInv;
    uint2 srcSize;
    uint2 dstSize;
    float nearPlane;
    float farPlane;
    uint passMode; // 0 = full-res device depth -> half-res linear viewZ mip 0; 1 = mip m-1 -> mip m
    float effectRadius;
};

Texture2D<float> t_SrcDepth : register(t0);

#ifdef TARGET_VULKAN
    [[vk::image_format("r32f")]]
#endif
RWTexture2D<float> u_DstDepth : register(u0);

float DeviceDepthToViewZ(float d)
{
    float maxZ = min(farPlane, 65000.0f);
    if (d >= 0.99999f)
    {
        return maxZ;
    }
    float4 view = mul(projectionInv, float4(0.0f, 0.0f, d, 1.0f));
    float viewZ = abs(view.z / max(abs(view.w), 1e-6f));
    return clamp(viewZ, max(nearPlane, 1e-3f), maxZ);
}

float WeightedAverage4(float z0, float z1, float z2, float z3)
{
    float minZ = min(min(z0, z1), min(z2, z3));
    float maxZ = max(max(z0, z1), max(z2, z3));
    float maxValidZ = min(farPlane, 65000.0f);
    if (minZ >= maxValidZ * 0.998f)
    {
        return maxValidZ;
    }

    // Weight relative to the quad median so sloped surfaces are not pulled toward minZ.
    float medZ = (z0 + z1 + z2 + z3 - minZ - maxZ) * 0.5f;
    float invRange = 1.0f / max(effectRadius * 0.5f, 0.05f);
    float w0 = saturate(1.0f - abs(z0 - medZ) * invRange);
    float w1 = saturate(1.0f - abs(z1 - medZ) * invRange);
    float w2 = saturate(1.0f - abs(z2 - medZ) * invRange);
    float w3 = saturate(1.0f - abs(z3 - medZ) * invRange);

    w0 = max(w0, (abs(z0 - medZ) <= 1e-4f) ? 1.0f : 0.0f);
    w1 = max(w1, (abs(z1 - medZ) <= 1e-4f) ? 1.0f : 0.0f);
    w2 = max(w2, (abs(z2 - medZ) <= 1e-4f) ? 1.0f : 0.0f);
    w3 = max(w3, (abs(z3 - medZ) <= 1e-4f) ? 1.0f : 0.0f);

    float wSum = max(w0 + w1 + w2 + w3, 1e-5f);
    return clamp((z0 * w0 + z1 * w1 + z2 * w2 + z3 * w3) / wSum, 0.0f, maxValidZ);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= dstSize.x || DTid.y >= dstSize.y)
    {
        return;
    }

    int2 maxSrc = int2(max(int(srcSize.x) - 1, 0), max(int(srcSize.y) - 1, 0));
    int2 p00, p10, p01, p11;
    if (passMode == 0u)
    {
        float2 scale = float2(srcSize) / max(float2(dstSize), float2(1.0f, 1.0f));
        int2 basePx = int2(floor(float2(DTid.xy) * scale));
        int2 stepPx = max(int2(round(scale * 0.5f)), int2(1, 1));
        p00 = clamp(basePx, int2(0, 0), maxSrc);
        p10 = clamp(basePx + int2(stepPx.x, 0), int2(0, 0), maxSrc);
        p01 = clamp(basePx + int2(0, stepPx.y), int2(0, 0), maxSrc);
        p11 = clamp(basePx + stepPx, int2(0, 0), maxSrc);
    }
    else
    {
        int2 basePx = int2(DTid.xy) * 2;
        p00 = clamp(basePx + int2(0, 0), int2(0, 0), maxSrc);
        p10 = clamp(basePx + int2(1, 0), int2(0, 0), maxSrc);
        p01 = clamp(basePx + int2(0, 1), int2(0, 0), maxSrc);
        p11 = clamp(basePx + int2(1, 1), int2(0, 0), maxSrc);
    }

    float z00 = t_SrcDepth.Load(int3(p00, 0)).r;
    float z10 = t_SrcDepth.Load(int3(p10, 0)).r;
    float z01 = t_SrcDepth.Load(int3(p01, 0)).r;
    float z11 = t_SrcDepth.Load(int3(p11, 0)).r;

    if (passMode == 0u)
    {
        z00 = DeviceDepthToViewZ(z00);
        z10 = DeviceDepthToViewZ(z10);
        z01 = DeviceDepthToViewZ(z01);
        z11 = DeviceDepthToViewZ(z11);
    }

    u_DstDepth[DTid.xy] = WeightedAverage4(z00, z10, z01, z11);
}
