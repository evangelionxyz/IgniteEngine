// HBAO-style screen-space ambient occlusion.
// Kept at the old ssao.compute.hlsl path so existing engine bindings continue to work.

Texture2D t_Depth     : register(t0);
Texture2D t_Noise     : register(t1);
#ifdef TARGET_VULKAN
    [[vk::image_format("r8")]]
#endif
RWTexture2D<unorm float> u_Target : register(u0);
SamplerState s_Clamp  : register(s0);
SamplerState s_Repeat : register(s1);

cbuffer SSAOParams : register(b0)
{
    float4x4 u_Projection;
    float4x4 u_ProjectionInv;
    float4 u_Samples[32]; // unused by HBAO, kept for C++ buffer compatibility
    float4 u_Params;      // x=radius, y=bias, z=power, w=unused
    float4 u_NoiseScale;  // x=scaleX, y=scaleY
};

float3 ReconstructViewPos(float2 uv, float depth)
{
    float4 clip = float4(uv.x * 2.0f - 1.0f, -(uv.y * 2.0f - 1.0f), depth, 1.0f);
    float4 view = mul(u_ProjectionInv, clip);
    return view.xyz / max(view.w, 1e-6f);
}

float3 ReconstructNormal(float2 uv, float centerDepth, float3 centerVS)
{
    uint dw, dh;
    t_Depth.GetDimensions(dw, dh);
    float2 texel = 1.0f / float2(dw, dh);

    float dL = t_Depth.SampleLevel(s_Clamp, uv - float2(texel.x, 0.0f), 0.0f).r;
    float dR = t_Depth.SampleLevel(s_Clamp, uv + float2(texel.x, 0.0f), 0.0f).r;
    float dU = t_Depth.SampleLevel(s_Clamp, uv - float2(0.0f, texel.y), 0.0f).r;
    float dD = t_Depth.SampleLevel(s_Clamp, uv + float2(0.0f, texel.y), 0.0f).r;

    float3 pL = ReconstructViewPos(uv - float2(texel.x, 0.0f), dL);
    float3 pR = ReconstructViewPos(uv + float2(texel.x, 0.0f), dR);
    float3 pU = ReconstructViewPos(uv - float2(0.0f, texel.y), dU);
    float3 pD = ReconstructViewPos(uv + float2(0.0f, texel.y), dD);

    float3 dx = (abs(dL - centerDepth) < abs(dR - centerDepth)) ? (centerVS - pL) : (pR - centerVS);
    float3 dy = (abs(dU - centerDepth) < abs(dD - centerDepth)) ? (pU - centerVS) : (centerVS - pD);
    return normalize(cross(dx, dy));
}

float HashAngle(float2 uv)
{
    float3 noise = t_Noise.SampleLevel(s_Repeat, uv * u_NoiseScale.xy, 0.0f).xyz;
    return atan2(noise.y, noise.x);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    u_Target.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h)
        return;

    float2 texel = 1.0f / float2(w, h);
    float2 uv = (float2(DTid.xy) + 0.5f) * texel;

    float depth = t_Depth.SampleLevel(s_Clamp, uv, 0.0f).r;
    if (depth >= 0.99999f || depth <= 0.00001f)
    {
        u_Target[DTid.xy] = 1.0f;
        return;
    }

    float3 posVS = ReconstructViewPos(uv, depth);
    float3 normalVS = ReconstructNormal(uv, depth, posVS);

    // If normal reconstruction flips because of projection handedness, keep it facing the camera.
    if (dot(normalVS, -normalize(posVS)) < 0.0f)
        normalVS = -normalVS;

    float radius = max(u_Params.x, 0.01f);
    float bias = u_Params.y;
    float power = max(u_Params.z, 0.01f);

    // Convert the world-space radius to a screen-space search radius.
    // Clamp to avoid huge far-plane walks and tiny near-plane flicker.
    float viewZ = max(abs(posVS.z), 0.1f);
    float radiusPixels = clamp(radius * abs(u_Projection[1][1]) * float(h) * 0.5f / viewZ, 2.0f, 48.0f);

    static const int kDirections = 8;
    static const int kSteps = 6;

    float randomAngle = HashAngle(uv);
    float occlusion = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for (int dirIndex = 0; dirIndex < kDirections; ++dirIndex)
    {
        float angle = randomAngle + (6.28318530718f * (float(dirIndex) + 0.5f) / float(kDirections));
        float2 dir = float2(cos(angle), sin(angle));

        float horizon = 0.0f;

        [unroll]
        for (int stepIndex = 1; stepIndex <= kSteps; ++stepIndex)
        {
            float stepScale = (float(stepIndex) + 0.35f) / float(kSteps);
            float2 suv = uv + dir * texel * radiusPixels * stepScale;

            if (any(suv < 0.0f) || any(suv > 1.0f))
                continue;

            float sampleDepth = t_Depth.SampleLevel(s_Clamp, suv, 0.0f).r;
            if (sampleDepth >= 0.99999f || sampleDepth <= 0.00001f)
                continue;

            float3 sampleVS = ReconstructViewPos(suv, sampleDepth);
            float3 delta = sampleVS - posVS;
            float dist = length(delta);
            if (dist <= 1e-4f || dist > radius)
                continue;

            float3 horizonDir = delta / dist;
            float projected = dot(normalVS, horizonDir) - bias;
            float falloff = saturate(1.0f - (dist * dist) / (radius * radius));
            horizon = max(horizon, saturate(projected) * falloff);
        }

        occlusion += horizon;
        weightSum += 1.0f;
    }

    float ao = 1.0f - occlusion / max(weightSum, 1.0f);
    ao = pow(saturate(ao), power);
    u_Target[DTid.xy] = ao;
}
