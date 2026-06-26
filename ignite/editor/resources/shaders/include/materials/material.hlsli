struct Material
{
    float4 baseColorFactor;
    float4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
    int metallicChannel;
    int roughnessChannel;
    float padding[3];
};