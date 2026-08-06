// JFA Flood Pass Compute Shader

cbuffer JFAFloodParams : register(b0)
{
    int stepSize;
    int3 _padding;
};

Texture2D<int2> inputTexture : register(t0);

#ifdef TARGET_VULKAN
    [[vk::image_format("rg16i")]]
#endif
RWTexture2D<int2> outputTexture : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DISPATCHTHREADID)
{
    uint2 texSize;
    outputTexture.GetDimensions(texSize.x, texSize.y);

    if (dispatchId.x >= texSize.x || dispatchId.y >= texSize.y)
        return;

    int2 pixel = int2(dispatchId.xy);
    int2 bestSeed = inputTexture.Load(int3(pixel, 0));
    
    float bestDistSq = 1e9f;
    if (bestSeed.x != 32767)
    {
        float2 diff = float2(bestSeed - pixel);
        bestDistSq = dot(diff, diff);
    }

    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
                continue;

            int2 neighborCoord = pixel + int2(dx, dy) * stepSize;
            if (neighborCoord.x < 0 || neighborCoord.y < 0 || neighborCoord.x >= (int)texSize.x || neighborCoord.y >= (int)texSize.y)
                continue;

            int2 neighborSeed = inputTexture.Load(int3(neighborCoord, 0));
            if (neighborSeed.x != 32767)
            {
                float2 diff = float2(neighborSeed - pixel);
                float distSq = dot(diff, diff);
                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestSeed = neighborSeed;
                }
            }
        }
    }

    outputTexture[pixel] = bestSeed;
}
