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
    float4 u_Samples[32];
    float4 u_Params; // x=radius, y=bias, z=power, w=__pad
    float4 u_NoiseScale; // x=scaleX, y=scaleY
};

float3 ReconstructViewPos(float2 uv, float depth)
{
    float z = depth;
    float4 clip = float4(uv.x * 2.0f - 1.0f, -(uv.y * 2.0f - 1.0f), z, 1.0f);
    float4 view = mul(u_ProjectionInv, clip);
    view /= view.w;
    return view.xyz;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    u_Target.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h) return;

    float2 texel = 1.0f / float2(w, h);
    float2 uv = (float2(DTid.xy) + 0.5f) * texel;

    float depth = t_Depth.SampleLevel(s_Clamp, uv, 0.0f).r;
    if (depth >= 1.0f || depth <= 0.00001f)
    {
        u_Target[DTid.xy] = 1.0f;
        return;
    }

    float3 posVS = ReconstructViewPos(uv, depth);

    // -----------------------------------------------------------------------
    // Improved normal reconstruction using min-depth-difference neighbors.
    // At depth discontinuities (object edges), the naive right+up approach
    // picks the wrong neighbor and produces incorrect normals, causing
    // dark halos around silhouettes. By comparing left vs right and up vs
    // down, we pick the pair that stays on the same surface.
    // -----------------------------------------------------------------------
    uint dw, dh;
    t_Depth.GetDimensions(dw, dh);
    float2 dtexel = 1.0f / float2(dw, dh);

    float dL = t_Depth.SampleLevel(s_Clamp, uv - float2(dtexel.x, 0.0f), 0.0f).r;
    float dR = t_Depth.SampleLevel(s_Clamp, uv + float2(dtexel.x, 0.0f), 0.0f).r;
    float dU = t_Depth.SampleLevel(s_Clamp, uv - float2(0.0f, dtexel.y), 0.0f).r;
    float dD = t_Depth.SampleLevel(s_Clamp, uv + float2(0.0f, dtexel.y), 0.0f).r;

    float3 pL = ReconstructViewPos(uv - float2(dtexel.x, 0.0f), dL);
    float3 pR = ReconstructViewPos(uv + float2(dtexel.x, 0.0f), dR);
    float3 pU = ReconstructViewPos(uv - float2(0.0f, dtexel.y), dU);
    float3 pD = ReconstructViewPos(uv + float2(0.0f, dtexel.y), dD);

    // Pick the neighbor pair with the smallest depth difference to the center.
    // This avoids crossing depth discontinuities where the normal would be wrong.
    // To keep the coordinate system consistent:
    // - dx should point RIGHT: (posVS - pL) or (pR - posVS)
    // - dy should point UP: (pU - posVS) or (posVS - pD)
    float3 dx = (abs(dL - depth) < abs(dR - depth)) ? (posVS - pL) : (pR - posVS);
    float3 dy = (abs(dU - depth) < abs(dD - depth)) ? (pU - posVS) : (posVS - pD);

    float3 normal = normalize(cross(dx, dy));

    // -----------------------------------------------------------------------
    // Rotate the sample hemisphere using a tiled noise texture.
    // -----------------------------------------------------------------------
    float2 noiseUV = uv * u_NoiseScale.xy;
    float3 rand = t_Noise.SampleLevel(s_Repeat, noiseUV, 0).xyz;

    float3 tangent = normalize(rand - normal * dot(rand, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    float radius = u_Params.x;
    float bias = u_Params.y;
    float power = u_Params.z;

    // -----------------------------------------------------------------------
    // 32-tap hemisphere sampling with occlusion accumulation.
    // -----------------------------------------------------------------------
    float occ = 0.0f;
    int validSamples = 0;

    for (int i = 0; i < 32; i++)
    {
        // Orient the sample into the hemisphere aligned with the surface normal.
        float3 sampleVS = posVS + mul(u_Samples[i].xyz, TBN) * radius;

        // Project the sample position back to screen space.
        float4 offset = mul(u_Projection, float4(sampleVS, 1.0f));
        offset.xyz /= offset.w;
        float2 suv = float2(offset.x * 0.5f + 0.5f, -offset.y * 0.5f + 0.5f);

        // Skip samples that project outside the screen.
        if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f)
            continue;

        // Read what the depth buffer actually stores at the projected position.
        float sDepth = t_Depth.SampleLevel(s_Clamp, suv, 0).r;
        float3 occluderPos = ReconstructViewPos(suv, sDepth);

        // Occlusion test: if the occluder is closer to the camera than our
        // sample position (with bias), there is geometry blocking.
        // In view-space (Z negative looking forward):
        //   occluderPos.z > sampleVS.z  means occluder is closer to camera.
        float occluderZ = occluderPos.z; // negative
        float sampleZ = sampleVS.z;      // negative
        float isOccluded = (occluderZ >= sampleZ + bias) ? 1.0f : 0.0f;

        // Range check: reject occlusion from surfaces that are far behind the
        // center pixel (e.g., a wall far behind a thin railing). The radius
        // parameter controls the falloff. Added epsilon to prevent division
        // by near-zero which caused flickering artifacts.
        float depthDiff = abs(posVS.z - occluderPos.z);
        float rangeCheck = smoothstep(0.0f, 1.0f, radius / (depthDiff + 0.001f));

        occ += isOccluded * rangeCheck;
        validSamples++;
    }

    // Normalize by actual valid samples to avoid darkening from edge clipping.
    float sampleCount = max(float(validSamples), 1.0f);
    occ = 1.0f - (occ / sampleCount);
    occ = pow(max(occ, 0.0f), power);

    u_Target[DTid.xy] = occ;
}
