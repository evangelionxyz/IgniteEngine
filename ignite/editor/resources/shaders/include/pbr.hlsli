// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PBR_HLSLI
#define IGN_PBR_HLSLI

#define M_RCPPI 0.31830988618379067153776752674503
#define M_PI 3.1415926535897932384626433832795

// OpenPBR Surface core
struct OpenPBRSurface
{
    float3 baseColor; float baseWeight; float metalness; float specularWeight;
    float3 specularColor; float specularRoughness; float specularIOR;
    float coatWeight; float3 coatColor; float coatRoughness; float coatIOR;
    float coatDarkening;
    float specularAnisotropy;
    float subsurfaceWeight; float3 subsurfaceColor; float3 subsurfaceRadius; float subsurfaceScale;
    float transmissionWeight; float3 transmissionColor; float transmissionDepth;
    float fuzzWeight; float3 fuzzColor; float fuzzRoughness;
    float useMultiScatter;
};

static float3 SchlickFresnelCos(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

static float3 SchlickFresnel(float3 lightDirection, float3 normal, float3 specularColor)
{
    return SchlickFresnelCos(dot(lightDirection, normal), specularColor);
}

static float GGXDistribution(float NdotH, float roughness)
{
    float alpha = max(roughness * roughness, 0.0025f);
    float alpha2 = alpha * alpha;
    float d = NdotH * NdotH * (alpha2 - 1.0f) + 1.0f;
    return alpha2 / max(M_PI * d * d, 1e-6f);
}

static float GGXSmithG1(float NdotX, float roughness)
{
    float alpha = max(roughness * roughness, 0.0025f);
    float alpha2 = alpha * alpha;
    float n2 = max(NdotX * NdotX, 1e-5f);
    return 2.0f * NdotX / max(NdotX + sqrt(alpha2 + (1.0f - alpha2) * n2), 1e-5f);
}

static float3 GGXBRDF(float3 N, float3 L, float3 V, float roughness, float3 F)
{
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    if (NdotL <= 0.0f || NdotV <= 0.0f) return 0.0f;
    float3 H = normalize(L + V);
    float D = GGXDistribution(saturate(dot(N, H)), roughness);
    float G = GGXSmithG1(NdotL, roughness) * GGXSmithG1(NdotV, roughness);
    return F * (D * G / max(4.0f * NdotL * NdotV, 1e-5f));
}

static float DielectricF0(float iorRatio)
{
    float f = (iorRatio - 1.0f) / max(iorRatio + 1.0f, 1e-4f);
    return f * f;
}

static float3 OpenPBRMetalFresnel(float cosTheta, float3 f0, float3 edgeTint, float weight)
{
    const float muBar = 1.0f / 7.0f;
    float3 schlick = SchlickFresnelCos(cosTheta, f0);
    float3 schlickBar = SchlickFresnelCos(muBar, f0);
    float correction = cosTheta * pow(1.0f - cosTheta, 6.0f) / max(muBar * pow(1.0f - muBar, 6.0f), 1e-6f);
    return saturate(weight * (schlick - correction * (schlickBar - saturate(edgeTint) * schlickBar)));
}

static float3 OpenPBRDielectricFresnel(float cosTheta, OpenPBRSurface surface)
{
    float ratio = lerp(surface.specularIOR, surface.specularIOR / max(surface.coatIOR, 1.0f), surface.coatWeight);
    float f0 = min(DielectricF0(ratio) * max(surface.specularWeight, 0.0f), 1.0f);
    return SchlickFresnelCos(cosTheta, f0) * saturate(surface.specularColor);
}

static float3 OpenPBRBaseFresnel(float cosTheta, OpenPBRSurface surface)
{
    float3 dielectric = OpenPBRDielectricFresnel(cosTheta, surface);
    float3 metal = OpenPBRMetalFresnel(cosTheta, saturate(surface.baseColor * surface.baseWeight),
        surface.specularColor, surface.specularWeight);
    return lerp(dielectric, metal, surface.metalness);
}

static float3 OrenNayarDiffuse(float3 N, float3 L, float3 V, float roughness, float3 albedo)
{
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float LdotV = saturate(dot(L, V));
    
    float s = LdotV - NdotL * NdotV;
    float t = s <= 0.0f ? 1.0f : max(max(NdotL, NdotV), 1e-6f);
    
    float sigma2 = roughness * roughness;
    float A = 1.0f - 0.5f * sigma2 / (sigma2 + 0.33f);
    float B = 0.45f * sigma2 / (sigma2 + 0.09f);
    
    return albedo * M_RCPPI * (A + B * s / t);
}

static float GGXDirectionalAlbedo(float cosTheta, float roughness)
{
    float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
    float4 c1 = float4(1.0f, 0.0425f, 1.04f, -0.04f);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28f * saturate(cosTheta))) * r.x + r.y;
    float2 AB = float2(-1.04f, 1.04f) * a004 + r.zw;
    return saturate(AB.x + AB.y);
}

static float3 MultiScatterCompensation(float NdotV, float NdotL, float roughness, float3 F0, float enabled)
{
    float Eo = GGXDirectionalAlbedo(NdotV, roughness);
    float Ei = GGXDirectionalAlbedo(NdotL, roughness);
    float3 compensation = 1.0f + F0 * ((1.0f - Eo) * (1.0f - Ei) / max(Eo * Ei, 1e-5f));
    return lerp(float3(1.0f, 1.0f, 1.0f), compensation, enabled);
}

static void ComputeAnisotropicAlpha(float roughness, float anisotropy, out float alphaT, out float alphaB)
{
    float r2 = roughness * roughness;
    float a = anisotropy;
    alphaT = r2 * sqrt(2.0f / max(1.0f + (1.0f - a) * (1.0f - a), 1e-6f));
    alphaB = (1.0f - a) * alphaT;
}

static float GGXDistributionAniso(float3 N, float3 H, float3 T, float3 B, float alphaT, float alphaB)
{
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = dot(N, H);
    
    float aT = max(alphaT, 1e-4f);
    float aB = max(alphaB, 1e-4f);
    
    float d = (TdotH * TdotH) / (aT * aT) + (BdotH * BdotH) / (aB * aB) + NdotH * NdotH;
    return 1.0f / max(M_PI * aT * aB * d * d, 1e-6f);
}

static float GGXSmithG1Aniso(float3 V, float3 T, float3 B, float3 N, float alphaT, float alphaB)
{
    float TdotV = dot(T, V);
    float BdotV = dot(B, V);
    float NdotV = dot(N, V);
    
    float aT = max(alphaT, 1e-4f);
    float aB = max(alphaB, 1e-4f);
    
    float lambda = 0.5f * (sqrt(1.0f + (aT * aT * TdotV * TdotV + aB * aB * BdotV * BdotV) / max(NdotV * NdotV, 1e-6f)) - 1.0f);
    return 1.0f / max(1.0f + lambda, 1e-6f);
}

static float3 GGXBRDFAniso(float3 N, float3 L, float3 V, float3 T, float3 B, float alphaT, float alphaB, float3 F)
{
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    if (NdotL <= 0.0f || NdotV <= 0.0f) return 0.0f;
    
    float3 H = normalize(L + V);
    float D = GGXDistributionAniso(N, H, T, B, alphaT, alphaB);
    float G1L = GGXSmithG1Aniso(L, T, B, N, alphaT, alphaB);
    float G1V = GGXSmithG1Aniso(V, T, B, N, alphaT, alphaB);
    
    return F * (D * G1L * G1V / max(4.0f * NdotL * NdotV, 1e-5f));
}

static float3 SubsurfaceWrapDiffuse(float3 N, float3 L, float3 subsurfaceColor, float3 subsurfaceRadius, float subsurfaceScale)
{
    float3 wrap = saturate(subsurfaceRadius * subsurfaceScale);
    float NdotL = dot(N, L);
    float3 NdotL_wrap = (NdotL + wrap) / max(1.0f + wrap, 1e-5f);
    return subsurfaceColor * max(NdotL_wrap, 0.0f) * M_RCPPI;
}

static float3 TransmissionBeerLaw(float3 transmissionColor, float transmissionDepth, float NdotV)
{
    if (transmissionDepth <= 0.0f) return transmissionColor;
    float3 absorptionCoeff = -log(max(transmissionColor, 1e-6f)) / max(transmissionDepth, 1e-6f);
    return exp(-absorptionCoeff * transmissionDepth / max(NdotV, 0.05f));
}

static float CharlieNDF(float NdotH, float roughness)
{
    float alpha = max(roughness * roughness, 1e-5f);
    float sinTheta2 = max(1.0f - NdotH * NdotH, 1e-5f);
    return (2.0f + 1.0f / alpha) * pow(sinTheta2, 0.5f / alpha) / (2.0f * M_PI);
}

static float3 FuzzBRDF(float3 N, float3 L, float3 V, float roughness, float3 fuzzColor)
{
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float D = CharlieNDF(NdotH, roughness);
    float G = 1.0f / max(4.0f * (NdotL + NdotV - NdotL * NdotV), 1e-5f);
    return fuzzColor * D * G;
}

static float3 OpenPBRDirect(OpenPBRSurface surface, float3 N, float3 L, float3 V, float3 T, float3 B, float3 irradiance)
{
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    if (NdotL <= 0.0f || NdotV <= 0.0f) return 0.0f;
    
    float3 H = normalize(L + V);
    float3 baseF = OpenPBRBaseFresnel(saturate(dot(L, H)), surface);
    
    float3 baseSpecular = 0.0f;
    if (surface.specularAnisotropy > 0.0f)
    {
        float alphaT, alphaB;
        ComputeAnisotropicAlpha(surface.specularRoughness, surface.specularAnisotropy, alphaT, alphaB);
        baseSpecular = GGXBRDFAniso(N, L, V, T, B, alphaT, alphaB, baseF);
    }
    else
    {
        baseSpecular = GGXBRDF(N, L, V, surface.specularRoughness, baseF);
    }
    
    float3 comp = MultiScatterCompensation(NdotV, NdotL, surface.specularRoughness, baseF, surface.useMultiScatter);
    baseSpecular *= comp;
    
    float3 diffuseTerm = OrenNayarDiffuse(N, L, V, surface.specularRoughness, float3(1.0f, 1.0f, 1.0f)) * NdotL;
    float3 ssTerm = SubsurfaceWrapDiffuse(N, L, surface.subsurfaceColor, surface.subsurfaceRadius, surface.subsurfaceScale);
    float3 diffSS = lerp(diffuseTerm, ssTerm, surface.subsurfaceWeight);
    
    float3 transTerm = TransmissionBeerLaw(surface.transmissionColor, surface.transmissionDepth, NdotV) * M_RCPPI * NdotL;
    float3 diffSSTrans = lerp(diffSS, transTerm, surface.transmissionWeight);
    
    float3 baseDiffuse = (1.0f - surface.metalness) * surface.baseWeight * surface.baseColor * (1.0f - baseF) * diffSSTrans;
    
    float3 base = baseDiffuse + baseSpecular * NdotL;
    
    float3 coatF = SchlickFresnelCos(saturate(dot(L, H)), DielectricF0(max(surface.coatIOR, 1.0f)));
    float3 coat = surface.coatWeight * GGXBRDF(N, L, V, surface.coatRoughness, coatF) * NdotL;
    
    float3 throughCoat = lerp(float3(1.0f, 1.0f, 1.0f), saturate(surface.coatColor) * (1.0f - coatF),
        surface.coatWeight * surface.coatDarkening);
        
    float3 fuzz = surface.fuzzWeight * FuzzBRDF(N, L, V, surface.fuzzRoughness, surface.fuzzColor) * NdotL;
    
    return (fuzz + (1.0f - surface.fuzzWeight) * (coat + base * throughCoat)) * irradiance;
}

static float3 OpenPBREnvironment(OpenPBRSurface surface, float3 N, float3 V, float3 radiance)
{
    float NdotV = saturate(dot(N, V));
    float3 baseF = OpenPBRBaseFresnel(NdotV, surface);
    float3 coatF = SchlickFresnelCos(NdotV, DielectricF0(max(surface.coatIOR, 1.0f)));
    
    // Attenuate sharp mirror reflections on rough surfaces (environment map is single-mip without prefiltering)
    float baseRoughnessFactor = 1.0f - saturate(surface.specularRoughness);
    baseRoughnessFactor = baseRoughnessFactor * baseRoughnessFactor;
    float3 baseSpecular = baseF * baseRoughnessFactor;
    
    float coatRoughnessFactor = 1.0f - saturate(surface.coatRoughness);
    coatRoughnessFactor = coatRoughnessFactor * coatRoughnessFactor;
    float3 coatSpecular = surface.coatWeight * coatF * coatRoughnessFactor;
    
    float3 throughCoat = lerp(float3(1.0f, 1.0f, 1.0f), saturate(surface.coatColor) * (1.0f - coatF),
        surface.coatWeight * surface.coatDarkening);
        
    float fuzzRoughnessFactor = 1.0f - saturate(surface.fuzzRoughness);
    float3 fuzz = surface.fuzzWeight * surface.fuzzColor * (fuzzRoughnessFactor * fuzzRoughnessFactor);
    
    return radiance * (fuzz + (1.0f - surface.fuzzWeight) * (coatSpecular + baseSpecular * throughCoat));
}

static float3 GGX(float3 N, float3 L, float3 V, float3 irradiance, float3 diffuse, float3 specular, float roughness)
{
    float NdotL = saturate(dot(N, L));
    float3 H = normalize(V + L);
    return (diffuse * M_RCPPI + GGXBRDF(N, L, V, roughness,
        SchlickFresnelCos(saturate(dot(L, H)), specular))) * NdotL * irradiance;
}

#endif
