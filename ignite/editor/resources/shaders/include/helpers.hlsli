float4 Uncharted2Tonemap(float4 color)
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

float3 Uncharted2Tonemap(float3 color, float exposure, float gamma)
{
    float4 mappedColor = Uncharted2Tonemap(float4(color, 1.0f) * exposure);

    // normalize by the tonemapped white point
    float4 whiteScale = 1.0f / Uncharted2Tonemap(float4(11.2f, 11.2f, 11.2f, 11.2f));
    mappedColor *= whiteScale;

    // Gamma correction
    float3 gammaCorrected = pow(mappedColor.rgb, 1.0f / gamma);
    return gammaCorrected.rgb;
}

float3 Reinhard2Tonemap(float3 color)
{
  const float L_white = 4.0;
  return (color * (1.0 + color / (L_white * L_white))) / (1.0 + color);
}

float3 Reinhard2Tonemap(float3 color, float exposure, float gamma) {
    float3 mappedColor = Reinhard2Tonemap(color * exposure);
    
    float3 whiteScale = 1.0f / Reinhard2Tonemap(float3(11.2f, 11.2f, 11.2f));
    mappedColor *= mappedColor;
    
    // Gamma correction
    float3 gammaCorrected = pow(mappedColor.rgb, 1.0f / gamma);
    return gammaCorrected.rgb;
}

float3 FilmicTonemap(float3 color)
{
    float3 X = max(float3(0.0f, 0.0f, 0.0f), color - 0.004);
    float3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
    return pow(result, float3(2.2f, 2.2f, 2.2f));
}

float3 FilmicTonemap(float3 color, float exposure, float gamma)
{
    float3 mappedColor = FilmicTonemap(color * exposure);
    
    float3 whiteScale = 1.0f / FilmicTonemap(float3(11.2f, 11.2f, 11.2f));
    mappedColor *= mappedColor;
    
    // Gamma correction
    float3 gammaCorrected = pow(mappedColor.rgb, 1.0f / gamma);
    return gammaCorrected.rgb;
}

float3 SRGBToLinear(float3 srgb)
{
    float3 lt = step(float3(0.04045, 0.04045, 0.04045), srgb);
    return lerp(srgb / 12.92, pow((srgb + 0.055) / 1.055, 2.4), lt);
}

float3 SampleSphericalMap(Texture2D tex, SamplerState samp, float3 dir)
{
    dir = normalize(dir);
    float2 uv;
    uv.x = atan2(dir.z, dir.x) / (2.0f * 3.14159265f) + 0.5f;
    uv.y = asin(clamp(dir.y, -1.0f, 1.0f)) / 3.14159265f + 0.5f;
    return tex.Sample(samp, uv).rgb;
}

// Convert RGB to Luminance 
float Luminance(float3 color)
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
float2 SobelColor(float2 uv, float2 texelSize, Texture2D <float4>tex, SamplerState s)
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

float2 SobelDepth(float2 uv, float2 texelSize, Texture2D <float>depthTex, SamplerState s)
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
