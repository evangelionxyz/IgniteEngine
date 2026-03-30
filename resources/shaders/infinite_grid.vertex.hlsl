struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
};

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

cbuffer CameraBuffer : register(b0)
{
    Camera camera;
}

cbuffer GridBuffer : register(b1)
{
    GridSettings grid;
}

static const float2 positions[4] =
{
    float2(-1.0f, -1.0f),
    float2(1.0f, -1.0f),
    float2(1.0f, 1.0f),
    float2(-1.0f, 1.0f)
};

static const int indices[6] = { 0, 1, 2, 0, 2, 3 };

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : WORLDPOS;
    float3 cameraPos : CAMERAPOS;
    float gridSize : GRIDSIZE;
    float planeMode : PLANEMODE;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;

    int index = indices[vertexID];
    float2 pos = positions[index] * grid.settings0.z;
    float3 worldPos;

    if (grid.settings1.x > 0.5f)
    {
        worldPos = float3(pos.x + camera.position.x, pos.y + camera.position.y, 0.0f);
    }
    else
    {
        worldPos = float3(pos.x + camera.position.x, 0.0f, pos.y + camera.position.z);
    }

    output.position = mul(mul(camera.projection, camera.view), float4(worldPos, 1.0f));
    output.worldPos = worldPos;
    output.cameraPos = camera.position.xyz;
    output.gridSize = grid.settings0.z;
    output.planeMode = grid.settings1.x;

    return output;
}