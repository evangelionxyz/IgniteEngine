// Copyright (c) 2026 Evangelion Manuhutu

#ifdef TARGET_VULKAN
    [[vk::image_format("rgba16f")]]
#endif
RWTexture2D<float4> multiScatteringLUT : register(u0);
Texture2D<float4>   transmittanceLUT   : register(t0);
SamplerState        linearSampler      : register(s0);

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

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    multiScatteringLUT.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;

    float uvX = (id.x + 0.5) / w;
    float uvY = (id.y + 0.5) / h;

    float cosSunZenith = uvX * 2.0 - 1.0;
    float r = uvY * (AtmosphereRadius - PlanetRadius) + PlanetRadius;

    float3 pos = float3(0.0, r, 0.0);
    float sinSun = sqrt(max(0.0, 1.0 - cosSunZenith * cosSunZenith));
    float3 sunDir = normalize(float3(sinSun, cosSunZenith, 0.0));

    const int sqrtSamples = 8;
    const int numSamples = sqrtSamples * sqrtSamples;

    float3 L2ndOrder = float3(0, 0, 0);
    float3 Fms       = float3(0, 0, 0);

    for (int i = 0; i < sqrtSamples; i++)
    {
        for (int j = 0; j < sqrtSamples; j++)
        {
            float theta = (i + 0.5) / float(sqrtSamples) * 3.14159265;
            float phi   = (j + 0.5) / float(sqrtSamples) * 2.0 * 3.14159265;

            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            float3 dir = float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));

            float2 hit = RayIntersectSphere(pos, dir, AtmosphereRadius);
            float tMax = hit.y;

            if (tMax > 0.0)
            {
                const int steps = 16;
                float dt = tMax / steps;
                float3 rayL = float3(0, 0, 0);
                float3 rayF = float3(0, 0, 0);

                for (int s = 0; s < steps; s++)
                {
                    float3 p = pos + dir * (dt * (s + 0.5));
                    float pLen = length(p);
                    float pHeight = max(0.0, pLen - PlanetRadius);

                    float rayleighDensity = exp(-pHeight / max(0.001, RayleighDensityH));
                    float mieDensity      = exp(-pHeight / max(0.001, MieDensityH));
                    float ozoneDensity    = max(0.0, 1.0 - abs(pHeight - 25.0) / 15.0);

                    float3 scattering = RayleighScattering * rayleighDensity + MieScattering * mieDensity;
                    float3 extinction = RayleighScattering * rayleighDensity + MieExtinction * mieDensity + OzoneAbsorption * ozoneDensity;

                    float cosSunZenithAtP = dot(normalize(p), sunDir);
                    float3 transCamToP = SampleTransmittance(pLen, dot(normalize(p), dir));
                    float3 transPToSun = cosSunZenithAtP > 0.0 ? SampleTransmittance(pLen, cosSunZenithAtP) : float3(0, 0, 0);

                    rayL += transCamToP * transPToSun * scattering * dt;
                    rayF += transCamToP * scattering * dt;
                }

                L2ndOrder += rayL * sinTheta;
                Fms       += rayF * sinTheta;
            }
        }
    }

    L2ndOrder *= (2.0 * 3.14159265 * 3.14159265) / float(numSamples);
    Fms       *= (2.0 * 3.14159265 * 3.14159265) / float(numSamples);

    float msEnergy = dot(Fms, float3(0.33333334, 0.33333334, 0.33333334));
    float msGain = 1.0 / max(1.0, 1.0 + msEnergy);
    float3 multiScatFactor = max(float3(0.0, 0.0, 0.0), L2ndOrder * msGain);
    multiScatteringLUT[id.xy] = float4(multiScatFactor, 1.0);
}
