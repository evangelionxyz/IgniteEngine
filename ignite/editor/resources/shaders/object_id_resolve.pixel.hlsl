// Resolves a multisampled R32_UINT object-ID attachment to a single-sample target.
// Hardware resolve is unsupported for integer formats, so pick the first sample that holds an object.
Texture2DMS<uint> objectIdMS : register(t0);

struct PixelInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

uint main(PixelInput input) : SV_TARGET
{
    uint width, height, sampleCount;
    objectIdMS.GetDimensions(width, height, sampleCount);

    int2 pixel = int2(input.position.xy);
    uint result = 0xFFFFFFFFu;
    [loop]
    for (uint s = 0; s < sampleCount; ++s)
    {
        uint id = objectIdMS.Load(pixel, s);
        if (id != 0xFFFFFFFFu)
        {
            result = id;
            break;
        }
    }
    return result;
}
