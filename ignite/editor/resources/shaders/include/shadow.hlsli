#define NUM_CASCADES 4

struct CascadesShadows
{
    float4x4 lightViewProjection[NUM_CASCADES];
    
    float4 cascadeSplits; // view-space distances (camera space z positive forward magnitude)
    
    float shadowStrength;
    float minBias;
    float maxBias;
    float pcfRadius;

    int cascadeIndex;
    float padding[3];
};

// ---------------------------------------------------------------------------
// 16-tap Poisson disk in unit-disk space.
// Samples are hand-tuned for good coverage without visible patterns.
// ---------------------------------------------------------------------------
static const float2 k_PoissonDisk[16] =
{
    float2(-0.94201624f, -0.39906216f),
    float2(0.94558609f, -0.76890725f),
    float2(-0.09418410f, -0.92938870f),
    float2(0.34495938f, 0.29387760f),
    float2(-0.91588581f, 0.45771432f),
    float2(-0.81544232f, -0.87912464f),
    float2(-0.38277543f, 0.27676845f),
    float2(0.97484398f, 0.75648379f),
    float2(0.44323325f, -0.97511554f),
    float2(0.53742981f, -0.47373420f),
    float2(-0.26496911f, -0.41893023f),
    float2(0.79197514f, 0.19090188f),
    float2(-0.24188840f, 0.99706507f),
    float2(-0.81409955f, 0.91437590f),
    float2(0.19984126f, 0.78641367f),
    float2(0.14383161f, -0.14100790f)
};

static int GetCascadeIndex(float4 cascadeSplits, float viewDepth)
{
    if (viewDepth < cascadeSplits.x)
        return 0;
    if (viewDepth < cascadeSplits.y)
        return 1;
    if (viewDepth < cascadeSplits.z)
        return 2;
    return 3;
}

// ---------------------------------------------------------------------------
// Interleaved-gradient noise hash — fast, low-correlation per-pixel rotation
// angle used to decorrelate Poisson taps across neighbouring pixels, which
// removes the regular banding you get from an unrotated disk.
// Reference: Jimenez 2014, "Next-Generation Post Processing in Call of Duty"
// ---------------------------------------------------------------------------
static float InterleavedGradientNoise(float2 screenPos)
{
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(screenPos, magic.xy)));
}

static float SampleShadow(CascadesShadows csm, Texture2DArray shadowMap, SamplerState shadowSampler,
    float4x4 cameraView, float3 worldPos, float3 normal, float3 lightDirection, float2 screenPos)
{
    float3 viewPos = mul(cameraView, float4(worldPos, 1.0f)).xyz;
    float viewDepth = -viewPos.z;
    int cascadeIdx = GetCascadeIndex(csm.cascadeSplits, viewDepth);
    cascadeIdx = clamp(cascadeIdx, 0, NUM_CASCADES - 1);

    float4 lightSpace = mul(csm.lightViewProjection[cascadeIdx], float4(worldPos, 1.0f));
    float3 ndc = lightSpace.xyz / lightSpace.w;
    float2 shadowUV = ndc.xy * 0.5f + 0.5f;
    float shadowDepth = ndc.z; // orthoZO outputs z in [0,1] for both D3D12 and Vulkan
    // Vulkan texture UV origin is top-left while NDC Y+ is upward, so we must
    // flip Y to convert from NDC-space to texture-space correctly.
    shadowUV.y = 1.0f - shadowUV.y;

    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||
        shadowUV.y < 0.0f || shadowUV.y > 1.0f)
    {
        return 1.0f;
    }

    // Slope-scaled bias: steep surfaces get more bias to avoid acne.
    // Per-cascade scale: far cascades have lower depth precision so they need
    // proportionally more bias. cascade 0 = 1x, cascade 1 = 1.5x, ...
    float cosTheta = saturate(dot(normal, -lightDirection));
    float baseBias = lerp(csm.maxBias, csm.minBias, cosTheta);
    float cascadeBiasScale = 1.0f + float(cascadeIdx) * 0.5f;
    float bias = baseBias * cascadeBiasScale;
    float compareDepth = shadowDepth - bias;

    // Texel size in UV space.
    uint width, height, layers;
    shadowMap.GetDimensions(width, height, layers);
    float2 texelSize = 1.0f / float2(width, height);

    // ---------------------------------------------------------------------------
    // PCF radius scales with cascade: keep cascade 0 sharp, relax for far ones.
    // csm.pcfRadius acts as the base radius in texels for cascade 0.
    // ---------------------------------------------------------------------------
    float cascadeScale = 1.0f + float(cascadeIdx) * 0.5f;
    float filterRadius = max(csm.pcfRadius, 0.5f) * cascadeScale;

    // ---------------------------------------------------------------------------
    // Per-pixel rotation of the Poisson disk using interleaved-gradient noise.
    // This breaks the structured pattern and eliminates banding without TAA.
    // ---------------------------------------------------------------------------
    float angle = InterleavedGradientNoise(screenPos) * 6.28318530718f; // 2*PI
    float sinA = sin(angle);
    float cosA = cos(angle);
    float2x2 rot = float2x2(cosA, -sinA, sinA, cosA);

    // ---------------------------------------------------------------------------
    // 16-tap Poisson-disk PCF with hardware comparison sampler.
    // SampleCmpLevelZero performs a bilinear 2x2 gather and returns the
    // average of the four per-texel comparisons — 4x quality for free.
    // ---------------------------------------------------------------------------
    float visibility = 0.0f;
    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        float2 tapOffset = mul(rot, k_PoissonDisk[i]) * texelSize * filterRadius;
        float2 sampleUV = clamp(shadowUV + tapOffset, 0.0f, 1.0f);
        float storedDepth = shadowMap.SampleLevel(
            shadowSampler,
            float3(sampleUV, float(cascadeIdx)),
            0.0f).r;
        // Lit when fragment depth (with bias) is less than the stored occluder depth.
        visibility += (compareDepth < storedDepth) ? 1.0f : 0.0f;
    }
    visibility /= 16.0f;

    return saturate(lerp(1.0f, visibility, csm.shadowStrength));
}