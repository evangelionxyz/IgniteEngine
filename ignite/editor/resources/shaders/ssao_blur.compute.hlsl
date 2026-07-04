// Bilateral depth-aware separable blur for SSAO.
// Run twice: first with u_Horizontal=1 (H pass), then u_Horizontal=0 (V pass).
// The depth comparison prevents blurring across depth discontinuities (edges),
// which eliminates the dark halos that a naive box blur produces.

Texture2D t_Src   : register(t0);  // raw AO (or previous blur pass)
Texture2D t_Depth : register(t1);  // scene depth buffer (full-res)
#ifdef TARGET_VULKAN
    [[vk::image_format("r8")]]
#endif
RWTexture2D<unorm float> u_Target : register(u0);
SamplerState s_Clamp : register(s0);

cbuffer BlurParams : register(b0)
{
    float u_Horizontal;      // 1.0 = horizontal pass, 0.0 = vertical pass
    float u_NearPlane;       // camera near plane for depth linearization
    float u_FarPlane;        // camera far plane for depth linearization
    float u_DepthSharpness;  // bilateral sharpness (higher = more edge-preserving)
};

// Linearize a [0,1] depth value from a perspective ZO projection into
// view-space distance (positive, in world units).
float LinearizeDepth(float d)
{
    return u_NearPlane * u_FarPlane / (u_FarPlane - d * (u_FarPlane - u_NearPlane));
}

// 7-tap Gaussian weights (sigma ≈ 2.0, normalized so they sum to 1).
// [0] = center, [1..3] = offsets ±1..±3
static const float kGauss[4] = { 0.3829f, 0.2417f, 0.0606f, 0.0060f };

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    u_Target.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h) return;

    float2 texel = 1.0f / float2(w, h);
    float2 uv = (float2(DTid.xy) + 0.5f) * texel;

    // Direction vector: horizontal or vertical, stepping one texel at a time.
    float2 dir = (u_Horizontal > 0.5f) ? float2(texel.x, 0.0f) : float2(0.0f, texel.y);

    // Center pixel
    float centerAO = t_Src.SampleLevel(s_Clamp, uv, 0.0f).r;
    float centerDepth = LinearizeDepth(t_Depth.SampleLevel(s_Clamp, uv, 0.0f).r);

    float sum = centerAO * kGauss[0];
    float wsum = kGauss[0];

    // Symmetric taps: ±1, ±2, ±3
    [unroll]
    for (int i = 1; i <= 3; ++i)
    {
        float2 offset = dir * float(i);

        // Positive direction tap
        {
            float2 suv = uv + offset;
            float ao = t_Src.SampleLevel(s_Clamp, suv, 0.0f).r;
            float d  = LinearizeDepth(t_Depth.SampleLevel(s_Clamp, suv, 0.0f).r);

            // Bilateral weight: exponential decay based on depth difference.
            // Large depth jumps (edges) get near-zero weight → edge preserved.
            float depthW = exp(-abs(d - centerDepth) * u_DepthSharpness);
            float finalW = kGauss[i] * depthW;
            sum  += ao * finalW;
            wsum += finalW;
        }

        // Negative direction tap
        {
            float2 suv = uv - offset;
            float ao = t_Src.SampleLevel(s_Clamp, suv, 0.0f).r;
            float d  = LinearizeDepth(t_Depth.SampleLevel(s_Clamp, suv, 0.0f).r);

            float depthW = exp(-abs(d - centerDepth) * u_DepthSharpness);
            float finalW = kGauss[i] * depthW;
            sum  += ao * finalW;
            wsum += finalW;
        }
    }

    u_Target[DTid.xy] = sum / max(wsum, 0.001f);
}
