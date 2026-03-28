struct GridSettings
{
    float4 thinColor;
    float4 thickColor;
    float4 xAxisColor;
    float4 yAxisColor;
    float4 zAxisColor;
    float4 settings0; // x=cellSize y=minPixelsBetweenCells z=gridSize w=majorLineScale
    float4 settings1; // x=planeMode(0:XZ, 1:XY) y=enableX z=enableY w=enableZ
};

cbuffer GridBuffer : register(b1)
{
    GridSettings grid;
}

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : WORLDPOS;
    float3 cameraPos : CAMERAPOS;
    float gridSize : GRIDSIZE;
    float planeMode : PLANEMODE;
};

float log10f(float x)
{
    return log(x) / log(10.0f);
}

float max2(float2 v)
{
    return max(v.x, v.y);
}

float GridLineAlpha(float2 uv, float2 dudv, float cellSize)
{
    float2 safeDudv = max(dudv, float2(1e-6f, 1e-6f));
    float2 modDiv = (uv - cellSize * floor(uv / cellSize)) / safeDudv;
    float2 a = 1.0f - abs(saturate(modDiv) * 2.0f - 1.0f);
    return max2(a);
}

float4 main(VSOutput input) : SV_Target
{
    const float cellSize = max(grid.settings0.x, 0.0001f);
    const float minPixelsBetweenCells = max(grid.settings0.y, 0.1f);
    const float majorLineScale = max(grid.settings0.w, 1.0f);

    float2 uv = (input.planeMode > 0.5f) ? input.worldPos.xy : input.worldPos.xz;
    float2 cameraUv = (input.planeMode > 0.5f) ? input.cameraPos.xy : input.cameraPos.xz;

    float2 dvx = float2(ddx(uv.x), ddy(uv.x));
    float2 dvy = float2(ddx(uv.y), ddy(uv.y));

    float2 dudv = float2(length(dvx), length(dvy));
    float l = length(dudv);

    float LOD = max(0.0f, log10f(l * minPixelsBetweenCells / cellSize) + 1.0f);

    float cellLod0 = cellSize * pow(majorLineScale, floor(LOD));
    float cellLod1 = cellLod0 * majorLineScale;
    float cellLod2 = cellLod1 * majorLineScale;

    float2 dudvScaled = max(dudv * 2.0f, float2(1e-6f, 1e-6f));

    float lod0a = GridLineAlpha(uv, dudvScaled, cellLod0);
    float lod1a = GridLineAlpha(uv, dudvScaled, cellLod1);
    float lod2a = GridLineAlpha(uv, dudvScaled, cellLod2);

    float LODFade = frac(LOD);

    float4 color;
    if (lod2a > 0.0f)
    {
        color = grid.thickColor;
        color.a *= lod2a;
    }
    else if (lod1a > 0.0f)
    {
        color = lerp(grid.thickColor, grid.thinColor, LODFade);
        color.a *= lod1a;
    }
    else
    {
        color = grid.thinColor;
        color.a *= lod0a * (1.0f - LODFade);
    }

    float axisAlphaU = 1.0f - saturate(abs(uv.x) / max(dudvScaled.x * 1.5f, 1e-6f));
    float axisAlphaV = 1.0f - saturate(abs(uv.y) / max(dudvScaled.y * 1.5f, 1e-6f));

    if (input.planeMode > 0.5f)
    {
        if (grid.settings1.y > 0.5f)
            color = lerp(color, grid.xAxisColor, axisAlphaV * grid.xAxisColor.a);

        if (grid.settings1.z > 0.5f)
            color = lerp(color, grid.yAxisColor, axisAlphaU * grid.yAxisColor.a);
    }
    else
    {
        if (grid.settings1.y > 0.5f)
            color = lerp(color, grid.xAxisColor, axisAlphaV * grid.xAxisColor.a);

        if (grid.settings1.w > 0.5f)
            color = lerp(color, grid.zAxisColor, axisAlphaU * grid.zAxisColor.a);
    }

    float falloff = 1.0f - smoothstep(0.7f, 1.0f, length(uv - cameraUv) / (input.gridSize * 0.8f));
    color.a *= falloff;

    return color;
}