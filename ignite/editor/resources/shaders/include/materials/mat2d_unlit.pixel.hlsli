float4 ComputeMaterial2DUnlit(Material2D material, float4 textureColor)
{
    return material.baseColor * textureColor + material.additiveColor;
}
