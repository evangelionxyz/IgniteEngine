// Copyright (c) 2026 Evangelion Manuhutu

cbuffer DownsampleCubeParams : register(b0)
{
    uint dstSize;
    uint srcSize;
    uint _pad0;
    uint _pad1;
};

Texture2DArray<float4> srcMip : register(t0);

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2DArray<float4> dstMip : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint face = dispatchThreadID.z;
    if (dispatchThreadID.x >= dstSize || dispatchThreadID.y >= dstSize || face >= 6u)
    {
        return;
    }

    uint2 baseCoord = dispatchThreadID.xy * 2u;
    uint maxCoord = max(srcSize, 1u) - 1u;

    uint2 c00 = min(baseCoord + uint2(0u, 0u), uint2(maxCoord, maxCoord));
    uint2 c10 = min(baseCoord + uint2(1u, 0u), uint2(maxCoord, maxCoord));
    uint2 c01 = min(baseCoord + uint2(0u, 1u), uint2(maxCoord, maxCoord));
    uint2 c11 = min(baseCoord + uint2(1u, 1u), uint2(maxCoord, maxCoord));

    float4 sum = srcMip.Load(int4(c00.x, c00.y, face, 0))
               + srcMip.Load(int4(c10.x, c10.y, face, 0))
               + srcMip.Load(int4(c01.x, c01.y, face, 0))
               + srcMip.Load(int4(c11.x, c11.y, face, 0));

    dstMip[uint3(dispatchThreadID.xy, face)] = sum * 0.25f;
}

