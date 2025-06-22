// Constant buffer for parameters
cbuffer EdgeDetectionParams : register(b0)
{
    float2 texelSize; // 1.0 / textureSize
    float edgeThreshold; // Sobel threshold
    float outlineWidth; // Thickness multiplier

    float4 outlineColor; // RGBA

    float depthSensitivity; // Depth edge weight
    int useObjectID; // 0 = ignore ID buffer
    uint selectedCount; // how many ids in list

    uint _padding; // pad to 16‑byte boundary
};

// Input textures
Texture2D<float4> sceneTexture : register(t0); // main scene color
Texture2D<float> depthTexture : register(t1); // depth buffer
Texture2D<uint> objectIDTexture : register(t2); // object ID buffer

// Selected object IDs
StructuredBuffer<uint> selectedIDs : register(t3);

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
    float2(-1, -1), float2(0, -1), float2(1, -1),
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
    for (int i = 0; i < 9; i++)
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
    for (int i = 0; i < 9; i++)
    {
        float2 sampleUV = uv + offsets[i] * texelSize;
        float depth = depthTexture.SampleLevel(linearSampler, sampleUV, 0).r;
        
        sobelXResult += depth * sobelX[i];
        sobelYResult += depth * sobelY[i];
    }

    return float2(sobelXResult, sobelYResult) * depthSensitivity;
}

bool IsIDSelected(uint id)
{
    if (id == 0xFFFFFFFFu)
        return false;
    
    [loop]
    for (uint i = 0; i < selectedCount; i++)
    {
        if (selectedIDs[i] == id)
            return true;
    }
    
    return false;
}

// Check if pixel is part of selected object
bool IsSelectedObject(int2 pixelCoord, float2 texSize)
{    
    uint centerID = objectIDTexture.Load(int3(pixelCoord, 0.0));
    
    // background -> no outline
    if (!IsIDSelected(centerID))
        return false;

    // look for a neighbour with a different ID (or background)
    [unroll]
    for (int i = 0; i < 9; i++)
    {
        int2 n = pixelCoord + int2(offsets[i]);
        if (n.x < 0 || n.y < 0 || n.x >= (int)texSize.x || n.y >= (int)texSize.y)
            continue;
            
        uint nid = objectIDTexture.Load(int3(n, 0));
        
        // Edge detected if neighbor has different ID or is background
        if (nid != centerID)
            return true;
    }

    return false;
}

// Compute Shader
[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DISPATCHTHREADID)
{
    uint2 texSize;
    outputTexture.GetDimensions(texSize.x, texSize.y);

    if (dispatchId.x >= texSize.x || dispatchId.y >= texSize.y)
        return; // Out of bounds
    
    int2 pixel = int2(dispatchId.xy);
    float2 uv = (float2(pixel) + 0.5f) * texelSize; // Center pixel UV

    // Sample original color
    float4 baseColor = sceneTexture.SampleLevel(linearSampler, uv, 0);

    // Check if this pixel should have outline
    if (!IsSelectedObject(pixel, float2(texSize)))
    {
        outputTexture[pixel] = baseColor;
        return;
    }

    // Sobel on color & depth
    float2 colorEdge = SobelColor(uv);
    float2 depthEdge = SobelDepth(uv) * depthSensitivity;
    float edgeIntensity = length(colorEdge + depthEdge);
    float outline = saturate(step(edgeThreshold, edgeIntensity) * outlineWidth);

    outputTexture[pixel] = lerp(baseColor, outlineColor, outline * outlineColor.a);
}
