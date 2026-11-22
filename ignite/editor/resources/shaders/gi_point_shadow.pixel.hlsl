// Copyright (c) 2026 Evangelion Manuhutu

#include "include/scene.hlsli"

cbuffer CameraBuffer : register(b1, space0) { Camera camera; }

float main(PixelVertexInput input) : SV_Target0
{
    // camera.position.xyz = light world position, camera.position.w = light range
    float dist = length(input.worldPos - camera.position.xyz);
    float range = max(camera.position.w, 1e-4f);
    return saturate(dist / range);
}
