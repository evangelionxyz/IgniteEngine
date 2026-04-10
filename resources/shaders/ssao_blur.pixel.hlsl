struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D t_Src : register(t0);
SamplerState s_Clamp : register(s0);

cbuffer BlurParams : register(b0)
{
    float u_Horizontal; // placeholder
    float3 _pad;
};

float4 main(VSOutput input) : SV_Target
{
    uint w, h;
    t_Src.GetDimensions(w, h);
    float2 texel = 1.0f / float2(w, h);
    
    float sum = 0.0f;
    float wsum = 0.0f;

    [unroll]
    for (int x = -2; x <= 2; ++x)
    {
        [unroll]
        for (int y = -2; y <= 2; ++y)
        {
            float weight = 1.0f - (abs((float)x) + abs((float)y)) / 8.0f;
            float2 suv = input.uv + float2(x, y) * texel;
            sum += t_Src.SampleLevel(s_Clamp, suv, 0.0f).r * weight;
            wsum += weight;
        }
    }

    float ao = sum / wsum;
    return float4(ao, ao, ao, 1.0f);
}
