Texture2D inputTexture : register(t0);
SamplerState sampler0  : register(s0);

cbuffer FXAABuffer : register(b0)
{
    float2 inverseScreenSize;
    float2 padding;
};

struct PixelInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

#define FXAA_EDGE_THRESHOLD      (1.0/8.0)
#define FXAA_EDGE_THRESHOLD_MIN  (1.0/24.0)
#define FXAA_SUBPIX_TRIM         (1.0/4.0)
#define FXAA_SUBPIX_CAP          (3.0/4.0)

float FxaaLuma(float3 rgb) {
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

float4 main(PixelInput input) : SV_TARGET
{
    float2 uv = input.uv;
    float3 rgbM = inputTexture.SampleLevel(sampler0, uv, 0).rgb;
    
    float lumaM  = FxaaLuma(rgbM);
    float lumaS  = FxaaLuma(inputTexture.SampleLevel(sampler0, uv + float2( 0.0,  1.0) * inverseScreenSize, 0).rgb);
    float lumaE  = FxaaLuma(inputTexture.SampleLevel(sampler0, uv + float2( 1.0,  0.0) * inverseScreenSize, 0).rgb);
    float lumaN  = FxaaLuma(inputTexture.SampleLevel(sampler0, uv + float2( 0.0, -1.0) * inverseScreenSize, 0).rgb);
    float lumaW  = FxaaLuma(inputTexture.SampleLevel(sampler0, uv + float2(-1.0,  0.0) * inverseScreenSize, 0).rgb);
    
    float maxLuma = max(lumaM, max(max(lumaN, lumaW), max(lumaS, lumaE)));
    float minLuma = min(lumaM, min(min(lumaN, lumaW), min(lumaS, lumaE)));
    
    float lumaRange = maxLuma - minLuma;
    
    if (lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, maxLuma * FXAA_EDGE_THRESHOLD)) {
        return float4(rgbM, 1.0);
    }
    
    float lumaNW = FxaaLuma(inputTexture.SampleLevel(sampler0, uv + float2(-1.0, -1.0) * inverseScreenSize, 0).rgb);
    float lumaNE = FxaaLuma(inputTexture.SampleLevel(sampler0, uv + float2( 1.0, -1.0) * inverseScreenSize, 0).rgb);
    float lumaSW = FxaaLuma(inputTexture.SampleLevel(sampler0, uv + float2(-1.0,  1.0) * inverseScreenSize, 0).rgb);
    float lumaSE = FxaaLuma(inputTexture.SampleLevel(sampler0, uv + float2( 1.0,  1.0) * inverseScreenSize, 0).rgb);
    
    float lumaDownUp = lumaN + lumaS;
    float lumaLeftRight = lumaW + lumaE;
    
    float lumaLeftCorners = lumaNW + lumaSW;
    float lumaDownCorners = lumaSW + lumaSE;
    float lumaRightCorners = lumaNE + lumaSE;
    float lumaUpCorners = lumaNW + lumaNE;
    
    float edgeHorizontal = abs(-2.0 * lumaW + lumaLeftCorners) + abs(-2.0 * lumaM + lumaDownUp) * 2.0 + abs(-2.0 * lumaE + lumaRightCorners);
    float edgeVertical   = abs(-2.0 * lumaN + lumaUpCorners) + abs(-2.0 * lumaM + lumaLeftRight) * 2.0 + abs(-2.0 * lumaS + lumaDownCorners);
    
    bool isHorizontal = (edgeHorizontal >= edgeVertical);
    
    float luma1 = isHorizontal ? lumaS : lumaE;
    float luma2 = isHorizontal ? lumaN : lumaW;
    float gradient1 = luma1 - lumaM;
    float gradient2 = luma2 - lumaM;
    
    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));
    
    float stepLength = isHorizontal ? inverseScreenSize.y : inverseScreenSize.x;
    
    float lumaLocalAverage = 0.0;
    if (is1Steepest) {
        lumaLocalAverage = 0.5 * (luma1 + lumaM);
    } else {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma2 + lumaM);
    }
    
    float2 currentUv = uv;
    if (isHorizontal) {
        currentUv.y += stepLength * 0.5;
    } else {
        currentUv.x += stepLength * 0.5;
    }
    
    float2 offset = isHorizontal ? float2(inverseScreenSize.x, 0.0) : float2(0.0, inverseScreenSize.y);
    
    float2 uv1 = currentUv - offset;
    float2 uv2 = currentUv + offset;
    
    float lumaEnd1 = FxaaLuma(inputTexture.SampleLevel(sampler0, uv1, 0).rgb);
    float lumaEnd2 = FxaaLuma(inputTexture.SampleLevel(sampler0, uv2, 0).rgb);
    lumaEnd1 -= lumaLocalAverage;
    lumaEnd2 -= lumaLocalAverage;
    
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;
    
    if (!reached1) uv1 -= offset;
    if (!reached2) uv2 += offset;
    
    if (!reachedBoth) {
        for(int i = 2; i < 8; i++) {
            if (!reached1) {
                lumaEnd1 = FxaaLuma(inputTexture.SampleLevel(sampler0, uv1, 0).rgb);
                lumaEnd1 -= lumaLocalAverage;
            }
            if (!reached2) {
                lumaEnd2 = FxaaLuma(inputTexture.SampleLevel(sampler0, uv2, 0).rgb);
                lumaEnd2 -= lumaLocalAverage;
            }
            
            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;
            reachedBoth = reached1 && reached2;
            
            if (!reached1) uv1 -= offset * 2.0;
            if (!reached2) uv2 += offset * 2.0;
            
            if (reachedBoth) break;
        }
    }
    
    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);
    
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    
    float edgeThickness = (distance1 + distance2);
    float pixelOffset = -distanceFinal / edgeThickness + 0.5;
    
    bool isLumaCenterSmaller = lumaM < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    
    float lumaAverage = (1.0/12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
    float subPixelOffset1 = clamp(abs(lumaAverage - lumaM)/lumaRange, 0.0, 1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * FXAA_SUBPIX_CAP;
    
    finalOffset = max(finalOffset, subPixelOffsetFinal);
    
    float2 finalUv = uv;
    if (isHorizontal) {
        finalUv.y += finalOffset * stepLength;
    } else {
        finalUv.x += finalOffset * stepLength;
    }
    
    float3 finalColor = inputTexture.SampleLevel(sampler0, finalUv, 0).rgb;
    return float4(finalColor, 1.0);
}
