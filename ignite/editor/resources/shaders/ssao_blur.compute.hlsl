Texture2D t_Src : register(t0);
#ifdef TARGET_VULKAN
    [[vk::image_format("r8")]]
#endif
RWTexture2D<unorm float> u_Target : register(u0);
SamplerState s_Clamp : register(s0);

cbuffer BlurParams : register(b0)
{
    float u_Horizontal; // placeholder
    float3 _pad;
};

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    u_Target.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h) return;

    float2 texel = 1.0f / float2(w, h);
    float2 uv = (float2(DTid.xy) + 0.5f) * texel;
    
    float sum = 0.0f;
    float wsum = 0.0f;

    [unroll]
    for (int x = -2; x <= 2; ++x)
    {
        [unroll]
        for (int y = -2; y <= 2; ++y)
        {
            float weight = 1.0f - (abs((float)x) + abs((float)y)) / 8.0f;
            float2 suv = uv + float2(x, y) * texel;
            sum += t_Src.SampleLevel(s_Clamp, suv, 0.0f).r * weight;
            wsum += weight;
        }
    }

    u_Target[DTid.xy] = sum / wsum;
}
