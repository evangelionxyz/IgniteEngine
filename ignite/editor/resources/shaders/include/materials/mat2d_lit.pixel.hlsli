float4 ComputeMaterial2DLit(Material2D material, Material2DLighting lighting, float3 worldPosition, float4 textureColor)
{
    float4 base = material.baseColor * textureColor + material.additiveColor;
    float3 lightingAccum = float3(0.15f, 0.15f, 0.15f);

    [loop]
    for (uint i = 0; i < lighting.pointLightCount; ++i)
    {
        PointLight2D light = lighting.pointLights[i];
        float2 toLight = light.position.xy - worldPosition.xy;
        float distanceToLight = length(toLight);

        if (distanceToLight < light.radius)
        {
            float falloff = saturate(1.0f - (distanceToLight / max(light.radius, 0.0001f)));
            float attenuation = falloff * falloff;
            lightingAccum += light.color.rgb * (light.intensity * attenuation);
        }
    }

    return float4(base.rgb * lightingAccum, base.a);
}
