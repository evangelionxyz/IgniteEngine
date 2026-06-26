struct Material
{
    float4 baseColorFactor;
    float4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
    int metallicChannel;
    int roughnessChannel;
    int blendMode;      // 0 = Opaque, 1 = Transparent
    float2 tilingFactor;
};