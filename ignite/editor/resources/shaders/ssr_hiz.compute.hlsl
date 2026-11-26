// Copyright (c) 2026 Evangelion Manuhutu

cbuffer SSRHiZParams : register(b0)
{
    uint2 srcSize;
    uint2 dstSize;
    uint passMode; // 0 = full-res HW depth -> half-res mip 0, 1 = mip m-1 -> mip m
    uint3 _pad;
};

Texture2D<float> t_SrcDepth : register(t0);

#ifdef TARGET_VULKAN
    [[vk::image_format("r32f")]]
#endif
RWTexture2D<float> u_DstDepth : register(u0);

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

    float d00 = t_SrcDepth.Load(int3(p00, 0)).r;
    float d10 = t_SrcDepth.Load(int3(p10, 0)).r;
    float d01 = t_SrcDepth.Load(int3(p01, 0)).r;
    float d11 = t_SrcDepth.Load(int3(p11, 0)).r;

    // Standard Z [0, 1]: minimum depth is the nearest surface.
    u_DstDepth[DTid.xy] = min(min(d00, d10), min(d01, d11));
}
