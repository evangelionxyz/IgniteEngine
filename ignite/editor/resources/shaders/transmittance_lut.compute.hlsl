// Copyright (c) 2026 Evangelion Manuhutu

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2D<float4> transmittanceLUT : register(u0);

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
    float4 SunDirectionAndIntensity; // xyz = normalized sun dir, w = intensity
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

void GetTransmittanceParamsFromUV(float2 uv, out float r, out float cosZenith)
{
    float H = sqrt(max(0.0, AtmosphereRadius * AtmosphereRadius - PlanetRadius * PlanetRadius));
    float rho = uv.y * H;
    r = sqrt(rho * rho + PlanetRadius * PlanetRadius);

    float dMin = AtmosphereRadius - r;
    float dMax = rho + H;
    float d = dMin + uv.x * (dMax - dMin);

    if (d == 0.0)
    {
        cosZenith = 1.0;
    }
    else
    {
        cosZenith = (AtmosphereRadius * AtmosphereRadius - r * r - d * d) / (2.0 * r * d);
    }
    cosZenith = clamp(cosZenith, -1.0, 1.0);
}

float3 ComputeOpticalDepth(float3 pos, float3 dir)
{
    float2 planetHit = RayIntersectSphere(pos, dir, PlanetRadius);
    if (planetHit.x > 0.0)
    {
        return float3(1e5, 1e5, 1e5);
    }

    float2 hit = RayIntersectSphere(pos, dir, AtmosphereRadius);
    float tMax = hit.y;
    if (tMax <= 0.0) return float3(0, 0, 0);

    const int steps = 64;
    float dt = tMax / steps;
    float3 opticalDepth = float3(0, 0, 0);

    for (int i = 0; i < steps; i++)
    {
        float3 p = pos + dir * (dt * (i + 0.5));
        float h = max(0.0, length(p) - PlanetRadius);

        float rayleighDensity = exp(-h / max(0.001, RayleighDensityH));
        float mieDensity      = exp(-h / max(0.001, MieDensityH));
        float ozoneDensity    = max(0.0, 1.0 - abs(h - 25.0) / 15.0);

        opticalDepth += (RayleighScattering * rayleighDensity
                        + MieExtinction * mieDensity
                        + OzoneAbsorption * ozoneDensity) * dt;
    }
    return opticalDepth;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    transmittanceLUT.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;

    float2 uv = (float2(id.xy) + 0.5) / float2(w, h);

    float r, cosZenith;
    GetTransmittanceParamsFromUV(uv, r, cosZenith);

    float3 pos = float3(0.0, r, 0.0);
    float sinZenith = sqrt(max(0.0, 1.0 - cosZenith * cosZenith));
    float3 dir = float3(sinZenith, cosZenith, 0.0);

    float3 opticalDepth = ComputeOpticalDepth(pos, dir);
    transmittanceLUT[id.xy] = float4(exp(-opticalDepth), 1.0);
}