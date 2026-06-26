enum Material2DType
{
    MATERIAL_2D_UNLIT = 0,
    MATERIAL_2D_LIT = 1,
};

struct Material2D
{
    float4 baseColor     : COLOR0;
    float4 additiveColor : COLOR1;
    float2 tiling        : TEXCOORD0;
    uint materialType    : MATTYPE0;
};

struct PointLight2D
{
    float4 position : POSITION0;
    float4 color    : COLOR0;
    float radius    : RADIUS0;
    float intensity : INTENSITY0;
};

struct Material2DLighting
{
    uint pointLightCount;
    float3 _padding;
    PointLight2D pointLights[32];
};

