#define NUM_CASCADES 4

struct Object
{
    float4x4 transformMatrix;
    float4x4 normalMatrix;
    uint objectID;
    float3 _padding;
};

struct Camera
{
    float4x4 projection;
    float4x4 view;
    float4 position;
};

struct Scene
{
    float4 lightColor;        // w component can store lightIntensity
    float2 lightAngle;
    float sunAngularRadius;
    int renderMode;
    int debugShadow;
    float exposure;
    float gamma;
    float ambient;
};

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
