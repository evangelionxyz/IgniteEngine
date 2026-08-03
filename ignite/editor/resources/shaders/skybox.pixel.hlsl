// Copyright (c) 2026 Evangelion Manuhutu

#include "include/helpers.hlsli"

struct Scene
{
    float4 lightColor; // rgb = color, a = intensity
    float2 lightAngle;
    float sunAngularRadius;
    int renderMode;
    int debugShadow;
    float exposure;
    float gamma;
    float ambient;
    int numPointLights;
    int numSpotLights;
    int skyType;         // 0 = HDRI, 1 = Procedural Sky
    float _pad2;
    float4 sunDirection; // xyz = normalized direction towards sun
};

cbuffer SceneBuffer : register(b1) { Scene scene; }

struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
};

cbuffer CameraBuffer : register(b0, space0) { Camera camera; }

struct PSInput
{
    float4 position : SV_POSITION;
    float3 UVW      : UVW;
};

Texture2D texture0 : register(t0);       // HDRI spherical map or Sky-View LUT
SamplerState sampler0 : register(s0);

struct PSOutput
{
    float4 color : SV_TARGET0;
};

float2 SkyViewZenithAzimuthToUV(float3 dir, float planetRadius, float cameraAltitudeKm)
{
    float azimuth = atan2(dir.z, dir.x);
    float u = (azimuth < 0.0 ? azimuth + 6.28318530718 : azimuth) / 6.28318530718;

    float r = planetRadius + max(0.001, cameraAltitudeKm);
    float horizonAngle = asin(clamp(planetRadius / r, 0.0, 1.0));
    float zenith = acos(clamp(dir.y, -1.0, 1.0));

    float v = 0.5;
    if (zenith <= horizonAngle)
    {
        // Looking UP: zenith in [0, horizonAngle] -> v in [0.5, 1.0] (Sky Zenith at v=1.0)
        float coord = sqrt(clamp((horizonAngle - zenith) / max(1e-4, horizonAngle), 0.0, 1.0));
        v = 0.5 * (1.0 + coord);
    }
    else
    {
        // Looking DOWN: zenith in [horizonAngle, pi] -> v in [0.0, 0.5] (Ground at v=0.0)
        float coord = sqrt(clamp((zenith - horizonAngle) / max(1e-4, 3.14159265359 - horizonAngle), 0.0, 1.0));
        v = 0.5 * (1.0 - coord);
    }

    return float2(u, clamp(v, 0.0, 1.0));
}

PSOutput main(PSInput input)
{
    PSOutput result;
    float3 dir = normalize(input.UVW);

    float3 color = float3(0, 0, 0);

    if (scene.skyType == 1) // Procedural Sky
    {
        // 6360.0km default planet radius, 0.001km (1m) altitude default for skybox view lookup
        float2 uv = SkyViewZenithAzimuthToUV(dir, 6360.0, max(0.001f, camera.position.y * 0.001f));

        color = texture0.Sample(sampler0, uv).rgb;

        // Add physical sun disk with limb darkening
        float3 sunDir = scene.sunDirection.xyz;
        if (length(sunDir) > 0.01)
        {
            sunDir = normalize(sunDir);
            float cosAngle = dot(dir, sunDir);
            float radius = max(0.001, scene.sunAngularRadius);
            float cosRadius = cos(radius);

            if (cosAngle > cosRadius)
            {
                float angularDistance = acos(clamp(cosAngle, -1.0f, 1.0f));
                float diskT = saturate(angularDistance / radius);
                float limbDarkening = sqrt(max(0.0, 1.0 - diskT * diskT));
                float sunMask = 1.0f - smoothstep(radius, radius * 1.35f, angularDistance);
                float3 sunLuminance = scene.lightColor.rgb * max(1.0, scene.lightColor.a) * sunMask * (0.5f + 0.5f * limbDarkening);
                color += sunLuminance;
            }
        }
    }
    else // HDRI Environment map
    {
        color = SampleSphericalMap(texture0, sampler0, dir);
    }

    result.color = float4(color, 1.0f);
    return result;
}
