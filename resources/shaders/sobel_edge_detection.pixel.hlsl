// Constant buffer for parameters
cbuffer EdgeDetectionParams : register(b0)
{
    float2 texelSize; // 1.0 / textureSize
    float edgeThreshold; // Edge detection sensitivity
    float outlineWidth; // Outline thickness multiplier
    float4 outlineColor; // Outline color (RGB + intensity)
    float depthSensitivity; // Depth edge sensitivity
    int useObjectID; // Use object ID buffer for selection
    float padding[1]; // Pad to 16-byte alignment
};

// Input textures
Texture2D<float4> sceneTexture : register(t0); // main scene color
Texture2D<float> depthTexture : register(t1); // depth buffer
Texture2D<uint> objectIDTexture : register(t2); // object ID buffer

// Output texture (for compute shader)
RWTexture2D<float4> outputTexture : register(u0); // output texture

// Sampler
SamplerState linearSampler : register(s0);

static const float sobelX[9] = {
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1
};

static const float sobelY[9] = {
    -1, -2, -1,
    0, 0, 0,
    1, 2, 1
};

static const float2 offsets[9] = {
    float2(-1,-1), float2(0,-1), float2(1,-1),
    float2(-1, 0), float2(0, 0), float2(1, 0),
    float2(-1, 1), float2(0, 1), float2(1, 1)
};

// Convert RGB to Luminance for edge detection
float Luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

// Sobel edge detection on color/luminance
float2 SobelColor(float2 uv)
{
    float sobelXResult = 0.0f;
    float sobelYResult = 0.0f;

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 sampleUV = uv + offsets[i] * texelSize;
        float3 color = sceneTexture.SampleLevel(linearSampler, sampleUV, 0).rgb; // Use red channel for luminance
        float luminance = Luminance(color);

        sobelXResult += luminance * sobelX[i];
        sobelYResult += luminance * sobelY[i];
    }

    return float2(sobelXResult, sobelYResult);
}


float2 SobelDepth(float2 uv)
{
    float sobelXResult = 0;
    float sobelYResult = 0;
    
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        float2 sampleUV = uv + offsets[i] * texelSize;
        float depth = depthTexture.SampleLevel(linearSampler, sampleUV, 0).r;
        
        sobelXResult += depth * sobelX[i];
        sobelYResult += depth * sobelY[i];
    }

    return float2(sobelXResult, sobelYResult) * depthSensitivity;
}


// Check if pixel is part of selected object
bool IsSelectedObject(float2 uv, float2 texSize)
{
    int2 pixelCoord = int2(uv * texSize);
    uint centerID = objectIDTexture.Load(int3(pixelCoord, 0.0));
    
    if (centerID == -1)
        return false;

     // Check if any neighboring pixel has different object ID
    [unroll]
    for (int i = 0; i < 9; i++)
    {
        if (i == 4)
            continue; // Skip center pixel
        
        int2 neighborCoord = pixelCoord + int2(offsets[i]);
        
        // Bounds check
        if (neighborCoord.x < 0 || neighborCoord.y < 0 ||
            neighborCoord.x >= texSize.x || neighborCoord.y >= texSize.y)
            continue;
            
        uint neighborID = objectIDTexture.Load(int3(neighborCoord, 0));
        
        // Edge detected if neighbor has different ID or is background
        if (neighborID != centerID)
            return true;
    }

    return false;
}

// Pixel Shader Version (for full-screen quad)
struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_Target
{
    uint2 texSize;
    outputTexture.GetDimensions(texSize.x, texSize.y);
    
    float2 uv = input.uv;
    
    // Sample original color
    float4 originalColor = sceneTexture.SampleLevel(linearSampler, uv, 0);

    // Check if this pixel should have outline
    bool shouldOutline = IsSelectedObject(uv, float2(texSize));

    if (!shouldOutline)
        return originalColor; // No outline, just return original color

    // perform edge detection
    float2 colorEdge = SobelColor(uv);
    float2 depthEdge = SobelDepth(uv);
    
    // Combine edge results
    float2 totalEdge = colorEdge + depthEdge;
    float edgeIntensity = length(totalEdge);

    // Apply threshold and create outline
    float outline = step(edgeThreshold, edgeIntensity);
    outline *= outlineWidth; // Scale by outline width
    outline = saturate(outline); // Clamp to [0, 1]

    return lerp(originalColor, outlineColor, outline * outlineColor.a);
}

