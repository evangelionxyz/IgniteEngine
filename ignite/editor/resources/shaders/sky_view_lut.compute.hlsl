// Copyright (c) 2026 Evangelion Manuhutu

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2D<float4> skyViewLUT           : register(u0);
Texture2D<float4>   transmittanceLUT     : register(t0);
Texture2D<float4>   multiScatteringLUT   : register(t1);
SamplerState        linearSampler        : register(s0);

cbuffer AtmosphereParamsBuffer : register(b0)
{
    float3 RayleighScattering;       // (5.802, 13.558, 33.1) * 1e-3 (per km)
    float  RayleighDensityH;         // 8.0 km
    float3 MieScattering;            // (3.996, 3.996, 3.996) * 1e-3 (per km)
    float  MieDensityH;              // 1.2 km
    float3 MieExtinction;            // MieScattering * 1.11
    float  MieG;                     // 0.8
    float3 OzoneAbsorption;          // (0.650, 1.881, 0.085) * 1e-3 (per km)
    float  PlanetRadius;             // 6360.0 km
    float3 GroundAlbedo;             // (0.1, 0.1, 0.1)
    float  AtmosphereRadius;         // 6460.0 km
    float4 SunDirectionAndIntensity; // xyz = sun dir, w = intensity
    float4 SunColorAndRadius;        // rgb = sun color, a = angular radius
    float4 CameraPositionAndAltitude;// xyz = camera pos (m), w = altitude (km)
};

float2 RayIntersectSphere(float3 ro, float3 rd, float radius)
{
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0) return float2(-1.0, -1.0);
    float s = sqrt(disc);
    return float2(-b - s, -b + s);
}

float2 GetTransmittanceUV(float r, float cosZenith)
{
    float H = sqrt(max(0.0, AtmosphereRadius * AtmosphereRadius - PlanetRadius * PlanetRadius));
    float rho = sqrt(max(0.0, r * r - PlanetRadius * PlanetRadius));
    float v = rho / H;

    float dMin = AtmosphereRadius - r;
    float dMax = rho + H;
    float d = sqrt(max(0.0, r * r * cosZenith * cosZenith - r * r + AtmosphereRadius * AtmosphereRadius)) - r * cosZenith;
    float u = (d - dMin) / max(0.0001, dMax - dMin);

    return float2(clamp(u, 0.0, 1.0), clamp(v, 0.0, 1.0));
}

float3 SampleTransmittance(float r, float cosZenith)
{
    float2 uv = GetTransmittanceUV(r, cosZenith);
    return transmittanceLUT.SampleLevel(linearSampler, uv, 0).rgb;
}

float3 SampleMultiScattering(float r, float cosSunZenith)
{
    float u = clamp(cosSunZenith * 0.5 + 0.5, 0.0, 1.0);
    float v = clamp((r - PlanetRadius) / max(0.001, AtmosphereRadius - PlanetRadius), 0.0, 1.0);
    return multiScatteringLUT.SampleLevel(linearSampler, float2(u, v), 0).rgb;
}

float RayleighPhase(float cosTheta)
{
    return (3.0 / (16.0 * 3.14159265359)) * (1.0 + cosTheta * cosTheta);
}

float CornetteShanksPhase(float cosTheta, float g)
{
    float g2 = g * g;
    float num = 3.0 * (1.0 - g2) * (1.0 + cosTheta * cosTheta);
    float denom = (8.0 * 3.14159265359) * (2.0 + g2) * pow(max(1e-4, 1.0 + g2 - 2.0 * g * cosTheta), 1.5);
    return num / denom;
}

void UVToSkyViewZenithAzimuth(float2 uv, float r, out float zenith, out float azimuth)
{
    azimuth = uv.x * 2.0 * 3.14159265359;

    float horizonAngle = asin(clamp(PlanetRadius / r, 0.0, 1.0));
    if (uv.y >= 0.5)
    {
        // Top half of texture (v in [0.5, 1.0]) -> Sky Zenith (zenith in [0, horizonAngle])
        float coord = 2.0 * uv.y - 1.0;
        coord = coord * coord;
        zenith = horizonAngle - coord * horizonAngle;
    }
    else
    {
        // Bottom half of texture (v in [0.0, 0.5]) -> Ground (zenith in [horizonAngle, pi])
        float coord = 1.0 - 2.0 * uv.y;
        coord = coord * coord;
        zenith = horizonAngle + coord * (3.14159265359 - horizonAngle);
    }
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    skyViewLUT.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;

    float2 uv = (float2(id.xy) + 0.5) / float2(w, h);

    float cameraAltitudeKm = max(0.001, CameraPositionAndAltitude.w);
    float r = PlanetRadius + cameraAltitudeKm;

    float zenith, azimuth;
    UVToSkyViewZenithAzimuth(uv, r, zenith, azimuth);

    float sinZenith = sin(zenith);
    float cosZenith = cos(zenith);
    float3 viewDir = normalize(float3(sinZenith * cos(azimuth), cosZenith, sinZenith * sin(azimuth)));

    float3 cameraPos = float3(0.0, r, 0.0);

    float3 rawSunDir = SunDirectionAndIntensity.xyz;
    float sunIntensity = SunDirectionAndIntensity.w;
    float3 sunDir = length(rawSunDir) > 0.001 ? normalize(rawSunDir) : float3(0, 1, 0);

    float3 sunColor = SunColorAndRadius.rgb * sunIntensity;

    float3 rayStart = cameraPos;
    float2 atmoHit = RayIntersectSphere(rayStart, viewDir, AtmosphereRadius);
    
    // Check if camera is outside atmosphere (space view)
    if (r > AtmosphereRadius)
    {
        if (atmoHit.x < 0.0)
        {
            skyViewLUT[id.xy] = float4(0.0, 0.0, 0.0, 1.0);
            return;
        }
        rayStart = cameraPos + viewDir * atmoHit.x;
        atmoHit = RayIntersectSphere(rayStart, viewDir, AtmosphereRadius);
    }

    float tMax = atmoHit.y;
    float2 planetHit = RayIntersectSphere(rayStart, viewDir, PlanetRadius);
    bool hitsGround = planetHit.x > 0.0;
    if (hitsGround)
    {
        tMax = planetHit.x;
    }

    if (tMax <= 0.0)
    {
        skyViewLUT[id.xy] = float4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    const int steps = 32;
    float dt = tMax / steps;

    float cosTheta = dot(viewDir, sunDir);
    float rPhase = RayleighPhase(cosTheta);
    float mPhase = CornetteShanksPhase(cosTheta, MieG);

    float3 inScattering = float3(0.0, 0.0, 0.0);
    float3 transmittance = float3(1.0, 1.0, 1.0);

    for (int i = 0; i < steps; i++)
    {
        float3 p = rayStart + viewDir * (dt * (i + 0.5));
        float pLen = length(p);
        float pHeight = max(0.0, pLen - PlanetRadius);

        float rayleighDensity = exp(-pHeight / max(0.001, RayleighDensityH));
        float mieDensity      = exp(-pHeight / max(0.001, MieDensityH));
        float ozoneDensity    = max(0.0, 1.0 - abs(pHeight - 25.0) / 15.0);

        float3 sRayleigh = RayleighScattering * rayleighDensity;
        float3 sMie      = MieScattering * mieDensity;
        float3 extinction = sRayleigh + MieExtinction * mieDensity + OzoneAbsorption * ozoneDensity;

        float3 sampleTrans = exp(-extinction * dt);

        float cosSunZenithAtP = dot(normalize(p), sunDir);
        float3 pSunTrans = cosSunZenithAtP > 0.0 ? SampleTransmittance(pLen, cosSunZenithAtP) : float3(0, 0, 0);
        float3 multiScat = SampleMultiScattering(pLen, cosSunZenithAtP);

        float3 singleScat = (sRayleigh * rPhase + sMie * mPhase) * pSunTrans * sunColor;
        float3 secondScat = multiScat * sunColor * 0.35f;

        float3 stepScattering = singleScat + secondScat;
        float3 stepInScattering = (stepScattering - stepScattering * sampleTrans) / max(float3(1e-5, 1e-5, 1e-5), extinction);

        inScattering += transmittance * stepInScattering;
        transmittance *= sampleTrans;
    }

    if (hitsGround)
    {
        float3 groundPos = rayStart + viewDir * planetHit.x;
        float groundLen = length(groundPos);
        float3 groundNorm = normalize(groundPos);
        float NdotS = max(0.0, dot(groundNorm, sunDir));
        float3 groundSunTrans = NdotS > 0.0 ? SampleTransmittance(groundLen, NdotS) : float3(0, 0, 0);
        float3 groundRadiance = (GroundAlbedo / 3.14159265359) * groundSunTrans * sunColor * NdotS;

        inScattering += transmittance * groundRadiance;
    }

    skyViewLUT[id.xy] = float4(inScattering, 1.0);
}
