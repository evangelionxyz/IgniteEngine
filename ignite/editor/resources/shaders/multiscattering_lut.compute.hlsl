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
    float sinHorizonSq = (PlanetRadius * PlanetRadius) / (r * r);
    if (cosZenith < 0.0 && (1.0 - cosZenith * cosZenith) < sinHorizonSq)
    {
        return float3(0.0, 0.0, 0.0);
    }
    float2 uv = GetTransmittanceUV(r, cosZenith);
    return transmittanceLUT.SampleLevel(linearSampler, uv, 0).rgb;
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

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint w, h;
    multiScatteringLUT.GetDimensions(w, h);
    if (id.x >= w || id.y >= h) return;

    float uvX = (float(id.x) + 0.5) / float(w);
    float uvY = (float(id.y) + 0.5) / float(h);

    float cosSunZenith = uvX * 2.0 - 1.0;
    float r = uvY * (AtmosphereRadius - PlanetRadius) + PlanetRadius;

    float3 pos = float3(0.0, r, 0.0);
    float sinSun = sqrt(max(0.0, 1.0 - cosSunZenith * cosSunZenith));
    float3 sunDir = normalize(float3(sinSun, cosSunZenith, 0.0));

    const int sqrtSamples = 8;
    const int numSamples = sqrtSamples * sqrtSamples;

    float3 L2ndOrder = float3(0.0, 0.0, 0.0);
    float3 Fms       = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < sqrtSamples; i++)
    {
        for (int j = 0; j < sqrtSamples; j++)
        {
            float theta = (float(i) + 0.5) / float(sqrtSamples) * 3.14159265359;
            float phi   = (float(j) + 0.5) / float(sqrtSamples) * 2.0 * 3.14159265359;

            float cosTheta = cos(theta);
            float sinTheta = sin(theta);
            float3 dir = float3(sinTheta * cos(phi), cosTheta, sinTheta * sin(phi));

            float2 planetHit = RayIntersectSphere(pos, dir, PlanetRadius);
            float2 atmoHit   = RayIntersectSphere(pos, dir, AtmosphereRadius);

            bool hitsGround = planetHit.x > 0.0;
            float tMax = hitsGround ? planetHit.x : atmoHit.y;

            if (tMax > 0.0)
            {
                const int steps = 16;
                float dt = tMax / float(steps);
                float3 rayL = float3(0.0, 0.0, 0.0);
                float3 rayF = float3(0.0, 0.0, 0.0);
                float3 rayTrans = float3(1.0, 1.0, 1.0);

                for (int s = 0; s < steps; s++)
                {
                    float3 p = pos + dir * (dt * (float(s) + 0.5));
                    float pLen = length(p);
                    float pHeight = max(0.0, pLen - PlanetRadius);

                    float rayleighDensity = exp(-pHeight / max(0.001, RayleighDensityH));
                    float mieDensity      = exp(-pHeight / max(0.001, MieDensityH));
                    float ozoneDensity    = max(0.0, 1.0 - abs(pHeight - 25.0) / 15.0);

                    float3 sRayleigh  = RayleighScattering * rayleighDensity;
                    float3 sMie       = MieScattering * mieDensity;
                    float3 scattering = sRayleigh + sMie;
                    float3 extinction = sRayleigh + MieExtinction * mieDensity + OzoneAbsorption * ozoneDensity;

                    float3 sampleTrans = exp(-extinction * dt);

                    float cosSunAtP = dot(normalize(p), sunDir);
                    float3 sunTrans = SampleTransmittance(pLen, cosSunAtP);

                    float cosViewSun = dot(dir, sunDir);
                    float rP = RayleighPhase(cosViewSun);
                    float mP = CornetteShanksPhase(cosViewSun, MieG);

                    float3 stepScat = (sRayleigh * rP + sMie * mP) * sunTrans;
                    float3 stepInScat = (stepScat - stepScat * sampleTrans) / max(float3(1e-5, 1e-5, 1e-5), extinction);
                    float3 stepF = (scattering - scattering * sampleTrans) / max(float3(1e-5, 1e-5, 1e-5), extinction);

                    rayL += rayTrans * stepInScat;
                    rayF += rayTrans * stepF;
                    rayTrans *= sampleTrans;
                }

                if (hitsGround)
                {
                    float3 groundPos = pos + dir * planetHit.x;
                    float3 groundNorm = normalize(groundPos);
                    float NdotS = max(0.0, dot(groundNorm, sunDir));
                    float3 groundSunTrans = SampleTransmittance(PlanetRadius, NdotS);
                    float3 groundRadiance = (GroundAlbedo / 3.14159265359) * groundSunTrans * NdotS;
                    rayL += rayTrans * groundRadiance;
                }

                L2ndOrder += rayL * sinTheta;
                Fms       += rayF * sinTheta;
            }
        }
    }

    // Average over the unit sphere (factor 2 * pi^2 / (4 * pi * numSamples) = pi / (2 * numSamples))
    float sphereNorm = 3.14159265359 / (2.0 * float(numSamples));
    L2ndOrder *= sphereNorm;
    Fms       *= sphereNorm;

    // Geometric series sum of multiple scattering orders: Psi = L2ndOrder / (1.0 - Fms)
    float3 msFactor = 1.0 / max(float3(1e-3, 1e-3, 1e-3), float3(1.0, 1.0, 1.0) - Fms);
    float3 multiScatFactor = max(float3(0.0, 0.0, 0.0), L2ndOrder * msFactor);
    multiScatteringLUT[id.xy] = float4(multiScatFactor, 1.0);
}
