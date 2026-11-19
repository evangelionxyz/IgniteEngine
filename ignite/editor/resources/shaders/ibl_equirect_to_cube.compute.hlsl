// Copyright (c) 2026 Evangelion Manuhutu

#include "include/helpers.hlsli"
#include "include/cubemap_helpers.hlsli"

cbuffer EquirectToCubeParams : register(b0)
{
    uint faceSize;
    int skyType;
    float cameraAltitudeKm;
    float _pad;
};

Texture2D<float4> srcEnvTexture : register(t0);
SamplerState linearSampler : register(s0);

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2DArray<float4> dstCubeTexture : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint face = dispatchThreadID.z;
    if (dispatchThreadID.x >= faceSize || dispatchThreadID.y >= faceSize || face >= 6u)
    {
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5f) / float(faceSize);
    float3 dir = CubemapUVToDir(face, uv);
    float3 radiance = max(SampleEnvironmentMap(srcEnvTexture, linearSampler, dir, skyType, cameraAltitudeKm), 0.0f);
    dstCubeTexture[uint3(dispatchThreadID.xy, face)] = float4(radiance, 1.0f);
}

