struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

Texture2D t_Depth     : register(t0);
Texture2D t_Noise     : register(t1);
SamplerState s_Clamp  : register(s0);
SamplerState s_Repeat : register(s1);

cbuffer SSAOParams : register(b0)
{
    float4x4 u_Projection;
    float4x4 u_ProjectionInv;
    float4 u_Samples[32];
    float4 u_Params; // x=radius, y=bias, z=power, w=__pad
    float4 u_NoiseScale; // x=scaleX, y=scaleY
};

float3 ReconstructViewPos(float2 uv, float depth)
{
    // NVRHI convention: depth corresponds to Z in clip space.
    // Vulkan & DX12 both usually use 0 to 1 for depth in NVRHI projection setup.
    // But since this was ported from OpenGL where Z is -1 to +1, we might need to adjust.
    // Assuming Ignition uses standard Direct3D clip space (NDC Z = 0 to 1).
    float z = depth;
    float4 clip = float4(uv.x * 2.0f - 1.0f, -(uv.y * 2.0f - 1.0f), z, 1.0f);
    float4 view = mul(u_ProjectionInv, clip);
    view /= view.w;
    return view.xyz;
}

float4 main(VSOutput input) : SV_Target
{
    float depth = t_Depth.SampleLevel(s_Clamp, input.uv, 0.0f).r;
    if (depth >= 1.0f || depth <= 0.00001f) // adjust max depth condition as needed for your pipeline
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    float3 posVS = ReconstructViewPos(input.uv, depth);

    uint w, h;
    t_Depth.GetDimensions(w, h);
    float2 texel = 1.0f / float2(w, h);
    
    float2 uvR = input.uv + float2(texel.x, 0.0f);
    float dR = t_Depth.SampleLevel(s_Clamp, uvR, 0.0f).r;
    float2 uvU = input.uv + float2(0.0f, texel.y);
    float dU = t_Depth.SampleLevel(s_Clamp, uvU, 0.0f).r;

    float3 pR = ReconstructViewPos(uvR, dR);
    float3 pU = ReconstructViewPos(uvU, dU);
    float3 dx = pR - posVS;
    float3 dy = pU - posVS;
    
    // Cross product order depends on coordinate system.
    float3 normal = normalize(cross(dx, dy));

    // Random rotation
    float2 noiseUV = input.uv * u_NoiseScale.xy;
    float3 rand = t_Noise.SampleLevel(s_Repeat, noiseUV, 0).xyz;

    float3 tangent = normalize(rand - normal * dot(rand, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal); // HLSL float3x3 uses row vectors

    float radius = u_Params.x;
    float bias = u_Params.y;
    float power = u_Params.z;

    float occ = 0.0f;
    for (int i = 0; i < 32; i++)
    {
        // Multiply by TBN
        float3 sampleVS = mul(u_Samples[i].xyz, TBN);
        sampleVS = posVS + sampleVS * radius;

        float4 offset = float4(sampleVS, 1.0f);
        offset = mul(u_Projection, offset);
        offset.xyz /= offset.w;

        float2 suv = float2(offset.x * 0.5f + 0.5f, -offset.y * 0.5f + 0.5f);

        if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f)
            continue;

        float sDepth = t_Depth.SampleLevel(s_Clamp, suv, 0).r;
        float3 sampleDepthVS = ReconstructViewPos(suv, sDepth);

        float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(posVS.z - sampleDepthVS.z));
        float centerDist = -posVS.z;
        float sampleDist = -sampleDepthVS.z;
        
        occ += ((sampleDist < (-sampleVS.z - bias)) ? 1.0f : 0.0f) * rangeCheck;
    }

    occ = 1.0f - (occ / 32.0f);
    occ = pow(max(occ, 0.0f), power);

    return float4(occ, occ, occ, 1.0f);
}
