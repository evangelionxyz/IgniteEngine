// JFA Seed Pass Compute Shader

cbuffer JFASeedParams : register(b0)
{
    uint selectedCount;
    uint3 _padding;
};

Texture2D<uint> objectIDTexture : register(t0);
StructuredBuffer<uint> selectedIDs : register(t1);

#ifdef TARGET_VULKAN
    [[vk::image_format("rg16i")]]
#endif
RWTexture2D<int2> seedTexture : register(u0);

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

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DISPATCHTHREADID)
{
    uint2 texSize;
    seedTexture.GetDimensions(texSize.x, texSize.y);

    if (dispatchId.x >= texSize.x || dispatchId.y >= texSize.y)
        return;

    int2 pixel = int2(dispatchId.xy);
    uint centerID = objectIDTexture.Load(int3(pixel, 0));

    if (IsIDSelected(centerID))
    {
        seedTexture[pixel] = pixel;
    }
    else
    {
        seedTexture[pixel] = int2(32767, 32767);
    }
}
