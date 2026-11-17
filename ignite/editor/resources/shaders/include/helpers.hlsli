// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_HELPERS_HLSLI
#define IGN_HELPERS_HLSLI

static float4 Uncharted2Tonemap(float4 color)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;

    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B)+ D * F))-E/F;
}

static float3 Uncharted2Tonemap(float3 color, float exposure, float gamma)
{
    float4 mappedColor = Uncharted2Tonemap(float4(color, 1.0f) * exposure);

    // normalize by the tonemapped white point
    float4 whiteScale = 1.0f / Uncharted2Tonemap(float4(11.2f, 11.2f, 11.2f, 11.2f));
    mappedColor *= whiteScale;

    // Gamma correction
    float3 gammaCorrected = pow(mappedColor.rgb, 1.0f / gamma);
    return gammaCorrected.rgb;
}

static float3 Reinhard2Tonemap(float3 color)
{
  const float L_white = 4.0;
  return (color * (1.0 + color / (L_white * L_white))) / (1.0 + color);
}

static float3 Reinhard2Tonemap(float3 color, float exposure, float gamma) {
    float3 mappedColor = Reinhard2Tonemap(color * exposure);
    
    float3 whiteScale = 1.0f / Reinhard2Tonemap(float3(11.2f, 11.2f, 11.2f));
    mappedColor *= whiteScale;
    
    // Gamma correction
    float3 gammaCorrected = pow(max(mappedColor.rgb, 0.0f), 1.0f / max(gamma, 0.001f));
    return gammaCorrected.rgb;
}

static float3 FilmicTonemap(float3 color)
{
    // Narkowicz ACES Filmic approximation in linear space
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

static float3 FilmicTonemap(float3 color, float exposure, float gamma)
{
    float3 mappedColor = FilmicTonemap(max(color * exposure, 0.0f));
    
    float3 whiteScale = 1.0f / max(FilmicTonemap(float3(11.2f, 11.2f, 11.2f)), 1e-4f);
    mappedColor = saturate(mappedColor * whiteScale);
    
    // Gamma correction
    float3 gammaCorrected = pow(max(mappedColor.rgb, 0.0f), 1.0f / max(gamma, 0.001f));
    return gammaCorrected.rgb;
}

static float3 ApplyTonemap(float3 color, int tonemapMode, float exposure, float gamma)
{
    if (tonemapMode == 1)
    {
        return Uncharted2Tonemap(color, exposure, gamma);
    }

    if (tonemapMode == 2)
    {
        return FilmicTonemap(color, exposure, gamma);
    }

    return Reinhard2Tonemap(color, exposure, gamma);
}

static float3 SRGBToLinear(float3 srgb)
{
    float3 lt = step(float3(0.04045, 0.04045, 0.04045), srgb);
    return lerp(srgb / 12.92, pow((srgb + 0.055) / 1.055, 2.4), lt);
}

static float3 SampleSphericalMap(Texture2D tex, SamplerState samp, float3 dir)
{
    dir = normalize(dir);
    float2 uv;
    uv.x = atan2(dir.z, dir.x) * 0.15915494f + 0.5f;
    uv.y = asin(clamp(dir.y, -1.0f, 1.0f)) * 0.31830988f + 0.5f;
    return tex.SampleLevel(samp, uv, 0.0f).rgb;
}

static float3 SampleEnvironmentMap(Texture2D tex, SamplerState samp, float3 dir, int skyType, float cameraAltitudeKm)
{
    if (skyType == 1)
    {
        dir = normalize(dir);
        float azimuth = atan2(dir.z, dir.x);
        float u = (azimuth < 0.0f ? azimuth + 6.283185307f : azimuth) / 6.283185307f;

        float r = 6360.0f + max(0.001f, cameraAltitudeKm);
        float horizonAngle = asin(clamp(6360.0f / r, 0.0f, 1.0f));
        float zenith = acos(clamp(dir.y, -1.0f, 1.0f));

        float v = 0.5f;
        if (zenith <= horizonAngle)
        {
            // Looking UP: zenith in [0, horizonAngle] -> v in [0.5, 1.0] (Blue Sky Zenith at v=1.0)
            float coord = sqrt(clamp((horizonAngle - zenith) / max(1e-4f, horizonAngle), 0.0f, 1.0f));
            v = 0.5f * (1.0f + coord);
        }
        else
        {
            // Looking DOWN: zenith in [horizonAngle, pi] -> v in [0.0, 0.5] (Ground at v=0.0)
            float coord = sqrt(clamp((zenith - horizonAngle) / max(1e-4f, 3.14159265359f - horizonAngle), 0.0f, 1.0f));
            v = 0.5f * (1.0f - coord);
        }
        return tex.SampleLevel(samp, float2(u, clamp(v, 0.0f, 1.0f)), 0.0f).rgb;
    }
    return SampleSphericalMap(tex, samp, dir);
}

static void BuildOrthonormalBasis(float3 n, out float3 t, out float3 b)
{
    float s = (n.z >= 0.0f) ? 1.0f : -1.0f;
    float a = -1.0f / (s + n.z);
    float d = n.x * n.y * a;
    t = float3(1.0f + s * n.x * n.x * a, s * d, -s * n.x);
    b = float3(d, s + n.y * n.y * a, -n.y);
}

// Fast 8-tap precomputed Fibonacci sphere SH cosine lobe projection for diffuse irradiance.
// Avoids 32 runtime trig pairs per call while producing smooth hemisphere irradiance.
static const float3 k_IrradianceDirs[8] =
{
    float3( 0.000000f,  0.875000f,  0.484123f),
    float3(-0.562322f,  0.625000f, -0.541449f),
    float3( 0.145039f,  0.375000f,  0.915602f),
    float3( 0.677611f,  0.125000f, -0.724722f),
    float3(-0.968246f, -0.125000f,  0.216506f),
    float3( 0.779658f, -0.375000f,  0.501431f),
    float3(-0.226681f, -0.625000f, -0.747010f),
    float3(-0.341893f, -0.875000f,  0.342764f)
};

static float3 SampleEnvironmentIrradiance(Texture2D tex, SamplerState samp, float3 normal, int skyType, float cameraAltitudeKm)
{
    static const int SAMPLE_COUNT = 8;
    static const float MAX_RADIANCE = 16.0f;

    float3 sum = float3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        float3 dir = k_IrradianceDirs[i];
        float c = dot(normal, dir);
        float kernel = max(0.375f + 2.0f * c + 1.875f * c * c, 0.0f);
        float3 radiance = min(SampleEnvironmentMap(tex, samp, dir, skyType, cameraAltitudeKm), MAX_RADIANCE);
        sum += radiance * kernel;
    }
    return max(sum / float(SAMPLE_COUNT), 0.0f);
}

static float3 SampleRoughEnvironmentSpecular(Texture2D tex, SamplerState samp, float3 reflectDir, float roughness, int skyType, float cameraAltitudeKm)
{
    static const float MAX_RADIANCE = 24.0f;
    float3 T, B;
    BuildOrthonormalBasis(reflectDir, T, B);
    float spread = clamp(roughness * roughness * 0.65f, 0.02f, 0.65f);

    float3 d0 = normalize(reflectDir + spread * ( T + B));
    float3 d1 = normalize(reflectDir + spread * (-T + B));
    float3 d2 = normalize(reflectDir + spread * ( T - B));
    float3 d3 = normalize(reflectDir + spread * (-T - B));

    float3 sum = min(SampleEnvironmentMap(tex, samp, d0, skyType, cameraAltitudeKm), MAX_RADIANCE)
               + min(SampleEnvironmentMap(tex, samp, d1, skyType, cameraAltitudeKm), MAX_RADIANCE)
               + min(SampleEnvironmentMap(tex, samp, d2, skyType, cameraAltitudeKm), MAX_RADIANCE)
               + min(SampleEnvironmentMap(tex, samp, d3, skyType, cameraAltitudeKm), MAX_RADIANCE);
    return sum * 0.25f;
}

// Convert RGB to Luminance 
static float Luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

static const float sobelX[9] =
{
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1
};

static const float sobelY[9] =
{
    -1, -2, -1,
    0, 0, 0,
    1, 2, 1
};

static const float2 offsets[9] =
{
    float2(-1, -1), float2(0, -1), float2(1, -1),
    float2(-1, 0), float2(0, 0), float2(1, 0),
    float2(-1, 1), float2(0, 1), float2(1, 1)
};


// Sobel edge detection on color/luminance
static float2 SobelColor(float2 uv, float2 texelSize, Texture2D <float4>tex, SamplerState s)
{
    float sobelXResult = 0.0f;
    float sobelYResult = 0.0f;

    [unroll]
    for (int i = 0; i < 9; i++)
    {
        float2 sampleUV = uv + offsets[i] * texelSize;
        float3 color = tex.SampleLevel(s, sampleUV, 0).rgb;
        float luminance = Luminance(color);

        sobelXResult += luminance * sobelX[i];
        sobelYResult += luminance * sobelY[i];
    }

    return float2(sobelXResult, sobelYResult);
}

static float2 SobelDepth(float2 uv, float2 texelSize, Texture2D <float>depthTex, SamplerState s)
{
    float sobelXResult = 0;
    float sobelYResult = 0;
    
    [unroll]
    for (int i = 0; i < 9; i++)
    {
        float2 sampleUV = uv + offsets[i] * texelSize;
        float depth = depthTex.SampleLevel(s, sampleUV, 0).r;
        
        sobelXResult += depth * sobelX[i];
        sobelYResult += depth * sobelY[i];
    }

    return float2(sobelXResult, sobelYResult);
}

#endif
