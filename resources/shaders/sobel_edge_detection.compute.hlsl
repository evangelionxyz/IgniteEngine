#include "include/helpers.hlsli"

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
Texture2D<uint> objectIDTexture : register(t1); // object ID buffer
Texture2D<float> depthTexture : register(t2); // depth buffer

// Selected object IDs
StructuredBuffer<uint> selectedIDs : register(t3);

// Output texture (for compute shader)
RWTexture2D<float4> outputTexture : register(u0); // output texture

// Sampler
SamplerState linearSampler : register(s0);

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

    // Check if this pixel should have outline
    if (!IsSelectedObject(pixel, float2(texSize)))
    {
        outputTexture[pixel] = float4(0.0f, 0.0, 0.0, 0.0f);
        return;
    }

    outputTexture[pixel] = outlineColor;
}
